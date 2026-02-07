/**
 * @file package_manager.cpp
 * @brief Package manager implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/package_manager.hpp"
#include "makineai/core.hpp"
#include "makineai/patch_engine.hpp"
#include "makineai/runtime_manager.hpp"
#include "makineai/features.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"
#include "makineai/validation.hpp"
#include "makineai/ssl_pinning.hpp"

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <algorithm>
#include <fstream>
#include <chrono>

#ifdef MAKINEAI_HAS_MINIZIP
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>
#endif

namespace makineai {

using json = nlohmann::json;

// ZIP extraction helper
namespace {

#ifdef MAKINEAI_HAS_MINIZIP
Result<std::vector<std::pair<std::string, fs::path>>> extractZip(
    const fs::path& zipPath,
    const fs::path& destDir
) {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Extracting ZIP: {} -> {}", zipPath.string(), destDir.string());
    MAKINEAI_TIMED_SCOPE(log::PACKAGE, "zip_extraction");
    auto extractTimer = metrics().timer("zip_extraction");

    // Validate paths
    if (!validation::isPathSafe(zipPath)) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Unsafe ZIP path: {}", zipPath.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument, "Unsafe ZIP path"));
    }

    if (!validation::isPathSafe(destDir)) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Unsafe destination path: {}", destDir.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument, "Unsafe destination path"));
    }

    std::vector<std::pair<std::string, fs::path>> extractedFiles;

    void* reader = mz_zip_reader_create();
    if (!reader) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to create ZIP reader");
        return std::unexpected(Error(ErrorCode::ExtractionFailed, "Failed to create ZIP reader"));
    }

    int32_t err = mz_zip_reader_open_file(reader, zipPath.string().c_str());
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to open ZIP file: {}", zipPath.string());
        AuditLogger::logFileAccess(zipPath, "read", false, "Cannot open ZIP");
        return std::unexpected(Error(ErrorCode::ExtractionFailed,
            "Failed to open ZIP file: " + zipPath.string()));
    }

    AuditLogger::logFileAccess(zipPath, "read", true, "Opened for extraction");

    // Get canonical destination for path traversal check
    auto destCanonical = fs::weakly_canonical(destDir);

    err = mz_zip_reader_goto_first_entry(reader);
    int skippedEntries = 0;

    while (err == MZ_OK) {
        mz_zip_file* fileInfo = nullptr;
        err = mz_zip_reader_entry_get_info(reader, &fileInfo);
        if (err != MZ_OK) break;

        std::string entryName = fileInfo->filename;

        // Security: Check for path traversal
        if (entryName.find("..") != std::string::npos) {
            MAKINEAI_LOG_WARN(log::SECURITY, "Skipping entry with path traversal: {}", entryName);
            AuditLogger::logFileAccess(destDir / entryName, "extract", false, "Path traversal attempt");
            skippedEntries++;
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // Skip directories
        if (mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
            fs::path dirPath = destDir / entryName;

            // Validate destination is within target directory
            auto dirCanonical = fs::weakly_canonical(dirPath);
            if (dirCanonical.string().find(destCanonical.string()) != 0) {
                MAKINEAI_LOG_WARN(log::SECURITY, "Directory path escapes target: {}", entryName);
                skippedEntries++;
                err = mz_zip_reader_goto_next_entry(reader);
                continue;
            }

            std::error_code ec;
            fs::create_directories(dirPath, ec);
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        // Extract file
        fs::path destPath = destDir / entryName;

        // Validate destination is within target directory
        auto fileCanonical = fs::weakly_canonical(destPath);
        if (fileCanonical.string().find(destCanonical.string()) != 0) {
            MAKINEAI_LOG_WARN(log::SECURITY, "File path escapes target: {}", entryName);
            AuditLogger::logFileAccess(destPath, "extract", false, "Path traversal attempt");
            skippedEntries++;
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        std::error_code ec;
        fs::create_directories(destPath.parent_path(), ec);

        err = mz_zip_reader_entry_save_file(reader, destPath.string().c_str());
        if (err == MZ_OK) {
            extractedFiles.emplace_back(entryName, destPath);
            MAKINEAI_LOG_TRACE(log::FILE, "Extracted: {}", entryName);
        } else {
            MAKINEAI_LOG_WARN(log::FILE, "Failed to extract: {} (error {})", entryName, err);
            AuditLogger::logFileAccess(destPath, "extract", false, "Extraction failed");
        }

        err = mz_zip_reader_goto_next_entry(reader);
    }

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);

    if (skippedEntries > 0) {
        MAKINEAI_LOG_WARN(log::SECURITY, "Skipped {} potentially unsafe entries", skippedEntries);
    }

    MAKINEAI_LOG_INFO(log::PACKAGE, "Extracted {} files from {} (skipped {})",
        extractedFiles.size(), zipPath.string(), skippedEntries);
    AuditLogger::logDataExport("zip_extraction", destDir, true);
    metrics().increment("zip_extractions");
    metrics().recordHistogram("zip_extracted_files", static_cast<int64_t>(extractedFiles.size()));

    return extractedFiles;
}

#else
// Fallback without minizip - just copy file for now
Result<std::vector<std::pair<std::string, fs::path>>> extractZip(
    const fs::path& zipPath,
    const fs::path& destDir
) {
    (void)zipPath;
    (void)destDir;
    MAKINEAI_LOG_ERROR(log::PACKAGE, "ZIP extraction not available - minizip-ng not compiled");
    return std::unexpected(Error(ErrorCode::NotSupported,
        "ZIP extraction not available - minizip-ng not compiled"));
}
#endif

} // anonymous namespace

// Downloader implementation
class Downloader::Impl {
public:
    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~Impl() {
        curl_global_cleanup();
    }

    static size_t writeCallback(void* contents, size_t size, size_t nmemb,
                                ByteBuffer* buffer) {
        size_t totalSize = size * nmemb;
        auto* bytes = static_cast<uint8_t*>(contents);
        buffer->insert(buffer->end(), bytes, bytes + totalSize);
        return totalSize;
    }

    static size_t writeFileCallback(void* contents, size_t size, size_t nmemb,
                                    std::ofstream* file) {
        size_t totalSize = size * nmemb;
        file->write(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    static int progressCallback(void* clientp, curl_off_t dltotal,
                                curl_off_t dlnow, curl_off_t /*ultotal*/,
                                curl_off_t /*ulnow*/) {
        auto* ctx = static_cast<std::pair<DownloadProgressCallback*,
                                          CancellationToken*>*>(clientp);

        if (ctx->second && ctx->second->isCancelled()) {
            return 1;  // Abort transfer
        }

        if (ctx->first && *ctx->first && dltotal > 0) {
            DownloadProgress progress;
            progress.bytesDownloaded = static_cast<uint64_t>(dlnow);
            progress.totalBytes = static_cast<uint64_t>(dltotal);
            progress.speedBytesPerSec = 0;  // CURL can provide this
            (*ctx->first)(progress);
        }

        return 0;
    }
};

