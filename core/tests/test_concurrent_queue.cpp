/**
 * @file test_concurrent_queue.cpp
 * @brief Unit tests for concurrent_queue.hpp — Queue, BlockingQueue, SPSCQueue
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/concurrent_queue.hpp>

#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <cstring>

namespace makine {
namespace testing {

// ===========================================================================
// Queue<T>
// ===========================================================================

TEST(ConcurrentQueueTest, EnqueueAndDequeue) {
    concurrent::Queue<int> q;
    EXPECT_TRUE(q.enqueue(42));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(ConcurrentQueueTest, EmptyQueueReturnsNullopt) {
    concurrent::Queue<int> q;
    EXPECT_FALSE(q.tryDequeue().has_value());
}

TEST(ConcurrentQueueTest, FIFOOrdering) {
    concurrent::Queue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);

    EXPECT_EQ(*q.tryDequeue(), 1);
    EXPECT_EQ(*q.tryDequeue(), 2);
    EXPECT_EQ(*q.tryDequeue(), 3);
}

TEST(ConcurrentQueueTest, MoveEnqueue) {
    concurrent::Queue<std::string> q;
    std::string s = "hello";
    q.enqueue(std::move(s));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "hello");
}

TEST(ConcurrentQueueTest, SizeApprox) {
    concurrent::Queue<int> q;
    EXPECT_EQ(q.sizeApprox(), 0u);

    q.enqueue(1);
    q.enqueue(2);
    EXPECT_EQ(q.sizeApprox(), 2u);

    (void)q.tryDequeue();
    EXPECT_EQ(q.sizeApprox(), 1u);
}

TEST(ConcurrentQueueTest, EmptyCheck) {
    concurrent::Queue<int> q;
    EXPECT_TRUE(q.empty());

    q.enqueue(1);
    EXPECT_FALSE(q.empty());
}

TEST(ConcurrentQueueTest, EnqueueBulk) {
    concurrent::Queue<int> q;
    int items[] = {10, 20, 30};
    EXPECT_TRUE(q.enqueueBulk(items, 3));
    EXPECT_EQ(q.sizeApprox(), 3u);
}

TEST(ConcurrentQueueTest, TryDequeueBulk) {
    concurrent::Queue<int> q;
    q.enqueue(1);
    q.enqueue(2);
    q.enqueue(3);

    int items[5];
    size_t count = q.tryDequeueBulk(items, 5);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(items[0], 1);
    EXPECT_EQ(items[1], 2);
    EXPECT_EQ(items[2], 3);
}

TEST(ConcurrentQueueTest, TryDequeueBulkPartial) {
    concurrent::Queue<int> q;
    q.enqueue(1);
    q.enqueue(2);

    int items[5];
    size_t count = q.tryDequeueBulk(items, 1);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(items[0], 1);
}

// ===========================================================================
// BlockingQueue<T>
// ===========================================================================

TEST(BlockingQueueTest, EnqueueAndTryDequeue) {
    concurrent::BlockingQueue<int> q;
    EXPECT_TRUE(q.enqueue(42));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(BlockingQueueTest, TryDequeueEmptyReturnsNullopt) {
    concurrent::BlockingQueue<int> q;
    EXPECT_FALSE(q.tryDequeue().has_value());
}

TEST(BlockingQueueTest, DequeueWithTimeoutExpires) {
    concurrent::BlockingQueue<int> q;
    auto val = q.dequeueFor(std::chrono::milliseconds(50));
    EXPECT_FALSE(val.has_value());
}

TEST(BlockingQueueTest, DequeueWithTimeoutSucceeds) {
    concurrent::BlockingQueue<int> q;

    // Enqueue in background thread
    std::thread producer([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        q.enqueue(99);
    });

    auto val = q.dequeueFor(std::chrono::milliseconds(500));
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 99);

    producer.join();
}

TEST(BlockingQueueTest, CloseReturnsNulloptOnDequeue) {
    concurrent::BlockingQueue<int> q;

    std::thread closer([&q]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        q.close();
    });

    auto val = q.dequeue();
    EXPECT_FALSE(val.has_value());

    closer.join();
}

TEST(BlockingQueueTest, EnqueueAfterCloseReturnsFailure) {
    concurrent::BlockingQueue<int> q;
    q.close();
    EXPECT_FALSE(q.enqueue(42));
}

TEST(BlockingQueueTest, IsClosedReflectsState) {
    concurrent::BlockingQueue<int> q;
    EXPECT_FALSE(q.isClosed());

    q.close();
    EXPECT_TRUE(q.isClosed());
}

TEST(BlockingQueueTest, SizeApprox) {
    concurrent::BlockingQueue<int> q;
    EXPECT_EQ(q.sizeApprox(), 0u);

    q.enqueue(1);
    EXPECT_EQ(q.sizeApprox(), 1u);
}

TEST(BlockingQueueTest, EmptyCheck) {
    concurrent::BlockingQueue<int> q;
    EXPECT_TRUE(q.empty());

    q.enqueue(1);
    EXPECT_FALSE(q.empty());
}

TEST(BlockingQueueTest, MoveEnqueue) {
    concurrent::BlockingQueue<std::string> q;
    q.enqueue(std::string("test_move"));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "test_move");
}

// ===========================================================================
// SPSCQueue<T>
// ===========================================================================

TEST(SPSCQueueTest, TryEnqueueAndDequeue) {
    concurrent::SPSCQueue<int, 8> q;
    EXPECT_TRUE(q.tryEnqueue(42));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(SPSCQueueTest, EmptyDequeueReturnsNullopt) {
    concurrent::SPSCQueue<int, 8> q;
    EXPECT_FALSE(q.tryDequeue().has_value());
}

TEST(SPSCQueueTest, FIFOOrdering) {
    concurrent::SPSCQueue<int, 8> q;
    q.tryEnqueue(1);
    q.tryEnqueue(2);
    q.tryEnqueue(3);

    EXPECT_EQ(*q.tryDequeue(), 1);
    EXPECT_EQ(*q.tryDequeue(), 2);
    EXPECT_EQ(*q.tryDequeue(), 3);
}

TEST(SPSCQueueTest, FullQueueRejectsFurther) {
    // Capacity=4, but ring buffer uses one slot as sentinel → max 3 items
    concurrent::SPSCQueue<int, 4> q;
    q.tryEnqueue(1);
    q.tryEnqueue(2);
    q.tryEnqueue(3);

    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.tryEnqueue(4)); // Should fail
}

TEST(SPSCQueueTest, EmptyAndFullChecks) {
    concurrent::SPSCQueue<int, 4> q;
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());

    q.tryEnqueue(1);
    EXPECT_FALSE(q.empty());
}

TEST(SPSCQueueTest, SizeApprox) {
    concurrent::SPSCQueue<int, 8> q;
    EXPECT_EQ(q.sizeApprox(), 0u);

    q.tryEnqueue(10);
    q.tryEnqueue(20);
    EXPECT_EQ(q.sizeApprox(), 2u);

    (void)q.tryDequeue();
    EXPECT_EQ(q.sizeApprox(), 1u);
}

TEST(SPSCQueueTest, MoveEnqueue) {
    concurrent::SPSCQueue<std::string, 4> q;
    std::string s = "spsc_move";
    EXPECT_TRUE(q.tryEnqueue(std::move(s)));

    auto val = q.tryDequeue();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "spsc_move");
}

TEST(SPSCQueueTest, WrapAround) {
    concurrent::SPSCQueue<int, 4> q;

    // Fill and drain multiple times to exercise wrap-around
    for (int cycle = 0; cycle < 3; ++cycle) {
        q.tryEnqueue(cycle * 10 + 1);
        q.tryEnqueue(cycle * 10 + 2);

        auto v1 = q.tryDequeue();
        auto v2 = q.tryDequeue();
        ASSERT_TRUE(v1.has_value());
        ASSERT_TRUE(v2.has_value());
        EXPECT_EQ(*v1, cycle * 10 + 1);
        EXPECT_EQ(*v2, cycle * 10 + 2);
    }
}

// ===========================================================================
// Backend Info
// ===========================================================================

TEST(ConcurrentBackendTest, HasConcurrentQueueReturnsBool) {
    // Just verify it compiles and returns without crash
    (void)concurrent::hasConcurrentQueue();
    SUCCEED();
}

TEST(ConcurrentBackendTest, BackendNameNonNull) {
    auto name = concurrent::backendName();
    EXPECT_NE(name, nullptr);
    EXPECT_GT(std::strlen(name), 0u);
}

} // namespace testing
} // namespace makine
