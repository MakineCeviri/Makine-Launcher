/**
 * @file core.cpp
 * @brief MakineAI core implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/core.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef _WIN32
#include <shlobj.h>
#include <objbase.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace makineai {

// Default configuration
CoreConfig CoreConfig::defaultConfig() {
    CoreConfig config;

    // Use standard Windows app data paths
#ifdef _WIN32
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) {
        fs::path basePath = fs::path(appData) / "MakineAI";
        CoTaskMemFree(appData);

        config.dataDirectory = basePath / "data";
        config.cacheDirectory = basePath / "cache";
        config.logsDirectory = basePath / "logs";
    } else {
        // Fallback to current directory
        config.dataDirectory = fs::current_path() / "data";
        config.cacheDirectory = fs::current_path() / "cache";
        config.logsDirectory = fs::current_path() / "logs";
    }
#else
    config.dataDirectory = fs::current_path() / "data";
    config.cacheDirectory = fs::current_path() / "cache";
    config.logsDirectory = fs::current_path() / "logs";
#endif

    config.apiBaseUrl = "https://api.makineai.com/v1";
    config.publicKeyPath = "";  // Embedded in release builds
    config.logLevel = spdlog::level::info;
    config.autoUpdateRuntime = true;
    config.enableAnalytics = false;

    return config;
}

// Singleton instance
Core& Core::instance() {
    static Core instance;
    return instance;
}

Core::~Core() {
    shutdown();
}

VoidResult Core::initialize(const CoreConfig& config) {
    std::lock_guard lock(mutex_);

    if (initialized_) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Core already initialized"));
    }

    config_ = config;

    // Create directories
    std::error_code ec;
    fs::create_directories(config_.dataDirectory, ec);
    fs::create_directories(config_.cacheDirectory, ec);
    fs::create_directories(config_.logsDirectory, ec);

    // Setup logging
    try {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (config_.logsDirectory / "makineai.log").string(), true);

        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        logger_ = std::make_shared<spdlog::logger>("makineai", sinks.begin(), sinks.end());
        logger_->set_level(config_.logLevel);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

        spdlog::set_default_logger(logger_);
    } catch (const spdlog::spdlog_ex& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            std::string("Logger initialization failed: ") + ex.what()));
    }

    logger_->info("MakineAI Core {} initializing...", version());

    // Initialize modules
    try {
        assetParser_ = std::make_unique<AssetParser>();
        patchEngine_ = std::make_unique<PatchEngine>();
        gameDetector_ = std::make_unique<GameDetector>();
        packageManager_ = std::make_unique<PackageManager>();
        runtimeManager_ = std::make_unique<RuntimeManager>();
        securityManager_ = std::make_unique<SecurityManager>();
        versionTracker_ = std::make_unique<VersionTracker>();

        // Configure modules
        patchEngine_->setBackupDirectory(config_.dataDirectory / "backups");
        packageManager_->setApiUrl(config_.apiBaseUrl);
        packageManager_->setCacheDirectory(config_.cacheDirectory / "packages");
        runtimeManager_->setBundleDirectory(config_.dataDirectory / "runtime");
        versionTracker_->setDatabasePath(config_.dataDirectory / "versions.db");

        // Load public key if specified
        if (!config_.publicKeyPath.empty() && fs::exists(config_.publicKeyPath)) {
            auto result = securityManager_->loadPublicKey(config_.publicKeyPath);
            if (!result) {
                logger_->warn("Failed to load public key: {}", result.error().message());
            }
        }

    } catch (const std::exception& ex) {
        return std::unexpected(Error(ErrorCode::Unknown,
            std::string("Module initialization failed: ") + ex.what()));
    }

    initialized_ = true;
    logger_->info("MakineAI Core initialized successfully");

    return {};
}

void Core::shutdown() {
    std::lock_guard lock(mutex_);

    if (!initialized_) return;

    logger_->info("MakineAI Core shutting down...");

    // Cleanup modules in reverse order
    versionTracker_.reset();
    securityManager_.reset();
    runtimeManager_.reset();
    packageManager_.reset();
    gameDetector_.reset();
    patchEngine_.reset();
    assetParser_.reset();

    logger_->info("MakineAI Core shutdown complete");
    logger_.reset();

    initialized_ = false;
}

AssetParser& Core::assetParser() {
    if (!assetParser_) {
        throw Exception(Error(ErrorCode::Unknown, "AssetParser not initialized"));
    }
    return *assetParser_;
}

PatchEngine& Core::patchEngine() {
    if (!patchEngine_) {
        throw Exception(Error(ErrorCode::Unknown, "PatchEngine not initialized"));
    }
    return *patchEngine_;
}

GameDetector& Core::gameDetector() {
    if (!gameDetector_) {
        throw Exception(Error(ErrorCode::Unknown, "GameDetector not initialized"));
    }
    return *gameDetector_;
}

PackageManager& Core::packageManager() {
    if (!packageManager_) {
        throw Exception(Error(ErrorCode::Unknown, "PackageManager not initialized"));
    }
    return *packageManager_;
}

RuntimeManager& Core::runtimeManager() {
    if (!runtimeManager_) {
        throw Exception(Error(ErrorCode::Unknown, "RuntimeManager not initialized"));
    }
    return *runtimeManager_;
}

SecurityManager& Core::securityManager() {
    if (!securityManager_) {
        throw Exception(Error(ErrorCode::Unknown, "SecurityManager not initialized"));
    }
    return *securityManager_;
}

VersionTracker& Core::versionTracker() {
    if (!versionTracker_) {
        throw Exception(Error(ErrorCode::Unknown, "VersionTracker not initialized"));
    }
    return *versionTracker_;
}

// Version parsing helper
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

} // namespace makineai
