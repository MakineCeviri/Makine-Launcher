/**
 * @file cache.hpp
 * @brief Caching utilities for Makine
 *
 * Provides:
 * - LRUCache: Least Recently Used cache with size limit
 * - TTLCache: Time-To-Live cache with expiration
 * - GameInfoCache: Specialized cache for game detection results
 * - TranslationCache: Cache for TM lookups
 *
 * All caches are thread-safe.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include "makine/types.hpp"
#include "makine/constants.hpp"
#include "makine/logging.hpp"

#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace makine {

/**
 * @brief Thread-safe LRU (Least Recently Used) cache
 *
 * Automatically evicts least recently used items when full.
 *
 * @tparam Key Key type (must be hashable)
 * @tparam Value Value type
 */
template<typename Key, typename Value>
class LRUCache {
public:
    using KeyType = Key;
    using ValueType = Value;

    /**
     * @brief Construct cache with max size
     * @param maxSize Maximum number of items (0 = unlimited)
     */
    explicit LRUCache(size_t maxSize = 1000)
        : maxSize_(maxSize)
    {}

    /**
     * @brief Get value if present
     * @param key Key to look up
     * @return Value if found, nullopt otherwise
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::lock_guard lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            ++stats_.misses;
            return std::nullopt;
        }

        // Move to front (most recently used)
        items_.splice(items_.begin(), items_, it->second);
        ++stats_.hits;

        return it->second->second;
    }

    /**
     * @brief Get value or compute if missing
     * @param key Key to look up
     * @param compute Function to compute value if missing
     * @return Cached or computed value
     */
    [[nodiscard]] Value getOrCompute(const Key& key,
                                     std::function<Value()> compute) {
        // Try to get existing
        if (auto cached = get(key)) {
            return *cached;
        }

        // Compute new value
        Value value = compute();
        put(key, value);
        return value;
    }

    /**
     * @brief Store value in cache
     * @param key Key
     * @param value Value to store
     */
    void put(const Key& key, Value value) {
        std::lock_guard lock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update existing
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        // Evict if full
        if (maxSize_ > 0 && items_.size() >= maxSize_) {
            evictOne();
        }

        // Insert new
        items_.emplace_front(key, std::move(value));
        map_[key] = items_.begin();
    }

    /**
     * @brief Remove item from cache
     * @param key Key to remove
     * @return true if item was removed
     */
    bool invalidate(const Key& key) {
        std::lock_guard lock(mutex_);

        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        items_.erase(it->second);
        map_.erase(it);
        return true;
    }

    /**
     * @brief Clear all items
     */
    void clear() {
        std::lock_guard lock(mutex_);
        items_.clear();
        map_.clear();
    }

    /**
     * @brief Check if key exists
     */
    [[nodiscard]] bool contains(const Key& key) const {
        std::lock_guard lock(mutex_);
        return map_.find(key) != map_.end();
    }

    /**
     * @brief Get current size
     */
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return items_.size();
    }

    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return items_.empty();
    }

    /**
     * @brief Get max size
     */
    [[nodiscard]] size_t maxSize() const noexcept {
        return maxSize_;
    }

    /**
     * @brief Cache statistics
     */
    struct Stats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t evictions = 0;

        [[nodiscard]] double hitRate() const noexcept {
            uint64_t total = hits + misses;
            return total > 0 ? static_cast<double>(hits) / total : 0.0;
        }
    };

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] Stats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void resetStats() {
        std::lock_guard lock(mutex_);
        stats_ = Stats{};
    }

private:
    void evictOne() {
        // Remove least recently used (back of list)
        if (!items_.empty()) {
            auto last = items_.back();
            map_.erase(last.first);
            items_.pop_back();
            ++stats_.evictions;
        }
    }

    size_t maxSize_;
    std::list<std::pair<Key, Value>> items_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> map_;
    mutable std::mutex mutex_;
    Stats stats_;
};

/**
 * @brief Time-To-Live cache with automatic expiration
 *
 * Items are automatically invalidated after TTL expires.
 *
 * @tparam Key Key type
 * @tparam Value Value type
 */
template<typename Key, typename Value>
class TTLCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::milliseconds;

    /**
     * @brief Construct cache with TTL
     * @param defaultTTL Default time-to-live for entries
     * @param maxSize Maximum size (0 = unlimited)
     */
    explicit TTLCache(Duration defaultTTL = std::chrono::minutes(5),
                      size_t maxSize = kDefaultCacheMaxSize)
        : defaultTTL_(defaultTTL)
        , maxSize_(maxSize)
    {}

    /**
     * @brief Get value if present and not expired
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::lock_guard lock(mutex_);

        auto it = entries_.find(key);
        if (it == entries_.end()) {
            return std::nullopt;
        }

        // Check expiration
        if (Clock::now() > it->second.expiresAt) {
            entries_.erase(it);
            return std::nullopt;
        }

        return it->second.value;
    }

    /**
     * @brief Store value with default TTL
     */
    void put(const Key& key, Value value) {
        put(key, std::move(value), defaultTTL_);
    }

    /**
     * @brief Store value with custom TTL
     */
    void put(const Key& key, Value value, Duration ttl) {
        std::lock_guard lock(mutex_);

        // Clean up if too large
        if (maxSize_ > 0 && entries_.size() >= maxSize_) {
            cleanupExpired();
        }

        entries_[key] = Entry{
            std::move(value),
            Clock::now() + ttl
        };
    }

    /**
     * @brief Remove entry
     */
    bool invalidate(const Key& key) {
        std::lock_guard lock(mutex_);
        return entries_.erase(key) > 0;
    }

    /**
     * @brief Clear all entries
     */
    void clear() {
        std::lock_guard lock(mutex_);
        entries_.clear();
    }

    /**
     * @brief Remove expired entries
     */
    size_t cleanupExpired() {
        std::lock_guard lock(mutex_);
        return cleanupExpiredInternal();
    }

    /**
     * @brief Get current size (including expired)
     */
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return entries_.size();
    }

