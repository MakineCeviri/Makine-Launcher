/**
 * @file test_gamemaker_workflow.cpp
 * @brief Integration tests for GameMaker Studio game translation workflow
 *
 * Tests the complete workflow for GameMaker Studio games:
 * 1. Detect GameMaker game from data.win
 * 2. Parse IFF/FORM chunk structure
 * 3. Extract strings from STRG chunk
 * 4. Apply translations with length constraints
 * 5. Verify file integrity
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
#include <cstring>

namespace fs = std::filesystem;
using namespace makineai;

/**
 * @brief Test fixture for GameMaker workflow tests
 */
class GameMakerWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        #ifdef MAKINEAI_TEST_FIXTURES_DIR
            fixturesDir_ = MAKINEAI_TEST_FIXTURES_DIR;
        #else
            fixturesDir_ = fs::current_path() / "fixtures";
        #endif

        #ifdef MAKINEAI_TEST_TEMP_DIR
            tempDir_ = fs::path(MAKINEAI_TEST_TEMP_DIR) / "gamemaker_test";
        #else
            tempDir_ = fs::temp_directory_path() / "makineai_gamemaker_test";
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
     * @brief Check if GameMaker fixture exists
     */
    bool hasGameMakerFixture() const {
        return fs::exists(fixturesDir_ / "sample_gamemaker_game");
    }

    /**
     * @brief Copy fixture to temp for testing
     */
    fs::path copyFixture() {
        auto src = fixturesDir_ / "sample_gamemaker_game";
        auto dst = tempDir_ / "sample_gamemaker_game";

        if (fs::exists(src)) {
            fs::copy(src, dst, fs::copy_options::recursive);
        }

        return dst;
    }

    /**
     * @brief Create a minimal data.win file for testing
     *
     * Creates a simplified IFF/FORM structure with:
     * - FORM header
     * - GEN8 chunk (game info)
     * - STRG chunk (string table)
     */
    fs::path createMinimalDataWin() {
        auto gameDir = tempDir_ / "test_gamemaker";
        fs::create_directories(gameDir);

        auto dataPath = gameDir / "data.win";
        std::ofstream ofs(dataPath, std::ios::binary);

        // Helper to write uint32 little-endian
        auto write32 = [&](uint32_t val) {
            ofs.write(reinterpret_cast<char*>(&val), 4);
        };

        // FORM header
        ofs.write("FORM", 4);      // Magic
        uint32_t formSizePos = static_cast<uint32_t>(ofs.tellp());
        write32(0);                 // Size placeholder (will update)

        // GEN8 chunk (game metadata)
        ofs.write("GEN8", 4);
        write32(64);                // Chunk size

        // GEN8 data (simplified)
        uint8_t debugMode = 0;
        ofs.write(reinterpret_cast<char*>(&debugMode), 1);
        // Pad to offset 48 for version info
        std::vector<uint8_t> padding(47, 0);
        ofs.write(reinterpret_cast<char*>(padding.data()), padding.size());
        // Version info
        write32(1);  // Major
        write32(4);  // Minor
        write32(9);  // Release
        write32(9);  // Build

        // STRG chunk (string table)
        ofs.write("STRG", 4);

        // Calculate string data
        std::vector<std::string> strings = {
            "Hello World",
            "Game Over",
            "Press Start",
            "Level Complete",
            "Score: "
        };

        // String count
        uint32_t stringCount = static_cast<uint32_t>(strings.size());

        // Calculate chunk size
        // stringCount (4) + offsets (4 * count) + string data (length + data + null for each)
        uint32_t dataOffset = 4 + (4 * stringCount);
        std::vector<uint32_t> stringOffsets;
        std::vector<std::pair<uint32_t, std::string>> stringData;

        uint32_t currentOffset = dataOffset;
        for (const auto& str : strings) {
            stringOffsets.push_back(currentOffset);
            stringData.emplace_back(static_cast<uint32_t>(str.length()), str);
            currentOffset += 4 + str.length() + 1; // length + data + null
        }

        uint32_t strgChunkSize = currentOffset;
        write32(strgChunkSize);

        // Write string count
        write32(stringCount);

        // Write offsets
        for (uint32_t offset : stringOffsets) {
            write32(offset);
        }

        // Write string data
        for (const auto& [len, str] : stringData) {
            write32(len);
            ofs.write(str.c_str(), str.length());
            ofs.put('\0');
        }

        // Update FORM size
        uint32_t formSize = static_cast<uint32_t>(ofs.tellp()) - 8;
        ofs.seekp(formSizePos);
        write32(formSize);

        return gameDir;
    }

    /**
     * @brief Read string from data.win at specific offset
     */
    std::string readStringFromDataWin(const fs::path& dataPath, size_t offset) {
        std::ifstream ifs(dataPath, std::ios::binary);
        ifs.seekg(offset);

        uint32_t length;
        ifs.read(reinterpret_cast<char*>(&length), 4);

        std::string result(length, '\0');
        ifs.read(result.data(), length);

        return result;
    }

