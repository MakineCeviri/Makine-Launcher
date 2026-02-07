/**
 * @file database.cpp
 * @brief MakineAI SQLite database implementation
 * @copyright (c) 2026 MakineAI Team
 *
 * Full implementation of the database layer for MakineAI.
 * Based on the original Dart implementation from v0.0.8.
 */

#include "makineai/database.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <sqlite3.h>

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <ShlObj.h>
#include <Windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

namespace makineai {

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

    std::vector<char> plainData((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
    ifs.close();

    if (plainData.empty()) return false;

    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(plainData.size());
    input.pbData = reinterpret_cast<BYTE*>(plainData.data());

    DATA_BLOB output{};

    // CryptProtectData — encrypts data using the current user's credentials
    // Flag 0 = user-specific encryption (only current user can decrypt)
    if (!CryptProtectData(&input, L"MakineAI Database",
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

    std::vector<char> encData((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
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
        return path / "MakineAI" / DATABASE_NAME;
    }
    // Fallback
    return fs::path(std::getenv("LOCALAPPDATA")) / "MakineAI" / DATABASE_NAME;
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / ".local" / "share" / "MakineAI" / DATABASE_NAME;
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

// Convert EntryStatus enum to string
static std::string statusToDbString(EntryStatus status) {
    switch (status) {
        case EntryStatus::Untranslated: return "untranslated";
        case EntryStatus::Translated: return "translated";
        case EntryStatus::Fuzzy: return "fuzzy";
        case EntryStatus::Verified: return "verified";
        case EntryStatus::Rejected: return "rejected";
        default: return "untranslated";
    }
}

// Convert string to EntryStatus enum
static EntryStatus stringToStatus(const std::string& str) {
    if (str == "translated") return EntryStatus::Translated;
    if (str == "fuzzy") return EntryStatus::Fuzzy;
    if (str == "verified") return EntryStatus::Verified;
    if (str == "rejected") return EntryStatus::Rejected;
    return EntryStatus::Untranslated;
}

// Convert TermDomain enum to string
static std::string domainToDbString(TermDomain domain) {
    switch (domain) {
        case TermDomain::General: return "general";
        case TermDomain::RPG: return "rpg";
        case TermDomain::FPS: return "fps";
        case TermDomain::VisualNovel: return "visualNovel";
        case TermDomain::Strategy: return "strategy";
        case TermDomain::Simulation: return "simulation";
        case TermDomain::Adventure: return "adventure";
        case TermDomain::Puzzle: return "puzzle";
        case TermDomain::Action: return "action";
        case TermDomain::Horror: return "horror";
        default: return "general";
    }
}

// Convert string to TermDomain enum
static TermDomain stringToDomain(const std::string& str) {
    if (str == "rpg") return TermDomain::RPG;
    if (str == "fps") return TermDomain::FPS;
    if (str == "visualNovel") return TermDomain::VisualNovel;
    if (str == "strategy") return TermDomain::Strategy;
    if (str == "simulation") return TermDomain::Simulation;
    if (str == "adventure") return TermDomain::Adventure;
    if (str == "puzzle") return TermDomain::Puzzle;
    if (str == "action") return TermDomain::Action;
    if (str == "horror") return TermDomain::Horror;
    return TermDomain::General;
}

// Convert ProjectStatus enum to string
static std::string projectStatusToDbString(ProjectStatus status) {
    switch (status) {
        case ProjectStatus::Active: return "active";
        case ProjectStatus::Completed: return "completed";
        case ProjectStatus::Archived: return "archived";
        case ProjectStatus::Paused: return "paused";
        default: return "active";
    }
}

// Convert string to ProjectStatus enum
static ProjectStatus stringToProjectStatus(const std::string& str) {
    if (str == "completed") return ProjectStatus::Completed;
    if (str == "archived") return ProjectStatus::Archived;
    if (str == "paused") return ProjectStatus::Paused;
    return ProjectStatus::Active;
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
                "Failed to create database directory: " + ec.message()});
        }
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "Database path: {}", dbPath_.string());

#ifdef _WIN32
    // If encrypted database exists but plaintext doesn't, decrypt first
    auto encPath = getEncryptedPath(dbPath_);
    if (fs::exists(encPath) && !fs::exists(dbPath_)) {
        MAKINEAI_LOG_INFO(log::DATABASE, "Decrypting database from: {}", encPath.string());
        if (!dpapiDecryptFile(encPath, dbPath_)) {
            MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to decrypt database — DPAPI error");
            return std::unexpected(Error{ErrorCode::IOError,
                "Failed to decrypt database. The database may have been created by a different user."});
        }
        MAKINEAI_LOG_INFO(log::DATABASE, "Database decrypted successfully");
    }
#endif

    // Open database
    int rc = sqlite3_open(dbPath_.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to open database: {}", error);
        return std::unexpected(Error{ErrorCode::IOError, "Failed to open database: " + error});
    }

    // Track active connections
    Metrics::instance().gauge("db_connections_active", 1.0);

    // Enable foreign keys
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        MAKINEAI_LOG_WARN(log::DATABASE, "Failed to enable foreign keys: {}", error);
    }

    // Set WAL mode for better concurrency
    rc = sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        // Non-fatal, continue
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

    MAKINEAI_LOG_INFO(log::DATABASE, "Database version: {} (target: {})", userVersion, DATABASE_VERSION);

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
    std::string setVersion = "PRAGMA user_version = " + std::to_string(DATABASE_VERSION) + ";";
    sqlite3_exec(db_, setVersion.c_str(), nullptr, nullptr, nullptr);

    initialized_ = true;
    MAKINEAI_LOG_INFO(log::DATABASE, "Database initialized successfully");
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
                MAKINEAI_LOG_INFO(log::DATABASE, "Database encrypted and plaintext removed");
            } else {
                MAKINEAI_LOG_WARN(log::DATABASE,
                    "Failed to encrypt database — plaintext file remains on disk");
            }
        }
