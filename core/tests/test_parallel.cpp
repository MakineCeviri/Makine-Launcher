/**
 * @file test_parallel.cpp
 * @brief Unit tests for parallel execution utilities
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/parallel.hpp>

#include <atomic>
#include <numeric>
#include <string>
#include <vector>

namespace makine {
namespace testing {

class ParallelTest : public ::testing::Test {
protected:
    void SetUp() override {
        savedConfig_ = parallel::globalConfig;
        parallel::globalConfig.minItemsForParallel = 2;
    }

    void TearDown() override {
        parallel::globalConfig = savedConfig_;
    }

private:
    parallel::Config savedConfig_;
};

// =========================================================================
// map
// =========================================================================

TEST_F(ParallelTest, MapTransformsItems) {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto results = parallel::map(input, [](const int& x) { return x * 2; });

    ASSERT_EQ(results.size(), 5u);
    EXPECT_EQ(results[0], 2);
    EXPECT_EQ(results[1], 4);
    EXPECT_EQ(results[2], 6);
    EXPECT_EQ(results[3], 8);
    EXPECT_EQ(results[4], 10);
}

TEST_F(ParallelTest, MapEmptyVector) {
    std::vector<int> empty;
    auto results = parallel::map(empty, [](const int& x) { return x; });
    EXPECT_TRUE(results.empty());
}

TEST_F(ParallelTest, MapSingleItem) {
    std::vector<int> single = {5};
    auto results = parallel::map(single, [](const int& x) { return x * 10; });

    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], 50);
}

TEST_F(ParallelTest, MapPreservesOrder) {
    std::vector<int> input(50);
    std::iota(input.begin(), input.end(), 0);

    auto results = parallel::map(input, [](const int& x) { return x; });

    ASSERT_EQ(results.size(), 50u);
    for (size_t i = 0; i < 50; ++i) {
        EXPECT_EQ(results[i], static_cast<int>(i)) << "Order mismatch at index " << i;
    }
}

TEST_F(ParallelTest, MapWithStringTransform) {
    std::vector<std::string> input = {"hello", "world"};
    auto results = parallel::map(input, [](const std::string& s) {
        return s.size();
    });

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0], 5u);
    EXPECT_EQ(results[1], 5u);
}

// =========================================================================
// flatMap
// =========================================================================

TEST_F(ParallelTest, FlatMapMergesResults) {
    std::vector<int> input = {1, 2, 3};
    auto results = parallel::flatMap(input, [](const int& x) {
        return std::vector<int>{x, x * 10};
    });

    ASSERT_EQ(results.size(), 6u);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 10);
    EXPECT_EQ(results[2], 2);
    EXPECT_EQ(results[3], 20);
    EXPECT_EQ(results[4], 3);
    EXPECT_EQ(results[5], 30);
}

TEST_F(ParallelTest, FlatMapEmptyInput) {
    std::vector<int> empty;
    auto results = parallel::flatMap(empty, [](const int& x) {
        return std::vector<int>{x};
    });
    EXPECT_TRUE(results.empty());
}

// =========================================================================
// forEach
// =========================================================================

TEST_F(ParallelTest, ForEachAppliesSideEffect) {
    std::vector<int> input = {1, 2, 3, 4, 5};
    std::atomic<int> sum{0};

    parallel::forEach(input, [&sum](const int& x) {
        sum.fetch_add(x, std::memory_order_relaxed);
    });

    EXPECT_EQ(sum.load(), 15);
}

TEST_F(ParallelTest, ForEachEmptyInput) {
    std::vector<int> empty;
    std::atomic<int> count{0};

    parallel::forEach(empty, [&count](const int&) {
        count.fetch_add(1);
    });

    EXPECT_EQ(count.load(), 0);
}

TEST_F(ParallelTest, ForEachSingleItem) {
    std::vector<int> single = {42};
    std::atomic<int> sum{0};

    parallel::forEach(single, [&sum](const int& x) {
        sum.fetch_add(x);
    });

    EXPECT_EQ(sum.load(), 42);
}

// =========================================================================
// Progress callback
// =========================================================================

TEST_F(ParallelTest, ProgressCallbackFires) {
    std::vector<int> input = {1, 2, 3};
    std::atomic<int> progressCount{0};

    parallel::map(input, [](const int& x) { return x; },
        [&progressCount](uint32_t, uint32_t, const std::string&) {
            progressCount.fetch_add(1);
        });

    EXPECT_EQ(progressCount.load(), 3);
}

TEST_F(ParallelTest, ProgressCallbackReceivesTotal) {
    std::vector<int> input = {1, 2, 3, 4};
    uint32_t observedTotal = 0;

    parallel::map(input, [](const int& x) { return x; },
        [&observedTotal](uint32_t, uint32_t total, const std::string&) {
            observedTotal = total;
        });

    EXPECT_EQ(observedTotal, 4u);
}

// =========================================================================
// ProgressTracker
// =========================================================================

TEST_F(ParallelTest, ProgressTrackerInitialState) {
    parallel::ProgressTracker tracker(10);
    EXPECT_EQ(tracker.completed(), 0u);
    EXPECT_EQ(tracker.total(), 10u);
    EXPECT_FALSE(tracker.isDone());
}

TEST_F(ParallelTest, ProgressTrackerAdvance) {
    parallel::ProgressTracker tracker(3);
    tracker.advance();
    EXPECT_EQ(tracker.completed(), 1u);
    EXPECT_FALSE(tracker.isDone());

    tracker.advance();
    tracker.advance();
    EXPECT_EQ(tracker.completed(), 3u);
    EXPECT_TRUE(tracker.isDone());
}

// =========================================================================
// Utility functions
// =========================================================================

TEST_F(ParallelTest, HasTaskflowReturnsBool) {
    constexpr bool result = parallel::hasTaskflow();
    (void)result;
    SUCCEED();
}

TEST_F(ParallelTest, BackendInfoNonEmpty) {
    auto info = parallel::backendInfo();
    EXPECT_FALSE(info.empty());
    EXPECT_NE(info.find("threads"), std::string::npos);
}

TEST_F(ParallelTest, ConfigThreadCountPositive) {
    EXPECT_GT(parallel::globalConfig.threadCount(), 0u);
}

TEST_F(ParallelTest, ConfigDefaultMinItems) {
    parallel::Config defaultConfig;
    EXPECT_GE(defaultConfig.minItemsForParallel, 1u);
}

} // namespace testing
} // namespace makine
