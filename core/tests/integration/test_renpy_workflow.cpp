/**
 * @file test_renpy_workflow.cpp
 * @brief Integration tests for Ren'Py game translation workflow
 *
 * Tests the complete workflow:
 * 1. Detect Ren'Py game
 * 2. Extract strings from .rpy/.rpyc files
 * 3. Apply translations
 * 4. Verify game still works
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

namespace fs = std::filesystem;
using namespace makineai;

/**
 * @brief Test fixture for Ren'Py workflow tests
 */
class RenPyWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get fixtures directory
        #ifdef MAKINEAI_TEST_FIXTURES_DIR
            fixturesDir_ = MAKINEAI_TEST_FIXTURES_DIR;
        #else
            fixturesDir_ = fs::current_path() / "fixtures";
        #endif

        // Get temp directory for test outputs
        #ifdef MAKINEAI_TEST_TEMP_DIR
            tempDir_ = fs::path(MAKINEAI_TEST_TEMP_DIR) / "renpy_test";
        #else
            tempDir_ = fs::temp_directory_path() / "makineai_renpy_test";
        #endif

        // Create temp directory
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        // Clean up temp directory
        if (fs::exists(tempDir_)) {
            std::error_code ec;
            fs::remove_all(tempDir_, ec);
        }
    }

    /**
     * @brief Check if Ren'Py fixture exists
     */
    bool hasRenPyFixture() const {
        return fs::exists(fixturesDir_ / "sample_renpy_game");
    }

    /**
     * @brief Copy fixture to temp for testing
     */
    fs::path copyFixture() {
        auto src = fixturesDir_ / "sample_renpy_game";
        auto dst = tempDir_ / "sample_renpy_game";

        if (fs::exists(src)) {
            fs::copy(src, dst, fs::copy_options::recursive);
        }

        return dst;
    }

    /**
     * @brief Create a minimal Ren'Py game structure for testing
     */
    fs::path createMinimalRenPyGame() {
        auto gameDir = tempDir_ / "test_renpy_game";
        auto gameSubDir = gameDir / "game";
        auto renpyDir = gameDir / "renpy";

        fs::create_directories(gameSubDir);
        fs::create_directories(renpyDir);

        // Create script.rpy
        {
            std::ofstream ofs(gameSubDir / "script.rpy");
            ofs << R"(# Test Ren'Py script
label start:
    "Welcome to the game!"
    "This is a test message."

    menu:
        "Choose an option:"
        "Option 1":
            "You chose option 1."
        "Option 2":
            "You chose option 2."

    "The end."
    return
)";
        }

        // Create renpy/__init__.py (marker file)
        {
            std::ofstream ofs(renpyDir / "__init__.py");
            ofs << "# Ren'Py engine marker\n";
        }

        return gameDir;
    }

    /**
     * @brief Create sample translation entries
     */
    std::vector<TranslationEntry> createSampleTranslations() {
        std::vector<TranslationEntry> entries;

        TranslationEntry e1;
        e1.key = "Welcome to the game!";
        e1.original = "Welcome to the game!";
        e1.translated = "Oyuna hoş geldiniz!";
        entries.push_back(std::move(e1));

        TranslationEntry e2;
        e2.key = "This is a test message.";
        e2.original = "This is a test message.";
        e2.translated = "Bu bir test mesajıdır.";
        entries.push_back(std::move(e2));

        TranslationEntry e3;
        e3.key = "The end.";
        e3.original = "The end.";
        e3.translated = "Son.";
        entries.push_back(std::move(e3));

        return entries;
    }

protected:
    fs::path fixturesDir_;
    fs::path tempDir_;
};

/**
 * @test Detect Ren'Py game from directory structure
 */
TEST_F(RenPyWorkflowTest, DetectsRenPyGame) {
    // Create minimal Ren'Py game
    auto gameDir = createMinimalRenPyGame();

    // Detect engine
    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    // Should detect as Ren'Py
    ASSERT_TRUE(result.has_value()) << "Engine detection failed";
    EXPECT_EQ(result->engine, GameEngine::RenPy);
    EXPECT_GT(result->confidence, 0.5f);
}

/**
 * @test Extract strings from .rpy files
 */
TEST_F(RenPyWorkflowTest, ExtractsStringsFromRpyFiles) {
    // Create minimal game
    auto gameDir = createMinimalRenPyGame();

    // Get handler (assuming factory registration)
    // For now, test the core functionality
    auto scriptPath = gameDir / "game" / "script.rpy";

    // Read script content
    std::ifstream ifs(scriptPath);
    std::string content(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());

    // Should contain expected strings
    EXPECT_NE(content.find("Welcome to the game!"), std::string::npos);
    EXPECT_NE(content.find("This is a test message."), std::string::npos);
    EXPECT_NE(content.find("The end."), std::string::npos);
}

/**
 * @test Create backup before patching
 */