#endif

        MAKINEAI_LOG_INFO(log::DATABASE, "Database closed");
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
    MAKINEAI_LOG_INFO(log::DATABASE, "Creating database tables (v{})...", DATABASE_VERSION);

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

    // Translation cache table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS translation_cache (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            string_hash TEXT UNIQUE NOT NULL,
            original_text TEXT NOT NULL,
            translated_text TEXT NOT NULL,
            source TEXT NOT NULL,
            quality_score REAL DEFAULT 0,
            game_context TEXT,
            usage_count INTEGER DEFAULT 1,
            created_at INTEGER NOT NULL,
            last_used_at INTEGER NOT NULL
        )
    )");
    if (!result) return result;

    // Translation memory table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS translation_memory (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_text TEXT NOT NULL,
            target_text TEXT NOT NULL,
            source_hash TEXT NOT NULL,
            source_lang TEXT DEFAULT 'en',
            target_lang TEXT DEFAULT 'tr',
            context TEXT,
            game_id TEXT,
            engine_type TEXT,
            category TEXT,
            quality_score INTEGER DEFAULT 100,
            usage_count INTEGER DEFAULT 1,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            created_by TEXT,
            verified INTEGER DEFAULT 0,
            UNIQUE(source_hash, context, target_lang)
        )
    )");
    if (!result) return result;

    // N-gram index table (for fuzzy matching)
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS tm_ngrams (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tm_id INTEGER NOT NULL,
            ngram TEXT NOT NULL,
            position INTEGER,
            FOREIGN KEY (tm_id) REFERENCES translation_memory(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Translation variants table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS tm_variants (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tm_id INTEGER NOT NULL,
            variant_text TEXT NOT NULL,
            vote_count INTEGER DEFAULT 0,
            created_by TEXT,
            created_at INTEGER NOT NULL,
            FOREIGN KEY (tm_id) REFERENCES translation_memory(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Translation projects table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS translation_projects (
            id TEXT PRIMARY KEY,
            game_id TEXT,
            name TEXT NOT NULL,
            source_lang TEXT DEFAULT 'en',
            target_lang TEXT DEFAULT 'tr',
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            status TEXT DEFAULT 'active',
            progress REAL DEFAULT 0,
            settings TEXT,
            FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE SET NULL
        )
    )");
    if (!result) return result;

    // Translation entries table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS translation_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            project_id TEXT NOT NULL,
            file_path TEXT NOT NULL,
            entry_key TEXT,
            source_text TEXT NOT NULL,
            target_text TEXT,
            context TEXT,
            category TEXT,
            status TEXT DEFAULT 'untranslated',
            qa_score INTEGER DEFAULT 100,
            qa_issues TEXT,
            placeholders TEXT,
            line_number INTEGER,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            translated_by TEXT,
            FOREIGN KEY (project_id) REFERENCES translation_projects(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Glossary table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS glossary (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            term_source TEXT NOT NULL,
            term_target TEXT NOT NULL,
            term_type TEXT,
            domain TEXT,
            case_sensitive INTEGER DEFAULT 0,
            exact_match INTEGER DEFAULT 0,
            priority INTEGER DEFAULT 50,
            notes TEXT,
            examples TEXT,
            do_not_translate INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            game_specific TEXT,
            UNIQUE(term_source, domain, game_specific)
        )
    )");
    if (!result) return result;

    // Glossary alternatives table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS glossary_alternatives (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            glossary_id INTEGER NOT NULL,
            alternative TEXT NOT NULL,
            context TEXT,
            FOREIGN KEY (glossary_id) REFERENCES glossary(id) ON DELETE CASCADE
        )
    )");
    if (!result) return result;

    // Glossary forbidden table
    result = execute(R"(
        CREATE TABLE IF NOT EXISTS glossary_forbidden (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            glossary_id INTEGER NOT NULL,
            forbidden_translation TEXT NOT NULL,
            reason TEXT,
            FOREIGN KEY (glossary_id) REFERENCES glossary(id) ON DELETE CASCADE
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
        "CREATE INDEX IF NOT EXISTS idx_translation_cache_hash ON translation_cache(string_hash)",
        "CREATE INDEX IF NOT EXISTS idx_tm_hash ON translation_memory(source_hash)",
        "CREATE INDEX IF NOT EXISTS idx_tm_game ON translation_memory(game_id)",
        "CREATE INDEX IF NOT EXISTS idx_tm_engine ON translation_memory(engine_type)",
        "CREATE INDEX IF NOT EXISTS idx_tm_category ON translation_memory(category)",
        "CREATE INDEX IF NOT EXISTS idx_ngrams ON tm_ngrams(ngram)",
        "CREATE INDEX IF NOT EXISTS idx_ngrams_tm ON tm_ngrams(tm_id)",
        "CREATE INDEX IF NOT EXISTS idx_projects_game ON translation_projects(game_id)",
        "CREATE INDEX IF NOT EXISTS idx_projects_status ON translation_projects(status)",
        "CREATE INDEX IF NOT EXISTS idx_entries_project ON translation_entries(project_id)",
        "CREATE INDEX IF NOT EXISTS idx_entries_status ON translation_entries(status)",
        "CREATE INDEX IF NOT EXISTS idx_entries_category ON translation_entries(category)",
        "CREATE INDEX IF NOT EXISTS idx_entries_project_file ON translation_entries(project_id, file_path)",
        "CREATE INDEX IF NOT EXISTS idx_entries_project_status ON translation_entries(project_id, status)",
        "CREATE INDEX IF NOT EXISTS idx_entries_file_path ON translation_entries(file_path)",
        "CREATE INDEX IF NOT EXISTS idx_glossary_source ON glossary(term_source)",
        "CREATE INDEX IF NOT EXISTS idx_glossary_domain ON glossary(domain)",
        "CREATE INDEX IF NOT EXISTS idx_glossary_game ON glossary(game_specific)",
        "CREATE INDEX IF NOT EXISTS idx_backups_game ON backups(game_id)"
    };

    for (const auto& sql : indexes) {
        result = execute(sql);
        if (!result) {
            MAKINEAI_LOG_WARN(log::DATABASE, "Failed to create index: {}", sql);
            // Continue with other indexes
        }
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "Database tables created successfully (v{})", DATABASE_VERSION);
    return {};
}

Result<void> Database::migrateToV2() {
    MAKINEAI_LOG_INFO(log::DATABASE, "Migrating database to v2...");

    // v2 adds: translation_memory, tm_ngrams, tm_variants, translation_projects,
    // translation_entries, expanded glossary, backups

    // These tables might not exist, create them
    auto result = createTables();
    if (!result) {
        MAKINEAI_LOG_ERROR(log::DATABASE, "Migration to v2 failed");
        return result;
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "v2 migration completed");
    return {};
}

// ============== HELPER METHODS ==============

Result<void> Database::execute(const std::string& sql) {
    auto timer = Metrics::instance().timer("db_query");
    auto startTime = std::chrono::steady_clock::now();

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    // Track all queries
    Metrics::instance().increment("db_queries_total");
    Metrics::instance().recordHistogram("db_query_duration_ms", elapsed.count());

    if (rc != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "SQL error: {}", error);
        return std::unexpected(Error{ErrorCode::IOError, "SQL error: " + error});
    }

    // Warn about slow queries (>100ms)
    if (elapsed.count() > 100) {
        MAKINEAI_LOG_WARN(log::DATABASE, "Slow query detected ({}ms)", elapsed.count());
    }

    return {};
}

Result<Database::Statement> Database::prepare(const std::string& sql) {
    Statement stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt.stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to prepare statement: {}", error);
        Metrics::instance().increment("db_query_errors");
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to prepare statement: " + error});
    }
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Prepared statement");
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
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Saving game: {}", game.name);

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
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to save game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to save game: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Game saved successfully: {}", game.name);
    return {};
}

Result<std::optional<GameInfo>> Database::getGameBySteamId(const std::string& steamAppId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Looking up game by Steam ID");

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
    Metrics::instance().increment("db_queries_total");

    if (rc == SQLITE_DONE) {
        MAKINEAI_LOG_DEBUG(log::DATABASE, "Game not found by Steam ID");
        return std::nullopt;  // Not found
    }

    if (rc != SQLITE_ROW) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to query game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to query game: " + std::string(sqlite3_errmsg(db_))});
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

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found game: {}", game.name);
    return game;
}

Result<std::optional<GameInfo>> Database::getGameById(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Looking up game by ID");

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
    Metrics::instance().increment("db_queries_total");

    if (rc == SQLITE_DONE) {
        MAKINEAI_LOG_DEBUG(log::DATABASE, "Game not found by ID");
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to query game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to query game: " + std::string(sqlite3_errmsg(db_))});
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

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found game: {}", game.name);
    return game;
}

Result<std::vector<GameInfo>> Database::getAllGames() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting all games");

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

    Metrics::instance().increment("db_queries_total");
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Retrieved {} games", games.size());
    return games;
}

