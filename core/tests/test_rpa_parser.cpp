/**
 * @file test_rpa_parser.cpp
 * @brief Unit tests for Ren'Py RPA archive parser and pickle decoder
 * @copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <formats/renpy_rpa.hpp>

// Include internal headers for direct pickle testing
#include "../src/formats/pickle_reader.hpp"

#include <zlib.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using namespace makine;
using namespace makine::formats;

// ============================================================================
// PICKLE DECODER TESTS
// ============================================================================

class PickleDecoderTest : public ::testing::Test {
protected:
    // Helper to build a pickle byte stream
    static std::vector<uint8_t> makePickle(std::initializer_list<uint8_t> bytes) {
        return std::vector<uint8_t>(bytes);
    }
};

TEST_F(PickleDecoderTest, EmptyDict) {
    // protocol 2: \x80\x02 } .
    auto data = makePickle({0x80, 0x02, '}', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->isDict());
    EXPECT_TRUE(result->asDict().empty());
}

TEST_F(PickleDecoderTest, EmptyList) {
    auto data = makePickle({0x80, 0x02, ']', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    EXPECT_TRUE(result->asList().empty());
}

TEST_F(PickleDecoderTest, EmptyTuple) {
    auto data = makePickle({0x80, 0x02, ')', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList()); // tuples are represented as lists
    EXPECT_TRUE(result->asList().empty());
}

TEST_F(PickleDecoderTest, SingleInt) {
    // BININT1 42
    auto data = makePickle({0x80, 0x02, 'K', 42, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), 42);
}

TEST_F(PickleDecoderTest, BinInt) {
    // BININT 0x01020304 (little-endian)
    auto data = makePickle({0x80, 0x02, 'J', 0x04, 0x03, 0x02, 0x01, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), 0x01020304);
}

TEST_F(PickleDecoderTest, BinInt2) {
    // BININT2 0x1234 (little-endian)
    auto data = makePickle({0x80, 0x02, 'M', 0x34, 0x12, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), 0x1234);
}

TEST_F(PickleDecoderTest, NoneValue) {
    auto data = makePickle({0x80, 0x02, 'N', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isNone());
}

TEST_F(PickleDecoderTest, BoolValues) {
    auto dataTrue = makePickle({0x80, 0x02, 0x88, '.'});
    auto resultTrue = parsePickle(dataTrue);
    ASSERT_TRUE(resultTrue.has_value());
    EXPECT_TRUE(resultTrue->isBool());

    auto dataFalse = makePickle({0x80, 0x02, 0x89, '.'});
    auto resultFalse = parsePickle(dataFalse);
    ASSERT_TRUE(resultFalse.has_value());
    EXPECT_TRUE(resultFalse->isBool());
}

TEST_F(PickleDecoderTest, ShortBinUnicode) {
    // SHORT_BINUNICODE len=5 "hello"
    auto data = makePickle({0x80, 0x02, 0x8C, 5, 'h', 'e', 'l', 'l', 'o', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isString());
    EXPECT_EQ(result->asString(), "hello");
}

TEST_F(PickleDecoderTest, BinUnicode) {
    // BINUNICODE len=3 "abc"
    auto data = makePickle({0x80, 0x02, 'X', 3, 0, 0, 0, 'a', 'b', 'c', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isString());
    EXPECT_EQ(result->asString(), "abc");
}

TEST_F(PickleDecoderTest, ShortBinString) {
    // SHORT_BINSTRING len=3 "xyz"
    auto data = makePickle({0x80, 0x02, 'U', 3, 'x', 'y', 'z', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isString());
    EXPECT_EQ(result->asString(), "xyz");
}

TEST_F(PickleDecoderTest, ShortBinBytes) {
    // SHORT_BINBYTES len=2 [0xAB, 0xCD]
    auto data = makePickle({0x80, 0x02, 'C', 2, 0xAB, 0xCD, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isBytes());
    EXPECT_EQ(result->asBytes().size(), 2u);
    EXPECT_EQ(result->asBytes()[0], 0xAB);
    EXPECT_EQ(result->asBytes()[1], 0xCD);
}

TEST_F(PickleDecoderTest, Tuple1) {
    // BININT1 10, TUPLE1
    auto data = makePickle({0x80, 0x02, 'K', 10, 0x85, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 1u);
    EXPECT_EQ(result->asList()[0].asInt(), 10);
}

TEST_F(PickleDecoderTest, Tuple2) {
    // BININT1 1, BININT1 2, TUPLE2
    auto data = makePickle({0x80, 0x02, 'K', 1, 'K', 2, 0x86, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 2u);
    EXPECT_EQ(result->asList()[0].asInt(), 1);
    EXPECT_EQ(result->asList()[1].asInt(), 2);
}

TEST_F(PickleDecoderTest, Tuple3) {
    // BININT1 1, BININT1 2, BININT1 3, TUPLE3
    auto data = makePickle({0x80, 0x02, 'K', 1, 'K', 2, 'K', 3, 0x87, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 3u);
    EXPECT_EQ(result->asList()[0].asInt(), 1);
    EXPECT_EQ(result->asList()[1].asInt(), 2);
    EXPECT_EQ(result->asList()[2].asInt(), 3);
}

TEST_F(PickleDecoderTest, DictWithSetItem) {
    // EMPTY_DICT, SHORT_BINUNICODE "key", SHORT_BINUNICODE "val", SETITEM, STOP
    auto data = makePickle({
        0x80, 0x02,
        '}',                                    // EMPTY_DICT
        0x8C, 3, 'k', 'e', 'y',                // SHORT_BINUNICODE "key"
        0x8C, 3, 'v', 'a', 'l',                // SHORT_BINUNICODE "val"
        's',                                    // SETITEM
        '.'                                     // STOP
    });
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->isDict());
    ASSERT_EQ(result->asDict().size(), 1u);
    EXPECT_EQ(result->asDict()[0].first.asString(), "key");
    EXPECT_EQ(result->asDict()[0].second.asString(), "val");
}

TEST_F(PickleDecoderTest, DictWithSetItems) {
    // EMPTY_DICT, MARK, key1, val1, key2, val2, SETITEMS, STOP
    auto data = makePickle({
        0x80, 0x02,
        '}',                                    // EMPTY_DICT
        '(',                                    // MARK
        0x8C, 1, 'a',                           // "a"
        'K', 1,                                 // 1
        0x8C, 1, 'b',                           // "b"
        'K', 2,                                 // 2
        'u',                                    // SETITEMS
        '.'                                     // STOP
    });
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->isDict());
    ASSERT_EQ(result->asDict().size(), 2u);
}

TEST_F(PickleDecoderTest, ListWithAppend) {
    // EMPTY_LIST, BININT1 42, APPEND, STOP
    auto data = makePickle({
        0x80, 0x02,
        ']',                                    // EMPTY_LIST
        'K', 42,                                // 42
        'a',                                    // APPEND
        '.'                                     // STOP
    });
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 1u);
    EXPECT_EQ(result->asList()[0].asInt(), 42);
}

TEST_F(PickleDecoderTest, ListWithAppends) {
    // EMPTY_LIST, MARK, BININT1 10, BININT1 20, APPENDS, STOP
    auto data = makePickle({
        0x80, 0x02,
        ']',                                    // EMPTY_LIST
        '(',                                    // MARK
        'K', 10,                                // 10
        'K', 20,                                // 20
        'e',                                    // APPENDS
        '.'                                     // STOP
    });
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 2u);
    EXPECT_EQ(result->asList()[0].asInt(), 10);
    EXPECT_EQ(result->asList()[1].asInt(), 20);
}

TEST_F(PickleDecoderTest, MemoBindGet) {
    // Store "hello" in memo[0], then retrieve it with BINGET
    auto data = makePickle({
        0x80, 0x02,
        0x8C, 5, 'h', 'e', 'l', 'l', 'o',     // SHORT_BINUNICODE "hello"
        'q', 0,                                 // BINPUT 0
        '.'                                     // STOP
    });
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isString());
    EXPECT_EQ(result->asString(), "hello");
}

TEST_F(PickleDecoderTest, DictWithTupleValues_RpaStyle) {
    // Simulates RPA index: {b"script.rpy": [(100, 500, b"")]}
    // EMPTY_DICT
    //   SHORT_BINBYTES "script.rpy"
    //   EMPTY_LIST
    //     MARK
    //       BININT1 100, BININT2 500, SHORT_BINBYTES "", TUPLE3
    //     APPENDS
    //   SETITEM
    // STOP
    auto data = makePickle({
        0x80, 0x02,
        '}',                                        // EMPTY_DICT
        'C', 10, 's','c','r','i','p','t','.','r','p','y',  // SHORT_BINBYTES "script.rpy"
        ']',                                        // EMPTY_LIST
        '(',                                        // MARK
        'K', 100,                                   // BININT1 100 (offset)
        'M', 0xF4, 0x01,                            // BININT2 500 (length)
        'C', 0,                                     // SHORT_BINBYTES b"" (empty prefix)
        0x87,                                       // TUPLE3
        'e',                                        // APPENDS
        's',                                        // SETITEM
        '.'                                         // STOP
    });

    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(result->isDict());
    ASSERT_EQ(result->asDict().size(), 1u);

    const auto& key = result->asDict()[0].first;
    const auto& val = result->asDict()[0].second;

    EXPECT_TRUE(key.isBytes());
    std::string keyStr(key.asBytes().begin(), key.asBytes().end());
    EXPECT_EQ(keyStr, "script.rpy");

    EXPECT_TRUE(val.isList());
    ASSERT_EQ(val.asList().size(), 1u);

    const auto& tuple = val.asList()[0];
    EXPECT_TRUE(tuple.isList());
    ASSERT_EQ(tuple.asList().size(), 3u);
    EXPECT_EQ(tuple.asList()[0].asInt(), 100);
    EXPECT_EQ(tuple.asList()[1].asInt(), 500);
    EXPECT_TRUE(tuple.asList()[2].isBytes());
}

TEST_F(PickleDecoderTest, InvalidData) {
    auto data = makePickle({});
    auto result = parsePickle(data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PickleDecoderTest, UnsupportedOpcode) {
    // Use an opcode that's not in our decoder (e.g., 0xFF)
    auto data = makePickle({0x80, 0x02, 0xFF});
    auto result = parsePickle(data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PickleDecoderTest, Long1) {
    // LONG1 with 2 bytes: 0x00 0x01 = 256
    auto data = makePickle({0x80, 0x02, 0x8A, 2, 0x00, 0x01, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), 256);
}

TEST_F(PickleDecoderTest, Long1_Negative) {
    // LONG1 with 1 byte: 0xFF = -1
    auto data = makePickle({0x80, 0x02, 0x8A, 1, 0xFF, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), -1);
}

TEST_F(PickleDecoderTest, Long1_Zero) {
    // LONG1 with 0 bytes = 0
    auto data = makePickle({0x80, 0x02, 0x8A, 0, '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isInt());
    EXPECT_EQ(result->asInt(), 0);
}

TEST_F(PickleDecoderTest, TupleFromMark) {
    // MARK, BININT1 1, BININT1 2, TUPLE
    auto data = makePickle({0x80, 0x02, '(', 'K', 1, 'K', 2, 't', '.'});
    auto result = parsePickle(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->isList());
    ASSERT_EQ(result->asList().size(), 2u);
}

// ============================================================================
// RPA HEADER TESTS
// ============================================================================

class RpaHeaderTest : public ::testing::Test {
protected:
    fs::path tempDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() / "makine_rpa_test";
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    // Write content to a temp file
    fs::path writeFile(const std::string& name, const std::string& content) {
        auto path = tempDir / name;
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        return path;
    }

    fs::path writeFile(const std::string& name, const std::vector<uint8_t>& content) {
        auto path = tempDir / name;
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(content.data()),
                  static_cast<std::streamsize>(content.size()));
        return path;
    }
};

TEST_F(RpaHeaderTest, InvalidFile) {
    auto path = writeFile("bad.rpa", "This is not an RPA file\n");
    auto result = parseRpaArchive(path);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidFormat);
}

TEST_F(RpaHeaderTest, NonexistentFile) {
    auto result = parseRpaArchive(tempDir / "nonexistent.rpa");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::FileNotFound);
}

// ============================================================================
// SYNTHETIC RPA ARCHIVE TESTS
// ============================================================================

class RpaArchiveTest : public ::testing::Test {
protected:
    fs::path tempDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() / "makine_rpa_archive_test";
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    /**
     * Build a synthetic RPA-3.0 archive with one file entry.
     *
     * Layout:
     *   [header line]\n
     *   [file data]
     *   [compressed pickle index]
     */
    fs::path buildSyntheticRpa(
        const std::string& entryPath,
        const std::string& entryContent,
        uint64_t xorKey = 0
    ) {
        // Build the pickle index manually
        // Dict: {b"<entryPath>": [(offset ^ xorKey, length ^ xorKey, b"")]}
        std::vector<uint8_t> pickle;

        // Protocol header
        pickle.push_back(0x80); // PROTO
        pickle.push_back(0x02); // version 2

        // Empty dict
        pickle.push_back('}');  // EMPTY_DICT

        // Key: bytes of entryPath
        pickle.push_back('C');  // SHORT_BINBYTES
        pickle.push_back(static_cast<uint8_t>(entryPath.size()));
        pickle.insert(pickle.end(), entryPath.begin(), entryPath.end());

        // Value: list with one tuple
        pickle.push_back(']');  // EMPTY_LIST
        pickle.push_back('('); // MARK

        // The actual data will start right after the header line
        // Header line: "RPA-3.0 XXXXXXXXXXXXXXXX XXXXXXXXXXXXXXXX\n"
        // We'll calculate the exact offset later. Use placeholder.

        // For now, compute offset/length with XOR
        // Header line will be fixed-length: "RPA-3.0 " + 16-char hex offset + " " + 16-char hex key + "\n"
        // = 8 + 16 + 1 + 16 + 1 = 42 bytes
        uint64_t dataOffset = 42;
        uint64_t dataLength = entryContent.size();

        uint64_t encodedOffset = dataOffset ^ xorKey;
        uint64_t encodedLength = dataLength ^ xorKey;

        // BININT for offset (4 bytes, little-endian)
        pickle.push_back('J');
        pickle.push_back(static_cast<uint8_t>(encodedOffset & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedOffset >> 8) & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedOffset >> 16) & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedOffset >> 24) & 0xFF));

        // BININT for length (4 bytes)
        pickle.push_back('J');
        pickle.push_back(static_cast<uint8_t>(encodedLength & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedLength >> 8) & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedLength >> 16) & 0xFF));
        pickle.push_back(static_cast<uint8_t>((encodedLength >> 24) & 0xFF));

        // Empty prefix
        pickle.push_back('C');  // SHORT_BINBYTES
        pickle.push_back(0);    // length 0

        pickle.push_back(0x87); // TUPLE3
        pickle.push_back('e');  // APPENDS
        pickle.push_back('s');  // SETITEM
        pickle.push_back('.');  // STOP

        // Compress the pickle with zlib
        std::vector<uint8_t> compressed(compressBound(static_cast<uLong>(pickle.size())));
        uLongf compressedLen = static_cast<uLongf>(compressed.size());
        int zret = compress(compressed.data(), &compressedLen, pickle.data(),
                           static_cast<uLong>(pickle.size()));
        EXPECT_EQ(zret, Z_OK);
        compressed.resize(compressedLen);

        // Index offset = header (42) + data
        uint64_t indexOffset = dataOffset + dataLength;

        // Build header line (fixed 42 bytes including \n)
        char headerBuf[64];
        int headerLen = snprintf(headerBuf, sizeof(headerBuf),
            "RPA-3.0 %016llx %016llx\n",
            static_cast<unsigned long long>(indexOffset),
            static_cast<unsigned long long>(xorKey));

        // Assemble the full file
        std::vector<uint8_t> fileData;

        // Header line (exact headerLen bytes)
        fileData.insert(fileData.end(), headerBuf, headerBuf + headerLen);

        // Pad or trim to exactly 42 bytes if needed
        while (fileData.size() < dataOffset) fileData.push_back(' ');
        fileData.resize(dataOffset); // Truncate if needed (replace last with \n)
        fileData.back() = '\n';

        // File data
        fileData.insert(fileData.end(), entryContent.begin(), entryContent.end());

        // Compressed index
        fileData.insert(fileData.end(), compressed.begin(), compressed.end());

        auto path = tempDir / "test.rpa";
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(fileData.data()),
                  static_cast<std::streamsize>(fileData.size()));
        return path;
    }
};

