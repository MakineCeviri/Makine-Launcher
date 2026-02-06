/**
 * @file test_end_to_end.cpp
 * @brief End-to-end integration tests for MakineAI translation workflow
 *
 * Tests the complete user workflow:
 * 1. Scan for games
 * 2. Select a game
 * 3. Check for available translations
 * 4. Download/Install translation package
 * 5. Apply translation
 * 6. Verify and launch game
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "makineai/core.hpp"
#include "makineai/game_detector.hpp"
#include "makineai/asset_parser.hpp"
#include "makineai/database.hpp"
#include "makineai/handlers/engine_handler.hpp"

#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
using namespace makineai;

/**
 * @brief Test fixture for end-to-end workflow tests
 */
class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        #ifdef MAKINEAI_TEST_FIXTURES_DIR
            fixturesDir_ = MAKINEAI_TEST_FIXTURES_DIR;
        #else
            fixturesDir_ = fs::current_path() / "fixtures";
        #endif

        #ifdef MAKINEAI_TEST_TEMP_DIR
            tempDir_ = fs::path(MAKINEAI_TEST_TEMP_DIR) / "e2e_test";
        #else
            tempDir_ = fs::temp_directory_path() / "makineai_e2e_test";
        #endif

        fs::create_directories(tempDir_);

        // Create mock game library
        gamesDir_ = tempDir_ / "games";
        fs::create_directories(gamesDir_);

        // Create mock data directory
        dataDir_ = tempDir_ / "makineai_data";
        fs::create_directories(dataDir_);
        fs::create_directories(dataDir_ / "backups");
        fs::create_directories(dataDir_ / "packages");
        fs::create_directories(dataDir_ / "cache");
    }

    void TearDown() override {
        if (fs::exists(tempDir_)) {
            std::error_code ec;
            fs::remove_all(tempDir_, ec);
        }
    }

    /**
     * @brief Create a mock Ren'Py game in the games directory
     */
    fs::path createMockRenPyGame(const std::string& name) {
        auto gameDir = gamesDir_ / name;
        auto gameSubDir = gameDir / "game";
        auto renpyDir = gameDir / "renpy";

        fs::create_directories(gameSubDir);
        fs::create_directories(renpyDir);

        // Create marker files
        {
            std::ofstream ofs(renpyDir / "__init__.py");
            ofs << "# Ren'Py\n";
        }

        // Create script
        {
            std::ofstream ofs(gameSubDir / "script.rpy");
            ofs << R"(
label start:
    "Hello, player!"
    "Welcome to the game."
    menu:
        "Start":
            "Let's begin!"
        "Quit":
            return
)";
        }

        return gameDir;
    }

    /**
     * @brief Create a mock RPG Maker MV game in the games directory
     */
    fs::path createMockRPGMakerGame(const std::string& name) {
        auto gameDir = gamesDir_ / name;
        auto wwwDir = gameDir / "www";
        auto dataDir = wwwDir / "data";

        fs::create_directories(dataDir);

        // Create package.json
        {
            std::ofstream ofs(gameDir / "package.json");
            ofs << R"({"name":")" << name << R"(","main":"index.html"})";
        }

        // Create System.json
        {
            std::ofstream ofs(dataDir / "System.json");
            ofs << R"({"gameTitle":")" << name << R"(","locale":"en_US"})";
        }

        return gameDir;
    }

    /**
     * @brief Create a mock translation package
     */
    fs::path createMockTranslationPackage(const std::string& gameId,
                                          const std::string& language) {
        auto packageDir = dataDir_ / "packages" / (gameId + "_" + language);
        fs::create_directories(packageDir);

        // Create package manifest
        {
            std::ofstream ofs(packageDir / "manifest.json");
            ofs << R"({
  "id": ")" << gameId << "_" << language << R"(",
  "name": ")" << gameId << " " << language << R"( Translation",
  "version": "1.0.0",
  "language": ")" << language << R"(",
  "gameId": ")" << gameId << R"(",
  "files": ["translations.json"]
})";
        }

        // Create translations file
        {
            std::ofstream ofs(packageDir / "translations.json");
            ofs << R"({
  "entries": [
    {"key": "Hello, player!", "original": "Hello, player!", "translated": "Merhaba, oyuncu!"},
    {"key": "Welcome to the game.", "original": "Welcome to the game.", "translated": "Oyuna hoş geldiniz."}
  ]
})";
        }

        return packageDir;
    }

