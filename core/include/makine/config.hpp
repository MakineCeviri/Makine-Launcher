/**
 * @file config.hpp
 * @brief Makine configuration system
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides centralized configuration management for all Makine components.
 * Supports loading from files, environment variables, and runtime modification.
 */

#pragma once

#include "constants.hpp"

#include <filesystem>
#include <string>
#include <optional>
#include <functional>
#include <mutex>
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>

namespace makine {

namespace fs = std::filesystem;

// =============================================================================
// CONFIGURATION STRUCTURES
// =============================================================================

/**
 * @brief Configuration for game scanning operations
 */
struct ScanningConfig {
    /// Maximum number of parallel scanner threads
    uint32_t maxParallelScans = 4;

    /// Timeout for each scanner in milliseconds
    uint32_t scanTimeoutMs = kDefaultTimeoutMs;

    /// Whether to scan Steam library
    bool scanSteam = true;

    /// Whether to scan Epic Games library
    bool scanEpic = true;

    /// Whether to scan GOG library
    bool scanGOG = true;

    /// Maximum files to scan per game for engine detection
    uint32_t maxFilesToScan = 10000;

    /// Minimum confidence threshold for engine detection (0-100)
    uint32_t minEngineConfidence = 30;

    /// Cache scan results for this many seconds (0 = no cache)
    uint32_t cacheValiditySeconds = 300;

    bool operator==(const ScanningConfig&) const = default;
};

/**
 * @brief Configuration for patching operations
 */
struct PatchingConfig {
    /// Always create backup before patching
    bool alwaysCreateBackup = true;

    /// Minimum required disk space in MB before patching
    uint64_t minDiskSpaceMB = 500;

    /// Maximum retry attempts for failed operations
    uint32_t maxRetries = 3;

    /// Use atomic writes (write to temp, then rename)
    bool atomicWrites = true;

    /// Verify file integrity after patching
    bool verifyAfterPatch = true;

    /// Maximum backup age in days (0 = keep forever)
    uint32_t maxBackupAgeDays = 30;

    /// Maximum total backup size in MB (0 = unlimited)
    uint64_t maxBackupSizeMB = 0;

    /// Compress backups to save space
    bool compressBackups = true;

    bool operator==(const PatchingConfig&) const = default;
};

/**
 * @brief Configuration for translation operations
 *
 * Deferred fields removed: minQAScore, allowFuzzyMatches, fuzzyMatchThreshold,
 * preferSafeMethods, allowHybridMethods, autoFallback, maxEntriesInMemory,
 * sourceLanguage, targetLanguage. See git history if restoration is needed.
 */
struct TranslationConfig {
    bool operator==(const TranslationConfig&) const = default;
};

/**
 * @brief Configuration for security operations
 */
struct SecurityConfig {
    /// Verify package signatures before installation
    bool verifySignatures = true;

    /// Verify file checksums
    bool verifyChecksums = true;

    /// Path to trusted public keys directory
    std::string trustedKeysPath;

    /// Allow packages from untrusted sources (with warning)
    bool allowUntrustedPackages = false;

    /// Enable audit logging for security events
    bool enableAuditLog = true;

    /// Block path traversal attempts
    bool blockPathTraversal = true;

    bool operator==(const SecurityConfig&) const = default;
};

/**
 * @brief Configuration for network operations
 */
struct NetworkConfig {
    /// Connection timeout in milliseconds
    uint32_t connectionTimeoutMs = kShortTimeoutMs;

    /// Read timeout in milliseconds
    uint32_t readTimeoutMs = kDefaultTimeoutMs;

    /// Maximum download retries
    uint32_t maxDownloadRetries = 3;

    /// User agent string
    std::string userAgent = "Makine-Launcher/0.1.0";

    /// Proxy URL (empty = no proxy)
    std::string proxyUrl;

    /// Enable SSL certificate verification
    bool verifySsl = true;

    /// Maximum concurrent downloads
    uint32_t maxConcurrentDownloads = 4;

    bool operator==(const NetworkConfig&) const = default;
};

/**
 * @brief Configuration for logging
 */
struct LoggingConfig {
    /// Log level: trace, debug, info, warn, error, critical
    std::string level = "info";

    /// Log file path (empty = console only)
    std::string logFilePath;

    /// Maximum log file size in MB before rotation
    uint32_t maxLogSizeMB = 10;

    /// Number of rotated log files to keep
    uint32_t maxLogFiles = 5;

    /// Include timestamps in console output
    bool consoleTimestamps = true;

    /// Enable colored console output
    bool coloredOutput = true;

    bool operator==(const LoggingConfig&) const = default;
};

/**
 * @brief Configuration for database operations
 */
struct DatabaseConfig {
    /// Database file path
    std::string databasePath = "makine.db";

    /// Enable WAL mode for better concurrency
    bool enableWAL = true;

    /// Connection pool size
    uint32_t connectionPoolSize = 4;

    /// Query timeout in milliseconds
    uint32_t queryTimeoutMs = 5000;

    /// Vacuum database on startup
    bool vacuumOnStartup = false;

    bool operator==(const DatabaseConfig&) const = default;
};

/**
 * @brief Root configuration structure containing all settings
 */
struct CoreConfig {
    ScanningConfig scanning;
    PatchingConfig patching;
    TranslationConfig translation;
    SecurityConfig security;
    NetworkConfig network;
    LoggingConfig logging;
    DatabaseConfig database;

    /// Application data directory
    std::string dataDirectory;

    /// Temporary files directory
    std::string tempDirectory;

