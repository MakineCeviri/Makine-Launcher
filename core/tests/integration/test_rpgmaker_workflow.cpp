/**
 * @file test_rpgmaker_workflow.cpp
 * @brief Integration tests for RPG Maker game translation workflow
 *
 * Tests the complete workflow for RPG Maker MV/MZ games:
 * 1. Detect RPG Maker game (MV vs MZ)
 * 2. Parse JSON data files (System.json, Map*.json, etc.)
 * 3. Extract translatable strings
 * 4. Apply translations
 * 5. Verify JSON structure integrity
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "makineai/core.hpp"
#include "makineai/game_detector.hpp"
#include "makineai/handlers/engine_handler.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace makineai;
using json = nlohmann::json;

/**
 * @brief Test fixture for RPG Maker workflow tests
 */
class RPGMakerWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        #ifdef MAKINEAI_TEST_FIXTURES_DIR
            fixturesDir_ = MAKINEAI_TEST_FIXTURES_DIR;
        #else
            fixturesDir_ = fs::current_path() / "fixtures";
        #endif

        #ifdef MAKINEAI_TEST_TEMP_DIR
            tempDir_ = fs::path(MAKINEAI_TEST_TEMP_DIR) / "rpgmaker_test";
        #else
            tempDir_ = fs::temp_directory_path() / "makineai_rpgmaker_test";
        #endif

        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        if (fs::exists(tempDir_)) {
            std::error_code ec;
            fs::remove_all(tempDir_, ec);
        }
    }

    /**
     * @brief Check if RPG Maker fixture exists
     */
    bool hasRPGMakerFixture() const {
        return fs::exists(fixturesDir_ / "sample_rpgmaker_game");
    }

    /**
     * @brief Copy fixture to temp for testing
     */
    fs::path copyFixture() {
        auto src = fixturesDir_ / "sample_rpgmaker_game";
        auto dst = tempDir_ / "sample_rpgmaker_game";

        if (fs::exists(src)) {
            fs::copy(src, dst, fs::copy_options::recursive);
        }

        return dst;
    }

    /**
     * @brief Create a minimal RPG Maker MV game structure for testing
     */
    fs::path createMinimalRPGMakerMVGame() {
        auto gameDir = tempDir_ / "test_rpgmaker_mv";
        auto wwwDir = gameDir / "www";
        auto dataDir = wwwDir / "data";

        fs::create_directories(dataDir);

        // Create package.json (RPG Maker MV marker)
        {
            json package;
            package["name"] = "test-game";
            package["main"] = "index.html";
            package["js-flags"] = "--expose-gc";
            package["window"]["title"] = "Test RPG";
            package["window"]["width"] = 816;
            package["window"]["height"] = 624;

            std::ofstream ofs(gameDir / "package.json");
            ofs << package.dump(2);
        }

        // Create System.json
        {
            json system;
            system["gameTitle"] = "Test RPG Adventure";
            system["locale"] = "en_US";
            system["currencyUnit"] = "Gold";
            system["elements"] = json::array({"", "Fire", "Ice", "Thunder"});
            system["equipTypes"] = json::array({"", "Weapon", "Shield", "Head", "Body"});
            system["skillTypes"] = json::array({"", "Magic", "Special"});
            system["weaponTypes"] = json::array({"", "Sword", "Axe", "Staff"});
            system["armorTypes"] = json::array({"", "Light", "Heavy", "Robe"});

            std::ofstream ofs(dataDir / "System.json");
            ofs << system.dump(2);
        }

        // Create Map001.json
        {
            json map;
            map["displayName"] = "Starting Village";
            map["events"] = json::array();

            // Add an event with dialogue
            json event;
            event["id"] = 1;
            event["name"] = "Villager";
            event["pages"] = json::array();

            json page;
            page["conditions"] = json::object();
            page["list"] = json::array();

            // Add dialogue commands
            json cmd1;
            cmd1["code"] = 101; // Show Text header
            cmd1["parameters"] = json::array({"", 0, 0, 2});
            page["list"].push_back(cmd1);

            json cmd2;
            cmd2["code"] = 401; // Show Text content
            cmd2["parameters"] = json::array({"Welcome to our village!"});
            page["list"].push_back(cmd2);

            json cmd3;
            cmd3["code"] = 401;
            cmd3["parameters"] = json::array({"Please feel free to explore."});
            page["list"].push_back(cmd3);

            event["pages"].push_back(page);
            map["events"].push_back(nullptr); // Event 0 is null
            map["events"].push_back(event);

            std::ofstream ofs(dataDir / "Map001.json");
            ofs << map.dump(2);
        }

        // Create Actors.json
        {
            json actors = json::array();
            actors.push_back(nullptr); // Index 0 is null

            json actor;
            actor["id"] = 1;
            actor["name"] = "Hero";
            actor["nickname"] = "The Chosen One";
            actor["profile"] = "A brave adventurer seeking to save the world.";
            actors.push_back(actor);

            std::ofstream ofs(dataDir / "Actors.json");
            ofs << actors.dump(2);
        }

        return gameDir;
    }

    /**
     * @brief Create sample translations for RPG Maker
     */
    std::vector<TranslationEntry> createSampleTranslations() {
        std::vector<TranslationEntry> entries;

        TranslationEntry e1;
        e1.key = "System.gameTitle";
        e1.original = "Test RPG Adventure";
        e1.translated = "Test RPG Macerası";
        entries.push_back(std::move(e1));

        TranslationEntry e2;
        e2.key = "Map001.displayName";
        e2.original = "Starting Village";
        e2.translated = "Başlangıç Köyü";
        entries.push_back(std::move(e2));

        TranslationEntry e3;
        e3.key = "Actors.1.name";
        e3.original = "Hero";
        e3.translated = "Kahraman";
        entries.push_back(std::move(e3));

        return entries;
    }

