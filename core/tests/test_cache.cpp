/**
 * @file test_cache.cpp
 * @brief Unit tests for cache utilities (LRUCache, TTLCache)
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/cache.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace makine {
namespace testing {

// =========================================================================
// LRU CACHE
// =========================================================================

class LRUCacheTest : public ::testing::Test {
protected:
    LRUCache<std::string, int> cache_{5};
};

TEST_F(LRUCacheTest, InsertAndRetrieve) {
    cache_.put("key1", 42);
    auto result = cache_.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(LRUCacheTest, GetMissingReturnsNullopt) {
    EXPECT_FALSE(cache_.get("nonexistent").has_value());
}

TEST_F(LRUCacheTest, EvictionWhenFull) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    cache_.put("c", 3);
    cache_.put("d", 4);
    cache_.put("e", 5);
    EXPECT_EQ(cache_.size(), 5u);

    cache_.put("f", 6);
    EXPECT_EQ(cache_.size(), 5u);
    EXPECT_FALSE(cache_.get("a").has_value());
    EXPECT_TRUE(cache_.get("f").has_value());
}

TEST_F(LRUCacheTest, AccessOrderUpdates) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    cache_.put("c", 3);
    cache_.put("d", 4);
    cache_.put("e", 5);

    // Access "a" to make it MRU
    (void)cache_.get("a");

    // Add new item — should evict "b" (now LRU), not "a"
    cache_.put("f", 6);
    EXPECT_TRUE(cache_.get("a").has_value());
    EXPECT_FALSE(cache_.get("b").has_value());
}

TEST_F(LRUCacheTest, InvalidateRemovesEntry) {
    cache_.put("key", 100);
    EXPECT_TRUE(cache_.contains("key"));

    EXPECT_TRUE(cache_.invalidate("key"));
    EXPECT_FALSE(cache_.contains("key"));
    EXPECT_FALSE(cache_.get("key").has_value());
}

TEST_F(LRUCacheTest, InvalidateNonexistentReturnsFalse) {
    EXPECT_FALSE(cache_.invalidate("nonexistent"));
}

TEST_F(LRUCacheTest, ClearRemovesAll) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    EXPECT_EQ(cache_.size(), 2u);

    cache_.clear();
    EXPECT_EQ(cache_.size(), 0u);
    EXPECT_TRUE(cache_.empty());
}

TEST_F(LRUCacheTest, ContainsReturnsTrueForExisting) {
    cache_.put("exists", 42);
    EXPECT_TRUE(cache_.contains("exists"));
}

TEST_F(LRUCacheTest, ContainsReturnsFalseAfterEviction) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    cache_.put("c", 3);
    cache_.put("d", 4);
    cache_.put("e", 5);
    cache_.put("f", 6); // evicts "a"
    EXPECT_FALSE(cache_.contains("a"));
}

TEST_F(LRUCacheTest, StatsTrackHitsAndMisses) {
    cache_.put("hit", 1);
    (void)cache_.get("hit");     // hit
    (void)cache_.get("hit");     // hit
    (void)cache_.get("miss");    // miss

    auto stats = cache_.stats();
    EXPECT_EQ(stats.hits, 2u);
    EXPECT_EQ(stats.misses, 1u);
    EXPECT_DOUBLE_EQ(stats.hitRate(), 2.0 / 3.0);
}

TEST_F(LRUCacheTest, StatsTrackEvictions) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    cache_.put("c", 3);
    cache_.put("d", 4);
    cache_.put("e", 5);
    cache_.put("f", 6); // 1 eviction

    auto stats = cache_.stats();
    EXPECT_EQ(stats.evictions, 1u);
}

TEST_F(LRUCacheTest, ResetStatsZeroesCounters) {
    cache_.put("key", 1);
    (void)cache_.get("key");
    (void)cache_.get("miss");
    cache_.resetStats();

    auto stats = cache_.stats();
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_EQ(stats.misses, 0u);
    EXPECT_EQ(stats.evictions, 0u);
    EXPECT_DOUBLE_EQ(stats.hitRate(), 0.0);
}

TEST_F(LRUCacheTest, HitRateZeroWhenNoAccesses) {
    auto stats = cache_.stats();
    EXPECT_DOUBLE_EQ(stats.hitRate(), 0.0);
}

TEST_F(LRUCacheTest, GetOrComputeCallsFuncOnMiss) {
    bool called = false;
    auto result = cache_.getOrCompute("computed", [&]() {
        called = true;
        return 99;
    });
    EXPECT_TRUE(called);
    EXPECT_EQ(result, 99);
}

TEST_F(LRUCacheTest, GetOrComputeUsesCacheOnHit) {
    cache_.put("cached", 42);
    bool called = false;
    auto result = cache_.getOrCompute("cached", [&]() {
        called = true;
        return 99;
    });
    EXPECT_FALSE(called);
    EXPECT_EQ(result, 42);
}

TEST_F(LRUCacheTest, GetOrComputeStoresResult) {
    (void)cache_.getOrCompute("new", []() { return 123; });

    auto result = cache_.get("new");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 123);
}

TEST_F(LRUCacheTest, ZeroCapacityIsUnlimited) {
    LRUCache<std::string, int> unlimited(0);
    unlimited.put("a", 1);
    unlimited.put("b", 2);
    unlimited.put("c", 3);
    EXPECT_EQ(unlimited.size(), 3u);
    EXPECT_TRUE(unlimited.get("a").has_value());
}

TEST_F(LRUCacheTest, SingleItemCache) {
    LRUCache<std::string, int> tiny(1);
    tiny.put("first", 1);
    EXPECT_EQ(tiny.size(), 1u);

    tiny.put("second", 2);
    EXPECT_EQ(tiny.size(), 1u);
    EXPECT_FALSE(tiny.get("first").has_value());
    EXPECT_TRUE(tiny.get("second").has_value());
}

TEST_F(LRUCacheTest, UpdateExistingKey) {
    cache_.put("key", 1);
    cache_.put("key", 2);
    EXPECT_EQ(cache_.size(), 1u);

    auto result = cache_.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);
}

TEST_F(LRUCacheTest, MaxSizeAccessor) {
    EXPECT_EQ(cache_.maxSize(), 5u);
}

TEST_F(LRUCacheTest, EmptyOnCreation) {
    EXPECT_TRUE(cache_.empty());
    EXPECT_EQ(cache_.size(), 0u);
}

// =========================================================================
// TTL CACHE
// =========================================================================

class TTLCacheTest : public ::testing::Test {
protected:
    using Duration = std::chrono::milliseconds;
    TTLCache<std::string, int> cache_{Duration(500), 100};
};

TEST_F(TTLCacheTest, InsertAndRetrieveBeforeExpiry) {
    cache_.put("key", 42);
    auto result = cache_.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(TTLCacheTest, ExpiredEntryReturnsNullopt) {
    TTLCache<std::string, int> shortCache(Duration(50), 100);
    shortCache.put("key", 42);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(shortCache.get("key").has_value());
}

TEST_F(TTLCacheTest, CustomTTLPerEntry) {
    cache_.put("short", 1, Duration(50));
    cache_.put("long", 2, Duration(5000));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(cache_.get("short").has_value());
    EXPECT_TRUE(cache_.get("long").has_value());
}

TEST_F(TTLCacheTest, CleanupExpiredRemovesOldEntries) {
    TTLCache<std::string, int> shortCache(Duration(50), 100);
    shortCache.put("a", 1);
    shortCache.put("b", 2);
    EXPECT_EQ(shortCache.size(), 2u);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto removed = shortCache.cleanupExpired();
    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(shortCache.size(), 0u);
}

TEST_F(TTLCacheTest, InvalidateRemovesBeforeExpiry) {
    cache_.put("key", 42);
    EXPECT_TRUE(cache_.invalidate("key"));
    EXPECT_FALSE(cache_.get("key").has_value());
}

TEST_F(TTLCacheTest, InvalidateNonexistentReturnsFalse) {
    EXPECT_FALSE(cache_.invalidate("nonexistent"));
}

TEST_F(TTLCacheTest, ClearRemovesAll) {
    cache_.put("a", 1);
    cache_.put("b", 2);
    cache_.clear();
    EXPECT_EQ(cache_.size(), 0u);
}

TEST_F(TTLCacheTest, SizeIncludesExpired) {
    TTLCache<std::string, int> shortCache(Duration(50), 100);
    shortCache.put("a", 1);
    shortCache.put("b", 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // size() includes expired entries
    EXPECT_EQ(shortCache.size(), 2u);
}

TEST_F(TTLCacheTest, PutRefreshesExpiry) {
    TTLCache<std::string, int> shortCache(Duration(100), 100);
    shortCache.put("key", 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    shortCache.put("key", 2); // refresh TTL

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    auto result = shortCache.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2);
}

TEST_F(TTLCacheTest, ZeroTTLExpiresImmediately) {
    cache_.put("instant", 42, Duration(0));
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    EXPECT_FALSE(cache_.get("instant").has_value());
}

TEST_F(TTLCacheTest, VeryLongTTLDoesNotExpire) {
    cache_.put("eternal", 42, std::chrono::hours(24));
    auto result = cache_.get("eternal");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST_F(TTLCacheTest, CleanupExpiredOnFreshCacheReturnsZero) {
    EXPECT_EQ(cache_.cleanupExpired(), 0u);
}

} // namespace testing
} // namespace makine
