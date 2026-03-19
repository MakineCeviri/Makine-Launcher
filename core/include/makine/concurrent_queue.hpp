/**
 * @file concurrent_queue.hpp
 * @brief Lock-free concurrent queue with optional moodycamel backend
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides thread-safe queue for producer-consumer patterns.
 * Uses moodycamel::ConcurrentQueue when available for best performance.
 *
 * Compile-time detection:
 * - MAKINE_HAS_CONCURRENTQUEUE - moodycamel::ConcurrentQueue available
 *
 * Fallback: Mutex-based queue using std::deque.
 */

#pragma once

#include "features.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#ifdef MAKINE_HAS_CONCURRENTQUEUE
#include <moodycamel/concurrentqueue.h>
#include <moodycamel/blockingconcurrentqueue.h>
#endif

namespace makine::concurrent {

// =============================================================================
// Lock-Free Queue Interface
// =============================================================================

/**
 * @brief Thread-safe queue for producer-consumer patterns
 *
 * @tparam T Element type
 */
template <typename T>
class Queue {
public:
    Queue() = default;
    ~Queue() = default;

    // Non-copyable but movable
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue&&) noexcept = default;
    Queue& operator=(Queue&&) noexcept = default;

    /**
     * @brief Add item to queue
     * @return true if enqueued (always true for unbounded queue)
     */
    bool enqueue(const T& item) {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.enqueue(item);
#else
        std::lock_guard lock(mutex_);
        queue_.push_back(item);
        cv_.notify_one();
        return true;
#endif
    }

    /**
     * @brief Add item to queue (move version)
     */
    bool enqueue(T&& item) {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.enqueue(std::move(item));
#else
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(item));
        cv_.notify_one();
        return true;
#endif
    }

    /**
     * @brief Add multiple items to queue
     */
    bool enqueueBulk(const T* items, size_t count) {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.enqueue_bulk(items, count);
#else
        std::lock_guard lock(mutex_);
        for (size_t i = 0; i < count; ++i) {
            queue_.push_back(items[i]);
        }
        cv_.notify_all();
        return true;
#endif
    }

    /**
     * @brief Try to dequeue an item (non-blocking)
     * @return Item if available, nullopt otherwise
     */
    std::optional<T> tryDequeue() {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        T item;
        if (queue_.try_dequeue(item)) {
            return item;
        }
        return std::nullopt;
#else
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
#endif
    }

    /**
     * @brief Dequeue multiple items (non-blocking)
     * @return Number of items dequeued
     */
    size_t tryDequeueBulk(T* items, size_t maxCount) {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.try_dequeue_bulk(items, maxCount);
#else
        std::lock_guard lock(mutex_);
        size_t count = std::min(maxCount, queue_.size());
        for (size_t i = 0; i < count; ++i) {
            items[i] = std::move(queue_.front());
            queue_.pop_front();
        }
        return count;
#endif
    }

    /**
     * @brief Get approximate size (may be inaccurate under contention)
     */
    size_t sizeApprox() const noexcept {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.size_approx();
#else
        std::lock_guard lock(mutex_);
        return queue_.size();
#endif
    }

    /**
     * @brief Check if queue is approximately empty
     */
    bool empty() const noexcept {
        return sizeApprox() == 0;
    }

private:
#ifdef MAKINE_HAS_CONCURRENTQUEUE
    moodycamel::ConcurrentQueue<T> queue_;
#else
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
#endif
};

// =============================================================================
// Blocking Queue
// =============================================================================

/**
 * @brief Thread-safe blocking queue
 *
 * Like Queue but supports blocking dequeue operations.
 */
template <typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;
    ~BlockingQueue() {
        close();
    }

    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    /**
     * @brief Add item to queue
     */
    bool enqueue(const T& item) {
        if (closed_) return false;
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.enqueue(item);
#else
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        queue_.push_back(item);
        cv_.notify_one();
        return true;
#endif
    }

    /**
     * @brief Add item to queue (move version)
     */
    bool enqueue(T&& item) {
        if (closed_) return false;
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.enqueue(std::move(item));
#else
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        queue_.push_back(std::move(item));
        cv_.notify_one();
        return true;
#endif
    }

    /**
     * @brief Dequeue an item (blocking)
     *
     * Blocks until an item is available or queue is closed.
     *
     * @return Item if available, nullopt if queue is closed
     */
    std::optional<T> dequeue() {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        T item;
        while (!closed_) {
            if (queue_.wait_dequeue_timed(item, std::chrono::milliseconds(100))) {
                return item;
            }
        }
        // Drain remaining items
        if (queue_.try_dequeue(item)) {
            return item;
        }
        return std::nullopt;
#else
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() {
            return !queue_.empty() || closed_;
        });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
