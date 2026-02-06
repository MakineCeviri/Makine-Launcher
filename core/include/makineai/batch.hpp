/**
 * @file batch.hpp
 * @brief Batch processing utilities for MakineAI
 *
 * Provides:
 * - BatchProcessor: Generic batch operation executor
 * - BatchResult: Detailed results with per-item status
 * - BatchBuilder: Fluent API for building batches
 * - Progress tracking and cancellation support
 *
 * Design principles:
 * - Parallel execution by default (configurable)
 * - Graceful failure handling (continue on error option)
 * - Detailed per-item results
 * - Memory-efficient streaming for large batches
 *
 * Usage:
 * @code
 * // Simple batch processing
 * auto results = BatchProcessor<GameInfo, PatchResult>::create()
 *     .items(games)
 *     .process([](const GameInfo& game) -> Result<PatchResult> {
 *         return patchEngine.apply(game, translations);
 *     })
 *     .onProgress([](const BatchProgress& p) {
 *         std::cout << p.percentComplete() << "%" << std::endl;
 *     })
 *     .parallel(4)
 *     .continueOnError()
 *     .execute();
 *
 * // Check results
 * for (const auto& result : results) {
 *     if (result.success) {
 *         std::cout << result.item.name << ": OK" << std::endl;
 *     } else {
 *         std::cout << result.item.name << ": " << result.error.message() << std::endl;
 *     }
 * }
 * @endcode
 *
 * Copyright (c) 2026 MakineAI Team
 */

#pragma once

#include "makineai/types.hpp"
#include "makineai/error.hpp"
#include "makineai/logging.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace makineai {

// =============================================================================
// BATCH PROGRESS
// =============================================================================

/**
 * @brief Batch operation progress information
 */
struct BatchProgress {
    size_t totalItems = 0;          ///< Total items to process
    size_t completedItems = 0;      ///< Items completed (success + failed)
    size_t successfulItems = 0;     ///< Successfully processed items
    size_t failedItems = 0;         ///< Failed items
    size_t skippedItems = 0;        ///< Skipped items (e.g., due to cancellation)

    std::string currentItem;        ///< Currently processing item (description)
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdateTime;

    /**
     * @brief Get completion percentage
     */
    [[nodiscard]] double percentComplete() const noexcept {
        return totalItems > 0 ? 100.0 * completedItems / totalItems : 0.0;
    }

    /**
     * @brief Get remaining items
     */
    [[nodiscard]] size_t remainingItems() const noexcept {
        return completedItems < totalItems ? totalItems - completedItems : 0;
    }

    /**
     * @brief Get elapsed time
     */
    [[nodiscard]] std::chrono::milliseconds elapsedTime() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            lastUpdateTime - startTime);
    }

    /**
     * @brief Estimate time remaining
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> estimatedTimeRemaining() const {
        if (completedItems == 0) return std::nullopt;

        auto elapsed = elapsedTime();
        auto msPerItem = elapsed.count() / completedItems;
        auto remaining = remainingItems();

        return std::chrono::milliseconds(msPerItem * remaining);
    }

    /**
     * @brief Get items per second throughput
     */
    [[nodiscard]] double throughput() const noexcept {
        auto elapsed = elapsedTime();
        if (elapsed.count() == 0) return 0.0;
        return 1000.0 * completedItems / elapsed.count();
    }

    /**
     * @brief Check if batch is complete
     */
    [[nodiscard]] bool isComplete() const noexcept {
        return completedItems >= totalItems;
    }

    /**
     * @brief Check if all items succeeded
     */
    [[nodiscard]] bool allSucceeded() const noexcept {
        return isComplete() && failedItems == 0 && skippedItems == 0;
    }
};

// =============================================================================
// BATCH ITEM RESULT
// =============================================================================

/**
 * @brief Result for a single batch item
 *
 * @tparam TInput Input item type
 * @tparam TOutput Output/result type
 */
template<typename TInput, typename TOutput>
struct BatchItemResult {
    TInput item;                        ///< The input item
    std::optional<TOutput> output;      ///< Output (if successful)
    std::optional<Error> error;         ///< Error (if failed)
    bool success = false;               ///< Success flag
    bool skipped = false;               ///< Was item skipped?
    std::chrono::milliseconds duration{0};  ///< Processing time

    /**
     * @brief Create success result
     */
    static BatchItemResult Success(TInput item, TOutput output,
                                   std::chrono::milliseconds duration = {}) {
        BatchItemResult result;
        result.item = std::move(item);
        result.output = std::move(output);
        result.success = true;
        result.duration = duration;
        return result;
    }

