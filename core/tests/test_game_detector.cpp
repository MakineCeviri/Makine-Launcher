/**
 * @file test_game_detector.cpp
 * @brief Unit tests for GameDetector module
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/game_detector.hpp>
#include <makineai/types.hpp>
#include <filesystem>

namespace makineai {
namespace testing {

class GameDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

// Test engine detection
TEST_F(GameDetectorTest, DetectUnityMono) {
    // Create mock Unity Mono game structure
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_unity_mono";
    std::filesystem::create_directories(tempDir);
    std::filesystem::create_directories(tempDir / "GameName_Data" / "Managed");

    // Create Assembly-CSharp.dll (marker for Unity Mono)
    std::ofstream(tempDir / "GameName_Data" / "Managed" / "Assembly-CSharp.dll").close();
    std::ofstream(tempDir / "GameName.exe").close();

    auto& detector = GameDetector::instance();
    auto result = detector.detectGame(tempDir);

    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_EQ(result->engine, GameEngine::UnityMono);
    }

    // Cleanup
    std::filesystem::remove_all(tempDir);
}

TEST_F(GameDetectorTest, DetectUnityIL2CPP) {
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_unity_il2cpp";
    std::filesystem::create_directories(tempDir);
    std::filesystem::create_directories(tempDir / "GameName_Data" / "il2cpp_data");

    std::ofstream(tempDir / "GameAssembly.dll").close();
    std::ofstream(tempDir / "GameName.exe").close();

    auto& detector = GameDetector::instance();
    auto result = detector.detectGame(tempDir);

    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_EQ(result->engine, GameEngine::UnityIL2CPP);
    }

    std::filesystem::remove_all(tempDir);
}

TEST_F(GameDetectorTest, DetectUnrealEngine) {
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_unreal";
    std::filesystem::create_directories(tempDir / "Engine" / "Binaries");
    std::filesystem::create_directories(tempDir / "GameName" / "Content" / "Paks");

    std::ofstream(tempDir / "GameName" / "Content" / "Paks" / "pakchunk0-WindowsNoEditor.pak").close();
    std::ofstream(tempDir / "GameName.exe").close();

    auto& detector = GameDetector::instance();
    auto result = detector.detectGame(tempDir);

    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_EQ(result->engine, GameEngine::Unreal);
    }

    std::filesystem::remove_all(tempDir);
}

TEST_F(GameDetectorTest, DetectBethesda) {
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_bethesda";
    std::filesystem::create_directories(tempDir / "Data");

    std::ofstream(tempDir / "Data" / "Starfield - Main.ba2").close();
    std::ofstream(tempDir / "Starfield.exe").close();

    auto& detector = GameDetector::instance();
    auto result = detector.detectGame(tempDir);

    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_EQ(result->engine, GameEngine::Bethesda);
    }

    std::filesystem::remove_all(tempDir);
}

TEST_F(GameDetectorTest, DetectGameMaker) {
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_gamemaker";
    std::filesystem::create_directories(tempDir);

    // Create data.win file with FORM header
    std::ofstream dataFile(tempDir / "data.win", std::ios::binary);
    const char formHeader[] = "FORM";
    dataFile.write(formHeader, 4);
    uint32_t size = 1000;
    dataFile.write(reinterpret_cast<const char*>(&size), 4);
    dataFile.close();

    std::ofstream(tempDir / "Game.exe").close();

    auto& detector = GameDetector::instance();
    auto result = detector.detectGame(tempDir);

    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_EQ(result->engine, GameEngine::GameMaker);
    }

    std::filesystem::remove_all(tempDir);
}

// Test store detection
TEST_F(GameDetectorTest, DetectSteamStore) {
    // This test requires Steam to be installed
    // Skip if Steam not available
    GTEST_SKIP() << "Requires Steam installation";
}

TEST_F(GameDetectorTest, DetectEpicStore) {
    // This test requires Epic Games Launcher to be installed
    GTEST_SKIP() << "Requires Epic Games installation";
}

TEST_F(GameDetectorTest, DetectGOGStore) {
    // This test requires GOG Galaxy to be installed
    GTEST_SKIP() << "Requires GOG Galaxy installation";
}

// Test path validation
TEST_F(GameDetectorTest, InvalidPathReturnsNullopt) {
    auto& detector = GameDetector::instance();
    auto result = detector.detectGame("C:\\NonExistent\\Path\\That\\Does\\Not\\Exist");

    EXPECT_FALSE(result.has_value());
}

// Test hash verification
TEST_F(GameDetectorTest, VerifyGameHash) {
    // Create a temporary executable
    auto tempDir = std::filesystem::temp_directory_path() / "makineai_test_hash";
    std::filesystem::create_directories(tempDir);

    auto exePath = tempDir / "test.exe";
    std::ofstream exe(exePath, std::ios::binary);
    exe << "Test executable content";
    exe.close();

    GameInfo game;
    game.executablePath = exePath;
    game.installPath = tempDir;

    auto& detector = GameDetector::instance();
    bool verified = detector.verify(game);

    // Should return true for any valid executable
    EXPECT_TRUE(verified);

    std::filesystem::remove_all(tempDir);
}

} // namespace testing
} // namespace makineai