#endif
    }

    /**
     * @brief Dequeue with timeout
     *
     * @param timeout Maximum time to wait
     * @return Item if available within timeout, nullopt otherwise
     */
    template <typename Rep, typename Period>
    std::optional<T> dequeueFor(std::chrono::duration<Rep, Period> timeout) {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        T item;
        if (queue_.wait_dequeue_timed(item, timeout)) {
            return item;
        }
        return std::nullopt;
#else
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this]() {
            return !queue_.empty() || closed_;
        })) {
            return std::nullopt;  // Timeout
        }
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
#endif
    }

    /**
     * @brief Try to dequeue (non-blocking)
     */
    std::optional<T> tryDequeue() {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        T item;
        if (queue_.try_dequeue(item)) {
            return item;
        }
        return std::nullopt;
#else
        std::lock_guard lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
#endif
    }

    /**
     * @brief Close the queue
     *
     * No more items can be enqueued. Blocked consumers will wake up.
     */
    void close() {
        closed_ = true;
#ifndef MAKINE_HAS_CONCURRENTQUEUE
        cv_.notify_all();
#endif
    }

    /**
     * @brief Check if queue is closed
     */
    bool isClosed() const noexcept {
        return closed_;
    }

    /**
     * @brief Get approximate size
     */
    size_t sizeApprox() const noexcept {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
        return queue_.size_approx();
#else
        std::lock_guard lock(mutex_);
        return queue_.size();
#endif
    }

    bool empty() const noexcept {
        return sizeApprox() == 0;
    }

private:
#ifdef MAKINE_HAS_CONCURRENTQUEUE
    moodycamel::BlockingConcurrentQueue<T> queue_;
#else
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
#endif
    std::atomic<bool> closed_{false};
};

// =============================================================================
// Producer Token (for better performance with moodycamel)
// =============================================================================

#ifdef MAKINE_HAS_CONCURRENTQUEUE

/**
 * @brief Producer token for better enqueue performance
 *
 * Creating a ProducerToken for each producer thread improves
 * performance significantly with moodycamel::ConcurrentQueue.
 */
template <typename T>
class ProducerToken {
public:
    explicit ProducerToken(Queue<T>& queue)
        : token_(queue.queue_) {}

    explicit ProducerToken(BlockingQueue<T>& queue)
        : token_(queue.queue_) {}

    moodycamel::ProducerToken& get() { return token_; }

private:
    moodycamel::ProducerToken token_;
};

/**
 * @brief Consumer token for better dequeue performance
 */
template <typename T>
class ConsumerToken {
public:
    explicit ConsumerToken(Queue<T>& queue)
        : token_(queue.queue_) {}

    explicit ConsumerToken(BlockingQueue<T>& queue)
        : token_(queue.queue_) {}

    moodycamel::ConsumerToken& get() { return token_; }

private:
    moodycamel::ConsumerToken token_;
};

#endif // MAKINE_HAS_CONCURRENTQUEUE

// =============================================================================
// SPSC Queue (Single Producer, Single Consumer)
// =============================================================================

/**
 * @brief Optimized queue for single-producer single-consumer scenarios
 *
 * More efficient than general queue when only one thread produces
 * and one thread consumes.
 */
template <typename T, size_t Capacity = 1024>
class SPSCQueue {
public:
    SPSCQueue() : buffer_(Capacity) {}

    /**
     * @brief Add item (non-blocking)
     * @return true if enqueued, false if full
     */
    bool tryEnqueue(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % Capacity;

        if (nextHead == tail_.load(std::memory_order_acquire)) {
            return false;  // Full
        }

        buffer_[head] = item;
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    bool tryEnqueue(T&& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % Capacity;

        if (nextHead == tail_.load(std::memory_order_acquire)) {
            return false;  // Full
        }

        buffer_[head] = std::move(item);
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    /**
     * @brief Dequeue item (non-blocking)
     */
    std::optional<T> tryDequeue() {
        size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;  // Empty
        }

        T item = std::move(buffer_[tail]);
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return item;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        size_t nextHead = (head_.load(std::memory_order_acquire) + 1) % Capacity;
        return nextHead == tail_.load(std::memory_order_acquire);
    }

    size_t sizeApprox() const noexcept {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (head >= tail) ? (head - tail) : (Capacity - tail + head);
    }

private:
    std::vector<T> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Check if moodycamel::ConcurrentQueue is available
 */
inline bool hasConcurrentQueue() noexcept {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get queue backend name
 */
inline const char* backendName() noexcept {
#ifdef MAKINE_HAS_CONCURRENTQUEUE
    return "moodycamel::ConcurrentQueue";
#else
    return "std::deque + std::mutex";
#endif
}

} // namespace makine::concurrent
