/**
 * @file test_error.cpp
 * @brief Unit tests for error.hpp — Error, ErrorCode, ErrorCollector, retry, getSuggestions
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/error.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace makine {
namespace testing {

// ===========================================================================
// ErrorCode
// ===========================================================================

TEST(ErrorCodeTest, SuccessCodeIsZero) {
    EXPECT_EQ(static_cast<int>(ErrorCode::Success), 0);
}

TEST(ErrorCodeTest, ErrorMessageReturnsNonEmpty) {
    auto msg = errorMessage(ErrorCode::FileNotFound);
    EXPECT_FALSE(msg.empty());
    EXPECT_NE(msg, "Unknown error code");
}

TEST(ErrorCodeTest, ErrorMessageUnknownCode) {
    auto msg = errorMessage(static_cast<ErrorCode>(99999));
    EXPECT_EQ(msg, "Unknown error code");
}

TEST(ErrorCodeTest, AllCategoriesHaveMessages) {
    // Spot-check each category range
    EXPECT_NE(errorMessage(ErrorCode::Unknown), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::FileNotFound), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::NetworkError), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::GameNotFound), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::PatchFailed), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::SignatureInvalid), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::ParseError), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::RuntimeInstallFailed), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::DatabaseError), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::InvalidOffset), "Unknown error code");
    EXPECT_NE(errorMessage(ErrorCode::InvalidConfiguration), "Unknown error code");
}

// ===========================================================================
// Error — Construction
// ===========================================================================

class ErrorTest : public ::testing::Test {};

TEST_F(ErrorTest, DefaultConstructIsSuccess) {
    Error err;
    EXPECT_EQ(err.code(), ErrorCode::Success);
    EXPECT_TRUE(err.isSuccess());
    EXPECT_FALSE(err.isError());
    EXPECT_FALSE(static_cast<bool>(err));
}

TEST_F(ErrorTest, ConstructWithCode) {
    Error err(ErrorCode::FileNotFound);
    EXPECT_EQ(err.code(), ErrorCode::FileNotFound);
    EXPECT_TRUE(err.isError());
    EXPECT_FALSE(err.isSuccess());
    EXPECT_TRUE(static_cast<bool>(err));
}

TEST_F(ErrorTest, ConstructWithCodeAndMessage) {
    Error err(ErrorCode::PatchFailed, "binary patch failed");
    EXPECT_EQ(err.code(), ErrorCode::PatchFailed);
    EXPECT_EQ(err.message(), "binary patch failed");
}

TEST_F(ErrorTest, ConstructWithCodeDefaultMessage) {
    Error err(ErrorCode::FileNotFound);
    EXPECT_EQ(err.message(), "File not found");
}

TEST_F(ErrorTest, SourceLocationCaptured) {
    Error err(ErrorCode::Unknown, "test");
    EXPECT_NE(err.line(), 0u);
    EXPECT_TRUE(std::string(err.function()).size() > 0);
    EXPECT_TRUE(std::string(err.file()).size() > 0);
}

// ===========================================================================
// Error — Context Chain
// ===========================================================================

TEST_F(ErrorTest, WithContextAddsToChain) {
    Error err(ErrorCode::PatchFailed, "failed");
    err.withContext("unity handler");
    err.withContext("IL2CPP patching");

    EXPECT_EQ(err.contextChain().size(), 2u);
    EXPECT_EQ(err.contextChain()[0], "unity handler");
    EXPECT_EQ(err.contextChain()[1], "IL2CPP patching");
}

TEST_F(ErrorTest, WithFileStoresPath) {
    Error err(ErrorCode::FileNotFound, "missing");
    err.withFile(std::filesystem::path("C:/Games/test.pak"));

    ASSERT_TRUE(err.affectedFile().has_value());
    EXPECT_EQ(err.affectedFile()->string(), "C:/Games/test.pak");
}

TEST_F(ErrorTest, WithGameStoresName) {
    Error err(ErrorCode::GameNotFound);
    err.withGame("Elden Ring");

    ASSERT_TRUE(err.gameName().has_value());
    EXPECT_EQ(*err.gameName(), "Elden Ring");
}

TEST_F(ErrorTest, WithDetailAddsKeyValue) {
    Error err(ErrorCode::Unknown, "test");
    err.withDetail("version", "1.0.0");
    err.withDetail("engine", "Unity");

    EXPECT_EQ(err.details().size(), 2u);
    EXPECT_EQ(err.details().at("version"), "1.0.0");
    EXPECT_EQ(err.details().at("engine"), "Unity");
}

TEST_F(ErrorTest, FluentChainingRvalue) {
    auto err = Error(ErrorCode::PatchFailed, "failed")
        .withContext("context1")
        .withFile(std::filesystem::path("test.txt"))
        .withGame("TestGame")
        .withDetail("key", "value");

    EXPECT_EQ(err.code(), ErrorCode::PatchFailed);
    EXPECT_EQ(err.contextChain().size(), 1u);
    EXPECT_TRUE(err.affectedFile().has_value());
    EXPECT_TRUE(err.gameName().has_value());
    EXPECT_EQ(err.details().size(), 1u);
}

// ===========================================================================
// Error — Message Formatting
// ===========================================================================

TEST_F(ErrorTest, FullMessageContainsLocation) {
    Error err(ErrorCode::FileNotFound, "missing file");
    auto full = err.fullMessage();
    EXPECT_NE(full.find("missing file"), std::string::npos);
    EXPECT_NE(full.find("["), std::string::npos);
}

TEST_F(ErrorTest, FullMessageSuccessIsSuccess) {
    Error err;
    EXPECT_EQ(err.fullMessage(), "Success");
}

TEST_F(ErrorTest, DetailedMessageIncludesContext) {
    auto err = Error(ErrorCode::PatchFailed, "failed")
        .withContext("context A")
        .withGame("TestGame")
        .withFile(std::filesystem::path("test.pak"))
        .withDetail("step", "3");

    auto detailed = err.detailedMessage();
    EXPECT_NE(detailed.find("context A"), std::string::npos);
    EXPECT_NE(detailed.find("TestGame"), std::string::npos);
    EXPECT_NE(detailed.find("test.pak"), std::string::npos);
    EXPECT_NE(detailed.find("step"), std::string::npos);
}

TEST_F(ErrorTest, DetailedMessageSuccessIsSuccess) {
    Error err;
    EXPECT_EQ(err.detailedMessage(), "Success");
}

TEST_F(ErrorTest, ToJsonContainsFields) {
    auto err = Error(ErrorCode::FileNotFound, "not found")
        .withGame("TestGame")
        .withContext("scan");

    auto json = err.toJson();
    EXPECT_NE(json.find("\"code\":100"), std::string::npos);
    EXPECT_NE(json.find("not found"), std::string::npos);
    EXPECT_NE(json.find("TestGame"), std::string::npos);
    EXPECT_NE(json.find("context"), std::string::npos);
    EXPECT_NE(json.find("location"), std::string::npos);
}

TEST_F(ErrorTest, ToJsonEscapesSpecialChars) {
    Error err(ErrorCode::Unknown, "message with \"quotes\" and \\backslash");
    auto json = err.toJson();
    EXPECT_NE(json.find("\\\"quotes\\\""), std::string::npos);
    EXPECT_NE(json.find("\\\\backslash"), std::string::npos);
}

// ===========================================================================
// Exception
// ===========================================================================

TEST(ExceptionTest, ThrowAndCatch) {
    try {
        throw Exception(Error(ErrorCode::PatchFailed, "patch error"));
    } catch (const Exception& e) {
        EXPECT_EQ(e.code(), ErrorCode::PatchFailed);
        EXPECT_STREQ(e.what(), "patch error");
    }
}

TEST(ExceptionTest, CatchAsStdException) {
    try {
        throw Exception(Error(ErrorCode::Unknown, "unknown"));
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "unknown");
    }
}

// ===========================================================================
// Result<T>
// ===========================================================================

TEST(ResultTest, SuccessResult) {
    Result<int> r = 42;
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);
}

TEST(ResultTest, ErrorResult) {
    Result<int> r = std::unexpected(Error(ErrorCode::InvalidArgument, "bad arg"));
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), ErrorCode::InvalidArgument);
}

TEST(ResultTest, VoidResultSuccess) {
    VoidResult r;
    EXPECT_TRUE(r.has_value());
}

TEST(ResultTest, VoidResultError) {
    VoidResult r = std::unexpected(Error(ErrorCode::DiskFull));
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), ErrorCode::DiskFull);
}

// ===========================================================================
// ErrorCollector
// ===========================================================================

class ErrorCollectorTest : public ::testing::Test {
protected:
    ErrorCollector collector_;
};

TEST_F(ErrorCollectorTest, EmptyByDefault) {
    EXPECT_FALSE(collector_.hasErrors());
    EXPECT_EQ(collector_.count(), 0u);
}

TEST_F(ErrorCollectorTest, AddError) {
    collector_.add(Error(ErrorCode::FileNotFound, "file1"));
    collector_.add(Error(ErrorCode::FileNotFound, "file2"));
    EXPECT_TRUE(collector_.hasErrors());
    EXPECT_EQ(collector_.count(), 2u);
}

TEST_F(ErrorCollectorTest, AddSuccessIsIgnored) {
    collector_.add(Error());
    EXPECT_FALSE(collector_.hasErrors());
    EXPECT_EQ(collector_.count(), 0u);
}

TEST_F(ErrorCollectorTest, AddWithCodeAndMessage) {
    collector_.add(ErrorCode::Timeout, "timed out");
    EXPECT_TRUE(collector_.hasErrors());
    EXPECT_EQ(collector_.errors()[0].code(), ErrorCode::Timeout);
}

TEST_F(ErrorCollectorTest, AddIfFailed) {
    Result<int> ok = 42;
    Result<int> bad = std::unexpected(Error(ErrorCode::Unknown, "err"));

    collector_.addIfFailed(ok);
    EXPECT_FALSE(collector_.hasErrors());

    collector_.addIfFailed(bad);
    EXPECT_TRUE(collector_.hasErrors());
    EXPECT_EQ(collector_.count(), 1u);
}

TEST_F(ErrorCollectorTest, HasFatalErrors) {
    collector_.add(Error(ErrorCode::FileNotFound, "not fatal"));
    EXPECT_FALSE(collector_.hasFatalErrors());

    collector_.add(Error(ErrorCode::SecurityViolation, "fatal"));
    EXPECT_TRUE(collector_.hasFatalErrors());
}

TEST_F(ErrorCollectorTest, HasFatalErrorsTamperingDetected) {
    collector_.add(Error(ErrorCode::TamperingDetected, "tampered"));
    EXPECT_TRUE(collector_.hasFatalErrors());
}

TEST_F(ErrorCollectorTest, HasFatalErrorsFileCorrupted) {
    collector_.add(Error(ErrorCode::FileCorrupted, "corrupt"));
    EXPECT_TRUE(collector_.hasFatalErrors());
}

TEST_F(ErrorCollectorTest, ClearRemovesAll) {
    collector_.add(ErrorCode::Unknown, "e1");
    collector_.add(ErrorCode::Unknown, "e2");
    collector_.clear();
    EXPECT_FALSE(collector_.hasErrors());
    EXPECT_EQ(collector_.count(), 0u);
}

TEST_F(ErrorCollectorTest, AggregatedSingleError) {
    collector_.add(Error(ErrorCode::FileNotFound, "single"));
    auto agg = collector_.aggregated();
    EXPECT_EQ(agg.code(), ErrorCode::FileNotFound);
    EXPECT_EQ(agg.message(), "single");
}

TEST_F(ErrorCollectorTest, AggregatedMultipleErrors) {
    collector_.add(ErrorCode::FileNotFound, "err1");
    collector_.add(ErrorCode::Timeout, "err2");
    auto agg = collector_.aggregated(ErrorCode::Unknown);
    EXPECT_EQ(agg.code(), ErrorCode::Unknown);
    EXPECT_NE(agg.message().find("2 errors occurred"), std::string::npos);
    EXPECT_NE(agg.message().find("err1"), std::string::npos);
    EXPECT_NE(agg.message().find("err2"), std::string::npos);
}

TEST_F(ErrorCollectorTest, AggregatedEmptyIsSuccess) {
    auto agg = collector_.aggregated();
    EXPECT_EQ(agg.code(), ErrorCode::Success);
}

TEST_F(ErrorCollectorTest, ToResultSuccess) {
    auto r = collector_.toResult();
    EXPECT_TRUE(r.has_value());
}

TEST_F(ErrorCollectorTest, ToResultWithErrors) {
    collector_.add(ErrorCode::Unknown, "err");
    auto r = collector_.toResult();
    EXPECT_FALSE(r.has_value());
}

// ===========================================================================
// Retry
// ===========================================================================

TEST(RetryTest, SucceedsOnFirstTry) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        return 42;
    }, 3, std::chrono::milliseconds(1));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_EQ(attempts, 1);
}

TEST(RetryTest, SucceedsOnSecondTry) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        if (attempts < 2) {
            return std::unexpected(Error(ErrorCode::NetworkError, "temp fail"));
        }
        return 99;
    }, 3, std::chrono::milliseconds(1));

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 99);
    EXPECT_EQ(attempts, 2);
}

TEST(RetryTest, ExhaustsAllAttempts) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        return std::unexpected(Error(ErrorCode::NetworkError, "always fail"));
    }, 3, std::chrono::milliseconds(1));

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 3);
    EXPECT_NE(result.error().details().find("retry_attempts"), result.error().details().end());
}

TEST(RetryTest, DoesNotRetryOnInvalidArgument) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        return std::unexpected(Error(ErrorCode::InvalidArgument, "bad arg"));
    }, 3, std::chrono::milliseconds(1));

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 1);
}

TEST(RetryTest, DoesNotRetryOnCancelled) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        return std::unexpected(Error(ErrorCode::Cancelled, "cancelled"));
    }, 3, std::chrono::milliseconds(1));

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 1);
}

TEST(RetryTest, DoesNotRetryOnSecurityViolation) {
    int attempts = 0;
    auto result = retry([&]() -> Result<int> {
        ++attempts;
        return std::unexpected(Error(ErrorCode::SecurityViolation, "security"));
    }, 3, std::chrono::milliseconds(1));

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 1);
}

// ===========================================================================
// RetryIf
// ===========================================================================

TEST(RetryIfTest, RetryOnlyOnNetworkErrors) {
    int attempts = 0;
    auto result = retryIf(
        [&]() -> Result<int> {
            ++attempts;
            return std::unexpected(Error(ErrorCode::Timeout, "timeout"));
        },
        [](const Error& err) {
            return err.code() == ErrorCode::Timeout;
        },
        3, std::chrono::milliseconds(1)
    );

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 3);
}

TEST(RetryIfTest, DoNotRetryWhenPredicateFalse) {
    int attempts = 0;
    auto result = retryIf(
        [&]() -> Result<int> {
            ++attempts;
            return std::unexpected(Error(ErrorCode::FileNotFound));
        },
        [](const Error& err) {
            return err.code() == ErrorCode::Timeout;
        },
        3, std::chrono::milliseconds(1)
    );

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(attempts, 1);
}

// ===========================================================================
// getSuggestions
// ===========================================================================

TEST(ErrorSuggestionTest, FileAccessDeniedHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::FileAccessDenied);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, DiskFullHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::DiskFull);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, InsufficientDiskSpaceHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::InsufficientDiskSpace);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, NetworkErrorHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::NetworkError);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, SignatureInvalidHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::SignatureInvalid);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, GameNotFoundHasSuggestions) {
    auto suggestions = getSuggestions(ErrorCode::GameNotFound);
    EXPECT_GE(suggestions.size(), 1u);
}

TEST(ErrorSuggestionTest, UnknownCodeHasDefaultSuggestion) {
    auto suggestions = getSuggestions(ErrorCode::InvalidConfiguration);
    EXPECT_GE(suggestions.size(), 1u);
    // Default suggestion should have "retry" action
    bool hasRetry = false;
    for (const auto& s : suggestions) {
        if (s.action == "retry") hasRetry = true;
    }
    EXPECT_TRUE(hasRetry);
}

TEST(ErrorSuggestionTest, SuggestionHasDescription) {
    auto suggestions = getSuggestions(ErrorCode::FileLocked);
    EXPECT_GE(suggestions.size(), 1u);
    EXPECT_FALSE(suggestions[0].description.empty());
    EXPECT_FALSE(suggestions[0].action.empty());
}

// ===========================================================================
// MAKINE_ERROR macro
// ===========================================================================

TEST(ErrorMacroTest, MakineError) {
    auto err = MAKINE_ERROR(FileNotFound, "test macro");
    EXPECT_EQ(err.code(), ErrorCode::FileNotFound);
    EXPECT_EQ(err.message(), "test macro");
}

TEST(ErrorMacroTest, MakineSuccess) {
    auto err = MAKINE_SUCCESS;
    EXPECT_EQ(err.code(), ErrorCode::Success);
    EXPECT_TRUE(err.isSuccess());
}

} // namespace testing
} // namespace makine
