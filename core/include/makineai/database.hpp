/**
 * @file database.hpp
 * @brief MakineAI SQLite database interface
 * @copyright (c) 2026 MakineAI Team
 *
 * Thread-safe SQLite database for storing:
 * - Game information
 * - Translation memory
 * - Glossary terms
 * - Translation projects and entries
 * - Backup records
 * - Application settings
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <sqlite3.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace makineai {

/**
 * @brief Database version for migrations
 */
constexpr int DATABASE_VERSION = 2;

/**
 * @brief Database file name
 */
constexpr const char* DATABASE_NAME = "makineai.db";

/**
 * @brief SQLite database wrapper with RAII
 */
class Database {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the database instance
     */
    static Database& instance();

    /**
     * @brief Initialize database (call once at startup)
     * @param dbPath Optional custom path (default: %LOCALAPPDATA%/MakineAI/makineai.db)
     * @return Success or error
     */
    Result<void> initialize(const std::optional<fs::path>& dbPath = std::nullopt);

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
    Result<void> saveGame(const GameInfo& game);

    /**
     * @brief Get game by Steam App ID
     * @param steamAppId Steam application ID
     * @return Game info or nullopt if not found
     */
    Result<std::optional<GameInfo>> getGameBySteamId(const std::string& steamAppId);

    /**
     * @brief Get game by internal ID
     * @param gameId Internal game ID
     * @return Game info or nullopt if not found
     */
    Result<std::optional<GameInfo>> getGameById(const std::string& gameId);

    /**
     * @brief Get all stored games
     * @return List of games
     */
    Result<std::vector<GameInfo>> getAllGames();

    /**
     * @brief Delete a game and all related data
     * @param gameId Game ID to delete
     * @return Success or error
     */
    Result<void> deleteGame(const std::string& gameId);

    // ============== TRANSLATION MEMORY OPERATIONS ==============

    /**
     * @brief Add entry to translation memory
     * @param entry TM entry to add
     * @return ID of inserted entry or error
     */
    Result<int64_t> addToTranslationMemory(const TranslationMemoryEntry& entry);

    /**
     * @brief Find exact match in translation memory
     * @param sourceHash Hash of source text
     * @return TM entry or nullopt if not found
     */
    Result<std::optional<TranslationMemoryEntry>> findExactMatch(const std::string& sourceHash);

    /**
     * @brief Find entries by source text hash
     * @param sourceHash Hash of source text
     * @return List of matching entries
     */
    Result<std::vector<TranslationMemoryEntry>> findByHash(const std::string& sourceHash);

    /**
     * @brief Get translation memory entries for a game
     * @param gameId Game ID
     * @param limit Maximum number of entries
     * @return List of TM entries
     */
    Result<std::vector<TranslationMemoryEntry>> getEntriesForGame(
        const std::string& gameId,
        size_t limit = 1000
    );

    /**
     * @brief Get translation memory statistics
     * @return Map with 'total' and 'verified' counts
     */
    Result<std::pair<int64_t, int64_t>> getTranslationMemoryStats();

    /**
     * @brief Update usage count for a TM entry
     * @param entryId TM entry ID
     * @return Success or error
     */
    Result<void> incrementTMUsage(int64_t entryId);

    // ============== N-GRAM OPERATIONS (for fuzzy matching) ==============

    /**
     * @brief Store n-grams for a TM entry
     * @param tmId Translation memory entry ID
     * @param ngrams List of n-grams with positions
     * @return Success or error
     */
    Result<void> storeNgrams(int64_t tmId, const std::vector<std::pair<std::string, int>>& ngrams);

    /**
     * @brief Find TM entries by n-gram
     * @param ngram N-gram to search
     * @param limit Maximum results
     * @return List of TM entry IDs
     */
    Result<std::vector<int64_t>> findByNgram(const std::string& ngram, size_t limit = 100);

    // ============== GLOSSARY OPERATIONS ==============

    /**
     * @brief Add glossary term
     * @param term Glossary term to add
     * @return ID of inserted term or error
     */
    Result<int64_t> addGlossaryTerm(const GlossaryTerm& term);

