/**
 * @file test_string_pool.cpp
 * @brief Unit tests for string_pool.hpp — StringPool, ObjectPool, BufferPool, GlobalPools
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/string_pool.hpp>

#include <string>
#include <vector>

namespace makine {
namespace testing {

// ===========================================================================
// StringPool
// ===========================================================================

class StringPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringPool::instance().clear();
    }

    void TearDown() override {
        StringPool::instance().clear();
    }
};

TEST_F(StringPoolTest, InternReturnsSameView) {
    auto v1 = StringPool::instance().intern("hello");
    auto v2 = StringPool::instance().intern("hello");

    EXPECT_EQ(v1, v2);
    EXPECT_EQ(v1.data(), v2.data()); // Same pointer
}

TEST_F(StringPoolTest, InternDifferentStrings) {
    auto v1 = StringPool::instance().intern("alpha");
    auto v2 = StringPool::instance().intern("beta");

    EXPECT_NE(v1, v2);
}

TEST_F(StringPoolTest, ContainsAfterIntern) {
    StringPool::instance().intern("test");
    EXPECT_TRUE(StringPool::instance().contains("test"));
}

TEST_F(StringPoolTest, ContainsFalseForUnknown) {
    EXPECT_FALSE(StringPool::instance().contains("never_interned"));
}

TEST_F(StringPoolTest, SizeIncrementsOnNewStrings) {
    StringPool::instance().intern("a");
    StringPool::instance().intern("b");
    StringPool::instance().intern("a"); // Duplicate

    EXPECT_EQ(StringPool::instance().size(), 2u);
}

TEST_F(StringPoolTest, StatsTracksHits) {
    StringPool::instance().intern("x");
    StringPool::instance().intern("x"); // Hit

    auto stats = StringPool::instance().stats();
    EXPECT_EQ(stats.uniqueStrings, 1u);
    EXPECT_EQ(stats.totalInterns, 2u);
    EXPECT_GE(stats.hits, 1u);
    EXPECT_GT(stats.hitRate(), 0.0);
}

TEST_F(StringPoolTest, StatsHitRateZeroWhenNoHits) {
    StringPool::instance().intern("unique1");
    StringPool::instance().intern("unique2");

    auto stats = StringPool::instance().stats();
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_DOUBLE_EQ(stats.hitRate(), 0.0);
}

TEST_F(StringPoolTest, ClearEmptiesPool) {
    StringPool::instance().intern("a");
    StringPool::instance().intern("b");
    StringPool::instance().clear();

    EXPECT_EQ(StringPool::instance().size(), 0u);
    EXPECT_FALSE(StringPool::instance().contains("a"));
}

TEST_F(StringPoolTest, ReserveDoesNotCrash) {
    StringPool::instance().reserve(1000);
    SUCCEED();
}

TEST_F(StringPoolTest, InternEmptyString) {
    auto v = StringPool::instance().intern("");
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(StringPool::instance().contains(""));
}

TEST_F(StringPoolTest, ConvenienceInternFunction) {
    auto v = intern("convenience_test");
    EXPECT_EQ(v, "convenience_test");
}

TEST_F(StringPoolTest, StatsTracksTotalBytesStored) {
    StringPool::instance().intern("12345"); // 5 bytes

    auto stats = StringPool::instance().stats();
    EXPECT_GE(stats.totalBytesStored, 5u);
}

// ===========================================================================
// ObjectPool
// ===========================================================================

struct TestObj {
    int value = 0;
    bool wasReset = false;

    void reset() {
        value = 0;
        wasReset = true;
    }
};

TEST(ObjectPoolTest, AcquireReturnsNonNull) {
    ObjectPool<TestObj> pool;
    auto* obj = pool.acquire();
    ASSERT_NE(obj, nullptr);
    pool.release(obj);
}

TEST(ObjectPoolTest, PreAllocatedPool) {
    ObjectPool<TestObj> pool(5);
    EXPECT_EQ(pool.available(), 5u);
    EXPECT_EQ(pool.active(), 0u);
}

TEST(ObjectPoolTest, AcquireFromPreAllocated) {
    ObjectPool<TestObj> pool(3);
    auto* obj = pool.acquire();
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(pool.available(), 2u);
    EXPECT_EQ(pool.active(), 1u);
    pool.release(obj);
}

TEST(ObjectPoolTest, ReleaseReturnsToPool) {
    ObjectPool<TestObj> pool;
    auto* obj = pool.acquire();
    pool.release(obj);

    EXPECT_EQ(pool.available(), 1u);
    EXPECT_EQ(pool.active(), 0u);
}

TEST(ObjectPoolTest, ReleaseCallsReset) {
    ObjectPool<TestObj> pool;
    auto* obj = pool.acquire();
    obj->value = 42;
    pool.release(obj);

    auto stats = pool.stats();
    EXPECT_EQ(stats.releases, 1u);
}

TEST(ObjectPoolTest, ReleaseNullIsNoOp) {
    ObjectPool<TestObj> pool;
    pool.release(nullptr);
    auto stats = pool.stats();
    EXPECT_EQ(stats.releases, 0u);
}

TEST(ObjectPoolTest, StatsTracksAcquiresAndAllocations) {
    ObjectPool<TestObj> pool;
    auto* obj1 = pool.acquire();
    auto* obj2 = pool.acquire();

    auto stats = pool.stats();
    EXPECT_EQ(stats.acquires, 2u);
    EXPECT_EQ(stats.allocations, 2u); // Both were new

    pool.release(obj1);
    pool.release(obj2);
}

TEST(ObjectPoolTest, ReserveAddsToPool) {
    ObjectPool<TestObj> pool;
    pool.reserve(10);
    EXPECT_GE(pool.available(), 10u);
}

TEST(ObjectPoolTest, ShrinkClearsPool) {
    ObjectPool<TestObj> pool(5);
    EXPECT_EQ(pool.available(), 5u);

    pool.shrink();
    EXPECT_EQ(pool.available(), 0u);
}

TEST(ObjectPoolTest, ReusedObjectFromPool) {
    ObjectPool<TestObj> pool;

    auto* obj1 = pool.acquire();
    pool.release(obj1);

    // Second acquire should reuse from pool (no new allocation)
    auto* obj2 = pool.acquire();
    (void)obj2;

    auto stats = pool.stats();
    EXPECT_EQ(stats.allocations, 1u); // Only first was new
    pool.release(obj2);
}

// ===========================================================================
// PooledObject RAII
// ===========================================================================

TEST(PooledObjectTest, RAIIReleasesOnDestruction) {
    ObjectPool<TestObj> pool(3);

    {
        PooledObject<TestObj> obj(pool);
        EXPECT_NE(obj.get(), nullptr);
        obj->value = 42;
        EXPECT_EQ((*obj).value, 42);
    }

    // After scope, object returned to pool
    EXPECT_EQ(pool.active(), 0u);
    EXPECT_GE(pool.available(), 1u);
}

TEST(PooledObjectTest, BoolConversion) {
    ObjectPool<TestObj> pool;
    PooledObject<TestObj> obj(pool);
    EXPECT_TRUE(static_cast<bool>(obj));
}

TEST(PooledObjectTest, ManualRelease) {
    ObjectPool<TestObj> pool;
    PooledObject<TestObj> obj(pool);

    TestObj* raw = obj.release();
    EXPECT_NE(raw, nullptr);
    EXPECT_FALSE(static_cast<bool>(obj));

    // Must manually release
    pool.release(raw);
}

// ===========================================================================
// BufferPool
// ===========================================================================

TEST(BufferPoolTest, AcquireReturnsCorrectSize) {
    BufferPool pool(1024);
    auto buf = pool.acquire();
    EXPECT_EQ(buf.size(), 1024u);
}

TEST(BufferPoolTest, AcquiredBufferIsZeroed) {
    BufferPool pool(64);
    auto buf = pool.acquire();
    for (auto b : buf) {
        EXPECT_EQ(b, 0);
    }
}

TEST(BufferPoolTest, ReleaseAndReacquire) {
    BufferPool pool(256);
    auto buf = pool.acquire();
    buf[0] = 0xFF;
    pool.release(std::move(buf));

    EXPECT_EQ(pool.available(), 1u);

    auto buf2 = pool.acquire();
    EXPECT_EQ(buf2.size(), 256u);
    EXPECT_EQ(buf2[0], 0); // Should be cleared
}

TEST(BufferPoolTest, ReleaseWrongSizeIgnored) {
    BufferPool pool(256);

    std::vector<uint8_t> wrongSize(128, 0);
    pool.release(std::move(wrongSize));

    EXPECT_EQ(pool.available(), 0u); // Not accepted
}

TEST(BufferPoolTest, PreAllocatedBuffers) {
    BufferPool pool(512, 4);
    EXPECT_EQ(pool.available(), 4u);
    EXPECT_EQ(pool.bufferSize(), 512u);
}

TEST(BufferPoolTest, StatsTracking) {
    BufferPool pool(64);
    auto buf = pool.acquire();
    pool.release(std::move(buf));

    auto stats = pool.stats();
    EXPECT_EQ(stats.bufferSize, 64u);
    EXPECT_EQ(stats.acquires, 1u);
    EXPECT_EQ(stats.releases, 1u);
}

TEST(BufferPoolTest, ReserveAddsBuffers) {
    BufferPool pool(128);
    pool.reserve(5);
    EXPECT_GE(pool.available(), 5u);
}

TEST(BufferPoolTest, ShrinkClearsPool) {
    BufferPool pool(128, 5);
    pool.shrink();
    EXPECT_EQ(pool.available(), 0u);
}

// ===========================================================================
// PooledBuffer RAII
// ===========================================================================

TEST(PooledBufferTest, RAIIReleasesOnDestruction) {
    BufferPool pool(256, 2);

    {
        PooledBuffer buf(pool);
        EXPECT_EQ(buf.size(), 256u);
        buf[0] = 0xAA;
    }

    // Buffer returned to pool
    EXPECT_GE(pool.available(), 2u);
}

TEST(PooledBufferTest, DataAccess) {
    BufferPool pool(64);
    PooledBuffer buf(pool);

    EXPECT_NE(buf.data(), nullptr);
    buf[0] = 0x42;
    EXPECT_EQ(buf.data()[0], 0x42);
}

TEST(PooledBufferTest, ManualRelease) {
    BufferPool pool(128);
    PooledBuffer buf(pool);

    auto released = buf.release();
    EXPECT_EQ(released.size(), 128u);

    // Caller owns the buffer now
    pool.release(std::move(released));
}

TEST(PooledBufferTest, BufferReference) {
    BufferPool pool(64);
    PooledBuffer buf(pool);

    auto& ref = buf.buffer();
    EXPECT_EQ(ref.size(), 64u);
}

// ===========================================================================
// GlobalPools
// ===========================================================================

TEST(GlobalPoolsTest, SmallBufferSize) {
    auto& pools = GlobalPools::instance();
    EXPECT_EQ(pools.smallBuffers().bufferSize(), 1024u);
}

TEST(GlobalPoolsTest, MediumBufferSize) {
    auto& pools = GlobalPools::instance();
    EXPECT_EQ(pools.mediumBuffers().bufferSize(), 64u * 1024u);
}

TEST(GlobalPoolsTest, LargeBufferSize) {
    auto& pools = GlobalPools::instance();
    EXPECT_EQ(pools.largeBuffers().bufferSize(), 1024u * 1024u);
}

TEST(GlobalPoolsTest, StringPoolAccessible) {
    auto& pools = GlobalPools::instance();
    auto& sp = pools.strings();
    EXPECT_EQ(&sp, &StringPool::instance());
}

TEST(GlobalPoolsTest, ShrinkAllDoesNotCrash) {
    GlobalPools::instance().shrinkAll();
    SUCCEED();
}

TEST(GlobalPoolsTest, ConvenienceFunction) {
    EXPECT_EQ(&pools(), &GlobalPools::instance());
}

} // namespace testing
} // namespace makine
