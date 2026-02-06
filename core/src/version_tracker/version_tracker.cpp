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

} // namespace makineai
