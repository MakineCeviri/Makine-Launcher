/**
 * @file test_health.cpp
 * @brief Unit tests for health.hpp -- ComponentHealth, HealthStatus, HealthChecker
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/health.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace makine {
namespace testing {

namespace fs = std::filesystem;

// ===========================================================================
// ComponentHealth
// ===========================================================================

TEST(ComponentHealthTest, DefaultValues) {
    ComponentHealth ch;
    EXPECT_TRUE(ch.name.empty());
    EXPECT_TRUE(ch.healthy);
    EXPECT_EQ(ch.status, "OK");
    EXPECT_EQ(ch.latency.count(), 0);
    EXPECT_TRUE(ch.details.empty());
}

TEST(ComponentHealthTest, IsHealthyReflectsFlag) {
    ComponentHealth ch;
    ch.healthy = true;
    EXPECT_TRUE(ch.isHealthy());
    ch.healthy = false;
    EXPECT_FALSE(ch.isHealthy());
}

TEST(ComponentHealthTest, SetFields) {
    ComponentHealth ch;
    ch.name = "TestComponent";
    ch.healthy = false;
    ch.status = "Connection failed";
    ch.latency = std::chrono::milliseconds(150);
    ch.details.push_back("detail1");
    ch.details.push_back("detail2");
    EXPECT_EQ(ch.name, "TestComponent");
    EXPECT_FALSE(ch.healthy);
    EXPECT_EQ(ch.status, "Connection failed");
    EXPECT_EQ(ch.latency.count(), 150);
    EXPECT_EQ(ch.details.size(), 2u);
}

// ===========================================================================
// HealthStatus
// ===========================================================================

TEST(HealthStatusTest, DefaultValues) {
    HealthStatus hs;
    EXPECT_TRUE(hs.healthy);
    EXPECT_EQ(hs.availableDiskSpaceBytes, 0u);
    EXPECT_EQ(hs.totalDiskSpaceBytes, 0u);
    EXPECT_EQ(hs.memoryUsageBytes, 0u);
    EXPECT_EQ(hs.peakMemoryUsageBytes, 0u);
    EXPECT_EQ(hs.availableMemoryBytes, 0u);
    EXPECT_TRUE(hs.warnings.empty());
    EXPECT_TRUE(hs.errors.empty());
}

TEST(HealthStatusTest, IsHealthyWithNoErrors) {
    HealthStatus hs;
    hs.healthy = true;
    EXPECT_TRUE(hs.isHealthy());
}

TEST(HealthStatusTest, IsHealthyFalseWhenUnhealthy) {
    HealthStatus hs;
    hs.healthy = false;
    EXPECT_FALSE(hs.isHealthy());
}

TEST(HealthStatusTest, IsHealthyFalseWithErrors) {
    HealthStatus hs;
    hs.healthy = true;
    hs.errors.push_back("Something broke");
    EXPECT_FALSE(hs.isHealthy());
}

TEST(HealthStatusTest, HasWarningsEmpty) {
    HealthStatus hs;
    EXPECT_FALSE(hs.hasWarnings());
}

TEST(HealthStatusTest, HasWarningsTrue) {
    HealthStatus hs;
    hs.warnings.push_back("Low disk");
    EXPECT_TRUE(hs.hasWarnings());
}

TEST(HealthStatusTest, DiskUsagePercentZeroTotal) {
    HealthStatus hs;
    hs.totalDiskSpaceBytes = 0;
    hs.availableDiskSpaceBytes = 0;
    EXPECT_DOUBLE_EQ(hs.diskUsagePercent(), 0.0);
}

TEST(HealthStatusTest, DiskUsagePercentHalf) {
    HealthStatus hs;
    hs.totalDiskSpaceBytes = 1000;
    hs.availableDiskSpaceBytes = 500;
    EXPECT_NEAR(hs.diskUsagePercent(), 50.0, 0.1);
}

TEST(HealthStatusTest, DiskUsagePercentFull) {
    HealthStatus hs;
    hs.totalDiskSpaceBytes = 1000;
    hs.availableDiskSpaceBytes = 0;
    EXPECT_NEAR(hs.diskUsagePercent(), 100.0, 0.1);
}

TEST(HealthStatusTest, DiskUsagePercentEmpty) {
    HealthStatus hs;
    hs.totalDiskSpaceBytes = 1000;
    hs.availableDiskSpaceBytes = 1000;
    EXPECT_NEAR(hs.diskUsagePercent(), 0.0, 0.1);
}

TEST(HealthStatusTest, AvailableDiskSpaceHuman) {
    HealthStatus hs;
    hs.availableDiskSpaceBytes = 1024 * 1024 * 512;
    auto human = hs.availableDiskSpaceHuman();
    EXPECT_FALSE(human.empty());
    EXPECT_NE(human.find("MB"), std::string::npos);
}

TEST(HealthStatusTest, MemoryUsageHuman) {
    HealthStatus hs;
    hs.memoryUsageBytes = 1024 * 1024;
    auto human = hs.memoryUsageHuman();
    EXPECT_FALSE(human.empty());
    EXPECT_NE(human.find("MB"), std::string::npos);
}

TEST(HealthStatusTest, MemoryUsageHumanZero) {
    HealthStatus hs;
    hs.memoryUsageBytes = 0;
    auto human = hs.memoryUsageHuman();
    EXPECT_FALSE(human.empty());
    EXPECT_NE(human.find("B"), std::string::npos);
}

TEST(HealthStatusTest, AvailableDiskSpaceHumanGB) {
    HealthStatus hs;
    hs.availableDiskSpaceBytes = static_cast<uint64_t>(1024) * 1024 * 1024 * 2;
    auto human = hs.availableDiskSpaceHuman();
    EXPECT_NE(human.find("GB"), std::string::npos);
}

TEST(HealthStatusTest, ToTextContainsComponents) {
    HealthStatus hs;
    hs.healthy = true;
    hs.database.status = "OK";
    hs.fileSystem.status = "OK";
    hs.memory.status = "OK";
    hs.network.status = "OK";
    auto text = hs.toText();
    EXPECT_NE(text.find("HEALTHY"), std::string::npos);
    EXPECT_NE(text.find("Database"), std::string::npos);
    EXPECT_NE(text.find("FileSystem"), std::string::npos);
    EXPECT_NE(text.find("Memory"), std::string::npos);
    EXPECT_NE(text.find("Network"), std::string::npos);
}

TEST(HealthStatusTest, ToTextContainsErrors) {
    HealthStatus hs;
    hs.healthy = false;
    hs.errors.push_back("Database connection failed");
    auto text = hs.toText();
    EXPECT_NE(text.find("UNHEALTHY"), std::string::npos);
    EXPECT_NE(text.find("Database connection failed"), std::string::npos);
}

TEST(HealthStatusTest, ToTextContainsWarnings) {
    HealthStatus hs;
    hs.warnings.push_back("Low disk space");
    auto text = hs.toText();
    EXPECT_NE(text.find("Low disk space"), std::string::npos);
}

// ===========================================================================
// HealthChecker -- Component Checks
// ===========================================================================

class HealthCheckerTest : public ::testing::Test {
protected:
    fs::path testDir_;
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makine_health_tests";
        fs::create_directories(testDir_);
        HealthChecker::instance().setDataDirectory("");
        HealthChecker::instance().setDatabasePath("");
        HealthChecker::instance().setMinDiskSpace(100 * 1024 * 1024);
        HealthChecker::instance().setMaxMemoryUsage(500 * 1024 * 1024);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }
};

TEST_F(HealthCheckerTest, CheckDatabaseNoPaths) {
    auto health = HealthChecker::instance().checkDatabase();
    EXPECT_TRUE(health.healthy);
    EXPECT_EQ(health.name, "Database");
}

TEST_F(HealthCheckerTest, CheckDatabaseMissingFile) {
    HealthChecker::instance().setDatabasePath(testDir_ / "nonexistent.db");
    auto health = HealthChecker::instance().checkDatabase();
    EXPECT_FALSE(health.healthy);
    EXPECT_NE(health.status.find("not found"), std::string::npos);
}

TEST_F(HealthCheckerTest, CheckDatabaseInvalidFormat) {
    auto dbPath = testDir_ / "invalid.db";
    { std::ofstream f(dbPath); f << "not a sqlite database"; }
    HealthChecker::instance().setDatabasePath(dbPath);
    auto health = HealthChecker::instance().checkDatabase();
    EXPECT_FALSE(health.healthy);
    EXPECT_NE(health.status.find("Invalid"), std::string::npos);
}

TEST_F(HealthCheckerTest, CheckDatabaseValidSqlite) {
    auto dbPath = testDir_ / "valid.db";
    {
        std::ofstream f(dbPath, std::ios::binary);
        const char header[] = "SQLite format 3";
        f.write(header, 16);
    }
    HealthChecker::instance().setDatabasePath(dbPath);
    auto health = HealthChecker::instance().checkDatabase();
    EXPECT_TRUE(health.healthy);
    EXPECT_EQ(health.status, "OK");
}

TEST_F(HealthCheckerTest, CheckFileSystemDefault) {
    auto health = HealthChecker::instance().checkFileSystem();
    EXPECT_TRUE(health.healthy);
    EXPECT_EQ(health.name, "FileSystem");
}

TEST_F(HealthCheckerTest, CheckFileSystemWithDataDir) {
    HealthChecker::instance().setDataDirectory(testDir_);
    auto health = HealthChecker::instance().checkFileSystem();
    EXPECT_TRUE(health.healthy);
}

TEST_F(HealthCheckerTest, CheckFileSystemNonexistentDir) {
    HealthChecker::instance().setDataDirectory(testDir_ / "nonexistent_dir");
    auto health = HealthChecker::instance().checkFileSystem();
    EXPECT_FALSE(health.healthy);
}

TEST_F(HealthCheckerTest, CheckMemoryAllocates) {
    auto health = HealthChecker::instance().checkMemory();
    EXPECT_TRUE(health.healthy);
    EXPECT_EQ(health.name, "Memory");
    EXPECT_EQ(health.status, "OK");
}

TEST_F(HealthCheckerTest, CheckNetworkPlaceholder) {
    auto health = HealthChecker::instance().checkNetwork();
    EXPECT_TRUE(health.healthy);
    EXPECT_EQ(health.name, "Network");
}

TEST_F(HealthCheckerTest, FullCheckReturnsStatus) {
    auto status = HealthChecker::instance().check();
    EXPECT_TRUE(status.isHealthy());
    EXPECT_TRUE(status.errors.empty());
}

TEST_F(HealthCheckerTest, FullCheckHasTimestamp) {
    auto before = std::chrono::system_clock::now();
    auto status = HealthChecker::instance().check();
    auto after = std::chrono::system_clock::now();
    EXPECT_GE(status.timestamp, before);
    EXPECT_LE(status.timestamp, after);
}

TEST_F(HealthCheckerTest, FullCheckWithFailingDatabase) {
    HealthChecker::instance().setDatabasePath(testDir_ / "nonexistent.db");
    auto status = HealthChecker::instance().check();
    EXPECT_FALSE(status.isHealthy());
    EXPECT_FALSE(status.errors.empty());
}

TEST_F(HealthCheckerTest, FullCheckDiskSpacePopulated) {
    auto status = HealthChecker::instance().check();
    EXPECT_GT(status.totalDiskSpaceBytes, 0u);
    EXPECT_GT(status.availableDiskSpaceBytes, 0u);
}

TEST_F(HealthCheckerTest, FullCheckMemoryPopulated) {
    auto status = HealthChecker::instance().check();
    EXPECT_GT(status.memoryUsageBytes, 0u);
}

TEST_F(HealthCheckerTest, LowDiskSpaceWarning) {
    HealthChecker::instance().setMinDiskSpace(
        static_cast<uint64_t>(1024) * 1024 * 1024 * 1024 * 100);
    auto status = HealthChecker::instance().check();
    EXPECT_TRUE(status.hasWarnings());
}

TEST_F(HealthCheckerTest, HighMemoryWarning) {
    HealthChecker::instance().setMaxMemoryUsage(1);
    auto status = HealthChecker::instance().check();
    EXPECT_TRUE(status.hasWarnings());
}

TEST_F(HealthCheckerTest, LatencyIsNonNegative) {
    auto health = HealthChecker::instance().checkDatabase();
    EXPECT_GE(health.latency.count(), 0);
    health = HealthChecker::instance().checkFileSystem();
    EXPECT_GE(health.latency.count(), 0);
    health = HealthChecker::instance().checkMemory();
    EXPECT_GE(health.latency.count(), 0);
    health = HealthChecker::instance().checkNetwork();
    EXPECT_GE(health.latency.count(), 0);
}

// ===========================================================================
// Convenience Functions
// ===========================================================================

TEST(HealthConvenienceTest, HealthCheckerReference) {
    auto& checker = healthChecker();
    EXPECT_EQ(&checker, &HealthChecker::instance());
}

TEST(HealthConvenienceTest, IsSystemHealthy) {
    HealthChecker::instance().setDatabasePath("");
    HealthChecker::instance().setDataDirectory("");
    bool healthy = isSystemHealthy();
    EXPECT_TRUE(healthy);
}

} // namespace testing
} // namespace makine
