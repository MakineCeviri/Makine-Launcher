#pragma once

/**
 * @file string_pool.hpp
 * @brief String interning and object pooling utilities for Makine
 *
 * Provides:
 * - StringPool: String interning for memory deduplication
 * - ObjectPool: Generic object pooling for frequent allocations
 * - BufferPool: Reusable byte buffer pool
 *
 * Usage:
 * @code
 * // String interning
 * auto& pool = StringPool::instance();
 * std::string_view interned = pool.intern("Hello, World!");
 * // Same string returns same pointer
 * assert(pool.intern("Hello, World!").data() == interned.data());
 *
 * // Object pooling
 * ObjectPool<GameInfo> gamePool(100);  // Pre-allocate 100
 * auto* game = gamePool.acquire();
 * // ... use game ...
 * gamePool.release(game);
 *
 * // Buffer pooling
 * BufferPool buffers(4096);  // 4KB buffers
 * auto buffer = buffers.acquire();
 * // ... use buffer ...
 * buffers.release(std::move(buffer));
 * @endcode
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace makine {

// =============================================================================
// STRING POOL (String Interning)
// =============================================================================

/**
 * @brief Thread-safe string interning pool
 *
 * Stores unique strings and returns stable string_views to them.
 * Reduces memory usage when the same strings appear many times.
 */
class StringPool {
public:
    /**
     * @brief Pool statistics
     */
    struct Stats {
        size_t uniqueStrings = 0;     ///< Number of unique strings
        size_t totalInterns = 0;      ///< Total intern() calls
        size_t hits = 0;              ///< Times existing string was found
        size_t totalBytesStored = 0;  ///< Bytes used by stored strings

        [[nodiscard]] double hitRate() const noexcept {
            return totalInterns > 0 ? static_cast<double>(hits) / totalInterns : 0.0;
        }
    };

    /**
     * @brief Get global instance
     */
    static StringPool& instance() {
        static StringPool pool;
        return pool;
    }

    /**
     * @brief Intern a string
     * @param str String to intern
     * @return Stable string_view to the interned string
     *
     * If the string already exists in the pool, returns view to existing.
     * Otherwise, copies the string and returns view to copy.
     */
    [[nodiscard]] std::string_view intern(std::string_view str) {
        // Fast path: check if already interned (read lock)
        {
            std::shared_lock readLock(mutex_);
            if (auto it = pool_.find(std::string(str)); it != pool_.end()) {
                ++stats_.hits;
                ++stats_.totalInterns;
                return *it;
            }
        }

        // Slow path: add new string (write lock)
        std::unique_lock writeLock(mutex_);

        // Double-check after acquiring write lock
        auto [it, inserted] = pool_.emplace(str);
        ++stats_.totalInterns;

        if (inserted) {
            ++stats_.uniqueStrings;
            stats_.totalBytesStored += str.size();
        } else {
            ++stats_.hits;
        }

        return *it;
    }

    /**
     * @brief Check if string is interned
     */
    [[nodiscard]] bool contains(std::string_view str) const {
        std::shared_lock lock(mutex_);
        return pool_.find(std::string(str)) != pool_.end();
    }

    /**
     * @brief Get pool statistics
     */
    [[nodiscard]] Stats stats() const {
        std::shared_lock lock(mutex_);
        return stats_;
    }

    /**
     * @brief Get number of unique strings
     */
    [[nodiscard]] size_t size() const {
        std::shared_lock lock(mutex_);
        return pool_.size();
    }

    /**
     * @brief Clear the pool
     * @warning Invalidates all previously returned string_views!
     */
    void clear() {
        std::unique_lock lock(mutex_);
        pool_.clear();
        stats_ = Stats{};
    }

    /**
     * @brief Reserve capacity
     */
    void reserve(size_t count) {
        std::unique_lock lock(mutex_);
        pool_.reserve(count);
    }

private:
    StringPool() = default;
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_set<std::string> pool_;
    mutable Stats stats_;
};

/**
 * @brief Convenience function for string interning
 */
[[nodiscard]] inline std::string_view intern(std::string_view str) {
    return StringPool::instance().intern(str);
}

// =============================================================================
// OBJECT POOL (Generic)
// =============================================================================

