/**
 * @file epic_scanner.cpp
 * @brief Epic Games scanner implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/game_detector.hpp"
#include "makine/core.hpp"
#include "makine/json_utils.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"

#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace makine::scanners {

bool EpicScanner::isAvailable() const {
    auto manifestResult = findManifestDirectory();
    if (manifestResult.has_value()) {
        MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Available at {}", manifestResult->string());
        return true;
    }
    MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Not available");
    return false;
}

Result<std::vector<GameInfo>> EpicScanner::scan() const {
    MAKINE_TIMED_SCOPE(log::SCANNER, "EpicScanner::scan");
    MAKINE_LOG_INFO(log::SCANNER, "Starting Epic Games scan");

    auto scanTimer = metrics().timer("epic_scan");
    std::vector<GameInfo> games;

    auto manifestDirResult = findManifestDirectory();
    if (!manifestDirResult) {
        MAKINE_LOG_WARN(log::SCANNER, "Epic: Failed to find manifest directory: {}",
                         manifestDirResult.error().message());
        return std::unexpected(manifestDirResult.error());
    }

    fs::path manifestDir = *manifestDirResult;
    MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Scanning manifests in: {} (using {})",
                       manifestDir.string(), json::backendInfo());

    try {
        for (const auto& entry : fs::directory_iterator(manifestDir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            if (ext != ".item") continue;

            // Parse manifest using optimized JSON parser
            auto docResult = json::parseFile(entry.path());
            if (!docResult) {
                MAKINE_LOG_TRACE(log::SCANNER, "Epic: Failed to parse manifest {}: {}",
                                   entry.path().string(), docResult.error().message());
                continue;
            }

            const auto& manifest = *docResult;

            // Skip DLC, add-ons, and non-executable content
            bool isIncludedItem = manifest.getBool("bIsIncludedItem", false);
            std::string mainGameId = manifest.getString("MainGameCatalogItemId");
            bool isExecutable = manifest.getBool("bIsExecutable", true);

            if (isIncludedItem || !mainGameId.empty() || !isExecutable) {
                MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Skipping DLC/content '{}' (included={}, mainGame={}, exe={})",
                                 manifest.getString("DisplayName"), isIncludedItem, mainGameId, isExecutable);
                metrics().increment("epic_dlc_skipped");
                continue;
            }

            GameInfo game;
            game.id.store = GameStore::EpicGames;
            game.id.storeId = manifest.getString("CatalogItemId");
            game.name = manifest.getString("DisplayName");

            std::string installLocation = manifest.getString("InstallLocation");
            if (installLocation.empty()) continue;

            game.installPath = installLocation;
            if (!fs::exists(game.installPath)) {
                MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Game path not found for '{}': {}",
                                   game.name, installLocation);
                continue;
            }

            // Find executable
            std::string launchExe = manifest.getString("LaunchExecutable");
            if (!launchExe.empty()) {
                game.executablePath = game.installPath / launchExe;
            }

            // Extract Epic-specific metadata for better matching
            game.version = manifest.getString("AppVersionString");
            std::string appName = manifest.getString("AppName");
            if (!appName.empty() && game.name.empty())
                game.name = appName;

            // Get size
            game.sizeBytes = manifest.getUint("InstallSize", 0);

            MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Found game '{}' (ID: {})",
                               game.name, game.id.storeId);
            games.push_back(std::move(game));
            metrics().increment("epic_games_found");
        }
    } catch (const std::exception& e) {
        MAKINE_LOG_WARN(log::SCANNER, "Epic: Access error scanning manifests: {}", e.what());
    }

    MAKINE_LOG_INFO(log::SCANNER, "Epic scan complete: {} games found", games.size());
    metrics().gauge("epic_total_games", static_cast<double>(games.size()));
    return games;
}

Result<GameInfo> EpicScanner::getGame(const std::string& catalogId) const {
    auto allGames = scan();
    if (!allGames) {
        return std::unexpected(allGames.error());
    }

    for (const auto& game : *allGames) {
        if (game.id.storeId == catalogId) {
            return game;
        }
    }

    return std::unexpected(Error(ErrorCode::GameNotFound,
        fmt::format("Epic game not found: {}", catalogId)));
}

Result<fs::path> EpicScanner::findManifestDirectory() const {
    MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Searching for manifest directory");

#ifdef _WIN32
    wchar_t* programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData))) {
        fs::path manifestDir = fs::path(programData) / "Epic" /
            "EpicGamesLauncher" / "Data" / "Manifests";
        CoTaskMemFree(programData);

        if (fs::exists(manifestDir)) {
            MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Found manifest directory via ProgramData: {}",
                               manifestDir.string());
            return manifestDir;
        }
    }

    // Try alternate location
    fs::path altPath = "C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    if (fs::exists(altPath)) {
        MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Found manifest directory at default path");
        return altPath;
    }
#endif

    MAKINE_LOG_WARN(log::SCANNER, "Epic: Manifest directory not found");
    return std::unexpected(Error(ErrorCode::GameNotFound,
        "Epic Games manifest directory not found"));
}

} // namespace makine::scanners