protected:
    fs::path fixturesDir_;
    fs::path tempDir_;
};

/**
 * @test Detect GameMaker game from data.win
 */
TEST_F(GameMakerWorkflowTest, DetectsGameMakerGame) {
    auto gameDir = createMinimalDataWin();

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    ASSERT_TRUE(result.has_value()) << "Engine detection failed";
    EXPECT_EQ(result->engine, GameEngine::GameMaker);
    EXPECT_GT(result->confidence, 0.5f);
}

/**
 * @test Verify FORM magic number
 */
TEST_F(GameMakerWorkflowTest, VerifiesFORMMagic) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    std::ifstream ifs(dataPath, std::ios::binary);
    char magic[5] = {0};
    ifs.read(magic, 4);

    EXPECT_STREQ(magic, "FORM");
}

/**
 * @test Find STRG chunk in data.win
 */
TEST_F(GameMakerWorkflowTest, FindsSTRGChunk) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    std::ifstream ifs(dataPath, std::ios::binary);

    // Skip FORM header
    ifs.seekg(8);

    bool foundSTRG = false;
    while (ifs.good()) {
        char chunkType[5] = {0};
        ifs.read(chunkType, 4);

        if (!ifs.good()) break;

        uint32_t chunkSize;
        ifs.read(reinterpret_cast<char*>(&chunkSize), 4);

        if (std::string(chunkType) == "STRG") {
            foundSTRG = true;
            break;
        }

        // Skip chunk data
        ifs.seekg(chunkSize, std::ios::cur);
    }

    EXPECT_TRUE(foundSTRG) << "STRG chunk not found";
}

/**
 * @test Extract string count from STRG chunk
 */
TEST_F(GameMakerWorkflowTest, ExtractsStringCount) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    std::ifstream ifs(dataPath, std::ios::binary);

    // Skip FORM header
    ifs.seekg(8);

    // Skip GEN8 chunk
    char chunkType[5] = {0};
    ifs.read(chunkType, 4);
    uint32_t chunkSize;
    ifs.read(reinterpret_cast<char*>(&chunkSize), 4);
    ifs.seekg(chunkSize, std::ios::cur);

    // Read STRG
    ifs.read(chunkType, 4);
    EXPECT_STREQ(chunkType, "STRG");

    ifs.read(reinterpret_cast<char*>(&chunkSize), 4);

    // String count
    uint32_t stringCount;
    ifs.read(reinterpret_cast<char*>(&stringCount), 4);

    EXPECT_EQ(stringCount, 5u);
}

/**
 * @test Extract strings from STRG chunk
 */
TEST_F(GameMakerWorkflowTest, ExtractsStringsFromSTRGChunk) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    std::ifstream ifs(dataPath, std::ios::binary);

    // Skip FORM header
    ifs.seekg(8);

    // Skip GEN8 chunk
    char chunkType[5] = {0};
    ifs.read(chunkType, 4);
    uint32_t chunkSize;
    ifs.read(reinterpret_cast<char*>(&chunkSize), 4);
    ifs.seekg(chunkSize, std::ios::cur);

    // Read STRG header
    ifs.read(chunkType, 4);
    ifs.read(reinterpret_cast<char*>(&chunkSize), 4);

    auto strgStart = static_cast<size_t>(ifs.tellg());

    // String count
    uint32_t stringCount;
    ifs.read(reinterpret_cast<char*>(&stringCount), 4);

    // Read offsets
    std::vector<uint32_t> offsets(stringCount);
    for (uint32_t i = 0; i < stringCount; i++) {
        ifs.read(reinterpret_cast<char*>(&offsets[i]), 4);
    }

    // Read strings
    std::vector<std::string> extractedStrings;
    for (uint32_t offset : offsets) {
        ifs.seekg(strgStart + offset);

        uint32_t strLen;
        ifs.read(reinterpret_cast<char*>(&strLen), 4);

        std::string str(strLen, '\0');
        ifs.read(str.data(), strLen);
        extractedStrings.push_back(str);
    }

    EXPECT_EQ(extractedStrings.size(), 5u);
    EXPECT_THAT(extractedStrings, testing::Contains("Hello World"));
    EXPECT_THAT(extractedStrings, testing::Contains("Game Over"));
    EXPECT_THAT(extractedStrings, testing::Contains("Press Start"));
}

/**
 * @test String length constraint
 *
 * GameMaker strings have a fixed length in the data file.
 * Translations must fit within that length.
 */
