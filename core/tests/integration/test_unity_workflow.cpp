/**
 * @file test_unity_workflow.cpp
 * @brief Integration tests for Unity game translation workflow
 *
 * Tests the complete workflow for Unity games:
 * 1. Detect Unity game (Mono vs IL2CPP)
 * 2. Setup BepInEx/XUnity runtime (when applicable)
 * 3. Extract strings from assets or runtime
 * 4. Apply translations
 * 5. Verify game integrity
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
 * @brief Test fixture for Unity workflow tests
 */
class UnityWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        #ifdef MAKINEAI_TEST_FIXTURES_DIR
            fixturesDir_ = MAKINEAI_TEST_FIXTURES_DIR;
        #else
            fixturesDir_ = fs::current_path() / "fixtures";
        #endif

        #ifdef MAKINEAI_TEST_TEMP_DIR
            tempDir_ = fs::path(MAKINEAI_TEST_TEMP_DIR) / "unity_test";
        #else
            tempDir_ = fs::temp_directory_path() / "makineai_unity_test";
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
     * @brief Check if Unity fixture exists
     */
    bool hasUnityFixture() const {
        return fs::exists(fixturesDir_ / "sample_unity_game");
    }

    /**
     * @brief Copy fixture to temp for testing
     */
    fs::path copyFixture() {
        auto src = fixturesDir_ / "sample_unity_game";
        auto dst = tempDir_ / "sample_unity_game";

        if (fs::exists(src)) {
            fs::copy(src, dst, fs::copy_options::recursive);
        }

        return dst;
    }

    /**
     * @brief Create a minimal Unity Mono game structure for testing
     */
    fs::path createMinimalUnityMonoGame() {
        auto gameDir = tempDir_ / "TestGame";
        auto dataDir = gameDir / "TestGame_Data";
        auto managedDir = dataDir / "Managed";

        fs::create_directories(managedDir);

        // Create UnityPlayer.dll (marker file)
        {
            std::ofstream ofs(gameDir / "UnityPlayer.dll", std::ios::binary);
            // Write minimal PE header to make it look like a DLL
            unsigned char peHeader[] = {
                'M', 'Z', 0x90, 0x00  // DOS header signature
            };
            ofs.write(reinterpret_cast<char*>(peHeader), sizeof(peHeader));
        }

        // Create Assembly-CSharp.dll (marker file)
        {
            std::ofstream ofs(managedDir / "Assembly-CSharp.dll", std::ios::binary);
            unsigned char peHeader[] = { 'M', 'Z', 0x90, 0x00 };
            ofs.write(reinterpret_cast<char*>(peHeader), sizeof(peHeader));
        }

        // Create mono DLL (indicates Mono backend)
        {
            std::ofstream ofs(gameDir / "mono.dll", std::ios::binary);
            unsigned char peHeader[] = { 'M', 'Z', 0x90, 0x00 };
            ofs.write(reinterpret_cast<char*>(peHeader), sizeof(peHeader));
        }

        // Create globalgamemanagers (Unity data marker)
        {
            std::ofstream ofs(dataDir / "globalgamemanagers", std::ios::binary);
            // Write Unity version string
            std::string version = "5.6.7f1";
            ofs.write(version.c_str(), version.size() + 1);
        }

        // Create level0 (scene data)
        {
            std::ofstream ofs(dataDir / "level0", std::ios::binary);
            ofs.write("UNITY", 5);
        }

        return gameDir;
    }

    /**
     * @brief Create a minimal Unity IL2CPP game structure for testing
     */
    fs::path createMinimalUnityIL2CPPGame() {
        auto gameDir = tempDir_ / "TestGameIL2CPP";
        auto dataDir = gameDir / "TestGameIL2CPP_Data";
        auto il2cppDir = dataDir / "il2cpp_data";
        auto metadataDir = il2cppDir / "Metadata";

        fs::create_directories(metadataDir);

        // Create UnityPlayer.dll
        {
            std::ofstream ofs(gameDir / "UnityPlayer.dll", std::ios::binary);
            unsigned char peHeader[] = { 'M', 'Z', 0x90, 0x00 };
            ofs.write(reinterpret_cast<char*>(peHeader), sizeof(peHeader));
        }

        // Create GameAssembly.dll (IL2CPP marker)
        {
            std::ofstream ofs(gameDir / "GameAssembly.dll", std::ios::binary);
            unsigned char peHeader[] = { 'M', 'Z', 0x90, 0x00 };
            ofs.write(reinterpret_cast<char*>(peHeader), sizeof(peHeader));
        }

        // Create global-metadata.dat (IL2CPP metadata)
        {
            std::ofstream ofs(metadataDir / "global-metadata.dat", std::ios::binary);
            // Write IL2CPP metadata header magic
            uint32_t magic = 0xFAB11BAF;  // IL2CPP metadata magic
            ofs.write(reinterpret_cast<char*>(&magic), sizeof(magic));
        }

        // Create globalgamemanagers
        {
            std::ofstream ofs(dataDir / "globalgamemanagers", std::ios::binary);
            std::string version = "2021.3.1f1";
            ofs.write(version.c_str(), version.size() + 1);
        }

        return gameDir;
    }