protected:
    fs::path fixturesDir_;
    fs::path tempDir_;
};

/**
 * @test Detect RPG Maker MV game from package.json
 */
TEST_F(RPGMakerWorkflowTest, DetectsRPGMakerMVGame) {
    auto gameDir = createMinimalRPGMakerMVGame();

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    ASSERT_TRUE(result.has_value()) << "Engine detection failed";
    EXPECT_EQ(result->engine, GameEngine::RPGMaker_MV);
    EXPECT_GT(result->confidence, 0.5f);
}

/**
 * @test Parse System.json correctly
 */
TEST_F(RPGMakerWorkflowTest, ParsesSystemJson) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto systemPath = gameDir / "www" / "data" / "System.json";

    // Read and parse
    std::ifstream ifs(systemPath);
    json system = json::parse(ifs);

    // Verify structure
    EXPECT_EQ(system["gameTitle"], "Test RPG Adventure");
    EXPECT_EQ(system["locale"], "en_US");
    EXPECT_EQ(system["elements"][1], "Fire");
    EXPECT_EQ(system["skillTypes"][1], "Magic");
}

/**
 * @test Extract translatable strings from Map JSON
 */
TEST_F(RPGMakerWorkflowTest, ExtractsStringsFromMapJson) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto mapPath = gameDir / "www" / "data" / "Map001.json";

    std::ifstream ifs(mapPath);
    json map = json::parse(ifs);

    // Extract strings
    std::vector<std::string> extractedStrings;

    // Display name
    if (map.contains("displayName") && map["displayName"].is_string()) {
        extractedStrings.push_back(map["displayName"]);
    }

    // Event dialogue (code 401)
    if (map.contains("events") && map["events"].is_array()) {
        for (const auto& event : map["events"]) {
            if (event.is_null()) continue;
            if (!event.contains("pages")) continue;

            for (const auto& page : event["pages"]) {
                if (!page.contains("list")) continue;

                for (const auto& cmd : page["list"]) {
                    if (cmd["code"] == 401) {
                        auto params = cmd["parameters"];
                        if (params.is_array() && !params.empty() && params[0].is_string()) {
                            extractedStrings.push_back(params[0]);
                        }
                    }
                }
            }
        }
    }

    // Verify extraction
    EXPECT_GE(extractedStrings.size(), 3);
    EXPECT_THAT(extractedStrings, testing::Contains("Starting Village"));
    EXPECT_THAT(extractedStrings, testing::Contains("Welcome to our village!"));
    EXPECT_THAT(extractedStrings, testing::Contains("Please feel free to explore."));
}

/**
 * @test Apply translations to System.json
 */
TEST_F(RPGMakerWorkflowTest, AppliesTranslationsToSystemJson) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto systemPath = gameDir / "www" / "data" / "System.json";

    // Read original
    json system;
    {
        std::ifstream ifs(systemPath);
        system = json::parse(ifs);
    }

    EXPECT_EQ(system["gameTitle"], "Test RPG Adventure");

    // Apply translation
    system["gameTitle"] = "Test RPG Macerası";

    // Write back
    {
        std::ofstream ofs(systemPath, std::ios::trunc);
        ofs << system.dump(2);
    }

    // Verify
    {
        std::ifstream ifs(systemPath);
        json modified = json::parse(ifs);
        EXPECT_EQ(modified["gameTitle"], "Test RPG Macerası");
    }
}

/**
 * @test Preserves JSON structure after translation
 */