/**
 * @brief Thread-safe generic object pool
 *
 * Pre-allocates objects and reuses them to avoid frequent allocations.
 * Objects are default-constructed and should have a reset() method
 * or be cheaply re-initializable.
 *
 * @tparam T Object type (must be default constructible)
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief Pool statistics
     */
    struct Stats {
        size_t poolSize = 0;        ///< Current pool size
        size_t peakPoolSize = 0;    ///< Maximum pool size
        size_t acquires = 0;        ///< Total acquire() calls
        size_t releases = 0;        ///< Total release() calls
        size_t allocations = 0;     ///< New allocations (pool was empty)
    };

    /**
     * @brief Create pool with initial capacity
     */
    explicit ObjectPool(size_t initialSize = 0) {
        for (size_t i = 0; i < initialSize; ++i) {
            pool_.push(std::make_unique<T>());
        }
        stats_.poolSize = initialSize;
        stats_.peakPoolSize = initialSize;
    }

    /**
     * @brief Acquire an object from pool
     * @return Pointer to object (owned by pool, do not delete!)
     *
     * Returns existing object if available, or creates new one.
     */
    [[nodiscard]] T* acquire() {
        std::lock_guard lock(mutex_);
        ++stats_.acquires;

        if (pool_.empty()) {
            ++stats_.allocations;
            active_.push_back(std::make_unique<T>());
            return active_.back().get();
        }

        auto obj = std::move(pool_.top());
        pool_.pop();
        --stats_.poolSize;

        active_.push_back(std::move(obj));
        return active_.back().get();
    }

    /**
     * @brief Release object back to pool
     * @param obj Object to release
     *
     * Call reset() on object if it has one, then return to pool.
     */
    void release(T* obj) {
        if (!obj) return;

        std::lock_guard lock(mutex_);
        ++stats_.releases;

        // Find in active list
        auto it = std::find_if(active_.begin(), active_.end(),
            [obj](const std::unique_ptr<T>& p) { return p.get() == obj; });

        if (it != active_.end()) {
            // Reset if possible
            if constexpr (requires(T& t) { t.reset(); }) {
                (*it)->reset();
            }

            pool_.push(std::move(*it));
            active_.erase(it);
            ++stats_.poolSize;
            stats_.peakPoolSize = std::max(stats_.peakPoolSize, stats_.poolSize);
        }
    }

    /**
     * @brief Get pool statistics
     */
    [[nodiscard]] Stats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    /**
     * @brief Get current available pool size
     */
    [[nodiscard]] size_t available() const {
        std::lock_guard lock(mutex_);
        return pool_.size();
    }

    /**
     * @brief Get number of active (acquired) objects
     */
    [[nodiscard]] size_t active() const {
        std::lock_guard lock(mutex_);
        return active_.size();
    }

    /**
     * @brief Pre-allocate more objects
     */
    void reserve(size_t count) {
        std::lock_guard lock(mutex_);
        for (size_t i = pool_.size(); i < count; ++i) {
            pool_.push(std::make_unique<T>());
            ++stats_.poolSize;
        }
        stats_.peakPoolSize = std::max(stats_.peakPoolSize, stats_.poolSize);
    }

    /**
     * @brief Clear all pooled objects (does not affect active objects)
     */
    void shrink() {
        std::lock_guard lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
        stats_.poolSize = 0;
    }

private:
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<T>> pool_;
    std::vector<std::unique_ptr<T>> active_;
    mutable Stats stats_;
};

// =============================================================================
// BUFFER POOL (Specialized for byte buffers)
// =============================================================================

/**
 * @brief Thread-safe buffer pool for byte arrays
 *
 * Optimized for reusing fixed-size byte buffers.
 */
class BufferPool {
public:
    using Buffer = std::vector<uint8_t>;

    /**
     * @brief Pool statistics
     */
    struct Stats {
        size_t bufferSize = 0;      ///< Size of each buffer
        size_t poolSize = 0;        ///< Current pool size
        size_t acquires = 0;        ///< Total acquire() calls
        size_t releases = 0;        ///< Total release() calls
        size_t allocations = 0;     ///< New allocations
    };

    /**
     * @brief Create pool with specified buffer size
     * @param bufferSize Size of each buffer in bytes
     * @param initialCount Initial number of buffers to allocate
     */
    explicit BufferPool(size_t bufferSize, size_t initialCount = 0)
        : bufferSize_(bufferSize)
    {
        stats_.bufferSize = bufferSize;
        for (size_t i = 0; i < initialCount; ++i) {
            pool_.push(Buffer(bufferSize));
            ++stats_.poolSize;
        }
    }

    /**
     * @brief Acquire a buffer from pool
     * @return Buffer of configured size (cleared to zero)
     */
    [[nodiscard]] Buffer acquire() {
        std::lock_guard lock(mutex_);
        ++stats_.acquires;

        if (pool_.empty()) {
            ++stats_.allocations;
            return Buffer(bufferSize_, 0);
        }

        Buffer buf = std::move(pool_.top());
        pool_.pop();
        --stats_.poolSize;

        // Clear the buffer
        std::fill(buf.begin(), buf.end(), 0);
        return buf;
    }

    /**
     * @brief Release buffer back to pool
     * @param buffer Buffer to release (will be moved)
     */
    void release(Buffer buffer) {
        if (buffer.size() != bufferSize_) {
            // Wrong size, don't pool it
            return;
        }

        std::lock_guard lock(mutex_);
        ++stats_.releases;
        pool_.push(std::move(buffer));
        ++stats_.poolSize;
    }

    /**
     * @brief Get pool statistics
     */
    [[nodiscard]] Stats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    /**
     * @brief Get buffer size
     */
    [[nodiscard]] size_t bufferSize() const noexcept {
        return bufferSize_;
    }

    /**
     * @brief Get available buffer count
     */
    [[nodiscard]] size_t available() const {
        std::lock_guard lock(mutex_);
        return pool_.size();
    }