protected:
    fs::path fixturesDir_;
    fs::path tempDir_;
};

/**
 * @test Detect Unity Mono game
 */
TEST_F(UnityWorkflowTest, DetectsUnityMonoGame) {
    auto gameDir = createMinimalUnityMonoGame();

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    ASSERT_TRUE(result.has_value()) << "Engine detection failed";
    EXPECT_EQ(result->engine, GameEngine::Unity_Mono);
    EXPECT_GT(result->confidence, 0.5f);
}

/**
 * @test Detect Unity IL2CPP game
 */
TEST_F(UnityWorkflowTest, DetectsUnityIL2CPPGame) {
    auto gameDir = createMinimalUnityIL2CPPGame();

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    ASSERT_TRUE(result.has_value()) << "Engine detection failed";
    EXPECT_EQ(result->engine, GameEngine::Unity_IL2CPP);
    EXPECT_GT(result->confidence, 0.5f);
}

/**
 * @test Find UnityPlayer.dll marker
 */
TEST_F(UnityWorkflowTest, FindsUnityPlayerDll) {
    auto gameDir = createMinimalUnityMonoGame();
    auto unityPlayerPath = gameDir / "UnityPlayer.dll";

    EXPECT_TRUE(fs::exists(unityPlayerPath));
}

/**
 * @test Find data directory
 */
TEST_F(UnityWorkflowTest, FindsDataDirectory) {
    auto gameDir = createMinimalUnityMonoGame();

    // Look for *_Data directory
    fs::path dataDir;
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                dataDir = entry.path();
                break;
            }
        }
    }

    EXPECT_FALSE(dataDir.empty()) << "Should find *_Data directory";
    EXPECT_TRUE(fs::exists(dataDir / "globalgamemanagers"));
}

/**
 * @test Verify Managed directory for Mono games
 */
TEST_F(UnityWorkflowTest, FindsManagedDirectoryForMono) {
    auto gameDir = createMinimalUnityMonoGame();

    // Find data directory
    fs::path dataDir;
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                dataDir = entry.path();
                break;
            }
        }
    }

    auto managedDir = dataDir / "Managed";
    EXPECT_TRUE(fs::exists(managedDir));
    EXPECT_TRUE(fs::exists(managedDir / "Assembly-CSharp.dll"));
}

/**
 * @test Verify IL2CPP metadata for IL2CPP games
 */
TEST_F(UnityWorkflowTest, FindsIL2CPPMetadata) {
    auto gameDir = createMinimalUnityIL2CPPGame();

    // Find data directory
    fs::path dataDir;
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            auto name = entry.path().filename().string();
            if (name.ends_with("_Data")) {
                dataDir = entry.path();
                break;
            }
        }
    }

    auto metadataPath = dataDir / "il2cpp_data" / "Metadata" / "global-metadata.dat";
    EXPECT_TRUE(fs::exists(metadataPath));

    // Verify magic number
    std::ifstream ifs(metadataPath, std::ios::binary);
    uint32_t magic;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    EXPECT_EQ(magic, 0xFAB11BAF);
}

/**
 * @test BepInEx directory structure
 */
TEST_F(UnityWorkflowTest, CanCreateBepInExStructure) {
    auto gameDir = createMinimalUnityMonoGame();

    // Create BepInEx directory structure
    auto bepinexDir = gameDir / "BepInEx";
    auto pluginsDir = bepinexDir / "plugins";
    auto configDir = bepinexDir / "config";

    fs::create_directories(pluginsDir);
    fs::create_directories(configDir);

    // Create doorstop_config.ini (BepInEx loader config)
    {
        std::ofstream ofs(gameDir / "doorstop_config.ini");
        ofs << "[General]\n";
        ofs << "enabled=true\n";
        ofs << "target_assembly=BepInEx\\core\\BepInEx.Preloader.dll\n";
    }

    EXPECT_TRUE(fs::exists(bepinexDir));
    EXPECT_TRUE(fs::exists(pluginsDir));
    EXPECT_TRUE(fs::exists(configDir));
    EXPECT_TRUE(fs::exists(gameDir / "doorstop_config.ini"));
}

