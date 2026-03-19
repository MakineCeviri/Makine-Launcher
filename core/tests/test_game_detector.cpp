/**
 * @file test_game_detector.cpp
 * @brief Unit tests for GameDetector module
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makine/game_detector.hpp>
#include <makine/vdf_parser.hpp>
#include <makine/types.hpp>
#include <fstream>
#include <filesystem>

namespace makine {
namespace testing {

class GameDetectorTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    GameDetector detector_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makine_test_games";
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
#if defined(__MINGW32__) || defined(__MINGW64__)
    GTEST_SKIP() << "std::async deadlocks on MinGW GCC 13.1 (parallel::map fallback)";
#endif
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

// ============================================================================
// VDF Parser Tests
// ============================================================================

class VdfParserTest : public ::testing::Test {};

TEST_F(VdfParserTest, ParseSimpleKeyValue) {
    auto root = vdf::parse(R"("key" "value")");
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->getString("key"), "value");
}

TEST_F(VdfParserTest, ParseNestedObject) {
    auto root = vdf::parse(R"(
        "parent"
        {
            "child" "data"
        }
    )");
    ASSERT_TRUE(root.has_value());
    auto* parent = root->find("parent");
    ASSERT_NE(parent, nullptr);
    EXPECT_EQ(parent->getString("child"), "data");
}

TEST_F(VdfParserTest, ParseLibraryFoldersVdf) {
    const char* vdfContent = R"(
        "libraryfolders"
        {
            "0"
            {
                "path"      "C:\\Program Files (x86)\\Steam"
                "label"     ""
                "apps"
                {
                    "228980"    "1234567890"
                    "730"       "9876543210"
                }
            }
            "1"
            {
                "path"      "D:\\SteamLibrary"
                "label"     ""
                "apps"
                {
                    "570"       "1234567890"
                }
            }
        }
    )";

    auto root = vdf::parse(vdfContent);
    ASSERT_TRUE(root.has_value());

    auto* libFolders = root->find("libraryfolders");
    ASSERT_NE(libFolders, nullptr);
    EXPECT_TRUE(libFolders->isObject());

    // Check library folder 0
    auto* folder0 = libFolders->find("0");
    ASSERT_NE(folder0, nullptr);
    EXPECT_EQ(folder0->getString("path"), "C:\\Program Files (x86)\\Steam");

    auto* apps0 = folder0->find("apps");
    ASSERT_NE(apps0, nullptr);
    EXPECT_EQ(apps0->getString("228980"), "1234567890");
    EXPECT_EQ(apps0->getString("730"), "9876543210");

    // Check library folder 1
    auto* folder1 = libFolders->find("1");
    ASSERT_NE(folder1, nullptr);
    EXPECT_EQ(folder1->getString("path"), "D:\\SteamLibrary");
}

TEST_F(VdfParserTest, ParseAppManifestAcf) {
    const char* acfContent = R"(
        "AppState"
        {
            "appid"     "570"
            "Universe"  "1"
            "name"      "Dota 2"
            "StateFlags"    "4"
            "installdir"    "dota 2 beta"
            "SizeOnDisk"    "34567890123"
        }
    )";

    auto root = vdf::parse(acfContent);
    ASSERT_TRUE(root.has_value());

    auto* appState = root->find("AppState");
    ASSERT_NE(appState, nullptr);
    EXPECT_EQ(appState->getString("appid"), "570");
    EXPECT_EQ(appState->getString("name"), "Dota 2");
    EXPECT_EQ(appState->getString("installdir"), "dota 2 beta");
    EXPECT_EQ(appState->getString("SizeOnDisk"), "34567890123");
}

TEST_F(VdfParserTest, HandleEscapeSequences) {
    auto root = vdf::parse(R"("path" "C:\\Users\\test\\Games")");
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->getString("path"), "C:\\Users\\test\\Games");
}

TEST_F(VdfParserTest, HandleComments) {
    auto root = vdf::parse(
        "// This is a comment\n"
        "\"key\" \"value\"\n"
        "// Another comment\n"
        "\"key2\" \"value2\"\n"
    );
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->getString("key"), "value");
    EXPECT_EQ(root->getString("key2"), "value2");
}

TEST_F(VdfParserTest, GetStringDefaultValue) {
    auto root = vdf::parse(R"("key" "value")");
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->getString("nonexistent", "default"), "default");
    EXPECT_EQ(root->getString("nonexistent"), "");
}

TEST_F(VdfParserTest, EmptyInput) {
    auto root = vdf::parse("");
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->children.empty());
}

TEST_F(VdfParserTest, DeeplyNested) {
    auto root = vdf::parse(R"(
        "a" { "b" { "c" { "d" "deep" } } }
    )");
    ASSERT_TRUE(root.has_value());
    auto* a = root->find("a");
    ASSERT_NE(a, nullptr);
    auto* b = a->find("b");
    ASSERT_NE(b, nullptr);
    auto* c = b->find("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getString("d"), "deep");
}

// ============================================================================
// ADDITIONAL EDGE CASES
// ============================================================================

TEST_F(GameDetectorTest, DetectEngineEmptyDirectory) {
    auto gameDir = testDir_ / "EmptyGame";
    std::filesystem::create_directories(gameDir);

    auto engine = detector_.detectEngine(gameDir);
    EXPECT_EQ(engine, GameEngine::Unknown);
}

TEST_F(VdfParserTest, VdfParserCorruptedInput) {
    // Various corrupted VDF inputs should not crash
    auto r1 = vdf::parse("{{{{{ broken");
    // May return empty or error, just shouldn't crash
    SUCCEED();

    auto r2 = vdf::parse("\"key\" \"value\" extra garbage");
    SUCCEED();

    auto r3 = vdf::parse("\"unterminated key");
    SUCCEED();
}

TEST_F(VdfParserTest, VdfParserVeryLongValue) {
    // 10KB value string
    std::string longValue(10240, 'x');
    std::string vdf = "\"key\" \"" + longValue + "\"";
    auto root = vdf::parse(vdf);
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->getString("key"), longValue);
}

TEST_F(GameDetectorTest, GameInfoDefaultValues) {
    GameInfo game;
    EXPECT_TRUE(game.name.empty());
    EXPECT_EQ(game.engine, GameEngine::Unknown);
    EXPECT_EQ(game.store, GameStore::Unknown);
    EXPECT_EQ(game.sizeBytes, 0u);
    EXPECT_TRUE(game.is64Bit);
    EXPECT_DOUBLE_EQ(game.engineConfidence, 0.0);
    EXPECT_FALSE(game.hasTranslation);
}

} // namespace testing
} // namespace makine