Result<void> Database::deleteGame(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Deleting game");

    const char* sql = "DELETE FROM games WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to delete game: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to delete game: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Game deleted successfully");
    return {};
}

// ============== TRANSLATION MEMORY OPERATIONS ==============

Result<int64_t> Database::addToTranslationMemory(const TranslationMemoryEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Adding entry to translation memory");

    const char* sql = R"(
        INSERT OR REPLACE INTO translation_memory (
            source_text, target_text, source_hash, source_lang, target_lang,
            context, game_id, engine_type, category, quality_score,
            usage_count, created_at, updated_at, created_by, verified
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int64_t timestamp = now();

    sqlite3_bind_text(stmt.stmt, 1, entry.sourceText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, entry.targetText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 3, entry.sourceHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 4, entry.sourceLang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 5, entry.targetLang.c_str(), -1, SQLITE_TRANSIENT);

    if (entry.context) {
        sqlite3_bind_text(stmt.stmt, 6, entry.context->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 6);
    }

    if (entry.gameId) {
        sqlite3_bind_text(stmt.stmt, 7, entry.gameId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 7);
    }

    if (entry.engineType) {
        sqlite3_bind_text(stmt.stmt, 8, entry.engineType->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 8);
    }

    if (entry.category) {
        sqlite3_bind_text(stmt.stmt, 9, entry.category->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 9);
    }

    sqlite3_bind_int(stmt.stmt, 10, entry.qualityScore);
    sqlite3_bind_int(stmt.stmt, 11, entry.usageCount);
    sqlite3_bind_int64(stmt.stmt, 12, timestamp);
    sqlite3_bind_int64(stmt.stmt, 13, timestamp);

    if (entry.createdBy) {
        sqlite3_bind_text(stmt.stmt, 14, entry.createdBy->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 14);
    }

    sqlite3_bind_int(stmt.stmt, 15, entry.verified ? 1 : 0);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to add TM entry: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add TM entry: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "TM entry added successfully");
    return sqlite3_last_insert_rowid(db_);
}

Result<std::optional<TranslationMemoryEntry>> Database::findExactMatch(const std::string& sourceHash) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Finding exact TM match");

    const char* sql = R"(
        SELECT id, source_text, target_text, source_hash, source_lang, target_lang,
               context, game_id, engine_type, category, quality_score,
               usage_count, created_at, updated_at, created_by, verified
        FROM translation_memory WHERE source_hash = ?
        ORDER BY quality_score DESC LIMIT 1
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, sourceHash.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc == SQLITE_DONE) {
        MAKINEAI_LOG_DEBUG(log::DATABASE, "No exact TM match found");
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to query TM: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to query TM: " + std::string(sqlite3_errmsg(db_))});
    }

    TranslationMemoryEntry entry;
    entry.id = sqlite3_column_int64(stmt.stmt, 0);
    entry.sourceText = getRequiredText(stmt.stmt, 1);
    entry.targetText = getRequiredText(stmt.stmt, 2);
    entry.sourceHash = getRequiredText(stmt.stmt, 3);
    entry.sourceLang = getRequiredText(stmt.stmt, 4);
    entry.targetLang = getRequiredText(stmt.stmt, 5);
    entry.context = getTextColumn(stmt.stmt, 6);
    entry.gameId = getTextColumn(stmt.stmt, 7);
    entry.engineType = getTextColumn(stmt.stmt, 8);
    entry.category = getTextColumn(stmt.stmt, 9);
    entry.qualityScore = sqlite3_column_int(stmt.stmt, 10);
    entry.usageCount = sqlite3_column_int(stmt.stmt, 11);
    entry.createdAt = sqlite3_column_int64(stmt.stmt, 12);
    entry.updatedAt = sqlite3_column_int64(stmt.stmt, 13);
    entry.createdBy = getTextColumn(stmt.stmt, 14);
    entry.verified = sqlite3_column_int(stmt.stmt, 15) != 0;

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found exact TM match");
    return entry;
}

Result<std::vector<TranslationMemoryEntry>> Database::findByHash(const std::string& sourceHash) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Finding TM entries by hash");

    const char* sql = R"(
        SELECT id, source_text, target_text, source_hash, source_lang, target_lang,
               context, game_id, engine_type, category, quality_score,
               usage_count, created_at, updated_at, created_by, verified
        FROM translation_memory WHERE source_hash = ?
        ORDER BY quality_score DESC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, sourceHash.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<TranslationMemoryEntry> entries;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationMemoryEntry entry;
        entry.id = sqlite3_column_int64(stmt.stmt, 0);
        entry.sourceText = getRequiredText(stmt.stmt, 1);
        entry.targetText = getRequiredText(stmt.stmt, 2);
        entry.sourceHash = getRequiredText(stmt.stmt, 3);
        entry.sourceLang = getRequiredText(stmt.stmt, 4);
        entry.targetLang = getRequiredText(stmt.stmt, 5);
        entry.context = getTextColumn(stmt.stmt, 6);
        entry.gameId = getTextColumn(stmt.stmt, 7);
        entry.engineType = getTextColumn(stmt.stmt, 8);
        entry.category = getTextColumn(stmt.stmt, 9);
        entry.qualityScore = sqlite3_column_int(stmt.stmt, 10);
        entry.usageCount = sqlite3_column_int(stmt.stmt, 11);
        entry.createdAt = sqlite3_column_int64(stmt.stmt, 12);
        entry.updatedAt = sqlite3_column_int64(stmt.stmt, 13);
        entry.createdBy = getTextColumn(stmt.stmt, 14);
        entry.verified = sqlite3_column_int(stmt.stmt, 15) != 0;

        entries.push_back(std::move(entry));
    }

    Metrics::instance().increment("db_queries_total");
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found {} TM entries by hash", entries.size());
    return entries;
}

Result<std::vector<TranslationMemoryEntry>> Database::getEntriesForGame(
    const std::string& gameId, size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting TM entries for game");

    const char* sql = R"(
        SELECT id, source_text, target_text, source_hash, source_lang, target_lang,
               context, game_id, engine_type, category, quality_score,
               usage_count, created_at, updated_at, created_by, verified
        FROM translation_memory WHERE game_id = ?
        ORDER BY updated_at DESC LIMIT ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 2, static_cast<int64_t>(limit));

    std::vector<TranslationMemoryEntry> entries;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationMemoryEntry entry;
        entry.id = sqlite3_column_int64(stmt.stmt, 0);
        entry.sourceText = getRequiredText(stmt.stmt, 1);
        entry.targetText = getRequiredText(stmt.stmt, 2);
        entry.sourceHash = getRequiredText(stmt.stmt, 3);
        entry.sourceLang = getRequiredText(stmt.stmt, 4);
        entry.targetLang = getRequiredText(stmt.stmt, 5);
        entry.context = getTextColumn(stmt.stmt, 6);
        entry.gameId = getTextColumn(stmt.stmt, 7);
        entry.engineType = getTextColumn(stmt.stmt, 8);
        entry.category = getTextColumn(stmt.stmt, 9);
        entry.qualityScore = sqlite3_column_int(stmt.stmt, 10);
        entry.usageCount = sqlite3_column_int(stmt.stmt, 11);
        entry.createdAt = sqlite3_column_int64(stmt.stmt, 12);
        entry.updatedAt = sqlite3_column_int64(stmt.stmt, 13);
        entry.createdBy = getTextColumn(stmt.stmt, 14);
        entry.verified = sqlite3_column_int(stmt.stmt, 15) != 0;

        entries.push_back(std::move(entry));
    }

    Metrics::instance().increment("db_queries_total");
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found {} TM entries for game", entries.size());
    return entries;
}

