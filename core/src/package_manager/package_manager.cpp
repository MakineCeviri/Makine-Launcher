/**
 * @file package_manager.cpp
 * @brief Package manager implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/package_manager.hpp"
#include "makineai/core.hpp"
#include "makineai/patch_engine.hpp"
#include "makineai/runtime_manager.hpp"

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <chrono>

namespace makineai {

using json = nlohmann::json;

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
    CURL* curl = curl_easy_init();
    if (!curl) {
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

    if (progress || cancel) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Impl::progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            return std::unexpected(Error(ErrorCode::Cancelled,
                "Download cancelled"));
        }

        return std::unexpected(Error(ErrorCode::DownloadFailed,
            std::string("Download failed: ") + curl_easy_strerror(res)));
    }

    long httpCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        return std::unexpected(Error(ErrorCode::ServerError,
            "HTTP error: " + std::to_string(httpCode)));
    }

    return buffer;
}

VoidResult Downloader::downloadToFile(
    const std::string& url,
    const fs::path& destination,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::unexpected(Error(ErrorCode::NetworkError,
            "Failed to initialize CURL"));
    }

    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);

    std::ofstream file(destination, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
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
            return std::unexpected(Error(ErrorCode::Cancelled,
                "Download cancelled"));
        }

        return std::unexpected(Error(ErrorCode::DownloadFailed,
            std::string("Download failed: ") + curl_easy_strerror(res)));
    }

    long httpCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode >= 400) {
        fs::remove(destination, ec);
        return std::unexpected(Error(ErrorCode::ServerError,
            "HTTP error: " + std::to_string(httpCode)));
    }

    return {};
}

// PackageManager implementation
PackageManager::PackageManager() {
    loadInstalledInfo();
}

PackageManager::~PackageManager() = default;

Result<PackageManifest> PackageManager::fetchManifest(CancellationToken* cancel) {
    Downloader downloader;
    std::string manifestUrl = apiUrl_ + "/manifest";

    auto result = downloader.download(manifestUrl, nullptr, cancel);
    if (!result) {
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

        logger()->info("Fetched manifest with {} packages", manifest_.packages.size());
        return manifest_;

    } catch (const json::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError,
            std::string("Invalid manifest JSON: ") + e.what()));
    }
}

Result<TranslationPackage> PackageManager::findPackage(const GameInfo& game) const {
    for (const auto& pkg : manifest_.packages) {
        // Check by store ID
        if (pkg.gameId == game.id.storeId) {
            return pkg;
        }

        // Check by hash
        for (const auto& hash : pkg.supportedGameHashes) {
            if (hash == game.id.exeHash) {
                return pkg;
            }
        }
    }

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
    Downloader downloader;

    std::string url = package.downloadUrl;
    if (!url.starts_with("http")) {
        url = apiUrl_ + url;
    }

    fs::path cachePath = cacheDir_ / (package.packageId + ".zip");

    // Check if already cached
    if (fs::exists(cachePath)) {
        // Verify checksum
        auto& security = Core::instance().securityManager();
        auto hashResult = security.hashFile(cachePath);
        if (hashResult && *hashResult == package.sha256) {
            logger()->info("Using cached package: {}", package.packageId);
            return cachePath;
        }
        // Invalid cache, remove
        std::error_code ec;
        fs::remove(cachePath, ec);
    }

    // Download
    auto downloadProgress = [&progress, &package](const DownloadProgress& p) {
        if (progress) {
            DownloadProgress dp = p;
            dp.currentFile = package.packageId + ".zip";
            progress(dp);
        }
    };

    auto result = downloader.downloadToFile(url, cachePath, downloadProgress, cancel);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Verify
    auto verifyResult = verifyPackage(cachePath, package);
    if (!verifyResult) {
        std::error_code ec;
        fs::remove(cachePath, ec);
        return std::unexpected(verifyResult.error());
    }

    return cachePath;
}

VoidResult PackageManager::verifyPackage(
    const fs::path& packagePath,
    const TranslationPackage& package
) {
    auto& security = Core::instance().securityManager();

    // Verify checksum
    auto hashResult = security.hashFile(packagePath);
    if (!hashResult) {
        return std::unexpected(hashResult.error());
    }

    if (*hashResult != package.sha256) {
        return std::unexpected(Error(ErrorCode::ChecksumMismatch,
            "Package checksum mismatch"));
    }

    // Verify signature if public key is loaded
    if (security.hasPublicKey() && !package.signature.empty()) {
        // Read package for signature verification
        std::ifstream file(packagePath, std::ios::binary);
        ByteBuffer data((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

        auto sigResult = security.verifySignature(data, package.signature);
        if (!sigResult || !sigResult->valid) {
            return std::unexpected(Error(ErrorCode::SignatureInvalid,
                "Package signature invalid"));
        }
    }

    return {};
}

Result<PatchResult> PackageManager::install(
    const TranslationPackage& package,
    const GameInfo& game,
    ProgressCallback progress,
    CancellationToken* cancel
) {
    // Download package
    if (progress) progress(0, 5, "Downloading translation package...");

    auto downloadResult = download(package, nullptr, cancel);
    if (!downloadResult) {
        return std::unexpected(downloadResult.error());
    }

    // Check if runtime is needed
    if (package.requiresRuntime) {
        if (progress) progress(1, 5, "Installing translation runtime...");

        auto& runtime = Core::instance().runtimeManager();
        auto runtimeResult = runtime.install(game);
        if (!runtimeResult) {
            return std::unexpected(runtimeResult.error());
        }

        // Extract translations to runtime folder
        if (progress) progress(2, 5, "Extracting translations...");

        // TODO: Extract ZIP and copy to BepInEx/Translation folder
        // For now, return success placeholder

        PatchResult result;
        result.success = true;
        result.message = "Runtime translation installed";
        result.filesPatched = 0;  // Runtime doesn't patch files directly
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
            logger()->warn("Failed to save install info: {}", saveResult.error().message());
        }

        if (progress) progress(5, 5, "Installation complete");
        return result;
    }

    // For non-runtime packages, use patch engine
    if (progress) progress(1, 5, "Creating backup...");

    auto& patcher = Core::instance().patchEngine();

    // TODO: Extract ZIP and create patch operations
    std::vector<PatchOperation> operations;

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
            logger()->warn("Failed to save install info: {}", saveResult.error().message());
        }
    }

    return patchResult;
}

Result<RestoreResult> PackageManager::uninstall(const GameInfo& game) {
    auto& patcher = Core::instance().patchEngine();
    auto& runtime = Core::instance().runtimeManager();

    // Check if runtime was installed
    auto statusResult = runtime.checkStatus(game);
    if (statusResult && statusResult->installed) {
        // Uninstall runtime
        auto uninstallResult = runtime.uninstall(game);
        if (!uninstallResult) {
            return std::unexpected(uninstallResult.error());
        }
    }

    // Restore backup if exists
    std::string backupId = game.id.storeId;
    if (patcher.hasBackup(backupId)) {
        return patcher.restore(game.installPath, backupId);
    }

    RestoreResult result;
    result.success = true;
    result.message = "Translation uninstalled";
    return result;
}

Result<InstalledPackage> PackageManager::getInstalled(const std::string& gameId) const {
    // TODO: Read from database
    return std::unexpected(Error(ErrorCode::NotPatched,
        "No translation installed for: " + gameId));
}

bool PackageManager::isInstalled(const std::string& gameId) const {
    return getInstalled(gameId).has_value();
}

std::vector<InstalledPackage> PackageManager::listInstalled() const {
    // TODO: Read from database
    return {};
}

Result<std::vector<TranslationPackage>> PackageManager::checkUpdates() const {
    std::vector<TranslationPackage> updates;

    for (const auto& installed : listInstalled()) {
        auto pkgResult = getPackage(installed.packageId);
        if (pkgResult) {
            auto installedVer = Version::parse(installed.version);
            auto latestVer = Version::parse(pkgResult->version);

            if (installedVer && latestVer && *latestVer > *installedVer) {
                updates.push_back(*pkgResult);
            }
        }
    }

    return updates;
}

VoidResult PackageManager::clearCache() {
    std::error_code ec;
    fs::remove_all(cacheDir_, ec);
    fs::create_directories(cacheDir_, ec);
    return {};
}

VoidResult PackageManager::saveInstalledInfo(const InstalledPackage& info) {
    // TODO: Save to SQLite database
    (void)info;
    return {};
}

void PackageManager::loadInstalledInfo() {
    // TODO: Load from SQLite database
}

Result<ByteBuffer> PackageManager::httpGet(
    const std::string& url,
    DownloadProgressCallback progress,
    CancellationToken* cancel
) {
    Downloader downloader;
    return downloader.download(url, progress, cancel);
}

} // namespace makineai
