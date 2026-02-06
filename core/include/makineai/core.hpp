/**
 * @file core.hpp
 * @brief MakineAI main header - includes all public APIs
 * @copyright (c) 2026 MakineAI Team
 *
 * This is the main header file for the MakineAI C++ Native Core.
 * Include this file to access all MakineAI functionality.
 *
 * @code
 * #include <makineai/core.hpp>
 *
 * int main() {
 *     auto& core = makineai::Core::instance();
 *     auto initResult = core.initialize();
 *     if (!initResult) {
 *         std::cerr << initResult.error().message() << std::endl;
 *         return 1;
 *     }
 *
 *     // System is healthy?
 *     if (!core.isHealthy()) {
 *         std::cerr << "System health check failed" << std::endl;
 *     }
 *
 *     // Scan for games
 *     auto games = core.gameDetector().scanAll();
 *     for (const auto& game : games) {
 *         std::cout << game.name << std::endl;
 *     }
 *
 *     // Check metrics
 *     std::cout << core.metrics().toText() << std::endl;
 * }
 * @endcode
 */

#pragma once

// Version information
#define MAKINEAI_VERSION_MAJOR 0
#define MAKINEAI_VERSION_MINOR 1
#define MAKINEAI_VERSION_PATCH 0
#define MAKINEAI_VERSION_STRING "0.1.0-alpha"

// Core headers
#include "types.hpp"
#include "error.hpp"
#include "features.hpp"

// Infrastructure headers
#include "logging.hpp"
#include "config.hpp"
#include "metrics.hpp"
#include "health.hpp"
#include "audit.hpp"
#include "debug.hpp"
#include "validation.hpp"
#include "cache.hpp"
#include "async.hpp"

// Module headers
#include "database.hpp"
#include "asset_parser.hpp"
#include "patch_engine.hpp"
#include "game_detector.hpp"
#include "package_manager.hpp"
#include "runtime_manager.hpp"
#include "security.hpp"
#include "version_tracker.hpp"
#include "translation_memory.hpp"
#include "glossary_service.hpp"
#include "qa_service.hpp"
#include "translation_pipeline.hpp"

// Third-party includes
#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace makineai {

// Note: Forward declarations removed - types are imported via 'using' aliases
// from their respective headers (asset_parser.hpp, game_detector.hpp, etc.)

/**
 * @brief Initialization options for MakineAI core
 */
struct InitOptions {
    bool skipHealthCheck = false;     // Skip initial health check
    bool skipDatabaseInit = false;    // Don't initialize database (for testing)
    bool enableMetrics = true;        // Enable performance metrics
    bool enableAuditLog = true;       // Enable audit logging
    bool enableDebugDumps = false;    // Enable debug dumps on error
    bool verboseLogging = false;      // Enable verbose startup logging
};

/**
 * @brief Core initialization result with details
 */
struct InitResult {
    bool success = false;
    std::string message;
    HealthStatus healthStatus;
    std::chrono::milliseconds initDuration{0};

    // Feature availability
    struct {
        bool hasTaskflow = false;
        bool hasSimdjson = false;
        bool hasMio = false;
        bool hasLibsodium = false;
        bool hasBit7z = false;
        bool hasEfsw = false;
    } features;
};

/**
 * @brief Main MakineAI core class
 *
 * Singleton class that provides access to all MakineAI functionality.
 * Thread-safe for concurrent access.
 *
 * Initialization order:
 * 1. Crypto subsystem
 * 2. Logging
 * 3. Configuration
 * 4. Health check
 * 5. Database
 * 6. Core modules
 * 7. Translation services
 * 8. Metrics & Audit
 */
class Core {
public:
    /**
     * @brief Get the singleton instance
     */
    static Core& instance();

