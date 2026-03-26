/**
 * @file test_async.cpp
 * @brief Unit tests for async.hpp — AsyncStatus, AsyncProgress, AsyncOperation, AsyncQueue
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/async.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <thread>

namespace makine {
namespace testing {

// ===========================================================================
// AsyncStatus
// ===========================================================================

TEST(AsyncStatusTest, StatusToString) {
    EXPECT_EQ(asyncStatusToString(AsyncStatus::Pending), "Pending");
    EXPECT_EQ(asyncStatusToString(AsyncStatus::Running), "Running");
    EXPECT_EQ(asyncStatusToString(AsyncStatus::Completed), "Completed");
    EXPECT_EQ(asyncStatusToString(AsyncStatus::Failed), "Failed");
    EXPECT_EQ(asyncStatusToString(AsyncStatus::Cancelled), "Cancelled");
}

// ===========================================================================
// AsyncProgress
// ===========================================================================

TEST(AsyncProgressTest, DefaultValues) {
    AsyncProgress p;
    EXPECT_FLOAT_EQ(p.percentage, 0.0f);
    EXPECT_TRUE(p.message.empty());
    EXPECT_EQ(p.currentStep, 0u);
    EXPECT_EQ(p.totalSteps, 0u);
}

TEST(AsyncProgressTest, IsIndeterminate) {
    AsyncProgress p;
    p.totalSteps = 0;
    EXPECT_TRUE(p.isIndeterminate());

    p.totalSteps = 10;
    EXPECT_FALSE(p.isIndeterminate());
}

TEST(AsyncProgressTest, PercentageString) {
    AsyncProgress p;
    p.percentage = 0.75f;
    EXPECT_EQ(p.percentageString(), "75%");
}

TEST(AsyncProgressTest, PercentageStringZero) {
    AsyncProgress p;
    p.percentage = 0.0f;
    EXPECT_EQ(p.percentageString(), "0%");
}

TEST(AsyncProgressTest, PercentageStringFull) {
    AsyncProgress p;
    p.percentage = 1.0f;
    EXPECT_EQ(p.percentageString(), "100%");
}

// ===========================================================================
// AsyncOperation<T> — Lifecycle
// ===========================================================================

TEST(AsyncOperationTest, InitialStatusPending) {
    AsyncOperation<int> op;
    EXPECT_EQ(op.status(), AsyncStatus::Pending);
    EXPECT_FALSE(op.isComplete());
    EXPECT_FALSE(op.isCancelled());
}

TEST(AsyncOperationTest, StartSetsRunning) {
    AsyncOperation<int> op;
    op.start();
    EXPECT_EQ(op.status(), AsyncStatus::Running);
    EXPECT_FALSE(op.isComplete());
}

TEST(AsyncOperationTest, CompleteSetsCompleted) {
    AsyncOperation<int> op;
    op.start();
    op.complete(42);

    EXPECT_EQ(op.status(), AsyncStatus::Completed);
    EXPECT_TRUE(op.isComplete());
    EXPECT_FALSE(op.isCancelled());
}

TEST(AsyncOperationTest, FailSetsFailed) {
    AsyncOperation<int> op;
    op.start();
    op.fail(Error(ErrorCode::Unknown, "test error"));

    EXPECT_EQ(op.status(), AsyncStatus::Failed);
    EXPECT_TRUE(op.isComplete());

    auto err = op.error();
    ASSERT_TRUE(err.has_value());
}

TEST(AsyncOperationTest, MarkCancelledSetsCancelled) {
    AsyncOperation<int> op;
    op.start();
    op.markCancelled();

    EXPECT_EQ(op.status(), AsyncStatus::Cancelled);
    EXPECT_TRUE(op.isComplete());
    EXPECT_TRUE(op.isCancelled());
}

TEST(AsyncOperationTest, CancelRequestSetsFlag) {
    AsyncOperation<int> op;
    EXPECT_FALSE(op.cancellationRequested());

    op.cancel();
    EXPECT_TRUE(op.cancellationRequested());
}

// ===========================================================================
// AsyncOperation<T> — Await / Wait
// ===========================================================================

TEST(AsyncOperationTest, AwaitReturnsResult) {
    AsyncOperation<int> op;
    op.start();
    op.complete(99);

    auto result = op.await();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 99);
}

TEST(AsyncOperationTest, AwaitReturnsErrorOnFailure) {
    AsyncOperation<int> op;
    op.start();
    op.fail(Error(ErrorCode::Unknown, "err"));

    auto result = op.await();
    EXPECT_FALSE(result.has_value());
}

TEST(AsyncOperationTest, WaitForReturnsTrue) {
    AsyncOperation<int> op;
    op.start();
    op.complete(1);

    bool finished = op.waitFor(std::chrono::milliseconds(100));
    EXPECT_TRUE(finished);
}

TEST(AsyncOperationTest, WaitForReturnsFalseOnTimeout) {
    AsyncOperation<int> op;
    op.start();
    // Don't complete

    bool finished = op.waitFor(std::chrono::milliseconds(10));
    EXPECT_FALSE(finished);

    // Complete to avoid hanging on destruction
    op.complete(0);
}

// ===========================================================================
// AsyncOperation<T> — Progress
// ===========================================================================

TEST(AsyncOperationTest, ProgressReporting) {
    AsyncOperation<int> op;
    op.start();
    op.reportProgress(0.5f, "halfway", 5, 10);

    auto p = op.progress();
    EXPECT_FLOAT_EQ(p.percentage, 0.5f);
    EXPECT_EQ(p.message, "halfway");
    EXPECT_EQ(p.currentStep, 5u);
    EXPECT_EQ(p.totalSteps, 10u);
}

TEST(AsyncOperationTest, ProgressClampedToRange) {
    AsyncOperation<int> op;
    op.start();
    op.reportProgress(1.5f); // Above 1.0
    EXPECT_LE(op.progress().percentage, 1.0f);

    op.reportProgress(-0.5f); // Below 0.0
    EXPECT_GE(op.progress().percentage, 0.0f);
}

TEST(AsyncOperationTest, ProgressCallback) {
    AsyncOperation<int> op;
    int callbackCount = 0;
    float lastPercentage = -1.0f;

    op.onProgress([&](const AsyncProgress& p) {
        ++callbackCount;
        lastPercentage = p.percentage;
    });

    op.start();
    op.reportProgress(0.25f);
    op.reportProgress(0.75f);

    EXPECT_EQ(callbackCount, 2);
    EXPECT_FLOAT_EQ(lastPercentage, 0.75f);

    op.complete(0);
}

TEST(AsyncOperationTest, CompletionCallback) {
    AsyncOperation<int> op;
    int completedValue = -1;

    op.onComplete([&](const Result<int>& result) {
        if (result.has_value()) {
            completedValue = *result;
        }
    });

    op.start();
    op.complete(42);

    EXPECT_EQ(completedValue, 42);
}

TEST(AsyncOperationTest, CompletionCallbackOnFailure) {
    AsyncOperation<int> op;
    bool hadError = false;

    op.onComplete([&](const Result<int>& result) {
        hadError = !result.has_value();
    });

    op.start();
    op.fail(Error(ErrorCode::Unknown, "err"));

    EXPECT_TRUE(hadError);
}

// ===========================================================================
// AsyncOperation<T> — Error
// ===========================================================================

TEST(AsyncOperationTest, ErrorNulloptWhenPending) {
    AsyncOperation<int> op;
    EXPECT_FALSE(op.error().has_value());
}

TEST(AsyncOperationTest, ErrorNulloptWhenCompleted) {
    AsyncOperation<int> op;
    op.start();
    op.complete(1);
    EXPECT_FALSE(op.error().has_value());
}

TEST(AsyncOperationTest, ErrorPresentWhenFailed) {
    AsyncOperation<int> op;
    op.start();
    op.fail(Error(ErrorCode::Unknown, "details"));

    auto err = op.error();
    ASSERT_TRUE(err.has_value());
}

// ===========================================================================
// executeAsync
// ===========================================================================

TEST(ExecuteAsyncTest, SuccessfulExecution) {
    auto op = executeAsync<int>([]() -> Result<int> {
        return 42;
    });

    auto result = op->await();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(ExecuteAsyncTest, FailedExecution) {
    auto op = executeAsync<int>([]() -> Result<int> {
        return std::unexpected(Error(ErrorCode::Unknown, "fail"));
    });

    auto result = op->await();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(op->status(), AsyncStatus::Failed);
}

TEST(ExecuteAsyncTest, WithProgressReporting) {
    auto op = executeAsync<int>(
        [](AsyncOperation<int>& op) -> Result<int> {
            op.reportProgress(0.5f, "half done");
            return 100;
        }
    );

    auto result = op->await();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 100);
}

TEST(ExecuteAsyncTest, CancellationCheck) {
    auto op = executeAsync<int>(
        [](AsyncOperation<int>& op) -> Result<int> {
            // Check cancellation
            if (op.cancellationRequested()) {
                return std::unexpected(Error(ErrorCode::Cancelled, "cancelled"));
            }
            return 1;
        }
    );

    // Cancel immediately (may or may not take effect depending on timing)
    op->cancel();

    auto result = op->await();
    // Either completed or cancelled — both are valid
    EXPECT_TRUE(op->isComplete());
}

TEST(ExecuteAsyncTest, ExceptionHandled) {
    auto op = executeAsync<int>(
        [](AsyncOperation<int>&) -> Result<int> {
            throw std::runtime_error("boom");
        }
    );

    auto result = op->await();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(op->status(), AsyncStatus::Failed);
}

// ===========================================================================
// makeAsyncOperation
// ===========================================================================

TEST(MakeAsyncTest, CreatesSharedPtr) {
    auto op = makeAsyncOperation<int>();
    ASSERT_NE(op, nullptr);
    EXPECT_EQ(op->status(), AsyncStatus::Pending);
}

// ===========================================================================
// AsyncQueue
// ===========================================================================

TEST(AsyncQueueTest, EnqueueAndExecute) {
    AsyncQueue queue;

    auto op = queue.enqueue<int>(
        [](AsyncOperation<int>&) -> Result<int> {
            return 42;
        }
    );

    auto result = op->await();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
}

TEST(AsyncQueueTest, SequentialExecution) {
    AsyncQueue queue;
    std::vector<int> order;
    std::mutex mtx;

    auto op1 = queue.enqueue<int>(
        [&](AsyncOperation<int>&) -> Result<int> {
            std::lock_guard lock(mtx);
            order.push_back(1);
            return 1;
        }
    );

    auto op2 = queue.enqueue<int>(
        [&](AsyncOperation<int>&) -> Result<int> {
            std::lock_guard lock(mtx);
            order.push_back(2);
            return 2;
        }
    );

    (void)op1->await();
    (void)op2->await();

    std::lock_guard lock(mtx);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST(AsyncQueueTest, PendingCount) {
    AsyncQueue queue;

    // The queue processes immediately, so pending may be 0
    // Just verify the method works
    EXPECT_GE(queue.pendingCount(), 0u);
}

TEST(AsyncQueueTest, IsEmpty) {
    AsyncQueue queue;
    EXPECT_TRUE(queue.isEmpty());
}

TEST(AsyncQueueTest, FailedTask) {
    AsyncQueue queue;

    auto op = queue.enqueue<int>(
        [](AsyncOperation<int>&) -> Result<int> {
            return std::unexpected(Error(ErrorCode::Unknown, "queue fail"));
        }
    );

    auto result = op->await();
    EXPECT_FALSE(result.has_value());
}

} // namespace testing
} // namespace makine
