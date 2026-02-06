/**
 * @file test_version_tracker.cpp
 * @brief Unit tests for VersionTracker module
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/version_tracker.hpp>
#include <makineai/security.hpp>
#include <fstream>
#include <filesystem>

namespace makineai {
namespace testing {

class VersionTrackerTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    std::filesystem::path dataDir_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makineai_version_tests";
        dataDir_ = std::filesystem::temp_directory_path() / "makineai_version_data";
        std::filesystem::create_directories(testDir_);
        std::filesystem::create_directories(dataDir_);

        // Initialize version tracker
        auto& tracker = VersionTracker::instance();
        tracker.initialize(dataDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
        std::filesystem::remove_all(dataDir_);
    }

    void createTestExe(const std::filesystem::path& path, const std::string& content = "EXE") {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << content;
    }

    GameInfo createTestGame(const std::string& name) {
        auto gamePath = testDir_ / name;
        auto exePath = gamePath / (name + ".exe");
        createTestExe(exePath, "Game executable content for " + name);

        GameInfo game;
        game.storeId = name + "_store_id";
        game.name = name;
        game.installPath = gamePath;
        game.executablePath = exePath;
        game.version = "1.0.0";

        return game;
    }
};

// Test initialization
TEST_F(VersionTrackerTest, Initialize) {
    // Verify database was created
    auto dbPath = dataDir_ / "versions.db";
    EXPECT_TRUE(std::filesystem::exists(dbPath));
}

// Test version recording
TEST_F(VersionTrackerTest, RecordVersion) {
    auto game = createTestGame("TestGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);

    // Check that version was recorded
    auto storedHash = tracker.getStoredHash(game);
    EXPECT_TRUE(storedHash.has_value());
    EXPECT_FALSE(storedHash->empty());
}

TEST_F(VersionTrackerTest, RecordVersionMultipleGames) {
    auto game1 = createTestGame("Game1");
    auto game2 = createTestGame("Game2");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game1);
    tracker.recordVersion(game2);

    EXPECT_TRUE(tracker.getStoredHash(game1).has_value());
    EXPECT_TRUE(tracker.getStoredHash(game2).has_value());
}

// Test version checking
TEST_F(VersionTrackerTest, CheckVersionUnchanged) {
    auto game = createTestGame("UnchangedGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);

    // Check same version
    auto status = tracker.checkVersion(game);
    EXPECT_EQ(status, VersionStatus::Unchanged);
}

TEST_F(VersionTrackerTest, CheckVersionUpdated) {
    auto game = createTestGame("UpdatedGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);

    // Modify the executable
    std::ofstream(game.executablePath, std::ios::binary) << "Modified content";

    // Check version
    auto status = tracker.checkVersion(game);
    EXPECT_NE(status, VersionStatus::Unchanged);
}

TEST_F(VersionTrackerTest, CheckVersionUnknown) {
    auto game = createTestGame("UnknownGame");
    // Don't record version

    auto& tracker = VersionTracker::instance();
    auto status = tracker.checkVersion(game);

    EXPECT_EQ(status, VersionStatus::Unknown);
}

// Test compatibility checking
TEST_F(VersionTrackerTest, IsCompatibleWithMatchingHash) {
    auto game = createTestGame("CompatibleGame");

    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(game.executablePath);

    TranslationPackage package;
    package.id = "test_package";
    package.supportedHashes = {hash};

    auto& tracker = VersionTracker::instance();
    EXPECT_TRUE(tracker.isCompatible(game, package));
}

TEST_F(VersionTrackerTest, IsCompatibleWithNoMatchingHash) {
    auto game = createTestGame("IncompatibleGame");

    TranslationPackage package;
    package.id = "test_package";
    package.supportedHashes = {"different_hash_123"};

    auto& tracker = VersionTracker::instance();
    EXPECT_FALSE(tracker.isCompatible(game, package));
}

TEST_F(VersionTrackerTest, IsCompatibleWithVersionRange) {
    auto game = createTestGame("VersionRangeGame");
    game.version = "1.5.0";

    TranslationPackage package;
    package.id = "test_package";
    package.gameVersionMin = "1.0.0";
    package.gameVersionMax = "2.0.0";

    auto& tracker = VersionTracker::instance();
    EXPECT_TRUE(tracker.isCompatible(game, package));
}

TEST_F(VersionTrackerTest, IsNotCompatibleBelowMinVersion) {
    auto game = createTestGame("OldGame");
    game.version = "0.5.0";

    TranslationPackage package;
    package.id = "test_package";
    package.gameVersionMin = "1.0.0";

    auto& tracker = VersionTracker::instance();
    EXPECT_FALSE(tracker.isCompatible(game, package));
}

TEST_F(VersionTrackerTest, IsNotCompatibleAboveMaxVersion) {
    auto game = createTestGame("NewGame");
    game.version = "3.0.0";

    TranslationPackage package;
    package.id = "test_package";
    package.gameVersionMax = "2.0.0";

    auto& tracker = VersionTracker::instance();
    EXPECT_FALSE(tracker.isCompatible(game, package));
}

// Test patch installation tracking
TEST_F(VersionTrackerTest, MarkPatchInstalled) {
    auto game = createTestGame("PatchedGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);
    tracker.markPatchInstalled(game, "tr-v1.0");

    auto patchVersion = tracker.getInstalledPatchVersion(game);
    EXPECT_TRUE(patchVersion.has_value());
    EXPECT_EQ(*patchVersion, "tr-v1.0");
}

TEST_F(VersionTrackerTest, GetInstalledPatchVersionNoPatch) {
    auto game = createTestGame("UnpatchedGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);

    auto patchVersion = tracker.getInstalledPatchVersion(game);
    EXPECT_FALSE(patchVersion.has_value());
}

// Test VersionMonitor
TEST_F(VersionTrackerTest, VersionMonitorCreate) {
    auto monitor = VersionMonitor::create();
    EXPECT_NE(monitor, nullptr);
}

TEST_F(VersionTrackerTest, VersionMonitorStartStop) {
    auto monitor = VersionMonitor::create();

    std::vector<GameInfo> games;
    monitor->setCheckInterval(std::chrono::seconds(1));
    monitor->startMonitoring(games);

    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    monitor->stopMonitoring();
    // Should not crash
}

TEST_F(VersionTrackerTest, VersionMonitorCallback) {
    auto game = createTestGame("MonitoredGame");

    auto& tracker = VersionTracker::instance();
    tracker.recordVersion(game);

    bool callbackCalled = false;
    auto monitor = VersionMonitor::create();
    monitor->setCallback([&](const GameInfo& g, VersionStatus status) {
        callbackCalled = true;
    });

    // Modify game to trigger callback
    std::ofstream(game.executablePath, std::ios::binary) << "Modified";

    std::vector<GameInfo> games = {game};
    monitor->setCheckInterval(std::chrono::seconds(1));
    monitor->startMonitoring(games);

    // Wait for check
    std::this_thread::sleep_for(std::chrono::seconds(2));

    monitor->stopMonitoring();

    EXPECT_TRUE(callbackCalled);
}

} // namespace testing
} // namespace makineai
