/**
 * @file database.hpp
 * @brief Makine SQLite database interface
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Thread-safe SQLite database for storing:
 * - Game information
 * - Backup records
 * - Patch history
 * - Application settings
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Forward declaration - sqlite3 is not exposed in the public API
struct sqlite3;
struct sqlite3_stmt;

namespace makine {

/**
 * @brief Database version for migrations
 */
constexpr int DATABASE_VERSION = 2;

/**
 * @brief Database file name
 */
constexpr const char* DATABASE_NAME = "makine.db";

/**
 * @brief SQLite database wrapper with RAII
 *
 * Thread-safe: All operations are protected by an internal mutex.
 */
class Database {
public:
    /**
     * @brief RAII wrapper for prepared statements
     */
    struct Statement {
        sqlite3_stmt* stmt = nullptr;
        ~Statement();
    };

    /**
     * @brief Get singleton instance
     * @return Reference to the database instance
     */
    static Database& instance();

    /**
     * @brief Initialize database (call once at startup)
     * @param dbPath Optional custom path (default: %LOCALAPPDATA%/MakineCeviri/Makine-Launcher/makine.db)
     * @return Success or error
     */
    [[nodiscard]] Result<void> initialize(const std::optional<fs::path>& dbPath = std::nullopt);

    /**
     * @brief Close database connection
     */
    void close();

    /**
     * @brief Check if database is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Get database file path
     */
    [[nodiscard]] fs::path getPath() const noexcept;

    // ============== GAME OPERATIONS ==============

    /**
     * @brief Save or update a game
     * @param game Game information
     * @return Success or error
     */
    [[nodiscard]] Result<void> saveGame(const GameInfo& game);

    /**
     * @brief Get game by Steam App ID
     * @param steamAppId Steam application ID
     * @return Game info or nullopt if not found
     */
    [[nodiscard]] Result<std::optional<GameInfo>> getGameBySteamId(const std::string& steamAppId);

    /**
     * @brief Get game by internal ID
     * @param gameId Internal game ID
     * @return Game info or nullopt if not found
     */
    [[nodiscard]] Result<std::optional<GameInfo>> getGameById(const std::string& gameId);

    /**
     * @brief Get all stored games
     * @return List of games
     */
    [[nodiscard]] Result<std::vector<GameInfo>> getAllGames();

    /**
     * @brief Delete a game and all related data
     * @param gameId Game ID to delete
     * @return Success or error
     */
    [[nodiscard]] Result<void> deleteGame(const std::string& gameId);

    // ============== BACKUP OPERATIONS ==============

    /**
     * @brief Add backup record
     * @param backup Backup record
     * @return Success or error
     */
    [[nodiscard]] Result<void> addBackupRecord(const BackupRecord& backup);

    /**
     * @brief Get backups for a game
     * @param gameId Game ID
     * @return List of backup records
     */
    [[nodiscard]] Result<std::vector<BackupRecord>> getBackupsByGame(const std::string& gameId);

    /**
     * @brief Get backup by ID
     * @param backupId Backup ID
     * @return Backup record or nullopt
     */
    [[nodiscard]] Result<std::optional<BackupRecord>> getBackup(const std::string& backupId);

    /**
     * @brief Mark backup as deleted
     * @param backupId Backup ID
     * @return Success or error
     */
    [[nodiscard]] Result<void> deleteBackupRecord(const std::string& backupId);

    // ============== PATCH HISTORY OPERATIONS ==============

    /**
     * @brief Add patch record
     * @param gameId Game ID
     * @param patchType Type of patch applied
     * @param status Patch status
     * @param backupPath Path to backup
     * @param stringsPatched Number of strings patched
     * @param errorMessage Error message if failed
     * @return Patch record ID or error
     */
    [[nodiscard]] Result<int64_t> addPatchRecord(
        const std::string& gameId,
        const std::string& patchType,
        const std::string& status,
        const std::optional<std::string>& backupPath = std::nullopt,
        int stringsPatched = 0,
        const std::optional<std::string>& errorMessage = std::nullopt
    );

    /**
     * @brief Get patch history for a game
     * @param gameId Game ID
     * @return List of patch records as maps
     */
    [[nodiscard]] Result<std::vector<std::map<std::string, std::string>>> getPatchHistory(
        const std::string& gameId
    );

    /**
     * @brief Mark patch as reverted
     * @param patchId Patch record ID
     * @return Success or error
     */
    [[nodiscard]] Result<void> markPatchReverted(int64_t patchId);

    // ============== SETTINGS OPERATIONS ==============

    /**
     * @brief Set a setting value
     * @param key Setting key
     * @param value Setting value
     * @return Success or error
     */
    [[nodiscard]] Result<void> setSetting(const std::string& key, const std::string& value);

    /**
     * @brief Get a setting value
     * @param key Setting key
     * @return Value or nullopt if not found
     */
    [[nodiscard]] Result<std::optional<std::string>> getSetting(const std::string& key);

    /**
     * @brief Get all settings
     * @return Map of key-value pairs
     */
    [[nodiscard]] Result<std::map<std::string, std::string>> getAllSettings();

    // ============== UTILITY ==============

    /**
     * @brief Execute raw SQL query (for debugging)
     * @param sql SQL statement
     * @return Success or error
     */
    [[nodiscard]] Result<void> executeRaw(const std::string& sql);

    /**
     * @brief Begin transaction
     * @return Success or error
     */
    [[nodiscard]] Result<void> beginTransaction();

    /**
     * @brief Commit transaction
     * @return Success or error
     */
    [[nodiscard]] Result<void> commitTransaction();

    /**
     * @brief Rollback transaction
     * @return Success or error
     */
    [[nodiscard]] Result<void> rollbackTransaction();

    /**
     * @brief Vacuum database to reclaim space
     * @return Success or error
     */
    [[nodiscard]] Result<void> vacuum();

    // Destructor
    ~Database();

    // Non-copyable, non-movable (singleton)
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

private:
    Database() = default;

    // Helper methods
    [[nodiscard]] Result<void> execute(const std::string& sql);
    [[nodiscard]] Result<Statement> prepare(const std::string& sql);
    [[nodiscard]] Result<void> createTables();
    [[nodiscard]] Result<void> migrateToV2();

    // Static helpers
    static int64_t now() noexcept;
    [[nodiscard]] static std::string generateId(const std::string& prefix);

    // Private data - sqlite3 is forward-declared, not exposed
    sqlite3* db_ = nullptr;
    fs::path dbPath_;
    bool initialized_ = false;
    mutable std::mutex mutex_;
};

} // namespace makine
