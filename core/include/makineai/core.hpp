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
 *     core.initialize();
 *
 *     auto games = core.gameDetector().scanAll();
 *     for (const auto& game : games) {
 *         std::cout << game.name << std::endl;
 *     }
 * }
 * @endcode
 */

#pragma once

// Version information
#define MAKINEAI_VERSION_MAJOR 1
#define MAKINEAI_VERSION_MINOR 0
#define MAKINEAI_VERSION_PATCH 0
#define MAKINEAI_VERSION_STRING "1.0.0"

// Core headers
#include "types.hpp"
#include "error.hpp"

// Module headers
#include "asset_parser.hpp"
#include "patch_engine.hpp"
#include "game_detector.hpp"
#include "package_manager.hpp"
#include "runtime_manager.hpp"
#include "security.hpp"
#include "version_tracker.hpp"

// Third-party includes
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>

namespace makineai {

// Forward declarations
class AssetParser;
class PatchEngine;
class GameDetector;
class PackageManager;
class RuntimeManager;
class SecurityManager;
class VersionTracker;

/**
 * @brief Configuration for MakineAI core
 */
struct CoreConfig {
    fs::path dataDirectory;       // Where MakineAI stores its data
    fs::path cacheDirectory;      // Cache for downloads
    fs::path logsDirectory;       // Log files
    std::string apiBaseUrl;       // Translation package API URL
    std::string publicKeyPath;    // Path to public key for verification
    spdlog::level::level_enum logLevel = spdlog::level::info;

    // Runtime configuration
    bool autoUpdateRuntime = true;   // Auto-update BepInEx/XUnity
    bool enableAnalytics = false;    // Usage analytics (opt-in)

    static CoreConfig defaultConfig();
};

/**
 * @brief Main MakineAI core class
 *
 * Singleton class that provides access to all MakineAI functionality.
 * Thread-safe for concurrent access.
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

    /**
     * @brief Initialize the core with configuration
     * @param config Configuration settings
     * @return Result indicating success or failure
     */
    VoidResult initialize(const CoreConfig& config = CoreConfig::defaultConfig());

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Check if core is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const CoreConfig& config() const noexcept { return config_; }

    // Module accessors
    [[nodiscard]] AssetParser& assetParser();
    [[nodiscard]] PatchEngine& patchEngine();
    [[nodiscard]] GameDetector& gameDetector();
    [[nodiscard]] PackageManager& packageManager();
    [[nodiscard]] RuntimeManager& runtimeManager();
    [[nodiscard]] SecurityManager& securityManager();
    [[nodiscard]] VersionTracker& versionTracker();

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

private:
    Core() = default;
    ~Core();

    bool initialized_ = false;
    CoreConfig config_;
    std::shared_ptr<spdlog::logger> logger_;

    // Module instances
    std::unique_ptr<AssetParser> assetParser_;
    std::unique_ptr<PatchEngine> patchEngine_;
    std::unique_ptr<GameDetector> gameDetector_;
    std::unique_ptr<PackageManager> packageManager_;
    std::unique_ptr<RuntimeManager> runtimeManager_;
    std::unique_ptr<SecurityManager> securityManager_;
    std::unique_ptr<VersionTracker> versionTracker_;

    mutable std::mutex mutex_;
};

/**
 * @brief Quick access to logger
 */
inline std::shared_ptr<spdlog::logger> logger() {
    return Core::instance().logger();
}

} // namespace makineai