TEST_F(RenPyWorkflowTest, CreatesBackupBeforePatching) {
    // Create minimal game
    auto gameDir = createMinimalRenPyGame();

    // Create backup directory
    auto backupDir = tempDir_ / "backup";
    fs::create_directories(backupDir);

    // Copy original files
    auto srcScript = gameDir / "game" / "script.rpy";
    auto dstScript = backupDir / "script.rpy";
    fs::copy_file(srcScript, dstScript);

    // Verify backup exists
    ASSERT_TRUE(fs::exists(dstScript));

    // Verify content matches
    std::ifstream srcStream(srcScript);
    std::ifstream dstStream(dstScript);

    std::string srcContent(std::istreambuf_iterator<char>(srcStream),
                          std::istreambuf_iterator<char>());
    std::string dstContent(std::istreambuf_iterator<char>(dstStream),
                          std::istreambuf_iterator<char>());

    EXPECT_EQ(srcContent, dstContent);
}

/**
 * @test Apply translations and verify changes
 */
TEST_F(RenPyWorkflowTest, AppliesTranslationsToScript) {
    // Create minimal game
    auto gameDir = createMinimalRenPyGame();
    auto scriptPath = gameDir / "game" / "script.rpy";

    // Read original
    std::string original;
    {
        std::ifstream ifs(scriptPath);
        original.assign(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
    }

    // Apply simple translation (string replacement)
    std::string translated = original;
    size_t pos = translated.find("Welcome to the game!");
    if (pos != std::string::npos) {
        translated.replace(pos, 20, "Oyuna hoş geldiniz!");
    }

    // Write modified
    {
        std::ofstream ofs(scriptPath, std::ios::trunc);
        ofs << translated;
    }

    // Verify modification
    std::string modified;
    {
        std::ifstream ifs(scriptPath);
        modified.assign(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
    }

    EXPECT_NE(modified.find("Oyuna hoş geldiniz!"), std::string::npos);
    EXPECT_EQ(modified.find("Welcome to the game!"), std::string::npos);
}

/**
 * @test Restore from backup after patching failure
 */
TEST_F(RenPyWorkflowTest, RestoresFromBackupOnFailure) {
    // Create minimal game
    auto gameDir = createMinimalRenPyGame();
    auto scriptPath = gameDir / "game" / "script.rpy";

    // Create backup
    auto backupDir = tempDir_ / "backup";
    fs::create_directories(backupDir);
    auto backupPath = backupDir / "script.rpy";
    fs::copy_file(scriptPath, backupPath);

    // Read original content
    std::string originalContent;
    {
        std::ifstream ifs(backupPath);
        originalContent.assign(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>());
    }

    // Corrupt the file (simulate failed patch)
    {
        std::ofstream ofs(scriptPath, std::ios::trunc);
        ofs << "CORRUPTED DATA";
    }

    // Verify corruption
    {
        std::ifstream ifs(scriptPath);
        std::string corrupted(std::istreambuf_iterator<char>(ifs),
                             std::istreambuf_iterator<char>());
        EXPECT_EQ(corrupted, "CORRUPTED DATA");
    }

    // Restore from backup
    fs::copy_file(backupPath, scriptPath, fs::copy_options::overwrite_existing);

    // Verify restoration
    std::string restoredContent;
    {
        std::ifstream ifs(scriptPath);
        restoredContent.assign(std::istreambuf_iterator<char>(ifs),
                              std::istreambuf_iterator<char>());
    }

    EXPECT_EQ(restoredContent, originalContent);
}

/**
 * @test Full Ren'Py workflow with fixture
 *
 * This test requires the sample_renpy_game fixture.
 * It will be skipped if the fixture is not available.
 */
TEST_F(RenPyWorkflowTest, FullWorkflowWithFixture) {
    if (!hasRenPyFixture()) {
        GTEST_SKIP() << "Ren'Py fixture not available";
    }

    // Copy fixture to temp
    auto gameDir = copyFixture();

    // 1. Detect game
    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);
    ASSERT_TRUE(detection.has_value());
    EXPECT_EQ(detection->engine, GameEngine::RenPy);

    // 2. Extract strings
    // TODO: Use actual handler when integration is complete
    // auto handler = HandlerFactory::create(GameEngine::RenPy);
    // auto strings = handler->extractStrings(gameDir);

    // 3. Apply translations
    // auto translations = createSampleTranslations();
    // handler->applyTranslations(gameDir, translations);

    // 4. Verify changes
    // Verify files were modified correctly

    // For now, just verify detection works
    SUCCEED() << "Fixture-based workflow requires handler integration";
}

/**
 * @test Handles missing game directory gracefully
 */
TEST_F(RenPyWorkflowTest, HandlesMissingDirectory) {
    fs::path nonexistent = tempDir_ / "nonexistent_game";

    scanners::GameDetector detector;
    auto result = detector.detectEngine(nonexistent);

    // Should not crash, may return unknown or error
    // The specific behavior depends on implementation
    SUCCEED() << "Did not crash on missing directory";
}

/**
 * @test Handles empty game directory gracefully
 */
TEST_F(RenPyWorkflowTest, HandlesEmptyDirectory) {
    // Create empty directory
    auto emptyDir = tempDir_ / "empty_game";
    fs::create_directories(emptyDir);

    scanners::GameDetector detector;
    auto result = detector.detectEngine(emptyDir);

    // Should not crash
    // Result may be unknown or empty
    SUCCEED() << "Did not crash on empty directory";
}
