/**
 * @file core.cpp
 * @brief Makine core implementation with full system integration
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/core.hpp"
#include "makine/crypto_utils.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace makine {

// =============================================================================
// Singleton Instance
// =============================================================================

Core& Core::instance() {
    static Core instance;
    return instance;
}

Core::~Core() {
    shutdown();
}

// =============================================================================
// Main Initialization
// =============================================================================

Result<InitResult> Core::initialize(const CoreConfig& config, const InitOptions& options) {
    std::lock_guard lock(mutex_);

    if (initialized_.load(std::memory_order_acquire)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Core already initialized"));
    }

    auto startTime = std::chrono::steady_clock::now();
    InitResult result;

    // Store config in ConfigManager
    auto configValidation = ConfigManager::instance().updateConfig(config);
    if (!configValidation.valid) {
        return std::unexpected(Error(ErrorCode::InvalidConfiguration,
            fmt::format("Configuration validation failed: {}", configValidation.errors.front())));
    }

    if (auto cryptoResult = initializeCrypto(); !cryptoResult) {
        return std::unexpected(cryptoResult.error());
    }

    if (auto logResult = initializeLogging(config, options.verboseLogging); !logResult) {
        return std::unexpected(logResult.error());
    }

    MAKINE_LOG_INFO(log::CORE, "Makine Core {} initializing...", version());

    // Create required directories
    std::error_code ec;
    fs::create_directories(fs::path(config.dataDirectory), ec);
    fs::create_directories(fs::path(config.cacheDirectory), ec);
    fs::create_directories(fs::path(config.logsDirectory), ec);
    fs::create_directories(fs::path(config.dataDirectory) / "backups", ec);
    fs::create_directories(fs::path(config.dataDirectory) / "runtime", ec);
    fs::create_directories(fs::path(config.dataDirectory) / "dumps", ec);

    // Pre-flight health check
    configureHealthChecker(config);
    if (!options.skipHealthCheck) {
        MAKINE_LOG_DEBUG(log::CORE, "Running pre-flight health check...");
        result.healthStatus = HealthChecker::instance().check();

        if (!result.healthStatus.healthy) {
            MAKINE_LOG_WARN(log::CORE, "Health check warnings: {}",
                result.healthStatus.toText());

            // Only fail on critical issues
            constexpr uint64_t kMinAvailableMemory = 100ULL * 1024 * 1024;  // 100 MB
            if (result.healthStatus.availableMemoryBytes < kMinAvailableMemory) {
                return std::unexpected(Error(ErrorCode::Unknown,
                    fmt::format("Insufficient memory: {}", result.healthStatus.toText())));
            }
        }
    }

    if (!options.skipDatabaseInit) {
        if (auto dbResult = initializeDatabase(config); !dbResult) {
            MAKINE_LOG_ERROR(log::CORE, "Database initialization failed: {}",
                dbResult.error().message());
            return std::unexpected(dbResult.error());
        }
    }

    if (auto modResult = initializeModules(config); !modResult) {
        MAKINE_LOG_ERROR(log::CORE, "Module initialization failed: {}",
            modResult.error().message());
        return std::unexpected(modResult.error());
    }

    if (options.enableAuditLog) {
        configureAuditLogger(config);
    }

    if (options.enableDebugDumps) {
        DebugConfig debugConfig;
        debugConfig.dumpDirectory = fs::path(config.dataDirectory) / "dumps";
        debugConfig.maxDumpFiles = 50;
        debugConfig.maxDumpSizeMB = 500;
        debugConfig.includeStackTrace = true;
        DebugDumper::instance().configure(debugConfig);
    }

    logFeatureAvailability();

    auto endTime = std::chrono::steady_clock::now();
    result.initDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    if (options.enableMetrics) {
        Metrics::instance().recordDuration("core_init", result.initDuration);
    }

    result.features.hasTaskflow = Features::has_taskflow;
    result.features.hasSimdjson = Features::has_simdjson;
    result.features.hasMio = Features::has_mio;
    result.features.hasLibsodium = Features::has_sodium;
    result.features.hasBit7z = Features::has_bit7z;
    result.features.hasEfsw = Features::has_efsw;

    result.success = true;
    result.message = "Makine Core initialized successfully";
    initResult_ = result;

    initialized_.store(true, std::memory_order_release);

    AuditLogger::instance().logSystemEvent("core_initialized",
        fmt::format("Version: {}, Duration: {}ms", version(), result.initDuration.count()));

    MAKINE_LOG_INFO(log::CORE, "Makine Core initialized in {}ms",
        result.initDuration.count());

    return result;
}

// =============================================================================
// Shutdown
// =============================================================================

void Core::shutdown() {
    std::lock_guard lock(mutex_);

    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }

    MAKINE_LOG_INFO(log::CORE, "Makine Core shutting down...");

    // Invoke shutdown callbacks in reverse order
    for (auto it = shutdownCallbacks_.rbegin(); it != shutdownCallbacks_.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& ex) {
            MAKINE_LOG_WARN(log::CORE, "Shutdown callback exception: {}", ex.what());
        }
    }
    shutdownCallbacks_.clear();

    // Note: taskQueue_ will be cleaned up by its destructor

    // Audit log before we lose logging
    AuditLogger::instance().logSystemEvent("core_shutdown", "Normal shutdown");

    // Cleanup modules in reverse initialization order
    securityManager_.reset();
    runtimeManager_.reset();
    gameDetector_.reset();
    patchEngine_.reset();

    // Database last (others may depend on it)
    // Database is a singleton, so just ensure it's clean

    MAKINE_LOG_INFO(log::CORE, "Makine Core shutdown complete");

    // Reset logger last
    logger_.reset();

    initialized_.store(false, std::memory_order_release);
}

// =============================================================================
// Initialization Helpers
// =============================================================================

Result<void> initializeCrypto() {
    if (!crypto::initialize()) {
        return std::unexpected(Error(ErrorCode::Unknown,
            "Failed to initialize cryptographic subsystem"));
    }
    return {};
}

Result<void> Core::initializeCrypto() {
    return makine::initializeCrypto();
}

Result<void> Core::initializeLogging(const CoreConfig& config, bool verbose) {
    try {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (fs::path(config.logsDirectory) / "makine.log").string(), true);

        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        logger_ = std::make_shared<spdlog::logger>("makine", sinks.begin(), sinks.end());

        // Set log level
        auto level = verbose ? spdlog::level::debug : config.logLevel;
        logger_->set_level(level);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

        spdlog::set_default_logger(logger_);
        return {};
    } catch (const spdlog::spdlog_ex& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            fmt::format("Logger initialization failed: {}", ex.what())));
    }
}

Result<void> Core::initializeDatabase(const CoreConfig& config) {
    MAKINE_LOG_DEBUG(log::CORE, "Initializing database...");
    auto timer = Metrics::instance().timer("db_init");

    auto dbPath = fs::path(config.dataDirectory) / "makine.db";
    auto result = Database::instance().initialize(dbPath);

    if (!result) {
        return std::unexpected(result.error());
    }

    MAKINE_LOG_DEBUG(log::CORE, "Database initialized at: {}", dbPath.string());
    return {};
}

Result<void> Core::initializeModules(const CoreConfig& config) {
    MAKINE_LOG_DEBUG(log::CORE, "Initializing core modules...");
    auto timer = Metrics::instance().timer("modules_init");

    try {
        // Patch Engine
        patchEngine_ = std::make_unique<PatchEngine>();
        patchEngine_->setBackupDirectory(fs::path(config.dataDirectory) / "backups");

        // Game Detector
        gameDetector_ = std::make_unique<GameDetector>();

        // Runtime Manager
        runtimeManager_ = std::make_unique<RuntimeManager>();

        // Security Manager — load embedded key first, fall back to external
        securityManager_ = std::make_unique<SecurityManager>();
        {
            auto embeddedResult = securityManager_->loadEmbeddedKey();
            if (!embeddedResult) {
                MAKINE_LOG_DEBUG(log::CORE, "Embedded key not available: {}",
                    embeddedResult.error().message());

                // Fall back to external key file if configured
                if (!config.publicKeyPath.empty() && fs::exists(config.publicKeyPath)) {
                    auto fileResult = securityManager_->loadPublicKey(config.publicKeyPath);
                    if (!fileResult) {
                        MAKINE_LOG_WARN(log::CORE, "Failed to load public key: {}",
                            fileResult.error().message());
                        AuditLogger::instance().logSystemEvent("public_key_load_failed",
                            fileResult.error().message());
                    }
                } else {
                    MAKINE_LOG_WARN(log::CORE,
                        "No public key available — package signature verification will fail");
                }
            } else {
                MAKINE_LOG_INFO(log::CORE, "Embedded public key loaded successfully");
            }
        }

        MAKINE_LOG_DEBUG(log::CORE, "Core modules initialized");
        return {};

    } catch (const std::exception& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            fmt::format("Module initialization failed: {}", ex.what())));
    }
}

void Core::configureHealthChecker(const CoreConfig& config) {
    auto& checker = HealthChecker::instance();

    // Set minimum disk space (1 GB default)
    checker.setMinDiskSpace(1024 * 1024 * 1024);

    // Set data directory for disk checks
    checker.setDataDirectory(fs::path(config.dataDirectory));

    // Set database path
    checker.setDatabasePath(fs::path(config.dataDirectory) / "makine.db");
}

void Core::configureAuditLogger(const CoreConfig& config) {
    AuditConfig auditConfig;
    auditConfig.enabled = true;
    auditConfig.logFile = fs::path(config.logsDirectory) / "audit.log";
    auditConfig.maxLogSizeMB = 10;
    auditConfig.maxRetainedLogs = 90;
    auditConfig.logToConsole = false;

    AuditLogger::instance().configure(auditConfig);
}

void Core::logFeatureAvailability() {
    MAKINE_LOG_INFO(log::CORE, "=== Feature Availability ===");
    MAKINE_LOG_INFO(log::CORE, "  Taskflow (parallel):    {}", Features::has_taskflow ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  simdjson (fast JSON):   {}", Features::has_simdjson ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  mio (memory-mapped):    {}", Features::has_mio ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  libsodium (crypto):     {}", Features::has_sodium ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  bit7z (7-zip):          {}", Features::has_bit7z ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  libarchive (archives):  {}", Features::has_libarchive ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  efsw (file watcher):    {}", Features::has_efsw ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  simdutf (UTF conv):     {}", Features::has_simdutf ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  SQLiteCpp:              {}", Features::has_sqlitecpp ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  concurrentqueue:        {}", Features::has_concurrentqueue ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "============================");
}

// =============================================================================
// Configuration Access
// =============================================================================

const CoreConfig& Core::config() const noexcept {
    return ConfigManager::instance().config();
}

// =============================================================================
// Health Check
// =============================================================================

bool Core::isHealthy() const {
    return HealthChecker::instance().check().healthy;
}

HealthStatus Core::getHealthStatus() const {
    return HealthChecker::instance().check();
}

// =============================================================================
// Module Accessors
// =============================================================================

Database& Core::database() {
    return Database::instance();
}

PatchEngine& Core::patchEngine() {
    if (!patchEngine_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "PatchEngine not initialized"));
    }
    return *patchEngine_;
}

GameDetector& Core::gameDetector() {
    if (!gameDetector_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "GameDetector not initialized"));
    }
    return *gameDetector_;
}

RuntimeManager& Core::runtimeManager() {
    if (!runtimeManager_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "RuntimeManager not initialized"));
    }
    return *runtimeManager_;
}

SecurityManager& Core::securityManager() {
    if (!securityManager_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "SecurityManager not initialized"));
    }
    return *securityManager_;
}

// Translation service accessors are now inline in core.hpp (nullable pointers)

// =============================================================================
// Async Operations
// =============================================================================

AsyncOperationPtr<std::vector<GameInfo>> Core::scanGamesAsync(ProgressCallback progress) {
    // Simple synchronous implementation wrapped in async
    return executeAsync<std::vector<GameInfo>>([this]() -> Result<std::vector<GameInfo>> {
        if (!gameDetector_) {
            return std::unexpected(Error(ErrorCode::InvalidArgument,
                "GameDetector not initialized"));
        }

        auto result = gameDetector_->scanAll(nullptr);
        if (!result) {
            return std::unexpected(result.error());
        }

        // Record metric
        Metrics::instance().increment("games_scanned", static_cast<int64_t>(result->size()));

        return *result;
    });
}


// =============================================================================
// Shutdown Callbacks
// =============================================================================

void Core::onShutdown(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    shutdownCallbacks_.push_back(std::move(callback));
}

// =============================================================================
// Version Helper
// =============================================================================

std::optional<Version> Version::parse(std::string_view str) {
    Version v{};
    size_t pos = 0;
    size_t count = 0;
    uint32_t* parts[] = {&v.major, &v.minor, &v.patch, &v.build};

    while (pos < str.size() && count < 4) {
        size_t end = str.find('.', pos);
        if (end == std::string_view::npos) {
            end = str.size();
        }

        auto part = str.substr(pos, end - pos);
        try {
            *parts[count] = static_cast<uint32_t>(std::stoul(std::string(part)));
        } catch (const std::exception&) {
            return std::nullopt;
        }

        ++count;
        pos = end + 1;
    }

    return v;
}

// =============================================================================
// Features Logging
// =============================================================================

void Features::log_features() {
    // Log directly without Core access
    MAKINE_LOG_INFO(log::CORE, "=== Feature Availability ===");
    MAKINE_LOG_INFO(log::CORE, "  Taskflow (parallel):    {}", Features::has_taskflow ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  simdjson (fast JSON):   {}", Features::has_simdjson ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  mio (memory-mapped):    {}", Features::has_mio ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  libsodium (crypto):     {}", Features::has_sodium ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  bit7z (7-zip):          {}", Features::has_bit7z ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  libarchive (archives):  {}", Features::has_libarchive ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  efsw (file watcher):    {}", Features::has_efsw ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  simdutf (UTF conv):     {}", Features::has_simdutf ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  SQLiteCpp:              {}", Features::has_sqlitecpp ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "  concurrentqueue:        {}", Features::has_concurrentqueue ? "YES" : "NO");
    MAKINE_LOG_INFO(log::CORE, "============================");
}

} // namespace makine
