/**
 * @file types/patch_types.hpp
 * @brief Patch, backup, and restore type definitions
 * @copyright (c) 2026 MakineCeviri Team
 *
 * This file contains types related to patching operations,
 * backup management, and restoration.
 */

#pragma once

#include "makine/types/common.hpp"

#include <optional>

namespace makine {

// ============================================================================
// Patch Status
// ============================================================================

/**
 * @brief Translation patch status
 *
 * Indicates the current state of a translation patch for a game.
 */
enum class PatchStatus {
    NotInstalled,     ///< No translation installed
    Installed,        ///< Translation installed and active
    Outdated,         ///< Game updated, translation may not work
    Incompatible,     ///< Translation incompatible with game version
    Corrupted         ///< Translation files corrupted
};

// ============================================================================
// Operation Results
// ============================================================================

/**
 * @brief Result of a patch operation
 *
 * Contains detailed information about what happened during patching.
 */
struct PatchResult {
    bool success = false;
    std::string message;
    uint32_t filesPatched = 0;
    uint32_t filesFailed = 0;
    StringList errors;
    fs::path backupPath;          ///< Where backup was stored

    /// @brief Check if any files were patched
    [[nodiscard]] bool hasPatches() const noexcept {
        return filesPatched > 0;
    }

    /// @brief Check if there were any failures
    [[nodiscard]] bool hasFailures() const noexcept {
        return filesFailed > 0 || !errors.empty();
    }
};

/**
 * @brief Result of a backup operation
 *
 * Contains information about the created backup.
 */
struct BackupResult {
    bool success = false;
    std::string message;
    fs::path backupPath;
    uint64_t sizeBytes = 0;
    uint32_t fileCount = 0;

    /// @brief Check if backup is valid and non-empty
    [[nodiscard]] bool isValid() const noexcept {
        return success && fileCount > 0;
    }
};

/**
 * @brief Result of a restore operation
 *
 * Contains information about what was restored.
 */
struct RestoreResult {
    bool success = false;
    std::string message;
    uint32_t filesRestored = 0;
    uint32_t filesFailed = 0;

    /// @brief Check if restoration completed fully
    [[nodiscard]] bool isComplete() const noexcept {
        return success && filesFailed == 0;
    }
};

// ============================================================================
// Backup Management
// ============================================================================

/**
 * @brief Backup status
 *
 * Indicates the current state of a backup record.
 */
enum class BackupStatus {
    Active,           ///< Backup is available and valid
    Deleted,          ///< Backup has been removed
    Corrupted         ///< Backup files are corrupted
};

/**
 * @brief Backup record stored in database
 *
 * Contains metadata about a game backup.
 */
struct BackupRecord {
    std::string id;               ///< Unique backup ID
    std::string gameId;           ///< Associated game ID
    int64_t createdAt = 0;        ///< Creation timestamp
    std::string manifest;         ///< JSON string with file list
    std::optional<uint64_t> sizeBytes;
    BackupStatus status = BackupStatus::Active;

    /// @brief Check if backup is usable
    [[nodiscard]] bool isUsable() const noexcept {
        return status == BackupStatus::Active;
    }
};


// ============================================================================
// Entry Status
// ============================================================================

/**
 * @brief Translation entry quality status
 */
enum class EntryStatus : int {
    Untranslated,   ///< Not yet translated
    Translated,     ///< Translated but unreviewed
    Fuzzy,          ///< Auto-matched, needs review
    Verified,       ///< Reviewed and approved
    Rejected,       ///< Rejected, needs retranslation
};

} // namespace makine