Result<std::pair<int64_t, int64_t>> Database::getTranslationMemoryStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting TM stats");

    int64_t total = 0;
    int64_t verified = 0;

    {
        auto stmtResult = prepare("SELECT COUNT(*) FROM translation_memory");
        if (!stmtResult) return std::unexpected(stmtResult.error());
        auto& stmt = *stmtResult;
        if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            total = sqlite3_column_int64(stmt.stmt, 0);
        }
    }

    {
        auto stmtResult = prepare("SELECT COUNT(*) FROM translation_memory WHERE verified = 1");
        if (!stmtResult) return std::unexpected(stmtResult.error());
        auto& stmt = *stmtResult;
        if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            verified = sqlite3_column_int64(stmt.stmt, 0);
        }
    }

    Metrics::instance().increment("db_queries_total", 2);
    MAKINEAI_LOG_DEBUG(log::DATABASE, "TM stats: {} total, {} verified", total, verified);
    return std::make_pair(total, verified);
}

Result<double> Database::getAverageQualityScore() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting average quality score");

    const char* sql = "SELECT AVG(quality_score) FROM translation_memory WHERE quality_score > 0";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        // AVG returns NULL if no rows, check for that
        if (sqlite3_column_type(stmt.stmt, 0) != SQLITE_NULL) {
            Metrics::instance().increment("db_queries_total");
            return sqlite3_column_double(stmt.stmt, 0);
        }
    }

    Metrics::instance().increment("db_queries_total");
    return 0.0;  // No entries with quality scores
}

Result<void> Database::incrementTMUsage(int64_t entryId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Incrementing TM usage");

    const char* sql = R"(
        UPDATE translation_memory
        SET usage_count = usage_count + 1, updated_at = ?
        WHERE id = ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, now());
    sqlite3_bind_int64(stmt.stmt, 2, entryId);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to increment TM usage: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to increment TM usage: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<void> Database::updateTranslationMemoryEntry(
    int64_t tmId,
    const std::string& targetText,
    std::optional<int> qualityScore,
    std::optional<bool> verified
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Updating TM entry");

    // Build dynamic SQL based on which fields are being updated
    std::string sql = "UPDATE translation_memory SET target_text = ?, updated_at = ?";

    if (qualityScore.has_value()) {
        sql += ", quality_score = ?";
    }
    if (verified.has_value()) {
        sql += ", verified = ?";
    }

    sql += " WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int paramIndex = 1;
    sqlite3_bind_text(stmt.stmt, paramIndex++, targetText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, paramIndex++, now());

    if (qualityScore.has_value()) {
        sqlite3_bind_int(stmt.stmt, paramIndex++, *qualityScore);
    }
    if (verified.has_value()) {
        sqlite3_bind_int(stmt.stmt, paramIndex++, *verified ? 1 : 0);
    }

    sqlite3_bind_int64(stmt.stmt, paramIndex, tmId);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to update TM entry: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to update TM entry: " + std::string(sqlite3_errmsg(db_))});
    }

    if (sqlite3_changes(db_) == 0) {
        MAKINEAI_LOG_WARN(log::DATABASE, "TM entry not found: {}", tmId);
        return std::unexpected(Error{ErrorCode::NotFound,
            "TM entry not found: " + std::to_string(tmId)});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "TM entry updated successfully");
    return {};
}

Result<void> Database::deleteTranslationMemoryEntry(int64_t tmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Deleting TM entry");

    // Delete n-grams first (foreign key constraint)
    const char* deleteNgramsSql = "DELETE FROM tm_ngrams WHERE tm_id = ?";
    auto ngramStmt = prepare(deleteNgramsSql);
    if (ngramStmt) {
        sqlite3_bind_int64(ngramStmt->stmt, 1, tmId);
        sqlite3_step(ngramStmt->stmt);
    }

    // Delete variants
    const char* deleteVariantsSql = "DELETE FROM tm_variants WHERE tm_id = ?";
    auto variantStmt = prepare(deleteVariantsSql);
    if (variantStmt) {
        sqlite3_bind_int64(variantStmt->stmt, 1, tmId);
        sqlite3_step(variantStmt->stmt);
    }

    // Delete the main entry
    const char* deleteTmSql = "DELETE FROM translation_memory WHERE id = ?";
    auto tmStmt = prepare(deleteTmSql);
    if (!tmStmt) return std::unexpected(tmStmt.error());

    sqlite3_bind_int64(tmStmt->stmt, 1, tmId);

    int rc = sqlite3_step(tmStmt->stmt);
    Metrics::instance().increment("db_queries_total", 3);  // ngrams, variants, and main entry

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to delete TM entry: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to delete TM entry: " + std::string(sqlite3_errmsg(db_))});
    }

    if (sqlite3_changes(db_) == 0) {
        MAKINEAI_LOG_WARN(log::DATABASE, "TM entry not found: {}", tmId);
        return std::unexpected(Error{ErrorCode::NotFound,
            "TM entry not found: " + std::to_string(tmId)});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "TM entry deleted successfully");
    return {};
}

Result<TranslationMemoryEntry> Database::getTranslationMemoryEntryById(int64_t tmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting TM entry by ID");

    const char* sql = R"(
        SELECT id, source_text, target_text, source_lang, target_lang,
               source_hash, game_id, engine_type, file_path, context,
               quality_score, verified, usage_count, created_at, updated_at
        FROM translation_memory WHERE id = ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, tmId);

    Metrics::instance().increment("db_queries_total");

    if (sqlite3_step(stmt.stmt) != SQLITE_ROW) {
        MAKINEAI_LOG_WARN(log::DATABASE, "TM entry not found: {}", tmId);
        return std::unexpected(Error{ErrorCode::NotFound,
            "TM entry not found: " + std::to_string(tmId)});
    }

    TranslationMemoryEntry entry;
    entry.id = sqlite3_column_int64(stmt.stmt, 0);
    entry.sourceText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 1));
    entry.targetText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 2));
    entry.sourceLang = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 3));
    entry.targetLang = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 4));
    entry.sourceHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.stmt, 5));

    auto gameIdPtr = sqlite3_column_text(stmt.stmt, 6);
    if (gameIdPtr) entry.gameId = reinterpret_cast<const char*>(gameIdPtr);

    auto enginePtr = sqlite3_column_text(stmt.stmt, 7);
    if (enginePtr) entry.engineType = reinterpret_cast<const char*>(enginePtr);

    auto filePtr = sqlite3_column_text(stmt.stmt, 8);
    if (filePtr) entry.filePath = reinterpret_cast<const char*>(filePtr);

    auto ctxPtr = sqlite3_column_text(stmt.stmt, 9);
    if (ctxPtr) entry.context = reinterpret_cast<const char*>(ctxPtr);

    entry.qualityScore = sqlite3_column_int(stmt.stmt, 10);
    entry.verified = sqlite3_column_int(stmt.stmt, 11) != 0;
    entry.usageCount = sqlite3_column_int(stmt.stmt, 12);
    entry.createdAt = sqlite3_column_int64(stmt.stmt, 13);
    entry.updatedAt = sqlite3_column_int64(stmt.stmt, 14);

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found TM entry by ID");
    return entry;
}

