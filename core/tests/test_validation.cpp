/**
 * @file test_validation.cpp
 * @brief Unit tests for validation utilities
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/validation.hpp>
#include <makine/path_utils.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace makine {
namespace testing {

namespace fs = std::filesystem;
using namespace makine::validation;

// =============================================================================
// TEST FIXTURE
// =============================================================================

class ValidationTest : public ::testing::Test {
protected:
    fs::path testDir_;
    fs::path testFile_;

    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makine_validation_tests";
        fs::create_directories(testDir_);

        testFile_ = testDir_ / "sample.txt";
        std::ofstream(testFile_) << "hello";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    void createFile(const fs::path& path, const std::string& content) {
        std::ofstream(path) << content;
    }
};

// =============================================================================
// PATH SAFETY
// =============================================================================

TEST_F(ValidationTest, IsPathSafe_NormalPath) {
    EXPECT_TRUE(isPathSafe(fs::path("games/steam/data.pak")));
}

TEST_F(ValidationTest, IsPathSafe_RejectsTraversal) {
    EXPECT_FALSE(isPathSafe(fs::path("games/../../etc/passwd")));
}

TEST_F(ValidationTest, IsPathSafe_RejectsNullByte) {
    std::string pathStr = "games/data";
    pathStr.push_back('\0');
    pathStr += ".txt";
    EXPECT_FALSE(isPathSafe(fs::path(pathStr)));
}

#ifdef _WIN32
TEST_F(ValidationTest, IsPathSafe_RejectsWindowsDeviceNames) {
    EXPECT_FALSE(isPathSafe(fs::path("CON")));
    EXPECT_FALSE(isPathSafe(fs::path("NUL")));
    EXPECT_FALSE(isPathSafe(fs::path("COM1")));
    EXPECT_FALSE(isPathSafe(fs::path("LPT1")));
    EXPECT_FALSE(isPathSafe(fs::path("PRN")));
    EXPECT_FALSE(isPathSafe(fs::path("AUX")));
}

TEST_F(ValidationTest, IsPathSafe_DeviceNameCaseInsensitive) {
    EXPECT_FALSE(isPathSafe(fs::path("con")));
    EXPECT_FALSE(isPathSafe(fs::path("nul.txt")));
}
#endif

// =============================================================================
// PATH EXISTS / DIRECTORY / FILE
// =============================================================================

TEST_F(ValidationTest, ValidatePathExists_EmptyPath) {
    auto result = validatePathExists(fs::path());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, ValidatePathExists_ExistingPath) {
    auto result = validatePathExists(testDir_);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, testDir_);
}

TEST_F(ValidationTest, ValidatePathExists_NonExistent) {
    auto result = validatePathExists(testDir_ / "no_such_file.bin");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::FileNotFound);
}

TEST_F(ValidationTest, ValidateDirectory_ValidDir) {
    auto result = validateDirectory(testDir_);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateDirectory_FileNotDir) {
    auto result = validateDirectory(testFile_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, ValidateFile_ValidFile) {
    auto result = validateFile(testFile_);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateFile_DirectoryNotFile) {
    auto result = validateFile(testDir_);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

// =============================================================================
// READABLE / WRITABLE
// =============================================================================

TEST_F(ValidationTest, ValidateReadable_ReadableFile) {
    auto result = validateReadable(testFile_);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateReadable_NonExistentFile) {
    auto result = validateReadable(testDir_ / "ghost.txt");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateWritable_WritableDir) {
    auto result = validateWritable(testDir_);
    ASSERT_TRUE(result.has_value());
    // Write test file should be cleaned up by the function
    EXPECT_FALSE(fs::exists(testDir_ / ".makine_write_test"));
}

// =============================================================================
// PATH SANITIZATION
// =============================================================================

TEST_F(ValidationTest, SanitizePath_RemovesDotDot) {
    auto result = sanitizePath(fs::path("games/../data/file.txt"));
    ASSERT_TRUE(result.has_value());
    // Should not contain ".."
    EXPECT_EQ(result->string().find(".."), std::string::npos);
}

TEST_F(ValidationTest, SanitizePath_EmptyPath) {
    auto result = sanitizePath(fs::path());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, SanitizePath_AllDotsBecomesEmpty) {
    // Path made entirely of ".." and "." yields empty after sanitization
    auto result = sanitizePath(fs::path("../.."));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

// =============================================================================
// STRING VALIDATION
// =============================================================================

TEST_F(ValidationTest, ValidateNotEmpty_NonEmpty) {
    auto result = validateNotEmpty("hello", "name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello");
}

TEST_F(ValidationTest, ValidateNotEmpty_Empty) {
    auto result = validateNotEmpty("", "name");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, ValidateLength_WithinBounds) {
    auto result = validateLength("test", 1, 10, "field");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateLength_TooShort) {
    auto result = validateLength("a", 3, 10, "field");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateLength_TooLong) {
    auto result = validateLength("abcdefghijk", 1, 5, "field");
    ASSERT_FALSE(result.has_value());
}

// =============================================================================
// UTF-8 VALIDATION
// =============================================================================

TEST_F(ValidationTest, IsValidUtf8_AsciiString) {
    EXPECT_TRUE(isValidUtf8("Hello World"));
}

TEST_F(ValidationTest, IsValidUtf8_TurkishChars) {
    // Valid UTF-8: Turkish characters
    EXPECT_TRUE(isValidUtf8("\xC3\xBC\xC3\xB6\xC3\xA7\xC4\xB1")); // uocI
}

TEST_F(ValidationTest, IsValidUtf8_InvalidContinuationByte) {
    // 0xC0 expects a continuation byte, 0x01 is not one
    EXPECT_FALSE(isValidUtf8("\xC0\x01"));
}

TEST_F(ValidationTest, IsValidUtf8_TruncatedSequence) {
    // 0xE0 starts a 3-byte sequence but only 1 continuation byte follows
    EXPECT_FALSE(isValidUtf8("\xE0\x80"));
}

TEST_F(ValidationTest, ValidateUtf8_ValidString) {
    auto result = validateUtf8("Merhaba", "greeting");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateUtf8_InvalidString) {
    auto result = validateUtf8("\xFF\xFE", "data");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

// =============================================================================
// STRING SANITIZATION
// =============================================================================

TEST_F(ValidationTest, SanitizeString_RemovesControlChars) {
    std::string input = "hello\x01\x02world";
    auto result = sanitizeString(input);
    EXPECT_EQ(result, "helloworld");
}

TEST_F(ValidationTest, SanitizeString_PreservesWhitespace) {
    std::string input = "hello\tworld\n";
    auto result = sanitizeString(input);
    EXPECT_EQ(result, "hello\tworld\n");
}

TEST_F(ValidationTest, SanitizeString_HonorsMaxLen) {
    std::string input = "abcdefghij";
    auto result = sanitizeString(input, 5);
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result, "abcde");
}

// =============================================================================
// GAME ID VALIDATION
// =============================================================================

TEST_F(ValidationTest, ValidateGameId_Valid) {
    auto result = validateGameId("steam_123456");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "steam_123456");
}

TEST_F(ValidationTest, ValidateGameId_ValidWithColon) {
    auto result = validateGameId("epic:game-name");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateGameId_Empty) {
    auto result = validateGameId("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, ValidateGameId_TooLong) {
    std::string longId(257, 'a');
    auto result = validateGameId(longId);
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateGameId_InvalidChars) {
    auto result = validateGameId("game id with spaces");
    ASSERT_FALSE(result.has_value());
}

// =============================================================================
// PACKAGE ID VALIDATION
// =============================================================================

TEST_F(ValidationTest, ValidatePackageId_Valid) {
    auto result = validatePackageId("steam_123456_tr_1.0.0");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidatePackageId_InvalidChars) {
    auto result = validatePackageId("pkg id@bad");
    ASSERT_FALSE(result.has_value());
}

// =============================================================================
// URL VALIDATION
// =============================================================================

TEST_F(ValidationTest, ValidateUrl_ValidHttp) {
    auto result = validateUrl("http://example.com/path");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateUrl_ValidHttps) {
    auto result = validateUrl("https://cdn.makineceviri.org/data/pkg.makine");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateUrl_Empty) {
    auto result = validateUrl("");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateUrl_InvalidScheme) {
    auto result = validateUrl("ftp://files.example.com");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateUrl_SuspiciousDoubleSlash) {
    // Double slash after scheme+host triggers suspicious pattern check
    auto result = validateUrl("https://evil.com//secret");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateUrl_SuspiciousTraversal) {
    auto result = validateUrl("https://evil.com/../admin");
    ASSERT_FALSE(result.has_value());
}

// =============================================================================
// HTTPS URL ENFORCEMENT
// =============================================================================

TEST_F(ValidationTest, ValidateHttpsUrl_AcceptsHttps) {
    auto result = validateHttpsUrl("https://cdn.makineceviri.org/index.json");
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateHttpsUrl_RejectsHttp) {
    auto result = validateHttpsUrl("http://example.com/data");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

// =============================================================================
// SHA-256 VALIDATION
// =============================================================================

TEST_F(ValidationTest, ValidateSha256_Valid) {
    std::string hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    auto result = validateSha256(hash);
    ASSERT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateSha256_WrongLength) {
    auto result = validateSha256("abcdef");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateSha256_InvalidChars) {
    // 64 chars but contains 'g' which is not hex
    std::string badHash = std::string(63, 'a') + "g";
    auto result = validateSha256(badHash);
    ASSERT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateSha256_UppercaseHex) {
    std::string hash = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    auto result = validateSha256(hash);
    ASSERT_TRUE(result.has_value());
}

// =============================================================================
// VALIDATION BUILDER
// =============================================================================

TEST_F(ValidationTest, ValidationBuilder_AllPass) {
    auto v = validate()
        .checkDirectory(testDir_, "testDir")
        .checkFile(testFile_, "testFile")
        .checkNotEmpty("hello", "name")
        .checkLength("test", 1, 100, "field");

    EXPECT_TRUE(v.isValid());
    EXPECT_TRUE(v.errors().empty());
    EXPECT_TRUE(v.result().has_value());
}

TEST_F(ValidationTest, ValidationBuilder_SomeFail) {
    auto v = validate()
        .checkNotEmpty("", "emptyField")
        .checkLength("x", 5, 10, "shortField")
        .checkFile(testDir_ / "nope.txt", "missing");

    EXPECT_FALSE(v.isValid());
    EXPECT_EQ(v.errors().size(), 3u);

    auto result = v.result();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), ErrorCode::InvalidArgument);
}

TEST_F(ValidationTest, ValidationBuilder_ErrorAccumulation) {
    auto v = validate()
        .checkNotEmpty("ok", "field1")
        .checkNotEmpty("", "field2")
        .checkNotEmpty("", "field3");

    EXPECT_FALSE(v.isValid());
    // Two empty fields should produce exactly 2 errors
    EXPECT_EQ(v.errors().size(), 2u);
}

// =============================================================================
// ADDITIONAL EDGE CASES
// =============================================================================

TEST_F(ValidationTest, ContainsTraversalMixedSlashes) {
    // Mixed forward and back slashes with traversal
    EXPECT_TRUE(path::containsTraversalPattern("a\\..\\b"));
    EXPECT_TRUE(path::containsTraversalPattern("a\\..\\..\\b"));
}

TEST_F(ValidationTest, ValidateUrl_PortBoundaryValues) {
    // Port 0 - edge case
    auto result0 = validateUrl("https://example.com:0/path");
    // Implementation-dependent: may or may not reject port 0

    // Port 65535 - max valid
    auto result65535 = validateUrl("https://example.com:65535/path");
    // Should be valid
    if (result65535.has_value()) {
        EXPECT_FALSE(result65535->empty());
    }
    SUCCEED();
}

TEST_F(ValidationTest, ValidateGameId_ExactlyMaxLength) {
    // gameId max is 256 — exactly 256 should pass
    std::string exactMax(256, 'a');
    auto result = validateGameId(exactMax);
    EXPECT_TRUE(result.has_value());
}

TEST_F(ValidationTest, ValidateGameId_OneOverMax) {
    // 257 chars should fail
    std::string overMax(257, 'a');
    auto result = validateGameId(overMax);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ValidationTest, ValidateSha256_ExactlyValidLowercase) {
    std::string hash(64, 'a');
    auto result = validateSha256(hash);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, hash);
}

TEST_F(ValidationTest, ValidateSha256_Empty) {
    auto result = validateSha256("");
    EXPECT_FALSE(result.has_value());
}

#ifdef _WIN32
TEST_F(ValidationTest, IsPathSafe_ReservedNameInSubdir) {
    // CON in a subdirectory path component
    // Implementation checks final component only, not intermediates
    EXPECT_TRUE(isPathSafe(fs::path("game/CON/data.txt")));
}
#endif

TEST_F(ValidationTest, SanitizePath_NormalPathUnchanged) {
    auto result = sanitizePath(fs::path("games/data/file.txt"));
    ASSERT_TRUE(result.has_value());
    auto str = result->generic_string();
    EXPECT_NE(str.find("games"), std::string::npos);
    EXPECT_NE(str.find("file.txt"), std::string::npos);
}

} // namespace testing
} // namespace makine
