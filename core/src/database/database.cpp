/**
 * @file database.cpp
 * @brief Makine SQLite database implementation
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Full implementation of the database layer for Makine.
 * Based on the original Dart implementation from v0.0.8.
 */

#include <map>
#include "makine/database.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"
#include "makine/detail/scoped_metrics.hpp"

#include <sqlite3.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>

#ifdef _WIN32
#include <ShlObj.h>
#include <Windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace makine {

// ============== DPAPI FILE ENCRYPTION ==============

#ifdef _WIN32
namespace {

// Get path for encrypted database file
fs::path getEncryptedPath(const fs::path& dbPath) {
    return fs::path(dbPath.string() + ".enc");
}

// Encrypt a file using Windows DPAPI (user-specific)
bool dpapiEncryptFile(const fs::path& plainPath, const fs::path& encPath) {
    // Read plaintext file
    std::ifstream ifs(plainPath, std::ios::binary);
    if (!ifs) return false;

    std::vector<char> plainData(std::istreambuf_iterator<char>{ifs},
                                 std::istreambuf_iterator<char>{});
    ifs.close();

    if (plainData.empty()) return false;

    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(plainData.size());
    input.pbData = reinterpret_cast<BYTE*>(plainData.data());

    DATA_BLOB output{};

    // CryptProtectData — encrypts data using the current user's credentials
    // Flag 0 = user-specific encryption (only current user can decrypt)
    if (!CryptProtectData(&input, L"Makine Database",
                          nullptr, nullptr, nullptr,
                          0, &output)) {
        SecureZeroMemory(plainData.data(), plainData.size());
        return false;
    }

    // Write encrypted data
    std::ofstream ofs(encPath, std::ios::binary);
    if (!ofs) {
        LocalFree(output.pbData);
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
    ofs.close();
    LocalFree(output.pbData);

    // Securely clear plaintext from memory
    SecureZeroMemory(plainData.data(), plainData.size());

    return true;
}

// Decrypt a DPAPI-encrypted file
bool dpapiDecryptFile(const fs::path& encPath, const fs::path& plainPath) {
    // Read encrypted file
    std::ifstream ifs(encPath, std::ios::binary);
    if (!ifs) return false;

    std::vector<char> encData(std::istreambuf_iterator<char>{ifs},
                               std::istreambuf_iterator<char>{});
    ifs.close();

    if (encData.empty()) return false;

    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(encData.size());
    input.pbData = reinterpret_cast<BYTE*>(encData.data());

    DATA_BLOB output{};

    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            0, &output)) {
        return false;
    }

    // Write decrypted data
    std::ofstream ofs(plainPath, std::ios::binary);
    if (!ofs) {
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(output.pbData), output.cbData);
    ofs.close();

    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);

    return true;
}

} // anonymous namespace
#endif // _WIN32

// ============== HELPER FUNCTIONS ==============

Database::Statement::~Statement() {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

int64_t Database::now() noexcept {
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string Database::generateId(const std::string& prefix) {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << prefix << "_" << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    return ss.str();
}

// Get default database path
static fs::path getDefaultDbPath() {
#ifdef _WIN32
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        fs::path path(localAppData);
        CoTaskMemFree(localAppData);
        return path / "MakineCeviri" / "Makine-Launcher" / DATABASE_NAME;
    }
    // Fallback
    return fs::path(std::getenv("LOCALAPPDATA")) / "MakineCeviri" / "Makine-Launcher" / DATABASE_NAME;
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".local" / "share" / "MakineCeviri" / "Makine-Launcher" / DATABASE_NAME;
    }
    return fs::path(".") / DATABASE_NAME;
#endif
}

// Convert GameEngine enum to string
static std::string engineToDbString(GameEngine engine) {
    switch (engine) {
        case GameEngine::Unity_Mono: return "Unity_Mono";
        case GameEngine::Unity_IL2CPP: return "Unity_IL2CPP";
        case GameEngine::Unreal: return "Unreal";
        case GameEngine::Bethesda: return "Bethesda";
        case GameEngine::GameMaker: return "GameMaker";
        case GameEngine::RenPy: return "RenPy";
        case GameEngine::RPGMaker_MV: return "RPGMaker_MV";
        case GameEngine::RPGMaker_VX: return "RPGMaker_VX";
        case GameEngine::Godot: return "Godot";
        case GameEngine::Source: return "Source";
        case GameEngine::CryEngine: return "CryEngine";
        case GameEngine::Frostbite: return "Frostbite";
        case GameEngine::IdTech: return "IdTech";
        case GameEngine::Custom: return "Custom";
        default: return "Unknown";
    }
}

