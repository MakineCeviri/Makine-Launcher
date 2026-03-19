/**
 * @file test_core.cpp
 * @brief Unit tests for core.hpp -- InitOptions, InitResult, Core singleton basics
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/core.hpp>

#include <string>

namespace makine {
namespace testing {

// ===========================================================================
// InitOptions
// ===========================================================================

TEST(InitOptionsTest, DefaultValues) {
    InitOptions opts;
    EXPECT_FALSE(opts.skipHealthCheck);
    EXPECT_FALSE(opts.skipDatabaseInit);
    EXPECT_TRUE(opts.enableMetrics);
    EXPECT_TRUE(opts.enableAuditLog);
    EXPECT_FALSE(opts.enableDebugDumps);
    EXPECT_FALSE(opts.verboseLogging);
}

TEST(InitOptionsTest, CustomValues) {
    InitOptions opts;
    opts.skipHealthCheck = true;
    opts.skipDatabaseInit = true;
    opts.enableMetrics = false;
    EXPECT_TRUE(opts.skipHealthCheck);
    EXPECT_TRUE(opts.skipDatabaseInit);
    EXPECT_FALSE(opts.enableMetrics);
}

// ===========================================================================
// InitResult
// ===========================================================================

TEST(InitResultTest, DefaultValues) {
    InitResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.message.empty());
    EXPECT_EQ(result.initDuration.count(), 0);
}

TEST(InitResultTest, FeatureDefaults) {
    InitResult result;
    EXPECT_FALSE(result.features.hasTaskflow);
    EXPECT_FALSE(result.features.hasSimdjson);
    EXPECT_FALSE(result.features.hasMio);
    EXPECT_FALSE(result.features.hasLibsodium);
    EXPECT_FALSE(result.features.hasBit7z);
    EXPECT_FALSE(result.features.hasEfsw);
}

TEST(InitResultTest, SetFields) {
    InitResult result;
    result.success = true;
    result.message = "Initialized successfully";
    result.initDuration = std::chrono::milliseconds(150);
    result.features.hasTaskflow = true;
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "Initialized successfully");
    EXPECT_EQ(result.initDuration.count(), 150);
    EXPECT_TRUE(result.features.hasTaskflow);
}

// ===========================================================================
// Core Singleton
// ===========================================================================

TEST(CoreTest, InstanceReturnsSameReference) {
    auto& c1 = Core::instance();
    auto& c2 = Core::instance();
    EXPECT_EQ(&c1, &c2);
}

TEST(CoreTest, VersionIsNonEmpty) {
    auto ver = Core::version();
    EXPECT_FALSE(ver.empty());
}

TEST(CoreTest, VersionStartsWithDigit) {
    auto ver = Core::version();
    EXPECT_GE(ver[0], static_cast<char>(48));
    EXPECT_LE(ver[0], static_cast<char>(57));
}

TEST(CoreTest, FeaturesAccessible) {
    const auto& f = Core::features();
    // Just check it does not crash
    (void)f.has_taskflow;
    (void)f.has_simdjson;
    SUCCEED();
}

TEST(CoreTest, MetricsAccessor) {
    auto& core = Core::instance();
    auto& m = core.metrics();
    EXPECT_EQ(&m, &Metrics::instance());
}

TEST(CoreTest, HealthCheckerAccessor) {
    auto& core = Core::instance();
    auto& h = core.healthChecker();
    EXPECT_EQ(&h, &HealthChecker::instance());
}

TEST(CoreTest, AuditLoggerAccessor) {
    auto& core = Core::instance();
    auto& a = core.auditLogger();
    EXPECT_EQ(&a, &AuditLogger::instance());
}

TEST(CoreTest, ConfigManagerAccessor) {
    auto& core = Core::instance();
    auto& cm = core.configManager();
    EXPECT_EQ(&cm, &ConfigManager::instance());
}

TEST(CoreTest, InitResultBeforeInit) {
    auto& core = Core::instance();
    const auto& result = core.initResult();
    // Before explicit init, result may or may not be populated
    // Just verify it does not crash
    (void)result.success;
    (void)result.message;
    SUCCEED();
}

} // namespace testing
} // namespace makine
