/**
 * @file package_manager.hpp
 * @brief Translation package downloading and management
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <functional>
#include <memory>

namespace makineai {

/**
 * @brief Download progress information
 */
struct DownloadProgress {
    uint64_t bytesDownloaded;
    uint64_t totalBytes;
    double speedBytesPerSec;
    std::string currentFile;
};

using DownloadProgressCallback = std::function<void(const DownloadProgress&)>;

/**
 * @brief Package manifest from server
 */
struct PackageManifest {
    std::string version;
    uint64_t updatedAt;
    std::vector<TranslationPackage> packages;
};

/**
 * @brief Installed package information
 */
struct InstalledPackage {
    std::string packageId;
    std::string gameId;
    std::string version;
    fs::path installPath;
    uint64_t installedAt;
    std::string gameHashAtInstall;
};

/**
 * @brief Package manager for downloading and installing translations
 */
class PackageManager {
public:
    PackageManager();
    ~PackageManager();

    /**
     * @brief Fetch latest package manifest from server
     */
    [[nodiscard]] Result<PackageManifest> fetchManifest(
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Get cached manifest (doesn't fetch from server)
     */
    [[nodiscard]] const PackageManifest& cachedManifest() const { return manifest_; }

    /**
     * @brief Find package for a specific game
     */
    [[nodiscard]] Result<TranslationPackage> findPackage(
        const GameInfo& game
    ) const;

    /**
     * @brief Find package by ID
     */
    [[nodiscard]] Result<TranslationPackage> getPackage(
        const std::string& packageId
    ) const;

    /**
     * @brief Check if translation is available for game
     */
    [[nodiscard]] bool hasTranslation(const GameInfo& game) const;

    /**
     * @brief Download a translation package
     */
    [[nodiscard]] Result<fs::path> download(
        const TranslationPackage& package,
        DownloadProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Verify downloaded package integrity
     */
    [[nodiscard]] VoidResult verifyPackage(
        const fs::path& packagePath,
        const TranslationPackage& package
    );

    /**
     * @brief Install a translation package to a game
     */
    [[nodiscard]] Result<PatchResult> install(
        const TranslationPackage& package,
        const GameInfo& game,
        ProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Uninstall translation from game (restore backup)
     */
    [[nodiscard]] Result<RestoreResult> uninstall(
        const GameInfo& game
    );

    /**
     * @brief Get installed package info for a game
     */
    [[nodiscard]] Result<InstalledPackage> getInstalled(
        const std::string& gameId
    ) const;

    /**
     * @brief Check if game has translation installed
     */
    [[nodiscard]] bool isInstalled(const std::string& gameId) const;

    /**
     * @brief List all installed packages
     */
    [[nodiscard]] std::vector<InstalledPackage> listInstalled() const;

    /**
     * @brief Check for updates to installed translations
     */
    [[nodiscard]] Result<std::vector<TranslationPackage>> checkUpdates() const;

    /**
     * @brief Set API base URL
     */
    void setApiUrl(const std::string& url) { apiUrl_ = url; }

    /**
     * @brief Set cache directory
     */
    void setCacheDirectory(const fs::path& dir) { cacheDir_ = dir; }

    /**
     * @brief Clear downloaded package cache
     */
    VoidResult clearCache();

private:
    std::string apiUrl_;
    fs::path cacheDir_;
    fs::path installedDbPath_;
    PackageManifest manifest_;
    std::vector<InstalledPackage> installedPackages_;  // In-memory cache

    [[nodiscard]] Result<ByteBuffer> httpGet(
        const std::string& url,
        DownloadProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    [[nodiscard]] VoidResult saveInstalledInfo(const InstalledPackage& info);
    void loadInstalledInfo();
    void removeInstalledInfo(const std::string& gameId);
};

/**
 * @brief HTTP downloader helper
 */
class Downloader {
public:
    Downloader();
    ~Downloader();

    /**
     * @brief Download file from URL
     */
    [[nodiscard]] Result<ByteBuffer> download(
        const std::string& url,
        DownloadProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Download file to disk
     */
    [[nodiscard]] VoidResult downloadToFile(
        const std::string& url,
        const fs::path& destination,
        DownloadProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Set timeout in seconds
     */
    void setTimeout(uint32_t seconds) { timeout_ = seconds; }

    /**
     * @brief Set user agent string
     */
    void setUserAgent(const std::string& ua) { userAgent_ = ua; }

private:
    uint32_t timeout_ = 30;
    std::string userAgent_ = "MakineAI/1.0";
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace makineai