// Convert string to GameEngine enum
static GameEngine stringToEngine(const std::string& str) {
    if (str == "Unity_Mono") return GameEngine::Unity_Mono;
    if (str == "Unity_IL2CPP") return GameEngine::Unity_IL2CPP;
    if (str == "Unreal") return GameEngine::Unreal;
    if (str == "Bethesda") return GameEngine::Bethesda;
    if (str == "GameMaker") return GameEngine::GameMaker;
    if (str == "RenPy") return GameEngine::RenPy;
    if (str == "RPGMaker_MV") return GameEngine::RPGMaker_MV;
    if (str == "RPGMaker_VX") return GameEngine::RPGMaker_VX;
    if (str == "Godot") return GameEngine::Godot;
    if (str == "Source") return GameEngine::Source;
    if (str == "CryEngine") return GameEngine::CryEngine;
    if (str == "Frostbite") return GameEngine::Frostbite;
    if (str == "IdTech") return GameEngine::IdTech;
    if (str == "Custom") return GameEngine::Custom;
    return GameEngine::Unknown;
}

// Convert GameStore enum to string
static std::string storeToDbString(GameStore store) {
    switch (store) {
        case GameStore::Steam: return "Steam";
        case GameStore::EpicGames: return "EpicGames";
        case GameStore::GOG: return "GOG";
        case GameStore::Xbox: return "Xbox";
        case GameStore::Manual: return "Manual";
        default: return "Unknown";
    }
}

// Convert string to GameStore enum
static GameStore stringToStore(const std::string& str) {
    if (str == "Steam") return GameStore::Steam;
    if (str == "EpicGames") return GameStore::EpicGames;
    if (str == "GOG") return GameStore::GOG;
    if (str == "Xbox") return GameStore::Xbox;
    if (str == "Manual") return GameStore::Manual;
    return GameStore::Unknown;
}

// Get text from SQLite column (handles NULL)
static std::optional<std::string> getTextColumn(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    if (text) {
        return std::string(reinterpret_cast<const char*>(text));
    }
    return std::nullopt;
}

// Get required text from SQLite column
static std::string getRequiredText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? std::string(reinterpret_cast<const char*>(text)) : "";
}

// ============== SINGLETON ==============

Database& Database::instance() {
    static Database instance;
    return instance;
}

Database::~Database() {
    close();
}

// ============== INITIALIZATION ==============

Result<void> Database::initialize(const std::optional<fs::path>& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return {};  // Already initialized
    }

    // Determine database path
    dbPath_ = dbPath.value_or(getDefaultDbPath());

    // Create directory if needed
    fs::path dbDir = dbPath_.parent_path();
    if (!fs::exists(dbDir)) {
        std::error_code ec;
        fs::create_directories(dbDir, ec);
        if (ec) {
            return std::unexpected(Error{ErrorCode::IOError,
                fmt::format("Failed to create database directory: {}", ec.message())});
        }
    }

    MAKINE_LOG_INFO(log::DATABASE, "Database path: {}", dbPath_.string());

#ifdef _WIN32
    // If encrypted database exists but plaintext doesn't, decrypt first
    auto encPath = getEncryptedPath(dbPath_);
    if (fs::exists(encPath) && !fs::exists(dbPath_)) {
        MAKINE_LOG_INFO(log::DATABASE, "Decrypting database from: {}", encPath.string());
        if (!dpapiDecryptFile(encPath, dbPath_)) {
            MAKINE_LOG_ERROR(log::DATABASE, "Failed to decrypt database — DPAPI error");
            return std::unexpected(Error{ErrorCode::IOError,
                "Failed to decrypt database. The database may have been created by a different user."});
        }
        MAKINE_LOG_INFO(log::DATABASE, "Database decrypted successfully");
    }