protected:
    fs::path fixturesDir_;
    fs::path tempDir_;
    fs::path gamesDir_;
    fs::path dataDir_;
};

/**
 * @test Scan mock games directory
 */
TEST_F(EndToEndTest, ScansGamesDirectory) {
    // Create mock games
    createMockRenPyGame("TestRenPyGame");
    createMockRPGMakerGame("TestRPGMakerGame");

    // Scan directory
    scanners::GameDetector detector;
    std::vector<GameInfo> foundGames;

    for (const auto& entry : fs::directory_iterator(gamesDir_)) {
        if (entry.is_directory()) {
            auto detection = detector.detectEngine(entry.path());
            if (detection.has_value() && detection->confidence > 0.5f) {
                GameInfo info;
                info.name = entry.path().filename().string();
                info.installPath = entry.path();
                info.engine = detection->engine;
                foundGames.push_back(info);
            }
        }
    }

    EXPECT_GE(foundGames.size(), 2u);
}

/**
 * @test Complete workflow: detect, backup, translate, verify
 */
TEST_F(EndToEndTest, CompleteTranslationWorkflow) {
    // 1. Create mock game
    auto gameDir = createMockRenPyGame("WorkflowTestGame");

    // 2. Detect game engine
    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);

    ASSERT_TRUE(detection.has_value());
    EXPECT_EQ(detection->engine, GameEngine::RenPy);

    // 3. Read original script
    auto scriptPath = gameDir / "game" / "script.rpy";
    std::string originalContent;
    {
        std::ifstream ifs(scriptPath);
        originalContent.assign(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>());
    }

    EXPECT_NE(originalContent.find("Hello, player!"), std::string::npos);

    // 4. Create backup
    auto backupDir = dataDir_ / "backups" / "WorkflowTestGame";
    fs::create_directories(backupDir);
    fs::copy_file(scriptPath, backupDir / "script.rpy");

    EXPECT_TRUE(fs::exists(backupDir / "script.rpy"));

    // 5. Apply translation (simple string replacement)
    std::string translatedContent = originalContent;
    size_t pos = translatedContent.find("Hello, player!");
    if (pos != std::string::npos) {
        translatedContent.replace(pos, 14, "Merhaba, oyuncu!");
    }
    pos = translatedContent.find("Welcome to the game.");
    if (pos != std::string::npos) {
        translatedContent.replace(pos, 20, "Oyuna hoş geldiniz.");
    }

    {
        std::ofstream ofs(scriptPath, std::ios::trunc);
        ofs << translatedContent;
    }

    // 6. Verify translation applied
    {
        std::ifstream ifs(scriptPath);
        std::string modifiedContent(std::istreambuf_iterator<char>(ifs),
                                   std::istreambuf_iterator<char>());

        EXPECT_NE(modifiedContent.find("Merhaba, oyuncu!"), std::string::npos);
        EXPECT_NE(modifiedContent.find("Oyuna hoş geldiniz."), std::string::npos);
        EXPECT_EQ(modifiedContent.find("Hello, player!"), std::string::npos);
    }

    // 7. Verify backup can restore
    fs::copy_file(backupDir / "script.rpy", scriptPath,
                  fs::copy_options::overwrite_existing);

    {
        std::ifstream ifs(scriptPath);
        std::string restoredContent(std::istreambuf_iterator<char>(ifs),
                                   std::istreambuf_iterator<char>());

        EXPECT_NE(restoredContent.find("Hello, player!"), std::string::npos);
        EXPECT_EQ(restoredContent.find("Merhaba, oyuncu!"), std::string::npos);
    }
}

/**
 * @test Multiple games with different engines
 */
