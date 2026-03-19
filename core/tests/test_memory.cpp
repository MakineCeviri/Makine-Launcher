/**
 * @file test_memory.cpp
 * @brief Unit tests for memory.hpp — MemoryTracker, MemoryGuard, ScopedMemoryTrack
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/memory.hpp>

#include <string>

namespace makine {
namespace testing {

class MemoryTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MemoryTracker::reset();
        MemoryTracker::startTracking();
    }

    void TearDown() override {
        MemoryTracker::stopTracking();
        MemoryTracker::reset();
    }
};

// ===========================================================================
// Basic Tracking
// ===========================================================================

TEST_F(MemoryTrackerTest, IsTrackingAfterStart) {
    EXPECT_TRUE(MemoryTracker::isTracking());
}

TEST_F(MemoryTrackerTest, IsNotTrackingAfterStop) {
    MemoryTracker::stopTracking();
    EXPECT_FALSE(MemoryTracker::isTracking());
}

TEST_F(MemoryTrackerTest, RecordIncreasesCurrentBytes) {
    MemoryTracker::record("cache", 1024);
    EXPECT_EQ(MemoryTracker::currentBytes(), 1024);
}

TEST_F(MemoryTrackerTest, ReleaseDecreasesCurrentBytes) {
    MemoryTracker::record("cache", 1024);
    MemoryTracker::release("cache", 512);
    EXPECT_EQ(MemoryTracker::currentBytes(), 512);
}

TEST_F(MemoryTrackerTest, PeakBytesTracked) {
    MemoryTracker::record("cache", 2048);
    MemoryTracker::release("cache", 2048);
    MemoryTracker::record("cache", 1024);

    EXPECT_EQ(MemoryTracker::currentBytes(), 1024);
    EXPECT_GE(MemoryTracker::peakBytes(), 2048);
}

TEST_F(MemoryTrackerTest, MultipleCategoriesTracked) {
    MemoryTracker::record("cache", 1024);
    MemoryTracker::record("strings", 512);
    MemoryTracker::record("buffers", 256);

    EXPECT_EQ(MemoryTracker::currentBytes(), 1024 + 512 + 256);
}

TEST_F(MemoryTrackerTest, RecordIgnoredWhenNotTracking) {
    MemoryTracker::stopTracking();
    MemoryTracker::record("cache", 1024);
    EXPECT_EQ(MemoryTracker::currentBytes(), 0);
}

TEST_F(MemoryTrackerTest, ReleaseIgnoredWhenNotTracking) {
    MemoryTracker::record("cache", 1024);
    MemoryTracker::stopTracking();
    MemoryTracker::release("cache", 512);
    // Current bytes should still be 1024 since release was ignored
    EXPECT_EQ(MemoryTracker::currentBytes(), 1024);
}

// ===========================================================================
// Category Stats
// ===========================================================================

TEST_F(MemoryTrackerTest, CategoryStatsCorrect) {
    MemoryTracker::record("cache", 1024);
    MemoryTracker::record("cache", 2048);
    MemoryTracker::release("cache", 512);

    auto stats = MemoryTracker::getCategoryStats("cache");
    EXPECT_EQ(stats.name, "cache");
    EXPECT_EQ(stats.currentBytes, 1024 + 2048 - 512);
    EXPECT_EQ(stats.allocationCount, 2u);
    EXPECT_EQ(stats.releaseCount, 1u);
    EXPECT_EQ(stats.totalAllocated, 1024u + 2048u);
    EXPECT_EQ(stats.totalReleased, 512u);
}

TEST_F(MemoryTrackerTest, CategoryStatsUnknownCategory) {
    auto stats = MemoryTracker::getCategoryStats("nonexistent");
    EXPECT_EQ(stats.name, "nonexistent");
    EXPECT_EQ(stats.currentBytes, 0);
    EXPECT_EQ(stats.allocationCount, 0u);
}

TEST_F(MemoryTrackerTest, AverageAllocationSize) {
    MemoryTracker::record("cache", 100);
    MemoryTracker::record("cache", 200);
    MemoryTracker::record("cache", 300);

    auto stats = MemoryTracker::getCategoryStats("cache");
    EXPECT_DOUBLE_EQ(stats.averageAllocationSize(), 200.0);
}

TEST_F(MemoryTrackerTest, AverageAllocationSizeZeroAllocations) {
    CategoryMemoryStats stats;
    EXPECT_DOUBLE_EQ(stats.averageAllocationSize(), 0.0);
}

// ===========================================================================
// Memory Report
// ===========================================================================

TEST_F(MemoryTrackerTest, GetReportNonEmpty) {
    MemoryTracker::record("test", 1024);

    auto report = MemoryTracker::getReport();
    EXPECT_EQ(report.trackedCurrentBytes, 1024);
    EXPECT_GE(report.trackedPeakBytes, 1024);
    EXPECT_GE(report.trackedTotalAllocated, 1024u);
    EXPECT_FALSE(report.categories.empty());
}

TEST_F(MemoryTrackerTest, ReportToTextNonEmpty) {
    MemoryTracker::record("test", 1024);
    auto report = MemoryTracker::getReport();
    auto text = report.toText();
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("Memory Report"), std::string::npos);
}

TEST_F(MemoryTrackerTest, ReportToJsonNonEmpty) {
    MemoryTracker::record("test", 1024);
    auto report = MemoryTracker::getReport();
    auto json = report.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"process\""), std::string::npos);
    EXPECT_NE(json.find("\"categories\""), std::string::npos);
}

TEST_F(MemoryTrackerTest, ProcessMemoryReported) {
    auto report = MemoryTracker::getReport();
    // On Windows, process memory should be > 0
    EXPECT_GT(report.processCurrentBytes, 0u);
    EXPECT_GT(report.processPeakBytes, 0u);
}

TEST_F(MemoryTrackerTest, SystemMemoryReported) {
    auto report = MemoryTracker::getReport();
    EXPECT_GT(report.systemTotalBytes, 0u);
    EXPECT_GT(report.systemAvailableBytes, 0u);
    EXPECT_GT(report.systemMemoryLoad, 0u);
    EXPECT_LE(report.systemMemoryLoad, 100u);
}

// ===========================================================================
// Reset
// ===========================================================================

TEST_F(MemoryTrackerTest, ResetClearsAll) {
    MemoryTracker::record("cache", 1024);
    MemoryTracker::reset();

    EXPECT_EQ(MemoryTracker::currentBytes(), 0);
    EXPECT_EQ(MemoryTracker::peakBytes(), 0);
    auto stats = MemoryTracker::getCategoryStats("cache");
    EXPECT_EQ(stats.allocationCount, 0u);
}

// ===========================================================================
// Callback
// ===========================================================================

TEST_F(MemoryTrackerTest, CallbackFires) {
    int callbackCount = 0;
    MemoryTracker::addCallback([&](const std::string&, int64_t, bool) {
        ++callbackCount;
    });

    MemoryTracker::record("test", 100);
    MemoryTracker::release("test", 50);
    EXPECT_EQ(callbackCount, 2);
}

// ===========================================================================
// Tracking Duration
// ===========================================================================

TEST_F(MemoryTrackerTest, TrackingDurationNonNegative) {
    auto duration = MemoryTracker::trackingDuration();
    EXPECT_GE(duration.count(), 0);
}

// ===========================================================================
// MemoryGuard
// ===========================================================================

TEST(MemoryGuardTest, CanAllocateUnlimited) {
    MemoryTracker::reset();
    MemoryGuard::setLimits(MemoryLimits{0, 0, 80});
    EXPECT_TRUE(MemoryGuard::canAllocate("test", 1024));
    MemoryTracker::reset();
}

TEST(MemoryGuardTest, CanAllocateWithinTotalLimit) {
    MemoryTracker::reset();
    MemoryTracker::startTracking();

    MemoryLimits limits;
    limits.maxTotalBytes = 2048;
    MemoryGuard::setLimits(limits);

    EXPECT_TRUE(MemoryGuard::canAllocate("test", 1024));

    MemoryTracker::record("test", 1500);
    EXPECT_FALSE(MemoryGuard::canAllocate("test", 1024));

    MemoryTracker::stopTracking();
    MemoryTracker::reset();
}

TEST(MemoryGuardTest, CanAllocateWithinCategoryLimit) {
    MemoryTracker::reset();
    MemoryTracker::startTracking();

    MemoryLimits limits;
    limits.maxCategoryBytes = 1024;
    MemoryGuard::setLimits(limits);

    EXPECT_TRUE(MemoryGuard::canAllocate("test", 512));

    MemoryTracker::record("test", 800);
    EXPECT_FALSE(MemoryGuard::canAllocate("test", 512));

    MemoryTracker::stopTracking();
    MemoryTracker::reset();
}

TEST(MemoryGuardTest, IsNearLimitUnlimited) {
    MemoryTracker::reset();
    MemoryGuard::setLimits(MemoryLimits{0, 0, 80});
    EXPECT_FALSE(MemoryGuard::isNearLimit());
    MemoryTracker::reset();
}

TEST(MemoryGuardTest, UsagePercentUnlimited) {
    MemoryTracker::reset();
    MemoryGuard::setLimits(MemoryLimits{0, 0, 80});
    EXPECT_DOUBLE_EQ(MemoryGuard::usagePercent(), 0.0);
    MemoryTracker::reset();
}

// ===========================================================================
// Convenience Functions
// ===========================================================================

TEST(MemoryConvenienceTest, MemorySnapshotReturnsReport) {
    auto report = memorySnapshot();
    EXPECT_GT(report.processCurrentBytes, 0u);
}

TEST(MemoryConvenienceTest, StartStopTracking) {
    stopMemoryTracking();
    EXPECT_FALSE(MemoryTracker::isTracking());

    startMemoryTracking();
    EXPECT_TRUE(MemoryTracker::isTracking());

    stopMemoryTracking();
    MemoryTracker::reset();
}

} // namespace testing
} // namespace makine