    /**
     * @brief Update glossary term
     * @param term Term with updated values
     * @return Success or error
     */
    Result<void> updateGlossaryTerm(const GlossaryTerm& term);

    /**
     * @brief Delete glossary term
     * @param termId Term ID to delete
     * @return Success or error
     */
    Result<void> deleteGlossaryTerm(int64_t termId);

    /**
     * @brief Search glossary by source term
     * @param searchTerm Search string
     * @param domain Optional domain filter
     * @param gameSpecific Optional game-specific filter
     * @param limit Maximum results
     * @return List of matching terms
     */
    Result<std::vector<GlossaryTerm>> searchGlossary(
        const std::string& searchTerm,
        const std::optional<TermDomain>& domain = std::nullopt,
        const std::optional<std::string>& gameSpecific = std::nullopt,
        size_t limit = 20
    );

    /**
     * @brief Get all glossary terms
     * @param domain Optional domain filter
     * @param gameSpecific Optional game-specific filter
     * @return List of all terms
     */
    Result<std::vector<GlossaryTerm>> getAllGlossaryTerms(
        const std::optional<TermDomain>& domain = std::nullopt,
        const std::optional<std::string>& gameSpecific = std::nullopt
    );

    /**
     * @brief Add alternative translation for a glossary term
     * @param glossaryId Glossary term ID
     * @param alternative Alternative translation
     * @param context Optional usage context
     * @return Success or error
     */
    Result<void> addGlossaryAlternative(
        int64_t glossaryId,
        const std::string& alternative,
        const std::optional<std::string>& context = std::nullopt
    );

    /**
     * @brief Add forbidden translation for a glossary term
     * @param glossaryId Glossary term ID
     * @param forbidden Forbidden translation
     * @param reason Optional reason why it's forbidden
     * @return Success or error
     */
    Result<void> addForbiddenTranslation(
        int64_t glossaryId,
        const std::string& forbidden,
        const std::optional<std::string>& reason = std::nullopt
    );

    // ============== PROJECT OPERATIONS ==============

    /**
     * @brief Create translation project
     * @param project Project to create
     * @return Project ID or error
     */
    Result<std::string> createProject(const TranslationProject& project);

    /**
     * @brief Get project by ID
     * @param projectId Project ID
     * @return Project or nullopt if not found
     */
    Result<std::optional<TranslationProject>> getProject(const std::string& projectId);

    /**
     * @brief Get all projects
     * @return List of all projects
     */
    Result<std::vector<TranslationProject>> getAllProjects();

    /**
     * @brief Get projects for a game
     * @param gameId Game ID
     * @return List of projects for the game
     */
    Result<std::vector<TranslationProject>> getProjectsByGame(const std::string& gameId);

    /**
     * @brief Update project
     * @param project Project with updated values
     * @return Success or error
     */
    Result<void> updateProject(const TranslationProject& project);

    /**
     * @brief Delete project and all entries
     * @param projectId Project ID to delete
     * @return Success or error
     */
    Result<void> deleteProject(const std::string& projectId);

    // ============== TRANSLATION ENTRY OPERATIONS ==============

    /**
     * @brief Save single translation entry
     * @param entry Entry to save
     * @return Entry ID or error
     */
    Result<int64_t> saveEntry(const TranslationEntry& entry);

    /**
     * @brief Save multiple entries in batch
     * @param entries List of entries
     * @return Success or error
     */
    Result<void> saveEntries(const std::vector<TranslationEntry>& entries);

    /**
     * @brief Get entries by project
     * @param projectId Project ID
     * @param status Optional status filter
     * @param category Optional category filter
     * @param limit Maximum entries
     * @param offset Skip first N entries
     * @return List of entries
     */
    Result<std::vector<TranslationEntry>> getEntriesByProject(
        const std::string& projectId,
        const std::optional<EntryStatus>& status = std::nullopt,
        const std::optional<EntryCategory>& category = std::nullopt,
        size_t limit = 1000,
        size_t offset = 0
    );

    /**
     * @brief Get entries for a specific file
     * @param projectId Project ID
     * @param filePath File path
     * @return List of entries for the file
     */
    Result<std::vector<TranslationEntry>> getEntriesByFile(
        const std::string& projectId,
        const std::string& filePath
    );