    /**
     * @brief Create failure result
     */
    static BatchItemResult Failure(TInput item, Error error,
                                   std::chrono::milliseconds duration = {}) {
        BatchItemResult result;
        result.item = std::move(item);
        result.error = std::move(error);
        result.success = false;
        result.duration = duration;
        return result;
    }

    /**
     * @brief Create skipped result
     */
    static BatchItemResult Skipped(TInput item, const std::string& reason = "") {
        BatchItemResult result;
        result.item = std::move(item);
        result.skipped = true;
        result.success = false;
        if (!reason.empty()) {
            result.error = Error(ErrorCode::Cancelled, reason);
        }
        return result;
    }
};

// =============================================================================
// BATCH RESULT
// =============================================================================

/**
 * @brief Complete batch operation result
 *
 * @tparam TInput Input item type
 * @tparam TOutput Output/result type
 */
template<typename TInput, typename TOutput>
struct BatchResult {
    std::vector<BatchItemResult<TInput, TOutput>> items;
    BatchProgress progress;
    bool cancelled = false;

    /**
     * @brief Get all successful results
     */
    [[nodiscard]] std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>>
    successful() const {
        std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>> result;
        for (const auto& item : items) {
            if (item.success) {
                result.push_back(std::cref(item));
            }
        }
        return result;
    }

    /**
     * @brief Get all failed results
     */
    [[nodiscard]] std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>>
    failed() const {
        std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>> result;
        for (const auto& item : items) {
            if (!item.success && !item.skipped) {
                result.push_back(std::cref(item));
            }
        }
        return result;
    }

    /**
     * @brief Get all skipped results
     */
    [[nodiscard]] std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>>
    skipped() const {
        std::vector<std::reference_wrapper<const BatchItemResult<TInput, TOutput>>> result;
        for (const auto& item : items) {
            if (item.skipped) {
                result.push_back(std::cref(item));
            }
        }
        return result;
    }

    /**
     * @brief Get success rate
     */
    [[nodiscard]] double successRate() const noexcept {
        return items.empty() ? 0.0 : 100.0 * progress.successfulItems / items.size();
    }

    /**
     * @brief Check if all succeeded
     */
    [[nodiscard]] bool allSucceeded() const noexcept {
        return progress.allSucceeded();
    }

    /**
     * @brief Get aggregated error (if any failed)
     */
    [[nodiscard]] std::optional<Error> aggregatedError() const {
        if (progress.failedItems == 0) return std::nullopt;

        std::ostringstream oss;
        oss << progress.failedItems << " of " << items.size() << " items failed";

        Error error(ErrorCode::OperationFailed, oss.str());
        for (const auto& item : items) {
            if (item.error) {
                error = error.withDetail("failure", item.error->message());
                break;  // Just include first error in details
            }
        }
        return error;
    }
};

// =============================================================================
// BATCH OPTIONS
// =============================================================================

/**
 * @brief Configuration options for batch processing
 */
struct BatchOptions {
    size_t parallelism = 1;                 ///< Number of parallel workers (1 = sequential)
    bool continueOnError = false;           ///< Continue processing after failures
    bool stopOnCancel = true;               ///< Stop immediately on cancellation
    size_t maxRetries = 0;                  ///< Max retries per item
    std::chrono::milliseconds retryDelay{100};  ///< Delay between retries
    std::chrono::milliseconds timeout{0};   ///< Per-item timeout (0 = no timeout)
    bool preserveOrder = true;              ///< Preserve input order in results
};

// =============================================================================
// BATCH PROCESSOR
// =============================================================================

/**
 * @brief Generic batch processor
 *
 * @tparam TInput Input item type
 * @tparam TOutput Output/result type
 */
template<typename TInput, typename TOutput>
class BatchProcessor {
public:
    using ItemResult = BatchItemResult<TInput, TOutput>;
    using Result = BatchResult<TInput, TOutput>;
    using ProcessFunc = std::function<makineai::Result<TOutput>(const TInput&)>;
    using ProgressCallback = std::function<void(const BatchProgress&)>;
    using ItemCallback = std::function<void(const ItemResult&)>;
    using FilterFunc = std::function<bool(const TInput&)>;
    using DescriptionFunc = std::function<std::string(const TInput&)>;

    /**
     * @brief Create a batch processor builder
     */
    static BatchProcessor create() {
        return BatchProcessor();
    }

    /**
     * @brief Set items to process
     */
    BatchProcessor& items(std::vector<TInput> items) {
        items_ = std::move(items);
        return *this;
    }

    /**
     * @brief Set processing function
     */
    BatchProcessor& process(ProcessFunc func) {
        processFunc_ = std::move(func);
        return *this;
    }

    /**
     * @brief Set progress callback
     */
    BatchProcessor& onProgress(ProgressCallback callback) {
        progressCallback_ = std::move(callback);
        return *this;
    }

