/**
 * @file test_asset_parser.cpp
 * @brief Unit tests for AssetParser module
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/asset_parser.hpp>
#include <fstream>
#include <filesystem>

namespace makineai {
namespace testing {

class AssetParserTest : public ::testing::Test {
protected:
    AssetParser parser_;
    std::filesystem::path testDataDir_;

    void SetUp() override {
        testDataDir_ = std::filesystem::temp_directory_path() / "makineai_asset_tests";
        std::filesystem::create_directories(testDataDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDataDir_);
    }

    // Helper to create a mock BA2 file
    void createMockBA2(const std::filesystem::path& path) {
        std::ofstream fs(path, std::ios::binary);

        // Write BA2 header (BTDX magic)
        const uint32_t magic = 0x58445442; // "BTDX"
        const uint32_t version = 1;
        const uint32_t type = 0x4C524E47; // "GNRL"
        const uint32_t fileCount = 0;
        const uint64_t nameTableOffset = 0;

        fs.write(reinterpret_cast<const char*>(&magic), 4);
        fs.write(reinterpret_cast<const char*>(&version), 4);
        fs.write(reinterpret_cast<const char*>(&type), 4);
        fs.write(reinterpret_cast<const char*>(&fileCount), 4);
        fs.write(reinterpret_cast<const char*>(&nameTableOffset), 8);

        fs.close();
    }

    // Helper to create a mock UnityFS bundle
    void createMockUnityBundle(const std::filesystem::path& path) {
        std::ofstream fs(path, std::ios::binary);

        // Write UnityFS header
        const char* signature = "UnityFS\0";
        fs.write(signature, 8);

        // Version
        const uint32_t version = 6;
        fs.write(reinterpret_cast<const char*>(&version), 4);

        fs.close();
    }

    // Helper to create a mock PAK file
    void createMockPAK(const std::filesystem::path& path) {
        std::ofstream fs(path, std::ios::binary);

        // PAK files have footer at the end
        // Write some padding first
        std::vector<uint8_t> padding(100, 0);
        fs.write(reinterpret_cast<const char*>(padding.data()), padding.size());

        // Write footer (magic at end - 44 bytes)
        const uint32_t magic = 0x5A6F12E1;
        const uint32_t version = 9;
        const uint64_t indexOffset = 0;
        const uint64_t indexSize = 0;
        uint8_t indexHash[20] = {0};

        fs.write(reinterpret_cast<const char*>(&magic), 4);
        fs.write(reinterpret_cast<const char*>(&version), 4);
        fs.write(reinterpret_cast<const char*>(&indexOffset), 8);
        fs.write(reinterpret_cast<const char*>(&indexSize), 8);
        fs.write(reinterpret_cast<const char*>(indexHash), 20);

        fs.close();
    }

    // Helper to create mock GameMaker data.win
    void createMockDataWin(const std::filesystem::path& path) {
        std::ofstream fs(path, std::ios::binary);

        // FORM header
        const char* magic = "FORM";
        fs.write(magic, 4);

        uint32_t size = 100;
        fs.write(reinterpret_cast<const char*>(&size), 4);

        // Some padding
        std::vector<uint8_t> data(100, 0);
        fs.write(reinterpret_cast<const char*>(data.data()), data.size());

        fs.close();
    }
};

// Test format detection
TEST_F(AssetParserTest, DetectBA2Format) {
    // Bethesda detection looks for .ba2 files in the Data/ subfolder
    auto dataDir = testDataDir_ / "Data";
    std::filesystem::create_directories(dataDir);

    auto ba2Path = dataDir / "test.ba2";
    createMockBA2(ba2Path);

    auto engine = parser_.detectEngine(testDataDir_);

    // Should detect Bethesda engine
    EXPECT_EQ(engine, GameEngine::Bethesda);
}

TEST_F(AssetParserTest, DetectUnityBundleFormat) {
    // Create Unity-like directory structure
    auto dataDir = testDataDir_ / "Game_Data";
    std::filesystem::create_directories(dataDir);

    auto bundlePath = dataDir / "resources.assets";
    createMockUnityBundle(bundlePath);

    // Parser should recognize Unity format
    EXPECT_TRUE(std::filesystem::exists(bundlePath));
}

TEST_F(AssetParserTest, DetectPAKFormat) {
    auto pakDir = testDataDir_ / "Content" / "Paks";
    std::filesystem::create_directories(pakDir);

    auto pakPath = pakDir / "pakchunk0.pak";
    createMockPAK(pakPath);

    auto engine = parser_.detectEngine(testDataDir_);

    // Should detect Unreal engine
    EXPECT_EQ(engine, GameEngine::Unreal);
}

TEST_F(AssetParserTest, DetectGameMakerFormat) {
    auto dataPath = testDataDir_ / "data.win";
    createMockDataWin(dataPath);

    auto engine = parser_.detectEngine(testDataDir_);

    EXPECT_EQ(engine, GameEngine::GameMaker);
}

// Test parsing
TEST_F(AssetParserTest, ParseBA2File) {
    auto ba2Path = testDataDir_ / "test.ba2";
    createMockBA2(ba2Path);

    auto result = parser_.parseFile(ba2Path);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
}

TEST_F(AssetParserTest, ParseNonExistentFile) {
    auto result = parser_.parseFile("C:\\NonExistent\\File.ba2");

    // Should return error
    EXPECT_FALSE(result.has_value());
}

// Test getParserForFile
TEST_F(AssetParserTest, GetParserForBA2) {
    auto ba2Path = testDataDir_ / "test.ba2";
    createMockBA2(ba2Path);

    auto* parserPtr = parser_.getParserForFile(ba2Path);
    EXPECT_NE(parserPtr, nullptr);
}

TEST_F(AssetParserTest, GetParserForRandomFile) {
    auto randomPath = testDataDir_ / "random.txt";
    std::ofstream(randomPath) << "Random content";

    auto* parserPtr = parser_.getParserForFile(randomPath);
    EXPECT_EQ(parserPtr, nullptr);
}

// Test string extraction (for translatable content)
TEST_F(AssetParserTest, ExtractStringsFromBA2) {
    // Skip if real BA2 test file not available
    GTEST_SKIP() << "Requires real BA2 test file";
}

// Test format-specific functionality
TEST_F(AssetParserTest, BA2StringsFileParsing) {
    // Create mock .strings file
    auto stringsPath = testDataDir_ / "test.strings";
    std::ofstream fs(stringsPath, std::ios::binary);

    // Header: count + dataSize
    uint32_t count = 2;
    uint32_t dataSize = 20;
    fs.write(reinterpret_cast<const char*>(&count), 4);
    fs.write(reinterpret_cast<const char*>(&dataSize), 4);

    // Directory entries (ID + offset)
    uint32_t id1 = 1, offset1 = 0;
    uint32_t id2 = 2, offset2 = 6;
    fs.write(reinterpret_cast<const char*>(&id1), 4);
    fs.write(reinterpret_cast<const char*>(&offset1), 4);
    fs.write(reinterpret_cast<const char*>(&id2), 4);
    fs.write(reinterpret_cast<const char*>(&offset2), 4);

    // String data (null-terminated)
    fs << "Hello" << '\0' << "World" << '\0';

    fs.close();

    // Test parsing
    auto result = parser_.parseFile(stringsPath);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(result->strings.size(), 2);
}

// Test registered parsers
TEST_F(AssetParserTest, HasRegisteredParsers) {
    const auto& parsers = parser_.parsers();
    EXPECT_GT(parsers.size(), 0);
}

} // namespace testing
} // namespace makineai
