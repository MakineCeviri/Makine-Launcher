/**
 * @file epic_scanner.cpp
 * @brief Epic Games scanner implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/game_detector.hpp"
#include "makineai/core.hpp"
#include "makineai/json_utils.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace makineai::scanners {

bool EpicScanner::isAvailable() const {
    auto manifestResult = findManifestDirectory();
    if (manifestResult.has_value()) {
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Available at {}", manifestResult->string());
        return true;
    }
    MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Not available");
    return false;
}

Result<std::vector<GameInfo>> EpicScanner::scan() const {
    MAKINEAI_TIMED_SCOPE(log::SCANNER, "EpicScanner::scan");
    MAKINEAI_LOG_INFO(log::SCANNER, "Starting Epic Games scan");

    auto scanTimer = metrics().timer("epic_scan");
    std::vector<GameInfo> games;

    auto manifestDirResult = findManifestDirectory();
    if (!manifestDirResult) {
        MAKINEAI_LOG_WARN(log::SCANNER, "Epic: Failed to find manifest directory: {}",
                         manifestDirResult.error().message());
        return std::unexpected(manifestDirResult.error());
    }

    fs::path manifestDir = *manifestDirResult;
    MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Scanning manifests in: {} (using {})",
                       manifestDir.string(), json::backendInfo());

    try {
        for (const auto& entry : fs::directory_iterator(manifestDir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            if (ext != ".item") continue;

            // Parse manifest using optimized JSON parser
            auto docResult = json::parseFile(entry.path());
            if (!docResult) {
                MAKINEAI_LOG_TRACE(log::SCANNER, "Epic: Failed to parse manifest {}: {}",
                                   entry.path().string(), docResult.error().message());
                continue;
            }

            const auto& manifest = *docResult;

            GameInfo game;
            game.id.store = GameStore::EpicGames;
            game.id.storeId = manifest.getString("CatalogItemId");
            game.name = manifest.getString("DisplayName");

            std::string installLocation = manifest.getString("InstallLocation");
            if (installLocation.empty()) continue;

            game.installPath = installLocation;
            if (!fs::exists(game.installPath)) {
                MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Game path not found for '{}': {}",
                                   game.name, installLocation);
                continue;
            }

            // Find executable
            std::string launchExe = manifest.getString("LaunchExecutable");
            if (!launchExe.empty()) {
                game.executablePath = game.installPath / launchExe;
            }

            // Get size
            game.sizeBytes = manifest.getUint("InstallSize", 0);

            MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Found game '{}' (ID: {})",
                               game.name, game.id.storeId);
            games.push_back(std::move(game));
            metrics().increment("epic_games_found");
        }
    } catch (const std::exception& e) {
        MAKINEAI_LOG_WARN(log::SCANNER, "Epic: Access error scanning manifests: {}", e.what());
    }

    MAKINEAI_LOG_INFO(log::SCANNER, "Epic scan complete: {} games found", games.size());
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
        "Epic game not found: " + catalogId));
}

Result<fs::path> EpicScanner::findManifestDirectory() const {
    MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Searching for manifest directory");

#ifdef _WIN32
    wchar_t* programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData))) {
        fs::path manifestDir = fs::path(programData) / "Epic" /
            "EpicGamesLauncher" / "Data" / "Manifests";
        CoTaskMemFree(programData);

        if (fs::exists(manifestDir)) {
            MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Found manifest directory via ProgramData: {}",
                               manifestDir.string());
            return manifestDir;
        }
    }

    // Try alternate location
    fs::path altPath = "C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    if (fs::exists(altPath)) {
        MAKINEAI_LOG_DEBUG(log::SCANNER, "Epic: Found manifest directory at default path");
        return altPath;
    }
#endif

    MAKINEAI_LOG_WARN(log::SCANNER, "Epic: Manifest directory not found");
    return std::unexpected(Error(ErrorCode::GameNotFound,
        "Epic Games manifest directory not found"));
}

} // namespace makineai::scanners
