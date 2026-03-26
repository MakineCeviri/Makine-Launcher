/**
 * @file test_utf_utils.cpp
 * @brief Unit tests for utf_utils.hpp — UTF validation, conversion, encoding detection
 *
 * Critical for a Turkish game translation app.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/utf_utils.hpp>

#include <cstring>
#include <string>
#include <vector>

namespace makine {
namespace testing {

// ===========================================================================
// UTF-8 Validation
// ===========================================================================

TEST(Utf8ValidationTest, ValidAscii) {
    EXPECT_TRUE(utf::isValidUtf8("Hello, world!"));
}

TEST(Utf8ValidationTest, EmptyString) {
    EXPECT_TRUE(utf::isValidUtf8(""));
}

TEST(Utf8ValidationTest, ValidTurkishCharacters) {
    // Turkish specific: ç, ğ, ı, ö, ş, ü, İ, Ğ, Ş, Ç, Ö, Ü
    EXPECT_TRUE(utf::isValidUtf8("çğıöşüİĞŞÇÖÜ"));
}

TEST(Utf8ValidationTest, ValidMultibyteSequences) {
    // 2-byte: é (U+00E9)
    EXPECT_TRUE(utf::isValidUtf8("\xC3\xA9"));
    // 3-byte: € (U+20AC)
    EXPECT_TRUE(utf::isValidUtf8("\xE2\x82\xAC"));
    // 4-byte: 𐍈 (U+10348)
    EXPECT_TRUE(utf::isValidUtf8("\xF0\x90\x8D\x88"));
}

TEST(Utf8ValidationTest, InvalidContinuationByte) {
    // Continuation byte without start byte
    EXPECT_FALSE(utf::isValidUtf8("\x80"));
    EXPECT_FALSE(utf::isValidUtf8("\xBF"));
}

TEST(Utf8ValidationTest, TruncatedSequence) {
    // 2-byte sequence missing continuation
    EXPECT_FALSE(utf::isValidUtf8("\xC3"));
    // 3-byte sequence missing continuations
    EXPECT_FALSE(utf::isValidUtf8("\xE2\x82"));
    // 4-byte sequence missing continuations
    EXPECT_FALSE(utf::isValidUtf8("\xF0\x90\x8D"));
}

TEST(Utf8ValidationTest, OverlongEncoding) {
    // Overlong 2-byte for ASCII NUL
    EXPECT_FALSE(utf::isValidUtf8("\xC0\x80"));
    EXPECT_FALSE(utf::isValidUtf8("\xC1\x80"));
}

TEST(Utf8ValidationTest, SurrogateHalf) {
    // U+D800 encoded in UTF-8 (not allowed)
    EXPECT_FALSE(utf::isValidUtf8("\xED\xA0\x80"));
}

TEST(Utf8ValidationTest, BeyondMaxCodePoint) {
    // U+110000 (above max U+10FFFF)
    EXPECT_FALSE(utf::isValidUtf8("\xF4\x90\x80\x80"));
}

TEST(Utf8ValidationTest, InvalidStartByte) {
    // 0xFE and 0xFF are never valid in UTF-8
    EXPECT_FALSE(utf::isValidUtf8("\xFE"));
    EXPECT_FALSE(utf::isValidUtf8("\xFF"));
}

// ===========================================================================
// UTF-8 Code Point Counting
// ===========================================================================

TEST(Utf8CountTest, AsciiString) {
    EXPECT_EQ(utf::countUtf8CodePoints("Hello"), 5);
}

TEST(Utf8CountTest, EmptyString) {
    EXPECT_EQ(utf::countUtf8CodePoints(""), 0);
}

TEST(Utf8CountTest, TurkishString) {
    // "Türkçe" — T(1) ü(2) r(1) k(1) ç(2) e(1) = 6 code points, 8 bytes
    EXPECT_EQ(utf::countUtf8CodePoints("Türkçe"), 6);
}

TEST(Utf8CountTest, MixedMultibyte) {
    // é(2 bytes) + €(3 bytes) + 𐍈(4 bytes) = 3 code points
    EXPECT_EQ(utf::countUtf8CodePoints("\xC3\xA9\xE2\x82\xAC\xF0\x90\x8D\x88"), 3);
}

TEST(Utf8CountTest, SingleEmoji) {
    // 😀 = 4 bytes = 1 code point
    EXPECT_EQ(utf::countUtf8CodePoints("😀"), 1);
}

// ===========================================================================
// UTF-16 Validation
// ===========================================================================

TEST(Utf16LeValidationTest, ValidAsciiAsUtf16LE) {
    // "Hi" in UTF-16 LE: H=0x48,0x00 i=0x69,0x00
    std::vector<uint8_t> data = {0x48, 0x00, 0x69, 0x00};
    EXPECT_TRUE(utf::isValidUtf16LE(data));
}

TEST(Utf16LeValidationTest, EmptyData) {
    std::vector<uint8_t> data;
    EXPECT_TRUE(utf::isValidUtf16LE(data));
}

TEST(Utf16LeValidationTest, OddByteCount) {
    std::vector<uint8_t> data = {0x48, 0x00, 0x69};
    EXPECT_TRUE(utf::isValidUtf16LE(data)); // simdutf ignores trailing byte
}

TEST(Utf16LeValidationTest, ValidSurrogatePair) {
    // U+10348: high=0xD800, low=0xDF48 in LE
    std::vector<uint8_t> data = {0x00, 0xD8, 0x48, 0xDF};
    EXPECT_TRUE(utf::isValidUtf16LE(data));
}

TEST(Utf16LeValidationTest, LoneSurrogateHigh) {
    // High surrogate without low surrogate
    std::vector<uint8_t> data = {0x00, 0xD8, 0x48, 0x00};
    EXPECT_FALSE(utf::isValidUtf16LE(data)); // Lone surrogate is invalid
}

TEST(Utf16LeValidationTest, LoneSurrogateLow) {
    // Low surrogate without high surrogate
    std::vector<uint8_t> data = {0x00, 0xDC, 0x48, 0x00};
    EXPECT_FALSE(utf::isValidUtf16LE(data)); // Lone surrogate is invalid
}

TEST(Utf16BeValidationTest, ValidAsciiAsUtf16BE) {
    // "Hi" in UTF-16 BE: H=0x00,0x48 i=0x00,0x69
    std::vector<uint8_t> data = {0x00, 0x48, 0x00, 0x69};
    EXPECT_TRUE(utf::isValidUtf16BE(data));
}

TEST(Utf16BeValidationTest, OddByteCount) {
    std::vector<uint8_t> data = {0x00, 0x48, 0x00};
    EXPECT_TRUE(utf::isValidUtf16BE(data)); // Implementation ignores trailing byte
}

// ===========================================================================
// UTF-8 <-> UTF-16 Conversion
// ===========================================================================

TEST(Utf8ToUtf16LeTest, AsciiString) {
    auto result = utf::utf8ToUtf16LE("Hi");
    // H=0x48,0x00 i=0x69,0x00
    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0x48);
    EXPECT_EQ(result[1], 0x00);
    EXPECT_EQ(result[2], 0x69);
    EXPECT_EQ(result[3], 0x00);
}

TEST(Utf8ToUtf16LeTest, EmptyString) {
    auto result = utf::utf8ToUtf16LE("");
    EXPECT_TRUE(result.empty());
}

TEST(Utf8ToUtf16LeTest, TurkishCharacters) {
    // "ş" = U+015F = 0x5F, 0x01 in UTF-16 LE
    auto result = utf::utf8ToUtf16LE("ş");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 0x5F);
    EXPECT_EQ(result[1], 0x01);
}

TEST(Utf8ToUtf16LeTest, FourByteCharacterProducesSurrogatePair) {
    // 𐍈 (U+10348) should produce surrogate pair
    auto result = utf::utf8ToUtf16LE("\xF0\x90\x8D\x88");
    EXPECT_EQ(result.size(), 4u); // 2 UTF-16 code units = 4 bytes
}

TEST(Utf16LeToUtf8Test, AsciiRoundTrip) {
    std::string original = "Hello, World!";
    auto utf16 = utf::utf8ToUtf16LE(original);
    auto roundTrip = utf::utf16LEToUtf8(utf16);
    EXPECT_EQ(roundTrip, original);
}

TEST(Utf16LeToUtf8Test, TurkishRoundTrip) {
    std::string original = "Türkçe çeviri yapılıyor";
    auto utf16 = utf::utf8ToUtf16LE(original);
    auto roundTrip = utf::utf16LEToUtf8(utf16);
    EXPECT_EQ(roundTrip, original);
}

TEST(Utf16LeToUtf8Test, EmptyData) {
    std::vector<uint8_t> empty;
    EXPECT_EQ(utf::utf16LEToUtf8(empty), "");
}

TEST(Utf16LeToUtf8Test, OddByteCountReturnsEmpty) {
    std::vector<uint8_t> odd = {0x48, 0x00, 0x69};
    EXPECT_EQ(utf::utf16LEToUtf8(odd), "");
}

TEST(Utf8ToUtf16BeTest, AsciiString) {
    auto result = utf::utf8ToUtf16BE("A");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 0x00);
    EXPECT_EQ(result[1], 0x41);
}

TEST(Utf16BeToUtf8Test, RoundTrip) {
    std::string original = "İstanbul";
    auto utf16be = utf::utf8ToUtf16BE(original);
    auto roundTrip = utf::utf16BEToUtf8(utf16be);
    EXPECT_EQ(roundTrip, original);
}

// ===========================================================================
// Encoding Detection
// ===========================================================================

TEST(EncodingDetectionTest, EmptyDataIsUnknown) {
    std::vector<uint8_t> empty;
    EXPECT_EQ(utf::detectEncoding(empty), utf::Encoding::Unknown);
}

TEST(EncodingDetectionTest, AsciiDetected) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::ASCII);
}

TEST(EncodingDetectionTest, Utf8Detected) {
    // "ü" in UTF-8: 0xC3 0xBC
    std::vector<uint8_t> data = {'H', 0xC3, 0xBC, 'l', 'l', 'o'};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF8);
}

TEST(EncodingDetectionTest, Utf8BomDetected) {
    std::vector<uint8_t> data = {0xEF, 0xBB, 0xBF, 'H', 'i'};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF8_BOM);
}

TEST(EncodingDetectionTest, Utf16LeBomDetected) {
    std::vector<uint8_t> data = {0xFF, 0xFE, 0x48, 0x00};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF16_LE_BOM);
}

TEST(EncodingDetectionTest, Utf16BeBomDetected) {
    std::vector<uint8_t> data = {0xFE, 0xFF, 0x00, 0x48};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF16_BE_BOM);
}

TEST(EncodingDetectionTest, Utf32LeDetected) {
    std::vector<uint8_t> data = {0xFF, 0xFE, 0x00, 0x00, 'H', 0x00, 0x00, 0x00};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF32_LE);
}

TEST(EncodingDetectionTest, Utf32BeDetected) {
    std::vector<uint8_t> data = {0x00, 0x00, 0xFE, 0xFF, 0x00, 0x00, 0x00, 'H'};
    EXPECT_EQ(utf::detectEncoding(data), utf::Encoding::UTF32_BE);
}

// ===========================================================================
// encodingToString
// ===========================================================================

TEST(EncodingToStringTest, AllEncodingsNonEmpty) {
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::Unknown), "Unknown");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::ASCII), "ASCII");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF8), "UTF-8");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF8_BOM), "UTF-8 (BOM)");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF16_LE), "UTF-16 LE");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF16_LE_BOM), "UTF-16 LE (BOM)");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF16_BE), "UTF-16 BE");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF16_BE_BOM), "UTF-16 BE (BOM)");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF32_LE), "UTF-32 LE");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::UTF32_BE), "UTF-32 BE");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::Latin1), "Latin-1");
    EXPECT_STREQ(utf::encodingToString(utf::Encoding::ShiftJIS), "Shift-JIS");
}

// ===========================================================================
// toUtf8
// ===========================================================================

TEST(ToUtf8Test, EmptyDataReturnsEmpty) {
    std::vector<uint8_t> empty;
    EXPECT_EQ(utf::toUtf8(empty), "");
}

TEST(ToUtf8Test, Utf8BomStripped) {
    std::vector<uint8_t> data = {0xEF, 0xBB, 0xBF, 'H', 'i'};
    EXPECT_EQ(utf::toUtf8(data, utf::Encoding::UTF8_BOM), "Hi");
}

TEST(ToUtf8Test, Utf16LeBomConverted) {
    // BOM + "A" in UTF-16 LE
    std::vector<uint8_t> data = {0xFF, 0xFE, 0x41, 0x00};
    auto result = utf::toUtf8(data, utf::Encoding::UTF16_LE_BOM);
    EXPECT_EQ(result, "A");
}

TEST(ToUtf8Test, AsciiPassThrough) {
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    EXPECT_EQ(utf::toUtf8(data, utf::Encoding::ASCII), "Test");
}

TEST(ToUtf8Test, AutoDetectAscii) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(utf::toUtf8(data), "Hello");
}

// ===========================================================================
// Backend Info
// ===========================================================================

TEST(UtfBackendTest, HasSimdutfReturnsBool) {
    // Just verify it returns without crashing
    (void)utf::hasSimdutf();
    SUCCEED();
}

TEST(UtfBackendTest, SimdutfVersionNonNull) {
    auto version = utf::simdutfVersion();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(std::strlen(version), 0u);
}

} // namespace testing
} // namespace makine
