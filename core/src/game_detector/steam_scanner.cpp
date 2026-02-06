/**
 * @file steam_scanner.cpp
 * @brief Steam game scanner implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/game_detector.hpp"
#include "makineai/core.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <fstream>
#include <regex>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace makineai::scanners {

bool SteamScanner::isAvailable() const {
#ifdef _WIN32
    // Check registry for Steam installation
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found via HKCU registry");
        return true;
    }
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Software\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found via HKLM registry");
        return true;
    }
    MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Not found in registry");
#endif
    return false;
}

Result<std::vector<GameInfo>> SteamScanner::scan() const {
    MAKINEAI_TIMED_SCOPE(log::SCANNER, "SteamScanner::scan");
    MAKINEAI_LOG_INFO(log::SCANNER, "Starting Steam game scan");

    auto scanTimer = metrics().timer("steam_scan");
    std::vector<GameInfo> games;

    auto libraryResult = findLibraryFolders();
    if (!libraryResult) {
        MAKINEAI_LOG_WARN(log::SCANNER, "Steam: Failed to find library folders: {}",
                         libraryResult.error().message());
        return std::unexpected(libraryResult.error());
    }

    MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found {} library folders", libraryResult->size());

    for (const auto& libraryPath : *libraryResult) {
        fs::path steamapps = fs::path(libraryPath) / "steamapps";
        if (!fs::exists(steamapps)) {
            MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Steamapps not found at {}", libraryPath);
            continue;
        }

        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Scanning library at {}", libraryPath);

        // Scan for .acf manifest files
        try {
            for (const auto& entry : fs::directory_iterator(steamapps)) {
                if (!entry.is_regular_file()) continue;

                auto filename = entry.path().filename().string();
                if (!filename.starts_with("appmanifest_") ||
                    !filename.ends_with(".acf")) {
                    continue;
                }

                auto gameResult = parseAppManifest(entry.path());
                if (gameResult) {
                    MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found game '{}' (AppID: {})",
                                      gameResult->name, gameResult->id.storeId);
                    games.push_back(std::move(*gameResult));
                    metrics().increment("steam_games_found");
                }
            }
        } catch (const std::exception& e) {
            MAKINEAI_LOG_WARN(log::SCANNER, "Steam: Access error scanning {}: {}",
                             steamapps.string(), e.what());
        }
    }

    MAKINEAI_LOG_INFO(log::SCANNER, "Steam scan complete: {} games found", games.size());
    metrics().gauge("steam_total_games", static_cast<double>(games.size()));

    return games;
}

Result<GameInfo> SteamScanner::getGame(const std::string& appId) const {
    auto libraryResult = findLibraryFolders();
    if (!libraryResult) {
        return std::unexpected(libraryResult.error());
    }

    for (const auto& libraryPath : *libraryResult) {
        fs::path manifestPath = fs::path(libraryPath) / "steamapps" /
            ("appmanifest_" + appId + ".acf");

        if (fs::exists(manifestPath)) {
            return parseAppManifest(manifestPath);
        }
    }

    return std::unexpected(Error(ErrorCode::GameNotFound,
        "Steam game not found: " + appId));
}

Result<StringList> SteamScanner::findLibraryFolders() const {
    MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Searching for library folders");
    StringList folders;

#ifdef _WIN32
    // Get Steam install path from registry
    HKEY hKey;
    wchar_t steamPath[MAX_PATH] = {0};
    DWORD pathSize = sizeof(steamPath);

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(steamPath), &pathSize) == ERROR_SUCCESS) {

            // Convert to narrow string
            char narrowPath[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, steamPath, -1,
                narrowPath, MAX_PATH, nullptr, nullptr);
            folders.push_back(narrowPath);
            MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found install path via registry: {}", narrowPath);
        }
        RegCloseKey(hKey);
    }

    if (folders.empty()) {
        // Try default paths
        const char* defaultPaths[] = {
            "C:\\Program Files (x86)\\Steam",
            "C:\\Program Files\\Steam",
            "D:\\Steam",
            "D:\\SteamLibrary"
        };

        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Registry path not found, trying default paths");
        for (const auto& path : defaultPaths) {
            if (fs::exists(path)) {
                folders.push_back(path);
                MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found at default path: {}", path);
                break;
            }
        }
    }
#endif

    if (folders.empty()) {
        MAKINEAI_LOG_WARN(log::SCANNER, "Steam: Installation not found");
        return std::unexpected(Error(ErrorCode::GameNotFound,
            "Steam installation not found"));
    }

    // Parse libraryfolders.vdf for additional library locations
    fs::path vdfPath = fs::path(folders[0]) / "steamapps" / "libraryfolders.vdf";
    if (fs::exists(vdfPath)) {
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Parsing libraryfolders.vdf");
        std::ifstream vdf(vdfPath);
        std::string line;

        // Simple VDF parsing for "path" entries
        std::regex pathRegex("\"path\"\\s+\"([^\"]+)\"");
        std::smatch match;

        while (std::getline(vdf, line)) {
            if (std::regex_search(line, match, pathRegex)) {
                std::string path = match[1].str();
                // Unescape backslashes
                std::string unescaped;
                for (size_t i = 0; i < path.size(); ++i) {
                    if (path[i] == '\\' && i + 1 < path.size() && path[i+1] == '\\') {
                        unescaped += '\\';
                        ++i;
                    } else {
                        unescaped += path[i];
                    }
                }

                // Add if not duplicate
                if (std::find(folders.begin(), folders.end(), unescaped) == folders.end()) {
                    if (fs::exists(unescaped)) {
                        folders.push_back(unescaped);
                        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found additional library: {}", unescaped);
                    }
                }
            }
        }
    } else {
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: libraryfolders.vdf not found at {}", vdfPath.string());
    }

    MAKINEAI_LOG_DEBUG(log::SCANNER, "Steam: Found {} library folders total", folders.size());
    return folders;
}

// Check if an app is a redistributable package (VC++, DirectX, etc.)
static bool isRedistributable(const std::string& appId, const std::string& name) {
    // Known redistributable AppIDs
    static const std::vector<std::string> redistributableIds = {
        "228980",  // Steamworks Common Redistributables
        "228981",  // SteamVR
        "250820",  // SteamVR Performance Test
        "1007",    // Steam Client
        "1070560", // Steam Linux Runtime
        "1391110", // Steam Linux Runtime - Soldier
        "1628350", // Steam Linux Runtime - Sniper
    };

    for (const auto& id : redistributableIds) {
        if (appId == id) return true;
    }

    // Check name patterns
    static const std::vector<std::string> redistributablePatterns = {
        "redistributable",
        "redist",
        "directx",
        "vcredist",
        "visual c++",
        "microsoft visual",
        ".net framework",
        "openal",
        "physx",
        "easyanticheat",
        "battleye",
        "denuvo",
    };

    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    for (const auto& pattern : redistributablePatterns) {
        if (lowerName.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

Result<GameInfo> SteamScanner::parseAppManifest(const fs::path& acfFile) const {
    std::ifstream file(acfFile);
    if (!file) {
        MAKINEAI_LOG_WARN(log::SCANNER, "Steam: Cannot open manifest: {}", acfFile.string());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open manifest: " + acfFile.string()));
    }

    GameInfo game;
    game.id.store = GameStore::Steam;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Simple VDF parsing
    auto getValue = [&content](const std::string& key) -> std::string {
        std::regex regex("\"" + key + "\"\\s+\"([^\"]+)\"", std::regex::icase);
        std::smatch match;
        if (std::regex_search(content, match, regex)) {
            return match[1].str();
        }
        return "";
    };

    game.id.storeId = getValue("appid");
    game.name = getValue("name");
    std::string installDir = getValue("installdir");

    // Filter out redistributable packages
    if (isRedistributable(game.id.storeId, game.name)) {
        return std::unexpected(Error(ErrorCode::Cancelled,
            "Skipping redistributable: " + game.name));
    }

    if (game.id.storeId.empty() || game.name.empty() || installDir.empty()) {
        return std::unexpected(Error(ErrorCode::ParseError,
            "Invalid manifest: " + acfFile.string()));
    }

    // Construct install path
    fs::path steamapps = acfFile.parent_path();
    game.installPath = steamapps / "common" / installDir;

    if (!fs::exists(game.installPath)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game directory not found: " + game.installPath.string()));
    }

    // Find main executable
    for (const auto& entry : fs::directory_iterator(game.installPath)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".exe") {
                auto filename = entry.path().filename().string();
                std::transform(filename.begin(), filename.end(),
                    filename.begin(), ::tolower);

                // Skip common non-game executables
                if (filename.find("launcher") != std::string::npos ||
                    filename.find("crash") != std::string::npos ||
                    filename.find("unins") != std::string::npos ||
                    filename.find("redist") != std::string::npos ||
                    filename.find("setup") != std::string::npos) {
                    continue;
                }

                game.executablePath = entry.path();
                break;
            }
        }
    }

    // Try to get size
    try {
        game.sizeBytes = std::stoull(getValue("SizeOnDisk"));
    } catch (...) {
        game.sizeBytes = 0;
    }

    return game;
}

} // namespace makineai::scanners