    /**
     * @brief Set per-item completion callback
     */
    BatchProcessor& onItemComplete(ItemCallback callback) {
        itemCallback_ = std::move(callback);
        return *this;
    }

    /**
     * @brief Set filter function (skip items that don't pass)
     */
    BatchProcessor& filter(FilterFunc func) {
        filterFunc_ = std::move(func);
        return *this;
    }

    /**
     * @brief Set description function for progress reporting
     */
    BatchProcessor& describe(DescriptionFunc func) {
        descriptionFunc_ = std::move(func);
        return *this;
    }

    /**
     * @brief Set parallelism level
     */
    BatchProcessor& parallel(size_t workers) {
        options_.parallelism = workers;
        return *this;
    }

    /**
     * @brief Enable continue on error
     */
    BatchProcessor& continueOnError(bool enable = true) {
        options_.continueOnError = enable;
        return *this;
    }

    /**
     * @brief Set retry count
     */
    BatchProcessor& retries(size_t count, std::chrono::milliseconds delay = std::chrono::milliseconds{100}) {
        options_.maxRetries = count;
        options_.retryDelay = delay;
        return *this;
    }

    /**
     * @brief Set per-item timeout
     */
    BatchProcessor& timeout(std::chrono::milliseconds ms) {
        options_.timeout = ms;
        return *this;
    }

    /**
     * @brief Set cancellation token
     */
    BatchProcessor& cancellation(std::atomic<bool>* token) {
        cancelToken_ = token;
        return *this;
    }

    /**
     * @brief Execute the batch
     */
    [[nodiscard]] Result execute() {
        if (!processFunc_) {
            throw std::logic_error("BatchProcessor: process function not set");
        }

        Result result;
        result.progress.totalItems = items_.size();
        result.progress.startTime = std::chrono::steady_clock::now();
        result.progress.lastUpdateTime = result.progress.startTime;
        result.items.reserve(items_.size());

        if (items_.empty()) {
            return result;
        }

        // Initialize result items (for order preservation)
        for (size_t i = 0; i < items_.size(); ++i) {
            ItemResult ir;
            ir.item = items_[i];
            result.items.push_back(std::move(ir));
        }

        if (options_.parallelism <= 1) {
            executeSequential(result);
        } else {
            executeParallel(result);
        }

        result.progress.lastUpdateTime = std::chrono::steady_clock::now();
        return result;
    }

private:
    void executeSequential(Result& result) {
        for (size_t i = 0; i < items_.size(); ++i) {
            if (isCancelled()) {
                // Mark remaining as skipped
                for (size_t j = i; j < items_.size(); ++j) {
                    result.items[j] = ItemResult::Skipped(items_[j], "Cancelled");
                    ++result.progress.skippedItems;
                    ++result.progress.completedItems;
                }
                result.cancelled = true;
                break;
            }

            result.items[i] = processItem(items_[i], i, result.progress);

            if (result.items[i].success) {
                ++result.progress.successfulItems;
            } else if (result.items[i].skipped) {
                ++result.progress.skippedItems;
            } else {
                ++result.progress.failedItems;
                if (!options_.continueOnError) {
                    // Mark remaining as skipped
                    for (size_t j = i + 1; j < items_.size(); ++j) {
                        result.items[j] = ItemResult::Skipped(items_[j], "Stopped due to error");
                        ++result.progress.skippedItems;
                        ++result.progress.completedItems;
                    }
                    break;
                }
            }

            ++result.progress.completedItems;
            result.progress.lastUpdateTime = std::chrono::steady_clock::now();
            notifyProgress(result.progress);
        }
    }

    void executeParallel(Result& result) {
        std::mutex resultMutex;
        std::atomic<size_t> nextIndex{0};
        std::atomic<bool> shouldStop{false};

        auto worker = [&]() {
            while (!shouldStop.load() && !isCancelled()) {
                size_t index = nextIndex.fetch_add(1);
                if (index >= items_.size()) break;

                BatchProgress localProgress;
                {
                    std::lock_guard lock(resultMutex);
                    localProgress = result.progress;
                }

                auto itemResult = processItem(items_[index], index, localProgress);

                {
                    std::lock_guard lock(resultMutex);
                    result.items[index] = std::move(itemResult);

                    if (result.items[index].success) {
                        ++result.progress.successfulItems;
                    } else if (result.items[index].skipped) {
                        ++result.progress.skippedItems;
                    } else {
                        ++result.progress.failedItems;
                        if (!options_.continueOnError) {
                            shouldStop.store(true);
                        }
                    }

                    ++result.progress.completedItems;
                    result.progress.lastUpdateTime = std::chrono::steady_clock::now();
                }

                notifyProgress(result.progress);
            }
        };

        // Launch workers
        std::vector<std::thread> workers;
        size_t numWorkers = std::min(options_.parallelism, items_.size());

        for (size_t i = 0; i < numWorkers; ++i) {
            workers.emplace_back(worker);
        }

        // Wait for completion
        for (auto& w : workers) {
            if (w.joinable()) {
                w.join();
            }
        }

        // Handle cancellation
        if (isCancelled() || shouldStop.load()) {
            result.cancelled = isCancelled();
            // Mark unprocessed as skipped
            for (size_t i = 0; i < items_.size(); ++i) {
                if (!result.items[i].success && !result.items[i].error && !result.items[i].skipped) {
                    result.items[i] = ItemResult::Skipped(items_[i],
                        isCancelled() ? "Cancelled" : "Stopped due to error");
                    ++result.progress.skippedItems;
                    ++result.progress.completedItems;
                }
            }
        }
    }