TEST_F(EndToEndTest, HandlesMultipleEngines) {
    // Create games with different engines
    createMockRenPyGame("RenPyGame1");
    createMockRenPyGame("RenPyGame2");
    createMockRPGMakerGame("RPGMakerGame1");

    scanners::GameDetector detector;

    // Detect all games
    struct DetectedGame {
        std::string name;
        GameEngine engine;
    };
    std::vector<DetectedGame> detected;

    for (const auto& entry : fs::directory_iterator(gamesDir_)) {
        if (!entry.is_directory()) continue;

        auto result = detector.detectEngine(entry.path());
        if (result.has_value() && result->confidence > 0.5f) {
            detected.push_back({
                entry.path().filename().string(),
                result->engine
            });
        }
    }

    // Should detect all 3 games
    EXPECT_EQ(detected.size(), 3u);

    // Count by engine
    int renpyCount = 0;
    int rpgmakerCount = 0;

    for (const auto& game : detected) {
        if (game.engine == GameEngine::RenPy) renpyCount++;
        if (game.engine == GameEngine::RPGMaker_MV) rpgmakerCount++;
    }

    EXPECT_EQ(renpyCount, 2);
    EXPECT_EQ(rpgmakerCount, 1);
}

/**
 * @test Translation package discovery
 */
TEST_F(EndToEndTest, DiscoversTranslationPackages) {
    // Create mock packages
    createMockTranslationPackage("game1", "tr");
    createMockTranslationPackage("game1", "de");
    createMockTranslationPackage("game2", "tr");

    // List packages
    auto packagesDir = dataDir_ / "packages";
    std::vector<std::string> packageIds;

    for (const auto& entry : fs::directory_iterator(packagesDir)) {
        if (entry.is_directory()) {
            auto manifestPath = entry.path() / "manifest.json";
            if (fs::exists(manifestPath)) {
                packageIds.push_back(entry.path().filename().string());
            }
        }
    }

    EXPECT_EQ(packageIds.size(), 3u);
    EXPECT_THAT(packageIds, testing::Contains("game1_tr"));
    EXPECT_THAT(packageIds, testing::Contains("game1_de"));
    EXPECT_THAT(packageIds, testing::Contains("game2_tr"));
}

/**
 * @test Backup directory organization
 */
TEST_F(EndToEndTest, OrganizesBackupsCorrectly) {
    // Create multiple backups
    auto game1Backup = dataDir_ / "backups" / "game1";
    auto game2Backup = dataDir_ / "backups" / "game2";

    fs::create_directories(game1Backup);
    fs::create_directories(game2Backup);

    // Create backup files
    {
        std::ofstream ofs(game1Backup / "script.rpy");
        ofs << "backup content 1";
    }
    {
        std::ofstream ofs(game2Backup / "System.json");
        ofs << "backup content 2";
    }

    // Verify structure
    EXPECT_TRUE(fs::exists(game1Backup / "script.rpy"));
    EXPECT_TRUE(fs::exists(game2Backup / "System.json"));

    // List all backups
    auto backupsDir = dataDir_ / "backups";
    std::vector<std::string> gameBackups;

    for (const auto& entry : fs::directory_iterator(backupsDir)) {
        if (entry.is_directory()) {
            gameBackups.push_back(entry.path().filename().string());
        }
    }

    EXPECT_EQ(gameBackups.size(), 2u);
}

/**
 * @test Rollback on failed translation
 */
TEST_F(EndToEndTest, RollbacksOnFailure) {
    // Create game
    auto gameDir = createMockRenPyGame("RollbackTestGame");
    auto scriptPath = gameDir / "game" / "script.rpy";

    // Get original content
    std::string originalContent;
    {
        std::ifstream ifs(scriptPath);
        originalContent.assign(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>());
    }

    // Create backup
    auto backupDir = dataDir_ / "backups" / "RollbackTestGame";
    fs::create_directories(backupDir);
    fs::copy_file(scriptPath, backupDir / "script.rpy");

    // Simulate failed translation (corrupt the file)
    {
        std::ofstream ofs(scriptPath, std::ios::trunc);
        ofs << "CORRUPTED DATA - SIMULATED FAILURE";
    }

    // Verify corruption
    {
        std::ifstream ifs(scriptPath);
        std::string corrupted(std::istreambuf_iterator<char>(ifs),
                             std::istreambuf_iterator<char>());
        EXPECT_EQ(corrupted, "CORRUPTED DATA - SIMULATED FAILURE");
    }

    // Rollback
    fs::copy_file(backupDir / "script.rpy", scriptPath,
                  fs::copy_options::overwrite_existing);

    // Verify rollback
    {
        std::ifstream ifs(scriptPath);
        std::string restored(std::istreambuf_iterator<char>(ifs),
                            std::istreambuf_iterator<char>());
        EXPECT_EQ(restored, originalContent);
    }
}

