/**
 * @file parallel.hpp
 * @brief Parallel execution utilities with Taskflow integration
 * @copyright (c) 2026 MakineAI Team
 *
 * Provides a unified API for parallel task execution.
 * Uses Taskflow when available, falls back to std::async otherwise.
 *
 * Usage:
 *   auto results = parallel::map(items, [](const auto& item) {
 *       return process(item);
 *   });
 */

#pragma once

#include "makineai/features.hpp"
#include "makineai/logging.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef MAKINEAI_HAS_TASKFLOW
#include <taskflow/taskflow.hpp>
#endif

namespace makineai::parallel {

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * @brief Configuration for parallel execution
 */
struct Config {
    /// Maximum number of worker threads (0 = auto-detect)
    uint32_t maxThreads = 0;

    /// Minimum items per thread before parallelization kicks in
    uint32_t minItemsForParallel = 2;

    /// Enable progress callbacks (may add overhead)
    bool enableProgress = true;

    /// Get optimal thread count
    [[nodiscard]] uint32_t threadCount() const {
        if (maxThreads > 0) {
            return maxThreads;
        }
        return std::max(1u, std::thread::hardware_concurrency());
    }
};

/// Global configuration (can be modified before use)
inline Config globalConfig;

// =============================================================================
// PROGRESS TRACKING
// =============================================================================

/**
 * @brief Thread-safe progress tracker
 */
class ProgressTracker {
public:
    using Callback = std::function<void(uint32_t current, uint32_t total, const std::string& message)>;

    explicit ProgressTracker(uint32_t total, Callback callback = nullptr)
        : total_(total), callback_(std::move(callback)) {}

    void advance(const std::string& message = "") {
        uint32_t current = ++completed_;
        if (callback_) {
            std::lock_guard lock(mutex_);
            callback_(current, total_, message);
        }
    }

    void setMessage(const std::string& message) {
        if (callback_) {
            std::lock_guard lock(mutex_);
            callback_(completed_.load(), total_, message);
        }
    }

    [[nodiscard]] uint32_t completed() const { return completed_.load(); }
    [[nodiscard]] uint32_t total() const { return total_; }
    [[nodiscard]] bool isDone() const { return completed_.load() >= total_; }

private:
    uint32_t total_;
    std::atomic<uint32_t> completed_{0};
    Callback callback_;
    std::mutex mutex_;
};

// =============================================================================
// PARALLEL EXECUTION - TASKFLOW IMPLEMENTATION
// =============================================================================

#ifdef MAKINEAI_HAS_TASKFLOW

namespace detail {

/**
 * @brief Shared Taskflow executor (thread-safe singleton)
 */
inline tf::Executor& executor() {
    static tf::Executor exec(globalConfig.threadCount());
    return exec;
}

}  // namespace detail

/**
 * @brief Execute tasks in parallel and collect results
 *
 * @tparam T Input type
 * @tparam Func Function type (T -> R)
 * @param items Items to process
 * @param func Function to apply to each item
 * @param progress Optional progress callback
 * @return Vector of results in same order as input
 */
template <typename T, typename Func>
auto map(const std::vector<T>& items, Func&& func,
         typename ProgressTracker::Callback progress = nullptr)
    -> std::vector<std::invoke_result_t<Func, const T&>> {

    using ResultType = std::invoke_result_t<Func, const T&>;

    if (items.empty()) {
        return {};
    }

    // Skip parallelization for small workloads
    if (items.size() < globalConfig.minItemsForParallel) {
        std::vector<ResultType> results;
        results.reserve(items.size());
        ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);

        for (const auto& item : items) {
            results.push_back(func(item));
            tracker.advance();
        }
        return results;
    }

    MAKINEAI_LOG_DEBUG(log::CORE, "Parallel map with Taskflow: {} items, {} threads",
                       items.size(), globalConfig.threadCount());

    std::vector<ResultType> results(items.size());
    ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);

    tf::Taskflow taskflow;

    for (size_t i = 0; i < items.size(); ++i) {
        taskflow.emplace([&, idx = i]() {
            results[idx] = func(items[idx]);
            tracker.advance();
        });
    }

    detail::executor().run(taskflow).wait();

    return results;
}

/**
 * @brief Execute tasks in parallel, merging results into single vector
 *
 * @tparam T Input type
 * @tparam Func Function type (T -> std::vector<R>)
 * @param items Items to process
 * @param func Function returning vector for each item
 * @param progress Optional progress callback
 * @return Merged vector of all results
 */
template <typename T, typename Func>
auto flatMap(const std::vector<T>& items, Func&& func,
             typename ProgressTracker::Callback progress = nullptr)
    -> typename std::invoke_result_t<Func, const T&> {

    using ResultType = typename std::invoke_result_t<Func, const T&>;
    using ElementType = typename ResultType::value_type;

    if (items.empty()) {
        return {};
    }

    // Execute in parallel
    std::vector<ResultType> partialResults(items.size());
    ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);
    std::mutex mergeMutex;

    tf::Taskflow taskflow;

    for (size_t i = 0; i < items.size(); ++i) {
        taskflow.emplace([&, idx = i]() {
            partialResults[idx] = func(items[idx]);
            tracker.advance();
        });
    }

    detail::executor().run(taskflow).wait();

    // Merge results
    ResultType merged;
    size_t totalSize = 0;
    for (const auto& partial : partialResults) {
        totalSize += partial.size();
    }
    merged.reserve(totalSize);

    for (auto& partial : partialResults) {
        for (auto& elem : partial) {
            merged.push_back(std::move(elem));
        }
    }

    return merged;
}