    ItemResult processItem(const TInput& item, size_t index, BatchProgress& progress) {
        // Check filter
        if (filterFunc_ && !filterFunc_(item)) {
            return ItemResult::Skipped(item, "Filtered out");
        }

        // Update current item description
        if (descriptionFunc_) {
            progress.currentItem = descriptionFunc_(item);
        } else {
            progress.currentItem = "Item " + std::to_string(index + 1);
        }

        auto startTime = std::chrono::steady_clock::now();

        // Process with retries
        std::optional<Error> lastError;
        for (size_t attempt = 0; attempt <= options_.maxRetries; ++attempt) {
            if (attempt > 0) {
                std::this_thread::sleep_for(options_.retryDelay);
            }

            try {
                auto result = processFunc_(item);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime);

                if (result) {
                    auto itemResult = ItemResult::Success(item, std::move(*result), duration);
                    if (itemCallback_) {
                        itemCallback_(itemResult);
                    }
                    return itemResult;
                } else {
                    lastError = result.error();
                }
            } catch (const std::exception& e) {
                lastError = Error(ErrorCode::OperationFailed,
                    std::string("Exception: ") + e.what());
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);

        auto itemResult = ItemResult::Failure(item,
            lastError.value_or(Error(ErrorCode::Unknown, "Unknown error")), duration);

        if (itemCallback_) {
            itemCallback_(itemResult);
        }

        return itemResult;
    }

    bool isCancelled() const {
        return cancelToken_ && cancelToken_->load();
    }

    void notifyProgress(const BatchProgress& progress) {
        if (progressCallback_) {
            try {
                progressCallback_(progress);
            } catch (...) {
                // Don't let callback errors affect processing
            }
        }
    }

    std::vector<TInput> items_;
    ProcessFunc processFunc_;
    ProgressCallback progressCallback_;
    ItemCallback itemCallback_;
    FilterFunc filterFunc_;
    DescriptionFunc descriptionFunc_;
    BatchOptions options_;
    std::atomic<bool>* cancelToken_ = nullptr;
};

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Process items in batch (simple API)
 *
 * @tparam TInput Input type
 * @tparam TOutput Output type
 * @param items Items to process
 * @param func Processing function
 * @param parallelism Number of parallel workers
 * @return Batch result
 */
template<typename TInput, typename TOutput>
BatchResult<TInput, TOutput> processBatch(
    const std::vector<TInput>& items,
    std::function<Result<TOutput>(const TInput&)> func,
    size_t parallelism = 1)
{
    return BatchProcessor<TInput, TOutput>::create()
        .items(std::vector<TInput>(items))
        .process(std::move(func))
        .parallel(parallelism)
        .continueOnError()
        .execute();
}

/**
 * @brief Map function over items (like std::transform but with error handling)
 */
template<typename TInput, typename TOutput>
std::vector<TOutput> batchMap(
    const std::vector<TInput>& items,
    std::function<TOutput(const TInput&)> func)
{
    auto result = BatchProcessor<TInput, TOutput>::create()
        .items(std::vector<TInput>(items))
        .process([&func](const TInput& item) -> Result<TOutput> {
            return func(item);
        })
        .continueOnError()
        .execute();

    std::vector<TOutput> outputs;
    outputs.reserve(result.progress.successfulItems);

    for (const auto& item : result.items) {
        if (item.success && item.output) {
            outputs.push_back(*item.output);
        }
    }

    return outputs;
}

/**
 * @brief Filter items that match predicate
 */
template<typename T>
std::vector<T> batchFilter(
    const std::vector<T>& items,
    std::function<bool(const T&)> predicate)
{
    std::vector<T> result;
    result.reserve(items.size());

    for (const auto& item : items) {
        if (predicate(item)) {
            result.push_back(item);
        }
    }

    return result;
}

} // namespace makineai
