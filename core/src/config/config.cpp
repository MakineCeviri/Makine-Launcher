/**
 * @file config.cpp
 * @brief Makine configuration system implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/config.hpp"
#include "makine/logging.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace makine {

using json = nlohmann::json;

// =============================================================================
// JSON SERIALIZATION HELPERS
// =============================================================================

// ScanningConfig
void to_json(json& j, const ScanningConfig& c) {
    j = json{
        {"maxParallelScans", c.maxParallelScans},
        {"scanTimeoutMs", c.scanTimeoutMs},
        {"scanSteam", c.scanSteam},
        {"scanEpic", c.scanEpic},
        {"scanGOG", c.scanGOG},
        {"maxFilesToScan", c.maxFilesToScan},
        {"minEngineConfidence", c.minEngineConfidence},
        {"cacheValiditySeconds", c.cacheValiditySeconds}
    };
}

void from_json(const json& j, ScanningConfig& c) {
    if (j.contains("maxParallelScans")) j.at("maxParallelScans").get_to(c.maxParallelScans);
    if (j.contains("scanTimeoutMs")) j.at("scanTimeoutMs").get_to(c.scanTimeoutMs);
    if (j.contains("scanSteam")) j.at("scanSteam").get_to(c.scanSteam);
    if (j.contains("scanEpic")) j.at("scanEpic").get_to(c.scanEpic);
    if (j.contains("scanGOG")) j.at("scanGOG").get_to(c.scanGOG);
    if (j.contains("maxFilesToScan")) j.at("maxFilesToScan").get_to(c.maxFilesToScan);
    if (j.contains("minEngineConfidence")) j.at("minEngineConfidence").get_to(c.minEngineConfidence);
    if (j.contains("cacheValiditySeconds")) j.at("cacheValiditySeconds").get_to(c.cacheValiditySeconds);
}

// PatchingConfig
void to_json(json& j, const PatchingConfig& c) {
    j = json{
        {"alwaysCreateBackup", c.alwaysCreateBackup},
        {"minDiskSpaceMB", c.minDiskSpaceMB},
        {"maxRetries", c.maxRetries},
        {"atomicWrites", c.atomicWrites},
        {"verifyAfterPatch", c.verifyAfterPatch},
        {"maxBackupAgeDays", c.maxBackupAgeDays},
        {"maxBackupSizeMB", c.maxBackupSizeMB},
        {"compressBackups", c.compressBackups}
    };
}

void from_json(const json& j, PatchingConfig& c) {
    if (j.contains("alwaysCreateBackup")) j.at("alwaysCreateBackup").get_to(c.alwaysCreateBackup);
    if (j.contains("minDiskSpaceMB")) j.at("minDiskSpaceMB").get_to(c.minDiskSpaceMB);
    if (j.contains("maxRetries")) j.at("maxRetries").get_to(c.maxRetries);
    if (j.contains("atomicWrites")) j.at("atomicWrites").get_to(c.atomicWrites);
    if (j.contains("verifyAfterPatch")) j.at("verifyAfterPatch").get_to(c.verifyAfterPatch);
    if (j.contains("maxBackupAgeDays")) j.at("maxBackupAgeDays").get_to(c.maxBackupAgeDays);
    if (j.contains("maxBackupSizeMB")) j.at("maxBackupSizeMB").get_to(c.maxBackupSizeMB);
    if (j.contains("compressBackups")) j.at("compressBackups").get_to(c.compressBackups);
}

// TranslationConfig (all deferred fields removed)
void to_json(json& j, const TranslationConfig&) {
    j = json::object();
}

void from_json(const json&, TranslationConfig&) {
    // No fields to deserialize
}

// SecurityConfig
void to_json(json& j, const SecurityConfig& c) {
    j = json{
        {"verifySignatures", c.verifySignatures},
        {"verifyChecksums", c.verifyChecksums},
        {"trustedKeysPath", c.trustedKeysPath},
        {"allowUntrustedPackages", c.allowUntrustedPackages},
        {"enableAuditLog", c.enableAuditLog},
        {"blockPathTraversal", c.blockPathTraversal}
    };
}

void from_json(const json& j, SecurityConfig& c) {
    if (j.contains("verifySignatures")) j.at("verifySignatures").get_to(c.verifySignatures);
    if (j.contains("verifyChecksums")) j.at("verifyChecksums").get_to(c.verifyChecksums);
    if (j.contains("trustedKeysPath")) j.at("trustedKeysPath").get_to(c.trustedKeysPath);
    if (j.contains("allowUntrustedPackages")) j.at("allowUntrustedPackages").get_to(c.allowUntrustedPackages);
    if (j.contains("enableAuditLog")) j.at("enableAuditLog").get_to(c.enableAuditLog);
    if (j.contains("blockPathTraversal")) j.at("blockPathTraversal").get_to(c.blockPathTraversal);
}

// NetworkConfig
void to_json(json& j, const NetworkConfig& c) {
    j = json{
        {"connectionTimeoutMs", c.connectionTimeoutMs},
        {"readTimeoutMs", c.readTimeoutMs},
        {"maxDownloadRetries", c.maxDownloadRetries},
        {"userAgent", c.userAgent},
        {"proxyUrl", c.proxyUrl},
        {"verifySsl", c.verifySsl},
        {"maxConcurrentDownloads", c.maxConcurrentDownloads}
    };
}

void from_json(const json& j, NetworkConfig& c) {
    if (j.contains("connectionTimeoutMs")) j.at("connectionTimeoutMs").get_to(c.connectionTimeoutMs);
    if (j.contains("readTimeoutMs")) j.at("readTimeoutMs").get_to(c.readTimeoutMs);
    if (j.contains("maxDownloadRetries")) j.at("maxDownloadRetries").get_to(c.maxDownloadRetries);
    if (j.contains("userAgent")) j.at("userAgent").get_to(c.userAgent);
    if (j.contains("proxyUrl")) j.at("proxyUrl").get_to(c.proxyUrl);
    if (j.contains("verifySsl")) j.at("verifySsl").get_to(c.verifySsl);
    if (j.contains("maxConcurrentDownloads")) j.at("maxConcurrentDownloads").get_to(c.maxConcurrentDownloads);
}

// LoggingConfig
void to_json(json& j, const LoggingConfig& c) {
    j = json{
        {"level", c.level},
        {"logFilePath", c.logFilePath},
        {"maxLogSizeMB", c.maxLogSizeMB},
        {"maxLogFiles", c.maxLogFiles},
        {"consoleTimestamps", c.consoleTimestamps},
        {"coloredOutput", c.coloredOutput}
    };
}

void from_json(const json& j, LoggingConfig& c) {
    if (j.contains("level")) j.at("level").get_to(c.level);
    if (j.contains("logFilePath")) j.at("logFilePath").get_to(c.logFilePath);
    if (j.contains("maxLogSizeMB")) j.at("maxLogSizeMB").get_to(c.maxLogSizeMB);
    if (j.contains("maxLogFiles")) j.at("maxLogFiles").get_to(c.maxLogFiles);
    if (j.contains("consoleTimestamps")) j.at("consoleTimestamps").get_to(c.consoleTimestamps);
    if (j.contains("coloredOutput")) j.at("coloredOutput").get_to(c.coloredOutput);
}

// DatabaseConfig
void to_json(json& j, const DatabaseConfig& c) {
    j = json{
        {"databasePath", c.databasePath},
        {"enableWAL", c.enableWAL},
        {"connectionPoolSize", c.connectionPoolSize},
        {"queryTimeoutMs", c.queryTimeoutMs},
        {"vacuumOnStartup", c.vacuumOnStartup}
    };
}

void from_json(const json& j, DatabaseConfig& c) {
    if (j.contains("databasePath")) j.at("databasePath").get_to(c.databasePath);
    if (j.contains("enableWAL")) j.at("enableWAL").get_to(c.enableWAL);
    if (j.contains("connectionPoolSize")) j.at("connectionPoolSize").get_to(c.connectionPoolSize);
    if (j.contains("queryTimeoutMs")) j.at("queryTimeoutMs").get_to(c.queryTimeoutMs);
    if (j.contains("vacuumOnStartup")) j.at("vacuumOnStartup").get_to(c.vacuumOnStartup);
}

// CoreConfig
void to_json(json& j, const CoreConfig& c) {
    j = json{
        {"scanning", c.scanning},
        {"patching", c.patching},
        {"translation", c.translation},
        {"security", c.security},
        {"network", c.network},
        {"logging", c.logging},
        {"database", c.database},
        {"dataDirectory", c.dataDirectory},
        {"tempDirectory", c.tempDirectory},
        {"backupDirectory", c.backupDirectory},
        {"cacheDirectory", c.cacheDirectory}
    };
}

void from_json(const json& j, CoreConfig& c) {
    if (j.contains("scanning")) j.at("scanning").get_to(c.scanning);
    if (j.contains("patching")) j.at("patching").get_to(c.patching);
    if (j.contains("translation")) j.at("translation").get_to(c.translation);
    if (j.contains("security")) j.at("security").get_to(c.security);
    if (j.contains("network")) j.at("network").get_to(c.network);
    if (j.contains("logging")) j.at("logging").get_to(c.logging);
    if (j.contains("database")) j.at("database").get_to(c.database);
    if (j.contains("dataDirectory")) j.at("dataDirectory").get_to(c.dataDirectory);
    if (j.contains("tempDirectory")) j.at("tempDirectory").get_to(c.tempDirectory);
    if (j.contains("backupDirectory")) j.at("backupDirectory").get_to(c.backupDirectory);
    if (j.contains("cacheDirectory")) j.at("cacheDirectory").get_to(c.cacheDirectory);
}

// Helper to get environment variable
std::optional<std::string> getEnvVar(const char* name) {
    const char* value = std::getenv(name);
    if (value && value[0] != '\0') {
        return std::string(value);
    }
    return std::nullopt;
}

// =============================================================================
// CORECONFIG IMPLEMENTATION
// =============================================================================

CoreConfig CoreConfig::loadFromFile(const fs::path& path) {
    CoreConfig config = getDefaults();

    if (!fs::exists(path)) {
        MAKINE_LOG_WARN(log::CONFIG, "Config file not found: {}, using defaults", path.string());
        return config;
    }

    try {
        std::ifstream file(path);
        if (!file) {
            MAKINE_LOG_ERROR(log::CONFIG, "Cannot open config file: {}", path.string());
            return config;
        }

        json j = json::parse(file);
        config = j.get<CoreConfig>();

        MAKINE_LOG_INFO(log::CONFIG, "Configuration loaded from: {}", path.string());
    }
    catch (const json::exception& e) {
        MAKINE_LOG_ERROR(log::CONFIG, "Failed to parse config file: {}", e.what());
    }
    catch (const std::exception& e) {
        MAKINE_LOG_ERROR(log::CONFIG, "Failed to load config file: {}", e.what());
    }

    config.applyEnvironmentOverrides();
    return config;
}

void CoreConfig::saveToFile(const fs::path& path) const {
    try {
        // Ensure parent directory exists
        if (path.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
        }

        json j = *this;
        std::ofstream file(path);
        if (!file) {
            MAKINE_LOG_ERROR(log::CONFIG, "Cannot create config file: {}", path.string());
            return;
        }

        file << j.dump(4);
        MAKINE_LOG_INFO(log::CONFIG, "Configuration saved to: {}", path.string());
    }
    catch (const std::exception& e) {
        MAKINE_LOG_ERROR(log::CONFIG, "Failed to save config file: {}", e.what());
    }
}

CoreConfig CoreConfig::getDefaults() {
    CoreConfig config;

    // Set default directories based on platform
#ifdef _WIN32
    const char* appData = std::getenv("LOCALAPPDATA");
    if (appData) {
        fs::path base = fs::path(appData) / "MakineCeviri" / "Makine-Launcher";
        config.dataDirectory = base.string();
        config.tempDirectory = (base / "temp").string();
        config.backupDirectory = (base / "backups").string();
        config.cacheDirectory = (base / "cache").string();
        config.logsDirectory = (base / "logs").string();
        config.database.databasePath = (base / "makine.db").string();
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path base = fs::path(home) / ".makine";
        config.dataDirectory = base.string();
        config.tempDirectory = (base / "temp").string();
        config.backupDirectory = (base / "backups").string();
        config.cacheDirectory = (base / "cache").string();
        config.logsDirectory = (base / "logs").string();
        config.database.databasePath = (base / "makine.db").string();
    }
#endif

    // API and runtime settings
    config.apiBaseUrl = "https://api.makineceviri.org/v1";
    config.publicKeyPath = "";
    config.logLevel = spdlog::level::info;
    config.autoUpdateRuntime = true;
    config.enableAnalytics = false;

    return config;
}

void CoreConfig::applyEnvironmentOverrides() {
    // MAKINE_LOG_LEVEL
    if (auto val = getEnvVar("MAKINE_LOG_LEVEL")) {
        logging.level = *val;
        MAKINE_LOG_DEBUG(log::CONFIG, "Log level overridden by env: {}", *val);
    }

    // MAKINE_DATA_DIR
    if (auto val = getEnvVar("MAKINE_DATA_DIR")) {
        dataDirectory = *val;
        MAKINE_LOG_DEBUG(log::CONFIG, "Data directory overridden by env: {}", *val);
    }

    // MAKINE_SCAN_TIMEOUT
    if (auto val = getEnvVar("MAKINE_SCAN_TIMEOUT")) {
        try {
            scanning.scanTimeoutMs = std::stoul(*val);
            MAKINE_LOG_DEBUG(log::CONFIG, "Scan timeout overridden by env: {}ms", *val);
        } catch (const std::exception&) {
            MAKINE_LOG_WARN(log::CONFIG, "Invalid MAKINE_SCAN_TIMEOUT value: {}", *val);
        }
    }

    // MAKINE_MIN_DISK_SPACE
    if (auto val = getEnvVar("MAKINE_MIN_DISK_SPACE")) {
        try {
            patching.minDiskSpaceMB = std::stoull(*val);
            MAKINE_LOG_DEBUG(log::CONFIG, "Min disk space overridden by env: {}MB", *val);
        } catch (const std::exception&) {
            MAKINE_LOG_WARN(log::CONFIG, "Invalid MAKINE_MIN_DISK_SPACE value: {}", *val);
        }
    }

    // MAKINE_PROXY_URL
    if (auto val = getEnvVar("MAKINE_PROXY_URL")) {
        network.proxyUrl = *val;
        MAKINE_LOG_DEBUG(log::CONFIG, "Proxy URL overridden by env");
    }

    // MAKINE_VERIFY_SSL — only allow disabling in debug builds
#ifndef NDEBUG
    if (auto val = getEnvVar("MAKINE_VERIFY_SSL")) {
        network.verifySsl = (*val == "1" || *val == "true" || *val == "yes");
        MAKINE_LOG_WARN(log::CONFIG, "SSL verification overridden by env: {} (debug build only)", network.verifySsl);
    }
#endif
}

// =============================================================================
// CONFIGURATION VALIDATION
// =============================================================================

namespace {
    constexpr uint32_t kMaxParallelScans     = 16;
    constexpr uint32_t kMinScanTimeoutMs     = 1000;
    constexpr uint32_t kMinDiskSpaceMB       = 100;
    constexpr uint32_t kMaxPatchRetries      = 10;
    constexpr uint32_t kMinConnectionTimeout = 100;
} // namespace

ConfigValidationResult validateConfig(const CoreConfig& config) {
    ConfigValidationResult result;

    // Scanning validation
    if (config.scanning.maxParallelScans == 0) {
        result.errors.push_back("scanning.maxParallelScans must be greater than 0");
        result.valid = false;
    }
    if (config.scanning.maxParallelScans > kMaxParallelScans) {
        result.warnings.push_back("scanning.maxParallelScans > 16 may cause resource issues");
    }
    if (config.scanning.scanTimeoutMs < kMinScanTimeoutMs) {
        result.warnings.push_back("scanning.scanTimeoutMs < 1000ms may cause premature timeouts");
    }

    // Patching validation
    if (config.patching.minDiskSpaceMB < kMinDiskSpaceMB) {
        result.warnings.push_back("patching.minDiskSpaceMB < 100MB may cause patch failures");
    }
    if (config.patching.maxRetries > kMaxPatchRetries) {
        result.warnings.push_back("patching.maxRetries > 10 may cause long delays on failure");
    }

    // Network validation
    if (config.network.connectionTimeoutMs < kMinConnectionTimeout) {
        result.warnings.push_back("network.connectionTimeoutMs < 100ms may cause connection issues");
    }
    if (!config.network.verifySsl) {
        result.warnings.push_back("network.verifySsl is disabled - this is a security risk");
    }

    // Logging validation
    static const std::unordered_set<std::string> validLevels = {
        "trace", "debug", "info", "warn", "error", "critical"
    };
    if (!validLevels.count(config.logging.level)) {
        result.errors.push_back("logging.level must be one of: trace, debug, info, warn, error, critical");
        result.valid = false;
    }

    // Directory validation
    if (!config.dataDirectory.empty() && !fs::exists(config.dataDirectory)) {
        result.warnings.push_back("dataDirectory does not exist and will be created");
    }

    return result;
}

// =============================================================================
// CONFIGMANAGER IMPLEMENTATION
// =============================================================================

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::initialize(const fs::path& configPath) {
    std::lock_guard lock(mutex_);

    configPath_ = configPath;
    config_ = CoreConfig::loadFromFile(configPath);

    auto validation = validateConfig(config_);
    if (!validation.valid) {
        for (const auto& error : validation.errors) {
            MAKINE_LOG_ERROR(log::CONFIG, "Configuration error: {}", error);
        }
        return false;
    }

    for (const auto& warning : validation.warnings) {
        MAKINE_LOG_WARN(log::CONFIG, "Configuration warning: {}", warning);
    }

    initialized_ = true;
    MAKINE_LOG_INFO(log::CONFIG, "Configuration manager initialized");
    return true;
}

const CoreConfig& ConfigManager::config() const {
    std::lock_guard lock(mutex_);
    return config_;
}

ConfigValidationResult ConfigManager::updateConfig(const CoreConfig& newConfig) {
    auto validation = validateConfig(newConfig);
    if (!validation.valid) {
        return validation;
    }

    CoreConfig oldConfig;
    {
        std::lock_guard lock(mutex_);
        oldConfig = config_;
        config_ = newConfig;
    }

    notifyObservers(oldConfig, newConfig);
    MAKINE_LOG_INFO(log::CONFIG, "Configuration updated");

    return validation;
}

bool ConfigManager::reloadFromFile() {
    if (configPath_.empty()) {
        MAKINE_LOG_ERROR(log::CONFIG, "Cannot reload: no config file path set");
        return false;
    }

    auto newConfig = CoreConfig::loadFromFile(configPath_);
    auto validation = updateConfig(newConfig);
    return validation.valid;
}

bool ConfigManager::saveToFile() {
    if (configPath_.empty()) {
        MAKINE_LOG_ERROR(log::CONFIG, "Cannot save: no config file path set");
        return false;
    }

    std::lock_guard lock(mutex_);
    config_.saveToFile(configPath_);
    return true;
}

size_t ConfigManager::addObserver(ConfigChangedCallback callback) {
    std::lock_guard lock(observerMutex_);
    size_t id = nextObserverId_++;
    observers_.emplace_back(id, std::move(callback));
    return id;
}

void ConfigManager::removeObserver(size_t observerId) {
    std::lock_guard lock(observerMutex_);
    observers_.erase(
        std::remove_if(observers_.begin(), observers_.end(),
            [observerId](const auto& pair) { return pair.first == observerId; }),
        observers_.end()
    );
}

void ConfigManager::notifyObservers(const CoreConfig& oldConfig, const CoreConfig& newConfig) {
    std::vector<ConfigChangedCallback> callbacks;
    {
        std::lock_guard lock(observerMutex_);
        callbacks.reserve(observers_.size());
        for (const auto& [id, callback] : observers_) {
            callbacks.push_back(callback);
        }
    }

    for (const auto& callback : callbacks) {
        try {
            callback(oldConfig, newConfig);
        } catch (const std::exception& e) {
            MAKINE_LOG_ERROR(log::CONFIG, "Observer callback threw exception: {}", e.what());
        }
    }
}

} // namespace makine
