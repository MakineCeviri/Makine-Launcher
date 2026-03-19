/**
 * @file gog_scanner.cpp
 * @brief GOG Galaxy scanner implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/game_detector.hpp"
#include "makine/core.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"

#include <algorithm>
#include <sqlite3.h>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace makine::scanners {

bool GOGScanner::isAvailable() const {
    auto dbResult = findDatabasePath();
    if (dbResult.has_value() && fs::exists(*dbResult)) {
        MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Available at {}", dbResult->string());
        return true;
    }
    MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Not available");
    return false;
}

Result<std::vector<GameInfo>> GOGScanner::scan() const {
    MAKINE_TIMED_SCOPE(log::SCANNER, "GOGScanner::scan");
    MAKINE_LOG_INFO(log::SCANNER, "Starting GOG Galaxy scan");

    auto scanTimer = metrics().timer("gog_scan");
    std::vector<GameInfo> games;

    auto dbPathResult = findDatabasePath();
    if (!dbPathResult) {
        MAKINE_LOG_WARN(log::SCANNER, "GOG: Failed to find database: {}",
                         dbPathResult.error().message());
        return std::unexpected(dbPathResult.error());
    }

    fs::path dbPath = *dbPathResult;
    if (!fs::exists(dbPath)) {
        MAKINE_LOG_WARN(log::SCANNER, "GOG: Database file not found: {}", dbPath.string());
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "GOG Galaxy database not found"));
    }

    MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Opening database at {}", dbPath.string());

    // Open SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(dbPath.string().c_str(), &db,
        SQLITE_OPEN_READONLY, nullptr);

    if (rc != SQLITE_OK) {
        MAKINE_LOG_WARN(log::SCANNER, "GOG: Cannot open database (SQLite error: {})", rc);
        if (db) sqlite3_close(db);
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open GOG database"));
    }

    // Query installed games
    // GOG Galaxy 2.0 uses a different schema than 1.x
    const char* sql = R"(
        SELECT
            p.releaseKey,
            pd.title,
            ip.path
        FROM
            InstalledBaseProducts ibp
        JOIN Products p ON ibp.productId = p.id
        JOIN ProductDetails pd ON p.id = pd.productId
        LEFT JOIN InstallationPaths ip ON p.id = ip.productId
        WHERE ip.path IS NOT NULL
    )";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Galaxy 2.0 schema not found, trying 1.x schema");
        // Try alternate query for GOG Galaxy 1.x
        const char* sql1x = R"(
            SELECT
                productId,
                title,
                installationPath
            FROM
                LimitedDetails
            WHERE installationPath IS NOT NULL
        )";

        rc = sqlite3_prepare_v2(db, sql1x, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            MAKINE_LOG_WARN(log::SCANNER, "GOG: Cannot query database (SQLite error: {})", rc);
            sqlite3_close(db);
            return std::unexpected(Error(ErrorCode::ParseError,
                "Cannot query GOG database"));
        }
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GameInfo game;
        game.id.store = GameStore::GOG;

        const char* releaseKey = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 0));
        const char* title = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 1));
        const char* path = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt, 2));

        if (!releaseKey || !title || !path) continue;

        game.id.storeId = releaseKey;
        game.name = title;
        game.installPath = path;

        if (!fs::exists(game.installPath)) {
            MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Game path not found for '{}': {}",
                               title, path);
            continue;
        }

        // Find executable
        try {
            for (const auto& entry : fs::directory_iterator(game.installPath)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (ext == ".exe") {
                    auto filename = entry.path().filename().string();
                    std::transform(filename.begin(), filename.end(),
                        filename.begin(), ::tolower);

                    if (filename.find("unins") != std::string::npos ||
                        filename.find("setup") != std::string::npos ||
                        filename.find("redist") != std::string::npos) {
                        continue;
                    }

                    game.executablePath = entry.path();
                    break;
                }
            }
        } catch (const std::exception& e) {
            MAKINE_LOG_WARN(log::SCANNER, "GOG: Access error scanning '{}': {}",
                              game.name, e.what());
        }

        MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Found game '{}' (ID: {})",
                           game.name, game.id.storeId);
        games.push_back(std::move(game));
        metrics().increment("gog_games_found");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    MAKINE_LOG_INFO(log::SCANNER, "GOG scan complete: {} games found", games.size());
    metrics().gauge("gog_total_games", static_cast<double>(games.size()));

    return games;
}

Result<GameInfo> GOGScanner::getGame(const std::string& gameId) const {
    auto allGames = scan();
    if (!allGames) {
        return std::unexpected(allGames.error());
    }

    for (const auto& game : *allGames) {
        if (game.id.storeId == gameId) {
            return game;
        }
    }

    return std::unexpected(Error(ErrorCode::GameNotFound,
        "GOG game not found: " + gameId));
}

Result<fs::path> GOGScanner::findDatabasePath() const {
    MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Searching for database");

#ifdef _WIN32
    wchar_t* programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData))) {
        fs::path dbPath = fs::path(programData) / "GOG.com" / "Galaxy" /
            "storage" / "galaxy-2.0.db";
        CoTaskMemFree(programData);

        if (fs::exists(dbPath)) {
            MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Found Galaxy 2.0 database");
            return dbPath;
        }

        // Try GOG Galaxy 1.x path
        dbPath = fs::path(programData) / "GOG.com" / "Galaxy" /
            "storage" / "index.db";
        if (fs::exists(dbPath)) {
            MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Found Galaxy 1.x database");
            return dbPath;
        }
    }

    // Try default path
    fs::path defaultPath = "C:\\ProgramData\\GOG.com\\Galaxy\\storage\\galaxy-2.0.db";
    if (fs::exists(defaultPath)) {
        MAKINE_LOG_DEBUG(log::SCANNER, "GOG: Found database at default path");
        return defaultPath;
    }
#endif

    MAKINE_LOG_WARN(log::SCANNER, "GOG: Database not found");
    return std::unexpected(Error(ErrorCode::GameNotFound,
        "GOG Galaxy database not found"));
}

} // namespace makine::scanners