// ============== N-GRAM OPERATIONS ==============

Result<void> Database::storeNgrams(int64_t tmId, const std::vector<std::pair<std::string, int>>& ngrams) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Storing {} n-grams", ngrams.size());

    const char* sql = "INSERT INTO tm_ngrams (tm_id, ngram, position) VALUES (?, ?, ?)";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    for (const auto& [ngram, position] : ngrams) {
        sqlite3_reset(stmt.stmt);
        sqlite3_bind_int64(stmt.stmt, 1, tmId);
        sqlite3_bind_text(stmt.stmt, 2, ngram.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.stmt, 3, position);

        int rc = sqlite3_step(stmt.stmt);
        if (rc != SQLITE_DONE) {
            Metrics::instance().increment("db_query_errors");
            MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to store n-gram: {}", sqlite3_errmsg(db_));
            return std::unexpected(Error{ErrorCode::IOError,
                "Failed to store n-gram: " + std::string(sqlite3_errmsg(db_))});
        }
    }

    Metrics::instance().increment("db_queries_total", static_cast<int64_t>(ngrams.size()));
    MAKINEAI_LOG_DEBUG(log::DATABASE, "N-grams stored successfully");
    return {};
}

Result<std::vector<int64_t>> Database::findByNgram(const std::string& ngram, size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Finding TM entries by n-gram");

    const char* sql = R"(
        SELECT DISTINCT tm_id FROM tm_ngrams
        WHERE ngram = ? LIMIT ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, ngram.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 2, static_cast<int64_t>(limit));

    std::vector<int64_t> ids;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int64(stmt.stmt, 0));
    }

    Metrics::instance().increment("db_queries_total");
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Found {} TM entries by n-gram", ids.size());
    return ids;
}

// ============== SETTINGS OPERATIONS ==============

Result<void> Database::setSetting(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Setting config key");

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
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to set setting: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to set setting: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<std::optional<std::string>> Database::getSetting(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting config key");

    const char* sql = "SELECT value FROM settings WHERE key = ? LIMIT 1";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc == SQLITE_DONE) {
        MAKINEAI_LOG_DEBUG(log::DATABASE, "Setting not found");
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to get setting: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to get setting: " + std::string(sqlite3_errmsg(db_))});
    }

    return getRequiredText(stmt.stmt, 0);
}

Result<std::map<std::string, std::string>> Database::getAllSettings() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting all settings");

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

    Metrics::instance().increment("db_queries_total");
    MAKINEAI_LOG_DEBUG(log::DATABASE, "Retrieved {} settings", settings.size());
    return settings;
}

// ============== GLOSSARY OPERATIONS ==============

Result<int64_t> Database::addGlossaryTerm(const GlossaryTerm& term) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Adding glossary term");

    const char* sql = R"(
        INSERT OR REPLACE INTO glossary (
            term_source, term_target, term_type, domain,
            case_sensitive, exact_match, priority, notes, examples,
            do_not_translate, created_at, game_specific
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, term.termSource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, term.termTarget.c_str(), -1, SQLITE_TRANSIENT);

    if (term.termType) {
        // Convert TermType to string - simplified
        sqlite3_bind_text(stmt.stmt, 3, "other", -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 3);
    }

    if (term.domain) {
        sqlite3_bind_text(stmt.stmt, 4, domainToDbString(*term.domain).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 4);
    }

    sqlite3_bind_int(stmt.stmt, 5, term.caseSensitive ? 1 : 0);
    sqlite3_bind_int(stmt.stmt, 6, term.exactMatch ? 1 : 0);
    sqlite3_bind_int(stmt.stmt, 7, term.priority);

    if (term.notes) {
        sqlite3_bind_text(stmt.stmt, 8, term.notes->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 8);
    }

    // Examples as JSON array
    if (!term.examples.empty()) {
        std::string json = "[";
        for (size_t i = 0; i < term.examples.size(); ++i) {
            if (i > 0) json += ",";
            json += "\"" + term.examples[i] + "\"";
        }
        json += "]";
        sqlite3_bind_text(stmt.stmt, 9, json.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 9);
    }

    sqlite3_bind_int(stmt.stmt, 10, term.doNotTranslate ? 1 : 0);
    sqlite3_bind_int64(stmt.stmt, 11, term.createdAt > 0 ? term.createdAt : now());

    if (term.gameSpecific) {
        sqlite3_bind_text(stmt.stmt, 12, term.gameSpecific->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 12);
    }

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to add glossary term: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add glossary term: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Glossary term added successfully");
    return sqlite3_last_insert_rowid(db_);
}

Result<void> Database::updateGlossaryTerm(const GlossaryTerm& term) {
    if (!term.id) {
        MAKINEAI_LOG_WARN(log::DATABASE, "Term ID is required for update");
        return std::unexpected(Error{ErrorCode::InvalidArgument, "Term ID is required for update"});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Updating glossary term");

    // For simplicity, delete and re-add
    auto deleteResult = deleteGlossaryTerm(*term.id);
    if (!deleteResult) return deleteResult;

    auto addResult = addGlossaryTerm(term);
    if (!addResult) return std::unexpected(addResult.error());

    return {};
}

Result<void> Database::deleteGlossaryTerm(int64_t termId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Deleting glossary term");

    const char* sql = "DELETE FROM glossary WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, termId);

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to delete glossary term: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to delete glossary term: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Glossary term deleted successfully");
    return {};
}

Result<std::vector<GlossaryTerm>> Database::searchGlossary(
    const std::string& searchTerm,
    const std::optional<TermDomain>& domain,
    const std::optional<std::string>& gameSpecific,
    size_t limit) {

    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Searching glossary");

    std::string sql = R"(
        SELECT id, term_source, term_target, term_type, domain,
               case_sensitive, exact_match, priority, notes, examples,
               do_not_translate, created_at, game_specific
        FROM glossary WHERE term_source LIKE ?
    )";

    std::vector<std::string> params;
    params.push_back("%" + searchTerm + "%");

    if (domain) {
        sql += " AND (domain = ? OR domain IS NULL)";
        params.push_back(domainToDbString(*domain));
    }

    if (gameSpecific) {
        sql += " AND (game_specific = ? OR game_specific IS NULL)";
        params.push_back(*gameSpecific);
    }

    sql += " ORDER BY priority DESC, term_source ASC LIMIT ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int paramIndex = 1;
    for (const auto& param : params) {
        sqlite3_bind_text(stmt.stmt, paramIndex++, param.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt.stmt, paramIndex, static_cast<int64_t>(limit));

    std::vector<GlossaryTerm> terms;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        GlossaryTerm term;
        term.id = sqlite3_column_int64(stmt.stmt, 0);
        term.termSource = getRequiredText(stmt.stmt, 1);
        term.termTarget = getRequiredText(stmt.stmt, 2);
        // term_type parsing simplified
        auto domainStr = getTextColumn(stmt.stmt, 4);
        if (domainStr) {
            term.domain = stringToDomain(*domainStr);
        }
        term.caseSensitive = sqlite3_column_int(stmt.stmt, 5) != 0;
        term.exactMatch = sqlite3_column_int(stmt.stmt, 6) != 0;
        term.priority = sqlite3_column_int(stmt.stmt, 7);
        term.notes = getTextColumn(stmt.stmt, 8);
        // examples JSON parsing - simplified, just store raw
        term.doNotTranslate = sqlite3_column_int(stmt.stmt, 10) != 0;
        term.createdAt = sqlite3_column_int64(stmt.stmt, 11);
        term.gameSpecific = getTextColumn(stmt.stmt, 12);

        terms.push_back(std::move(term));
    }

    return terms;
}

