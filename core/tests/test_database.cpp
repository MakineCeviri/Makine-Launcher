/**
 * @file test_database.cpp
 * @brief Unit tests for SQLite database operations
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Tests game CRUD, settings, backup, and transaction support
 * against a real SQLite DB.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makine/database.hpp>
#include <makine/types.hpp>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace makine;
using ::testing::SizeIs;

// Database tests require DPAPI (CryptProtectData/CryptUnprotectData) which
// hangs on MinGW GCC 13.1. These tests work correctly on MSVC builds.
#if defined(__MINGW32__) || defined(__MINGW64__)

TEST(DatabaseTest, MinGWSkipped) {
    GTEST_SKIP() << "Database tests skipped on MinGW (DPAPI incompatibility)";
}

#else  // \!__MINGW32__

// ============================================================================
// Test Fixture — creates a temp DB for each test
// ============================================================================

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use Core-initialized database (singleton already set up by test_main)
        // Calling initialize() again is a no-op (early return if initialized)
        auto& db = Database::instance();
        if (!db.isInitialized()) {
            tempDir_ = fs::temp_directory_path() / "makine_test_db";
            fs::create_directories(tempDir_);
            dbPath_ = tempDir_ / "test.db";
            auto result = db.initialize(dbPath_);
            ASSERT_TRUE(result.has_value()) << "DB init failed: " << result.error().message;
            ownsDb_ = true;
        }
    }

    void TearDown() override {
        if (ownsDb_) {
            std::error_code ec;
            fs::remove(dbPath_, ec);
            fs::remove(fs::path(dbPath_.string() + "-wal"), ec);
            fs::remove(fs::path(dbPath_.string() + "-shm"), ec);
            fs::remove(fs::path(dbPath_.string() + ".enc"), ec);
            Database::instance().close();
            fs::remove_all(tempDir_, ec);
        }
        // Clean up test data from Core DB (delete games, settings, etc.)
        if (Database::instance().isInitialized()) {
            Database::instance().executeRaw("DELETE FROM games");
            Database::instance().executeRaw("DELETE FROM settings");
            Database::instance().executeRaw("DELETE FROM backups");
        }
    }

    // Helper: create a minimal GameInfo
    static GameInfo makeGame(const std::string& storeId, const std::string& name) {
        GameInfo game;
        game.id.storeId = storeId;
        game.id.store = GameStore::Steam;
        game.id.exeHash = "hash_" + storeId;
        game.name = name;
        game.installPath = "C:/Games/" + name;
        game.executablePath = "C:/Games/" + name + "/game.exe";
        game.dataPath = "C:/Games/" + name + "/data";
        game.engine = GameEngine::Unity;
        game.version = "1.0.0";
        game.engineVersion = "2021.3";
        game.detectedAt = "2026-01-01T00:00:00Z";
        return game;
    }

    fs::path tempDir_;
    fs::path dbPath_;
    bool ownsDb_ = false;
};

// ============================================================================
// Initialization
// ============================================================================

TEST_F(DatabaseTest, InitializeSetsPathAndState) {
    EXPECT_TRUE(Database::instance().isInitialized());
    EXPECT_EQ(Database::instance().getPath(), dbPath_);
}

TEST_F(DatabaseTest, CloseAndReinitialize) {
#if defined(__MINGW32__) || defined(__MINGW64__)
    GTEST_SKIP() << "DPAPI encrypt/decrypt in close() hangs on MinGW";
#endif
    Database::instance().close();
    EXPECT_FALSE(Database::instance().isInitialized());

    auto result = Database::instance().initialize(dbPath_);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(Database::instance().isInitialized());
}

// ============================================================================
// Game Operations
// ============================================================================

TEST_F(DatabaseTest, SaveAndRetrieveGame) {
    auto game = makeGame("1716740", "Disco Elysium");

    auto saveResult = Database::instance().saveGame(game);
    ASSERT_TRUE(saveResult.has_value());

    auto getResult = Database::instance().getGameBySteamId("1716740");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());

    auto& retrieved = getResult.value().value();
    EXPECT_EQ(retrieved.name, "Disco Elysium");
    EXPECT_EQ(retrieved.id.storeId, "1716740");
    EXPECT_EQ(retrieved.engine, GameEngine::Unity);
}

TEST_F(DatabaseTest, GetNonexistentGameReturnsNullopt) {
    auto result = Database::instance().getGameBySteamId("999999");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());
}

TEST_F(DatabaseTest, GetAllGamesReturnsMultiple) {
    Database::instance().saveGame(makeGame("100", "Game A"));
    Database::instance().saveGame(makeGame("200", "Game B"));
    Database::instance().saveGame(makeGame("300", "Game C"));

    auto result = Database::instance().getAllGames();
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(3));
}

TEST_F(DatabaseTest, DeleteGameRemovesIt) {
    auto game = makeGame("100", "Game A");
    Database::instance().saveGame(game);

    auto delResult = Database::instance().deleteGame("100");
    EXPECT_TRUE(delResult.has_value());

    auto getResult = Database::instance().getGameBySteamId("100");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

TEST_F(DatabaseTest, SaveGameUpdateExisting) {
    auto game = makeGame("100", "Game A");
    Database::instance().saveGame(game);

    game.name = "Game A Updated";
    Database::instance().saveGame(game);

    auto result = Database::instance().getGameBySteamId("100");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(result.value().value().name, "Game A Updated");
}

// ============================================================================
// Settings Operations
// ============================================================================

TEST_F(DatabaseTest, SetAndGetSetting) {
    auto setResult = Database::instance().setSetting("language", "tr");
    EXPECT_TRUE(setResult.has_value());

    auto getResult = Database::instance().getSetting("language");
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value(), "tr");
}

TEST_F(DatabaseTest, GetNonexistentSettingReturnsNullopt) {
    auto result = Database::instance().getSetting("nonexistent_key");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());
}

TEST_F(DatabaseTest, OverwriteSetting) {
    Database::instance().setSetting("theme", "dark");
    Database::instance().setSetting("theme", "light");

    auto result = Database::instance().getSetting("theme");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(result.value().value(), "light");
}

TEST_F(DatabaseTest, GetAllSettings) {
    Database::instance().setSetting("a", "1");
    Database::instance().setSetting("b", "2");
    Database::instance().setSetting("c", "3");

    auto result = Database::instance().getAllSettings();
    ASSERT_TRUE(result.has_value());
    EXPECT_GE(result.value().size(), 3u);
    EXPECT_EQ(result.value().at("a"), "1");
}

// ============================================================================
// Backup Record Operations
// ============================================================================

TEST_F(DatabaseTest, AddAndGetBackupRecord) {
    BackupRecord record;
    record.id = "bkp_001";
    record.gameId = "game_100";
    record.createdAt = 1700000000;
    record.manifest = R"({"files":["data.json"]})";
    record.sizeBytes = 1024;
    record.status = BackupStatus::Active;

    auto addResult = Database::instance().addBackupRecord(record);
    EXPECT_TRUE(addResult.has_value());

    auto getResult = Database::instance().getBackupsByGame("game_100");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_THAT(getResult.value(), SizeIs(1));
    EXPECT_EQ(getResult.value().front().id, "bkp_001");
}

TEST_F(DatabaseTest, DeleteBackupRecord) {
    BackupRecord record;
    record.id = "bkp_del";
    record.gameId = "game_200";
    record.status = BackupStatus::Active;
    Database::instance().addBackupRecord(record);

    auto delResult = Database::instance().deleteBackupRecord("bkp_del");
    EXPECT_TRUE(delResult.has_value());

    auto getResult = Database::instance().getBackup("bkp_del");
    ASSERT_TRUE(getResult.has_value());
    // After deletion, may return nullopt or deleted status
    if (getResult.value().has_value()) {
        EXPECT_NE(getResult.value().value().status, BackupStatus::Active);
    }
}

// ============================================================================
// Transaction Operations
// ============================================================================

TEST_F(DatabaseTest, TransactionCommit) {
    auto beginResult = Database::instance().beginTransaction();
    EXPECT_TRUE(beginResult.has_value());

    Database::instance().setSetting("tx_key", "tx_value");

    auto commitResult = Database::instance().commitTransaction();
    EXPECT_TRUE(commitResult.has_value());

    auto get = Database::instance().getSetting("tx_key");
    ASSERT_TRUE(get.has_value());
    ASSERT_TRUE(get.value().has_value());
    EXPECT_EQ(get.value().value(), "tx_value");
}

TEST_F(DatabaseTest, TransactionRollback) {
    Database::instance().setSetting("rollback_test", "before");

    auto beginResult = Database::instance().beginTransaction();
    EXPECT_TRUE(beginResult.has_value());

    Database::instance().setSetting("rollback_test", "during_tx");

    auto rollbackResult = Database::instance().rollbackTransaction();
    EXPECT_TRUE(rollbackResult.has_value());

    auto get = Database::instance().getSetting("rollback_test");
    ASSERT_TRUE(get.has_value());
    ASSERT_TRUE(get.value().has_value());
    EXPECT_EQ(get.value().value(), "before");
}

// ============================================================================
// Vacuum
// ============================================================================

TEST_F(DatabaseTest, VacuumSucceeds) {
    // Add and delete data to create free pages
    for (int i = 0; i < 20; ++i) {
        Database::instance().setSetting("tmp_" + std::to_string(i), "data");
    }
    for (int i = 0; i < 20; ++i) {
        Database::instance().executeRaw(
            "DELETE FROM settings WHERE key = 'tmp_" + std::to_string(i) + "'");
    }

    auto result = Database::instance().vacuum();
    EXPECT_TRUE(result.has_value());
}

#endif  // \!__MINGW32__