/**
 * @brief Execute void tasks in parallel
 *
 * @tparam T Input type
 * @tparam Func Function type (T -> void)
 * @param items Items to process
 * @param func Function to apply to each item
 * @param progress Optional progress callback
 */
template <typename T, typename Func>
void forEach(const std::vector<T>& items, Func&& func,
             typename ProgressTracker::Callback progress = nullptr) {

    if (items.empty()) {
        return;
    }

    if (items.size() < globalConfig.minItemsForParallel) {
        ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);
        for (const auto& item : items) {
            func(item);
            tracker.advance();
        }
        return;
    }

    MAKINEAI_LOG_DEBUG(log::CORE, "Parallel forEach with Taskflow: {} items", items.size());

    ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);
    tf::Taskflow taskflow;

    for (const auto& item : items) {
        taskflow.emplace([&, &itemRef = item]() {
            func(itemRef);
            tracker.advance();
        });
    }

    detail::executor().run(taskflow).wait();
}

#else  // !MAKINEAI_HAS_TASKFLOW

// =============================================================================
// PARALLEL EXECUTION - STD::ASYNC FALLBACK
// =============================================================================

/**
 * @brief Execute tasks in parallel using std::async (fallback)
 */
template <typename T, typename Func>
auto map(const std::vector<T>& items, Func&& func,
         typename ProgressTracker::Callback progress = nullptr)
    -> std::vector<std::invoke_result_t<Func, const T&>> {

    using ResultType = std::invoke_result_t<Func, const T&>;

    if (items.empty()) {
        return {};
    }

    // For small workloads, run sequentially
    if (items.size() < globalConfig.minItemsForParallel) {
        std::vector<ResultType> results;
        results.reserve(items.size());
        ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);

        for (const auto& item : items) {
            results.push_back(func(item));
            tracker.advance();
        }
        return results;
    }

    MAKINEAI_LOG_DEBUG(log::CORE, "Parallel map with std::async: {} items", items.size());

    std::vector<std::future<ResultType>> futures;
    futures.reserve(items.size());

    ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);

    for (const auto& item : items) {
        futures.push_back(std::async(std::launch::async, [&, &itemRef = item]() {
            auto result = func(itemRef);
            tracker.advance();
            return result;
        }));
    }

    std::vector<ResultType> results;
    results.reserve(items.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

/**
 * @brief Execute tasks in parallel, merging results (fallback)
 */
template <typename T, typename Func>
auto flatMap(const std::vector<T>& items, Func&& func,
             typename ProgressTracker::Callback progress = nullptr)
    -> typename std::invoke_result_t<Func, const T&> {

    using ResultType = typename std::invoke_result_t<Func, const T&>;

    if (items.empty()) {
        return {};
    }

    auto partialResults = map(items, std::forward<Func>(func), progress);

    // Merge
    ResultType merged;
    size_t totalSize = 0;
    for (const auto& partial : partialResults) {
        totalSize += partial.size();
    }
    merged.reserve(totalSize);

    for (auto& partial : partialResults) {
        for (auto& elem : partial) {
            merged.push_back(std::move(elem));
        }
    }

    return merged;
}

/**
 * @brief Execute void tasks in parallel (fallback)
 */
template <typename T, typename Func>
void forEach(const std::vector<T>& items, Func&& func,
             typename ProgressTracker::Callback progress = nullptr) {

    if (items.empty()) {
        return;
    }

    if (items.size() < globalConfig.minItemsForParallel) {
        ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);
        for (const auto& item : items) {
            func(item);
            tracker.advance();
        }
        return;
    }

    MAKINEAI_LOG_DEBUG(log::CORE, "Parallel forEach with std::async: {} items", items.size());

    std::vector<std::future<void>> futures;
    futures.reserve(items.size());

    ProgressTracker tracker(static_cast<uint32_t>(items.size()), progress);

    for (const auto& item : items) {
        futures.push_back(std::async(std::launch::async, [&, &itemRef = item]() {
            func(itemRef);
            tracker.advance();
        }));
    }

    for (auto& future : futures) {
        future.get();
    }
}

#endif  // MAKINEAI_HAS_TASKFLOW

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Check if parallel execution is using Taskflow
 */
[[nodiscard]] inline constexpr bool hasTaskflow() {
#ifdef MAKINEAI_HAS_TASKFLOW
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get information about parallel execution backend
 */
[[nodiscard]] inline std::string backendInfo() {
#ifdef MAKINEAI_HAS_TASKFLOW
    return "Taskflow (threads: " + std::to_string(globalConfig.threadCount()) + ")";
#else
    return "std::async fallback (threads: " + std::to_string(globalConfig.threadCount()) + ")";
#endif
}

}  // namespace makineai::parallel
