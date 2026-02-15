/**
 * @file version_tracker.cpp
 * @brief Version tracker implementation
 *
 * Tracks game versions to detect updates and maintain
 * translation compatibility.
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include "makineai/version_tracker.hpp"
#include "makineai/security.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace makineai {

// Database implementation using JSON file
class VersionTracker::Database {
public:
    std::unordered_map<std::string, RecordedVersion> records;

    bool load(const fs::path& path) {
        if (!fs::exists(path)) {
            return true; // Empty database is valid
        }

        try {
            std::ifstream ifs(path);
            if (!ifs) return false;

            auto j = nlohmann::json::parse(ifs);
            records.clear();

            for (const auto& item : j["records"]) {
                RecordedVersion rv;
                rv.gameId = item["gameId"].get<std::string>();
                rv.exeHash = item["exeHash"].get<std::string>();
                rv.version = item["version"].get<std::string>();
                rv.patchVersion = item["patchVersion"].get<std::string>();
                rv.recordedAt = item["recordedAt"].get<uint64_t>();
                rv.lastCheckedAt = item["lastCheckedAt"].get<uint64_t>();
                rv.gamePath = item["gamePath"].get<std::string>();

                records[rv.gameId] = std::move(rv);
            }

            return true;
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::VERSION, "Failed to load version database: {}", e.what());
            return false;
        }
    }

    bool save(const fs::path& path) {
        try {
            nlohmann::json j;
            j["version"] = 1;
            j["records"] = nlohmann::json::array();

            for (const auto& [id, rv] : records) {
                j["records"].push_back({
                    {"gameId", rv.gameId},
                    {"exeHash", rv.exeHash},
                    {"version", rv.version},
                    {"patchVersion", rv.patchVersion},
                    {"recordedAt", rv.recordedAt},
                    {"lastCheckedAt", rv.lastCheckedAt},
                    {"gamePath", rv.gamePath.string()}
                });
            }

            // Ensure directory exists
            fs::create_directories(path.parent_path());

            std::ofstream ofs(path);
            if (!ofs) return false;

            ofs << j.dump(2);
            return true;
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::VERSION, "Failed to save version database: {}", e.what());
            return false;
        }
    }
};

VersionTracker::VersionTracker() : db_(std::make_unique<Database>()) {
    MAKINEAI_LOG_DEBUG(log::VERSION, "VersionTracker initialized");
}

VersionTracker::~VersionTracker() {
    saveDatabase();
}

void VersionTracker::setDatabasePath(const fs::path& path) {
    dbPath_ = path;
    loadDatabase();
}

void VersionTracker::loadDatabase() {
    if (!dbPath_.empty()) {
        db_->load(dbPath_);
        MAKINEAI_LOG_INFO(log::VERSION, "Loaded {} version records", db_->records.size());

        // Update metrics for tracked versions
        Metrics::instance().gauge("games_with_updates", 0);
        Metrics::instance().increment("versions_tracked", static_cast<int64_t>(db_->records.size()));
    }
}

void VersionTracker::saveDatabase() {
    if (!dbPath_.empty()) {
        db_->save(dbPath_);
    }
}

VoidResult VersionTracker::recordVersion(const GameInfo& game, const std::string& patchVersion) {
    try {
        // Hash the executable
        SecurityManager security;
        auto hashResult = security.hashFile(game.executablePath);
        if (!hashResult) {
            return std::unexpected(Error(ErrorCode::Unknown,
                "Failed to hash executable: " + hashResult.error().message()));
        }

        RecordedVersion rv;
        rv.gameId = game.id.storeId;
        rv.exeHash = *hashResult;
        rv.version = game.version;
        rv.patchVersion = patchVersion;
        rv.recordedAt = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        rv.lastCheckedAt = rv.recordedAt;
        rv.gamePath = game.installPath;

        db_->records[rv.gameId] = std::move(rv);
        saveDatabase();

        MAKINEAI_LOG_INFO(log::VERSION, "Recorded version for {}: hash={}, patch={}",
                    game.name, hashResult->substr(0, 16) + "...", patchVersion);

        // Increment tracked versions counter
        Metrics::instance().increment("versions_tracked");

        return {};

    } catch (const std::exception& e) {
        return std::unexpected(Error(ErrorCode::Unknown, e.what()));
    }
}

Result<VersionCheckResult> VersionTracker::checkVersion(const GameInfo& game) const {
    VersionCheckResult result;
    result.status = VersionStatus::Unknown;

    // Check if we have a record
    auto it = db_->records.find(game.id.storeId);
    if (it == db_->records.end()) {
        result.message = "No version record for this game";
        return result;
    }

    result.recorded = it->second;

    // Hash current executable
    SecurityManager security;
    auto hashResult = security.hashFile(game.executablePath);
    if (!hashResult) {
        result.message = "Failed to hash executable";
        return result;
    }

    result.currentHash = *hashResult;
    result.currentVersion = game.version;

    // Compare hashes
    MAKINEAI_LOG_DEBUG(log::VERSION, "Comparing versions for {}: recorded={}, current={}",
                game.id.storeId, result.recorded.exeHash.substr(0, 8) + "...",
                result.currentHash.substr(0, 8) + "...");

    if (result.currentHash == result.recorded.exeHash) {
        result.status = VersionStatus::Unchanged;
        result.message = "Game version unchanged";
        MAKINEAI_LOG_DEBUG(log::VERSION, "Game {} version unchanged", game.id.storeId);
    } else {
        // Game was updated - check compatibility (simplified)
        // In full implementation, would query server for compatible versions
        result.status = VersionStatus::UpdatedIncompatible;
        result.message = "Game was updated - translation may need update";
        result.translationAvailable = false;

        MAKINEAI_LOG_WARN(log::VERSION, "Version mismatch for {}: expected hash {}, got {}",
                    game.id.storeId,
                    result.recorded.exeHash.substr(0, 16) + "...",
                    result.currentHash.substr(0, 16) + "...");

        // Increment version updates counter
        Metrics::instance().increment("version_updates");
    }

    return result;
}

Result<std::vector<VersionCheckResult>> VersionTracker::checkAll() const {
    std::vector<VersionCheckResult> results;
    int gamesWithUpdates = 0;

    for (const auto& [gameId, record] : db_->records) {
        GameInfo game;
        game.id.storeId = gameId;
        game.installPath = record.gamePath;
        game.executablePath = record.gamePath / "game.exe"; // Would need proper exe detection
        game.version = record.version;

        auto result = checkVersion(game);
        if (result) {
            results.push_back(*result);
            if (result->status != VersionStatus::Unchanged) {
                gamesWithUpdates++;
            }
        }
    }

    // Update gauge for games needing updates
    Metrics::instance().gauge("games_with_updates", static_cast<double>(gamesWithUpdates));

    MAKINEAI_LOG_INFO(log::VERSION, "Checked {} games, {} need updates",
                results.size(), gamesWithUpdates);

    return results;
}

Result<RecordedVersion> VersionTracker::getRecorded(const std::string& gameId) const {
    auto it = db_->records.find(gameId);
    if (it == db_->records.end()) {
        return std::unexpected(Error(ErrorCode::GameNotFound,
            "No version record for game: " + gameId));
    }
    return it->second;
}

bool VersionTracker::hasRecord(const std::string& gameId) const {
    return db_->records.contains(gameId);
}

VoidResult VersionTracker::updateRecord(
    const std::string& gameId,
    const std::string& newHash,
    const std::string& newVersion,
    const std::string& patchVersion
) {
    auto it = db_->records.find(gameId);
    if (it == db_->records.end()) {
        return std::unexpected(Error(ErrorCode::GameNotFound,
            "No version record for game: " + gameId));
    }

    it->second.exeHash = newHash;
    it->second.version = newVersion;
    it->second.patchVersion = patchVersion;
    it->second.lastCheckedAt = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    saveDatabase();

    MAKINEAI_LOG_INFO(log::VERSION, "Updated version record for {}", gameId);
    return {};
}

VoidResult VersionTracker::removeRecord(const std::string& gameId) {
    auto it = db_->records.find(gameId);
    if (it == db_->records.end()) {
        return std::unexpected(Error(ErrorCode::GameNotFound,
            "No version record for game: " + gameId));
    }

    db_->records.erase(it);
    saveDatabase();

    MAKINEAI_LOG_INFO(log::VERSION, "Removed version record for {}", gameId);
    return {};
}

std::vector<RecordedVersion> VersionTracker::listRecords() const {
    std::vector<RecordedVersion> result;
    result.reserve(db_->records.size());

    for (const auto& [_, record] : db_->records) {
        result.push_back(record);
    }

    return result;
}

// VersionMonitor implementation

class MonitorThread {
public:
    std::thread thread;
    std::atomic<bool> stopFlag{false};
    std::mutex mutex;
    std::condition_variable cv;
};

VersionMonitor::VersionMonitor(VersionTracker& tracker)
    : tracker_(tracker), thread_(std::make_unique<MonitorThread>()) {
}

VersionMonitor::~VersionMonitor() {
    stop();
}

void VersionMonitor::start(uint32_t intervalSeconds, ChangeCallback callback) {
    if (running_) return;

    running_ = true;
    thread_->stopFlag = false;

    thread_->thread = std::thread([this, intervalSeconds, callback]() {
        while (!thread_->stopFlag) {
            // Wait for interval or stop signal
            {
                std::unique_lock lock(thread_->mutex);
                thread_->cv.wait_for(lock, std::chrono::seconds(intervalSeconds),
                    [this]() { return thread_->stopFlag.load(); });
            }

            if (thread_->stopFlag) break;

            // Check all versions
            auto results = tracker_.checkAll();
            if (results) {
                for (const auto& result : *results) {
                    if (result.status != VersionStatus::Unchanged) {
                        callback(result);
                    }
                }
            }
        }
    });

    MAKINEAI_LOG_INFO(log::VERSION, "Version monitor started (interval: {}s)", intervalSeconds);
}

void VersionMonitor::stop() {
    if (!running_) return;

    thread_->stopFlag = true;
    thread_->cv.notify_all();

    if (thread_->thread.joinable()) {
        thread_->thread.join();
    }

    running_ = false;
    MAKINEAI_LOG_INFO(log::VERSION, "Version monitor stopped");
}

void VersionMonitor::checkNow() {
    auto results = tracker_.checkAll();
    if (!results) return;

    for (const auto& result : *results) {
        if (result.status != VersionStatus::Unchanged) {
            MAKINEAI_LOG_INFO(log::VERSION, "Version change detected for {}: {}",
                        result.recorded.gameId, result.message);
        }
    }
}

// ============================================================================
// FileSnapshot & Compatibility (Issue #58)
// ============================================================================

std::unordered_map<std::string, std::string> FileSnapshot::hashMap() const {
    std::unordered_map<std::string, std::string> map;
    map.reserve(files.size());
    for (const auto& f : files) {
        map[f.relativePath.generic_string()] = f.sha256;
    }
    return map;
}

std::string CompatibilityReport::summary() const {
    switch (level) {
        case CompatibilityLevel::Compatible:
            return "Translation is compatible (" + std::to_string(totalTracked) + " files unchanged)";
        case CompatibilityLevel::PartiallyCompatible:
            return "Partially compatible: " + std::to_string(modifiedFiles.size()) +
                   " files modified, " + std::to_string(addedFiles.size()) +
                   " added, " + std::to_string(removedFiles.size()) + " removed";
        case CompatibilityLevel::Incompatible:
            return "Incompatible: " + std::to_string(modifiedFiles.size() + removedFiles.size()) +
                   " critical files changed — translation may be broken";
        default:
            return "Cannot determine compatibility";
    }
}

int CompatibilityReport::integrityPercent() const {
    if (totalTracked == 0) return 0;
    return static_cast<int>((unchangedCount * 100) / totalTracked);
}

fs::path VersionTracker::snapshotPath(const std::string& gameId) const {
    fs::path dir = snapshotDir_.empty() ? dbPath_.parent_path() / "snapshots" : snapshotDir_;
    return dir / (gameId + ".snapshot.json");
}

std::string VersionTracker::hashFileContent(const fs::path& filePath) const {
    SecurityManager security;
    auto result = security.hashFile(filePath);
    if (result) return *result;
    return {};
}

VoidResult VersionTracker::saveSnapshot(const FileSnapshot& snapshot) const {
    try {
        nlohmann::json j;
        j["version"] = 1;
        j["gameId"] = snapshot.gameId;
        j["takenAt"] = snapshot.takenAt;
        j["patchVersion"] = snapshot.patchVersion;
        j["files"] = nlohmann::json::array();

        for (const auto& f : snapshot.files) {
            j["files"].push_back({
                {"path", f.relativePath.generic_string()},
                {"sha256", f.sha256},
                {"size", f.fileSize},
                {"modified", f.modifiedAt}
            });
        }

        auto path = snapshotPath(snapshot.gameId);
        fs::create_directories(path.parent_path());

        std::ofstream ofs(path);
        if (!ofs) {
            return std::unexpected(Error(ErrorCode::FileWriteFailed,
                "Cannot write snapshot: " + path.string()));
        }
        ofs << j.dump(2);

        MAKINEAI_LOG_INFO(log::VERSION, "Saved snapshot for {}: {} files",
            snapshot.gameId, snapshot.files.size());
        return {};

    } catch (const std::exception& e) {
        return std::unexpected(Error(ErrorCode::Unknown, e.what()));
    }
}

Result<FileSnapshot> VersionTracker::loadSnapshot(const std::string& gameId) const {
    auto path = snapshotPath(gameId);
    if (!fs::exists(path)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "No snapshot for game: " + gameId));
    }

    try {
        std::ifstream ifs(path);
        if (!ifs) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "Cannot read snapshot: " + path.string()));
        }

        auto j = nlohmann::json::parse(ifs);
        FileSnapshot snapshot;
        snapshot.gameId = j["gameId"].get<std::string>();
        snapshot.takenAt = j["takenAt"].get<uint64_t>();
        snapshot.patchVersion = j["patchVersion"].get<std::string>();

        for (const auto& item : j["files"]) {
            FileHashRecord rec;
            rec.relativePath = item["path"].get<std::string>();
            rec.sha256 = item["sha256"].get<std::string>();
            rec.fileSize = item["size"].get<uint64_t>();
            rec.modifiedAt = item["modified"].get<uint64_t>();
            snapshot.files.push_back(std::move(rec));
        }

        return snapshot;

    } catch (const std::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError,
            "Failed to parse snapshot: " + std::string(e.what())));
    }
}

VoidResult VersionTracker::takeSnapshot(
    const GameInfo& game,
    const std::vector<fs::path>& trackedFiles,
    const std::string& patchVersion
) {
    FileSnapshot snapshot;
    snapshot.gameId = game.id.storeId;
    snapshot.takenAt = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    snapshot.patchVersion = patchVersion;

    MAKINEAI_LOG_INFO(log::VERSION, "Taking file snapshot for {}: {} files",
        game.name, trackedFiles.size());

    for (const auto& relPath : trackedFiles) {
        fs::path absPath = game.installPath / relPath;

        if (!fs::exists(absPath)) {
            MAKINEAI_LOG_WARN(log::VERSION, "Snapshot: file not found: {}", relPath.string());
            continue;
        }

        FileHashRecord rec;
        rec.relativePath = relPath;
        rec.sha256 = hashFileContent(absPath);

        if (rec.sha256.empty()) {
            MAKINEAI_LOG_WARN(log::VERSION, "Snapshot: failed to hash: {}", relPath.string());
            continue;
        }

        std::error_code ec;
        rec.fileSize = fs::file_size(absPath, ec);
        auto ftime = fs::last_write_time(absPath, ec);
        if (!ec) {
            rec.modifiedAt = static_cast<uint64_t>(
                ftime.time_since_epoch().count());
        }

        snapshot.files.push_back(std::move(rec));
    }

    return saveSnapshot(snapshot);
}

Result<CompatibilityReport> VersionTracker::checkCompatibility(
    const GameInfo& game
) const {
    auto snapshotResult = loadSnapshot(game.id.storeId);
    if (!snapshotResult) {
        CompatibilityReport report;
        report.level = CompatibilityLevel::Unknown;
        report.gameId = game.id.storeId;
        return report;
    }

    const auto& snapshot = *snapshotResult;
    auto baselineMap = snapshot.hashMap();

    CompatibilityReport report;
    report.gameId = game.id.storeId;
    report.totalTracked = snapshot.files.size();
    report.unchangedCount = 0;

    // Check each file in the snapshot against current state
    for (const auto& [relPath, expectedHash] : baselineMap) {
        fs::path absPath = game.installPath / relPath;

        if (!fs::exists(absPath)) {
            report.removedFiles.push_back(relPath);
            continue;
        }

        std::string currentHash = hashFileContent(absPath);
        if (currentHash == expectedHash) {
            report.unchangedCount++;
        } else {
            report.modifiedFiles.push_back(relPath);
        }
    }

    // Check for new files in same directories (optional: detect added content)
    // Only scan directories that contained tracked files
    std::set<fs::path> trackedDirs;
    for (const auto& f : snapshot.files) {
        trackedDirs.insert(f.relativePath.parent_path());
    }

    for (const auto& dir : trackedDirs) {
        fs::path absDir = game.installPath / dir;
        if (!fs::is_directory(absDir)) continue;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(absDir, ec)) {
            if (!entry.is_regular_file()) continue;

            auto relPath = fs::relative(entry.path(), game.installPath, ec).generic_string();
            if (ec) continue;

            if (baselineMap.find(relPath) == baselineMap.end()) {
                report.addedFiles.push_back(relPath);
            }
        }
    }

    // Determine compatibility level
    size_t changedCount = report.modifiedFiles.size() + report.removedFiles.size();

    if (changedCount == 0) {
        report.level = CompatibilityLevel::Compatible;
    } else if (report.integrityPercent() >= 80) {
        report.level = CompatibilityLevel::PartiallyCompatible;
    } else {
        report.level = CompatibilityLevel::Incompatible;
    }

    MAKINEAI_LOG_INFO(log::VERSION, "Compatibility check for {}: {} ({}% intact, {} modified, {} removed, {} added)",
        game.id.storeId, report.summary(), report.integrityPercent(),
        report.modifiedFiles.size(), report.removedFiles.size(), report.addedFiles.size());

    return report;
}

Result<FileSnapshot> VersionTracker::getSnapshot(const std::string& gameId) const {
    return loadSnapshot(gameId);
}

bool VersionTracker::hasSnapshot(const std::string& gameId) const {
    return fs::exists(snapshotPath(gameId));
}

VoidResult VersionTracker::removeSnapshot(const std::string& gameId) {
    auto path = snapshotPath(gameId);
    if (!fs::exists(path)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "No snapshot for game: " + gameId));
    }

    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Failed to remove snapshot: " + ec.message()));
    }

    MAKINEAI_LOG_INFO(log::VERSION, "Removed snapshot for {}", gameId);
    return {};
}

} // namespace makineai
