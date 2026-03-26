/**
 * @file steam_scanner.cpp
 * @brief Steam game scanner implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/game_detector.hpp"
#include "makine/vdf_parser.hpp"
#include "makine/core.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_set>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace makine::scanners {

#ifdef _WIN32
namespace {
// RAII wrapper for Windows registry key handles
struct RegKeyGuard {
    HKEY key = nullptr;
    ~RegKeyGuard() { if (key) RegCloseKey(key); }
    HKEY* ptr() { return &key; }
    HKEY get() const { return key; }
};
} // namespace
#endif

bool SteamScanner::isAvailable() const {
#ifdef _WIN32
    // Check registry for Steam installation
    {
        RegKeyGuard hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Valve\\Steam", 0, KEY_READ, hKey.ptr()) == ERROR_SUCCESS) {
            MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found via HKCU registry");
            return true;
        }
    }
    {
        RegKeyGuard hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"Software\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, hKey.ptr()) == ERROR_SUCCESS) {
            MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found via HKLM registry");
            return true;
        }
    }
    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Not found in registry");
#endif
    return false;
}

Result<std::vector<GameInfo>> SteamScanner::scan() const {
    MAKINE_TIMED_SCOPE(log::SCANNER, "SteamScanner::scan");
    MAKINE_LOG_INFO(log::SCANNER, "Starting Steam game scan");

    auto scanTimer = metrics().timer("steam_scan");
    std::vector<GameInfo> games;

    auto libraryResult = findLibraryFolders();
    if (!libraryResult) {
        MAKINE_LOG_WARN(log::SCANNER, "Steam: Failed to find library folders: {}",
                         libraryResult.error().message());
        return std::unexpected(libraryResult.error());
    }

    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found {} library folders", libraryResult->size());

    for (const auto& libraryPath : *libraryResult) {
        fs::path steamapps = fs::path(libraryPath) / "steamapps";
        if (!fs::exists(steamapps)) {
            MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Steamapps not found at {}", libraryPath);
            continue;
        }

        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Scanning library at {}", libraryPath);

        // Scan for .acf manifest files
        try {
            for (const auto& entry : fs::directory_iterator(steamapps)) {
                try {
                    if (!entry.is_regular_file()) continue;

                    auto filename = entry.path().filename().string();
                    if (!filename.starts_with("appmanifest_") ||
                        !filename.ends_with(".acf")) {
                        continue;
                    }

                    auto gameResult = parseAppManifest(entry.path());
                    if (gameResult) {
                        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found game '{}' (AppID: {})",
                                          gameResult->name, gameResult->id.storeId);
                        games.push_back(std::move(*gameResult));
                        metrics().increment("steam_games_found");
                    }
                } catch (const std::exception& e) {
                    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Error processing manifest {}: {}",
                                     entry.path().string(), e.what());
                }
            }
        } catch (const std::exception& e) {
            MAKINE_LOG_WARN(log::SCANNER, "Steam: Access error scanning {}: {}",
                             steamapps.string(), e.what());
        }
    }

    MAKINE_LOG_INFO(log::SCANNER, "Steam scan complete: {} games found", games.size());
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
    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Searching for library folders");
    StringList folders;

#ifdef _WIN32
    // Get Steam install path from registry
    {
        RegKeyGuard hKey;
        wchar_t steamPath[MAX_PATH] = {0};
        DWORD pathSize = sizeof(steamPath);

        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Valve\\Steam", 0, KEY_READ, hKey.ptr()) == ERROR_SUCCESS) {

            if (RegQueryValueExW(hKey.get(), L"SteamPath", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(steamPath), &pathSize) == ERROR_SUCCESS
                && steamPath[0] != L'\0') {

                int requiredSize = WideCharToMultiByte(CP_UTF8, 0, steamPath, -1,
                    nullptr, 0, nullptr, nullptr);
                if (requiredSize > 0) {
                    std::string narrowPath(requiredSize, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, steamPath, -1,
                        narrowPath.data(), requiredSize, nullptr, nullptr);
                    narrowPath.resize(narrowPath.size() - 1);
                    folders.push_back(narrowPath);
                    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found install path via registry: {}", narrowPath);
                }
            }
        }
    }

    if (folders.empty()) {
        // Try default paths
        const char* defaultPaths[] = {
            "C:\\Program Files (x86)\\Steam",
            "C:\\Program Files\\Steam",
            "D:\\Steam",
            "D:\\SteamLibrary"
        };

        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Registry path not found, trying default paths");
        for (const auto& path : defaultPaths) {
            if (fs::exists(path)) {
                folders.push_back(path);
                MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found at default path: {}", path);
                break;
            }
        }
    }
#endif

    if (folders.empty()) {
        MAKINE_LOG_WARN(log::SCANNER, "Steam: Installation not found");
        return std::unexpected(Error(ErrorCode::GameNotFound,
            "Steam installation not found"));
    }

    // Parse libraryfolders.vdf for additional library locations
    fs::path vdfPath = fs::path(folders[0]) / "steamapps" / "libraryfolders.vdf";
    if (fs::exists(vdfPath)) {
        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Parsing libraryfolders.vdf");
        std::ifstream vdf(vdfPath);
        std::string content((std::istreambuf_iterator<char>(vdf)),
                             std::istreambuf_iterator<char>());

        auto root = vdf::parse(content);
        if (root) {
            // Structure: "libraryfolders" { "0" { "path" "..." } "1" { ... } }
            const vdf::Node* libFolders = root->find("libraryfolders");
            if (!libFolders) libFolders = &*root;

            for (const auto& [key, entry] : libFolders->children) {
                if (!entry.isObject()) continue;

                std::string path = entry.getString("path");
                if (path.empty()) continue;

                if (std::find(folders.begin(), folders.end(), path) == folders.end()) {
                    if (fs::exists(path)) {
                        folders.push_back(path);
                        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found additional library: {}", path);
                    }
                }
            }
        } else {
            MAKINE_LOG_WARN(log::SCANNER, "Steam: Failed to parse libraryfolders.vdf");
        }
    } else {
        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: libraryfolders.vdf not found at {}", vdfPath.string());
    }

    MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Found {} library folders total", folders.size());
    return folders;
}

// Check if an app is a redistributable package (VC++, DirectX, etc.)
static bool isRedistributable(const std::string& appId, const std::string& name) {
    // Known redistributable AppIDs (O(1) lookup)
    static const std::unordered_set<std::string> redistributableIds = {
        "228980",  // Steamworks Common Redistributables
        "228981",  // SteamVR
        "250820",  // SteamVR Performance Test
        "1007",    // Steam Client
        "1070560", // Steam Linux Runtime
        "1391110", // Steam Linux Runtime - Soldier
        "1628350", // Steam Linux Runtime - Sniper
    };

    if (redistributableIds.count(appId)) return true;

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
        MAKINE_LOG_WARN(log::SCANNER, "Steam: Cannot open manifest: {}", acfFile.string());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open manifest: " + acfFile.string()));
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    auto root = vdf::parse(content);
    if (!root) {
        return std::unexpected(Error(ErrorCode::ParseError,
            "Failed to parse manifest: " + acfFile.string()));
    }

    // ACF structure: "AppState" { "appid" "123" "name" "Game Name" ... }
    const vdf::Node* appState = root->find("AppState");
    if (!appState) appState = &*root;

    GameInfo game;
    game.id.store = GameStore::Steam;
    game.id.storeId = appState->getString("appid");
    game.name = appState->getString("name");
    std::string installDir = appState->getString("installdir");

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

    // Find main executable (with error handling for permission issues)
    try {
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
    } catch (const std::exception& e) {
        MAKINE_LOG_DEBUG(log::SCANNER, "Steam: Cannot enumerate {}: {}",
                          game.installPath.string(), e.what());
    }

    // Try to get size
    try {
        std::string sizeStr = appState->getString("SizeOnDisk");
        game.sizeBytes = sizeStr.empty() ? 0 : std::stoull(sizeStr);
    } catch (const std::exception&) {
        game.sizeBytes = 0;
    }

    return game;
}

} // namespace makine::scanners
