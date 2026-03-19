/**
 * @file security_manager.cpp
 * @brief Security manager implementation
 *
 * Provides:
 * - Hash calculations (SHA256, SHA384, SHA512, MD5)
 * - RSA signature verification
 * - Windows Authenticode verification
 * - Secure random generation
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include "makine/security.hpp"
#include "makine/features.hpp"
#include "makine/logging.hpp"
#include "makine/audit.hpp"
#include "makine/metrics.hpp"
#include <spdlog/spdlog.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>  // CRYPTO_memcmp (SEC-2)
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Optional: libsodium for modern cryptography
#ifdef MAKINE_HAS_SODIUM
#include <sodium.h>
#endif

// Optional: mio for memory-mapped file hashing
#ifdef MAKINE_HAS_MIO
#include <mio/mmap.hpp>
#endif

// Threshold for using memory-mapped I/O (1 MB)
static constexpr size_t MIO_THRESHOLD = 1024 * 1024;

#ifdef _WIN32
#include <Windows.h>
#include <wintrust.h>
#include <softpub.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#endif

namespace makine {

// Private implementation class
class SecurityManager::Impl {
public:
    struct EvpPkeyDeleter {
        void operator()(EVP_PKEY* k) const noexcept { if (k) EVP_PKEY_free(k); }
    };
    std::unique_ptr<EVP_PKEY, EvpPkeyDeleter> publicKey;
    std::string publicKeyId;

    ~Impl() = default;

    const EVP_MD* getDigest(HashAlgorithm algo) const {
        switch (algo) {
            case HashAlgorithm::SHA256: return EVP_sha256();
            case HashAlgorithm::SHA384: return EVP_sha384();
            case HashAlgorithm::SHA512: return EVP_sha512();
            case HashAlgorithm::MD5: return EVP_md5();
            default: return EVP_sha256();
        }
    }

    std::string bytesToHex(const unsigned char* data, size_t len) const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            ss << std::setw(2) << static_cast<int>(data[i]);
        }
        return ss.str();
    }
};

// Embedded Ed25519 public key for package signature verification.
// Private key: scripts/certs/signing_private.pem (NEVER commit this)
// Regenerate with: openssl genpkey -algorithm Ed25519 -out private.pem
//                  openssl pkey -in private.pem -pubout -out public.pem
static constexpr const char* EMBEDDED_PUBLIC_KEY_PEM = R"(
-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEAenbLqZcQ4eoWsVvjpg3FQrkd0V1Q8b3P/OJSMkudvWo=
-----END PUBLIC KEY-----
)";

// Production key is embedded — Ed25519 (32-byte, fast, side-channel resistant)
static constexpr bool EMBEDDED_KEY_IS_REAL = true;

SecurityManager::SecurityManager() : impl_(std::make_unique<Impl>()) {
#ifdef MAKINE_HAS_SODIUM
    if (sodium_init() < 0) {
        spdlog::warn("libsodium initialization failed, falling back to OpenSSL");
    } else {
        spdlog::debug("SecurityManager initialized with libsodium");
    }
#else
    spdlog::debug("SecurityManager initialized with OpenSSL");
#endif
}

SecurityManager::~SecurityManager() = default;

Result<std::string> SecurityManager::hash(ByteSpan data, HashAlgorithm algo) const {
#ifdef MAKINE_HAS_SODIUM
    // Use libsodium BLAKE2b for SHA256 (faster, same security level)
    if (algo == HashAlgorithm::SHA256) {
        unsigned char digest[crypto_generichash_BYTES_MAX];
        constexpr size_t hashLen = 32;  // 256 bits

        if (crypto_generichash(digest, hashLen,
                               data.data(), data.size(),
                               nullptr, 0) != 0) {
            return std::unexpected(Error(ErrorCode::Unknown, "BLAKE2b hash failed"));
        }
        return impl_->bytesToHex(digest, hashLen);
    }
    // Fall through to OpenSSL for SHA384/SHA512/MD5
#endif

    auto ctx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to create hash context"));
    }

    const EVP_MD* md = impl_->getDigest(algo);
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest, &digestLen) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown, "Hash calculation failed"));
    }

    return impl_->bytesToHex(digest, digestLen);
}

Result<std::string> SecurityManager::hashFile(const fs::path& file, HashAlgorithm algo) const {
    if (!fs::exists(file)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Cannot open file for hashing: " + file.string()));
    }

    auto fileSize = fs::file_size(file);

#ifdef MAKINE_HAS_MIO
    // Use memory-mapped I/O for large files (avoids repeated read syscalls)
    if (fileSize >= MIO_THRESHOLD) {
        std::error_code ec;
        auto mmap = mio::make_mmap_source(file.string(), ec);
        if (!ec) {
            auto data = reinterpret_cast<const uint8_t*>(mmap.data());
            return hash(ByteSpan{data, mmap.size()}, algo);
        }
        // mmap failed, fall through to streaming
        spdlog::debug("mmap failed for {}, falling back to streaming", file.string());
    }
#endif

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Cannot open file for hashing: " + file.string()));
    }

#ifdef MAKINE_HAS_SODIUM
    // Use libsodium streaming hash for SHA256
    if (algo == HashAlgorithm::SHA256) {
        crypto_generichash_state state;
        constexpr size_t hashLen = 32;

        if (crypto_generichash_init(&state, nullptr, 0, hashLen) != 0) {
            return std::unexpected(Error(ErrorCode::Unknown, "BLAKE2b init failed"));
        }

        char buffer[8192];
        while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount() > 0) {
            if (crypto_generichash_update(&state,
                    reinterpret_cast<const unsigned char*>(buffer),
                    static_cast<size_t>(ifs.gcount())) != 0) {
                return std::unexpected(Error(ErrorCode::Unknown, "BLAKE2b update failed"));
            }
        }

        unsigned char digest[crypto_generichash_BYTES_MAX];
        if (crypto_generichash_final(&state, digest, hashLen) != 0) {
            return std::unexpected(Error(ErrorCode::Unknown, "BLAKE2b finalization failed"));
        }

        return impl_->bytesToHex(digest, hashLen);
    }
#endif

    // OpenSSL streaming hash
    auto ctx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to create hash context"));
    }

    const EVP_MD* md = impl_->getDigest(algo);
    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to init digest"));
    }

    char buffer[8192];
    while (ifs.read(buffer, sizeof(buffer)) || ifs.gcount() > 0) {
        if (EVP_DigestUpdate(ctx.get(), buffer, static_cast<size_t>(ifs.gcount())) != 1) {
            return std::unexpected(Error(ErrorCode::Unknown, "Hash update failed"));
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digestLen) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown, "Hash finalization failed"));
    }

    return impl_->bytesToHex(digest, digestLen);
}

bool SecurityManager::verifyHash(ByteSpan data, const std::string& expectedHash,
                                  HashAlgorithm algo) const {
    auto result = hash(data, algo);
    if (!result) return false;
    // Constant-time comparison to prevent timing side-channel attacks (SEC-2)
    if (result->size() != expectedHash.size()) return false;
    return CRYPTO_memcmp(result->data(), expectedHash.data(), result->size()) == 0;
}

VoidResult SecurityManager::loadEmbeddedKey() {
    MAKINE_LOG_INFO(log::SECURITY, "Loading embedded public key");

    if (!EMBEDDED_KEY_IS_REAL) {
        MAKINE_LOG_WARN(log::SECURITY,
            "Embedded key is placeholder — replace before production release");
#ifdef NDEBUG
        // In release builds, fail if placeholder key is still present
        return std::unexpected(Error(ErrorCode::CertificateError,
            "Embedded public key is placeholder — build with real key for production"));
#endif
    }

    auto result = loadPublicKeyPEM(EMBEDDED_PUBLIC_KEY_PEM);
    if (result) {
        AuditLogger::logSystemEvent("embedded_key_loaded",
            "Embedded public key loaded successfully", AuditSeverity::Info);
    }
    return result;
}

VoidResult SecurityManager::loadPublicKey(const fs::path& keyPath) {
    MAKINE_LOG_INFO(log::SECURITY, "Loading public key from: {}", keyPath.string());
    AuditLogger::logFileAccess(keyPath, "read_public_key");

    std::ifstream ifs(keyPath);
    if (!ifs) {
        MAKINE_LOG_ERROR(log::SECURITY, "Cannot open public key file: {}", keyPath.string());
        AuditLogger::logFileAccess(keyPath, "read_public_key", false, "File not found");
        metrics().increment("security.public_key_load_failures");
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Cannot open public key file: " + keyPath.string()));
    }

    std::string pem((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());

    auto result = loadPublicKeyPEM(pem);
    if (!result) {
        AuditLogger::logFileAccess(keyPath, "read_public_key", false, result.error().message());
    }
    return result;
}

VoidResult SecurityManager::loadPublicKeyPEM(const std::string& pem) {
    MAKINE_LOG_DEBUG(log::SECURITY, "Attempting to load public key PEM ({} bytes)", pem.size());

    auto bioDeleter = [](BIO* b) { if (b) BIO_free(b); };
    std::unique_ptr<BIO, decltype(bioDeleter)> bio(
        BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), bioDeleter);
    if (!bio) {
        MAKINE_LOG_ERROR(log::SECURITY, "Failed to create BIO for public key");
        AuditLogger::logSystemEvent("public_key_load_failed", "BIO creation failed", AuditSeverity::Warning);
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to create BIO"));
    }

    impl_->publicKey.reset(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));

    if (!impl_->publicKey) {
        MAKINE_LOG_ERROR(log::SECURITY, "Failed to parse public key PEM");
        AuditLogger::logSystemEvent("public_key_load_failed", "PEM parse error", AuditSeverity::Critical);
        metrics().increment("security.public_key_load_failures");
        return std::unexpected(Error(ErrorCode::SignatureInvalid,
            "Failed to parse public key PEM"));
    }

    publicKeyLoaded_ = true;
    MAKINE_LOG_INFO(log::SECURITY, "Public key loaded successfully");
    AuditLogger::logSystemEvent("public_key_loaded", "Public key successfully loaded", AuditSeverity::Info);
    metrics().increment("security.public_key_loads");
    return {};
}

Result<SignatureResult> SecurityManager::verifySignature(
    ByteSpan data, const std::string& signatureBase64
) const {
    // Start timing the verification operation
    auto timer = metrics().timer("security.signature_verification");

    SignatureResult result{false, "", 0, "", ""};
    MAKINE_LOG_DEBUG(log::SECURITY, "Verifying signature for {} bytes of data", data.size());

    if (!publicKeyLoaded_ || !impl_->publicKey) {
        result.message = "No public key loaded";
        MAKINE_LOG_WARN(log::SECURITY, "Signature verification attempted without public key");
        AuditLogger::logSignatureVerification("unknown", false, "No public key loaded");
        metrics().increment("security.signature_verify_failures");
        return result;
    }

    // Decode base64 signature
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(signatureBase64.data(),
                                static_cast<int>(signatureBase64.size()));
    bmem = BIO_push(b64, bmem);
    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);

    std::vector<unsigned char> signature(signatureBase64.size());
    int sigLen = BIO_read(bmem, signature.data(), static_cast<int>(signature.size()));
    BIO_free_all(bmem);

    if (sigLen <= 0) {
        result.message = "Failed to decode base64 signature";
        MAKINE_LOG_WARN(log::SECURITY, "Failed to decode base64 signature");
        AuditLogger::logSignatureVerification("unknown", false, "Base64 decode failed");
        metrics().increment("security.signature_verify_failures");
        return result;
    }
    signature.resize(static_cast<size_t>(sigLen));

    // Verify signature
    auto ctx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) {
        result.message = "Failed to create verification context";
        MAKINE_LOG_ERROR(log::SECURITY, "Failed to create EVP_MD_CTX for verification");
        AuditLogger::logSignatureVerification("unknown", false, "Context creation failed");
        metrics().increment("security.signature_verify_failures");
        return result;
    }

    bool verified = false;
    // Ed25519: pass nullptr for digest type — Ed25519 uses its own SHA-512 internally.
    // Use one-shot EVP_DigestVerify instead of Update+Final pattern (SEC-1 fix).
    if (EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, impl_->publicKey.get()) == 1) {
        int verifyResult = EVP_DigestVerify(
            ctx.get(),
            signature.data(), signature.size(),
            data.data(), data.size());
        verified = (verifyResult == 1);
    }

    result.valid = verified;
    result.publicKeyId = impl_->publicKeyId;

    // SECURITY CRITICAL: Log all verification results
    if (verified) {
        MAKINE_LOG_INFO(log::SECURITY, "Signature verification PASSED");
        AuditLogger::logSignatureVerification(impl_->publicKeyId, true,
            fmt::format("Data size: {} bytes", data.size()));
        metrics().increment("security.signature_verify_successes");
    } else {
        result.message = "Signature verification failed";
        MAKINE_LOG_WARN(log::SECURITY, "Signature verification FAILED");
        AuditLogger::logSignatureVerification(impl_->publicKeyId, false,
            fmt::format("Data size: {} bytes, verification rejected", data.size()));
        metrics().increment("security.signature_verify_failures");
    }

    return result;
}

Result<SignatureResult> SecurityManager::verifyPackageSignature(
    const fs::path& packagePath, const PackageSignature& signature
) const {
    // Start timing the package verification operation
    auto timer = metrics().timer("security.package_verification");

    SignatureResult result{false, "", 0, "", ""};
    MAKINE_LOG_INFO(log::SECURITY, "Verifying package signature: {}", packagePath.string());
    AuditLogger::logFileAccess(packagePath, "verify_package_signature");

    // Hash the package file
    auto hashResult = hashFile(packagePath);
    if (!hashResult) {
        result.message = "Failed to hash package: " + hashResult.error().message();
        MAKINE_LOG_ERROR(log::SECURITY, "Failed to hash package {}: {}",
            packagePath.string(), hashResult.error().message());
        AuditLogger::logSignatureVerification(packagePath.string(), false,
            "Hash calculation failed: " + hashResult.error().message());
        metrics().increment("security.package_verify_failures");
        return result;
    }

    // Compare hash — hashFile returns hex, sig has "sha256:<hex>" format
    std::string computedHashWithPrefix = "sha256:" + *hashResult;
    if (computedHashWithPrefix != signature.packageHash) {
        result.message = "Package hash mismatch";
        MAKINE_LOG_WARN(log::SECURITY, "Package hash mismatch for {}: expected={}, got={}",
            packagePath.string(), signature.packageHash, computedHashWithPrefix);
        AuditLogger::logSignatureVerification(packagePath.string(), false,
            "Hash mismatch - possible tampering or corruption detected");
        metrics().increment("security.package_verify_failures");
        metrics().increment("security.hash_mismatches");
        return result;
    }

    MAKINE_LOG_DEBUG(log::SECURITY, "Package hash verified: {}", computedHashWithPrefix);

    // Verify signature over the full hash string (matches signer: "sha256:<hex>")
    ByteBuffer hashBytes(computedHashWithPrefix.begin(), computedHashWithPrefix.end());
    auto sigResult = verifySignature(hashBytes, signature.signature);
    if (!sigResult) {
        MAKINE_LOG_ERROR(log::SECURITY, "Signature verification error for package {}",
            packagePath.string());
        AuditLogger::logSignatureVerification(packagePath.string(), false,
            "Signature verification error");
        metrics().increment("security.package_verify_failures");
        return sigResult;
    }

    result = *sigResult;
    result.signedAt = signature.timestamp;
    result.publicKeyId = signature.publicKeyId;

    // SECURITY CRITICAL: Log final package verification result
    if (result.valid) {
        MAKINE_LOG_INFO(log::SECURITY, "Package signature verification PASSED: {}",
            packagePath.string());
        AuditLogger::logSignatureVerification(packagePath.string(), true,
            "Package verified, keyId=" + signature.publicKeyId);
        metrics().increment("security.package_verify_successes");
    } else {
        MAKINE_LOG_WARN(log::SECURITY, "Package signature verification FAILED: {}",
            packagePath.string());
        AuditLogger::logSignatureVerification(packagePath.string(), false,
            "Signature invalid, keyId=" + signature.publicKeyId);
        metrics().increment("security.package_verify_failures");
    }

    return result;
}

Result<SignatureResult> SecurityManager::verifyAuthenticode(const fs::path& exePath) const {
    // Start timing the Authenticode verification
    auto timer = metrics().timer("security.authenticode_verification");

    SignatureResult result{false, "", 0, "", ""};
    MAKINE_LOG_INFO(log::SECURITY, "Verifying Authenticode signature: {}", exePath.string());
    AuditLogger::logFileAccess(exePath, "verify_authenticode");

#ifdef _WIN32
    std::wstring wpath = exePath.wstring();

    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = wpath.c_str();

    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData{};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;

    LONG status = WinVerifyTrust(NULL, &guidAction, &trustData);

    // Clean up
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &guidAction, &trustData);

    result.valid = (status == ERROR_SUCCESS);
    if (!result.valid) {
        switch (status) {
            case TRUST_E_NOSIGNATURE:
                result.message = "File is not signed";
                MAKINE_LOG_WARN(log::SECURITY, "Authenticode: File is not signed: {}",
                    exePath.string());
                break;
            case TRUST_E_EXPLICIT_DISTRUST:
                result.message = "Signature explicitly distrusted";
                MAKINE_LOG_WARN(log::SECURITY, "Authenticode: Signature explicitly distrusted: {}",
                    exePath.string());
                break;
            case TRUST_E_SUBJECT_NOT_TRUSTED:
                result.message = "Subject not trusted";
                MAKINE_LOG_WARN(log::SECURITY, "Authenticode: Subject not trusted: {}",
                    exePath.string());
                break;
            default:
                result.message = fmt::format("Verification failed: {}", status);
                MAKINE_LOG_WARN(log::SECURITY, "Authenticode verification failed with status {}: {}",
                    status, exePath.string());
        }
        AuditLogger::logSignatureVerification(exePath.string(), false,
            "Authenticode: " + result.message);
        metrics().increment("security.authenticode_verify_failures");
    } else {
        MAKINE_LOG_INFO(log::SECURITY, "Authenticode verification PASSED: {}", exePath.string());
        AuditLogger::logSignatureVerification(exePath.string(), true, "Authenticode: Valid signature");
        metrics().increment("security.authenticode_verify_successes");
    }
#else
    result.message = "Authenticode verification only available on Windows";
    MAKINE_LOG_DEBUG(log::SECURITY, "Authenticode verification skipped (non-Windows platform)");
#endif

    return result;
}

Result<ByteBuffer> SecurityManager::randomBytes(size_t count) const {
    ByteBuffer buffer(count);
#ifdef MAKINE_HAS_SODIUM
    randombytes_buf(buffer.data(), count);
#else
    if (RAND_bytes(buffer.data(), static_cast<int>(count)) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to generate random bytes"));
    }
#endif
    return buffer;
}

Result<std::string> SecurityManager::randomHex(size_t bytes) const {
    auto result = randomBytes(bytes);
    if (!result) {
        return std::unexpected(result.error());
    }
    return impl_->bytesToHex(result->data(), result->size());
}

// IntegrityChecker implementation

Result<std::vector<IntegrityChecker::FileEntry>> IntegrityChecker::createManifest(
    const fs::path& baseDir, const StringList& files
) {
    std::vector<FileEntry> manifest;
    SecurityManager security;

    for (const auto& relPath : files) {
        fs::path fullPath = baseDir / relPath;

        if (!fs::exists(fullPath)) {
            spdlog::warn("File not found for manifest: {}", fullPath.string());
            continue;
        }

        auto hashResult = security.hashFile(fullPath);
        if (!hashResult) {
            spdlog::warn("Failed to hash file: {}", fullPath.string());
            continue;
        }

        FileEntry entry;
        entry.relativePath = relPath;
        entry.hash = *hashResult;
        entry.size = fs::file_size(fullPath);
        entry.modifiedTime = static_cast<uint64_t>(
            fs::last_write_time(fullPath).time_since_epoch().count());

        manifest.push_back(std::move(entry));
    }

    spdlog::info("Created manifest with {} files", manifest.size());
    return manifest;
}

Result<bool> IntegrityChecker::verify(
    const fs::path& baseDir,
    const std::vector<FileEntry>& manifest,
    StringList* modifiedFiles
) {
    SecurityManager security;
    bool allValid = true;

    for (const auto& entry : manifest) {
        fs::path fullPath = baseDir / entry.relativePath;

        if (!fs::exists(fullPath)) {
            spdlog::warn("Missing file: {}", entry.relativePath.string());
            if (modifiedFiles) {
                modifiedFiles->push_back(entry.relativePath.string());
            }
            allValid = false;
            continue;
        }

        auto hashResult = security.hashFile(fullPath);
        if (!hashResult || *hashResult != entry.hash) {
            spdlog::warn("Modified file: {}", entry.relativePath.string());
            if (modifiedFiles) {
                modifiedFiles->push_back(entry.relativePath.string());
            }
            allValid = false;
        }
    }

    return allValid;
}

VoidResult IntegrityChecker::saveManifest(
    const fs::path& path, const std::vector<FileEntry>& manifest
) {
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& entry : manifest) {
            j.push_back({
                {"path", entry.relativePath.string()},
                {"hash", entry.hash},
                {"size", entry.size},
                {"modified", entry.modifiedTime}
            });
        }

        std::ofstream ofs(path);
        if (!ofs) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot create manifest file"));
        }
        ofs << j.dump(2);

        spdlog::debug("Saved manifest to: {}", path.string());
        return {};

    } catch (const std::exception& e) {
        return std::unexpected(Error(ErrorCode::Unknown, e.what()));
    }
}

Result<std::vector<IntegrityChecker::FileEntry>> IntegrityChecker::loadManifest(
    const fs::path& path
) {
    try {
        std::ifstream ifs(path);
        if (!ifs) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "Cannot open manifest file"));
        }

        nlohmann::json j = nlohmann::json::parse(ifs);
        std::vector<FileEntry> manifest;

        for (const auto& item : j) {
            FileEntry entry;
            entry.relativePath = item["path"].get<std::string>();
            entry.hash = item["hash"].get<std::string>();
            entry.size = item["size"].get<uint64_t>();
            entry.modifiedTime = item["modified"].get<uint64_t>();
            manifest.push_back(std::move(entry));
        }

        spdlog::debug("Loaded manifest with {} entries", manifest.size());
        return manifest;

    } catch (const std::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError, e.what()));
    }
}

// =============================================================================
// PathValidator Implementation
// =============================================================================

bool PathValidator::hasNullBytes(const std::string& path) noexcept {
    return path.find('\0') != std::string::npos;
}

bool PathValidator::isUncPath(const std::string& path) noexcept {
    if (path.length() < 2) return false;
    // Check for \\ or // at the start (UNC path)
    return (path[0] == '\\' && path[1] == '\\') ||
           (path[0] == '/' && path[1] == '/');
}

bool PathValidator::hasTraversal(const std::string& path) noexcept {
    // Check for direct ..
    if (path.find("..") != std::string::npos) {
        return true;
    }

    // Check for URL-encoded traversal
    std::string decoded = urlDecode(path);
    if (decoded.find("..") != std::string::npos) {
        return true;
    }

    return false;
}

bool PathValidator::isSafe(const std::string& path) noexcept {
    if (path.empty()) return false;
    if (hasNullBytes(path)) return false;
    if (hasTraversal(path)) return false;
    if (isUncPath(path)) return false;
    return true;
}

std::string PathValidator::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Check if next two chars are hex
            char c1 = str[i + 1];
            char c2 = str[i + 2];

            auto hexValue = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };

            int v1 = hexValue(c1);
            int v2 = hexValue(c2);

            if (v1 >= 0 && v2 >= 0) {
                result += static_cast<char>((v1 << 4) | v2);
                i += 2;
                continue;
            }
        }
        result += str[i];
    }

    return result;
}

std::string PathValidator::sanitize(const std::string& path) {
    std::string result;
    result.reserve(path.size());

    // Remove null bytes
    for (char c : path) {
        if (c != '\0') {
            result += c;
        }
    }

    // Trim leading/trailing whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = result.find_last_not_of(" \t\r\n");
    result = result.substr(start, end - start + 1);

    // Normalize separators
    result = normalizeSeparators(result);

    // Remove consecutive separators
    std::string cleaned;
    cleaned.reserve(result.size());
    char prevChar = 0;
    for (char c : result) {
        bool isSep = (c == '\\' || c == '/');
        bool prevSep = (prevChar == '\\' || prevChar == '/');
        if (!(isSep && prevSep)) {
            cleaned += c;
        }
        prevChar = c;
    }

#ifdef _WIN32
    // Remove trailing dots (Windows special handling)
    while (!cleaned.empty() && cleaned.back() == '.') {
        cleaned.pop_back();
    }
#endif

    return cleaned;
}

std::string PathValidator::normalizeSeparators(const std::string& path) {
    std::string result = path;
#ifdef _WIN32
    // Convert forward slashes to backslashes on Windows
    for (char& c : result) {
        if (c == '/') c = '\\';
    }
#else
    // Convert backslashes to forward slashes on Unix
    for (char& c : result) {
        if (c == '\\') c = '/';
    }
#endif
    return result;
}

bool PathValidator::isUnderDirectory(const fs::path& path, const fs::path& allowedDir) {
    try {
        // Resolve to canonical paths (resolves symlinks and ..)
        fs::path canonicalPath = fs::weakly_canonical(path);
        fs::path canonicalDir = fs::weakly_canonical(allowedDir);

        // Check if path starts with allowedDir
        auto pathStr = canonicalPath.string();
        auto dirStr = canonicalDir.string();

        // Ensure directory string ends with separator for proper prefix matching
        if (!dirStr.empty() && dirStr.back() != fs::path::preferred_separator) {
            dirStr += fs::path::preferred_separator;
        }

        // Path should either be equal to dir or start with dir/
        return pathStr == canonicalDir.string() ||
               pathStr.rfind(dirStr, 0) == 0;  // starts_with

    } catch (const std::exception&) {
        return false;
    }
}

std::optional<fs::path> PathValidator::joinSafe(
    const fs::path& base,
    const std::string& relative
) {
    // Validate the relative path first
    if (hasNullBytes(relative) || hasTraversal(relative) || isUncPath(relative)) {
        return std::nullopt;
    }

    // Sanitize
    std::string sanitized = sanitize(relative);
    if (sanitized.empty()) {
        return std::nullopt;
    }

    // Build the joined path
    fs::path result = base / sanitized;

    // Verify the result is still under base
    if (!isUnderDirectory(result, base)) {
        spdlog::warn("Path escape attempt: {} + {} -> {}",
                     base.string(), relative, result.string());
        return std::nullopt;
    }

    return result;
}

PathValidationResult PathValidator::validate(
    const std::string& path,
    const PathValidationOptions& options
) {
    PathValidationResult result;
    result.valid = false;

    // Empty path check
    if (path.empty()) {
        result.reason = "Empty path";
        return result;
    }

    // Null byte check
    if (!options.allowNullBytes && hasNullBytes(path)) {
        result.reason = "Path contains null bytes";
        return result;
    }

    // UNC path check
    if (!options.allowUncPaths && isUncPath(path)) {
        result.reason = "UNC paths not allowed";
        return result;
    }

    // Traversal check (before and after URL decoding)
    if (!options.allowTraversal && hasTraversal(path)) {
        result.reason = "Path contains traversal sequences";
        return result;
    }

    // Sanitize the path
    std::string sanitized = sanitize(path);
    if (sanitized.empty()) {
        result.reason = "Path empty after sanitization";
        return result;
    }

    // Convert to fs::path for further validation
    fs::path fsPath(sanitized);

    // Relative path check
    if (!options.allowRelative && fsPath.is_relative()) {
        // Try to resolve with base path
        if (!options.basePath.empty()) {
            fsPath = options.basePath / fsPath;
        } else {
            result.reason = "Relative paths not allowed";
            return result;
        }
    }

    // Symlink check
    if (!options.allowSymlinks) {
        try {
            if (fs::exists(fsPath) && fs::is_symlink(fsPath)) {
                result.reason = "Symlinks not allowed";
                return result;
            }
        } catch (const std::exception&) {
            // Path might not exist yet, that's OK
        }
    }

    // Allowed directories check
    if (!options.allowedDirs.empty()) {
        bool allowed = false;
        for (const auto& allowedDir : options.allowedDirs) {
            if (isUnderDirectory(fsPath, allowedDir)) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            result.reason = "Path not under allowed directories";
            return result;
        }
    }

    result.valid = true;
    result.sanitizedPath = fsPath.string();
    return result;
}

// =============================================================================
// PathGuard Implementation
// =============================================================================

PathGuard::PathGuard(const std::string& path, const PathValidationOptions& options)
    : result_(PathValidator::validate(path, options))
{
    if (result_.valid) {
        safePath_ = fs::path(result_.sanitizedPath);
    }
}

} // namespace makine