TEST_F(RpaArchiveTest, ParseSyntheticArchive_NoXor) {
    std::string content = "label start:\n    \"Hello, world!\"\n";
    auto rpaPath = buildSyntheticRpa("game/script.rpy", content, 0);

    auto result = parseRpaArchive(rpaPath);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_EQ(result->header.version, RpaVersion::V3);
    EXPECT_EQ(result->header.xorKey, 0u);
    ASSERT_EQ(result->entries.size(), 1u);
    EXPECT_EQ(result->entries[0].path, "game/script.rpy");
    EXPECT_EQ(result->entries[0].dataLength, content.size());
}

TEST_F(RpaArchiveTest, ParseSyntheticArchive_WithXor) {
    std::string content = "define e = Character(\"Eileen\")\n";
    uint64_t xorKey = 0xDEADBEEF;
    auto rpaPath = buildSyntheticRpa("game/chars.rpy", content, xorKey);

    auto result = parseRpaArchive(rpaPath);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_EQ(result->header.version, RpaVersion::V3);
    EXPECT_EQ(result->header.xorKey, xorKey);
    ASSERT_EQ(result->entries.size(), 1u);
    EXPECT_EQ(result->entries[0].path, "game/chars.rpy");
    // After XOR decode, length should match
    EXPECT_EQ(result->entries[0].dataLength, content.size());
}

