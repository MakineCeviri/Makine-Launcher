/**
 * @file crash_recovery.hpp
 * @brief Crash recovery journal for install/uninstall operations
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Writes a persistent JSON journal before operations start.
 * If the app crashes mid-operation, the journal survives on disk.
 * On next startup, detect and recover partial state.
 *
 * Pure C++ implementation — no Qt dependency.
 */

#pragma once

#include "makine/types/common.hpp"
#include "makine/error.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace makine::recovery {

/**
 * @brief Type of operation being journaled
 */
enum class OperationType {
    Install,
    Uninstall,
    BackupCreate,
    BackupRestore
};

[[nodiscard]] constexpr std::string_view operationTypeToString(OperationType t) noexcept {
    switch (t) {
        case OperationType::Install:       return "install";
        case OperationType::Uninstall:     return "uninstall";
        case OperationType::BackupCreate:  return "backup_create";
        case OperationType::BackupRestore: return "backup_restore";
    }
    return "unknown";
}

[[nodiscard]] OperationType stringToOperationType(std::string_view s) noexcept;

/**
 * @brief Journal entry describing an in-progress operation
 */
struct JournalEntry {
    OperationType type{OperationType::Install};
    std::string gameId;
    std::string gamePath;           // Target game directory
    std::string backupId;           // For backup operations
    std::string backupPath;         // For backup create: the dir being created
    std::string variant;            // For install: selected variant
    std::vector<std::string> modifiedFiles; // Files already copied/deleted (relative paths)
    int64_t startedAt{0};
};

/**
 * @brief Recovery result for a single operation
 */
struct RecoveryResult {
    bool success{false};
    int filesProcessed{0};
    std::string message;
};

/**
 * @brief Operation journal for crash recovery
 *
 * Thread-safe. Journal is flushed to disk periodically (every N file records)
 * and on begin/commit/abort.
 *
 * Usage:
 * @code
 *   CrashRecoveryJournal journal("/path/to/data");
 *   journal.beginOperation({.type = Install, .gameId = "123", .gamePath = "/game"});
 *   journal.recordFileModified("data/localization.pak");
 *   journal.commitOperation(); // deletes journal file
 * @endcode
 */
class CrashRecoveryJournal {
public:
    /**
     * @brief Construct journal with data directory for persistence
     * @param dataDir Directory where journal file will be stored
     */
    explicit CrashRecoveryJournal(const fs::path& dataDir);
    ~CrashRecoveryJournal() = default;

    // Non-copyable, non-movable (mutex member)
    CrashRecoveryJournal(const CrashRecoveryJournal&) = delete;
    CrashRecoveryJournal& operator=(const CrashRecoveryJournal&) = delete;

    // --- Journal lifecycle (thread-safe) ---

    /**
     * @brief Begin journaling a new operation
     * @return false if another operation is already in progress
     */
    bool beginOperation(const JournalEntry& entry);

    /**
     * @brief Record a file modification (batched writes)
     * Flushes to disk every kFlushInterval files.
     */
    void recordFileModified(const std::string& relativePath);

    /**
     * @brief Mark operation as successfully completed (deletes journal)
     */
    void commitOperation();

    /**
     * @brief Abort operation (deletes journal without recovery)
     */
    void abortOperation();

    /**
     * @brief Check if active operation is in progress
     */
    [[nodiscard]] bool isActive() const;

    // --- Startup recovery ---

    /**
     * @brief Check if a crashed journal file exists from a previous session
     */
    [[nodiscard]] bool hasPendingOperation() const;

    /**
     * @brief Read the pending journal entry
     * @return Entry data, or empty entry if no journal/corrupted
     */
    [[nodiscard]] JournalEntry readPendingOperation() const;

    /**
     * @brief Recover from a crashed operation
     *
     * Dispatches to the appropriate recovery handler based on operation type.
     * Deletes the journal file after recovery attempt.
     *
     * @param installedStatePath Path to installed_packages.json (for uninstall recovery)
     * @return Recovery result
     */
    RecoveryResult recover(const fs::path& installedStatePath = {});

    /**
     * @brief Get the journal file path
     */
    [[nodiscard]] fs::path journalPath() const;

private:
    void flushJournal();
    void deleteJournal();

    RecoveryResult recoverInstall(const JournalEntry& entry);
    RecoveryResult recoverUninstall(const JournalEntry& entry, const fs::path& installedStatePath);
    RecoveryResult recoverBackupCreate(const JournalEntry& entry);
    RecoveryResult recoverBackupRestore(const JournalEntry& entry);

    fs::path dataDir_;
    JournalEntry current_;
    bool active_{false};
    int pendingCount_{0};
    mutable std::mutex mutex_;

    static constexpr int kFlushInterval = 20;
};

} // namespace makine::recovery