private:
    size_t cleanupExpiredInternal() {
        auto now = Clock::now();
        size_t removed = 0;

        for (auto it = entries_.begin(); it != entries_.end(); ) {
            if (now > it->second.expiresAt) {
                it = entries_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }

        return removed;
    }

    struct Entry {
        Value value;
        TimePoint expiresAt;
    };

    Duration defaultTTL_;
    size_t maxSize_;
    std::unordered_map<Key, Entry> entries_;
    mutable std::mutex mutex_;
};

/**
 * @brief Specialized cache for game detection results
 *
 * Caches game info by install path, with file modification time tracking.
 */
class GameInfoCache {
public:
    explicit GameInfoCache(size_t maxSize = 500)
        : cache_(maxSize)
    {}

    /**
     * @brief Get cached game info if still valid
     * @param installPath Game install path
     * @return Cached info if valid, nullopt otherwise
     */
    [[nodiscard]] std::optional<GameInfo> get(const fs::path& installPath) {
        auto key = installPath.string();
        auto entry = cache_.get(key);

        if (!entry) {
            return std::nullopt;
        }

        // Check if any key files have changed
        if (hasChanged(installPath, entry->lastModified)) {
            cache_.invalidate(key);
            return std::nullopt;
        }

        return entry->info;
    }

    /**
     * @brief Cache game info
     */
    void put(const GameInfo& info) {
        CacheEntry entry{
            info,
            getLastModified(info.installPath)
        };
        cache_.put(info.installPath.string(), std::move(entry));
    }

    /**
     * @brief Invalidate cache for a path
     */
    void invalidate(const fs::path& installPath) {
        cache_.invalidate(installPath.string());
    }

    /**
     * @brief Clear all cached games
     */
    void clear() {
        cache_.clear();
    }

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] auto stats() const {
        return cache_.stats();
    }

private:
    struct CacheEntry {
        GameInfo info;
        fs::file_time_type lastModified;
    };

    static fs::file_time_type getLastModified(const fs::path& path) {
        std::error_code ec;
        if (fs::exists(path, ec)) {
            return fs::last_write_time(path, ec);
        }
        return fs::file_time_type::min();
    }

    static bool hasChanged(const fs::path& path, fs::file_time_type cached) {
        return getLastModified(path) != cached;
    }

    LRUCache<std::string, CacheEntry> cache_;
};

/**
 * @brief Specialized cache for translation memory lookups
 *
 * Caches TM search results by source text hash.
 */
class TranslationCache {
public:
    explicit TranslationCache(
        std::chrono::minutes ttl = std::chrono::minutes(10),
        size_t maxSize = kDefaultCacheMaxSize
    )
        : cache_(std::chrono::duration_cast<TTLCache<std::string, CacheEntry>::Duration>(ttl),
                maxSize)
    {}

    /**
     * @brief Get cached translation
     */
    [[nodiscard]] std::optional<std::string> get(
        const std::string& sourceText,
        const std::string& sourceLanguage,
        const std::string& targetLanguage
    ) {
        auto key = makeKey(sourceText, sourceLanguage, targetLanguage);
        auto entry = cache_.get(key);
        return entry ? std::optional(entry->translation) : std::nullopt;
    }

    /**
     * @brief Cache translation
     */
    void put(
        const std::string& sourceText,
        const std::string& sourceLanguage,
        const std::string& targetLanguage,
        const std::string& translation,
        float confidence = 1.0f
    ) {
        auto key = makeKey(sourceText, sourceLanguage, targetLanguage);
        cache_.put(key, CacheEntry{translation, confidence});
    }

    /**
     * @brief Clear cache
     */
    void clear() {
        cache_.clear();
    }

private:
    static std::string makeKey(
        const std::string& source,
        const std::string& srcLang,
        const std::string& tgtLang
    ) {
        return srcLang + ":" + tgtLang + ":" + source;
    }

    struct CacheEntry {
        std::string translation;
        float confidence;
    };

    TTLCache<std::string, CacheEntry> cache_;
};

/**
 * @brief Global caches singleton
 */
class CacheManager {
public:
    static CacheManager& instance() {
        static CacheManager manager;
        return manager;
    }

    GameInfoCache& gameInfo() { return gameInfoCache_; }
    TranslationCache& translations() { return translationCache_; }

    /**
     * @brief Clear all caches
     */
    void clearAll() {
        gameInfoCache_.clear();
        translationCache_.clear();

        MAKINE_LOG_INFO(log::CORE, "All caches cleared");
    }

    /**
     * @brief Log cache statistics
     */
    void logStats() {
        auto gameStats = gameInfoCache_.stats();
        MAKINE_LOG_INFO(log::CORE,
            "GameInfoCache: hits={}, misses={}, rate={:.1f}%",
            gameStats.hits, gameStats.misses, gameStats.hitRate() * 100);
    }

private:
    CacheManager() = default;

    GameInfoCache gameInfoCache_{500};
    TranslationCache translationCache_{std::chrono::minutes(10), kDefaultCacheMaxSize};
};

/**
 * @brief Convenience function to access global cache manager
 */
inline CacheManager& caches() {
    return CacheManager::instance();
}

} // namespace makine