Downloader::Downloader() : impl_(std::make_unique<Impl>()) {}
Downloader::~Downloader() = default;

Result<ByteBuffer> Downloader::download(
    const std::string& url,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    MAKINEAI_LOG_INFO(log::NETWORK, "Starting download from: {}", url);
    auto downloadTimer = metrics().timer("download_to_memory");

    // Validate URL
    auto urlResult = validation::validateUrl(url);
    if (!urlResult) {
        MAKINEAI_LOG_ERROR(log::NETWORK, "Invalid URL: {}", url);
        AuditLogger::logNetworkRequest(url, "GET", false, "Invalid URL format");
        return std::unexpected(urlResult.error());
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        MAKINEAI_LOG_ERROR(log::NETWORK, "Failed to initialize CURL");
        return std::unexpected(Error(ErrorCode::NetworkError,
            "Failed to initialize CURL"));
    }

    ByteBuffer buffer;
    std::pair<DownloadProgressCallback*, CancellationToken*> ctx{&progress, cancel};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Impl::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    ssl::applySslPinning(curl, url);

    if (progress || cancel) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Impl::progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            MAKINEAI_LOG_WARN(log::NETWORK, "Download cancelled by user: {}", url);
            AuditLogger::logNetworkRequest(url, "GET", false, "Cancelled by user");
            return std::unexpected(Error(ErrorCode::Cancelled,
                "Download cancelled"));
        }

        MAKINEAI_LOG_ERROR(log::NETWORK, "Download failed: {} - {}", url, curl_easy_strerror(res));
        AuditLogger::logNetworkRequest(url, "GET", false, curl_easy_strerror(res));
        return std::unexpected(Error(ErrorCode::DownloadFailed,
            std::string("Download failed: ") + curl_easy_strerror(res)));
    }

    long httpCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        MAKINEAI_LOG_ERROR(log::NETWORK, "HTTP error {} for: {}", httpCode, url);
        AuditLogger::logNetworkRequest(url, "GET", false, "HTTP " + std::to_string(httpCode));
        return std::unexpected(Error(ErrorCode::ServerError,
            "HTTP error: " + std::to_string(httpCode)));
    }

    MAKINEAI_LOG_INFO(log::NETWORK, "Download complete: {} ({} bytes)", url, buffer.size());
    AuditLogger::logNetworkRequest(url, "GET", true, "Downloaded " + std::to_string(buffer.size()) + " bytes");
    metrics().increment("downloads_completed");
    metrics().recordHistogram("download_size_bytes", static_cast<int64_t>(buffer.size()));

    return buffer;
}

VoidResult Downloader::downloadToFile(
    const std::string& url,
    const fs::path& destination,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    MAKINEAI_LOG_INFO(log::NETWORK, "Starting file download: {} -> {}", url, destination.string());
    auto downloadTimer = metrics().timer("download_to_file");

    // Validate URL
    auto urlResult = validation::validateUrl(url);
    if (!urlResult) {
        MAKINEAI_LOG_ERROR(log::NETWORK, "Invalid URL: {}", url);
        AuditLogger::logNetworkRequest(url, "GET", false, "Invalid URL format");
        return std::unexpected(urlResult.error());
    }

    // Validate destination path
    if (!validation::isPathSafe(destination)) {
        MAKINEAI_LOG_ERROR(log::FILE, "Unsafe destination path: {}", destination.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Unsafe destination path: " + destination.string()));
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        MAKINEAI_LOG_ERROR(log::NETWORK, "Failed to initialize CURL");
        return std::unexpected(Error(ErrorCode::NetworkError,
            "Failed to initialize CURL"));
    }

    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    if (ec) {
        MAKINEAI_LOG_WARN(log::FILE, "Failed to create parent directories: {}", ec.message());
    }

    std::ofstream file(destination, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
        MAKINEAI_LOG_ERROR(log::FILE, "Cannot create file: {}", destination.string());
        AuditLogger::logFileAccess(destination, "create", false, "Permission denied");
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot create file: " + destination.string()));
    }

    std::pair<DownloadProgressCallback*, CancellationToken*> ctx{&progress, cancel};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Impl::writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    ssl::applySslPinning(curl, url);

    if (progress || cancel) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Impl::progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    }

    CURLcode res = curl_easy_perform(curl);
    file.close();

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        fs::remove(destination, ec);  // Clean up partial file

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            MAKINEAI_LOG_WARN(log::NETWORK, "Download cancelled by user: {}", url);
            AuditLogger::logNetworkRequest(url, "GET", false, "Cancelled by user");
            return std::unexpected(Error(ErrorCode::Cancelled,
                "Download cancelled"));
        }

        MAKINEAI_LOG_ERROR(log::NETWORK, "Download failed: {} - {}", url, curl_easy_strerror(res));
        AuditLogger::logNetworkRequest(url, "GET", false, curl_easy_strerror(res));
        return std::unexpected(Error(ErrorCode::DownloadFailed,
            std::string("Download failed: ") + curl_easy_strerror(res)));
    }

    long httpCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        fs::remove(destination, ec);
        MAKINEAI_LOG_ERROR(log::NETWORK, "HTTP error {} for: {}", httpCode, url);
        AuditLogger::logNetworkRequest(url, "GET", false, "HTTP " + std::to_string(httpCode));
        return std::unexpected(Error(ErrorCode::ServerError,
            "HTTP error: " + std::to_string(httpCode)));
    }

    // Get file size for logging
    auto fileSize = fs::file_size(destination, ec);
    MAKINEAI_LOG_INFO(log::NETWORK, "File download complete: {} ({} bytes)", destination.string(), fileSize);
    AuditLogger::logNetworkRequest(url, "GET", true, "Downloaded to " + destination.string());
    AuditLogger::logFileAccess(destination, "write", true, std::to_string(fileSize) + " bytes");
    metrics().increment("file_downloads_completed");
    metrics().recordHistogram("file_download_size_bytes", static_cast<int64_t>(fileSize));

    return {};
}

