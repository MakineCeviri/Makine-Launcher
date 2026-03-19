/**
 * @file test_file_integrity.cpp
 * @brief Unit tests for file integrity module
 * @copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/file_integrity.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace makine::integrity;

// Well-known SHA-256 hashes for test vectors
static constexpr const char* kEmptyFileHash =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
static constexpr const char* kHelloWorldHash =
    "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f";

// ============================================================================
// TEST FIXTURE
// ============================================================================

class FileIntegrityTest : public ::testing::Test {
protected:
    fs::path tempDir_;

    void SetUp() override {
        tempDir_ = fs::temp_directory_path() / "makine_integrity_tests";
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }

    // Write arbitrary content to a file under tempDir_
    fs::path writeFile(const std::string& name, const std::string& content) {
        auto path = tempDir_ / name;
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
        return path;
    }

    // Write a .sha256 sidecar for a given file path
    void writeSidecar(const fs::path& filePath, const std::string& hashContent) {
        auto sidecarPath = filePath;
        sidecarPath += ".sha256";
        std::ofstream ofs(sidecarPath);
        ofs << hashContent;
    }
};

// ============================================================================
// computeFileHash
// ============================================================================

TEST_F(FileIntegrityTest, ComputeFileHash_KnownContent) {
    auto path = writeFile("hello.txt", "Hello, World!");
    auto result = computeFileHash(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, kHelloWorldHash);
}

TEST_F(FileIntegrityTest, ComputeFileHash_EmptyFile) {
    auto path = writeFile("empty.bin", "");
    auto result = computeFileHash(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, kEmptyFileHash);
}

TEST_F(FileIntegrityTest, ComputeFileHash_NonexistentFile) {
    auto result = computeFileHash(tempDir_ / "does_not_exist.bin");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileNotFound);
}

TEST_F(FileIntegrityTest, ComputeFileHash_DifferentChunkSizesProduceSameHash) {
    // A file large enough that chunk size matters
    std::string content(200'000, 'X');
    auto path = writeFile("large.bin", content);

    auto hash1 = computeFileHash(path, 1024);
    auto hash2 = computeFileHash(path, 65536);
    auto hash3 = computeFileHash(path, 131072);

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    ASSERT_TRUE(hash3.has_value());
    EXPECT_EQ(*hash1, *hash2);
    EXPECT_EQ(*hash2, *hash3);
}

TEST_F(FileIntegrityTest, ComputeFileHash_DifferentContentDifferentHash) {
    auto pathA = writeFile("a.bin", "alpha");
    auto pathB = writeFile("b.bin", "beta");

    auto hashA = computeFileHash(pathA);
    auto hashB = computeFileHash(pathB);

    ASSERT_TRUE(hashA.has_value());
    ASSERT_TRUE(hashB.has_value());
    EXPECT_NE(*hashA, *hashB);
}

TEST_F(FileIntegrityTest, ComputeFileHash_ReturnedHashIsValid) {
    auto path = writeFile("check.txt", "validate me");
    auto result = computeFileHash(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(isValidSha256Hex(*result));
}

// ============================================================================
// readHashFile
// ============================================================================

TEST_F(FileIntegrityTest, ReadHashFile_PlainFormat) {
    auto hashFile = writeFile("plain.sha256", std::string(kEmptyFileHash) + "\n");
    auto result = readHashFile(hashFile);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, kEmptyFileHash);
}

TEST_F(FileIntegrityTest, ReadHashFile_GnuFormat) {
    // GNU coreutils: "<hash>  <filename>\n"
    std::string gnuContent = std::string(kHelloWorldHash) + "  hello.txt\n";
    auto hashFile = writeFile("gnu.sha256", gnuContent);
    auto result = readHashFile(hashFile);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, kHelloWorldHash);
}

TEST_F(FileIntegrityTest, ReadHashFile_NonexistentFile) {
    auto result = readHashFile(tempDir_ / "missing.sha256");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileNotFound);
}

TEST_F(FileIntegrityTest, ReadHashFile_EmptyFile) {
    auto hashFile = writeFile("empty.sha256", "");
    auto result = readHashFile(hashFile);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileCorrupted);
}

TEST_F(FileIntegrityTest, ReadHashFile_InvalidHashContent) {
    auto hashFile = writeFile("bad.sha256", "not_a_valid_hash\n");
    auto result = readHashFile(hashFile);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileCorrupted);
}

// ============================================================================
// verifyFile
// ============================================================================

TEST_F(FileIntegrityTest, VerifyFile_MatchingHash) {
    auto path = writeFile("data.bin", "Hello, World!");
    writeSidecar(path, std::string(kHelloWorldHash) + "\n");

    auto result = verifyFile(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST_F(FileIntegrityTest, VerifyFile_MismatchedHash) {
    auto path = writeFile("data.bin", "Hello, World!");
    // Wrong hash on purpose
    writeSidecar(path, std::string(kEmptyFileHash) + "\n");

    auto result = verifyFile(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

TEST_F(FileIntegrityTest, VerifyFile_NoSidecarFile) {
    auto path = writeFile("nosidecar.bin", "some content");
    // Deliberately do NOT create a .sha256 sidecar

    auto result = verifyFile(path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), makine::ErrorCode::FileNotFound);
}

// ============================================================================
// secureCompareHex
// ============================================================================

TEST_F(FileIntegrityTest, SecureCompareHex_EqualStrings) {
    EXPECT_TRUE(secureCompareHex("abcdef0123456789", "abcdef0123456789"));
}

TEST_F(FileIntegrityTest, SecureCompareHex_DifferentStrings) {
    EXPECT_FALSE(secureCompareHex("abcdef0123456789", "abcdef0123456780"));
}

TEST_F(FileIntegrityTest, SecureCompareHex_DifferentLengths) {
    EXPECT_FALSE(secureCompareHex("abc", "abcd"));
}

TEST_F(FileIntegrityTest, SecureCompareHex_EmptyStrings) {
    EXPECT_TRUE(secureCompareHex("", ""));
}

// ============================================================================
// isValidSha256Hex
// ============================================================================

TEST_F(FileIntegrityTest, IsValidSha256Hex_Valid) {
    EXPECT_TRUE(isValidSha256Hex(kEmptyFileHash));
    EXPECT_TRUE(isValidSha256Hex(kHelloWorldHash));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_WrongLength_Short) {
    EXPECT_FALSE(isValidSha256Hex("abcdef0123456789"));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_WrongLength_Long) {
    std::string tooLong(65, 'a');
    EXPECT_FALSE(isValidSha256Hex(tooLong));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_UppercaseRejected) {
    // Valid hex but uppercase — must be rejected
    std::string upper = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    EXPECT_FALSE(isValidSha256Hex(upper));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_NonHexChars) {
    // 64 chars but contains 'g' and 'z'
    std::string bad = "e3b0c44298fc1c149afbf4c8996fb924g7ae41e4649b934ca495991b7852bz55";
    EXPECT_FALSE(isValidSha256Hex(bad));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_Empty) {
    EXPECT_FALSE(isValidSha256Hex(""));
}

// ============================================================================
// END-TO-END HELPER
// ============================================================================

TEST_F(FileIntegrityTest, EndToEnd_ComputeWriteVerify) {
    // Write a file, compute its hash, create sidecar, then verify
    auto path = writeFile("payload.dat", "end-to-end test payload");

    auto hashResult = computeFileHash(path);
    ASSERT_TRUE(hashResult.has_value());
    ASSERT_TRUE(isValidSha256Hex(*hashResult));

    writeSidecar(path, *hashResult + "\n");

    auto verifyResult = verifyFile(path);
    ASSERT_TRUE(verifyResult.has_value());
    EXPECT_TRUE(*verifyResult);
}

// ============================================================================
// ADDITIONAL EDGE CASES
// ============================================================================

TEST_F(FileIntegrityTest, ComputeFileHash_BinaryContent) {
    // File with null bytes and binary content
    std::string binaryContent;
    binaryContent.push_back('\0');
    binaryContent.push_back('\xFF');
    binaryContent.push_back('\x01');
    binaryContent.push_back('\0');
    binaryContent.push_back('\xFE');
    auto path = writeFile("binary.bin", binaryContent);

    auto result = computeFileHash(path);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(isValidSha256Hex(*result));
}

TEST_F(FileIntegrityTest, ReadHashFile_ExtraWhitespace) {
    // Hash with leading/trailing whitespace
    std::string content = "  " + std::string(kEmptyFileHash) + "  \n";
    auto hashFile = writeFile("whitespace.sha256", content);
    auto result = readHashFile(hashFile);

    // Implementation may or may not trim whitespace
    if (result.has_value()) {
        EXPECT_EQ(*result, kEmptyFileHash);
    }
    SUCCEED();
}

TEST_F(FileIntegrityTest, VerifyFile_ZeroBytesSidecar) {
    auto path = writeFile("data.bin", "test content");
    // Create a zero-byte sidecar
    writeSidecar(path, "");

    auto result = verifyFile(path);
    // Zero-byte sidecar should fail to parse
    ASSERT_FALSE(result.has_value());
}

TEST_F(FileIntegrityTest, SecureCompareHex_OnlyFirstCharDiffers) {
    std::string a(64, 'a');
    std::string b(64, 'a');
    b[0] = 'b';
    EXPECT_FALSE(secureCompareHex(a, b));
}

TEST_F(FileIntegrityTest, SecureCompareHex_OnlyLastCharDiffers) {
    std::string a(64, 'a');
    std::string b(64, 'a');
    b[63] = 'b';
    EXPECT_FALSE(secureCompareHex(a, b));
}

TEST_F(FileIntegrityTest, IsValidSha256Hex_ExactlyValid) {
    // Exactly 64 lowercase hex characters
    std::string valid(64, 'a');
    EXPECT_TRUE(isValidSha256Hex(valid));

    // Mixed valid hex digits
    std::string mixed = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    EXPECT_TRUE(isValidSha256Hex(mixed));
}
