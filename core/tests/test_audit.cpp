/**
 * @file test_audit.cpp
 * @brief Unit tests for audit.hpp — AuditEvent, AuditLogger, SecureBuffer, secureZero, secureCompare
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/audit.hpp>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace makine {
namespace testing {

namespace fs = std::filesystem;

// ===========================================================================
// AuditEvent
// ===========================================================================

TEST(AuditEventTest, DefaultValues) {
    AuditEvent event;
    EXPECT_EQ(event.severity, AuditSeverity::Info);
    EXPECT_EQ(event.category, AuditCategory::SystemEvent);
    EXPECT_TRUE(event.success);
    EXPECT_TRUE(event.action.empty());
    EXPECT_TRUE(event.target.empty());
}

TEST(AuditEventTest, ToLogStringNonEmpty) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.severity = AuditSeverity::Warning;
    event.category = AuditCategory::FileAccess;
    event.action = "read";
    event.target = "/tmp/test.txt";
    event.success = true;

    auto log = event.toLogString();
    EXPECT_FALSE(log.empty());
    EXPECT_NE(log.find("WARN"), std::string::npos);
    EXPECT_NE(log.find("FILE"), std::string::npos);
    EXPECT_NE(log.find("read"), std::string::npos);
}

TEST(AuditEventTest, ToLogStringWithOptionalFields) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.action = "login";
    event.userId = "user123";
    event.sessionId = "sess-abc";

    auto log = event.toLogString();
    EXPECT_NE(log.find("user=\"user123\""), std::string::npos);
    EXPECT_NE(log.find("session=\"sess-abc\""), std::string::npos);
}

TEST(AuditEventTest, ToJsonNonEmpty) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.severity = AuditSeverity::Critical;
    event.category = AuditCategory::SignatureVerify;
    event.action = "verify";
    event.target = "pkg-123";
    event.success = false;

    auto json = event.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"severity\":\"CRIT\""), std::string::npos);
    EXPECT_NE(json.find("\"category\":\"SIG\""), std::string::npos);
    EXPECT_NE(json.find("\"success\":false"), std::string::npos);
}

TEST(AuditEventTest, ToJsonEscapesSpecialChars) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.action = "test";
    event.details = "line1\nline2\ttab\"quote";

    auto json = event.toJson();
    EXPECT_NE(json.find("\\n"), std::string::npos);
    EXPECT_NE(json.find("\\t"), std::string::npos);
    EXPECT_NE(json.find("\\\"quote"), std::string::npos);
}

TEST(AuditEventTest, ToJsonWithSourceIp) {
    AuditEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.action = "request";
    event.sourceIp = "192.168.1.1";

    auto json = event.toJson();
    EXPECT_NE(json.find("\"sourceIp\":\"192.168.1.1\""), std::string::npos);
}

// ===========================================================================
// AuditLogger
// ===========================================================================

class AuditLoggerTest : public ::testing::Test {
protected:
    fs::path testDir_;

    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makine_audit_tests";
        fs::create_directories(testDir_);

        // Configure with in-memory buffer, no file output
        AuditConfig config;
        config.enabled = true;
        config.logToFile = false;
        config.logToConsole = false;
        config.inMemoryBufferSize = 100;
        AuditLogger::instance().configure(config);
        AuditLogger::instance().clearCallbacks();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }
};

TEST_F(AuditLoggerTest, LogEventIncrementsCount) {
    auto before = AuditLogger::instance().totalEvents();

    AuditEvent event;
    event.action = "test";
    AuditLogger::instance().log(std::move(event));

    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogFileAccessConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logFileAccess("/tmp/test.txt", "read", true, "ok");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogNetworkRequestConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logNetworkRequest("https://example.com", "GET", true);
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogSignatureVerificationConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logSignatureVerification("pkg-123", true, "valid");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogPatchOperationConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logPatchOperation("1245620", true, "apply", "all files");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogConfigChangeConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logConfigChange("language", "en", "tr");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogDataExportConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logDataExport("metrics", testDir_ / "export.json");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, LogSystemEventConvenience) {
    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logSystemEvent("startup", "version 0.1.0");
    EXPECT_GT(AuditLogger::instance().totalEvents(), before);
}

TEST_F(AuditLoggerTest, GetRecentEventsReturnsEvents) {
    AuditLogger::logSystemEvent("event1");
    AuditLogger::logSystemEvent("event2");
    AuditLogger::logSystemEvent("event3");

    auto events = AuditLogger::instance().getRecentEvents(10);
    EXPECT_GE(events.size(), 3u);
}

TEST_F(AuditLoggerTest, GetRecentEventsLimitedCount) {
    for (int i = 0; i < 10; ++i) {
        AuditLogger::logSystemEvent("bulk_event");
    }

    auto events = AuditLogger::instance().getRecentEvents(3);
    EXPECT_LE(events.size(), 3u);
}

TEST_F(AuditLoggerTest, GetEventsByCategoryFilters) {
    AuditLogger::logFileAccess("/tmp/a.txt", "read");
    AuditLogger::logNetworkRequest("https://example.com", "GET");
    AuditLogger::logFileAccess("/tmp/b.txt", "write");

    auto fileEvents = AuditLogger::instance().getEventsByCategory(
        AuditCategory::FileAccess);
    for (const auto& e : fileEvents) {
        EXPECT_EQ(e.category, AuditCategory::FileAccess);
    }
}

TEST_F(AuditLoggerTest, CallbackInvoked) {
    int callbackCount = 0;
    AuditLogger::instance().addCallback([&](const AuditEvent&) {
        ++callbackCount;
    });

    AuditLogger::logSystemEvent("callback_test");
    EXPECT_GE(callbackCount, 1);
}

TEST_F(AuditLoggerTest, DisabledLoggerSkipsEvents) {
    AuditConfig config;
    config.enabled = false;
    config.logToFile = false;
    AuditLogger::instance().configure(config);
        AuditLogger::instance().clearCallbacks();

    auto before = AuditLogger::instance().totalEvents();
    AuditLogger::logSystemEvent("should_skip");
    EXPECT_EQ(AuditLogger::instance().totalEvents(), before);

    // Re-enable for other tests
    config.enabled = true;
    AuditLogger::instance().configure(config);
        AuditLogger::instance().clearCallbacks();
}

TEST_F(AuditLoggerTest, ConvenienceFunction) {
    auto& logger = audit();
    EXPECT_EQ(&logger, &AuditLogger::instance());
}

// ===========================================================================
// SecureBuffer
// ===========================================================================

TEST(SecureBufferTest, CreateWithSize) {
    SecureBuffer<char> buf(64);
    EXPECT_EQ(buf.size(), 64u);
    EXPECT_FALSE(buf.empty());
    EXPECT_NE(buf.data(), nullptr);
}

TEST(SecureBufferTest, InitializedToZero) {
    SecureBuffer<uint8_t> buf(32);
    for (size_t i = 0; i < buf.size(); ++i) {
        EXPECT_EQ(buf[i], 0);
    }
}

TEST(SecureBufferTest, CreateFromExistingData) {
    const char source[] = "secret_key";
    SecureBuffer<char> buf(source, strlen(source));
    EXPECT_EQ(buf.size(), strlen(source));
    EXPECT_EQ(std::string_view(buf.data(), buf.size()), "secret_key");
}

TEST(SecureBufferTest, ArrayAccess) {
    SecureBuffer<uint8_t> buf(4);
    buf[0] = 0xDE;
    buf[1] = 0xAD;
    buf[2] = 0xBE;
    buf[3] = 0xEF;

    EXPECT_EQ(buf[0], 0xDE);
    EXPECT_EQ(buf[3], 0xEF);
}

TEST(SecureBufferTest, ClearZerosMemory) {
    SecureBuffer<uint8_t> buf(16);
    buf[0] = 0xFF;
    buf[15] = 0xFF;

    buf.clear();

    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[15], 0);
}

TEST(SecureBufferTest, MoveConstructor) {
    SecureBuffer<char> a(32);
    a[0] = 'X';

    SecureBuffer<char> b(std::move(a));
    EXPECT_EQ(b.size(), 32u);
    EXPECT_EQ(b[0], 'X');
    EXPECT_EQ(a.size(), 0u);
}

TEST(SecureBufferTest, MoveAssignment) {
    SecureBuffer<char> a(16);
    a[0] = 'A';

    SecureBuffer<char> b(8);
    b = std::move(a);

    EXPECT_EQ(b.size(), 16u);
    EXPECT_EQ(b[0], 'A');
    EXPECT_EQ(a.size(), 0u);
}

TEST(SecureBufferTest, ViewForCharBuffer) {
    const char data[] = "Hello";
    SecureBuffer<char> buf(data, 5);

    auto view = buf.view();
    EXPECT_EQ(view, "Hello");
}

TEST(SecureBufferTest, EmptyAfterMoveOut) {
    SecureBuffer<char> a(32);
    SecureBuffer<char> b(std::move(a));
    EXPECT_TRUE(a.empty());
}

// ===========================================================================
// SecureString / SecureBytes aliases
// ===========================================================================

TEST(SecureAliasTest, SecureStringWorks) {
    SecureString str(64);
    EXPECT_EQ(str.size(), 64u);
}

TEST(SecureAliasTest, SecureBytesWorks) {
    SecureBytes bytes(128);
    EXPECT_EQ(bytes.size(), 128u);
}

// ===========================================================================
// secureZero
// ===========================================================================

TEST(SecureZeroTest, ZerosMemory) {
    uint8_t buffer[16];
    std::memset(buffer, 0xFF, sizeof(buffer));

    secureZero(buffer, sizeof(buffer));

    for (auto b : buffer) {
        EXPECT_EQ(b, 0);
    }
}

TEST(SecureZeroTest, NullPointerNoOp) {
    // Should not crash
    secureZero(nullptr, 100);
    SUCCEED();
}

TEST(SecureZeroTest, ZeroSizeNoOp) {
    uint8_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    secureZero(buffer, 0);
    // Buffer should be unchanged
    EXPECT_EQ(buffer[0], 0xFF);
}

// ===========================================================================
// secureCompare
// ===========================================================================

TEST(SecureCompareTest, EqualBuffers) {
    uint8_t a[] = {1, 2, 3, 4, 5};
    uint8_t b[] = {1, 2, 3, 4, 5};
    EXPECT_TRUE(secureCompare(a, b, 5));
}

TEST(SecureCompareTest, DifferentBuffers) {
    uint8_t a[] = {1, 2, 3, 4, 5};
    uint8_t b[] = {1, 2, 3, 4, 6};
    EXPECT_FALSE(secureCompare(a, b, 5));
}

TEST(SecureCompareTest, DifferentAtFirstByte) {
    uint8_t a[] = {0, 2, 3};
    uint8_t b[] = {1, 2, 3};
    EXPECT_FALSE(secureCompare(a, b, 3));
}

TEST(SecureCompareTest, ZeroLengthIsEqual) {
    uint8_t a[] = {1};
    uint8_t b[] = {2};
    EXPECT_TRUE(secureCompare(a, b, 0));
}

TEST(SecureCompareTest, AllZeroBuffers) {
    uint8_t a[32] = {};
    uint8_t b[32] = {};
    EXPECT_TRUE(secureCompare(a, b, 32));
}

} // namespace testing
} // namespace makine