// PackageManager implementation
PackageManager::PackageManager() {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Initializing PackageManager");
    loadInstalledInfo();
    MAKINEAI_LOG_INFO(log::PACKAGE, "PackageManager initialized with {} installed packages", installedPackages_.size());
}

PackageManager::~PackageManager() {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "PackageManager destroyed");
}

Result<PackageManifest> PackageManager::fetchManifest(CancellationToken* cancel) {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Fetching package manifest from API");
    MAKINEAI_TIMED_SCOPE(log::PACKAGE, "fetch_manifest");
    auto fetchTimer = metrics().timer("manifest_fetch");

    Downloader downloader;
    std::string manifestUrl = apiUrl_ + "/manifest";

    auto result = downloader.download(manifestUrl, nullptr, cancel);
    if (!result) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to fetch manifest: {}", result.error().message());
        metrics().increment("manifest_fetch_failures");
        return std::unexpected(result.error());
    }

    try {
        std::string jsonStr(result->begin(), result->end());
        json manifestJson = json::parse(jsonStr);

        manifest_.version = manifestJson.value("version", "");
        manifest_.updatedAt = manifestJson.value("updated", 0ULL);
        manifest_.packages.clear();

        for (const auto& pkgJson : manifestJson["packages"]) {
            TranslationPackage pkg;
            pkg.packageId = pkgJson.value("id", "");
            pkg.gameId = pkgJson.value("gameId", "");
            pkg.gameName = pkgJson.value("name", "");
            pkg.version = pkgJson.value("version", "");
            pkg.downloadUrl = pkgJson.value("url", "");
            pkg.signature = pkgJson.value("signature", "");
            pkg.sizeBytes = pkgJson.value("size", 0ULL);
            pkg.sha256 = pkgJson.value("sha256", "");

            // Validate package ID format
            auto pkgIdResult = validation::validatePackageId(pkg.packageId);
            if (!pkgIdResult) {
                MAKINEAI_LOG_WARN(log::PACKAGE, "Skipping package with invalid ID: {}", pkg.packageId);
                continue;
            }

            if (pkgJson.contains("gameVersions")) {
                for (const auto& v : pkgJson["gameVersions"]) {
                    pkg.supportedGameVersions.push_back(v.get<std::string>());
                }
            }

            if (pkgJson.contains("gameHashes")) {
                for (const auto& h : pkgJson["gameHashes"]) {
                    pkg.supportedGameHashes.push_back(h.get<std::string>());
                }
            }

            std::string engine = pkgJson.value("engine", "unknown");
            if (engine == "unity_mono") pkg.targetEngine = GameEngine::Unity_Mono;
            else if (engine == "unity_il2cpp") pkg.targetEngine = GameEngine::Unity_IL2CPP;
            else if (engine == "unreal") pkg.targetEngine = GameEngine::Unreal;
            else if (engine == "bethesda") pkg.targetEngine = GameEngine::Bethesda;
            else if (engine == "gamemaker") pkg.targetEngine = GameEngine::GameMaker;
            else pkg.targetEngine = GameEngine::Unknown;

            pkg.requiresRuntime = pkgJson.value("requiresRuntime", false);

            manifest_.packages.push_back(std::move(pkg));
        }

        MAKINEAI_LOG_INFO(log::PACKAGE, "Fetched manifest v{} with {} packages", manifest_.version, manifest_.packages.size());
        metrics().increment("manifest_fetches");
        metrics().gauge("manifest_package_count", static_cast<double>(manifest_.packages.size()));
        return manifest_;

    } catch (const json::exception& e) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to parse manifest JSON: {}", e.what());
        metrics().increment("manifest_parse_failures");
        return std::unexpected(Error(ErrorCode::ParseError,
            std::string("Invalid manifest JSON: ") + e.what()));
    }
}

Result<TranslationPackage> PackageManager::findPackage(const GameInfo& game) const {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Finding package for game: {} ({})", game.name, game.id.storeId);

    for (const auto& pkg : manifest_.packages) {
        // Check by store ID
        if (pkg.gameId == game.id.storeId) {
            MAKINEAI_LOG_DEBUG(log::PACKAGE, "Found package by store ID: {}", pkg.packageId);
            return pkg;
        }

        // Check by hash
        for (const auto& hash : pkg.supportedGameHashes) {
            if (hash == game.id.exeHash) {
                MAKINEAI_LOG_DEBUG(log::PACKAGE, "Found package by hash: {}", pkg.packageId);
                return pkg;
            }
        }
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "No translation package found for: {}", game.name);
    return std::unexpected(Error(ErrorCode::GameNotFound,
        "No translation package found for: " + game.name));
}

Result<TranslationPackage> PackageManager::getPackage(const std::string& packageId) const {
    for (const auto& pkg : manifest_.packages) {
        if (pkg.packageId == packageId) {
            return pkg;
        }
    }

    return std::unexpected(Error(ErrorCode::GameNotFound,
        "Package not found: " + packageId));
}

bool PackageManager::hasTranslation(const GameInfo& game) const {
    return findPackage(game).has_value();
}

