/**
 * @file file_integrity.cpp
 * @brief File integrity verification implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/file_integrity.hpp"
#include "makine/logging.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

namespace makine::integrity {

namespace {

// RAII wrapper for OpenSSL EVP_MD_CTX
struct EvpCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept {
        if (ctx) EVP_MD_CTX_free(ctx);
    }
};
using EvpCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpCtxDeleter>;

// Convert raw hash bytes to lowercase hex string
std::string bytesToHex(const unsigned char* data, unsigned int len) {
    static constexpr char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        result.push_back(hexChars[(data[i] >> 4) & 0x0F]);
        result.push_back(hexChars[data[i] & 0x0F]);
    }
    return result;
}

// Trim whitespace from both ends
std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
        sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
        sv.remove_suffix(1);
    return sv;
}

} // anonymous namespace

Result<std::string> computeFileHash(const fs::path& filePath, size_t chunkSize) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            fmt::format("Cannot open file for hashing: {}", filePath.string())));
    }

    EvpCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        return std::unexpected(Error(ErrorCode::Unknown, "Failed to create EVP_MD_CTX"));
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown, "EVP_DigestInit_ex failed"));
    }

    std::vector<char> buffer(chunkSize);
    while (file.read(buffer.data(), static_cast<std::streamsize>(chunkSize)) || file.gcount() > 0) {
        auto bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        if (EVP_DigestUpdate(ctx.get(), buffer.data(), static_cast<size_t>(bytesRead)) != 1) {
            return std::unexpected(Error(ErrorCode::Unknown, "EVP_DigestUpdate failed"));
        }
    }

    if (file.bad()) {
        return std::unexpected(Error(ErrorCode::FileCorrupted,
            fmt::format("I/O error reading file: {}", filePath.string())));
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &hashLen) != 1) {
        return std::unexpected(Error(ErrorCode::Unknown, "EVP_DigestFinal_ex failed"));
    }

    return bytesToHex(hash.data(), hashLen);
}

Result<std::string> readHashFile(const fs::path& hashFilePath) {
    std::ifstream file(hashFilePath);
    if (!file) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            fmt::format("Hash file not found: {}", hashFilePath.string())));
    }

    std::string line;
    if (!std::getline(file, line) || line.empty()) {
        return std::unexpected(Error(ErrorCode::FileCorrupted,
            fmt::format("Empty hash file: {}", hashFilePath.string())));
    }

    auto trimmed = trim(line);

    // Extract first token (hash) — split on whitespace
    auto spacePos = trimmed.find_first_of(" \t");
    auto hashStr = (spacePos != std::string_view::npos)
        ? trimmed.substr(0, spacePos)
        : trimmed;

    // Convert to lowercase
    std::string hash(hashStr);
    std::transform(hash.begin(), hash.end(), hash.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (!isValidSha256Hex(hash)) {
        return std::unexpected(Error(ErrorCode::FileCorrupted,
            fmt::format("Invalid SHA-256 hash format in: {}", hashFilePath.string())));
    }

    return hash;
}

Result<bool> verifyFile(const fs::path& filePath) {
    fs::path hashPath = filePath;
    hashPath += ".sha256";

    if (!fs::exists(hashPath)) {
        // No hash file = dev build, not an error but no verification possible
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "No .sha256 sidecar file found"));
    }

    auto expectedResult = readHashFile(hashPath);
    if (!expectedResult) return std::unexpected(expectedResult.error());

    auto actualResult = computeFileHash(filePath);
    if (!actualResult) return std::unexpected(actualResult.error());

    return secureCompareHex(*actualResult, *expectedResult);
}

bool secureCompareHex(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;

    // Constant-time comparison to prevent timing attacks
    volatile unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

bool isValidSha256Hex(std::string_view hex) noexcept {
    if (hex.size() != 64) return false;
    return std::all_of(hex.begin(), hex.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

} // namespace makine::integrity