Result<std::vector<GlossaryTerm>> Database::getAllGlossaryTerms(
    const std::optional<TermDomain>& domain,
    const std::optional<std::string>& gameSpecific) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = R"(
        SELECT id, term_source, term_target, term_type, domain,
               case_sensitive, exact_match, priority, notes, examples,
               do_not_translate, created_at, game_specific
        FROM glossary WHERE 1=1
    )";

    std::vector<std::string> params;

    if (domain) {
        sql += " AND domain = ?";
        params.push_back(domainToDbString(*domain));
    }

    if (gameSpecific) {
        sql += " AND game_specific = ?";
        params.push_back(*gameSpecific);
    }

    sql += " ORDER BY priority DESC, term_source ASC";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int paramIndex = 1;
    for (const auto& param : params) {
        sqlite3_bind_text(stmt.stmt, paramIndex++, param.c_str(), -1, SQLITE_TRANSIENT);
    }

    std::vector<GlossaryTerm> terms;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        GlossaryTerm term;
        term.id = sqlite3_column_int64(stmt.stmt, 0);
        term.termSource = getRequiredText(stmt.stmt, 1);
        term.termTarget = getRequiredText(stmt.stmt, 2);
        auto domainStr = getTextColumn(stmt.stmt, 4);
        if (domainStr) {
            term.domain = stringToDomain(*domainStr);
        }
        term.caseSensitive = sqlite3_column_int(stmt.stmt, 5) != 0;
        term.exactMatch = sqlite3_column_int(stmt.stmt, 6) != 0;
        term.priority = sqlite3_column_int(stmt.stmt, 7);
        term.notes = getTextColumn(stmt.stmt, 8);
        term.doNotTranslate = sqlite3_column_int(stmt.stmt, 10) != 0;
        term.createdAt = sqlite3_column_int64(stmt.stmt, 11);
        term.gameSpecific = getTextColumn(stmt.stmt, 12);

        terms.push_back(std::move(term));
    }

    return terms;
}

Result<void> Database::addGlossaryAlternative(
    int64_t glossaryId,
    const std::string& alternative,
    const std::optional<std::string>& context) {

    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO glossary_alternatives (glossary_id, alternative, context)
        VALUES (?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, glossaryId);
    sqlite3_bind_text(stmt.stmt, 2, alternative.c_str(), -1, SQLITE_TRANSIENT);

    if (context) {
        sqlite3_bind_text(stmt.stmt, 3, context->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 3);
    }

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add glossary alternative: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<void> Database::addForbiddenTranslation(
    int64_t glossaryId,
    const std::string& forbidden,
    const std::optional<std::string>& reason) {

    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT INTO glossary_forbidden (glossary_id, forbidden_translation, reason)
        VALUES (?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, glossaryId);
    sqlite3_bind_text(stmt.stmt, 2, forbidden.c_str(), -1, SQLITE_TRANSIENT);

    if (reason) {
        sqlite3_bind_text(stmt.stmt, 3, reason->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 3);
    }

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add forbidden translation: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

// ============== PROJECT OPERATIONS ==============

Result<std::string> Database::createProject(const TranslationProject& project) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Creating translation project: {}", project.name);

    std::string projectId = project.id.empty() ? generateId("proj") : project.id;
    int64_t timestamp = now();

    const char* sql = R"(
        INSERT INTO translation_projects (
            id, game_id, name, source_lang, target_lang,
            created_at, updated_at, status, progress, settings
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);

    if (project.gameId) {
        sqlite3_bind_text(stmt.stmt, 2, project.gameId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 2);
    }

    sqlite3_bind_text(stmt.stmt, 3, project.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 4, project.sourceLang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 5, project.targetLang.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 6, timestamp);
    sqlite3_bind_int64(stmt.stmt, 7, timestamp);
    sqlite3_bind_text(stmt.stmt, 8, projectStatusToDbString(project.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt.stmt, 9, project.progress);

    if (project.settings) {
        sqlite3_bind_text(stmt.stmt, 10, project.settings->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 10);
    }

    int rc = sqlite3_step(stmt.stmt);
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to create project: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to create project: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "Translation project created: {}", project.name);
    return projectId;
}

Result<std::optional<TranslationProject>> Database::getProject(const std::string& projectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting translation project");

    const char* sql = R"(
        SELECT id, game_id, name, source_lang, target_lang,
               created_at, updated_at, status, progress, settings
        FROM translation_projects WHERE id = ? LIMIT 1
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    if (rc != SQLITE_ROW) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to query project: " + std::string(sqlite3_errmsg(db_))});
    }

    TranslationProject project;
    project.id = getRequiredText(stmt.stmt, 0);
    project.gameId = getTextColumn(stmt.stmt, 1);
    project.name = getRequiredText(stmt.stmt, 2);
    project.sourceLang = getRequiredText(stmt.stmt, 3);
    project.targetLang = getRequiredText(stmt.stmt, 4);
    project.createdAt = sqlite3_column_int64(stmt.stmt, 5);
    project.updatedAt = sqlite3_column_int64(stmt.stmt, 6);
    project.status = stringToProjectStatus(getRequiredText(stmt.stmt, 7));
    project.progress = sqlite3_column_double(stmt.stmt, 8);
    project.settings = getTextColumn(stmt.stmt, 9);

    return project;
}

Result<std::vector<TranslationProject>> Database::getAllProjects() {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        SELECT id, game_id, name, source_lang, target_lang,
               created_at, updated_at, status, progress, settings
        FROM translation_projects ORDER BY updated_at DESC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    std::vector<TranslationProject> projects;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationProject project;
        project.id = getRequiredText(stmt.stmt, 0);
        project.gameId = getTextColumn(stmt.stmt, 1);
        project.name = getRequiredText(stmt.stmt, 2);
        project.sourceLang = getRequiredText(stmt.stmt, 3);
        project.targetLang = getRequiredText(stmt.stmt, 4);
        project.createdAt = sqlite3_column_int64(stmt.stmt, 5);
        project.updatedAt = sqlite3_column_int64(stmt.stmt, 6);
        project.status = stringToProjectStatus(getRequiredText(stmt.stmt, 7));
        project.progress = sqlite3_column_double(stmt.stmt, 8);
        project.settings = getTextColumn(stmt.stmt, 9);

        projects.push_back(std::move(project));
    }

    return projects;
}