Result<fs::path> PackageManager::download(
    const TranslationPackage& package,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Downloading package: {} v{}", package.packageId, package.version);
    MAKINEAI_TIMED_SCOPE_INFO(log::PACKAGE, "package_download");
    auto downloadTimer = metrics().timer("package_download");

    // Validate package ID
    auto pkgIdResult = validation::validatePackageId(package.packageId);
    if (!pkgIdResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Invalid package ID: {}", package.packageId);
        return std::unexpected(pkgIdResult.error());
    }

    Downloader downloader;

    std::string url = package.downloadUrl;
    if (!url.starts_with("http")) {
        url = apiUrl_ + url;
    }

    fs::path cachePath = cacheDir_ / (package.packageId + ".zip");

    // Check if already cached
    if (fs::exists(cachePath)) {
        MAKINEAI_LOG_DEBUG(log::PACKAGE, "Found cached package, verifying checksum: {}", cachePath.string());

        // Verify checksum
        auto& security = Core::instance().securityManager();
        auto hashResult = security.hashFile(cachePath);
        if (hashResult && *hashResult == package.sha256) {
            MAKINEAI_LOG_INFO(log::PACKAGE, "Using cached package (checksum verified): {}", package.packageId);
            metrics().increment("package_cache_hits");
            return cachePath;
        }

        // Invalid cache, remove
        MAKINEAI_LOG_WARN(log::PACKAGE, "Cached package has invalid checksum, re-downloading: {}", package.packageId);
        metrics().increment("package_cache_misses");
        std::error_code ec;
        fs::remove(cachePath, ec);
    }

    // Download
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Starting download from: {}", url);
    auto startTime = std::chrono::steady_clock::now();

    auto downloadProgress = [&progress, &package](const DownloadProgress& p) {
        if (progress) {
            DownloadProgress dp = p;
            dp.currentFile = package.packageId + ".zip";
            progress(dp);
        }
    };

    auto result = downloader.downloadToFile(url, cachePath, downloadProgress, cancel);
    if (!result) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Download failed for package {}: {}", package.packageId, result.error().message());
        metrics().increment("package_download_failures");
        return std::unexpected(result.error());
    }

    auto endTime = std::chrono::steady_clock::now();
    auto downloadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    MAKINEAI_LOG_INFO(log::PACKAGE, "Download completed in {}ms", downloadDuration.count());
    metrics().recordDuration("package_download_time", downloadDuration);

    // Verify checksum
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Verifying package checksum: {}", package.sha256);
    auto verifyResult = verifyPackage(cachePath, package);
    if (!verifyResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Package verification failed: {}", verifyResult.error().message());
        AuditLogger::logSignatureVerification(package.packageId, false, verifyResult.error().message());
        std::error_code ec;
        fs::remove(cachePath, ec);
        metrics().increment("package_verification_failures");
        return std::unexpected(verifyResult.error());
    }

    MAKINEAI_LOG_INFO(log::PACKAGE, "Package verified successfully: {}", package.packageId);
    AuditLogger::logSignatureVerification(package.packageId, true);
    metrics().increment("packages_downloaded");

    return cachePath;
}

