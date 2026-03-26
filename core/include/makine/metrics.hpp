#pragma once

/**
 * @file metrics.hpp
 * @brief Performance metrics collection for Makine
 *
 * Provides:
 * - Duration metrics (operation timing)
 * - Counter metrics (event counting)
 * - Gauge metrics (current values)
 * - Histogram metrics (distribution)
 *
 * Thread-safe, low-overhead metrics collection.
 *
 * Usage:
 * @code
 * // Record operation duration
 * {
 *     auto timer = Metrics::instance().timer("game_scan");
 *     // ... scan operation
 * } // Automatically records duration
 *
 * // Manual recording
 * Metrics::instance().recordDuration("json_parse", 45ms);
 * Metrics::instance().increment("games_found");
 * Metrics::instance().gauge("cache_size", cache.size());
 *
 * // Export metrics
 * std::cout << Metrics::instance().toJson();
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace makine {

/**
 * @brief Single metric data point
 */
struct MetricValue {
    int64_t count = 0;          ///< Number of observations
    int64_t total = 0;          ///< Sum of values
    int64_t min = INT64_MAX;    ///< Minimum value
    int64_t max = INT64_MIN;    ///< Maximum value
    double sumSquares = 0.0;    ///< Sum of squared values (for stddev)

    [[nodiscard]] double average() const noexcept {
        return count > 0 ? static_cast<double>(total) / count : 0.0;
    }

    [[nodiscard]] double stddev() const noexcept {
        if (count < 2) return 0.0;
        double avg = average();
        double variance = (sumSquares / count) - (avg * avg);
        return variance > 0 ? std::sqrt(variance) : 0.0;
    }

    void record(int64_t value) {
        ++count;
        total += value;
        min = std::min(min, value);
        max = std::max(max, value);
        sumSquares += static_cast<double>(value) * value;
    }

    void reset() {
        count = 0;
        total = 0;
        min = INT64_MAX;
        max = INT64_MIN;
        sumSquares = 0.0;
    }
};

/**
 * @brief RAII timer for automatic duration recording
 */
class ScopedMetricTimer {
public:
    using Clock = std::chrono::steady_clock;

    ScopedMetricTimer(std::string name, std::function<void(const std::string&, std::chrono::milliseconds)> recorder)
        : name_(std::move(name))
        , recorder_(std::move(recorder))
        , start_(Clock::now())
    {}

    ~ScopedMetricTimer() {
        if (recorder_) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - start_);
            recorder_(name_, duration);
        }
    }

    // Non-copyable, non-movable
    ScopedMetricTimer(const ScopedMetricTimer&) = delete;
    ScopedMetricTimer& operator=(const ScopedMetricTimer&) = delete;

    /**
     * @brief Get elapsed time so far
     */
    [[nodiscard]] std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start_);
    }

    /**
     * @brief Cancel recording (won't call recorder on destruction)
     */
    void cancel() {
        recorder_ = nullptr;
    }

private:
    std::string name_;
    std::function<void(const std::string&, std::chrono::milliseconds)> recorder_;
    Clock::time_point start_;
};

/**
 * @brief Thread-safe metrics collection singleton
 */
class Metrics {
public:
    /**
     * @brief Get singleton instance
     */
    static Metrics& instance() {
        static Metrics metrics;
        return metrics;
    }

    // ==========================================================================
    // DURATION METRICS
    // ==========================================================================

    /**
     * @brief Record a duration measurement
     * @param name Metric name
     * @param duration Duration in milliseconds
     */
    void recordDuration(const std::string& name, std::chrono::milliseconds duration) {
        std::unique_lock lock(mutex_);
        durations_[name].record(duration.count());
    }

    /**
     * @brief Create a scoped timer that records on destruction
     * @param name Metric name
     * @return RAII timer object
     */
    [[nodiscard]] ScopedMetricTimer timer(const std::string& name) {
        return ScopedMetricTimer(name, [this](const std::string& n, auto d) {
            recordDuration(n, d);
        });
    }

    /**
     * @brief Get duration metric
     */
    [[nodiscard]] MetricValue getDuration(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = durations_.find(name);
        return it != durations_.end() ? it->second : MetricValue{};
    }

    // ==========================================================================
    // COUNTER METRICS
    // ==========================================================================

    /**
     * @brief Increment a counter
     * @param name Counter name
     * @param delta Amount to add (default 1)
     */
    void increment(const std::string& name, int64_t delta = 1) {
        std::unique_lock lock(mutex_);
        counters_[name] += delta;
    }

    /**
     * @brief Decrement a counter
     */
    void decrement(const std::string& name, int64_t delta = 1) {
        increment(name, -delta);
    }

    /**
     * @brief Get counter value
     */
    [[nodiscard]] int64_t getCounter(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = counters_.find(name);
        return it != counters_.end() ? it->second : 0;
    }

    // ==========================================================================
    // GAUGE METRICS
    // ==========================================================================

