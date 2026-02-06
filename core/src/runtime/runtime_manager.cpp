/**
 * @file runtime_manager.cpp
 * @brief Unity runtime translation manager implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/runtime_manager.hpp"
#include "makineai/core.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"
#include "makineai/validation.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

namespace makineai {

using json = nlohmann::json;

RuntimeManager::RuntimeManager() = default;
RuntimeManager::~RuntimeManager() = default;

bool RuntimeManager::needsRuntime(const GameInfo& game) const {
    // Unity games need runtime translation
    return game.engine == GameEngine::Unity_Mono ||
           game.engine == GameEngine::Unity_IL2CPP;
}

UnityBackend RuntimeManager::detectBackend(const fs::path& gameDir) const {
    // IL2CPP check
    if (fs::exists(gameDir / "GameAssembly.dll")) {
        return UnityBackend::IL2CPP;
    }

    // Mono check - look for _Data folder with Managed
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                auto managedPath = entry.path() / "Managed";
                if (fs::exists(managedPath / "Assembly-CSharp.dll") ||
                    fs::exists(managedPath / "UnityEngine.dll")) {
                    return UnityBackend::Mono;
                }
            }
        }
    }

    return UnityBackend::Unknown;
}

Result<RuntimeStatus> RuntimeManager::checkStatus(const GameInfo& game) const {
    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Checking runtime status for: {}",
        game.name.empty() ? game.installPath.string() : game.name);

    RuntimeStatus status;
    status.installed = false;
    status.upToDate = false;
    status.backend = detectBackend(game.installPath);
    status.installPath = game.installPath;

    if (status.backend == UnityBackend::Unknown) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Not a Unity game or backend not detected");
        return std::unexpected(Error(ErrorCode::EngineNotDetected,
            "Not a Unity game or backend not detected"));
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Detected backend: {}",
        status.backend == UnityBackend::Mono ? "Mono" : "IL2CPP");

    // Check for BepInEx installation
    fs::path bepinexPath = game.installPath / "BepInEx";
    if (!fs::exists(bepinexPath)) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "BepInEx not installed");
        return status;  // Not installed
    }

    status.installed = true;

    // Check for XUnity.AutoTranslator
    fs::path pluginsPath = bepinexPath / "plugins";
    bool hasXUnity = false;

    if (fs::exists(pluginsPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(pluginsPath)) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().filename().string();
                if (filename.find("XUnity.AutoTranslator") != std::string::npos) {
                    hasXUnity = true;
                    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Found XUnity.AutoTranslator: {}",
                        entry.path().string());
                    break;
                }
            }
        }
    }

    if (!hasXUnity) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "XUnity.AutoTranslator not found - incomplete installation");
        status.installed = false;  // Incomplete installation
        return status;
    }

    // Check for translation files
    fs::path translationPath = bepinexPath / "Translation";
    if (fs::exists(translationPath)) {
        for (const auto& entry : fs::recursive_directory_iterator(translationPath)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".txt" || ext == ".json") {
                    status.translationFiles.push_back(entry.path().string());
                }
            }
        }
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Found {} translation files",
            status.translationFiles.size());
    }

    // Check version (read from our marker file if present)
    fs::path versionFile = bepinexPath / "MakineAI_version.json";
    if (fs::exists(versionFile)) {
        try {
            std::ifstream file(versionFile);
            json versionJson = json::parse(file);

            auto parseVersion = [](const std::string& str) -> Version {
                auto result = Version::parse(str);
                return result.value_or(Version{});
            };

            status.installedVersion.bepinex = parseVersion(
                versionJson.value("bepinex", "0.0.0"));
            status.installedVersion.xunity = parseVersion(
                versionJson.value("xunity", "0.0.0"));
            status.installedVersion.makineaiPlugin = parseVersion(
                versionJson.value("makineai", "0.0.0"));

            MAKINEAI_LOG_DEBUG(log::RUNTIME, "Installed version: BepInEx {}, XUnity {}, MakineAI {}",
                status.installedVersion.bepinex.toString(),
                status.installedVersion.xunity.toString(),
                status.installedVersion.makineaiPlugin.toString());

        } catch (const std::exception& e) {
            MAKINEAI_LOG_WARN(log::RUNTIME, "Failed to parse version file: {}", e.what());
            // Version file corrupted, treat as outdated
        }
    }

    status.bundledVersion = bundledVersion();
    status.upToDate = (status.installedVersion >= status.bundledVersion);

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Runtime status: installed={}, upToDate={}",
        status.installed, status.upToDate);

    return status;
}

VoidResult RuntimeManager::install(
    const GameInfo& game,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Starting runtime installation for game: {}",
        game.name.empty() ? game.installPath.string() : game.name);

    // Validate game path before installation
    auto pathValidation = validation::validateDirectory(game.installPath);
    if (!pathValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game path validation failed: {}",
            pathValidation.error().message());
        Metrics::instance().increment("runtime_install_failures");
        return std::unexpected(pathValidation.error());
    }

    // Validate path is writable
    auto writableValidation = validation::validateWritable(game.installPath);
    if (!writableValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game directory is not writable: {}",
            game.installPath.string());
        Metrics::instance().increment("runtime_install_failures");
        return std::unexpected(writableValidation.error());
    }

    auto backend = detectBackend(game.installPath);
    if (backend == UnityBackend::Unknown) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Cannot detect Unity backend for: {}",
            game.installPath.string());
        Metrics::instance().increment("runtime_install_failures");
        return std::unexpected(Error(ErrorCode::EngineNotDetected,
            "Cannot detect Unity backend"));
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Detected Unity backend: {}",
        backend == UnityBackend::Mono ? "Mono" : "IL2CPP");

    // Start timing the installation
    auto timer = Metrics::instance().timer("runtime_install");

    VoidResult result;
    if (backend == UnityBackend::Mono) {
        result = installMono(game, progress);
    } else {
        result = installIL2CPP(game, progress);
    }

    if (result) {
        Metrics::instance().increment("runtime_installs");
        MAKINEAI_LOG_INFO(log::RUNTIME, "Runtime installation completed successfully");

        // Audit log the installation
        AuditLogger::logPatchOperation(
            game.name.empty() ? game.installPath.string() : game.name,
            true,
            "runtime_install",
            backend == UnityBackend::Mono ? "BepInEx Mono" : "BepInEx IL2CPP"
        );
    } else {
        Metrics::instance().increment("runtime_install_failures");
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Runtime installation failed: {}",
            result.error().message());

        AuditLogger::logPatchOperation(
            game.name.empty() ? game.installPath.string() : game.name,
            false,
            "runtime_install",
            result.error().message()
        );
    }

    return result;
}

VoidResult RuntimeManager::installMono(
    const GameInfo& game,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Installing BepInEx for Unity Mono: {}",
        game.installPath.string());

    if (progress) {
        progress(0, 4, "Installing BepInEx for Unity Mono...");
    }

    fs::path bundlePath = getMonoBundlePath();

    // Validate runtime bundle exists and is a directory
    auto bundleValidation = validation::validateDirectory(bundlePath);
    if (!bundleValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Mono runtime bundle not found at: {}",
            bundlePath.string());
        return std::unexpected(Error(ErrorCode::RuntimeNotFound,
            "Mono runtime bundle not found"));
    }

    std::error_code ec;

    // Copy BepInEx files
    if (progress) {
        progress(1, 4, "Copying BepInEx files...");
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Copying BepInEx folder from bundle");

    // Copy main BepInEx folder
    fs::path srcBepInEx = bundlePath / "BepInEx";
    fs::path dstBepInEx = game.installPath / "BepInEx";

    fs::create_directories(dstBepInEx, ec);
    fs::copy(srcBepInEx, dstBepInEx,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to copy BepInEx: {}", ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to copy BepInEx: " + ec.message()));
    }

    // Audit log the BepInEx folder creation
    AuditLogger::logFileAccess(dstBepInEx, "create_directory", true,
        "BepInEx runtime installation");

    // Copy doorstop files
    if (progress) {
        progress(2, 4, "Copying doorstop loader...");
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Copying doorstop loader files");

    for (const auto& entry : fs::directory_iterator(bundlePath)) {
        auto filename = entry.path().filename().string();
        if (filename == "winhttp.dll" ||
            filename == "doorstop_config.ini" ||
            filename == ".doorstop_version") {
            fs::path destFile = game.installPath / filename;
            fs::copy_file(entry.path(), destFile,
                fs::copy_options::overwrite_existing, ec);

            if (!ec) {
                AuditLogger::logFileAccess(destFile, "write", true,
                    "Doorstop loader file");
            }
        }
    }

    // Configure XUnity
    if (progress) {
        progress(3, 4, "Configuring MakineAI Translation System...");
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Configuring XUnity.AutoTranslator");

    auto configResult = configureXUnity(game);
    if (!configResult) {
        MAKINEAI_LOG_WARN(log::RUNTIME, "XUnity configuration failed: {}",
            configResult.error().message());
    }

    // Write version marker
    fs::path versionFile = dstBepInEx / "MakineAI_version.json";
    json versionJson;
    auto bundled = bundledVersion();
    versionJson["bepinex"] = bundled.bepinex.toString();
    versionJson["xunity"] = bundled.xunity.toString();
    versionJson["makineai"] = bundled.makineaiPlugin.toString();
    versionJson["installedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::ofstream versionStream(versionFile);
    versionStream << versionJson.dump(2);

    AuditLogger::logFileAccess(versionFile, "write", true,
        "MakineAI version marker");

    if (progress) {
        progress(4, 4, "Installation complete");
    }

    MAKINEAI_LOG_INFO(log::RUNTIME, "Installed MakineAI Translation System (Mono) to {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::installIL2CPP(
    const GameInfo& game,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Installing BepInEx for Unity IL2CPP: {}",
        game.installPath.string());

    if (progress) {
        progress(0, 4, "Installing BepInEx for Unity IL2CPP...");
    }

    fs::path bundlePath = getIL2CPPBundlePath();

    // Validate runtime bundle exists and is a directory
    auto bundleValidation = validation::validateDirectory(bundlePath);
    if (!bundleValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "IL2CPP runtime bundle not found at: {}",
            bundlePath.string());
        return std::unexpected(Error(ErrorCode::RuntimeNotFound,
            "IL2CPP runtime bundle not found"));
    }

    // Similar to Mono installation but with IL2CPP-specific files
    std::error_code ec;

    if (progress) {
        progress(1, 4, "Copying BepInEx IL2CPP files...");
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Copying BepInEx IL2CPP folder from bundle");

    fs::path srcBepInEx = bundlePath / "BepInEx";
    fs::path dstBepInEx = game.installPath / "BepInEx";

    fs::create_directories(dstBepInEx, ec);
    fs::copy(srcBepInEx, dstBepInEx,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to copy BepInEx: {}", ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to copy BepInEx: " + ec.message()));
    }

    // Audit log the BepInEx folder creation
    AuditLogger::logFileAccess(dstBepInEx, "create_directory", true,
        "BepInEx IL2CPP runtime installation");

    // Copy .NET runtime for IL2CPP (if bundled)
    fs::path dotnetSrc = bundlePath / "dotnet";
    if (fs::exists(dotnetSrc)) {
        if (progress) {
            progress(2, 4, "Copying .NET runtime...");
        }
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Copying .NET runtime for IL2CPP");

        fs::path dotnetDst = game.installPath / "dotnet";
        fs::copy(dotnetSrc, dotnetDst,
            fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

        if (!ec) {
            AuditLogger::logFileAccess(dotnetDst, "create_directory", true,
                ".NET runtime for IL2CPP");
        }
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Copying doorstop loader files for IL2CPP");

    // Copy doorstop
    for (const auto& entry : fs::directory_iterator(bundlePath)) {
        auto filename = entry.path().filename().string();
        if (filename == "winhttp.dll" ||
            filename == "dobby.dll" ||
            filename == "doorstop_config.ini") {
            fs::path destFile = game.installPath / filename;
            fs::copy_file(entry.path(), destFile,
                fs::copy_options::overwrite_existing, ec);

            if (!ec) {
                AuditLogger::logFileAccess(destFile, "write", true,
                    "IL2CPP doorstop loader file");
            }
        }
    }

    // Configure
    if (progress) {
        progress(3, 4, "Configuring MakineAI Translation System...");
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Configuring XUnity.AutoTranslator for IL2CPP");

    auto configResult = configureXUnity(game);
    if (!configResult) {
        MAKINEAI_LOG_WARN(log::RUNTIME, "XUnity configuration failed: {}",
            configResult.error().message());
    }

    // Write version marker
    fs::path versionFile = dstBepInEx / "MakineAI_version.json";
    json versionJson;
    auto bundled = bundledVersion();
    versionJson["bepinex"] = bundled.bepinex.toString();
    versionJson["xunity"] = bundled.xunity.toString();
    versionJson["makineai"] = bundled.makineaiPlugin.toString();
    versionJson["installedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::ofstream versionStream(versionFile);
    versionStream << versionJson.dump(2);

    AuditLogger::logFileAccess(versionFile, "write", true,
        "MakineAI version marker (IL2CPP)");

    if (progress) {
        progress(4, 4, "Installation complete");
    }

    MAKINEAI_LOG_INFO(log::RUNTIME, "Installed MakineAI Translation System (IL2CPP) to {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::update(
    const GameInfo& game,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Updating runtime for game: {}",
        game.name.empty() ? game.installPath.string() : game.name);

    // Update is same as install - will overwrite existing files
    auto result = install(game, progress);

    if (result) {
        MAKINEAI_LOG_INFO(log::RUNTIME, "Runtime update completed successfully");

        AuditLogger::logPatchOperation(
            game.name.empty() ? game.installPath.string() : game.name,
            true,
            "runtime_update",
            "Updated to latest version"
        );
    } else {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Runtime update failed: {}",
            result.error().message());
    }

    return result;
}

// Safe recursive delete that verifies paths are within game directory
static VoidResult safeRemoveAll(const fs::path& targetPath, const fs::path& gameDir) {
    std::error_code ec;

    if (!fs::exists(targetPath, ec)) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Path does not exist, nothing to delete: {}",
            targetPath.string());
        return {}; // Nothing to delete
    }

    // SECURITY: Path traversal check
    if (targetPath.string().find("..") != std::string::npos) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "SECURITY: Path traversal detected in: {}",
            targetPath.string());
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Path traversal detected in target path"));
    }

    // SECURITY: Verify target is inside game directory using canonical paths
    auto canonicalTarget = fs::weakly_canonical(targetPath, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Cannot resolve canonical path: {} - {}",
            targetPath.string(), ec.message());
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Cannot resolve canonical path: " + ec.message()));
    }

    auto canonicalGame = fs::weakly_canonical(gameDir, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Cannot resolve game directory path: {} - {}",
            gameDir.string(), ec.message());
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Cannot resolve game directory path: " + ec.message()));
    }

    // Check that target starts with game directory
    auto targetStr = canonicalTarget.string();
    auto gameStr = canonicalGame.string();
    if (targetStr.length() <= gameStr.length() ||
        targetStr.substr(0, gameStr.length()) != gameStr) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "SECURITY: Target path {} is outside game directory {}",
            targetStr, gameStr);
        return std::unexpected(Error(ErrorCode::SecurityViolation,
            "Target path is outside game directory"));
    }

    // SECURITY: Check for symlinks pointing outside
    if (fs::is_symlink(targetPath, ec)) {
        auto linkTarget = fs::read_symlink(targetPath, ec);
        if (!ec) {
            auto resolvedLink = fs::weakly_canonical(targetPath.parent_path() / linkTarget, ec);
            auto resolvedStr = resolvedLink.string();
            if (resolvedStr.length() <= gameStr.length() ||
                resolvedStr.substr(0, gameStr.length()) != gameStr) {
                MAKINEAI_LOG_WARN(log::RUNTIME, "SECURITY: Skipping symlink pointing outside game directory: {}",
                    targetPath.string());
                return {};
            }
        }
    }

    // Safe to delete
    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Safely removing: {}", targetPath.string());
    fs::remove_all(targetPath, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to remove {}: {}", targetPath.string(), ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to remove: " + ec.message()));
    }

    return {};
}

VoidResult RuntimeManager::uninstall(const GameInfo& game) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Starting runtime uninstallation for game: {}",
        game.name.empty() ? game.installPath.string() : game.name);

    // Validate game path exists
    auto pathValidation = validation::validateDirectory(game.installPath);
    if (!pathValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game path validation failed: {}",
            pathValidation.error().message());
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game install path not found"));
    }

    std::error_code ec;
    std::vector<std::string> errors;

    // Remove BepInEx folder
    fs::path bepinexPath = game.installPath / "BepInEx";
    if (fs::exists(bepinexPath, ec)) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Removing BepInEx folder: {}", bepinexPath.string());
        auto result = safeRemoveAll(bepinexPath, game.installPath);
        if (!result) {
            errors.push_back("BepInEx: " + result.error().message());
            MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to remove BepInEx: {}",
                result.error().message());
        } else {
            AuditLogger::logFileAccess(bepinexPath, "delete", true,
                "BepInEx folder removal during uninstall");
        }
    }

    // Remove doorstop files
    const char* doorstopFiles[] = {
        "winhttp.dll",
        "doorstop_config.ini",
        ".doorstop_version",
        "dobby.dll"
    };

    for (const auto& filename : doorstopFiles) {
        fs::path filePath = game.installPath / filename;
        if (fs::exists(filePath, ec)) {
            MAKINEAI_LOG_DEBUG(log::RUNTIME, "Removing doorstop file: {}", filename);
            fs::remove(filePath, ec);
            if (ec) {
                errors.push_back(std::string(filename) + ": " + ec.message());
                MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to remove {}: {}",
                    filename, ec.message());
            } else {
                AuditLogger::logFileAccess(filePath, "delete", true,
                    "Doorstop file removal during uninstall");
            }
        }
    }

    // Remove .NET runtime folder (if present)
    fs::path dotnetPath = game.installPath / "dotnet";
    if (fs::exists(dotnetPath, ec)) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Removing .NET runtime folder: {}",
            dotnetPath.string());
        auto result = safeRemoveAll(dotnetPath, game.installPath);
        if (!result) {
            errors.push_back("dotnet: " + result.error().message());
            MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to remove .NET runtime: {}",
                result.error().message());
        } else {
            AuditLogger::logFileAccess(dotnetPath, "delete", true,
                ".NET runtime folder removal during uninstall");
        }
    }

    if (!errors.empty()) {
        MAKINEAI_LOG_WARN(log::RUNTIME, "Uninstall completed with {} errors", errors.size());
        for (const auto& err : errors) {
            MAKINEAI_LOG_WARN(log::RUNTIME, "  - {}", err);
        }

        // Audit log partial failure
        AuditLogger::logPatchOperation(
            game.name.empty() ? game.installPath.string() : game.name,
            false,
            "runtime_uninstall",
            "Completed with " + std::to_string(errors.size()) + " errors"
        );
    } else {
        // Audit log successful uninstall
        AuditLogger::logPatchOperation(
            game.name.empty() ? game.installPath.string() : game.name,
            true,
            "runtime_uninstall",
            "Complete removal"
        );
    }

    MAKINEAI_LOG_INFO(log::RUNTIME, "Uninstalled MakineAI Translation System from {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::addTranslations(
    const GameInfo& game,
    const fs::path& translationDir
) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Adding translations from: {} to game: {}",
        translationDir.string(),
        game.name.empty() ? game.installPath.string() : game.name);

    // Validate translation directory
    auto dirValidation = validation::validateDirectory(translationDir);
    if (!dirValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Translation directory validation failed: {}",
            dirValidation.error().message());
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Translation directory not found"));
    }

    // Validate game path
    auto gamePathValidation = validation::validateDirectory(game.installPath);
    if (!gamePathValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game path validation failed: {}",
            gamePathValidation.error().message());
        return std::unexpected(gamePathValidation.error());
    }

    fs::path targetDir = game.installPath / "BepInEx" / "Translation" / "tr" / "Text";
    std::error_code ec;
    fs::create_directories(targetDir, ec);

    if (ec) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to create translation directory: {}",
            ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to create translation directory: " + ec.message()));
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Created translation target directory: {}",
        targetDir.string());

    int filesCopied = 0;

    // Copy translation files
    for (const auto& entry : fs::recursive_directory_iterator(translationDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext != ".txt" && ext != ".json") continue;

        auto relPath = fs::relative(entry.path(), translationDir);
        fs::path targetPath = targetDir / relPath;

        fs::create_directories(targetPath.parent_path(), ec);
        fs::copy_file(entry.path(), targetPath,
            fs::copy_options::overwrite_existing, ec);

        if (!ec) {
            ++filesCopied;
            AuditLogger::logFileAccess(targetPath, "write", true,
                "Translation file copy");
        } else {
            MAKINEAI_LOG_WARN(log::RUNTIME, "Failed to copy translation file {}: {}",
                entry.path().string(), ec.message());
        }
    }

    MAKINEAI_LOG_INFO(log::RUNTIME, "Added {} translation files to {}",
        filesCopied, targetDir.string());

    // Record metrics
    Metrics::instance().increment("translation_files_added", filesCopied);

    return {};
}

VoidResult RuntimeManager::removeTranslations(const GameInfo& game) {
    MAKINEAI_LOG_INFO(log::RUNTIME, "Removing translations from game: {}",
        game.name.empty() ? game.installPath.string() : game.name);

    // Validate game path
    auto pathValidation = validation::validateDirectory(game.installPath);
    if (!pathValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game path validation failed: {}",
            pathValidation.error().message());
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game install path not found"));
    }

    fs::path translationDir = game.installPath / "BepInEx" / "Translation";
    std::error_code ec;
    if (fs::exists(translationDir, ec)) {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Removing translation directory: {}",
            translationDir.string());

        auto result = safeRemoveAll(translationDir, game.installPath);
        if (!result) {
            MAKINEAI_LOG_ERROR(log::RUNTIME, "Failed to remove translations: {}",
                result.error().message());
            return result;
        }

        AuditLogger::logFileAccess(translationDir, "delete", true,
            "Translation directory removal");

        MAKINEAI_LOG_INFO(log::RUNTIME, "Translations removed successfully");
    } else {
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "No translation directory found to remove");
    }

    return {};
}

RuntimeVersion RuntimeManager::bundledVersion() const {
    RuntimeVersion version;
    version.bepinex = {5, 4, 23};       // BepInEx 5.4.23
    version.xunity = {5, 3, 0};         // XUnity.AutoTranslator 5.3.0
    version.makineaiPlugin = {1, 0, 0}; // MakineAI Plugin 1.0.0
    return version;
}

VoidResult RuntimeManager::configureXUnity(
    const GameInfo& game,
    const XUnityConfig& config
) {
    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Configuring XUnity.AutoTranslator for: {}",
        game.installPath.string());

    fs::path configPath = game.installPath / "BepInEx" / "config" /
        "AutoTranslatorConfig.ini";

    std::error_code ec;
    fs::create_directories(configPath.parent_path(), ec);

    std::ofstream configFile(configPath);
    if (!configFile) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Cannot write XUnity config to: {}",
            configPath.string());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot write XUnity config"));
    }

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Writing XUnity configuration: endpoint={}, IMGUI={}, UGUI={}, NGUI={}, TMP={}",
        config.endpoint,
        config.enableIMGUIHooking ? "true" : "false",
        config.enableUGUIHooking ? "true" : "false",
        config.enableNGUIHooking ? "true" : "false",
        config.enableTextMeshProHooking ? "true" : "false");

    configFile << "[Service]\n";
    configFile << "Endpoint=" << config.endpoint << "\n";
    configFile << "\n";
    configFile << "[General]\n";
    configFile << "Language=tr\n";
    configFile << "FromLanguage=en\n";
    configFile << "\n";
    configFile << "[Behaviour]\n";
    configFile << "MaxCharactersPerTranslation=200\n";
    configFile << "EnableIMGUIHooking=" << (config.enableIMGUIHooking ? "True" : "False") << "\n";
    configFile << "EnableUGUIHooking=" << (config.enableUGUIHooking ? "True" : "False") << "\n";
    configFile << "EnableNGUIHooking=" << (config.enableNGUIHooking ? "True" : "False") << "\n";
    configFile << "EnableTextMeshProHooking=" << (config.enableTextMeshProHooking ? "True" : "False") << "\n";
    configFile << "\n";
    configFile << "[Files]\n";
    configFile << "Directory=" << config.translationsDirectory << "\n";
    configFile << "OutputFile=" << config.outputFile << "\n";
    configFile << "\n";
    configFile << "[TextFrameworks]\n";
    configFile << "EnableTextMeshPro=True\n";
    configFile << "EnableUGUI=True\n";
    configFile << "\n";
    configFile << "; MakineAI Translation System\n";
    configFile << "; https://makineai.com\n";

    // Audit log the configuration file write
    AuditLogger::logFileAccess(configPath, "write", true,
        "XUnity.AutoTranslator configuration");

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "XUnity configuration written successfully");

    return {};
}

fs::path RuntimeManager::getMonoBundlePath() const {
    return bundleDir_ / "bepinex_mono";
}

fs::path RuntimeManager::getIL2CPPBundlePath() const {
    return bundleDir_ / "bepinex_il2cpp";
}

// Unity analysis helper
Result<UnityAnalysis> analyzeUnityGame(const fs::path& gameDir) {
    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Analyzing Unity game at: {}", gameDir.string());

    UnityAnalysis analysis;
    analysis.isUnity = false;
    analysis.backend = UnityBackend::Unknown;

    // Validate game directory
    auto dirValidation = validation::validateDirectory(gameDir);
    if (!dirValidation) {
        MAKINEAI_LOG_ERROR(log::RUNTIME, "Game directory validation failed: {}",
            dirValidation.error().message());
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game directory not found"));
    }

    // Check for IL2CPP
    if (fs::exists(gameDir / "GameAssembly.dll")) {
        analysis.isUnity = true;
        analysis.backend = UnityBackend::IL2CPP;
        MAKINEAI_LOG_DEBUG(log::RUNTIME, "Detected IL2CPP backend (GameAssembly.dll found)");
    }

    // Find _Data folder
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                analysis.dataDirectory = entry.path();

                auto managedPath = entry.path() / "Managed";
                if (fs::exists(managedPath)) {
                    analysis.managedDirectory = managedPath;

                    if (fs::exists(managedPath / "UnityEngine.dll")) {
                        analysis.isUnity = true;
                        if (analysis.backend == UnityBackend::Unknown) {
                            analysis.backend = UnityBackend::Mono;
                        }
                    }
                }

                // Try to read Unity version from globalgamemanagers
                fs::path ggm = entry.path() / "globalgamemanagers";
                if (fs::exists(ggm)) {
                    std::ifstream file(ggm, std::ios::binary);
                    // Unity version is usually near the start
                    char buffer[256] = {0};
                    file.read(buffer, 255);
                    // Look for version pattern like "2019.4.1f1" or "5.6.3p1"
                    for (int i = 0; i < 253; ++i) {
                        if (std::isdigit(buffer[i]) && buffer[i+1] == '.' &&
                            std::isdigit(buffer[i+2])) {
                            analysis.unityVersion = "";
                            for (int j = i; j < 255 && (std::isalnum(buffer[j]) ||
                                buffer[j] == '.' || buffer[j] == 'f' ||
                                buffer[j] == 'p'); ++j) {
                                analysis.unityVersion += buffer[j];
                            }
                            break;
                        }
                    }
                }

                break;
            }
        }
    }

    // Check architecture from exe
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".exe") {
                // Quick PE header check
                std::ifstream file(entry.path(), std::ios::binary);
                if (file) {
                    char dosHeader[64];
                    file.read(dosHeader, 64);
                    if (dosHeader[0] == 'M' && dosHeader[1] == 'Z') {
                        uint32_t peOffset;
                        std::memcpy(&peOffset, &dosHeader[60], 4);
                        if (peOffset > 0x10000) break;  // Sanity check
                        file.seekg(peOffset + 4);  // Skip PE signature
                        uint16_t machine;
                        file.read(reinterpret_cast<char*>(&machine), 2);
                        analysis.is64Bit = (machine == 0x8664);
                    }
                }
                break;
            }
        }
    }

    // Check for existing BepInEx
    analysis.hasExistingBepInEx = fs::exists(gameDir / "BepInEx");

    MAKINEAI_LOG_DEBUG(log::RUNTIME, "Unity analysis complete: isUnity={}, backend={}, version={}, hasExistingBepInEx={}",
        analysis.isUnity,
        analysis.backend == UnityBackend::Mono ? "Mono" :
            (analysis.backend == UnityBackend::IL2CPP ? "IL2CPP" : "Unknown"),
        analysis.unityVersion.empty() ? "unknown" : analysis.unityVersion,
        analysis.hasExistingBepInEx);

    return analysis;
}

} // namespace makineai