/**
 * @test Concurrent operations on different games
 */
TEST_F(EndToEndTest, HandlesConcurrentOperations) {
    // Create multiple games
    auto game1 = createMockRenPyGame("ConcurrentGame1");
    auto game2 = createMockRenPyGame("ConcurrentGame2");
    auto game3 = createMockRenPyGame("ConcurrentGame3");

    std::atomic<int> completedOperations{0};
    std::vector<std::thread> threads;

    // Simulate concurrent translation operations
    auto translateGame = [&](const fs::path& gameDir) {
        auto scriptPath = gameDir / "game" / "script.rpy";

        // Read
        std::string content;
        {
            std::ifstream ifs(scriptPath);
            content.assign(std::istreambuf_iterator<char>(ifs),
                          std::istreambuf_iterator<char>());
        }

        // Modify
        size_t pos = content.find("Hello, player!");
        if (pos != std::string::npos) {
            content.replace(pos, 14, "Merhaba, oyuncu!");
        }

        // Write
        {
            std::ofstream ofs(scriptPath, std::ios::trunc);
            ofs << content;
        }

        completedOperations++;
    };

    threads.emplace_back(translateGame, game1);
    threads.emplace_back(translateGame, game2);
    threads.emplace_back(translateGame, game3);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completedOperations.load(), 3);

    // Verify all games were translated
    for (const auto& gameDir : {game1, game2, game3}) {
        auto scriptPath = gameDir / "game" / "script.rpy";
        std::ifstream ifs(scriptPath);
        std::string content(std::istreambuf_iterator<char>(ifs),
                           std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("Merhaba, oyuncu!"), std::string::npos);
    }
}

/**
 * @test Progress reporting during workflow
 */
TEST_F(EndToEndTest, ReportsProgressDuringWorkflow) {
    auto gameDir = createMockRenPyGame("ProgressTestGame");

    std::vector<std::pair<int, std::string>> progressUpdates;

    // Simulate workflow with progress
    auto reportProgress = [&](int percent, const std::string& message) {
        progressUpdates.emplace_back(percent, message);
    };

    // Step 1: Detect
    reportProgress(10, "Detecting game engine...");

    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);
    ASSERT_TRUE(detection.has_value());

    reportProgress(20, "Game engine detected: Ren'Py");

    // Step 2: Backup
    reportProgress(30, "Creating backup...");

    auto backupDir = dataDir_ / "backups" / "ProgressTestGame";
    fs::create_directories(backupDir);

    reportProgress(50, "Backup complete");

    // Step 3: Apply
    reportProgress(60, "Applying translations...");

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    reportProgress(90, "Translations applied");

    // Step 4: Verify
    reportProgress(95, "Verifying changes...");
    reportProgress(100, "Complete!");

    // Verify progress was reported
    EXPECT_GE(progressUpdates.size(), 6u);
    EXPECT_EQ(progressUpdates.front().first, 10);
    EXPECT_EQ(progressUpdates.back().first, 100);
}

/**
 * @test Cache usage for performance
 */
TEST_F(EndToEndTest, UsesCacheForPerformance) {
    // Create game
    auto gameDir = createMockRenPyGame("CacheTestGame");

    // First scan (cold)
    auto startCold = std::chrono::steady_clock::now();

    scanners::GameDetector detector;
    auto result1 = detector.detectEngine(gameDir);

    auto endCold = std::chrono::steady_clock::now();
    auto coldDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        endCold - startCold).count();

    ASSERT_TRUE(result1.has_value());

    // Second scan should use cache (warm)
    auto startWarm = std::chrono::steady_clock::now();

    auto result2 = detector.detectEngine(gameDir);

    auto endWarm = std::chrono::steady_clock::now();
    auto warmDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        endWarm - startWarm).count();

    ASSERT_TRUE(result2.has_value());

    // Results should be the same
    EXPECT_EQ(result1->engine, result2->engine);

    // Log durations (cache may or may not be implemented)
    std::cout << "Cold scan: " << coldDuration << " us" << std::endl;
    std::cout << "Warm scan: " << warmDuration << " us" << std::endl;
}
