/**
 * @file version_tracker.hpp
 * @brief Game version tracking and update detection
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <memory>

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

private:
    fs::path dbPath_;
    class Database;
    std::unique_ptr<Database> db_;

    void loadDatabase();
    void saveDatabase();
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
