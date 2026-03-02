/**
 * @file test_database.cpp
 * @brief Unit tests for SQLite database operations
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests game CRUD, translation memory, glossary, projects,
 * settings, and transaction support against a real SQLite DB.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/database.hpp>
#include <makineai/types.hpp>
#include <filesystem>

namespace fs = std::filesystem;
using namespace makineai;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::Gt;

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
            tempDir_ = fs::temp_directory_path() / "makineai_test_db";
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
            Database::instance().executeRaw("DELETE FROM translation_memory");
            Database::instance().executeRaw("DELETE FROM glossary");
            Database::instance().executeRaw("DELETE FROM projects");
            Database::instance().executeRaw("DELETE FROM entries");
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

    // Helper: create a TranslationMemoryEntry
    static TranslationMemoryEntry makeTMEntry(
        const std::string& source,
        const std::string& target,
        int quality = 85
    ) {
        TranslationMemoryEntry entry;
        entry.sourceText = source;
        entry.targetText = target;
        entry.sourceHash = "hash_" + source;
        entry.sourceLang = "en";
        entry.targetLang = "tr";
        entry.qualityScore = quality;
        entry.verified = false;
        return entry;
    }

    // Helper: create a GlossaryTerm
    static GlossaryTerm makeTerm(
        const std::string& source,
        const std::string& target,
        TermDomain domain = TermDomain::General
    ) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.domain = domain;
        term.priority = 50;
        return term;
    }

    // Helper: create a TranslationProject
    static TranslationProject makeProject(
        const std::string& id,
        const std::string& name,
        const std::string& gameId = ""
    ) {
        TranslationProject proj;
        proj.id = id;
        proj.name = name;
        if (!gameId.empty())
            proj.gameId = gameId;
        proj.sourceLang = "en";
        proj.targetLang = "tr";
        proj.status = ProjectStatus::Active;
        return proj;
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
// Translation Memory Operations
// ============================================================================

TEST_F(DatabaseTest, AddAndFindTMEntry) {
    auto entry = makeTMEntry("Save Game", "Oyunu Kaydet");

    auto addResult = Database::instance().addToTranslationMemory(entry);
    ASSERT_TRUE(addResult.has_value());
    EXPECT_GT(addResult.value(), 0);

    auto findResult = Database::instance().findExactMatch("hash_Save Game");
    ASSERT_TRUE(findResult.has_value());
    ASSERT_TRUE(findResult.value().has_value());
    EXPECT_EQ(findResult.value().value().targetText, "Oyunu Kaydet");
}

TEST_F(DatabaseTest, FindExactMatchMissingReturnsNullopt) {
    auto result = Database::instance().findExactMatch("nonexistent_hash");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());
}

TEST_F(DatabaseTest, TMStatsAfterInsertions) {
    Database::instance().addToTranslationMemory(makeTMEntry("A", "X"));
    Database::instance().addToTranslationMemory(makeTMEntry("B", "Y"));
    Database::instance().addToTranslationMemory(makeTMEntry("C", "Z"));

    auto stats = Database::instance().getTranslationMemoryStats();
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats.value().first, 3);  // total
}

TEST_F(DatabaseTest, UpdateTMEntry) {
    auto entry = makeTMEntry("Hello", "Merhaba", 70);
    auto id = Database::instance().addToTranslationMemory(entry);
    ASSERT_TRUE(id.has_value());

    auto updateResult = Database::instance().updateTranslationMemoryEntry(
        id.value(), "Selam", 95, true);
    EXPECT_TRUE(updateResult.has_value());

    auto getResult = Database::instance().getTranslationMemoryEntryById(id.value());
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().targetText, "Selam");
    EXPECT_TRUE(getResult.value().verified);
}

TEST_F(DatabaseTest, DeleteTMEntry) {
    auto id = Database::instance().addToTranslationMemory(
        makeTMEntry("Test", "Deneme"));
    ASSERT_TRUE(id.has_value());

    auto delResult = Database::instance().deleteTranslationMemoryEntry(id.value());
    EXPECT_TRUE(delResult.has_value());

    auto getResult = Database::instance().getTranslationMemoryEntryById(id.value());
    EXPECT_FALSE(getResult.has_value());  // Should fail — entry deleted
}

TEST_F(DatabaseTest, IncrementTMUsage) {
    auto id = Database::instance().addToTranslationMemory(
        makeTMEntry("Reload", "Yeniden Yukle"));
    ASSERT_TRUE(id.has_value());

    Database::instance().incrementTMUsage(id.value());
    Database::instance().incrementTMUsage(id.value());

    auto entry = Database::instance().getTranslationMemoryEntryById(id.value());
    ASSERT_TRUE(entry.has_value());
    EXPECT_GE(entry.value().usageCount, 2);
}

// ============================================================================
// N-gram Operations
// ============================================================================

TEST_F(DatabaseTest, StoreAndFindNgrams) {
    auto id = Database::instance().addToTranslationMemory(
        makeTMEntry("Loading screen", "Yukleme ekrani"));
    ASSERT_TRUE(id.has_value());

    std::vector<std::pair<std::string, int>> ngrams = {
        {"loa", 0}, {"oad", 1}, {"adi", 2}, {"din", 3}, {"ing", 4}
    };

    auto storeResult = Database::instance().storeNgrams(id.value(), ngrams);
    EXPECT_TRUE(storeResult.has_value());

    auto findResult = Database::instance().findByNgram("loa");
    ASSERT_TRUE(findResult.has_value());
    EXPECT_THAT(findResult.value(), SizeIs(Gt(0u)));
    EXPECT_EQ(findResult.value().front(), id.value());
}

// ============================================================================
// Glossary Operations
// ============================================================================

TEST_F(DatabaseTest, AddAndSearchGlossaryTerm) {
    auto term = makeTerm("Health", "Can", TermDomain::RPG);

    auto addResult = Database::instance().addGlossaryTerm(term);
    ASSERT_TRUE(addResult.has_value());

    auto searchResult = Database::instance().searchGlossary("Health");
    ASSERT_TRUE(searchResult.has_value());
    EXPECT_THAT(searchResult.value(), SizeIs(1));
    EXPECT_EQ(searchResult.value().front().termTarget, "Can");
}

TEST_F(DatabaseTest, GetAllGlossaryTerms) {
    Database::instance().addGlossaryTerm(makeTerm("Health", "Can"));
    Database::instance().addGlossaryTerm(makeTerm("Mana", "Buyü Gücü"));
    Database::instance().addGlossaryTerm(makeTerm("Shield", "Kalkan"));

    auto result = Database::instance().getAllGlossaryTerms();
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(3));
}

TEST_F(DatabaseTest, DeleteGlossaryTerm) {
    auto id = Database::instance().addGlossaryTerm(
        makeTerm("Weapon", "Silah"));
    ASSERT_TRUE(id.has_value());

    auto delResult = Database::instance().deleteGlossaryTerm(id.value());
    EXPECT_TRUE(delResult.has_value());

    auto all = Database::instance().getAllGlossaryTerms();
    ASSERT_TRUE(all.has_value());
    EXPECT_THAT(all.value(), IsEmpty());
}

TEST_F(DatabaseTest, GlossaryAlternativeAndForbidden) {
    auto id = Database::instance().addGlossaryTerm(
        makeTerm("Skill", "Yetenek", TermDomain::RPG));
    ASSERT_TRUE(id.has_value());

    auto altResult = Database::instance().addGlossaryAlternative(
        id.value(), "Beceri", "context: passive skill");
    EXPECT_TRUE(altResult.has_value());

    auto forbidResult = Database::instance().addForbiddenTranslation(
        id.value(), "Ustalık", "Too archaic");
    EXPECT_TRUE(forbidResult.has_value());
}

// ============================================================================
// Project Operations
// ============================================================================

TEST_F(DatabaseTest, CreateAndGetProject) {
    auto proj = makeProject("proj_001", "Disco Elysium TR", "1716740");

    auto createResult = Database::instance().createProject(proj);
    ASSERT_TRUE(createResult.has_value());

    auto getResult = Database::instance().getProject(createResult.value());
    ASSERT_TRUE(getResult.has_value());
    ASSERT_TRUE(getResult.value().has_value());
    EXPECT_EQ(getResult.value().value().name, "Disco Elysium TR");
}

TEST_F(DatabaseTest, GetProjectsByGame) {
    Database::instance().createProject(makeProject("p1", "Project 1", "game_A"));
    Database::instance().createProject(makeProject("p2", "Project 2", "game_A"));
    Database::instance().createProject(makeProject("p3", "Project 3", "game_B"));

    auto result = Database::instance().getProjectsByGame("game_A");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(2));
}

TEST_F(DatabaseTest, DeleteProjectCleansUp) {
    auto createResult = Database::instance().createProject(
        makeProject("p_del", "To Delete"));
    ASSERT_TRUE(createResult.has_value());

    auto delResult = Database::instance().deleteProject(createResult.value());
    EXPECT_TRUE(delResult.has_value());

    auto getResult = Database::instance().getProject(createResult.value());
    ASSERT_TRUE(getResult.has_value());
    EXPECT_FALSE(getResult.value().has_value());
}

// ============================================================================
// Translation Entry Operations
// ============================================================================

TEST_F(DatabaseTest, SaveAndGetEntries) {
    auto projId = Database::instance().createProject(
        makeProject("ep1", "Entry Test"));
    ASSERT_TRUE(projId.has_value());

    TranslationEntry entry;
    entry.projectId = projId.value();
    entry.filePath = "strings.json";
    entry.sourceText = "Hello World";
    entry.targetText = "Merhaba Dünya";
    entry.status = EntryStatus::Translated;
    entry.category = EntryCategory::UI;
    entry.qaScore = 90;

    auto saveResult = Database::instance().saveEntry(entry);
    ASSERT_TRUE(saveResult.has_value());

    auto getResult = Database::instance().getEntriesByProject(projId.value());
    ASSERT_TRUE(getResult.has_value());
    EXPECT_THAT(getResult.value(), SizeIs(1));
    EXPECT_EQ(getResult.value().front().sourceText, "Hello World");
}

TEST_F(DatabaseTest, BatchSaveEntries) {
    auto projId = Database::instance().createProject(
        makeProject("ep2", "Batch Test"));
    ASSERT_TRUE(projId.has_value());

    std::vector<TranslationEntry> entries;
    for (int i = 0; i < 50; ++i) {
        TranslationEntry e;
        e.projectId = projId.value();
        e.filePath = "data.json";
        e.sourceText = "String " + std::to_string(i);
        e.status = EntryStatus::Untranslated;
        entries.push_back(e);
    }

    auto result = Database::instance().saveEntries(entries);
    EXPECT_TRUE(result.has_value());

    auto getResult = Database::instance().getEntriesByProject(projId.value());
    ASSERT_TRUE(getResult.has_value());
    EXPECT_THAT(getResult.value(), SizeIs(50));
}

TEST_F(DatabaseTest, EntryStats) {
    auto projId = Database::instance().createProject(
        makeProject("ep3", "Stats Test"));
    ASSERT_TRUE(projId.has_value());

    // Add mixed entries
    TranslationEntry e1;
    e1.projectId = projId.value();
    e1.filePath = "a.json";
    e1.sourceText = "Untranslated";
    e1.status = EntryStatus::Untranslated;
    Database::instance().saveEntry(e1);

    TranslationEntry e2;
    e2.projectId = projId.value();
    e2.filePath = "a.json";
    e2.sourceText = "Translated";
    e2.targetText = "Çevrildi";
    e2.status = EntryStatus::Translated;
    Database::instance().saveEntry(e2);

    auto stats = Database::instance().getEntryStats(projId.value());
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats.value().total, 2);
    EXPECT_EQ(stats.value().translated, 1);
    EXPECT_EQ(stats.value().untranslated, 1);
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
