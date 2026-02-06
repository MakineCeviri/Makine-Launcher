/**
 * @file test_security.cpp
 * @brief Unit tests for Security module
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/security.hpp>
#include <fstream>
#include <filesystem>

namespace makineai {
namespace testing {

class SecurityTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makineai_security_tests";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
    }

    void createTestFile(const std::filesystem::path& path, const std::string& content) {
        std::ofstream(path) << content;
    }
};

// Test file hashing
TEST_F(SecurityTest, HashFile) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Hello, World!");

    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(testFile);

    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64); // SHA-256 produces 64 hex chars
}

TEST_F(SecurityTest, HashFileSameContent) {
    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Same content");
    createTestFile(file2, "Same content");

    auto& security = SecurityManager::instance();
    std::string hash1 = security.hashFile(file1);
    std::string hash2 = security.hashFile(file2);

    EXPECT_EQ(hash1, hash2);
}

TEST_F(SecurityTest, HashFileDifferentContent) {
    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Content A");
    createTestFile(file2, "Content B");

    auto& security = SecurityManager::instance();
    std::string hash1 = security.hashFile(file1);
    std::string hash2 = security.hashFile(file2);

    EXPECT_NE(hash1, hash2);
}

TEST_F(SecurityTest, HashNonExistentFile) {
    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(testDir_ / "nonexistent.txt");

    EXPECT_TRUE(hash.empty());
}

// Test data hashing
TEST_F(SecurityTest, HashData) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};

    auto& security = SecurityManager::instance();
    std::string hash = security.hashData(data);

    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64);
}

TEST_F(SecurityTest, HashEmptyData) {
    std::vector<uint8_t> data;

    auto& security = SecurityManager::instance();
    std::string hash = security.hashData(data);

    // SHA-256 of empty data is a specific hash
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64);
}

// Test file integrity verification
TEST_F(SecurityTest, VerifyFileIntegrityMatch) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Test content");

    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(testFile);

    EXPECT_TRUE(security.verifyFileIntegrity(testFile, hash));
}

TEST_F(SecurityTest, VerifyFileIntegrityMismatch) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Test content");

    auto& security = SecurityManager::instance();
    std::string wrongHash = "0000000000000000000000000000000000000000000000000000000000000000";

    EXPECT_FALSE(security.verifyFileIntegrity(testFile, wrongHash));
}

TEST_F(SecurityTest, VerifyFileIntegrityCaseInsensitive) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Test content");

    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(testFile);

    // Convert to uppercase
    std::string upperHash = hash;
    std::transform(upperHash.begin(), upperHash.end(), upperHash.begin(), ::toupper);

    EXPECT_TRUE(security.verifyFileIntegrity(testFile, upperHash));
}

// Test package signature verification
TEST_F(SecurityTest, VerifyPackageSignatureNoKey) {
    // Without a proper public key loaded, verification should fail
    TranslationPackage package;
    package.id = "test_package";
    package.signature = "invalid_signature";

    std::vector<uint8_t> packageData = {'t', 'e', 's', 't'};

    auto& security = SecurityManager::instance();
    bool valid = security.verifyPackageSignature(package, packageData);

    // Should fail without proper key
    EXPECT_FALSE(valid);
}

// Test Authenticode verification (Windows only)
#ifdef _WIN32
TEST_F(SecurityTest, VerifyAuthenticodeUnsigned) {
    auto testFile = testDir_ / "unsigned.exe";
    createTestFile(testFile, "MZ"); // Minimal PE header

    auto& security = SecurityManager::instance();
    bool valid = security.verifyAuthenticode(testFile);

    // Unsigned file should fail
    EXPECT_FALSE(valid);
}
#endif

// Test IntegrityChecker
TEST_F(SecurityTest, IntegrityCheckerAddFile) {
    auto checker = IntegrityChecker::create();
    EXPECT_NE(checker, nullptr);

    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Test");

    auto& security = SecurityManager::instance();
    std::string hash = security.hashFile(testFile);

    checker->addFile(testFile, hash);
    auto result = checker->check();

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.validFiles, 1);
    EXPECT_EQ(result.missingFiles.size(), 0);
    EXPECT_EQ(result.modifiedFiles.size(), 0);
}

TEST_F(SecurityTest, IntegrityCheckerDetectMissing) {
    auto checker = IntegrityChecker::create();

    // Add non-existent file
    checker->addFile(testDir_ / "missing.txt", "somehash");
    auto result = checker->check();

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.missingFiles.size(), 1);
}

TEST_F(SecurityTest, IntegrityCheckerDetectModified) {
    auto checker = IntegrityChecker::create();

    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Original");

    auto& security = SecurityManager::instance();
    std::string originalHash = security.hashFile(testFile);

    // Modify file
    createTestFile(testFile, "Modified");

    checker->addFile(testFile, originalHash);
    auto result = checker->check();

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.modifiedFiles.size(), 1);
}

TEST_F(SecurityTest, IntegrityCheckerSaveAndLoadChecksums) {
    auto checker = IntegrityChecker::create();

    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Content 1");
    createTestFile(file2, "Content 2");

    auto& security = SecurityManager::instance();
    checker->addFile(file1, security.hashFile(file1));
    checker->addFile(file2, security.hashFile(file2));

    // Save checksums
    auto checksumFile = testDir_ / "checksums.txt";
    checker->saveChecksums(checksumFile);

    EXPECT_TRUE(std::filesystem::exists(checksumFile));

    // Load in new checker
    auto newChecker = IntegrityChecker::create();
    newChecker->loadChecksums(checksumFile);

    auto result = newChecker->check();
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.validFiles, 2);
}

} // namespace testing
} // namespace makineai