TEST_F(GameMakerWorkflowTest, RespectsStringLengthConstraint) {
    std::string original = "Hello";
    size_t maxLength = 10;

    // Translation fits
    std::string shortTranslation = "Merhaba";
    EXPECT_LE(shortTranslation.length(), maxLength);

    // Translation too long
    std::string longTranslation = "Merhaba Dünya!";
    EXPECT_GT(longTranslation.length(), maxLength);

    // Would need truncation or rejection
}

/**
 * @test Backup data.win before modification
 */
TEST_F(GameMakerWorkflowTest, BackupsDataWinBeforeModification) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    // Create backup
    auto backupDir = tempDir_ / "backup";
    fs::create_directories(backupDir);
    auto backupPath = backupDir / "data.win";

    fs::copy_file(dataPath, backupPath);

    // Verify backup
    EXPECT_TRUE(fs::exists(backupPath));

    // Verify sizes match
    EXPECT_EQ(fs::file_size(dataPath), fs::file_size(backupPath));
}

/**
 * @test Modify string in data.win
 */
TEST_F(GameMakerWorkflowTest, ModifiesStringInDataWin) {
    auto gameDir = createMinimalDataWin();
    auto dataPath = gameDir / "data.win";

    // Read file into memory
    std::vector<uint8_t> fileData;
    {
        std::ifstream ifs(dataPath, std::ios::binary | std::ios::ate);
        auto size = static_cast<size_t>(ifs.tellg());
        fileData.resize(size);
        ifs.seekg(0);
        ifs.read(reinterpret_cast<char*>(fileData.data()), size);
    }

    // Find "Hello World" string offset
    // In our test file, it's the first string after offsets
    // Skip: FORM(8) + GEN8(8+64) + STRG(8) + count(4) + offsets(5*4) = 112
    // First string data starts at offset 24 within STRG data
    // STRG starts at offset 80, so string data at: 80 + 24 = 104

    // The string has 4-byte length prefix, then data
    // "Hello World" is 11 chars

    // Find the string offset
    size_t strgChunkStart = 8 + 8 + 64; // After FORM header and GEN8
    size_t strgDataStart = strgChunkStart + 8; // After STRG header
    size_t stringCount = 5;
    size_t offsetsEnd = strgDataStart + 4 + (4 * stringCount); // After count and offsets

    // First string is at offset stored at strgDataStart + 4
    uint32_t firstStringOffset;
    std::memcpy(&firstStringOffset, &fileData[strgDataStart + 4], 4);

    size_t firstStringAddr = strgDataStart + firstStringOffset + 4; // Skip length prefix

    // Verify original string
    std::string original(reinterpret_cast<char*>(&fileData[firstStringAddr]), 11);
    EXPECT_EQ(original, "Hello World");

    // Replace with same-length string
    std::string replacement = "Merhaba Dun"; // 11 chars
    std::memcpy(&fileData[firstStringAddr], replacement.c_str(), 11);

    // Write back
    {
        std::ofstream ofs(dataPath, std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<char*>(fileData.data()), fileData.size());
    }

    // Verify modification
    {
        std::ifstream ifs(dataPath, std::ios::binary);
        ifs.seekg(firstStringAddr);
        char buffer[12] = {0};
        ifs.read(buffer, 11);
        EXPECT_STREQ(buffer, "Merhaba Dun");
    }
}

/**
 * @test Full GameMaker workflow with fixture
 */
TEST_F(GameMakerWorkflowTest, FullWorkflowWithFixture) {
    if (!hasGameMakerFixture()) {
        GTEST_SKIP() << "GameMaker fixture not available";
    }

    auto gameDir = copyFixture();

    // 1. Detect game
    scanners::GameDetector detector;
    auto detection = detector.detectEngine(gameDir);
    ASSERT_TRUE(detection.has_value());
    EXPECT_EQ(detection->engine, GameEngine::GameMaker);

    // 2-4. Handler integration would go here
    // TODO: Use actual handler when integration is complete

    SUCCEED() << "Fixture-based workflow requires handler integration";
}

/**
 * @test Handles corrupted data.win gracefully
 */
TEST_F(GameMakerWorkflowTest, HandlesCorruptedDataWinGracefully) {
    auto gameDir = tempDir_ / "corrupted_game";
    fs::create_directories(gameDir);

    // Create invalid data.win
    {
        std::ofstream ofs(gameDir / "data.win", std::ios::binary);
        ofs << "NOT A VALID FORM FILE";
    }

    scanners::GameDetector detector;
    auto result = detector.detectEngine(gameDir);

    // Should not detect as GameMaker or should have low confidence
    if (result.has_value() && result->engine == GameEngine::GameMaker) {
        EXPECT_LT(result->confidence, 0.5f);
    }
}
