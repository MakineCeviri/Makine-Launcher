/**
 * @file epic_scanner.cpp
 * @brief Epic Games scanner implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/game_detector.hpp"
#include "makineai/core.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace makineai {

using json = nlohmann::json;

bool EpicScanner::isAvailable() const {
    auto manifestResult = findManifestDirectory();
    return manifestResult.has_value();
}

Result<std::vector<GameInfo>> EpicScanner::scan() const {
    std::vector<GameInfo> games;

    auto manifestDirResult = findManifestDirectory();
    if (!manifestDirResult) {
        return std::unexpected(manifestDirResult.error());
    }

    fs::path manifestDir = *manifestDirResult;

    for (const auto& entry : fs::directory_iterator(manifestDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext != ".item") continue;

        try {
            std::ifstream file(entry.path());
            if (!file) continue;

            json manifest = json::parse(file);

            GameInfo game;
            game.id.store = GameStore::EpicGames;
            game.id.storeId = manifest.value("CatalogItemId", "");
            game.name = manifest.value("DisplayName", "");

            std::string installLocation = manifest.value("InstallLocation", "");
            if (installLocation.empty()) continue;

            game.installPath = installLocation;
            if (!fs::exists(game.installPath)) continue;

            // Find executable
            std::string launchExe = manifest.value("LaunchExecutable", "");
            if (!launchExe.empty()) {
                game.executablePath = game.installPath / launchExe;
            }

            // Get size
            game.sizeBytes = manifest.value("InstallSize", 0ULL);

            games.push_back(std::move(game));

        } catch (const json::exception& e) {
            logger()->debug("Failed to parse Epic manifest {}: {}",
                entry.path().string(), e.what());
        }
    }

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
#ifdef _WIN32
    wchar_t* programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData))) {
        fs::path manifestDir = fs::path(programData) / "Epic" /
            "EpicGamesLauncher" / "Data" / "Manifests";
        CoTaskMemFree(programData);

        if (fs::exists(manifestDir)) {
            return manifestDir;
        }
    }

    // Try alternate location
    fs::path altPath = "C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    if (fs::exists(altPath)) {
        return altPath;
    }
#endif

    return std::unexpected(Error(ErrorCode::GameNotFound,
        "Epic Games manifest directory not found"));
}

} // namespace makineai