VoidResult PackageManager::verifyPackage(
    const fs::path& packagePath,
    const TranslationPackage& package
) {
    MAKINEAI_LOG_DEBUG(log::SECURITY, "Verifying package: {} at {}", package.packageId, packagePath.string());
    MAKINEAI_TIMED_SCOPE(log::SECURITY, "package_verification");

    // Validate paths
    if (!validation::isPathSafe(packagePath)) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Unsafe package path detected: {}", packagePath.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Unsafe package path"));
    }

    auto& security = Core::instance().securityManager();

    // Validate expected checksum format
    if (!package.sha256.empty()) {
        auto hashResult = validation::validateSha256(package.sha256);
        if (!hashResult) {
            MAKINEAI_LOG_ERROR(log::SECURITY, "Invalid SHA256 format in package manifest: {}", package.sha256);
            return std::unexpected(hashResult.error());
        }
    }

    // Verify checksum
    MAKINEAI_LOG_DEBUG(log::SECURITY, "Computing file hash for: {}", packagePath.string());
    auto hashResult = security.hashFile(packagePath);
    if (!hashResult) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Failed to compute hash: {}", hashResult.error().message());
        return std::unexpected(hashResult.error());
    }

    if (*hashResult != package.sha256) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Checksum mismatch for {}: expected {} got {}",
            package.packageId, package.sha256, *hashResult);
        AuditLogger::logSignatureVerification(package.packageId, false,
            "Checksum mismatch: expected=" + package.sha256 + " actual=" + *hashResult);
        return std::unexpected(Error(ErrorCode::ChecksumMismatch,
            "Package checksum mismatch"));
    }

    MAKINEAI_LOG_DEBUG(log::SECURITY, "Checksum verified: {}", package.sha256);

    // MANDATORY: Verify package signature
    // Signature verification is required for all packages in production.
    if (!security.hasPublicKey()) {
        MAKINEAI_LOG_ERROR(log::SECURITY,
            "No public key loaded — cannot verify package signature for: {}",
            package.packageId);
        AuditLogger::logSignatureVerification(package.packageId, false,
            "No public key loaded — signature verification impossible");
        return std::unexpected(Error(ErrorCode::SignatureRequired,
            "Package signature verification requires a loaded public key"));
    }

    if (package.signature.empty()) {
        MAKINEAI_LOG_ERROR(log::SECURITY,
            "Package has no signature: {}", package.packageId);
        AuditLogger::logSignatureVerification(package.packageId, false,
            "Package missing signature — rejected");
        return std::unexpected(Error(ErrorCode::SignatureRequired,
            "Package must have a valid signature"));
    }

    MAKINEAI_LOG_DEBUG(log::SECURITY, "Verifying package signature");

    // Read package for signature verification
    std::ifstream file(packagePath, std::ios::binary);
    if (!file) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Cannot read package file for signature verification");
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot read package file"));
    }

    ByteBuffer data((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    auto sigResult = security.verifySignature(data, package.signature);
    if (!sigResult || !sigResult->valid) {
        MAKINEAI_LOG_ERROR(log::SECURITY, "Package signature invalid for: {}", package.packageId);
        AuditLogger::logSignatureVerification(package.packageId, false, "Invalid signature");
        return std::unexpected(Error(ErrorCode::SignatureInvalid,
            "Package signature invalid"));
    }

    MAKINEAI_LOG_DEBUG(log::SECURITY, "Package signature verified");

    MAKINEAI_LOG_INFO(log::SECURITY, "Package verification successful: {}", package.packageId);
    return {};
}

Result<PatchResult> PackageManager::install(
    const TranslationPackage& package,
    const GameInfo& game,
    ProgressCallback progress,
    CancellationToken* cancel
) {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Installing package {} v{} for game: {}",
        package.packageId, package.version, game.name);
    MAKINEAI_TIMED_SCOPE_INFO(log::PACKAGE, "package_install");
    auto installTimer = metrics().timer("package_install");

    // Audit log: Starting installation
    AuditLogger::logPatchOperation(game.id.storeId, true, "install_start",
        "package=" + package.packageId + " version=" + package.version);

    // Validate game install path
    if (!validation::isPathSafe(game.installPath)) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Unsafe game install path: {}", game.installPath.string());
        AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed", "Unsafe game path");
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Unsafe game install path"));
    }

    // Download package
    if (progress) progress(0, 5, "Downloading translation package...");

    auto downloadResult = download(package, nullptr, cancel);
    if (!downloadResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Package download failed: {}", downloadResult.error().message());
        AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed",
            "Download failed: " + downloadResult.error().message());
        metrics().increment("package_install_failures");
        return std::unexpected(downloadResult.error());
    }

    // Check if runtime is needed
    if (package.requiresRuntime) {
        MAKINEAI_LOG_INFO(log::PACKAGE, "Package requires runtime, installing BepInEx/XUnity");
        if (progress) progress(1, 5, "Installing translation runtime...");

        auto& runtime = Core::instance().runtimeManager();
        auto runtimeResult = runtime.install(game);
        if (!runtimeResult) {
            MAKINEAI_LOG_ERROR(log::RUNTIME, "Runtime installation failed: {}", runtimeResult.error().message());
            AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed",
                "Runtime install failed: " + runtimeResult.error().message());
            metrics().increment("runtime_install_failures");
            return std::unexpected(runtimeResult.error());
        }

        // Extract translations to runtime folder
        if (progress) progress(2, 5, "Extracting translations...");

        // Extract ZIP to BepInEx/Translation folder
        fs::path translationDir = game.installPath / "BepInEx" / "Translation";

        // Validate extraction path
        if (!validation::isPathSafe(translationDir)) {
            MAKINEAI_LOG_ERROR(log::PACKAGE, "Unsafe extraction path: {}", translationDir.string());
            return std::unexpected(Error(ErrorCode::InvalidArgument, "Unsafe extraction path"));
        }

        std::error_code ec;
        fs::create_directories(translationDir, ec);
        if (ec) {
            MAKINEAI_LOG_ERROR(log::FILE, "Failed to create translation directory: {}", ec.message());
        }

        MAKINEAI_LOG_DEBUG(log::PACKAGE, "Extracting package to: {}", translationDir.string());
        auto extractResult = extractZip(*downloadResult, translationDir);
        if (!extractResult) {
            MAKINEAI_LOG_ERROR(log::PACKAGE, "Extraction failed: {}", extractResult.error().message());
            AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed",
                "Extraction failed: " + extractResult.error().message());
            return std::unexpected(extractResult.error());
        }

        MAKINEAI_LOG_INFO(log::PACKAGE, "Extracted {} files", extractResult->size());
        AuditLogger::logFileAccess(translationDir, "extract", true,
            std::to_string(extractResult->size()) + " files");

        PatchResult result;
        result.success = true;
        result.message = "Runtime translation installed";
        result.filesPatched = static_cast<int>(extractResult->size());
        result.filesFailed = 0;

        // Save installed info
        InstalledPackage installed;
        installed.packageId = package.packageId;
        installed.gameId = game.id.storeId;
        installed.version = package.version;
        installed.installPath = game.installPath;
        installed.installedAt = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        installed.gameHashAtInstall = game.id.exeHash;

        if (auto saveResult = saveInstalledInfo(installed); !saveResult) {
            MAKINEAI_LOG_WARN(log::PACKAGE, "Failed to save install info: {}", saveResult.error().message());
        }

        if (progress) progress(5, 5, "Installation complete");

        // Audit log: Installation complete
        AuditLogger::logPatchOperation(game.id.storeId, true, "install_complete",
            "package=" + package.packageId + " files=" + std::to_string(result.filesPatched));
        MAKINEAI_LOG_INFO(log::PACKAGE, "Package installation complete: {} ({} files)",
            package.packageId, result.filesPatched);
        metrics().increment("packages_installed");
        metrics().increment("runtime_installs");

        return result;
    }

    // For non-runtime packages, use patch engine
    MAKINEAI_LOG_INFO(log::PACKAGE, "Installing via patch engine (non-runtime)");
    if (progress) progress(1, 5, "Creating backup...");

    auto& patcher = Core::instance().patchEngine();

    // Extract ZIP to temp directory
    fs::path extractDir = cacheDir_ / "extracted" / package.packageId;

    // Validate extraction path
    if (!validation::isPathSafe(extractDir)) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Unsafe extraction directory: {}", extractDir.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument, "Unsafe extraction directory"));
    }

    std::error_code ec;
    fs::remove_all(extractDir, ec);  // Clean previous extraction
    fs::create_directories(extractDir, ec);

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Extracting package to temp: {}", extractDir.string());
    auto extractResult = extractZip(*downloadResult, extractDir);
    if (!extractResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Extraction to temp failed: {}", extractResult.error().message());
        AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed",
            "Extraction failed: " + extractResult.error().message());
        return std::unexpected(extractResult.error());
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Extracted {} files to temp directory", extractResult->size());

    // Create patch operations from extracted files
    std::vector<PatchOperation> operations;
    for (const auto& [entryName, sourcePath] : *extractResult) {
        // Validate entry name for path traversal
        if (entryName.find("..") != std::string::npos) {
            MAKINEAI_LOG_WARN(log::SECURITY, "Skipping potentially malicious path: {}", entryName);
            continue;
        }

        // Determine target path (relative to game install)
        fs::path targetPath = game.installPath / entryName;

        // Validate target path is within game directory
        auto targetCanonical = fs::weakly_canonical(targetPath);
        auto gameCanonical = fs::weakly_canonical(game.installPath);
        if (targetCanonical.string().find(gameCanonical.string()) != 0) {
            MAKINEAI_LOG_WARN(log::SECURITY, "Path traversal detected, skipping: {}", entryName);
            continue;
        }

        PatchOperation op;
        op.type = PatchOperation::Type::Copy;
        op.source = sourcePath;
        op.target = targetPath;
        operations.push_back(std::move(op));
    }

    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Created {} patch operations", operations.size());

    if (progress) progress(3, 5, "Applying translations...");

    auto patchResult = patcher.apply(operations, game, package.version, progress, cancel);

    if (patchResult) {
        // Save installed info
        InstalledPackage installed;
        installed.packageId = package.packageId;
        installed.gameId = game.id.storeId;
        installed.version = package.version;
        installed.installPath = game.installPath;
        installed.installedAt = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        installed.gameHashAtInstall = game.id.exeHash;

        if (auto saveResult = saveInstalledInfo(installed); !saveResult) {
            MAKINEAI_LOG_WARN(log::PACKAGE, "Failed to save install info: {}", saveResult.error().message());
        }

        // Audit log: Installation complete
        AuditLogger::logPatchOperation(game.id.storeId, true, "install_complete",
            "package=" + package.packageId + " files=" + std::to_string(patchResult->filesPatched));
        MAKINEAI_LOG_INFO(log::PACKAGE, "Package installation complete: {} ({} files patched, {} failed)",
            package.packageId, patchResult->filesPatched, patchResult->filesFailed);
        metrics().increment("packages_installed");
        metrics().increment("patch_installs");
    } else {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Patch application failed");
        AuditLogger::logPatchOperation(game.id.storeId, false, "install_failed", "Patch failed");
        metrics().increment("package_install_failures");
    }

    return patchResult;
}