#endif

    // Open database
    int rc = sqlite3_open(dbPath_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to open database: {}", error);
        return std::unexpected(Error{ErrorCode::IOError, fmt::format("Failed to open database: {}", error)});
    }

    // Track active connections
    Metrics::instance().gauge("db_connections_active", 1.0);

    // Enable foreign keys
    {
        char* rawErr = nullptr;
        rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &rawErr);
        auto errMsg = std::unique_ptr<char, decltype(&sqlite3_free)>(rawErr, sqlite3_free);
        if (rc != SQLITE_OK) {
            std::string error = rawErr ? rawErr : "Unknown error";
            MAKINE_LOG_WARN(log::DATABASE, "Failed to enable foreign keys: {}", error);
        }
    }

    // Set WAL mode for better concurrency
    {
        char* rawErr = nullptr;
        rc = sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &rawErr);
        auto errMsg = std::unique_ptr<char, decltype(&sqlite3_free)>(rawErr, sqlite3_free);
        if (rc != SQLITE_OK) {
            // Non-fatal, continue
        }
    }

    // Check database version
    int userVersion = 0;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            userVersion = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    MAKINE_LOG_INFO(log::DATABASE, "Database version: {} (target: {})", userVersion, DATABASE_VERSION);

    // Create tables if new database
    if (userVersion == 0) {
        auto result = createTables();
        if (!result) {
            return result;
        }
    }
    // Migrate if old version
    else if (userVersion < DATABASE_VERSION) {
        if (userVersion < 2) {
            auto result = migrateToV2();
            if (!result) {
                return result;
            }
        }
    }

    // Update version
    auto setVersion = fmt::format("PRAGMA user_version = {};", DATABASE_VERSION);
    sqlite3_exec(db_, setVersion.c_str(), nullptr, nullptr, nullptr);

    initialized_ = true;
    MAKINE_LOG_INFO(log::DATABASE, "Database initialized successfully");
    return {};
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
        Metrics::instance().gauge("db_connections_active", 0.0);

#ifdef _WIN32
        // Encrypt database file after closing
        if (!dbPath_.empty() && fs::exists(dbPath_)) {
            auto encPath = getEncryptedPath(dbPath_);
            if (dpapiEncryptFile(dbPath_, encPath)) {
                // Remove plaintext file after successful encryption
                std::error_code ec;
                fs::remove(dbPath_, ec);
                // Also remove WAL and SHM journal files
                fs::remove(fs::path(dbPath_.string() + "-wal"), ec);
                fs::remove(fs::path(dbPath_.string() + "-shm"), ec);
                MAKINE_LOG_INFO(log::DATABASE, "Database encrypted and plaintext removed");
            } else {
                MAKINE_LOG_WARN(log::DATABASE,
                    "Failed to encrypt database — plaintext file remains on disk");
            }
        }
#endif

        MAKINE_LOG_INFO(log::DATABASE, "Database closed");
    }
}

bool Database::isInitialized() const noexcept {
    return initialized_;
}

fs::path Database::getPath() const noexcept {
    return dbPath_;
}

// ============== TABLE CREATION ==============

Result<void> Database::createTables() {
    MAKINE_LOG_INFO(log::DATABASE, "Creating database tables (v{})...", DATABASE_VERSION);

    // Games table
    auto result = execute(R"(
        CREATE TABLE IF NOT EXISTS games (
            id TEXT PRIMARY KEY,
            steam_app_id TEXT UNIQUE,
            name TEXT NOT NULL,
            install_path TEXT,
            executable_path TEXT,
            engine TEXT,
            engine_version TEXT,
            store TEXT DEFAULT 'Unknown',
            version TEXT,
            size_bytes INTEGER,
            is_64bit INTEGER DEFAULT 1,
            icon_path TEXT,
            publisher TEXT,
            developer TEXT,
            is_verified INTEGER DEFAULT 0,
            last_scanned_at INTEGER,
            file_checksum TEXT,
            patch_status TEXT DEFAULT 'none',
            metadata TEXT,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        )
    )");
    if (!result) return result;

    // Patch history table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS patch_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            game_id TEXT NOT NULL,
            patch_type TEXT NOT NULL,
            status TEXT NOT NULL,
            backup_path TEXT,
            original_checksum TEXT,
            patched_checksum TEXT,
            strings_patched INTEGER DEFAULT 0,
            error_message TEXT,
            applied_at INTEGER NOT NULL,
            reverted_at INTEGER,
            FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Backups table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS backups (
            id TEXT PRIMARY KEY,
            game_id TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            manifest TEXT NOT NULL,
            size_bytes INTEGER,
            status TEXT DEFAULT 'active',
            FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Settings table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at INTEGER NOT NULL
        )
    )");
    if (!result) return result;

    // Create indexes
    const std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_games_steam_id ON games(steam_app_id)",
        "CREATE INDEX IF NOT EXISTS idx_games_engine ON games(engine)",
        "CREATE INDEX IF NOT EXISTS idx_games_store ON games(store)",
        "CREATE INDEX IF NOT EXISTS idx_patch_history_game ON patch_history(game_id)",
        "CREATE INDEX IF NOT EXISTS idx_backups_game ON backups(game_id)"
    };

    for (const auto& sql : indexes) {
        result = execute(sql);
        if (!result) {
            MAKINE_LOG_WARN(log::DATABASE, "Failed to create index: {}", sql);
            // Continue with other indexes
        }
    }

    MAKINE_LOG_INFO(log::DATABASE, "Database tables created successfully (v{})", DATABASE_VERSION);
    return {};
}

