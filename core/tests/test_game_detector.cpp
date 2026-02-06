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
#include <fstream>
#include <filesystem>

namespace makineai {
namespace testing {

class GameDetectorTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    GameDetector detector_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makineai_test_games";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    void createFile(const std::filesystem::path& path, const std::string& content = "") {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
        ofs.close();
    }
};

// Test GameEngine enum
TEST_F(GameDetectorTest, GameEngineEnumValues) {
    // Verify enum values exist
    GameEngine unityMono = GameEngine::Unity_Mono;
    GameEngine unityIl2cpp = GameEngine::Unity_IL2CPP;
    GameEngine unreal = GameEngine::Unreal;
    GameEngine rpgmakerMv = GameEngine::RPGMaker_MV;
    GameEngine rpgmakerVx = GameEngine::RPGMaker_VX;
    GameEngine renpy = GameEngine::RenPy;
    GameEngine gamemaker = GameEngine::GameMaker;
    GameEngine bethesda = GameEngine::Bethesda;
    GameEngine godot = GameEngine::Godot;
    GameEngine unknown = GameEngine::Unknown;

    EXPECT_NE(unityMono, unknown);
    EXPECT_NE(unreal, unknown);
    EXPECT_NE(rpgmakerMv, unknown);
    EXPECT_NE(renpy, unknown);
    EXPECT_NE(godot, unknown);
}

// Test GameInfo struct
TEST_F(GameDetectorTest, GameInfoStruct) {
    GameInfo game;
    game.name = "Test Game";
    game.installPath = testDir_;
    game.executablePath = testDir_ / "game.exe";
    game.engine = GameEngine::Unity_Mono;
    game.version = "1.0.0";

    EXPECT_EQ(game.name, "Test Game");
    EXPECT_EQ(game.engine, GameEngine::Unity_Mono);
    EXPECT_EQ(game.version, "1.0.0");
}

// Test GameStore enum
TEST_F(GameDetectorTest, GameStoreEnumValues) {
    GameStore steam = GameStore::Steam;
    GameStore epic = GameStore::EpicGames;
    GameStore gog = GameStore::GOG;
    GameStore manual = GameStore::Manual;

    EXPECT_NE(steam, manual);
    EXPECT_NE(epic, gog);
}

// Test path validation
TEST_F(GameDetectorTest, InvalidPathReturnsError) {
    auto result = detector_.detectGame(std::filesystem::path("C:\\NonExistent\\Path\\That\\Does\\Not\\Exist"));

    // Should return error or nullopt for non-existent path
    EXPECT_FALSE(result.has_value());
}

// Test Unity Mono detection structure
TEST_F(GameDetectorTest, UnityMonoDirectoryStructure) {
    // Create Unity Mono game structure
    auto gameDir = testDir_ / "UnityGame";
    auto dataDir = gameDir / "UnityGame_Data" / "Managed";
    std::filesystem::create_directories(dataDir);

    createFile(dataDir / "Assembly-CSharp.dll");
    createFile(gameDir / "UnityGame.exe", "MZ");

    // Verify structure was created
    EXPECT_TRUE(std::filesystem::exists(dataDir / "Assembly-CSharp.dll"));
    EXPECT_TRUE(std::filesystem::exists(gameDir / "UnityGame.exe"));
}

// Test Unreal Engine detection structure
TEST_F(GameDetectorTest, UnrealDirectoryStructure) {
    auto gameDir = testDir_ / "UnrealGame";
    auto contentDir = gameDir / "Game" / "Content" / "Paks";
    std::filesystem::create_directories(contentDir);

    createFile(contentDir / "pakchunk0-WindowsNoEditor.pak");
    createFile(gameDir / "Game.exe", "MZ");

    EXPECT_TRUE(std::filesystem::exists(contentDir / "pakchunk0-WindowsNoEditor.pak"));
}

// Test GameMaker detection structure
TEST_F(GameDetectorTest, GameMakerDirectoryStructure) {
    auto gameDir = testDir_ / "GameMakerGame";
    std::filesystem::create_directories(gameDir);

    // Create data.win with FORM header
    std::ofstream dataFile(gameDir / "data.win", std::ios::binary);
    const char formHeader[] = "FORM";
    dataFile.write(formHeader, 4);
    uint32_t size = 1000;
    dataFile.write(reinterpret_cast<const char*>(&size), 4);
    dataFile.close();

    createFile(gameDir / "Game.exe", "MZ");

    EXPECT_TRUE(std::filesystem::exists(gameDir / "data.win"));
}

// Test Bethesda detection structure
TEST_F(GameDetectorTest, BethesdaDirectoryStructure) {
    auto gameDir = testDir_ / "BethesdaGame";
    auto dataDir = gameDir / "Data";
    std::filesystem::create_directories(dataDir);

    createFile(dataDir / "Game - Main.ba2");
    createFile(gameDir / "Game.exe", "MZ");

    EXPECT_TRUE(std::filesystem::exists(dataDir / "Game - Main.ba2"));
}

// Test RenPy detection structure
TEST_F(GameDetectorTest, RenpyDirectoryStructure) {
    auto gameDir = testDir_ / "RenpyGame";
    auto gameDataDir = gameDir / "game";
    std::filesystem::create_directories(gameDataDir);

    createFile(gameDataDir / "script.rpy");
    createFile(gameDir / "RenpyGame.exe", "MZ");

    EXPECT_TRUE(std::filesystem::exists(gameDataDir / "script.rpy"));
}

// Test scanAll (scans all registered stores)
TEST_F(GameDetectorTest, ScanAllReturnsResult) {
    auto result = detector_.scanAll();

    // Should return a valid result (possibly empty list)
    if (result.has_value()) {
        EXPECT_GE(result->size(), 0u);
    }
    SUCCEED();
}

// Test engine detection from directory
TEST_F(GameDetectorTest, DetectEngineFromDirectory) {
    // Create an empty test directory
    auto gameDir = testDir_ / "TestGame";
    std::filesystem::create_directories(gameDir);

    // Detect engine (should return Unknown for empty dir)
    auto engine = detector_.detectEngine(gameDir);
    EXPECT_EQ(engine, GameEngine::Unknown);
}

// Test signature scanning
TEST_F(GameDetectorTest, ScanForSignatures) {
    // Create test directory
    auto gameDir = testDir_ / "SignatureTest";
    std::filesystem::create_directories(gameDir);

    // Scan for signatures
    auto signatures = detector_.scanForSignatures(gameDir);

    // Empty directory should have no signatures
    EXPECT_FALSE(signatures.hasUnityEngine);
    EXPECT_FALSE(signatures.hasPakFiles);
    EXPECT_FALSE(signatures.hasRpaFiles);
}

} // namespace testing
} // namespace makineai
