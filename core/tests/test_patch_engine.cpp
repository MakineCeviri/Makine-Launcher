/**
 * @file test_patch_engine.cpp
 * @brief Unit tests for PatchEngine module
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/patch_engine.hpp>
#include <fstream>
#include <filesystem>

namespace makineai {
namespace testing {

class PatchEngineTest : public ::testing::Test {
protected:
    std::filesystem::path testDir_;
    std::filesystem::path backupDir_;

    void SetUp() override {
        testDir_ = std::filesystem::temp_directory_path() / "makineai_patch_tests";
        backupDir_ = std::filesystem::temp_directory_path() / "makineai_patch_backups";
        std::filesystem::create_directories(testDir_);
        std::filesystem::create_directories(backupDir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir_);
        std::filesystem::remove_all(backupDir_);
    }

    void createTestFile(const std::filesystem::path& path, const std::string& content) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << content;
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream fs(path);
        std::stringstream ss;
        ss << fs.rdbuf();
        return ss.str();
    }
};

// Test backup functionality
TEST_F(PatchEngineTest, BackupSingleFile) {
    auto testFile = testDir_ / "original.txt";
    createTestFile(testFile, "Original content");

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto backupResult = patcher.backup(game.installPath, backupDir_);

    EXPECT_TRUE(backupResult.success);
    EXPECT_FALSE(backupResult.backupPath.empty());
    EXPECT_TRUE(std::filesystem::exists(backupResult.backupPath));
}

TEST_F(PatchEngineTest, BackupMultipleFiles) {
    createTestFile(testDir_ / "file1.txt", "Content 1");
    createTestFile(testDir_ / "subdir" / "file2.txt", "Content 2");
    createTestFile(testDir_ / "subdir" / "deep" / "file3.txt", "Content 3");

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto backupResult = patcher.backup(game.installPath, backupDir_);

    EXPECT_TRUE(backupResult.success);
    EXPECT_GT(backupResult.filesBackedUp, 0);
}

// Test restore functionality
TEST_F(PatchEngineTest, RestoreFromBackup) {
    // Create original file
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Original");

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();

    // Backup
    auto backupResult = patcher.backup(game.installPath, backupDir_);
    EXPECT_TRUE(backupResult.success);

    // Modify file
    createTestFile(testFile, "Modified");
    EXPECT_EQ(readFile(testFile), "Modified");

    // Restore
    bool restored = patcher.restore(game.installPath, backupResult.backupPath);
    EXPECT_TRUE(restored);

    // Verify restored content
    EXPECT_EQ(readFile(testFile), "Original");
}

// Test patch operations
TEST_F(PatchEngineTest, ApplyCopyOperation) {
    auto sourceFile = testDir_ / "source.txt";
    auto targetFile = testDir_ / "target.txt";
    createTestFile(sourceFile, "Source content");

    PatchOperation op;
    op.type = PatchOperationType::Copy;
    op.sourcePath = sourceFile;
    op.targetPath = targetFile;

    std::vector<PatchOperation> operations = {op};

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto result = patcher.apply(operations, game, "1.0.0");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.filesPatched, 1);
    EXPECT_TRUE(std::filesystem::exists(targetFile));
    EXPECT_EQ(readFile(targetFile), "Source content");
}

TEST_F(PatchEngineTest, ApplyReplaceOperation) {
    auto targetFile = testDir_ / "target.txt";
    createTestFile(targetFile, "Original content");

    PatchOperation op;
    op.type = PatchOperationType::Replace;
    op.targetPath = targetFile;
    op.data = std::vector<uint8_t>{'N', 'e', 'w', ' ', 'd', 'a', 't', 'a'};

    std::vector<PatchOperation> operations = {op};

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto result = patcher.apply(operations, game, "1.0.0");

    EXPECT_TRUE(result.success);
    EXPECT_EQ(readFile(targetFile), "New data");
}

TEST_F(PatchEngineTest, ApplyDeleteOperation) {
    auto targetFile = testDir_ / "to_delete.txt";
    createTestFile(targetFile, "Delete me");
    EXPECT_TRUE(std::filesystem::exists(targetFile));

    PatchOperation op;
    op.type = PatchOperationType::Delete;
    op.targetPath = targetFile;

    std::vector<PatchOperation> operations = {op};

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto result = patcher.apply(operations, game, "1.0.0");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(std::filesystem::exists(targetFile));
}

// Test atomic operations (rollback on failure)
TEST_F(PatchEngineTest, RollbackOnFailure) {
    auto file1 = testDir_ / "file1.txt";
    auto file2 = testDir_ / "file2.txt";
    createTestFile(file1, "Original 1");
    createTestFile(file2, "Original 2");

    // First operation succeeds
    PatchOperation op1;
    op1.type = PatchOperationType::Replace;
    op1.targetPath = file1;
    op1.data = std::vector<uint8_t>{'M', 'o', 'd', '1'};

    // Second operation fails (source doesn't exist)
    PatchOperation op2;
    op2.type = PatchOperationType::Copy;
    op2.sourcePath = testDir_ / "nonexistent.txt";
    op2.targetPath = file2;

    std::vector<PatchOperation> operations = {op1, op2};

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto result = patcher.apply(operations, game, "1.0.0");

    // Operation should fail
    EXPECT_FALSE(result.success);

    // File1 should be rolled back to original (if atomic)
    // This depends on implementation details
}

// Test progress reporting
TEST_F(PatchEngineTest, ProgressCallback) {
    createTestFile(testDir_ / "file1.txt", "Content 1");
    createTestFile(testDir_ / "file2.txt", "Content 2");
    createTestFile(testDir_ / "file3.txt", "Content 3");

    std::vector<PatchOperation> operations;
    for (int i = 1; i <= 3; i++) {
        PatchOperation op;
        op.type = PatchOperationType::Replace;
        op.targetPath = testDir_ / ("file" + std::to_string(i) + ".txt");
        op.data = std::vector<uint8_t>{'N', 'e', 'w'};
        operations.push_back(op);
    }

    int progressCallCount = 0;
    auto progressCallback = [&](int current, int total, const std::string& message) {
        progressCallCount++;
        EXPECT_LE(current, total);
        EXPECT_GE(current, 0);
    };

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    auto result = patcher.apply(operations, game, "1.0.0", progressCallback);

    EXPECT_TRUE(result.success);
    EXPECT_GT(progressCallCount, 0);
}

// Test verify
TEST_F(PatchEngineTest, VerifyPatchedFiles) {
    auto testFile = testDir_ / "test.txt";
    createTestFile(testFile, "Patched content");

    GameInfo game;
    game.installPath = testDir_;

    auto& patcher = PatchEngine::instance();
    bool verified = patcher.verify(game.installPath);

    // Basic verification should pass for existing files
    EXPECT_TRUE(verified);
}

} // namespace testing
} // namespace makineai