    // Delete copy/move constructors
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    Core(Core&&) = delete;
    Core& operator=(Core&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the core with configuration
     * @param config Configuration settings (uses ConfigManager)
     * @param options Initialization options
     * @return Result indicating success or failure with details
     */
    Result<InitResult> initialize(
        const CoreConfig& config = CoreConfig::getDefaults(),
        const InitOptions& options = {}
    );

    /**
     * @brief Shutdown and cleanup all resources
     *
     * Shuts down modules in reverse initialization order.
     * Safe to call multiple times.
     */
    void shutdown();

    /**
     * @brief Check if core is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept {
        return initialized_.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Configuration & Infrastructure
    // =========================================================================

    /**
     * @brief Get configuration manager
     *
     * Provides runtime configuration access and modification.
     */
    [[nodiscard]] ConfigManager& configManager() noexcept {
        return ConfigManager::instance();
    }

    /**
     * @brief Get current configuration (shorthand)
     */
    [[nodiscard]] const CoreConfig& config() const noexcept;

    /**
     * @brief Get metrics collector
     *
     * Access performance metrics, counters, and histograms.
     */
    [[nodiscard]] Metrics& metrics() noexcept {
        return Metrics::instance();
    }

    /**
     * @brief Get health checker
     *
     * System health monitoring and validation.
     */
    [[nodiscard]] HealthChecker& healthChecker() noexcept {
        return HealthChecker::instance();
    }

    /**
     * @brief Quick health check
     */
    [[nodiscard]] bool isHealthy() const;

    /**
     * @brief Get full health status
     */
    [[nodiscard]] HealthStatus getHealthStatus() const;

    /**
     * @brief Get audit logger
     *
     * Security-critical event logging.
     */
    [[nodiscard]] AuditLogger& auditLogger() noexcept {
        return AuditLogger::instance();
    }

    /**
     * @brief Get debug dumper
     *
     * State dumps and crash reports.
     */
    [[nodiscard]] DebugDumper& debugDumper() noexcept {
        return DebugDumper::instance();
    }

    /**
     * @brief Get cache manager
     *
     * Access to LRU/TTL caches for games, translations, etc.
     */
    [[nodiscard]] CacheManager& caches() noexcept {
        return CacheManager::instance();
    }

    // =========================================================================
    // Core Modules
    // =========================================================================

    /**
     * @brief Get database instance
     */
    [[nodiscard]] Database& database();

    /**
     * @brief Get asset parser
     */
    [[nodiscard]] AssetParser& assetParser();

    /**
     * @brief Get patch engine
     */
    [[nodiscard]] PatchEngine& patchEngine();

    /**
     * @brief Get game detector
     */
    [[nodiscard]] GameDetector& gameDetector();

    /**
     * @brief Get package manager
     */
    [[nodiscard]] PackageManager& packageManager();

    /**
     * @brief Get runtime manager (BepInEx/XUnity)
     */
    [[nodiscard]] RuntimeManager& runtimeManager();

    /**
     * @brief Get security manager
     */
    [[nodiscard]] SecurityManager& securityManager();

    /**
     * @brief Get version tracker
     */
    [[nodiscard]] VersionTracker& versionTracker();

    // =========================================================================
    // Translation Services
    // =========================================================================

    /**
     * @brief Get translation memory
     */
    [[nodiscard]] TranslationMemory& translationMemory();

    /**
     * @brief Get glossary service
     */
    [[nodiscard]] GlossaryService& glossaryService();

    /**
     * @brief Get QA service
     */
    [[nodiscard]] QAService& qaService();

    /**
     * @brief Get translation pipeline (decision engine)
     */
    [[nodiscard]] TranslationPipeline& translationPipeline();

    // =========================================================================
    // Async Operations
    // =========================================================================

    /**
     * @brief Scan for games asynchronously
     *
     * @param progress Optional progress callback
     * @return Async operation that resolves to game list
     */
    [[nodiscard]] AsyncOperationPtr<std::vector<GameInfo>> scanGamesAsync(
        ProgressCallback progress = nullptr
    );

    /**
     * @brief Apply translation package asynchronously
     *
     * @param game Target game
     * @param packageId Translation package ID
     * @param progress Optional progress callback
     * @return Async operation that resolves to patch result
     */
    [[nodiscard]] AsyncOperationPtr<PatchResult> applyTranslationAsync(
        const GameInfo& game,
        const std::string& packageId,
        ProgressCallback progress = nullptr
    );

    /**
     * @brief Get background task queue
     *
     * Queue for background operations.
     */
    [[nodiscard]] AsyncQueue& taskQueue() { return taskQueue_; }

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Get MakineAI version string
     */
    [[nodiscard]] static constexpr std::string_view version() noexcept {
        return MAKINEAI_VERSION_STRING;
    }

    /**
     * @brief Get logger instance
     */
    [[nodiscard]] std::shared_ptr<spdlog::logger> logger() const { return logger_; }

    /**
     * @brief Check feature availability
     */
    [[nodiscard]] static const Features& features() noexcept {
        static Features f;
        return f;
    }

    /**
     * @brief Get initialization result (after initialize() called)
     */
    [[nodiscard]] const InitResult& initResult() const noexcept { return initResult_; }

    /**
     * @brief Register shutdown callback
     *
     * Callbacks are invoked in reverse registration order during shutdown.
     */
    void onShutdown(std::function<void()> callback);

private:
    Core() = default;
    ~Core();

    // Initialization helpers
    Result<void> initializeCrypto();
    Result<void> initializeLogging(const CoreConfig& config, bool verbose);
    Result<void> initializeDatabase(const CoreConfig& config);
    Result<void> initializeModules(const CoreConfig& config);
    Result<void> initializeTranslationServices(const CoreConfig& config);
    void configureHealthChecker(const CoreConfig& config);
    void configureAuditLogger(const CoreConfig& config);
    void logFeatureAvailability();

    // State
    std::atomic<bool> initialized_{false};
    InitResult initResult_;
    std::shared_ptr<spdlog::logger> logger_;

    // Module instances (owned)
    std::unique_ptr<AssetParser> assetParser_;
    std::unique_ptr<PatchEngine> patchEngine_;
    std::unique_ptr<GameDetector> gameDetector_;
    std::unique_ptr<PackageManager> packageManager_;
    std::unique_ptr<RuntimeManager> runtimeManager_;
    std::unique_ptr<SecurityManager> securityManager_;
    std::unique_ptr<VersionTracker> versionTracker_;
    std::unique_ptr<TranslationMemory> translationMemory_;
    std::unique_ptr<GlossaryService> glossaryService_;
    std::unique_ptr<QAService> qaService_;
    std::unique_ptr<TranslationPipeline> translationPipeline_;

    // Background task queue
    AsyncQueue taskQueue_;

    // Shutdown callbacks
    std::vector<std::function<void()>> shutdownCallbacks_;

    // Thread safety
    mutable std::mutex mutex_;
};

// =============================================================================
// Convenience Functions
// =============================================================================
// Note: metrics(), healthChecker(), caches(), auditLogger(), configManager()
// are already defined in their respective headers (metrics.hpp, health.hpp, etc.)
// Only logger() is unique to core.hpp

/**
 * @brief Quick access to logger
 */
inline std::shared_ptr<spdlog::logger> logger() {
    return Core::instance().logger();
}

} // namespace makineai
