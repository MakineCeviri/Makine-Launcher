/**
 * @file version_tracker.hpp
 * @brief Game version tracking and update detection
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>
#include <unordered_map>

namespace makineai {

/**
 * @brief Game version change status
 */
enum class VersionStatus {
    Unchanged,          // Game hasn't changed
    UpdatedCompatible,  // Game updated, translation still works
    UpdatedIncompatible,// Game updated, translation may not work
    Unknown             // Cannot determine (first check or error)
};

/**
 * @brief Recorded game version information
 */
struct RecordedVersion {
    std::string gameId;
    std::string exeHash;
    std::string version;
    std::string patchVersion;   // Installed translation version
    uint64_t recordedAt;        // Unix timestamp
    uint64_t lastCheckedAt;
    fs::path gamePath;
};

/**
 * @brief Version check result
 */
struct VersionCheckResult {
    VersionStatus status;
    RecordedVersion recorded;           // What we had recorded
    std::string currentHash;            // Current exe hash
    std::string currentVersion;         // Current game version
    bool translationAvailable;          // Is there a compatible translation?
    std::string compatiblePatchVersion; // If available, which version
    std::string message;                // Human-readable status
};

// ============================================================================
// File Snapshot & Compatibility (Issue #58)
// ============================================================================

/**
 * @brief Hash record for a single file
 */
struct FileHashRecord {
    fs::path relativePath;      // Relative to game directory
    std::string sha256;         // SHA-256 hash
    uint64_t fileSize{0};       // Bytes
    uint64_t modifiedAt{0};     // Last write time (epoch)
};

/**
 * @brief Snapshot of all translation-relevant files in a game
 *
 * Taken at translation install time. Used to detect game updates
 * that may break the translation.
 */
struct FileSnapshot {
    std::string gameId;
    uint64_t takenAt{0};                    // When snapshot was taken
    std::string patchVersion;                // Translation version installed
    std::vector<FileHashRecord> files;       // All tracked files

    /// Build a lookup map from relative path to hash
    [[nodiscard]] std::unordered_map<std::string, std::string> hashMap() const;
};

/**
 * @brief How compatible is the current game state with installed translation
 */
enum class CompatibilityLevel {
    Compatible,             // All tracked files unchanged
    PartiallyCompatible,    // Some files changed but core assets intact
    Incompatible,           // Critical files changed, translation likely broken
    Unknown                 // Cannot determine (missing snapshot or error)
};

/**
 * @brief Detailed compatibility analysis between snapshot and current state
 */
struct CompatibilityReport {
    CompatibilityLevel level{CompatibilityLevel::Unknown};
    std::string gameId;

    // Changed files
    std::vector<fs::path> modifiedFiles;    // Hash changed
    std::vector<fs::path> addedFiles;       // New files not in snapshot
    std::vector<fs::path> removedFiles;     // Files in snapshot but missing

    size_t totalTracked{0};
    size_t unchangedCount{0};

    /// Human-readable summary
    [[nodiscard]] std::string summary() const;

    /// Percentage of files unchanged (0-100)
    [[nodiscard]] int integrityPercent() const;
};

/**
 * @brief Tracks game versions for update detection
 */
class VersionTracker {
public:
    VersionTracker();
    ~VersionTracker();

    /**
     * @brief Record current game version
     *
     * Called when translation is installed to record the baseline.
     */
    [[nodiscard]] VoidResult recordVersion(
        const GameInfo& game,
        const std::string& patchVersion
    );

    /**
     * @brief Check if game version has changed
     */
    [[nodiscard]] Result<VersionCheckResult> checkVersion(
        const GameInfo& game
    ) const;

    /**
     * @brief Check all recorded games for updates
     */
    [[nodiscard]] Result<std::vector<VersionCheckResult>> checkAll() const;

    /**
     * @brief Get recorded version for a game
     */
    [[nodiscard]] Result<RecordedVersion> getRecorded(
        const std::string& gameId
    ) const;

    /**
     * @brief Check if game has recorded version
     */
    [[nodiscard]] bool hasRecord(const std::string& gameId) const;

    /**
     * @brief Update recorded version after successful update
     */
    [[nodiscard]] VoidResult updateRecord(
        const std::string& gameId,
        const std::string& newHash,
        const std::string& newVersion,
        const std::string& patchVersion
    );

    /**
     * @brief Remove version record
     */
    [[nodiscard]] VoidResult removeRecord(const std::string& gameId);

    /**
     * @brief List all recorded games
     */
    [[nodiscard]] std::vector<RecordedVersion> listRecords() const;

    /**
     * @brief Set database path
     */
    void setDatabasePath(const fs::path& path);

    // =================================================================
    // File Snapshot API (Issue #58)
    // =================================================================

    /**
     * @brief Take a snapshot of translation-relevant files
     *
     * Hashes files that the translation modifies or depends on.
     * Call this when installing a translation to establish baseline.
     *
     * @param game Game info
     * @param trackedFiles Files to include (relative to game dir)
     * @param patchVersion Translation version being installed
     */
    [[nodiscard]] VoidResult takeSnapshot(
        const GameInfo& game,
        const std::vector<fs::path>& trackedFiles,
        const std::string& patchVersion
    );

    /**
     * @brief Check current game files against stored snapshot
     *
     * Detects modified, added, and removed files since the snapshot.
     */
    [[nodiscard]] Result<CompatibilityReport> checkCompatibility(
        const GameInfo& game
    ) const;

    /**
     * @brief Get stored snapshot for a game
     */
    [[nodiscard]] Result<FileSnapshot> getSnapshot(const std::string& gameId) const;

    /**
     * @brief Check if game has a file snapshot
     */
    [[nodiscard]] bool hasSnapshot(const std::string& gameId) const;

    /**
     * @brief Remove file snapshot
     */
    [[nodiscard]] VoidResult removeSnapshot(const std::string& gameId);

private:
    fs::path dbPath_;
    fs::path snapshotDir_;      // Directory for snapshot JSON files
    class Database;
    std::unique_ptr<Database> db_;

    void loadDatabase();
    void saveDatabase();

    // Snapshot helpers
    [[nodiscard]] fs::path snapshotPath(const std::string& gameId) const;
    [[nodiscard]] Result<FileSnapshot> loadSnapshot(const std::string& gameId) const;
    [[nodiscard]] VoidResult saveSnapshot(const FileSnapshot& snapshot) const;
    [[nodiscard]] std::string hashFileContent(const fs::path& filePath) const;
};

/**
 * @brief Automatic version monitor
 *
 * Can be used to periodically check for game updates
 * and notify the user.
 */
class VersionMonitor {
public:
    /**
     * @brief Callback when game version change is detected
     */
    using ChangeCallback = std::function<void(const VersionCheckResult&)>;

    VersionMonitor(VersionTracker& tracker);
    ~VersionMonitor();

    /**
     * @brief Start monitoring
     * @param intervalSeconds How often to check
     * @param callback Called when changes detected
     */
    void start(uint32_t intervalSeconds, ChangeCallback callback);

    /**
     * @brief Stop monitoring
     */
    void stop();

    /**
     * @brief Check if monitoring is active
     */
    [[nodiscard]] bool isRunning() const { return running_; }

    /**
     * @brief Force immediate check
     */
    void checkNow();

private:
    VersionTracker& tracker_;
    bool running_ = false;
    std::unique_ptr<class MonitorThread> thread_;
};

} // namespace makineai