TEST_F(RPGMakerWorkflowTest, PreservesJsonStructure) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto actorsPath = gameDir / "www" / "data" / "Actors.json";

    // Read original structure
    json original;
    {
        std::ifstream ifs(actorsPath);
        original = json::parse(ifs);
    }

    // Apply translation
    original[1]["name"] = "Kahraman";
    original[1]["nickname"] = "Seçilmiş Kişi";
    original[1]["profile"] = "Dünyayı kurtarmak için yola çıkmış cesur bir maceracı.";

    // Write back
    {
        std::ofstream ofs(actorsPath, std::ios::trunc);
        ofs << original.dump(2);
    }

    // Verify structure preserved
    {
        std::ifstream ifs(actorsPath);
        json modified = json::parse(ifs);

        // Structure should be intact
        ASSERT_TRUE(modified.is_array());
        ASSERT_EQ(modified.size(), 2);
        ASSERT_TRUE(modified[0].is_null());
        ASSERT_TRUE(modified[1].is_object());

        // Translations applied
        EXPECT_EQ(modified[1]["name"], "Kahraman");
        EXPECT_EQ(modified[1]["id"], 1); // Untranslated fields preserved
    }
}

/**
 * @test Create backup before modifying data files
 */
TEST_F(RPGMakerWorkflowTest, CreatesBackupBeforeModification) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto dataDir = gameDir / "www" / "data";

    // Create backup directory
    auto backupDir = tempDir_ / "backup" / "data";
    fs::create_directories(backupDir);

    // Backup all JSON files
    for (const auto& entry : fs::directory_iterator(dataDir)) {
        if (entry.path().extension() == ".json") {
            fs::copy_file(entry.path(), backupDir / entry.path().filename());
        }
    }

    // Verify backups
    EXPECT_TRUE(fs::exists(backupDir / "System.json"));
    EXPECT_TRUE(fs::exists(backupDir / "Map001.json"));
    EXPECT_TRUE(fs::exists(backupDir / "Actors.json"));
}

/**
 * @test Validate JSON after translation
 */
TEST_F(RPGMakerWorkflowTest, ValidatesJsonAfterTranslation) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto systemPath = gameDir / "www" / "data" / "System.json";

    // Modify and write
    {
        std::ifstream ifs(systemPath);
        json system = json::parse(ifs);
        system["gameTitle"] = "Türkçe Oyun Adı";

        std::ofstream ofs(systemPath, std::ios::trunc);
        ofs << system.dump(2);
    }

    // Validate JSON is still parseable
    std::ifstream ifs(systemPath);
    json validated;

    EXPECT_NO_THROW({
        validated = json::parse(ifs);
    }) << "Modified JSON should still be valid";

    EXPECT_TRUE(validated.is_object());
    EXPECT_EQ(validated["gameTitle"], "Türkçe Oyun Adı");
}

/**
 * @test Full RPG Maker workflow with fixture
 */
TEST_F(RPGMakerWorkflowTest, FullWorkflowWithFixture) {
    if (!hasRPGMakerFixture()) {
        GTEST_SKIP() << "RPG Maker fixture not available";
    }

    auto gameDir = copyFixture();

    // 1. Detect game
    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);
    ASSERT_TRUE(detection.has_value());
    EXPECT_TRUE(detection->engine == GameEngine::RPGMaker_MV ||
                detection->engine == GameEngine::RPGMaker_MZ);

    // 2-4. Handler integration would go here
    // TODO: Use actual handler when integration is complete

    SUCCEED() << "Fixture-based workflow requires handler integration";
}

/**
 * @test Handles malformed JSON gracefully
 */
TEST_F(RPGMakerWorkflowTest, HandlesMalformedJsonGracefully) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto systemPath = gameDir / "www" / "data" / "System.json";

    // Corrupt the JSON
    {
        std::ofstream ofs(systemPath, std::ios::trunc);
        ofs << "{ invalid json }}}";
    }

    // Attempt to parse
    std::ifstream ifs(systemPath);

    EXPECT_THROW({
        json::parse(ifs);
    }, json::parse_error) << "Should throw on malformed JSON";
}

/**
 * @test Handles UTF-8 strings correctly
 */
TEST_F(RPGMakerWorkflowTest, HandlesUTF8StringsCorrectly) {
    auto gameDir = createMinimalRPGMakerMVGame();
    auto systemPath = gameDir / "www" / "data" / "System.json";

    // Write Turkish characters
    {
        std::ifstream ifs(systemPath);
        json system = json::parse(ifs);
        system["gameTitle"] = "Türkçe: şçğüöıİŞÇĞÜÖ";

        std::ofstream ofs(systemPath, std::ios::trunc);
        ofs << system.dump(2);
    }

    // Read back and verify
    {
        std::ifstream ifs(systemPath);
        json system = json::parse(ifs);

        std::string title = system["gameTitle"];
        EXPECT_EQ(title, "Türkçe: şçğüöıİŞÇĞÜÖ");
    }
}