Result<std::vector<TranslationProject>> Database::getProjectsByGame(const std::string& gameId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        SELECT id, game_id, name, source_lang, target_lang,
               created_at, updated_at, status, progress, settings
        FROM translation_projects WHERE game_id = ?
        ORDER BY created_at DESC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, gameId.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<TranslationProject> projects;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationProject project;
        project.id = getRequiredText(stmt.stmt, 0);
        project.gameId = getTextColumn(stmt.stmt, 1);
        project.name = getRequiredText(stmt.stmt, 2);
        project.sourceLang = getRequiredText(stmt.stmt, 3);
        project.targetLang = getRequiredText(stmt.stmt, 4);
        project.createdAt = sqlite3_column_int64(stmt.stmt, 5);
        project.updatedAt = sqlite3_column_int64(stmt.stmt, 6);
        project.status = stringToProjectStatus(getRequiredText(stmt.stmt, 7));
        project.progress = sqlite3_column_double(stmt.stmt, 8);
        project.settings = getTextColumn(stmt.stmt, 9);

        projects.push_back(std::move(project));
    }

    return projects;
}

Result<void> Database::updateProject(const TranslationProject& project) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE translation_projects SET
            name = ?, status = ?, progress = ?, settings = ?, updated_at = ?
        WHERE id = ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, project.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, projectStatusToDbString(project.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt.stmt, 3, project.progress);

    if (project.settings) {
        sqlite3_bind_text(stmt.stmt, 4, project.settings->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 4);
    }

    sqlite3_bind_int64(stmt.stmt, 5, now());
    sqlite3_bind_text(stmt.stmt, 6, project.id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to update project: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<void> Database::deleteProject(const std::string& projectId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "DELETE FROM translation_projects WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to delete project: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

// ============== ENTRY OPERATIONS ==============

Result<int64_t> Database::saveEntry(const TranslationEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO translation_entries (
            project_id, file_path, entry_key, source_text, target_text,
            context, category, status, qa_score, qa_issues, placeholders,
            line_number, created_at, updated_at, translated_by
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int64_t timestamp = now();

    sqlite3_bind_text(stmt.stmt, 1, entry.projectId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, entry.filePath.c_str(), -1, SQLITE_TRANSIENT);

    if (entry.entryKey) {
        sqlite3_bind_text(stmt.stmt, 3, entry.entryKey->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 3);
    }

    sqlite3_bind_text(stmt.stmt, 4, entry.sourceText.c_str(), -1, SQLITE_TRANSIENT);

    if (entry.targetText) {
        sqlite3_bind_text(stmt.stmt, 5, entry.targetText->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 5);
    }

    if (entry.context) {
        sqlite3_bind_text(stmt.stmt, 6, entry.context->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 6);
    }

    if (entry.category) {
        // Simplified category conversion
        sqlite3_bind_text(stmt.stmt, 7, "other", -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 7);
    }

    sqlite3_bind_text(stmt.stmt, 8, statusToDbString(entry.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.stmt, 9, entry.qaScore);

    // QA issues as JSON - simplified
    sqlite3_bind_null(stmt.stmt, 10);

    // Placeholders as JSON - simplified
    sqlite3_bind_null(stmt.stmt, 11);

    if (entry.lineNumber) {
        sqlite3_bind_int(stmt.stmt, 12, *entry.lineNumber);
    } else {
        sqlite3_bind_null(stmt.stmt, 12);
    }

    sqlite3_bind_int64(stmt.stmt, 13, entry.createdAt > 0 ? entry.createdAt : timestamp);
    sqlite3_bind_int64(stmt.stmt, 14, timestamp);

    if (entry.translatedBy) {
        sqlite3_bind_text(stmt.stmt, 15, entry.translatedBy->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 15);
    }

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to save entry: " + std::string(sqlite3_errmsg(db_))});
    }

    return sqlite3_last_insert_rowid(db_);
}

Result<void> Database::saveEntries(const std::vector<TranslationEntry>& entries) {
    if (entries.empty()) return {};

    auto beginResult = beginTransaction();
    if (!beginResult) return beginResult;

    for (const auto& entry : entries) {
        auto result = saveEntry(entry);
        if (!result) {
            (void)rollbackTransaction();
            return std::unexpected(result.error());
        }
    }

    return commitTransaction();
}

Result<std::vector<TranslationEntry>> Database::getEntriesByProject(
    const std::string& projectId,
    const std::optional<EntryStatus>& status,
    const std::optional<EntryCategory>& category,
    size_t limit,
    size_t offset) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = R"(
        SELECT id, project_id, file_path, entry_key, source_text, target_text,
               context, category, status, qa_score, qa_issues, placeholders,
               line_number, created_at, updated_at, translated_by
        FROM translation_entries WHERE project_id = ?
    )";

    if (status) {
        sql += " AND status = ?";
    }

    if (category) {
        sql += " AND category = ?";
    }

    sql += " ORDER BY file_path ASC, id ASC LIMIT ? OFFSET ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    int paramIndex = 1;
    sqlite3_bind_text(stmt.stmt, paramIndex++, projectId.c_str(), -1, SQLITE_TRANSIENT);

    if (status) {
        sqlite3_bind_text(stmt.stmt, paramIndex++, statusToDbString(*status).c_str(), -1, SQLITE_TRANSIENT);
    }

    if (category) {
        // Simplified
        sqlite3_bind_text(stmt.stmt, paramIndex++, "other", -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_int64(stmt.stmt, paramIndex++, static_cast<int64_t>(limit));
    sqlite3_bind_int64(stmt.stmt, paramIndex, static_cast<int64_t>(offset));

    std::vector<TranslationEntry> entries;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationEntry entry;
        entry.id = sqlite3_column_int64(stmt.stmt, 0);
        entry.projectId = getRequiredText(stmt.stmt, 1);
        entry.filePath = getRequiredText(stmt.stmt, 2);
        entry.entryKey = getTextColumn(stmt.stmt, 3);
        entry.sourceText = getRequiredText(stmt.stmt, 4);
        entry.targetText = getTextColumn(stmt.stmt, 5);
        entry.context = getTextColumn(stmt.stmt, 6);
        // category parsing simplified
        entry.status = stringToStatus(getRequiredText(stmt.stmt, 8));
        entry.qaScore = sqlite3_column_int(stmt.stmt, 9);
        // qa_issues and placeholders parsing simplified
        auto lineNum = sqlite3_column_type(stmt.stmt, 12);
        if (lineNum != SQLITE_NULL) {
            entry.lineNumber = sqlite3_column_int(stmt.stmt, 12);
        }
        entry.createdAt = sqlite3_column_int64(stmt.stmt, 13);
        entry.updatedAt = sqlite3_column_int64(stmt.stmt, 14);
        entry.translatedBy = getTextColumn(stmt.stmt, 15);

        entries.push_back(std::move(entry));
    }

    return entries;
}

Result<std::vector<TranslationEntry>> Database::getEntriesByFile(
    const std::string& projectId,
    const std::string& filePath) {

    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        SELECT id, project_id, file_path, entry_key, source_text, target_text,
               context, category, status, qa_score, qa_issues, placeholders,
               line_number, created_at, updated_at, translated_by
        FROM translation_entries
        WHERE project_id = ? AND file_path = ?
        ORDER BY id ASC
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.stmt, 2, filePath.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<TranslationEntry> entries;

    while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
        TranslationEntry entry;
        entry.id = sqlite3_column_int64(stmt.stmt, 0);
        entry.projectId = getRequiredText(stmt.stmt, 1);
        entry.filePath = getRequiredText(stmt.stmt, 2);
        entry.entryKey = getTextColumn(stmt.stmt, 3);
        entry.sourceText = getRequiredText(stmt.stmt, 4);
        entry.targetText = getTextColumn(stmt.stmt, 5);
        entry.context = getTextColumn(stmt.stmt, 6);
        entry.status = stringToStatus(getRequiredText(stmt.stmt, 8));
        entry.qaScore = sqlite3_column_int(stmt.stmt, 9);
        auto lineNum = sqlite3_column_type(stmt.stmt, 12);
        if (lineNum != SQLITE_NULL) {
            entry.lineNumber = sqlite3_column_int(stmt.stmt, 12);
        }
        entry.createdAt = sqlite3_column_int64(stmt.stmt, 13);
        entry.updatedAt = sqlite3_column_int64(stmt.stmt, 14);
        entry.translatedBy = getTextColumn(stmt.stmt, 15);

        entries.push_back(std::move(entry));
    }

    return entries;
}

Result<void> Database::updateEntry(const TranslationEntry& entry) {
    if (!entry.id) {
        return std::unexpected(Error{ErrorCode::InvalidArgument, "Entry ID is required for update"});
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE translation_entries SET
            target_text = ?, status = ?, qa_score = ?, updated_at = ?, translated_by = ?
        WHERE id = ?
    )";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    if (entry.targetText) {
        sqlite3_bind_text(stmt.stmt, 1, entry.targetText->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 1);
    }

    sqlite3_bind_text(stmt.stmt, 2, statusToDbString(entry.status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.stmt, 3, entry.qaScore);
    sqlite3_bind_int64(stmt.stmt, 4, now());

    if (entry.translatedBy) {
        sqlite3_bind_text(stmt.stmt, 5, entry.translatedBy->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt.stmt, 5);
    }

    sqlite3_bind_int64(stmt.stmt, 6, *entry.id);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to update entry: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<void> Database::updateEntriesStatus(
    const std::vector<int64_t>& entryIds,
    EntryStatus status) {

    if (entryIds.empty()) return {};

    std::lock_guard<std::mutex> lock(mutex_);

    std::string placeholders;
    for (size_t i = 0; i < entryIds.size(); ++i) {
        if (i > 0) placeholders += ",";
        placeholders += "?";
    }

    std::string sql = "UPDATE translation_entries SET status = ?, updated_at = ? WHERE id IN (" +
                      placeholders + ")";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_text(stmt.stmt, 1, statusToDbString(status).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.stmt, 2, now());

    int paramIndex = 3;
    for (int64_t id : entryIds) {
        sqlite3_bind_int64(stmt.stmt, paramIndex++, id);
    }

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to update entries status: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<void> Database::deleteEntry(int64_t entryId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "DELETE FROM translation_entries WHERE id = ?";

    auto stmtResult = prepare(sql);
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_int64(stmt.stmt, 1, entryId);

    int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to delete entry: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

Result<EntryStats> Database::getEntryStats(const std::string& projectId) {
    std::lock_guard<std::mutex> lock(mutex_);

    EntryStats stats;

    // Total count
    {
        auto stmtResult = prepare(
            "SELECT COUNT(*) FROM translation_entries WHERE project_id = ?");
        if (!stmtResult) return std::unexpected(stmtResult.error());
        auto& stmt = *stmtResult;
        sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            stats.total = sqlite3_column_int64(stmt.stmt, 0);
        }
    }

    // Translated count (not untranslated)
    {
        auto stmtResult = prepare(
            "SELECT COUNT(*) FROM translation_entries WHERE project_id = ? AND status != 'untranslated'");
        if (!stmtResult) return std::unexpected(stmtResult.error());
        auto& stmt = *stmtResult;
        sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            stats.translated = sqlite3_column_int64(stmt.stmt, 0);
        }
    }

    // Verified count
    {
        auto stmtResult = prepare(
            "SELECT COUNT(*) FROM translation_entries WHERE project_id = ? AND status = 'verified'");
        if (!stmtResult) return std::unexpected(stmtResult.error());
        auto& stmt = *stmtResult;
        sqlite3_bind_text(stmt.stmt, 1, projectId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            stats.verified = sqlite3_column_int64(stmt.stmt, 0);
        }
    }

    stats.untranslated = stats.total - stats.translated;

    return stats;
}

Result<double> Database::calculateAndUpdateProgress(const std::string& projectId) {
    auto statsResult = getEntryStats(projectId);
    if (!statsResult) return std::unexpected(statsResult.error());

    const auto& stats = *statsResult;
    double progress = stats.total > 0 ?
        static_cast<double>(stats.translated) / static_cast<double>(stats.total) : 0.0;

    // Update project progress
    std::lock_guard<std::mutex> lock(mutex_);

    auto stmtResult = prepare("UPDATE translation_projects SET progress = ? WHERE id = ?");
    if (!stmtResult) return std::unexpected(stmtResult.error());
    auto& stmt = *stmtResult;

    sqlite3_bind_double(stmt.stmt, 1, progress);
    sqlite3_bind_text(stmt.stmt, 2, projectId.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt.stmt);

    return progress;
}

// ============== BACKUP OPERATIONS ==============

Result<void> Database::addBackupRecord(const BackupRecord& backup) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_INFO(log::DATABASE, "Adding backup record for game");

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
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to add backup record: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add backup record: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "Backup record added successfully");
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
            "Failed to get backup: " + std::string(sqlite3_errmsg(db_))});
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
            "Failed to delete backup record: " + std::string(sqlite3_errmsg(db_))});
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
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_INFO(log::DATABASE, "Adding patch record: type={}, status={}, strings={}",
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
    Metrics::instance().increment("db_queries_total");

    if (rc != SQLITE_DONE) {
        Metrics::instance().increment("db_query_errors");
        MAKINEAI_LOG_ERROR(log::DATABASE, "Failed to add patch record: {}", sqlite3_errmsg(db_));
        return std::unexpected(Error{ErrorCode::IOError,
            "Failed to add patch record: " + std::string(sqlite3_errmsg(db_))});
    }

    MAKINEAI_LOG_INFO(log::DATABASE, "Patch record added successfully");
    return sqlite3_last_insert_rowid(db_);
}

