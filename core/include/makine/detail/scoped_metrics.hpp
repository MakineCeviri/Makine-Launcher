#pragma once

/**
 * @file scoped_metrics.hpp
 * @brief RAII helper to reduce Metrics boilerplate in database/patch code
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Bundles timer + total counter + error counter into a single RAII object.
 * On destruction, increments the total counter and records duration.
 * If markError() was called, also increments the error counter.
 */

#include <string>
#include "makine/metrics.hpp"

namespace makine {

/**
 * @brief RAII metrics scope: starts timer, counts operations and errors
 *
 * Usage:
 * @code
 * Result<void> Database::execute(const std::string& sql) {
 *     ScopedMetrics m("db_query", "db_queries_total", "db_query_errors");
 *     // ... do work ...
 *     if (error) {
 *         m.markError();
 *         return std::unexpected(...);
 *     }
 *     return {};
 * }
 * @endcode
 */
class ScopedMetrics {
public:
    /**
     * @brief Construct with timer, total counter, and error counter names
     * @param timerName    Duration metric name (recorded on destruction)
     * @param totalName    Counter incremented on destruction (every call)
     * @param errorName    Counter incremented only if markError() was called
     */
    ScopedMetrics(std::string timerName,
                  std::string totalName,
                  std::string errorName)
        : timer_(Metrics::instance().timer(timerName))
        , totalName_(std::move(totalName))
        , errorName_(std::move(errorName))
    {}

    /**
     * @brief Construct with timer name only (no counters)
     * @param timerName Duration metric name (recorded on destruction)
     */
    explicit ScopedMetrics(std::string timerName)
        : timer_(Metrics::instance().timer(timerName))
    {}

    ~ScopedMetrics() {
        if (!totalName_.empty()) {
            Metrics::instance().increment(totalName_);
        }
        if (hasError_ && !errorName_.empty()) {
            Metrics::instance().increment(errorName_);
        }
    }

    // Non-copyable, non-movable
    ScopedMetrics(const ScopedMetrics&) = delete;
    ScopedMetrics& operator=(const ScopedMetrics&) = delete;

    /// Mark this scope as having encountered an error
    void markError() noexcept { hasError_ = true; }

    /// Get elapsed time since construction
    [[nodiscard]] auto elapsed() const { return timer_.elapsed(); }

private:
    ScopedMetricTimer timer_;
    std::string totalName_;
    std::string errorName_;
    bool hasError_ = false;
};

} // namespace makine