Result<void> Database::migrateToV2() {
    MAKINE_LOG_INFO(log::DATABASE, "Migrating database to v2...");

    // v2 added backups table

    // These tables might not exist, create them
    auto result = createTables();
    if (!result) {
        MAKINE_LOG_ERROR(log::DATABASE, "Migration to v2 failed");
        return result;
    }

    MAKINE_LOG_INFO(log::DATABASE, "v2 migration completed");
    return {};
}

// ============== HELPER METHODS ==============

Result<void> Database::execute(const std::string& sql) {
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    char* rawErr = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &rawErr);
    auto errMsg = std::unique_ptr<char, decltype(&sqlite3_free)>(rawErr, sqlite3_free);

    Metrics::instance().recordHistogram("db_query_duration_ms", m.elapsed().count());

    if (rc != SQLITE_OK) {
        std::string error = rawErr ? rawErr : "Unknown error";
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "SQL error: {}", error);
        return std::unexpected(Error{ErrorCode::IOError, fmt::format("SQL error: {}", error)});
    }

    // Warn about slow queries (>100ms)
    if (m.elapsed().count() > 100) {
        MAKINE_LOG_WARN(log::DATABASE, "Slow query detected ({}ms)", m.elapsed().count());
    }

    return {};
}

Result<Database::Statement> Database::prepare(const std::string& sql) {
    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt.stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to prepare statement: {}", error);
        Metrics::instance().increment("db_query_errors"); // no scoped timer needed here
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to prepare statement: {}", error)});
    }
    MAKINE_LOG_DEBUG(log::DATABASE, "Prepared statement");
    return stmt;
}

Result<void> Database::executeRaw(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    return execute(sql);
}

Result<void> Database::beginTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    return execute("BEGIN TRANSACTION");
}

Result<void> Database::commitTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    return execute("COMMIT");
}

Result<void> Database::rollbackTransaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    return execute("ROLLBACK");
}

Result<void> Database::vacuum() {
    std::lock_guard<std::mutex> lock(mutex_);
    return execute("VACUUM");
}

// ============== GAME OPERATIONS ==============

Result<void> Database::saveGame(const GameInfo& game) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Saving game: {}", game.name);

    const char* sql = R"(
        INSERT OR REPLACE INTO games (
            id, steam_app_id, name, install_path, executable_path,
            engine, engine_version, store, version, size_bytes,
            is_64bit, icon_path, publisher, developer,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int64_t timestamp = now();

    // Construct game ID from store and storeId
    std::string gameId = storeToDbString(game.id.store) + "_" + game.id.storeId;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, game.id.storeId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 3, game.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 4, game.installPath.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 5, game.executablePath.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 6, engineToDbString(game.engine).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 7, game.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 8, storeToDbString(game.id.store).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 9, game.version.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 10, static_cast<int64_t>(game.sizeBytes));
    sqlite3_bind_int(stmt.stmt, 11, game.is64Bit ? 1 : 0);

    if (game.iconPath) {
        sqlite3_bind_text(stmt.stmt, 12, game.iconPath->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 12);
    }

    if (game.publisher) {
        sqlite3_bind_text(stmt.stmt, 13, game.publisher->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 13);
    }

    if (game.developer) {
        sqlite3_bind_text(stmt.stmt, 14, game.developer->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 14);
    }

    sqlite3_bind_int64(stmt.stmt, 15, timestamp);
    sqlite3_bind_int64(stmt.stmt, 16, timestamp);

    int rc = sqlite3_step(stmt.stmt);

    if (rc != SQLITE_DONE) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to save game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to save game: {}", sqlite3_errmsg(db_))});
    }

    MAKINE_LOG_DEBUG(log::DATABASE, "Game saved successfully: {}", game.name);
    return {};
}

