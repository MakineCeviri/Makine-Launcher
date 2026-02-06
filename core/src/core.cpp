/**
 * @file core.cpp
 * @brief MakineAI core implementation with full system integration
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/core.hpp"
#include "makineai/crypto_utils.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace makineai {

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
    ConfigManager::instance().updateConfig(config);

    if (auto cryptoResult = initializeCrypto(); !cryptoResult) {
        return std::unexpected(cryptoResult.error());
    }

    if (auto logResult = initializeLogging(config, options.verboseLogging); !logResult) {
        return std::unexpected(logResult.error());
    }

    MAKINEAI_LOG_INFO(log::CORE, "MakineAI Core {} initializing...", version());

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
        MAKINEAI_LOG_DEBUG(log::CORE, "Running pre-flight health check...");
        result.healthStatus = HealthChecker::instance().check();

        if (!result.healthStatus.healthy) {
            MAKINEAI_LOG_WARN(log::CORE, "Health check warnings: {}",
                result.healthStatus.toText());

            // Only fail on critical issues
            if (result.healthStatus.availableMemoryBytes < 100 * 1024 * 1024) {  // 100 MB
                return std::unexpected(Error(ErrorCode::Unknown,
                    "Insufficient memory: " + result.healthStatus.toText()));
            }
        }
    }

    if (!options.skipDatabaseInit) {
        if (auto dbResult = initializeDatabase(config); !dbResult) {
            MAKINEAI_LOG_ERROR(log::CORE, "Database initialization failed: {}",
                dbResult.error().message());
            return std::unexpected(dbResult.error());
        }
    }

    if (auto modResult = initializeModules(config); !modResult) {
        MAKINEAI_LOG_ERROR(log::CORE, "Module initialization failed: {}",
            modResult.error().message());
        return std::unexpected(modResult.error());
    }

    if (auto transResult = initializeTranslationServices(config); !transResult) {
        MAKINEAI_LOG_ERROR(log::CORE, "Translation services initialization failed: {}",
            transResult.error().message());
        return std::unexpected(transResult.error());
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
    result.message = "MakineAI Core initialized successfully";
    initResult_ = result;

    initialized_.store(true, std::memory_order_release);

    AuditLogger::instance().logSystemEvent("core_initialized",
        "Version: " + std::string(version()) +
        ", Duration: " + std::to_string(result.initDuration.count()) + "ms");

    MAKINEAI_LOG_INFO(log::CORE, "MakineAI Core initialized in {}ms",
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

    MAKINEAI_LOG_INFO(log::CORE, "MakineAI Core shutting down...");

    // Invoke shutdown callbacks in reverse order
    for (auto it = shutdownCallbacks_.rbegin(); it != shutdownCallbacks_.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& ex) {
            MAKINEAI_LOG_WARN(log::CORE, "Shutdown callback exception: {}", ex.what());
        }
    }
    shutdownCallbacks_.clear();

    // Note: taskQueue_ will be cleaned up by its destructor

    // Audit log before we lose logging
    AuditLogger::instance().logSystemEvent("core_shutdown", "Normal shutdown");

    // Cleanup modules in reverse initialization order
    translationPipeline_.reset();
    qaService_.reset();
    glossaryService_.reset();
    translationMemory_.reset();
    versionTracker_.reset();
    securityManager_.reset();
    runtimeManager_.reset();
    packageManager_.reset();
    gameDetector_.reset();
    patchEngine_.reset();
    assetParser_.reset();

    // Database last (others may depend on it)
    // Database is a singleton, so just ensure it's clean

    MAKINEAI_LOG_INFO(log::CORE, "MakineAI Core shutdown complete");

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
    return makineai::initializeCrypto();
}

Result<void> Core::initializeLogging(const CoreConfig& config, bool verbose) {
    try {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (fs::path(config.logsDirectory) / "makineai.log").string(), true);

        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        logger_ = std::make_shared<spdlog::logger>("makineai", sinks.begin(), sinks.end());

        // Set log level
        auto level = verbose ? spdlog::level::debug : config.logLevel;
        logger_->set_level(level);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

        spdlog::set_default_logger(logger_);
        return {};
    } catch (const spdlog::spdlog_ex& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            std::string("Logger initialization failed: ") + ex.what()));
    }
}

Result<void> Core::initializeDatabase(const CoreConfig& config) {
    MAKINEAI_LOG_DEBUG(log::CORE, "Initializing database...");
    auto timer = Metrics::instance().timer("db_init");

    auto dbPath = fs::path(config.dataDirectory) / "makineai.db";
    auto result = Database::instance().initialize(dbPath);

    if (!result) {
        return std::unexpected(result.error());
    }

    MAKINEAI_LOG_DEBUG(log::CORE, "Database initialized at: {}", dbPath.string());
    return {};
}

Result<void> Core::initializeModules(const CoreConfig& config) {
    MAKINEAI_LOG_DEBUG(log::CORE, "Initializing core modules...");
    auto timer = Metrics::instance().timer("modules_init");

    try {
        // Asset Parser
        assetParser_ = std::make_unique<AssetParser>();

        // Patch Engine
        patchEngine_ = std::make_unique<PatchEngine>();
        patchEngine_->setBackupDirectory(fs::path(config.dataDirectory) / "backups");

        // Game Detector
        gameDetector_ = std::make_unique<GameDetector>();

        // Package Manager
        packageManager_ = std::make_unique<PackageManager>();
        packageManager_->setApiUrl(config.apiBaseUrl);
        packageManager_->setCacheDirectory(fs::path(config.cacheDirectory) / "packages");

        // Runtime Manager
        runtimeManager_ = std::make_unique<RuntimeManager>();
        runtimeManager_->setBundleDirectory(fs::path(config.dataDirectory) / "runtime");

        // Security Manager
        securityManager_ = std::make_unique<SecurityManager>();
        if (!config.publicKeyPath.empty() && fs::exists(config.publicKeyPath)) {
            auto result = securityManager_->loadPublicKey(config.publicKeyPath);
            if (!result) {
                MAKINEAI_LOG_WARN(log::CORE, "Failed to load public key: {}",
                    result.error().message());
                AuditLogger::instance().logSystemEvent("public_key_load_failed",
                    result.error().message());
            }
        }

        // Version Tracker
        versionTracker_ = std::make_unique<VersionTracker>();
        versionTracker_->setDatabasePath(fs::path(config.dataDirectory) / "versions.db");

        MAKINEAI_LOG_DEBUG(log::CORE, "Core modules initialized");
        return {};

    } catch (const std::exception& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            std::string("Module initialization failed: ") + ex.what()));
    }
}

Result<void> Core::initializeTranslationServices(const CoreConfig& config) {
    MAKINEAI_LOG_DEBUG(log::CORE, "Initializing translation services...");
    auto timer = Metrics::instance().timer("translation_services_init");

    try {
        // Translation Memory
        translationMemory_ = std::make_unique<TranslationMemory>();

        // Glossary Service (use singleton pattern's instance)
        // GlossaryService is a singleton, we don't create a new one
        // glossaryService_ = std::make_unique<GlossaryService>();

        // QA Service (has static methods only)
        qaService_ = std::make_unique<QAService>();

        // Translation Pipeline (Decision Engine)
        translationPipeline_ = std::make_unique<TranslationPipeline>();

        MAKINEAI_LOG_DEBUG(log::CORE, "Translation services initialized");
        return {};

    } catch (const std::exception& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            std::string("Translation services initialization failed: ") + ex.what()));
    }
}

void Core::configureHealthChecker(const CoreConfig& config) {
    auto& checker = HealthChecker::instance();

    // Set minimum disk space (1 GB default)
    checker.setMinDiskSpace(1024 * 1024 * 1024);

    // Set data directory for disk checks
    checker.setDataDirectory(fs::path(config.dataDirectory));

    // Set database path
    checker.setDatabasePath(fs::path(config.dataDirectory) / "makineai.db");
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
    MAKINEAI_LOG_INFO(log::CORE, "=== Feature Availability ===");
    MAKINEAI_LOG_INFO(log::CORE, "  Taskflow (parallel):    {}", Features::has_taskflow ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  simdjson (fast JSON):   {}", Features::has_simdjson ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  mio (memory-mapped):    {}", Features::has_mio ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  libsodium (crypto):     {}", Features::has_sodium ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  bit7z (7-zip):          {}", Features::has_bit7z ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  libarchive (archives):  {}", Features::has_libarchive ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  efsw (file watcher):    {}", Features::has_efsw ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  simdutf (UTF conv):     {}", Features::has_simdutf ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  SQLiteCpp:              {}", Features::has_sqlitecpp ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  concurrentqueue:        {}", Features::has_concurrentqueue ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "============================");
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

AssetParser& Core::assetParser() {
    if (!assetParser_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "AssetParser not initialized"));
    }
    return *assetParser_;
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

PackageManager& Core::packageManager() {
    if (!packageManager_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "PackageManager not initialized"));
    }
    return *packageManager_;
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

VersionTracker& Core::versionTracker() {
    if (!versionTracker_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "VersionTracker not initialized"));
    }
    return *versionTracker_;
}

TranslationMemory& Core::translationMemory() {
    if (!translationMemory_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "TranslationMemory not initialized"));
    }
    return *translationMemory_;
}

GlossaryService& Core::glossaryService() {
    // GlossaryService is a singleton
    return GlossaryService::instance();
}

QAService& Core::qaService() {
    if (!qaService_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "QAService not initialized"));
    }
    return *qaService_;
}

TranslationPipeline& Core::translationPipeline() {
    if (!translationPipeline_) {
        throw Exception(Error(ErrorCode::InvalidArgument, "TranslationPipeline not initialized"));
    }
    return *translationPipeline_;
}

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

AsyncOperationPtr<PatchResult> Core::applyTranslationAsync(
    const GameInfo& game,
    const std::string& packageId,
    ProgressCallback progress
) {
    // TODO(makineai): Implement proper async translation application
    // For now, return a not-implemented error
    return executeAsync<PatchResult>([game, packageId]() -> Result<PatchResult> {
        return std::unexpected(Error(ErrorCode::NotImplemented,
            "Async translation application not yet implemented"));
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
        } catch (...) {
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
    MAKINEAI_LOG_INFO(log::CORE, "=== Feature Availability ===");
    MAKINEAI_LOG_INFO(log::CORE, "  Taskflow (parallel):    {}", Features::has_taskflow ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  simdjson (fast JSON):   {}", Features::has_simdjson ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  mio (memory-mapped):    {}", Features::has_mio ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  libsodium (crypto):     {}", Features::has_sodium ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  bit7z (7-zip):          {}", Features::has_bit7z ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  libarchive (archives):  {}", Features::has_libarchive ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  efsw (file watcher):    {}", Features::has_efsw ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  simdutf (UTF conv):     {}", Features::has_simdutf ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  SQLiteCpp:              {}", Features::has_sqlitecpp ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "  concurrentqueue:        {}", Features::has_concurrentqueue ? "YES" : "NO");
    MAKINEAI_LOG_INFO(log::CORE, "============================");
}

} // namespace makineai
