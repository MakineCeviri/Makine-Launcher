#pragma once

/**
 * @file async.hpp
 * @brief Asynchronous operation support for Makine
 *
 * Provides:
 * - AsyncOperation: Trackable async task with progress & cancellation
 * - AsyncResult: Type-safe result container
 * - AsyncQueue: Background task queue
 *
 * Usage:
 * @code
 * // Start async scan
 * auto op = Core::instance().scanGamesAsync();
 *
 * // Monitor progress
 * op.onProgress([](float progress, const std::string& message) {
 *     updateProgressBar(progress);
 *     updateStatusText(message);
 * });
 *
 * // Handle completion
 * op.onComplete([](const std::vector<GameInfo>& games) {
 *     displayGames(games);
 * });
 *
 * // Or wait synchronously
 * auto games = op.await();
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include "makine/types.hpp"
#include "makine/error.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <variant>

namespace makine {

/**
 * @brief Status of an async operation
 */
enum class AsyncStatus {
    Pending,    ///< Operation not yet started
    Running,    ///< Operation in progress
    Completed,  ///< Operation completed successfully
    Failed,     ///< Operation failed with error
    Cancelled   ///< Operation was cancelled
};

/**
 * @brief Convert AsyncStatus to string
 */
constexpr std::string_view asyncStatusToString(AsyncStatus status) noexcept {
    switch (status) {
        case AsyncStatus::Pending:   return "Pending";
        case AsyncStatus::Running:   return "Running";
        case AsyncStatus::Completed: return "Completed";
        case AsyncStatus::Failed:    return "Failed";
        case AsyncStatus::Cancelled: return "Cancelled";
        default:                     return "Unknown";
    }
}

/**
 * @brief Progress information for async operations
 */
struct AsyncProgress {
    float percentage = 0.0f;          ///< Progress 0.0 - 1.0
    std::string message;              ///< Current status message
    uint32_t currentStep = 0;         ///< Current step number
    uint32_t totalSteps = 0;          ///< Total steps (0 = indeterminate)
    std::chrono::milliseconds elapsed{0}; ///< Time elapsed
    std::chrono::milliseconds estimatedRemaining{0}; ///< ETA (0 = unknown)

    [[nodiscard]] bool isIndeterminate() const noexcept {
        return totalSteps == 0;
    }

    [[nodiscard]] std::string percentageString() const {
        return std::to_string(static_cast<int>(percentage * 100)) + "%";
    }
};

// Forward declarations
template<typename T> class AsyncOperation;
class AsyncOperationBase;

/**
 * @brief Type-erased base class for async operations
 *
 * Allows storing different AsyncOperation<T> types in containers.
 */
class AsyncOperationBase {
public:
    virtual ~AsyncOperationBase() = default;

    [[nodiscard]] virtual AsyncStatus status() const noexcept = 0;
    [[nodiscard]] virtual AsyncProgress progress() const = 0;
    [[nodiscard]] virtual bool isComplete() const noexcept = 0;
    [[nodiscard]] virtual bool isCancelled() const noexcept = 0;
    [[nodiscard]] virtual std::optional<Error> error() const = 0;

    virtual void cancel() = 0;
    virtual void wait() = 0;
    virtual bool waitFor(std::chrono::milliseconds timeout) = 0;
};

/**
 * @brief Async operation with typed result
 *
 * Thread-safe container for async operation state, progress, and result.
 *
 * @tparam T Result type
 */
template<typename T>
class AsyncOperation : public AsyncOperationBase {
public:
    using ResultType = T;
    using ProgressCallback = std::function<void(const AsyncProgress&)>;
    using CompletionCallback = std::function<void(const Result<T>&)>;

    AsyncOperation() = default;

    // Non-copyable, movable
    AsyncOperation(const AsyncOperation&) = delete;
    AsyncOperation& operator=(const AsyncOperation&) = delete;
    AsyncOperation(AsyncOperation&&) noexcept = default;
    AsyncOperation& operator=(AsyncOperation&&) noexcept = default;