Result<std::optional<GameInfo>> Database::getGameBySteamId(const std::string& steamAppId) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Looking up game by Steam ID");

    const char* sql = R"(
        SELECT id, steam_app_id, name, install_path, executable_path,
               engine, engine_version, store, version, size_bytes,
               is_64bit, icon_path, publisher, developer
        FROM games WHERE steam_app_id = ? LIMIT 1
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, steamAppId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);

    if (rc == SQLITE_DONE) {
        MAKINE_LOG_DEBUG(log::DATABASE, "Game not found by Steam ID");
        return std::nullopt;  // Not found
    }

    if (rc != SQLITE_ROW) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to query game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to query game: {}", sqlite3_errmsg(db_))});
    }

    GameInfo game;
    game.id.storeId = getRequiredText(stmt.stmt, 1);
    game.id.store = stringToStore(getRequiredText(stmt.stmt, 7));
    game.name = getRequiredText(stmt.stmt, 2);
    game.installPath = getRequiredText(stmt.stmt, 3);
    game.executablePath = getRequiredText(stmt.stmt, 4);
    game.engine = stringToEngine(getRequiredText(stmt.stmt, 5));
    game.version = getRequiredText(stmt.stmt, 8);
    game.sizeBytes = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 9));
    game.is64Bit = sqlite3_column_int(stmt.stmt, 10) != 0;
    game.iconPath = getTextColumn(stmt.stmt, 11);
    game.publisher = getTextColumn(stmt.stmt, 12);
    game.developer = getTextColumn(stmt.stmt, 13);

    MAKINE_LOG_DEBUG(log::DATABASE, "Found game: {}", game.name);
    return game;
}

Result<std::optional<GameInfo>> Database::getGameById(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Looking up game by ID");

    const char* sql = R"(
        SELECT id, steam_app_id, name, install_path, executable_path,
               engine, engine_version, store, version, size_bytes,
               is_64bit, icon_path, publisher, developer
        FROM games WHERE id = ? LIMIT 1
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);

    if (rc == SQLITE_DONE) {
        MAKINE_LOG_DEBUG(log::DATABASE, "Game not found by ID");
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to query game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to query game: {}", sqlite3_errmsg(db_))});
    }

    GameInfo game;
    game.id.storeId = getRequiredText(stmt.stmt, 1);
    game.id.store = stringToStore(getRequiredText(stmt.stmt, 7));
    game.name = getRequiredText(stmt.stmt, 2);
    game.installPath = getRequiredText(stmt.stmt, 3);
    game.executablePath = getRequiredText(stmt.stmt, 4);
    game.engine = stringToEngine(getRequiredText(stmt.stmt, 5));
    game.version = getRequiredText(stmt.stmt, 8);
    game.sizeBytes = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 9));
    game.is64Bit = sqlite3_column_int(stmt.stmt, 10) != 0;
    game.iconPath = getTextColumn(stmt.stmt, 11);
    game.publisher = getTextColumn(stmt.stmt, 12);
    game.developer = getTextColumn(stmt.stmt, 13);

    MAKINE_LOG_DEBUG(log::DATABASE, "Found game: {}", game.name);
    return game;
}

Result<std::vector<GameInfo>> Database::getAllGames() {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Getting all games");

    const char* sql = R"(
        SELECT id, steam_app_id, name, install_path, executable_path,
               engine, engine_version, store, version, size_bytes,
               is_64bit, icon_path, publisher, developer
        FROM games ORDER BY name ASC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    std::vector<GameInfo> games;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        GameInfo game;
        game.id.storeId = getRequiredText(stmt.stmt, 1);
        game.id.store = stringToStore(getRequiredText(stmt.stmt, 7));
        game.name = getRequiredText(stmt.stmt, 2);
        game.installPath = getRequiredText(stmt.stmt, 3);
        game.executablePath = getRequiredText(stmt.stmt, 4);
        game.engine = stringToEngine(getRequiredText(stmt.stmt, 5));
        game.version = getRequiredText(stmt.stmt, 8);
        game.sizeBytes = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 9));
        game.is64Bit = sqlite3_column_int(stmt.stmt, 10) != 0;
        game.iconPath = getTextColumn(stmt.stmt, 11);
        game.publisher = getTextColumn(stmt.stmt, 12);
        game.developer = getTextColumn(stmt.stmt, 13);

        games.push_back(std::move(game));
    }

    MAKINE_LOG_DEBUG(log::DATABASE, "Retrieved {} games", games.size());
    return games;
}