    /**
     * @brief Set a gauge value (current state)
     * @param name Gauge name
     * @param value Current value
     */
    void gauge(const std::string& name, double value) {
        std::unique_lock lock(mutex_);
        gauges_[name] = value;
    }

    /**
     * @brief Get gauge value
     */
    [[nodiscard]] double getGauge(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = gauges_.find(name);
        return it != gauges_.end() ? it->second : 0.0;
    }

    // ==========================================================================
    // HISTOGRAM METRICS
    // ==========================================================================

    /**
     * @brief Record a value in a histogram
     */
    void recordHistogram(const std::string& name, int64_t value) {
        std::unique_lock lock(mutex_);
        histograms_[name].record(value);
    }

    /**
     * @brief Get histogram data
     */
    [[nodiscard]] MetricValue getHistogram(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = histograms_.find(name);
        return it != histograms_.end() ? it->second : MetricValue{};
    }

    // ==========================================================================
    // EXPORT & MANAGEMENT
    // ==========================================================================

    /**
     * @brief Export all metrics as JSON
     */
    [[nodiscard]] std::string toJson() const {
        std::shared_lock lock(mutex_);
        std::ostringstream oss;

        oss << "{\n";

        // Durations
        oss << "  \"durations\": {\n";
        bool first = true;
        for (const auto& [name, metric] : durations_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": {"
                << "\"count\": " << metric.count << ", "
                << "\"avg_ms\": " << metric.average() << ", "
                << "\"min_ms\": " << (metric.min == INT64_MAX ? 0 : metric.min) << ", "
                << "\"max_ms\": " << (metric.max == INT64_MIN ? 0 : metric.max) << ", "
                << "\"stddev_ms\": " << metric.stddev()
                << "}";
        }
        oss << "\n  },\n";

        // Counters
        oss << "  \"counters\": {\n";
        first = true;
        for (const auto& [name, value] : counters_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": " << value;
        }
        oss << "\n  },\n";

        // Gauges
        oss << "  \"gauges\": {\n";
        first = true;
        for (const auto& [name, value] : gauges_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": " << value;
        }
        oss << "\n  },\n";

        // Histograms
        oss << "  \"histograms\": {\n";
        first = true;
        for (const auto& [name, metric] : histograms_) {
            if (!first) oss << ",\n";
            first = false;
            oss << "    \"" << name << "\": {"
                << "\"count\": " << metric.count << ", "
                << "\"avg\": " << metric.average() << ", "
                << "\"min\": " << (metric.min == INT64_MAX ? 0 : metric.min) << ", "
                << "\"max\": " << (metric.max == INT64_MIN ? 0 : metric.max) << ", "
                << "\"stddev\": " << metric.stddev()
                << "}";
        }
        oss << "\n  }\n";

        oss << "}\n";

        return oss.str();
    }

    /**
     * @brief Export metrics summary as human-readable text
     */
    [[nodiscard]] std::string toText() const {
        std::shared_lock lock(mutex_);
        std::ostringstream oss;

        oss << "=== Makine Metrics ===\n\n";

        if (!durations_.empty()) {
            oss << "Durations:\n";
            for (const auto& [name, m] : durations_) {
                oss << "  " << name << ": "
                    << m.count << " calls, "
                    << "avg=" << m.average() << "ms, "
                    << "min=" << (m.min == INT64_MAX ? 0 : m.min) << "ms, "
                    << "max=" << (m.max == INT64_MIN ? 0 : m.max) << "ms\n";
            }
            oss << "\n";
        }

        if (!counters_.empty()) {
            oss << "Counters:\n";
            for (const auto& [name, value] : counters_) {
                oss << "  " << name << ": " << value << "\n";
            }
            oss << "\n";
        }

        if (!gauges_.empty()) {
            oss << "Gauges:\n";
            for (const auto& [name, value] : gauges_) {
                oss << "  " << name << ": " << value << "\n";
            }
            oss << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Reset all metrics
     */
    void reset() {
        std::unique_lock lock(mutex_);
        durations_.clear();
        counters_.clear();
        gauges_.clear();
        histograms_.clear();
    }

    /**
     * @brief Reset specific metric category
     */
    void resetDurations() {
        std::unique_lock lock(mutex_);
        durations_.clear();
    }

    void resetCounters() {
        std::unique_lock lock(mutex_);
        counters_.clear();
    }

    void resetGauges() {
        std::unique_lock lock(mutex_);
        gauges_.clear();
    }

private:
    Metrics() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, MetricValue> durations_;
    std::unordered_map<std::string, int64_t> counters_;
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, MetricValue> histograms_;
};

/**
 * @brief Convenience function to access global metrics
 */
inline Metrics& metrics() {
    return Metrics::instance();
}

/**
 * @brief Macro for timing a scope
 */
#define MAKINE_METRIC_TIMER(name) \
    auto _metric_timer_##__LINE__ = ::makine::metrics().timer(name)

/**
 * @brief Macro for timing a function
 */
#define MAKINE_METRIC_FUNCTION() \
    MAKINE_METRIC_TIMER(__FUNCTION__)

} // namespace makine