    /**
     * @brief Update single entry
     * @param entry Entry with updated values
     * @return Success or error
     */
    Result<void> updateEntry(const TranslationEntry& entry);

    /**
     * @brief Update multiple entries
     * @param entryIds List of entry IDs
     * @param status New status
     * @return Success or error
     */
    Result<void> updateEntriesStatus(
        const std::vector<int64_t>& entryIds,
        EntryStatus status
    );

    /**
     * @brief Delete entry
     * @param entryId Entry ID to delete
     * @return Success or error
     */
    Result<void> deleteEntry(int64_t entryId);

    /**
     * @brief Get entry statistics for a project
     * @return Map with total, translated, verified, untranslated counts
     */
    struct EntryStats {
        int64_t total = 0;
        int64_t translated = 0;
        int64_t verified = 0;
        int64_t untranslated = 0;
    };
    Result<EntryStats> getEntryStats(const std::string& projectId);

    /**
     * @brief Calculate and update project progress
     * @param projectId Project ID
     * @return Progress (0.0 - 1.0) or error
     */
    Result<double> calculateAndUpdateProgress(const std::string& projectId);

    // ============== BACKUP OPERATIONS ==============

    /**
     * @brief Add backup record
     * @param backup Backup record
     * @return Success or error
     */
    Result<void> addBackupRecord(const BackupRecord& backup);

    /**
     * @brief Get backups for a game
     * @param gameId Game ID
     * @return List of backup records
     */
    Result<std::vector<BackupRecord>> getBackupsByGame(const std::string& gameId);

    /**
     * @brief Get backup by ID
     * @param backupId Backup ID
     * @return Backup record or nullopt
     */
    Result<std::optional<BackupRecord>> getBackup(const std::string& backupId);

    /**
     * @brief Mark backup as deleted
     * @param backupId Backup ID
     * @return Success or error
     */
    Result<void> deleteBackupRecord(const std::string& backupId);

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
    Result<int64_t> addPatchRecord(
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
    Result<std::vector<std::map<std::string, std::string>>> getPatchHistory(
        const std::string& gameId
    );

    /**
     * @brief Mark patch as reverted
     * @param patchId Patch record ID
     * @return Success or error
     */
    Result<void> markPatchReverted(int64_t patchId);

    // ============== SETTINGS OPERATIONS ==============

    /**
     * @brief Set a setting value
     * @param key Setting key
     * @param value Setting value
     * @return Success or error
     */
    Result<void> setSetting(const std::string& key, const std::string& value);

    /**
     * @brief Get a setting value
     * @param key Setting key
     * @return Value or nullopt if not found
     */
    Result<std::optional<std::string>> getSetting(const std::string& key);

    /**
     * @brief Get all settings
     * @return Map of key-value pairs
     */
    Result<std::map<std::string, std::string>> getAllSettings();

    // ============== UTILITY ==============

    /**
     * @brief Execute raw SQL query (for debugging)
     * @param sql SQL statement
     * @return Success or error
     */
    Result<void> executeRaw(const std::string& sql);

    /**
     * @brief Begin transaction
     * @return Success or error
     */
    Result<void> beginTransaction();

    /**
     * @brief Commit transaction
     * @return Success or error
     */
    Result<void> commitTransaction();

    /**
     * @brief Rollback transaction
     * @return Success or error
     */
    Result<void> rollbackTransaction();

    /**
     * @brief Vacuum database to reclaim space
     * @return Success or error
     */
    Result<void> vacuum();

private:
    Database() = default;
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Create all tables
    Result<void> createTables();

    // Migration from older versions
    Result<void> migrateToV2();

    // Execute SQL with mutex protection
    Result<void> execute(const std::string& sql);

    // Prepare statement helper
    struct Statement {
        sqlite3_stmt* stmt = nullptr;
        ~Statement();
    };
    Result<Statement> prepare(const std::string& sql);

    // Current timestamp in milliseconds
    static int64_t now() noexcept;

    // Generate unique ID
    static std::string generateId(const std::string& prefix);

private:
    sqlite3* db_ = nullptr;
    fs::path dbPath_;
    std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace makineai