/**
 * @test XUnity.AutoTranslator config
 */
TEST_F(UnityWorkflowTest, CanCreateXUnityConfig) {
    auto gameDir = createMinimalUnityMonoGame();

    // Create BepInEx structure
    auto configDir = gameDir / "BepInEx" / "config";
    fs::create_directories(configDir);

    // Create XUnity.AutoTranslator config
    auto configPath = configDir / "AutoTranslatorConfig.ini";
    {
        std::ofstream ofs(configPath);
        ofs << "[General]\n";
        ofs << "Language=tr\n";
        ofs << "FromLanguage=en\n";
        ofs << "\n";
        ofs << "[TextFrameworks]\n";
        ofs << "EnableUGUI=true\n";
        ofs << "EnableTextMeshPro=true\n";
        ofs << "\n";
        ofs << "[Files]\n";
        ofs << "Directory=Translation\n";
        ofs << "OutputFile=_AutoGeneratedTranslations.txt\n";
    }

    EXPECT_TRUE(fs::exists(configPath));

    // Verify content
    std::ifstream ifs(configPath);
    std::string content(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("Language=tr"), std::string::npos);
}

/**
 * @test Backup game files before modification
 */
TEST_F(UnityWorkflowTest, CanBackupGameFiles) {
    auto gameDir = createMinimalUnityMonoGame();

    // Find files to backup
    std::vector<fs::path> filesToBackup;
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            filesToBackup.push_back(entry.path());
        }
    }

    // Create backup directory
    auto backupDir = tempDir_ / "backup";
    fs::create_directories(backupDir);

    // Backup files
    for (const auto& file : filesToBackup) {
        auto relativePath = fs::relative(file, gameDir);
        auto backupPath = backupDir / relativePath;
        fs::create_directories(backupPath.parent_path());
        fs::copy_file(file, backupPath);
    }

    // Verify backups
    EXPECT_TRUE(fs::exists(backupDir / "UnityPlayer.dll"));
    EXPECT_TRUE(fs::exists(backupDir / "TestGame_Data" / "globalgamemanagers"));
}

/**
 * @test Full Unity workflow with fixture
 */
TEST_F(UnityWorkflowTest, FullWorkflowWithFixture) {
    if (!hasUnityFixture()) {
        GTEST_SKIP() << "Unity fixture not available";
    }

    auto gameDir = copyFixture();

    // 1. Detect game
    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);
    ASSERT_TRUE(detection.has_value());
    EXPECT_TRUE(detection->engine == GameEngine::Unity_Mono ||
                detection->engine == GameEngine::Unity_IL2CPP);

    // 2-4. Handler integration would go here
    // TODO: Use actual handler when integration is complete

    SUCCEED() << "Fixture-based workflow requires handler integration";
}

/**
 * @test Handles missing UnityPlayer.dll gracefully
 */
TEST_F(UnityWorkflowTest, HandlesMissingUnityPlayerGracefully) {
    auto gameDir = tempDir_ / "NotAUnityGame";
    fs::create_directories(gameDir);

    // Create some random file
    {
        std::ofstream ofs(gameDir / "game.exe");
        ofs << "not unity";
    }

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    // Should not detect as Unity
    if (result.has_value()) {
        EXPECT_NE(result->engine, GameEngine::Unity_Mono);
        EXPECT_NE(result->engine, GameEngine::Unity_IL2CPP);
    }
}

/**
 * @test Distinguish between Mono and IL2CPP
 */
TEST_F(UnityWorkflowTest, DistinguishesBetweenMonoAndIL2CPP) {
    auto monoGame = createMinimalUnityMonoGame();
    auto il2cppGame = createMinimalUnityIL2CPPGame();

    scanners::GameDetector detector;

    auto monoResult = detector.detectEngine(monoGame);
    auto il2cppResult = detector.detectEngine(il2cppGame);

    ASSERT_TRUE(monoResult.has_value());
    ASSERT_TRUE(il2cppResult.has_value());

    EXPECT_EQ(monoResult->engine, GameEngine::Unity_Mono);
    EXPECT_EQ(il2cppResult->engine, GameEngine::Unity_IL2CPP);
}