Result<std::vector<std::map<std::string, std::string>>> Database::getPatchHistory(
    const std::string& gameId) {

    std::lock_guard<std::mutex> lock(mutex_);
    auto timer = Metrics::instance().timer("db_query");

    MAKINEAI_LOG_DEBUG(log::DATABASE, "Getting patch history");

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
        record["id"] = std::to_string(sqlite3_column_int64(stmt.stmt, 0));
        record["game_id"] = getRequiredText(stmt.stmt, 1);
        record["patch_type"] = getRequiredText(stmt.stmt, 2);
        record["status"] = getRequiredText(stmt.stmt, 3);

        auto backupPath = getTextColumn(stmt.stmt, 4);
        if (backupPath) record["backup_path"] = *backupPath;

        record["strings_patched"] = std::to_string(sqlite3_column_int(stmt.stmt, 5));

        auto errorMsg = getTextColumn(stmt.stmt, 6);
        if (errorMsg) record["error_message"] = *errorMsg;

        record["applied_at"] = std::to_string(sqlite3_column_int64(stmt.stmt, 7));

        if (sqlite3_column_type(stmt.stmt, 8) != SQLITE_NULL) {
            record["reverted_at"] = std::to_string(sqlite3_column_int64(stmt.stmt, 8));
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
            "Failed to mark patch reverted: " + std::string(sqlite3_errmsg(db_))});
    }

    return {};
}

} // namespace makineai
