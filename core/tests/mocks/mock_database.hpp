/**
 * @file mock_database.hpp
 * @brief Mock implementations of database interfaces for testing
 *
 * Provides Google Mock implementations for database operations
 * allowing unit tests to run without SQLite dependency.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <gmock/gmock.h>
#include "makine/database.hpp"
#include "makine/types.hpp"

#include <string>

#include <unordered_map>
#include <vector>
#include <optional>

namespace makine::testing {

/**
 * @brief Mock implementation of Database
 *
 * Usage:
 * @code
 * MockDatabase db;
 * EXPECT_CALL(db, getGame("game_123"))
 *     .WillOnce(Return(std::optional<GameInfo>{...}));
 * @endcode
 */
class MockDatabase {
public:
    // Game operations
    MOCK_METHOD(VoidResult, saveGame, (const GameInfo& game), ());
    MOCK_METHOD(std::optional<GameInfo>, getGame, (const std::string& gameId), ());
    MOCK_METHOD(std::vector<GameInfo>, getAllGames, (), ());
    MOCK_METHOD(VoidResult, deleteGame, (const std::string& gameId), ());

    // Backup operations
    MOCK_METHOD(VoidResult, saveBackupRecord, (const BackupRecord& record), ());
    MOCK_METHOD(std::optional<BackupRecord>, getBackupRecord,
                (const std::string& gameId), ());
    MOCK_METHOD(std::vector<BackupRecord>, getBackupHistory,
                (const std::string& gameId), ());

    // Package operations
    MOCK_METHOD(VoidResult, saveInstalledPackage,
                (const TranslationPackage& package), ());
    MOCK_METHOD(std::optional<TranslationPackage>, getInstalledPackage,
                (const std::string& packageId), ());
    MOCK_METHOD(std::vector<TranslationPackage>, getInstalledPackages,
                (const std::string& gameId), ());
};

/**
 * @brief In-memory fake database for testing
 *
 * Stores data in memory without SQLite dependency.
 * Useful for integration tests that need actual storage behavior.
 */
class FakeDatabase {
public:
    // Game operations
    VoidResult saveGame(const GameInfo& game) {
        games_[game.gameId.storeId] = game;
        return {};
    }

    std::optional<GameInfo> getGame(const std::string& gameId) {
        auto it = games_.find(gameId);
        if (it != games_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<GameInfo> getAllGames() {
        std::vector<GameInfo> result;
        result.reserve(games_.size());
        for (const auto& [id, game] : games_) {
            result.push_back(game);
        }
        return result;
    }

    VoidResult deleteGame(const std::string& gameId) {
        games_.erase(gameId);
        return {};
    }

    // Backup operations
    VoidResult saveBackupRecord(const BackupRecord& record) {
        backupRecords_[record.gameId].push_back(record);
        return {};
    }

    std::optional<BackupRecord> getBackupRecord(const std::string& gameId) {
        auto it = backupRecords_.find(gameId);
        if (it != backupRecords_.end() && !it->second.empty()) {
            return it->second.back();
        }
        return std::nullopt;
    }

    std::vector<BackupRecord> getBackupHistory(const std::string& gameId) {
        auto it = backupRecords_.find(gameId);
        if (it != backupRecords_.end()) {
            return it->second;
        }
        return {};
    }

    // Package operations
    VoidResult saveInstalledPackage(const TranslationPackage& package) {
        packages_[package.id] = package;
        return {};
    }

    std::optional<TranslationPackage> getInstalledPackage(
        const std::string& packageId
    ) {
        auto it = packages_.find(packageId);
        if (it != packages_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<TranslationPackage> getInstalledPackages(
        const std::string& gameId
    ) {
        std::vector<TranslationPackage> results;
        for (const auto& [id, package] : packages_) {
            if (package.gameId == gameId) {
                results.push_back(package);
            }
        }
        return results;
    }

    // Test helpers
    void clear() {
        games_.clear();
        backupRecords_.clear();
        packages_.clear();
    }

    size_t gameCount() const { return games_.size(); }
    size_t packageCount() const { return packages_.size(); }

private:
    std::unordered_map<std::string, GameInfo> games_;
    std::unordered_map<std::string, std::vector<BackupRecord>> backupRecords_;
    std::unordered_map<std::string, TranslationPackage> packages_;
};

/**
 * @brief Create a fake BackupRecord for testing
 */
inline BackupRecord createFakeBackupRecord(
    const std::string& gameId,
    const std::string& backupPath,
    BackupStatus status = BackupStatus::Complete
) {
    BackupRecord record;
    record.gameId = gameId;
    record.backupPath = backupPath;
    record.status = status;
    record.timestamp = std::chrono::system_clock::now();
    record.fileCount = 10;
    record.totalSize = 1024 * 1024;
    return record;
}

/**
 * @brief Create a fake TranslationPackage for testing
 */
inline TranslationPackage createFakePackage(
    const std::string& id,
    const std::string& gameId,
    const std::string& language = "tr",
    const std::string& version = "1.0.0"
) {
    TranslationPackage package;
    package.id = id;
    package.gameId = gameId;
    package.language = language;
    package.version = Version::parse(version).value_or(Version{1, 0, 0});
    package.name = "Test Translation Package";
    return package;
}

} // namespace makine::testing
