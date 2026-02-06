/**
 * @file gog_scanner.cpp
 * @brief GOG Galaxy scanner implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/game_detector.hpp"
#include "makineai/core.hpp"

#include <sqlite3.h>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace makineai {

bool GOGScanner::isAvailable() const {
    auto dbResult = findDatabasePath();
    return dbResult.has_value() && fs::exists(*dbResult);
}

Result<std::vector<GameInfo>> GOGScanner::scan() const {
    std::vector<GameInfo> games;

    auto dbPathResult = findDatabasePath();
    if (!dbPathResult) {
        return std::unexpected(dbPathResult.error());
    }

    fs::path dbPath = *dbPathResult;
    if (!fs::exists(dbPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "GOG Galaxy database not found"));
    }

    // Open SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(dbPath.string().c_str(), &db,
        SQLITE_OPEN_READONLY, nullptr);

    if (rc != SQLITE_OK) {
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

        if (!fs::exists(game.installPath)) continue;

        // Find executable
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

        games.push_back(std::move(game));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

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
#ifdef _WIN32
    wchar_t* programData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData))) {
        fs::path dbPath = fs::path(programData) / "GOG.com" / "Galaxy" /
            "storage" / "galaxy-2.0.db";
        CoTaskMemFree(programData);

        if (fs::exists(dbPath)) {
            return dbPath;
        }

        // Try GOG Galaxy 1.x path
        dbPath = fs::path(programData) / "GOG.com" / "Galaxy" /
            "storage" / "index.db";
        if (fs::exists(dbPath)) {
            return dbPath;
        }
    }

    // Try default path
    fs::path defaultPath = "C:\\ProgramData\\GOG.com\\Galaxy\\storage\\galaxy-2.0.db";
    if (fs::exists(defaultPath)) {
        return defaultPath;
    }
#endif

    return std::unexpected(Error(ErrorCode::GameNotFound,
        "GOG Galaxy database not found"));
}

} // namespace makineai