Result<void> Database::deleteGame(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Deleting game");

    const char* sql = "DELETE FROM games WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);

    if (rc != SQLITE_DONE) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to delete game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to delete game: {}", sqlite3_errmsg(db_))});
    }

    MAKINE_LOG_DEBUG(log::DATABASE, "Game deleted successfully");
    return {};
}


// ============== SETTINGS OPERATIONS ==============
Result<void> Database::setSetting(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Setting config key");

    const char* sql = R"(
        INSERT OR REPLACE INTO settings (key, value, updated_at)
        VALUES (?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 3, now());

    int rc = sqlite3_step(stmt.stmt);

    if (rc != SQLITE_DONE) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to set setting: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to set setting: {}", sqlite3_errmsg(db_))});
    }

    return {};
}

Result<std::optional<std::string>> Database::getSetting(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Getting config key");

    const char* sql = "SELECT value FROM settings WHERE key = ? LIMIT 1";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);

    if (rc == SQLITE_DONE) {
        MAKINE_LOG_DEBUG(log::DATABASE, "Setting not found");
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to get setting: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to get setting: {}", sqlite3_errmsg(db_))});
    }

    return getRequiredText(stmt.stmt, 0);
}

Result<std::map<std::string, std::string>> Database::getAllSettings() {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Getting all settings");

    const char* sql = "SELECT key, value FROM settings";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    std::map<std::string, std::string> settings;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        std::string key = getRequiredText(stmt.stmt, 0);
        std::string value = getRequiredText(stmt.stmt, 1);
        settings[key] = value;
    }

    MAKINE_LOG_DEBUG(log::DATABASE, "Retrieved {} settings", settings.size());
    return settings;
}

// ============== BACKUP OPERATIONS ==============

Result<void> Database::addBackupRecord(const BackupRecord& backup) {
    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_INFO(log::DATABASE, "Adding backup record for game");

    const char* sql = R"(
        INSERT INTO backups (id, game_id, created_at, manifest, size_bytes, status)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, backup.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, backup.gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 3, backup.createdAt > 0 ? backup.createdAt : now());
    sqlite3_bind_text(stmt.stmt, 4, backup.manifest.c_str(), -1, SQLITE_TRANSIENT);

    if (backup.sizeBytes) {
        sqlite3_bind_int64(stmt.stmt, 5, static_cast<int64_t>(*backup.sizeBytes));
    } else {
        sqlite3_bind_null(stmt.stmt, 5);
    }

    sqlite3_bind_text(stmt.stmt, 6, "active", -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);

    if (rc != SQLITE_DONE) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to add backup record: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to add backup record: {}", sqlite3_errmsg(db_))});
    }

    MAKINE_LOG_INFO(log::DATABASE, "Backup record added successfully");
    return {};
}

Result<std::vector<BackupRecord>> Database::getBackupsByGame(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        SELECT id, game_id, created_at, manifest, size_bytes, status
        FROM backups WHERE game_id = ? AND status = 'active'
        ORDER BY created_at DESC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<BackupRecord> backups;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        BackupRecord backup;
        backup.id = getRequiredText(stmt.stmt, 0);
        backup.gameId = getRequiredText(stmt.stmt, 1);
        backup.createdAt = sqlite3_column_int64(stmt.stmt, 2);
        backup.manifest = getRequiredText(stmt.stmt, 3);

        if (sqlite3_column_type(stmt.stmt, 4) != SQLITE_NULL) {
            backup.sizeBytes = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 4));
        }

        backup.status = BackupStatus::Active;

        backups.push_back(std::move(backup));
    }

    return backups;
}