    /**
     * @brief Pre-allocate more buffers
     */
    void reserve(size_t count) {
        std::lock_guard lock(mutex_);
        for (size_t i = pool_.size(); i < count; ++i) {
            pool_.push(Buffer(bufferSize_));
            ++stats_.poolSize;
        }
    }

    /**
     * @brief Clear all pooled buffers
     */
    void shrink() {
        std::lock_guard lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
        stats_.poolSize = 0;
    }

private:
    size_t bufferSize_;
    mutable std::mutex mutex_;
    std::stack<Buffer> pool_;
    mutable Stats stats_;
};

// =============================================================================
// SCOPED POOL OBJECT (RAII)
// =============================================================================

/**
 * @brief RAII wrapper for pooled objects
 *
 * Automatically releases object back to pool on destruction.
 *
 * @tparam T Object type
 */
template<typename T>
class PooledObject {
public:
    PooledObject(ObjectPool<T>& pool)
        : pool_(pool), obj_(pool.acquire())
    {}

    ~PooledObject() {
        if (obj_) {
            pool_.release(obj_);
        }
    }

    // Non-copyable
    PooledObject(const PooledObject&) = delete;
    PooledObject& operator=(const PooledObject&) = delete;

    // Movable
    PooledObject(PooledObject&& other) noexcept
        : pool_(other.pool_), obj_(other.obj_)
    {
        other.obj_ = nullptr;
    }

    PooledObject& operator=(PooledObject&& other) noexcept {
        if (this != &other) {
            if (obj_) pool_.release(obj_);
            obj_ = other.obj_;
            other.obj_ = nullptr;
        }
        return *this;
    }

    T* get() noexcept { return obj_; }
    const T* get() const noexcept { return obj_; }

    T& operator*() noexcept { return *obj_; }
    const T& operator*() const noexcept { return *obj_; }

    T* operator->() noexcept { return obj_; }
    const T* operator->() const noexcept { return obj_; }

    explicit operator bool() const noexcept { return obj_ != nullptr; }

    /**
     * @brief Release ownership (caller is responsible for returning to pool)
     */
    T* release() noexcept {
        T* tmp = obj_;
        obj_ = nullptr;
        return tmp;
    }

private:
    ObjectPool<T>& pool_;
    T* obj_;
};

/**
 * @brief RAII wrapper for pooled buffers
 */
class PooledBuffer {
public:
    PooledBuffer(BufferPool& pool)
        : pool_(pool), buffer_(pool.acquire()), released_(false)
    {}

    ~PooledBuffer() {
        if (!released_) {
            pool_.release(std::move(buffer_));
        }
    }

    // Non-copyable
    PooledBuffer(const PooledBuffer&) = delete;
    PooledBuffer& operator=(const PooledBuffer&) = delete;

    // Movable
    PooledBuffer(PooledBuffer&& other) noexcept
        : pool_(other.pool_)
        , buffer_(std::move(other.buffer_))
        , released_(other.released_)
    {
        other.released_ = true;
    }

    uint8_t* data() noexcept { return buffer_.data(); }
    const uint8_t* data() const noexcept { return buffer_.data(); }

    size_t size() const noexcept { return buffer_.size(); }

    uint8_t& operator[](size_t idx) { return buffer_[idx]; }
    const uint8_t& operator[](size_t idx) const { return buffer_[idx]; }

    BufferPool::Buffer& buffer() noexcept { return buffer_; }
    const BufferPool::Buffer& buffer() const noexcept { return buffer_; }

    /**
     * @brief Release ownership
     */
    BufferPool::Buffer release() {
        released_ = true;
        return std::move(buffer_);
    }

private:
    BufferPool& pool_;
    BufferPool::Buffer buffer_;
    bool released_;
};

// =============================================================================
// GLOBAL POOLS
// =============================================================================

/**
 * @brief Global buffer pool manager
 */
class GlobalPools {
public:
    static GlobalPools& instance() {
        static GlobalPools pools;
        return pools;
    }

    /**
     * @brief Get small buffer pool (1KB)
     */
    BufferPool& smallBuffers() { return smallBuffers_; }

    /**
     * @brief Get medium buffer pool (64KB)
     */
    BufferPool& mediumBuffers() { return mediumBuffers_; }

    /**
     * @brief Get large buffer pool (1MB)
     */
    BufferPool& largeBuffers() { return largeBuffers_; }

    /**
     * @brief Get string pool
     */
    StringPool& strings() { return StringPool::instance(); }

    /**
     * @brief Shrink all pools to free memory
     */
    void shrinkAll() {
        smallBuffers_.shrink();
        mediumBuffers_.shrink();
        largeBuffers_.shrink();
    }

private:
    GlobalPools()
        : smallBuffers_(1024, 16)      // 1KB buffers, pre-allocate 16
        , mediumBuffers_(64 * 1024, 8) // 64KB buffers, pre-allocate 8
        , largeBuffers_(1024 * 1024, 2) // 1MB buffers, pre-allocate 2
    {}

    BufferPool smallBuffers_;
    BufferPool mediumBuffers_;
    BufferPool largeBuffers_;
};

/**
 * @brief Convenience function to access global pools
 */
inline GlobalPools& pools() {
    return GlobalPools::instance();
}

} // namespace makine
