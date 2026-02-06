/**
 * @file runtime_manager.cpp
 * @brief Unity runtime translation manager implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/runtime_manager.hpp"
#include "makineai/core.hpp"

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
    RuntimeStatus status;
    status.installed = false;
    status.upToDate = false;
    status.backend = detectBackend(game.installPath);
    status.installPath = game.installPath;

    if (status.backend == UnityBackend::Unknown) {
        return std::unexpected(Error(ErrorCode::EngineNotDetected,
            "Not a Unity game or backend not detected"));
    }

    // Check for BepInEx installation
    fs::path bepinexPath = game.installPath / "BepInEx";
    if (!fs::exists(bepinexPath)) {
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
                    break;
                }
            }
        }
    }

    if (!hasXUnity) {
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

        } catch (...) {
            // Version file corrupted, treat as outdated
        }
    }

    status.bundledVersion = bundledVersion();
    status.upToDate = (status.installedVersion >= status.bundledVersion);

    return status;
}

VoidResult RuntimeManager::install(
    const GameInfo& game,
    ProgressCallback progress
) {
    auto backend = detectBackend(game.installPath);
    if (backend == UnityBackend::Unknown) {
        return std::unexpected(Error(ErrorCode::EngineNotDetected,
            "Cannot detect Unity backend"));
    }

    if (backend == UnityBackend::Mono) {
        return installMono(game, progress);
    } else {
        return installIL2CPP(game, progress);
    }
}

VoidResult RuntimeManager::installMono(
    const GameInfo& game,
    ProgressCallback progress
) {
    if (progress) {
        progress(0, 4, "Installing BepInEx for Unity Mono...");
    }

    fs::path bundlePath = getMonoBundlePath();
    if (!fs::exists(bundlePath)) {
        return std::unexpected(Error(ErrorCode::RuntimeNotFound,
            "Mono runtime bundle not found"));
    }

    std::error_code ec;

    // Copy BepInEx files
    if (progress) {
        progress(1, 4, "Copying BepInEx files...");
    }

    // Copy main BepInEx folder
    fs::path srcBepInEx = bundlePath / "BepInEx";
    fs::path dstBepInEx = game.installPath / "BepInEx";

    fs::create_directories(dstBepInEx, ec);
    fs::copy(srcBepInEx, dstBepInEx,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to copy BepInEx: " + ec.message()));
    }

    // Copy doorstop files
    if (progress) {
        progress(2, 4, "Copying doorstop loader...");
    }

    for (const auto& entry : fs::directory_iterator(bundlePath)) {
        auto filename = entry.path().filename().string();
        if (filename == "winhttp.dll" ||
            filename == "doorstop_config.ini" ||
            filename == ".doorstop_version") {
            fs::copy_file(entry.path(), game.installPath / filename,
                fs::copy_options::overwrite_existing, ec);
        }
    }

    // Configure XUnity
    if (progress) {
        progress(3, 4, "Configuring MakineAI Translation System...");
    }

    auto configResult = configureXUnity(game);
    if (!configResult) {
        logger()->warn("XUnity configuration failed: {}",
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

    if (progress) {
        progress(4, 4, "Installation complete");
    }

    logger()->info("Installed MakineAI Translation System (Mono) to {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::installIL2CPP(
    const GameInfo& game,
    ProgressCallback progress
) {
    if (progress) {
        progress(0, 4, "Installing BepInEx for Unity IL2CPP...");
    }

    fs::path bundlePath = getIL2CPPBundlePath();
    if (!fs::exists(bundlePath)) {
        return std::unexpected(Error(ErrorCode::RuntimeNotFound,
            "IL2CPP runtime bundle not found"));
    }

    // Similar to Mono installation but with IL2CPP-specific files
    std::error_code ec;

    if (progress) {
        progress(1, 4, "Copying BepInEx IL2CPP files...");
    }

    fs::path srcBepInEx = bundlePath / "BepInEx";
    fs::path dstBepInEx = game.installPath / "BepInEx";

    fs::create_directories(dstBepInEx, ec);
    fs::copy(srcBepInEx, dstBepInEx,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

    if (ec) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to copy BepInEx: " + ec.message()));
    }

    // Copy .NET runtime for IL2CPP (if bundled)
    fs::path dotnetSrc = bundlePath / "dotnet";
    if (fs::exists(dotnetSrc)) {
        if (progress) {
            progress(2, 4, "Copying .NET runtime...");
        }
        fs::copy(dotnetSrc, game.installPath / "dotnet",
            fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    }

    // Copy doorstop
    for (const auto& entry : fs::directory_iterator(bundlePath)) {
        auto filename = entry.path().filename().string();
        if (filename == "winhttp.dll" ||
            filename == "dobby.dll" ||
            filename == "doorstop_config.ini") {
            fs::copy_file(entry.path(), game.installPath / filename,
                fs::copy_options::overwrite_existing, ec);
        }
    }

    // Configure
    if (progress) {
        progress(3, 4, "Configuring MakineAI Translation System...");
    }

    auto configResult = configureXUnity(game);
    if (!configResult) {
        logger()->warn("XUnity configuration failed: {}",
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

    if (progress) {
        progress(4, 4, "Installation complete");
    }

    logger()->info("Installed MakineAI Translation System (IL2CPP) to {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::update(
    const GameInfo& game,
    ProgressCallback progress
) {
    // Update is same as install - will overwrite existing files
    return install(game, progress);
}

VoidResult RuntimeManager::uninstall(const GameInfo& game) {
    std::error_code ec;

    // Remove BepInEx folder
    fs::path bepinexPath = game.installPath / "BepInEx";
    if (fs::exists(bepinexPath)) {
        fs::remove_all(bepinexPath, ec);
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
        if (fs::exists(filePath)) {
            fs::remove(filePath, ec);
        }
    }

    // Remove .NET runtime folder (if present)
    fs::path dotnetPath = game.installPath / "dotnet";
    if (fs::exists(dotnetPath)) {
        fs::remove_all(dotnetPath, ec);
    }

    logger()->info("Uninstalled MakineAI Translation System from {}",
        game.installPath.string());

    return {};
}

VoidResult RuntimeManager::addTranslations(
    const GameInfo& game,
    const fs::path& translationDir
) {
    if (!fs::exists(translationDir)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Translation directory not found"));
    }

    fs::path targetDir = game.installPath / "BepInEx" / "Translation" / "tr" / "Text";
    std::error_code ec;
    fs::create_directories(targetDir, ec);

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
    }

    logger()->info("Added translation files to {}", targetDir.string());
    return {};
}

VoidResult RuntimeManager::removeTranslations(const GameInfo& game) {
    fs::path translationDir = game.installPath / "BepInEx" / "Translation";
    if (fs::exists(translationDir)) {
        std::error_code ec;
        fs::remove_all(translationDir, ec);
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
    fs::path configPath = game.installPath / "BepInEx" / "config" /
        "AutoTranslatorConfig.ini";

    std::error_code ec;
    fs::create_directories(configPath.parent_path(), ec);

    std::ofstream configFile(configPath);
    if (!configFile) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot write XUnity config"));
    }

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
    UnityAnalysis analysis;
    analysis.isUnity = false;
    analysis.backend = UnityBackend::Unknown;

    if (!fs::exists(gameDir)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game directory not found"));
    }

    // Check for IL2CPP
    if (fs::exists(gameDir / "GameAssembly.dll")) {
        analysis.isUnity = true;
        analysis.backend = UnityBackend::IL2CPP;
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
                    for (int i = 0; i < 200; ++i) {
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

    return analysis;
}

} // namespace makineai
