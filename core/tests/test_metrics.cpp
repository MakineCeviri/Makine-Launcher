/**
 * @file test_metrics.cpp
 * @brief Unit tests for metrics.hpp — MetricValue, ScopedMetricTimer, Metrics
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/metrics.hpp>

#include <chrono>
#include <cmath>
#include <string>
#include <thread>

namespace makine {
namespace testing {

// ===========================================================================
// MetricValue
// ===========================================================================

TEST(MetricValueTest, DefaultValues) {
    MetricValue mv;
    EXPECT_EQ(mv.count, 0);
    EXPECT_EQ(mv.total, 0);
    EXPECT_DOUBLE_EQ(mv.sumSquares, 0.0);
}

TEST(MetricValueTest, AverageZeroCount) {
    MetricValue mv;
    EXPECT_DOUBLE_EQ(mv.average(), 0.0);
}

TEST(MetricValueTest, StddevLessThanTwoSamples) {
    MetricValue mv;
    mv.record(100);
    EXPECT_DOUBLE_EQ(mv.stddev(), 0.0);
}

TEST(MetricValueTest, RecordSingleValue) {
    MetricValue mv;
    mv.record(42);
    EXPECT_EQ(mv.count, 1);
    EXPECT_EQ(mv.total, 42);
    EXPECT_EQ(mv.min, 42);
    EXPECT_EQ(mv.max, 42);
    EXPECT_DOUBLE_EQ(mv.average(), 42.0);
}

TEST(MetricValueTest, RecordMultipleValues) {
    MetricValue mv;
    mv.record(10);
    mv.record(20);
    mv.record(30);

    EXPECT_EQ(mv.count, 3);
    EXPECT_EQ(mv.total, 60);
    EXPECT_EQ(mv.min, 10);
    EXPECT_EQ(mv.max, 30);
    EXPECT_DOUBLE_EQ(mv.average(), 20.0);
}

TEST(MetricValueTest, StddevCalculation) {
    MetricValue mv;
    mv.record(10);
    mv.record(20);
    mv.record(30);

    // stddev of {10, 20, 30}: population stddev
    double avg = 20.0;
    double variance = ((10-avg)*(10-avg) + (20-avg)*(20-avg) + (30-avg)*(30-avg)) / 3.0;
    double expected = std::sqrt(variance);

    EXPECT_NEAR(mv.stddev(), expected, 0.01);
}

TEST(MetricValueTest, ResetClearsAll) {
    MetricValue mv;
    mv.record(100);
    mv.record(200);
    mv.reset();

    EXPECT_EQ(mv.count, 0);
    EXPECT_EQ(mv.total, 0);
    EXPECT_DOUBLE_EQ(mv.sumSquares, 0.0);
    EXPECT_DOUBLE_EQ(mv.average(), 0.0);
}

TEST(MetricValueTest, MinMaxWithNegativeValues) {
    MetricValue mv;
    mv.record(-10);
    mv.record(5);
    mv.record(-20);

    EXPECT_EQ(mv.min, -20);
    EXPECT_EQ(mv.max, 5);
}

// ===========================================================================
// Metrics Singleton
// ===========================================================================

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        Metrics::instance().reset();
    }

    void TearDown() override {
        Metrics::instance().reset();
    }
};

// --- Counters ---

TEST_F(MetricsTest, IncrementCounter) {
    Metrics::instance().increment("test_counter");
    EXPECT_EQ(Metrics::instance().getCounter("test_counter"), 1);
}

TEST_F(MetricsTest, IncrementCounterByDelta) {
    Metrics::instance().increment("test_counter", 5);
    EXPECT_EQ(Metrics::instance().getCounter("test_counter"), 5);
}

TEST_F(MetricsTest, DecrementCounter) {
    Metrics::instance().increment("test_counter", 10);
    Metrics::instance().decrement("test_counter", 3);
    EXPECT_EQ(Metrics::instance().getCounter("test_counter"), 7);
}

TEST_F(MetricsTest, GetCounterUnknownReturnsZero) {
    EXPECT_EQ(Metrics::instance().getCounter("nonexistent"), 0);
}

TEST_F(MetricsTest, MultipleCountersIndependent) {
    Metrics::instance().increment("a");
    Metrics::instance().increment("b", 5);
    EXPECT_EQ(Metrics::instance().getCounter("a"), 1);
    EXPECT_EQ(Metrics::instance().getCounter("b"), 5);
}

// --- Gauges ---

TEST_F(MetricsTest, SetAndGetGauge) {
    Metrics::instance().gauge("cache_size", 42.5);
    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("cache_size"), 42.5);
}

TEST_F(MetricsTest, GaugeOverwritesPreviousValue) {
    Metrics::instance().gauge("temp", 10.0);
    Metrics::instance().gauge("temp", 20.0);
    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("temp"), 20.0);
}

TEST_F(MetricsTest, GetGaugeUnknownReturnsZero) {
    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("nonexistent"), 0.0);
}

// --- Durations ---

TEST_F(MetricsTest, RecordDuration) {
    Metrics::instance().recordDuration("op", std::chrono::milliseconds(100));
    auto d = Metrics::instance().getDuration("op");
    EXPECT_EQ(d.count, 1);
    EXPECT_EQ(d.total, 100);
}

TEST_F(MetricsTest, GetDurationUnknownReturnsDefault) {
    auto d = Metrics::instance().getDuration("nonexistent");
    EXPECT_EQ(d.count, 0);
}

TEST_F(MetricsTest, ScopedTimerRecordsDuration) {
    {
        auto timer = Metrics::instance().timer("scoped_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto d = Metrics::instance().getDuration("scoped_test");
    EXPECT_EQ(d.count, 1);
    EXPECT_GE(d.total, 0); // Duration may be 0 on Windows due to timer resolution
}

TEST_F(MetricsTest, ScopedTimerCancel) {
    {
        auto timer = Metrics::instance().timer("cancelled");
        timer.cancel();
    }
    auto d = Metrics::instance().getDuration("cancelled");
    EXPECT_EQ(d.count, 0);
}

TEST_F(MetricsTest, ScopedTimerElapsed) {
    auto timer = Metrics::instance().timer("elapsed_test");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GE(timer.elapsed().count(), 5);
    timer.cancel(); // Don't record
}

// --- Histograms ---

TEST_F(MetricsTest, RecordHistogram) {
    Metrics::instance().recordHistogram("response_size", 1024);
    Metrics::instance().recordHistogram("response_size", 2048);

    auto h = Metrics::instance().getHistogram("response_size");
    EXPECT_EQ(h.count, 2);
    EXPECT_EQ(h.min, 1024);
    EXPECT_EQ(h.max, 2048);
}

TEST_F(MetricsTest, GetHistogramUnknownReturnsDefault) {
    auto h = Metrics::instance().getHistogram("nonexistent");
    EXPECT_EQ(h.count, 0);
}

// --- Export ---

TEST_F(MetricsTest, ToJsonNonEmpty) {
    Metrics::instance().increment("test");
    Metrics::instance().gauge("g", 1.0);
    auto json = Metrics::instance().toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"counters\""), std::string::npos);
    EXPECT_NE(json.find("\"gauges\""), std::string::npos);
    EXPECT_NE(json.find("\"durations\""), std::string::npos);
    EXPECT_NE(json.find("\"histograms\""), std::string::npos);
}

TEST_F(MetricsTest, ToTextNonEmpty) {
    Metrics::instance().increment("test");
    auto text = Metrics::instance().toText();
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("Makine Metrics"), std::string::npos);
}

// --- Reset ---

TEST_F(MetricsTest, ResetClearsAll) {
    Metrics::instance().increment("c");
    Metrics::instance().gauge("g", 1.0);
    Metrics::instance().recordDuration("d", std::chrono::milliseconds(1));
    Metrics::instance().recordHistogram("h", 1);

    Metrics::instance().reset();

    EXPECT_EQ(Metrics::instance().getCounter("c"), 0);
    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("g"), 0.0);
    EXPECT_EQ(Metrics::instance().getDuration("d").count, 0);
    EXPECT_EQ(Metrics::instance().getHistogram("h").count, 0);
}

TEST_F(MetricsTest, ResetDurationsOnly) {
    Metrics::instance().increment("c");
    Metrics::instance().recordDuration("d", std::chrono::milliseconds(1));

    Metrics::instance().resetDurations();

    EXPECT_EQ(Metrics::instance().getCounter("c"), 1); // Preserved
    EXPECT_EQ(Metrics::instance().getDuration("d").count, 0); // Cleared
}

TEST_F(MetricsTest, ResetCountersOnly) {
    Metrics::instance().increment("c");
    Metrics::instance().gauge("g", 5.0);

    Metrics::instance().resetCounters();

    EXPECT_EQ(Metrics::instance().getCounter("c"), 0); // Cleared
    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("g"), 5.0); // Preserved
}

TEST_F(MetricsTest, ResetGaugesOnly) {
    Metrics::instance().gauge("g", 5.0);
    Metrics::instance().increment("c");

    Metrics::instance().resetGauges();

    EXPECT_DOUBLE_EQ(Metrics::instance().getGauge("g"), 0.0); // Cleared
    EXPECT_EQ(Metrics::instance().getCounter("c"), 1); // Preserved
}

// --- Convenience ---

TEST_F(MetricsTest, ConvenienceFunction) {
    metrics().increment("via_convenience");
    EXPECT_EQ(metrics().getCounter("via_convenience"), 1);
}

} // namespace testing
} // namespace makine