    /// Backup storage directory
    std::string backupDirectory;

    /// Cache directory
    std::string cacheDirectory;

    /// Logs directory
    std::string logsDirectory;

    /// API base URL for translation services
    std::string apiBaseUrl = "https://api.makineceviri.org/v1";

    /// Path to public key for package verification
    std::string publicKeyPath;

    /// Log level
    spdlog::level::level_enum logLevel = spdlog::level::info;

    /// Auto-update runtime components
    bool autoUpdateRuntime = true;

    /// Enable analytics collection
    bool enableAnalytics = false;

    /**
     * @brief Load configuration from a JSON file
     * @param path Path to configuration file
     * @return Loaded configuration or default if file doesn't exist
     */
    [[nodiscard]] static CoreConfig loadFromFile(const fs::path& path);

    /**
     * @brief Save configuration to a JSON file
     * @param path Path to configuration file
     */
    void saveToFile(const fs::path& path) const;

    /**
     * @brief Get default configuration
     */
    [[nodiscard]] static CoreConfig getDefaults();

    /**
     * @brief Apply environment variable overrides
     *
     * Supported environment variables:
     * - MAKINE_LOG_LEVEL
     * - MAKINE_DATA_DIR
     * - MAKINE_SCAN_TIMEOUT
     * - MAKINE_MIN_DISK_SPACE
     * - MAKINE_PROXY_URL
     * - MAKINE_VERIFY_SSL
     */
    void applyEnvironmentOverrides();

    bool operator==(const CoreConfig&) const = default;
};

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

/**
 * @brief Result of configuration validation
 */
struct ConfigValidationResult {
    /// Whether the configuration is valid
    bool valid = true;

    /// List of validation errors (configuration cannot be used)
    std::vector<std::string> errors;

    /// List of validation warnings (configuration can be used but may cause issues)
    std::vector<std::string> warnings;

    /// Check if validation passed without any issues
    [[nodiscard]] bool isClean() const {
        return valid && errors.empty() && warnings.empty();
    }
};

/**
 * @brief Validate a configuration
 * @param config Configuration to validate
 * @return Validation result with errors and warnings
 */
[[nodiscard]] ConfigValidationResult validateConfig(const CoreConfig& config);

// =============================================================================
// CONFIGURATION MANAGER
// =============================================================================

/**
 * @brief Centralized configuration manager
 *
 * Provides thread-safe access to configuration and supports
 * runtime configuration changes with observer notifications.
 */
class ConfigManager {
public:
    /// Callback type for configuration change notifications
    using ConfigChangedCallback = std::function<void(const CoreConfig& oldConfig,
                                                     const CoreConfig& newConfig)>;

    /**
     * @brief Get singleton instance
     */
    static ConfigManager& instance();

    /**
     * @brief Initialize with configuration file
     * @param configPath Path to configuration file
     * @return true if initialization successful
     */
    [[nodiscard]] bool initialize(const fs::path& configPath);

    /**
     * @brief Get current configuration (thread-safe)
     */
    const CoreConfig& config() const;

    /**
     * @brief Update configuration (thread-safe)
     * @param newConfig New configuration
     * @return Validation result
     */
    [[nodiscard]] ConfigValidationResult updateConfig(const CoreConfig& newConfig);

    /**
     * @brief Reload configuration from file
     * @return true if reload successful
     */
    [[nodiscard]] bool reloadFromFile();

    /**
     * @brief Save current configuration to file
     * @return true if save successful
     */
    [[nodiscard]] bool saveToFile();

    /**
     * @brief Register observer for configuration changes
     * @param callback Callback to invoke on changes
     * @return Observer ID for unregistration
     */
    [[nodiscard]] size_t addObserver(ConfigChangedCallback callback);

    /**
     * @brief Unregister observer
     * @param observerId ID returned from addObserver
     */
    void removeObserver(size_t observerId);

    /**
     * @brief Get configuration file path
     */
    const fs::path& configPath() const { return configPath_; }

    /**
     * @brief Check if configuration is initialized
     */
    bool isInitialized() const { return initialized_; }

    // Delete copy/move
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

private:
    ConfigManager() = default;

    void notifyObservers(const CoreConfig& oldConfig, const CoreConfig& newConfig);

    mutable std::mutex mutex_;
    CoreConfig config_;
    fs::path configPath_;
    bool initialized_ = false;

    std::mutex observerMutex_;
    std::vector<std::pair<size_t, ConfigChangedCallback>> observers_;
    size_t nextObserverId_ = 1;
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Get current configuration (shortcut)
 */
inline const CoreConfig& config() {
    return ConfigManager::instance().config();
}

/**
 * @brief Get scanning configuration (shortcut)
 */
inline const ScanningConfig& scanningConfig() {
    return config().scanning;
}

/**
 * @brief Get patching configuration (shortcut)
 */
inline const PatchingConfig& patchingConfig() {
    return config().patching;
}

/**
 * @brief Get translation configuration (shortcut)
 */
inline const TranslationConfig& translationConfig() {
    return config().translation;
}

/**
 * @brief Get security configuration (shortcut)
 */
inline const SecurityConfig& securityConfig() {
    return config().security;
}

/**
 * @brief Get network configuration (shortcut)
 */
inline const NetworkConfig& networkConfig() {
    return config().network;
}

/**
 * @brief Get logging configuration (shortcut)
 */
inline const LoggingConfig& loggingConfig() {
    return config().logging;
}

/**
 * @brief Get database configuration (shortcut)
 */
inline const DatabaseConfig& databaseConfig() {
    return config().database;
}

} // namespace makine