Result<RestoreResult> PackageManager::uninstall(const GameInfo& game) {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Uninstalling translation for game: {} ({})", game.name, game.id.storeId);
    MAKINEAI_TIMED_SCOPE_INFO(log::PACKAGE, "package_uninstall");

    // Audit log: Starting uninstall
    AuditLogger::logPatchOperation(game.id.storeId, true, "uninstall_start", "game=" + game.name);

    auto& patcher = Core::instance().patchEngine();
    auto& runtime = Core::instance().runtimeManager();

    // Check if runtime was installed
    auto statusResult = runtime.checkStatus(game);
    if (statusResult && statusResult->installed) {
        MAKINEAI_LOG_INFO(log::RUNTIME, "Uninstalling translation runtime");
        // Uninstall runtime
        auto uninstallResult = runtime.uninstall(game);
        if (!uninstallResult) {
            MAKINEAI_LOG_ERROR(log::RUNTIME, "Runtime uninstall failed: {}", uninstallResult.error().message());
            AuditLogger::logPatchOperation(game.id.storeId, false, "uninstall_failed",
                "Runtime uninstall failed: " + uninstallResult.error().message());
            return std::unexpected(uninstallResult.error());
        }
        MAKINEAI_LOG_INFO(log::RUNTIME, "Runtime uninstalled successfully");
    }

    // Restore backup if exists
    std::string backupId = game.id.storeId;
    if (patcher.hasBackup(backupId)) {
        MAKINEAI_LOG_INFO(log::PACKAGE, "Restoring backup: {}", backupId);
        auto restoreResult = patcher.restore(game.installPath, backupId);
        if (restoreResult && restoreResult->success) {
            // Remove from installed packages list
            removeInstalledInfo(game.id.storeId);

            // Audit log: Uninstall complete
            AuditLogger::logPatchOperation(game.id.storeId, true, "uninstall_complete",
                "files_restored=" + std::to_string(restoreResult->filesRestored));
            MAKINEAI_LOG_INFO(log::PACKAGE, "Uninstall complete: {} files restored", restoreResult->filesRestored);
            metrics().increment("packages_uninstalled");
        } else if (restoreResult) {
            MAKINEAI_LOG_WARN(log::PACKAGE, "Restore completed with issues: {}", restoreResult->message);
        } else {
            MAKINEAI_LOG_ERROR(log::PACKAGE, "Backup restore failed: {}", restoreResult.error().message());
            AuditLogger::logPatchOperation(game.id.storeId, false, "uninstall_failed",
                "Restore failed: " + restoreResult.error().message());
        }
        return restoreResult;
    }

    // Remove from installed packages even if no backup
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "No backup found, removing from installed list");
    removeInstalledInfo(game.id.storeId);

    RestoreResult result;
    result.success = true;
    result.message = "Translation uninstalled";

    // Audit log: Uninstall complete (no backup)
    AuditLogger::logPatchOperation(game.id.storeId, true, "uninstall_complete", "No backup to restore");
    MAKINEAI_LOG_INFO(log::PACKAGE, "Uninstall complete for: {}", game.id.storeId);
    metrics().increment("packages_uninstalled");

    return result;
}

Result<InstalledPackage> PackageManager::getInstalled(const std::string& gameId) const {
    for (const auto& pkg : installedPackages_) {
        if (pkg.gameId == gameId) {
            return pkg;
        }
    }
    return std::unexpected(Error(ErrorCode::NotPatched,
        "No translation installed for: " + gameId));
}

bool PackageManager::isInstalled(const std::string& gameId) const {
    return getInstalled(gameId).has_value();
}

std::vector<InstalledPackage> PackageManager::listInstalled() const {
    return installedPackages_;
}

Result<std::vector<TranslationPackage>> PackageManager::checkUpdates() const {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Checking for package updates");
    MAKINEAI_TIMED_SCOPE(log::PACKAGE, "check_updates");

    std::vector<TranslationPackage> updates;

    for (const auto& installed : listInstalled()) {
        auto pkgResult = getPackage(installed.packageId);
        if (pkgResult) {
            auto installedVer = Version::parse(installed.version);
            auto latestVer = Version::parse(pkgResult->version);

            if (installedVer && latestVer && *latestVer > *installedVer) {
                MAKINEAI_LOG_INFO(log::PACKAGE, "Update available for {}: {} -> {}",
                    installed.packageId, installed.version, pkgResult->version);
                updates.push_back(*pkgResult);
            }
        }
    }

    MAKINEAI_LOG_INFO(log::PACKAGE, "Found {} package updates available", updates.size());
    metrics().gauge("updates_available", static_cast<double>(updates.size()));
    return updates;
}

