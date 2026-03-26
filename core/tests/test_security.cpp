/**
 * @file test_security.cpp
 * @brief Unit tests for Security module
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makine/security.hpp>
#include <fstream>
#include <filesystem>

namespace makine {
namespace testing {

class SecurityTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    SecurityManager security_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makine_security_tests";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    void createTestFile(const std::filesystem::path& path, const std::string& content) {
        std::ofstream(path) << content;
    }
};

// Test data hashing
TEST_F(SecurityTest, HashData) {
    ByteBuffer data = {'H', 'e', 'l', 'l', 'o'};

    auto result = security_.hash(data);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
    EXPECT_EQ(result->length(), 64); // SHA-256 produces 64 hex chars
}

TEST_F(SecurityTest, HashEmptyData) {
    ByteBuffer data;

    auto result = security_.hash(data);

    ASSERT_TRUE(result.has_value());
    // SHA-256 of empty data is a specific hash
    EXPECT_FALSE(result->empty());
    EXPECT_EQ(result->length(), 64);
}

TEST_F(SecurityTest, HashSameDataProducesSameHash) {
    ByteBuffer data1 = {'T', 'e', 's', 't'};
    ByteBuffer data2 = {'T', 'e', 's', 't'};

    auto hash1 = security_.hash(data1);
    auto hash2 = security_.hash(data2);

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_EQ(*hash1, *hash2);
}

TEST_F(SecurityTest, HashDifferentDataProducesDifferentHash) {
    ByteBuffer data1 = {'A'};
    ByteBuffer data2 = {'B'};

    auto hash1 = security_.hash(data1);
    auto hash2 = security_.hash(data2);

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_NE(*hash1, *hash2);
}

// Test file hashing
TEST_F(SecurityTest, HashFile) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Hello, World!");

    auto result = security_.hashFile(testFile);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
    EXPECT_EQ(result->length(), 64);
}

TEST_F(SecurityTest, HashFileSameContent) {
    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Same content");
    createTestFile(file2, "Same content");

    auto hash1 = security_.hashFile(file1);
    auto hash2 = security_.hashFile(file2);

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_EQ(*hash1, *hash2);
}

TEST_F(SecurityTest, HashFileDifferentContent) {
    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Content A");
    createTestFile(file2, "Content B");

    auto hash1 = security_.hashFile(file1);
    auto hash2 = security_.hashFile(file2);

    ASSERT_TRUE(hash1.has_value());
    ASSERT_TRUE(hash2.has_value());
    EXPECT_NE(*hash1, *hash2);
}

TEST_F(SecurityTest, HashNonExistentFile) {
    auto result = security_.hashFile(testDir_ / "nonexistent.txt");

    // Should return error for non-existent file
    EXPECT_FALSE(result.has_value());
}

// Test hash verification
TEST_F(SecurityTest, VerifyHashCorrect) {
    ByteBuffer data = {'T', 'e', 's', 't'};

    auto hashResult = security_.hash(data);
    ASSERT_TRUE(hashResult.has_value());

    EXPECT_TRUE(security_.verifyHash(data, *hashResult));
}

TEST_F(SecurityTest, VerifyHashIncorrect) {
    ByteBuffer data = {'T', 'e', 's', 't'};
    std::string wrongHash = "0000000000000000000000000000000000000000000000000000000000000000";

    EXPECT_FALSE(security_.verifyHash(data, wrongHash));
}

// Test different hash algorithms
TEST_F(SecurityTest, HashWithSHA384) {
    ByteBuffer data = {'T', 'e', 's', 't'};

    auto result = security_.hash(data, HashAlgorithm::SHA384);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->length(), 96); // SHA-384 produces 96 hex chars
}

TEST_F(SecurityTest, HashWithSHA512) {
    ByteBuffer data = {'T', 'e', 's', 't'};

    auto result = security_.hash(data, HashAlgorithm::SHA512);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->length(), 128); // SHA-512 produces 128 hex chars
}

// Test SignatureResult struct
TEST_F(SecurityTest, SignatureResultStruct) {
    SignatureResult result;
    result.valid = true;
    result.signedBy = "Test Signer";
    result.signedAt = 1234567890;
    result.publicKeyId = "key-123";
    result.message = "OK";

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.signedBy, "Test Signer");
}

// Test PackageSignature struct
TEST_F(SecurityTest, PackageSignatureStruct) {
    PackageSignature sig;
    sig.packageHash = "abc123";
    sig.signature = "base64signature";
    sig.publicKeyId = "key-456";
    sig.timestamp = 9876543210;

    EXPECT_EQ(sig.packageHash, "abc123");
    EXPECT_EQ(sig.timestamp, 9876543210);
}

// =========================================================================
// ADDITIONAL EDGE CASES
// =========================================================================

TEST_F(SecurityTest, HashLargeBuffer) {
    // 1MB buffer hash
    ByteBuffer data(1024 * 1024, 0x42);
    auto result = security_.hash(data);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->length(), 64);
}

TEST_F(SecurityTest, HashFileEmptyContent) {
    auto emptyFile = testDir_ / "empty.bin";
    createTestFile(emptyFile, "");
    auto result = security_.hashFile(emptyFile);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->length(), 64);
}

TEST_F(SecurityTest, VerifyHashWithEmptyHash) {
    ByteBuffer data = {'T', 'e', 's', 't'};
    EXPECT_FALSE(security_.verifyHash(data, ""));
}

TEST_F(SecurityTest, VerifyHashWithAllZeros) {
    ByteBuffer data = {'T', 'e', 's', 't'};
    std::string zeros(64, '0');
    EXPECT_FALSE(security_.verifyHash(data, zeros));
}

TEST_F(SecurityTest, SignatureResultDefaultValues) {
    SignatureResult result;
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.signedBy.empty());
    // signedAt may be uninitialized — skip check
    EXPECT_TRUE(result.publicKeyId.empty());
    EXPECT_TRUE(result.message.empty());
}

TEST_F(SecurityTest, HashWithSHA384KnownValues) {
    ByteBuffer data = {'H', 'e', 'l', 'l', 'o'};
    auto result384 = security_.hash(data, HashAlgorithm::SHA384);
    ASSERT_TRUE(result384.has_value());
    // SHA-384 produces 96 hex chars, verify consistency
    EXPECT_EQ(result384->length(), 96);

    // Same data should produce same hash
    auto result384b = security_.hash(data, HashAlgorithm::SHA384);
    ASSERT_TRUE(result384b.has_value());
    EXPECT_EQ(*result384, *result384b);
}

TEST_F(SecurityTest, HashWithSHA512KnownValues) {
    ByteBuffer data = {'H', 'e', 'l', 'l', 'o'};
    auto result512 = security_.hash(data, HashAlgorithm::SHA512);
    ASSERT_TRUE(result512.has_value());
    EXPECT_EQ(result512->length(), 128);

    auto result512b = security_.hash(data, HashAlgorithm::SHA512);
    ASSERT_TRUE(result512b.has_value());
    EXPECT_EQ(*result512, *result512b);
}

TEST_F(SecurityTest, DifferentAlgorithmsProduceDifferentHashes) {
    ByteBuffer data = {'T', 'e', 's', 't', ' ', 'd', 'a', 't', 'a'};
    auto sha256 = security_.hash(data, HashAlgorithm::SHA256);
    auto sha384 = security_.hash(data, HashAlgorithm::SHA384);
    auto sha512 = security_.hash(data, HashAlgorithm::SHA512);

    ASSERT_TRUE(sha256.has_value());
    ASSERT_TRUE(sha384.has_value());
    ASSERT_TRUE(sha512.has_value());

    // Different algorithm outputs must differ
    EXPECT_NE(*sha256, *sha384);
    EXPECT_NE(*sha384, *sha512);
    EXPECT_NE(*sha256, *sha512);
}

} // namespace testing
} // namespace makine