Result<std::optional<BackupRecord>> Database::getBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        SELECT id, game_id, created_at, manifest, size_bytes, status
        FROM backups WHERE id = ? LIMIT 1
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, backupId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to get backup: {}", sqlite3_errmsg(db_))});
    }

    BackupRecord backup;
    backup.id = getRequiredText(stmt.stmt, 0);
    backup.gameId = getRequiredText(stmt.stmt, 1);
    backup.createdAt = sqlite3_column_int64(stmt.stmt, 2);
    backup.manifest = getRequiredText(stmt.stmt, 3);

    if (sqlite3_column_type(stmt.stmt, 4) != SQLITE_NULL) {
        backup.sizeBytes = static_cast<uint64_t>(sqlite3_column_int64(stmt.stmt, 4));
    }

    std::string statusStr = getRequiredText(stmt.stmt, 5);
    if (statusStr == "deleted") {
        backup.status = BackupStatus::Deleted;
    } else if (statusStr == "corrupted") {
        backup.status = BackupStatus::Corrupted;
    } else {
        backup.status = BackupStatus::Active;
    }

    return backup;
}

Result<void> Database::deleteBackupRecord(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "UPDATE backups SET status = 'deleted' WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, backupId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to delete backup record: {}", sqlite3_errmsg(db_))});
    }

    return {};
}

// ============== PATCH HISTORY OPERATIONS ==============

Result<int64_t> Database::addPatchRecord(
    const std::string& gameId,
    const std::string& patchType,
    const std::string& status,
    const std::optional<std::string>& backupPath,
    int stringsPatched,
    const std::optional<std::string>& errorMessage) {

    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_INFO(log::DATABASE, "Adding patch record: type={}, status={}, strings={}",
                      patchType, status, stringsPatched);

    const char* sql = R"(
        INSERT INTO patch_history (
            game_id, patch_type, status, backup_path, strings_patched, error_message, applied_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, patchType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 3, status.c_str(), -1, SQLITE_TRANSIENT);

    if (backupPath) {
        sqlite3_bind_text(stmt.stmt, 4, backupPath->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 4);
    }

    sqlite3_bind_int(stmt.stmt, 5, stringsPatched);

    if (errorMessage) {
        sqlite3_bind_text(stmt.stmt, 6, errorMessage->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 6);
    }

    sqlite3_bind_int64(stmt.stmt, 7, now());

    int rc = sqlite3_step(stmt.stmt);

    if (rc != SQLITE_DONE) {
        m.markError();
        MAKINE_LOG_ERROR(log::DATABASE, "Failed to add patch record: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to add patch record: {}", sqlite3_errmsg(db_))});
    }

    MAKINE_LOG_INFO(log::DATABASE, "Patch record added successfully");
    return sqlite3_last_insert_rowid(db_);
}

Result<std::vector<std::map<std::string, std::string>>> Database::getPatchHistory(
    const std::string& gameId) {

    std::lock_guard<std::mutex> lock(mutex_);
    ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");

    MAKINE_LOG_DEBUG(log::DATABASE, "Getting patch history");

    const char* sql = R"(
        SELECT id, game_id, patch_type, status, backup_path,
               strings_patched, error_message, applied_at, reverted_at
        FROM patch_history WHERE game_id = ?
        ORDER BY applied_at DESC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<std::map<std::string, std::string>> history;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        std::map<std::string, std::string> record;
        record["id"] = fmt::to_string(sqlite3_column_int64(stmt.stmt, 0));
        record["game_id"] = getRequiredText(stmt.stmt, 1);
        record["patch_type"] = getRequiredText(stmt.stmt, 2);
        record["status"] = getRequiredText(stmt.stmt, 3);

        auto backupPath = getTextColumn(stmt.stmt, 4);
        if (backupPath) record["backup_path"] = *backupPath;

        record["strings_patched"] = fmt::to_string(sqlite3_column_int(stmt.stmt, 5));

        auto errorMsg = getTextColumn(stmt.stmt, 6);
        if (errorMsg) record["error_message"] = *errorMsg;

        record["applied_at"] = fmt::to_string(sqlite3_column_int64(stmt.stmt, 7));

        if (sqlite3_column_type(stmt.stmt, 8) != SQLITE_NULL) {
            record["reverted_at"] = fmt::to_string(sqlite3_column_int64(stmt.stmt, 8));
        }

        history.push_back(std::move(record));
    }

    return history;
}

Result<void> Database::markPatchReverted(int64_t patchId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE patch_history SET status = 'reverted', reverted_at = ?
        WHERE id = ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, now());
    sqlite3_bind_int64(stmt.stmt, 2, patchId);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            fmt::format("Failed to mark patch reverted: {}", sqlite3_errmsg(db_))});
    }

    return {};
}

} // namespace makine