    /**
     * @brief Get current operation status
     */
    [[nodiscard]] AsyncStatus status() const noexcept override {
        return status_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current progress
     */
    [[nodiscard]] AsyncProgress progress() const override {
        std::lock_guard lock(mutex_);
        return progress_;
    }

    /**
     * @brief Check if operation is complete (success or failure)
     */
    [[nodiscard]] bool isComplete() const noexcept override {
        auto s = status();
        return s == AsyncStatus::Completed ||
               s == AsyncStatus::Failed ||
               s == AsyncStatus::Cancelled;
    }

    /**
     * @brief Check if operation was cancelled
     */
    [[nodiscard]] bool isCancelled() const noexcept override {
        return status() == AsyncStatus::Cancelled;
    }

    /**
     * @brief Check if cancellation was requested
     */
    [[nodiscard]] bool cancellationRequested() const noexcept {
        return cancelRequested_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get error if operation failed
     */
    [[nodiscard]] std::optional<Error> error() const override {
        std::lock_guard lock(mutex_);
        if (result_ && !result_->has_value()) {
            return result_->error();
        }
        return std::nullopt;
    }

    /**
     * @brief Get result (blocks if not complete)
     */
    [[nodiscard]] Result<T> await() {
        wait();
        std::lock_guard lock(mutex_);
        if (result_) {
            return *result_;
        }
        return std::unexpected(Error(ErrorCode::Unknown, "No result available"));
    }

    /**
     * @brief Request cancellation
     */
    void cancel() override {
        cancelRequested_.store(true, std::memory_order_release);
        cv_.notify_all();
    }

    /**
     * @brief Wait for completion
     */
    void wait() override {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return isComplete(); });
    }

    /**
     * @brief Wait with timeout
     * @return true if completed, false if timed out
     */
    bool waitFor(std::chrono::milliseconds timeout) override {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return isComplete(); });
    }

    /**
     * @brief Register progress callback
     *
     * Called on each progress update. May be called from worker thread.
     */
    void onProgress(ProgressCallback callback) {
        std::lock_guard lock(mutex_);
        progressCallback_ = std::move(callback);
    }

    /**
     * @brief Register completion callback
     *
     * Called when operation completes (success, failure, or cancel).
     * May be called from worker thread.
     */
    void onComplete(CompletionCallback callback) {
        std::lock_guard lock(mutex_);
        completionCallback_ = std::move(callback);

        // If already complete, call immediately
        if (isComplete() && result_) {
            callback(*result_);
        }
    }

    // === Internal API (called by executor) ===

    /**
     * @brief Set operation as running
     */
    void start() {
        status_.store(AsyncStatus::Running, std::memory_order_release);
        startTime_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Update progress
     */
    void reportProgress(float percentage, std::string message = "",
                       uint32_t currentStep = 0, uint32_t totalSteps = 0) {
        std::unique_lock lock(mutex_);

        progress_.percentage = std::clamp(percentage, 0.0f, 1.0f);
        progress_.message = std::move(message);
        progress_.currentStep = currentStep;
        progress_.totalSteps = totalSteps;

        // Calculate elapsed time
        auto now = std::chrono::steady_clock::now();
        progress_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime_);

        // Estimate remaining time
        if (percentage > 0.0f && percentage < 1.0f) {
            auto totalEstimate = progress_.elapsed.count() / percentage;
            progress_.estimatedRemaining = std::chrono::milliseconds(
                static_cast<int64_t>(totalEstimate * (1.0f - percentage)));
        }

        // Call callback outside lock
        auto callback = progressCallback_;
        auto prog = progress_;
        lock.unlock();

        if (callback) {
            callback(prog);
        }
    }

    /**
     * @brief Complete with success
     */
    void complete(T value) {
        std::unique_lock lock(mutex_);

        result_ = Result<T>(std::move(value));
        status_.store(AsyncStatus::Completed, std::memory_order_release);

        auto callback = completionCallback_;
        auto result = *result_;
        lock.unlock();

        cv_.notify_all();

        if (callback) {
            callback(result);
        }
    }

    /**
     * @brief Complete with error
     */
    void fail(Error error) {
        std::unique_lock lock(mutex_);

        result_ = std::unexpected(std::move(error));
        status_.store(AsyncStatus::Failed, std::memory_order_release);

        auto callback = completionCallback_;
        auto result = *result_;
        lock.unlock();

        cv_.notify_all();

        if (callback) {
            callback(result);
        }
    }

    /**
     * @brief Mark as cancelled
     */
    void markCancelled() {
        std::unique_lock lock(mutex_);

        result_ = std::unexpected(Error(ErrorCode::Cancelled, "Operation cancelled"));
        status_.store(AsyncStatus::Cancelled, std::memory_order_release);

        auto callback = completionCallback_;
        auto result = *result_;
        lock.unlock();

        cv_.notify_all();

        if (callback) {
            callback(result);
        }
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<AsyncStatus> status_{AsyncStatus::Pending};
    std::atomic<bool> cancelRequested_{false};

    AsyncProgress progress_;
    std::optional<Result<T>> result_;

    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;

    std::chrono::steady_clock::time_point startTime_;
};