VoidResult PackageManager::clearCache() {
    MAKINEAI_LOG_INFO(log::PACKAGE, "Clearing package cache: {}", cacheDir_.string());

    std::error_code ec;
    auto sizeBefore = fs::exists(cacheDir_, ec) ?
        std::distance(fs::recursive_directory_iterator(cacheDir_, ec),
                     fs::recursive_directory_iterator()) : 0;

    fs::remove_all(cacheDir_, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::FILE, "Failed to clear cache: {}", ec.message());
        AuditLogger::logFileAccess(cacheDir_, "delete", false, ec.message());
        return std::unexpected(Error(ErrorCode::IOError, "Failed to clear cache: " + ec.message()));
    }

    fs::create_directories(cacheDir_, ec);

    MAKINEAI_LOG_INFO(log::PACKAGE, "Cache cleared ({} items removed)", sizeBefore);
    AuditLogger::logFileAccess(cacheDir_, "delete", true, std::to_string(sizeBefore) + " items cleared");
    metrics().increment("cache_clears");

    return {};
}

VoidResult PackageManager::saveInstalledInfo(const InstalledPackage& info) {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Saving installed package info: {} for game {}",
        info.packageId, info.gameId);

    // Validate input
    auto pkgIdResult = validation::validatePackageId(info.packageId);
    if (!pkgIdResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Invalid package ID: {}", info.packageId);
        return std::unexpected(pkgIdResult.error());
    }

    auto gameIdResult = validation::validateGameId(info.gameId);
    if (!gameIdResult) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Invalid game ID: {}", info.gameId);
        return std::unexpected(gameIdResult.error());
    }

    // Update in-memory cache (replace if exists, add if new)
    bool found = false;
    for (auto& pkg : installedPackages_) {
        if (pkg.gameId == info.gameId) {
            pkg = info;
            found = true;
            MAKINEAI_LOG_DEBUG(log::PACKAGE, "Updated existing installed package entry");
            break;
        }
    }
    if (!found) {
        installedPackages_.push_back(info);
        MAKINEAI_LOG_DEBUG(log::PACKAGE, "Added new installed package entry");
    }

    // Persist to JSON file
    if (installedDbPath_.empty()) {
        // Use default path if not set
        installedDbPath_ = cacheDir_ / "installed_packages.json";
    }

    // Validate path
    if (!validation::isPathSafe(installedDbPath_)) {
        MAKINEAI_LOG_ERROR(log::FILE, "Unsafe database path: {}", installedDbPath_.string());
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Unsafe database path"));
    }

    try {
        std::error_code ec;
        fs::create_directories(installedDbPath_.parent_path(), ec);
        if (ec) {
            MAKINEAI_LOG_WARN(log::FILE, "Failed to create parent directories: {}", ec.message());
        }

        json packagesJson = json::array();
        for (const auto& pkg : installedPackages_) {
            json pkgJson;
            pkgJson["packageId"] = pkg.packageId;
            pkgJson["gameId"] = pkg.gameId;
            pkgJson["version"] = pkg.version;
            pkgJson["installPath"] = pkg.installPath.string();
            pkgJson["installedAt"] = pkg.installedAt;
            pkgJson["gameHashAtInstall"] = pkg.gameHashAtInstall;
            packagesJson.push_back(pkgJson);
        }

        // Atomic write: write to temp file, then rename
        fs::path tempPath = installedDbPath_.string() + ".tmp";

        {
            std::ofstream file(tempPath, std::ios::trunc);
            if (!file) {
                MAKINEAI_LOG_ERROR(log::FILE, "Cannot write installed packages file: {}",
                    installedDbPath_.string());
                AuditLogger::logFileAccess(installedDbPath_, "write", false, "Permission denied");
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Cannot write installed packages file: " + installedDbPath_.string()));
            }
            file << packagesJson.dump(2);
            file.flush();

            if (!file.good()) {
                file.close();
                fs::remove(tempPath, ec);
                MAKINEAI_LOG_ERROR(log::FILE, "Write failed, possible disk full");
                AuditLogger::logFileAccess(installedDbPath_, "write", false, "Write failed - disk full?");
                return std::unexpected(Error(ErrorCode::IOError,
                    "Installed packages write failed - possible disk full"));
            }
        } // File closed here

        // Atomic rename
        fs::rename(tempPath, installedDbPath_, ec);
        if (ec) {
            fs::remove(tempPath, ec);
            MAKINEAI_LOG_ERROR(log::FILE, "Atomic rename failed: {}", ec.message());
            AuditLogger::logFileAccess(installedDbPath_, "write", false, "Rename failed: " + ec.message());
            return std::unexpected(Error(ErrorCode::IOError,
                "Installed packages rename failed: " + ec.message()));
        }

        MAKINEAI_LOG_DEBUG(log::PACKAGE, "Saved {} installed packages to {}",
            installedPackages_.size(), installedDbPath_.string());
        AuditLogger::logFileAccess(installedDbPath_, "write", true,
            std::to_string(installedPackages_.size()) + " packages");
        metrics().gauge("installed_packages_count", static_cast<double>(installedPackages_.size()));
        return {};

    } catch (const std::exception& e) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Exception saving installed packages: {}", e.what());
        AuditLogger::logFileAccess(installedDbPath_, "write", false, e.what());
        return std::unexpected(Error(ErrorCode::StorageError,
            std::string("Failed to save installed packages: ") + e.what()));
    }
}