TEST_F(RpaArchiveTest, ExtractEntry) {
    std::string content = "\"This is test content.\"";
    auto rpaPath = buildSyntheticRpa("test.rpy", content, 0);

    auto archResult = parseRpaArchive(rpaPath);
    ASSERT_TRUE(archResult.has_value()) << archResult.error().message();
    ASSERT_EQ(archResult->entries.size(), 1u);

    auto dataResult = extractRpaEntry(rpaPath, archResult->entries[0]);
    ASSERT_TRUE(dataResult.has_value()) << dataResult.error().message();

    std::string extracted(dataResult->begin(), dataResult->end());
    EXPECT_EQ(extracted, content);
}

TEST_F(RpaArchiveTest, FindByExtension) {
    std::string content = "label start:\n    \"Hello\"\n";
    auto rpaPath = buildSyntheticRpa("game/script.rpy", content, 0);

    auto archResult = parseRpaArchive(rpaPath);
    ASSERT_TRUE(archResult.has_value());

    auto rpyEntries = archResult->findByExtension("rpy");
    EXPECT_EQ(rpyEntries.size(), 1u);

    auto pngEntries = archResult->findByExtension("png");
    EXPECT_TRUE(pngEntries.empty());
}

TEST_F(RpaArchiveTest, FindEntry) {
    std::string content = "\"test\"";
    auto rpaPath = buildSyntheticRpa("game/script.rpy", content, 0);

    auto archResult = parseRpaArchive(rpaPath);
    ASSERT_TRUE(archResult.has_value());

    EXPECT_NE(archResult->findEntry("game/script.rpy"), nullptr);
    EXPECT_EQ(archResult->findEntry("nonexistent.rpy"), nullptr);
}