/**
 * @brief Shared pointer to async operation (for safe sharing)
 */
template<typename T>
using AsyncOperationPtr = std::shared_ptr<AsyncOperation<T>>;

/**
 * @brief Create a new async operation
 */
template<typename T>
AsyncOperationPtr<T> makeAsyncOperation() {
    return std::make_shared<AsyncOperation<T>>();
}

/**
 * @brief Execute function asynchronously and return operation handle
 *
 * @tparam T Result type
 * @param func Function to execute, receives operation reference for progress/cancel
 * @return Shared pointer to async operation
 */
template<typename T>
AsyncOperationPtr<T> executeAsync(
    std::function<Result<T>(AsyncOperation<T>&)> func
) {
    auto op = makeAsyncOperation<T>();

    std::thread([op, func = std::move(func)]() mutable {
        op->start();

        try {
            auto result = func(*op);

            if (op->cancellationRequested()) {
                op->markCancelled();
            } else if (result.has_value()) {
                op->complete(std::move(*result));
            } else {
                op->fail(result.error());
            }
        } catch (const std::exception& e) {
            op->fail(Error(ErrorCode::Unknown, e.what()));
        } catch (...) {
            op->fail(Error(ErrorCode::Unknown, "Unknown exception"));
        }
    }).detach();

    return op;
}

/**
 * @brief Simple async execution without progress
 */
template<typename T>
AsyncOperationPtr<T> executeAsync(std::function<Result<T>()> func) {
    return executeAsync<T>([func = std::move(func)](AsyncOperation<T>&) {
        return func();
    });
}

/**
 * @brief Background task queue for sequential execution
 *
 * Ensures tasks run one at a time in order, with progress tracking.
 */
class AsyncQueue {
public:
    AsyncQueue() : running_(true) {
        worker_ = std::thread(&AsyncQueue::workerLoop, this);
    }

    ~AsyncQueue() {
        {
            std::lock_guard lock(mutex_);
            running_ = false;
        }
        cv_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    // Non-copyable, non-movable
    AsyncQueue(const AsyncQueue&) = delete;
    AsyncQueue& operator=(const AsyncQueue&) = delete;

    /**
     * @brief Enqueue a task
     */
    template<typename T>
    AsyncOperationPtr<T> enqueue(
        std::function<Result<T>(AsyncOperation<T>&)> func
    ) {
        auto op = makeAsyncOperation<T>();

        {
            std::lock_guard lock(mutex_);
            tasks_.push([op, func = std::move(func)]() mutable {
                op->start();
                try {
                    auto result = func(*op);
                    if (op->cancellationRequested()) {
                        op->markCancelled();
                    } else if (result.has_value()) {
                        op->complete(std::move(*result));
                    } else {
                        op->fail(result.error());
                    }
                } catch (const std::exception& e) {
                    op->fail(Error(ErrorCode::Unknown, e.what()));
                }
            });
        }

        cv_.notify_one();
        return op;
    }

    /**
     * @brief Get number of pending tasks
     */
    [[nodiscard]] size_t pendingCount() const {
        std::lock_guard lock(mutex_);
        return tasks_.size();
    }

    /**
     * @brief Check if queue is empty
     */
    [[nodiscard]] bool isEmpty() const {
        std::lock_guard lock(mutex_);
        return tasks_.empty();
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return !running_ || !tasks_.empty(); });

                if (!running_ && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            if (task) {
                task();
            }
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::thread worker_;
    bool running_;
};

/**
 * @brief Global async queue singleton
 */
inline AsyncQueue& globalAsyncQueue() {
    static AsyncQueue queue;
    return queue;
}

} // namespace makine