void PackageManager::loadInstalledInfo() {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Loading installed packages database");
    MAKINEAI_TIMED_SCOPE(log::PACKAGE, "load_installed_info");

    installedPackages_.clear();

    if (installedDbPath_.empty()) {
        // Use default path if not set
        installedDbPath_ = cacheDir_ / "installed_packages.json";
    }

    // Validate path
    if (!validation::isPathSafe(installedDbPath_)) {
        MAKINEAI_LOG_ERROR(log::FILE, "Unsafe database path: {}", installedDbPath_.string());
        return;
    }

    if (!fs::exists(installedDbPath_)) {
        MAKINEAI_LOG_DEBUG(log::PACKAGE, "No installed packages file found at {}",
            installedDbPath_.string());
        return;
    }

    try {
        std::ifstream file(installedDbPath_);
        if (!file) {
            MAKINEAI_LOG_WARN(log::FILE, "Cannot read installed packages file: {}",
                installedDbPath_.string());
            AuditLogger::logFileAccess(installedDbPath_, "read", false, "Cannot open file");
            return;
        }

        json packagesJson = json::parse(file);

        for (const auto& pkgJson : packagesJson) {
            InstalledPackage pkg;
            pkg.packageId = pkgJson.value("packageId", "");
            pkg.gameId = pkgJson.value("gameId", "");
            pkg.version = pkgJson.value("version", "");
            pkg.installPath = pkgJson.value("installPath", "");
            pkg.installedAt = pkgJson.value("installedAt", 0ULL);
            pkg.gameHashAtInstall = pkgJson.value("gameHashAtInstall", "");

            // Validate loaded data
            auto pkgIdResult = validation::validatePackageId(pkg.packageId);
            auto gameIdResult = validation::validateGameId(pkg.gameId);

            if (!pkgIdResult || !gameIdResult) {
                MAKINEAI_LOG_WARN(log::PACKAGE, "Skipping invalid package entry: {} / {}",
                    pkg.packageId, pkg.gameId);
                continue;
            }

            installedPackages_.push_back(std::move(pkg));
        }

        MAKINEAI_LOG_INFO(log::PACKAGE, "Loaded {} installed packages from {}",
            installedPackages_.size(), installedDbPath_.string());
        AuditLogger::logFileAccess(installedDbPath_, "read", true,
            "Loaded " + std::to_string(installedPackages_.size()) + " packages");
        metrics().gauge("installed_packages_count", static_cast<double>(installedPackages_.size()));

    } catch (const json::exception& e) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "JSON parse error loading installed packages: {}", e.what());
        AuditLogger::logFileAccess(installedDbPath_, "read", false, "JSON parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to load installed packages: {}", e.what());
        AuditLogger::logFileAccess(installedDbPath_, "read", false, e.what());
    }
}

void PackageManager::removeInstalledInfo(const std::string& gameId) {
    MAKINEAI_LOG_DEBUG(log::PACKAGE, "Removing installed info for game: {}", gameId);

    auto it = std::remove_if(installedPackages_.begin(), installedPackages_.end(),
        [&gameId](const InstalledPackage& pkg) {
            return pkg.gameId == gameId;
        });

    if (it != installedPackages_.end()) {
        size_t removedCount = std::distance(it, installedPackages_.end());
        installedPackages_.erase(it, installedPackages_.end());
        MAKINEAI_LOG_DEBUG(log::PACKAGE, "Removed {} package entries for game {}", removedCount, gameId);

        // Re-save the file without the removed package
        if (!installedDbPath_.empty() && fs::exists(installedDbPath_)) {
            try {
                json packagesJson = json::array();
                for (const auto& pkg : installedPackages_) {
                    json pkgJson;
                    pkgJson["packageId"] = pkg.packageId;
                    pkgJson["gameId"] = pkg.gameId;
                    pkgJson["version"] = pkg.version;
                    pkgJson["installPath"] = pkg.installPath.string();
                    pkgJson["installedAt"] = pkg.installedAt;
                    pkgJson["gameHashAtInstall"] = pkg.gameHashAtInstall;
                    packagesJson.push_back(pkgJson);
                }

                // Atomic write: write to temp file, then rename
                fs::path tempPath = installedDbPath_.string() + ".tmp";

                std::ofstream file(tempPath, std::ios::trunc);
                if (file) {
                    file << packagesJson.dump(2);
                    file.flush();

                    if (file.good()) {
                        file.close();
                        std::error_code ec;
                        fs::rename(tempPath, installedDbPath_, ec);
                        if (!ec) {
                            MAKINEAI_LOG_DEBUG(log::PACKAGE, "Removed package for game {} from installed list", gameId);
                            AuditLogger::logFileAccess(installedDbPath_, "write", true,
                                "Removed " + gameId + ", " + std::to_string(installedPackages_.size()) + " remaining");
                            metrics().gauge("installed_packages_count", static_cast<double>(installedPackages_.size()));
                        } else {
                            fs::remove(tempPath, ec);
                            MAKINEAI_LOG_ERROR(log::FILE, "Failed to rename installed packages file: {}", ec.message());
                            AuditLogger::logFileAccess(installedDbPath_, "write", false, "Rename failed");
                        }
                    } else {
                        file.close();
                        std::error_code ec;
                        fs::remove(tempPath, ec);
                        MAKINEAI_LOG_ERROR(log::FILE, "Failed to flush installed packages file");
                        AuditLogger::logFileAccess(installedDbPath_, "write", false, "Flush failed");
                    }
                } else {
                    MAKINEAI_LOG_ERROR(log::FILE, "Cannot open installed packages file for writing");
                    AuditLogger::logFileAccess(installedDbPath_, "write", false, "Cannot open file");
                }
            } catch (const std::exception& e) {
                MAKINEAI_LOG_ERROR(log::PACKAGE, "Failed to update installed packages file: {}", e.what());
                AuditLogger::logFileAccess(installedDbPath_, "write", false, e.what());
            }
        }
    } else {
        MAKINEAI_LOG_DEBUG(log::PACKAGE, "No installed package found for game: {}", gameId);
    }
}

Result<ByteBuffer> PackageManager::httpGet(
    const std::string& url,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    MAKINEAI_LOG_DEBUG(log::NETWORK, "HTTP GET: {}", url);
    Downloader downloader;
    return downloader.download(url, progress, cancel);
}

} // namespace makineai
