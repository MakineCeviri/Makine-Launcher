/**
 * @file test_sandbox.cpp
 * @brief Unit tests for sandbox policy infrastructure
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

// sandbox.hpp transitively includes <chrono> which triggers
// utc_clock errors on MinGW GCC 13.1 — skip all tests
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <gtest/gtest.h>
TEST(SandboxPolicyTest, DISABLED_SkippedOnMinGW) { GTEST_SKIP() << "chrono broken on MinGW GCC 13.1"; }
#else
#include <gtest/gtest.h>
#include <makine/sandbox.hpp>

#include <chrono>
#include <string>

namespace makine {
namespace testing {

// =========================================================================
// FILE PERMISSION BITWISE OPS
// =========================================================================

class SandboxPolicyTest : public ::testing::Test {};

TEST_F(SandboxPolicyTest, FilePermissionBitwiseOr) {
    auto rw = FilePermission::Read | FilePermission::Write;
    EXPECT_TRUE(hasPermission(rw, FilePermission::Read));
    EXPECT_TRUE(hasPermission(rw, FilePermission::Write));
    EXPECT_FALSE(hasPermission(rw, FilePermission::Execute));
    EXPECT_FALSE(hasPermission(rw, FilePermission::Delete));
}

TEST_F(SandboxPolicyTest, FilePermissionNoneHasNothing) {
    EXPECT_FALSE(hasPermission(FilePermission::None, FilePermission::Read));
    EXPECT_FALSE(hasPermission(FilePermission::None, FilePermission::Write));
    EXPECT_FALSE(hasPermission(FilePermission::None, FilePermission::Execute));
    EXPECT_FALSE(hasPermission(FilePermission::None, FilePermission::Delete));
}

TEST_F(SandboxPolicyTest, FilePermissionFullHasAll) {
    EXPECT_TRUE(hasPermission(FilePermission::Full, FilePermission::Read));
    EXPECT_TRUE(hasPermission(FilePermission::Full, FilePermission::Write));
    EXPECT_TRUE(hasPermission(FilePermission::Full, FilePermission::Execute));
    EXPECT_TRUE(hasPermission(FilePermission::Full, FilePermission::Delete));
}

TEST_F(SandboxPolicyTest, FilePermissionReadWritePreset) {
    EXPECT_TRUE(hasPermission(FilePermission::ReadWrite, FilePermission::Read));
    EXPECT_TRUE(hasPermission(FilePermission::ReadWrite, FilePermission::Write));
    EXPECT_FALSE(hasPermission(FilePermission::ReadWrite, FilePermission::Execute));
}

TEST_F(SandboxPolicyTest, ProcessRestrictionBitwiseOps) {
    auto strict = ProcessRestriction::NoChildProcesses | ProcessRestriction::NoElevation;
    EXPECT_NE(static_cast<int>(strict), 0);
}

// =========================================================================
// RESOURCE LIMITS
// =========================================================================

TEST_F(SandboxPolicyTest, StrictResourceLimits) {
    auto limits = ResourceLimits::strict();
    EXPECT_EQ(limits.maxMemoryBytes, 512u * 1024 * 1024);
    EXPECT_EQ(limits.maxCpuTime, std::chrono::seconds(60));
    EXPECT_EQ(limits.maxWallTime, std::chrono::seconds(300));
    EXPECT_EQ(limits.maxOpenFiles, 100u);
    EXPECT_EQ(limits.maxFileSizeBytes, 10u * 1024 * 1024);
    EXPECT_EQ(limits.maxDiskWriteBytes, 100u * 1024 * 1024);
}

TEST_F(SandboxPolicyTest, PermissiveResourceLimits) {
    auto limits = ResourceLimits::permissive();
    EXPECT_EQ(limits.maxMemoryBytes, 0u);
    EXPECT_EQ(limits.maxCpuTime, std::chrono::seconds(0));
    EXPECT_EQ(limits.maxWallTime, std::chrono::seconds(0));
}

TEST_F(SandboxPolicyTest, DefaultResourceLimits) {
    ResourceLimits defaults;
    EXPECT_EQ(defaults.maxMemoryBytes, 0u);
    EXPECT_EQ(defaults.maxOpenFiles, 1024u);
    EXPECT_EQ(defaults.maxFileSizeBytes, 100u * 1024 * 1024);
    EXPECT_EQ(defaults.maxDiskWriteBytes, 1024u * 1024 * 1024);
}

// =========================================================================
// FILE ACCESS RULE
// =========================================================================

TEST_F(SandboxPolicyTest, FileAccessRuleReadOnly) {
    auto rule = FileAccessRule::readOnly("/game/*", "game files");
    EXPECT_EQ(rule.pathPattern, "/game/*");
    EXPECT_TRUE(hasPermission(rule.permissions, FilePermission::Read));
    EXPECT_FALSE(hasPermission(rule.permissions, FilePermission::Write));
    EXPECT_EQ(rule.description, "game files");
    EXPECT_FALSE(rule.isRegex);
    EXPECT_EQ(rule.priority, 0);
}

TEST_F(SandboxPolicyTest, FileAccessRuleReadWrite) {
    auto rule = FileAccessRule::readWrite("/temp/*", "temp files");
    EXPECT_TRUE(hasPermission(rule.permissions, FilePermission::Read));
    EXPECT_TRUE(hasPermission(rule.permissions, FilePermission::Write));
    EXPECT_FALSE(hasPermission(rule.permissions, FilePermission::Execute));
}

TEST_F(SandboxPolicyTest, FileAccessRuleDeny) {
    auto rule = FileAccessRule::deny("/system/*", "system files");
    EXPECT_EQ(rule.permissions, FilePermission::None);
    EXPECT_EQ(rule.priority, 100);
}

TEST_F(SandboxPolicyTest, FileAccessRuleEmptyDescription) {
    auto rule = FileAccessRule::readOnly("/path/*");
    EXPECT_TRUE(rule.description.empty());
}

// =========================================================================
// NETWORK ACCESS RULE
// =========================================================================

TEST_F(SandboxPolicyTest, NetworkAccessRuleAllowHttps) {
    auto rule = NetworkAccessRule::allowHttps("api.makineceviri.org", "API");
    EXPECT_EQ(rule.hostPattern, "api.makineceviri.org");
    EXPECT_EQ(rule.portMin, 443);
    EXPECT_EQ(rule.portMax, 443);
    EXPECT_TRUE(rule.allow);
    EXPECT_EQ(rule.protocol, "tcp");
}

TEST_F(SandboxPolicyTest, NetworkAccessRuleLocalhostOnly) {
    auto rule = NetworkAccessRule::localhostOnly("debug");
    EXPECT_EQ(rule.hostPattern, "localhost");
    EXPECT_EQ(rule.portMin, 0);
    EXPECT_EQ(rule.portMax, 65535);
    EXPECT_TRUE(rule.allow);
}

TEST_F(SandboxPolicyTest, NetworkAccessRuleDefaultPorts) {
    NetworkAccessRule rule;
    EXPECT_EQ(rule.portMin, 0);
    EXPECT_EQ(rule.portMax, 65535);
    EXPECT_TRUE(rule.allow);
}

// =========================================================================
// SANDBOX POLICY STRUCT
// =========================================================================

TEST_F(SandboxPolicyTest, DefaultPolicyCreation) {
    SandboxPolicy policy;
    EXPECT_EQ(policy.name, "default");
    EXPECT_FALSE(policy.enforced);
    EXPECT_EQ(policy.networkAccess, NetworkAccess::AllowList);
    EXPECT_EQ(policy.processRestrictions, ProcessRestriction::None);
    EXPECT_TRUE(policy.fileRules.empty());
    EXPECT_TRUE(policy.networkRules.empty());
}

TEST_F(SandboxPolicyTest, PolicyWithRules) {
    SandboxPolicy policy;
    policy.name = "test-policy";
    policy.enforced = true;
    policy.fileRules.push_back(FileAccessRule::readOnly("/game/*"));
    policy.fileRules.push_back(FileAccessRule::deny("/system/*"));
    policy.networkRules.push_back(NetworkAccessRule::allowHttps("api.com"));

    EXPECT_EQ(policy.fileRules.size(), 2u);
    EXPECT_EQ(policy.networkRules.size(), 1u);
}

// =========================================================================
// SANDBOX CONTEXT
// =========================================================================

TEST_F(SandboxPolicyTest, SandboxContextCreation) {
    SandboxPolicy policy;
    policy.name = "test-sandbox";
    policy.enforced = true;

    SandboxContext ctx(std::move(policy));
    EXPECT_EQ(ctx.name(), "test-sandbox");
    EXPECT_TRUE(ctx.isEnforced());
}

TEST_F(SandboxPolicyTest, SandboxContextAuditMode) {
    SandboxPolicy policy;
    policy.name = "audit-sandbox";
    policy.enforced = false;

    SandboxContext ctx(std::move(policy));
    EXPECT_FALSE(ctx.isEnforced());
}

TEST_F(SandboxPolicyTest, SandboxContextResourceMemoryTracking) {
    SandboxPolicy policy;
    policy.name = "resource-test";
    policy.limits = ResourceLimits::strict();

    SandboxContext ctx(std::move(policy));

    EXPECT_TRUE(ctx.recordMemoryAlloc(1024));
    auto stats = ctx.getStats();
    EXPECT_EQ(stats.currentMemoryBytes, 1024u);

    ctx.recordMemoryFree(512);
    stats = ctx.getStats();
    EXPECT_EQ(stats.currentMemoryBytes, 512u);
    EXPECT_EQ(stats.peakMemoryBytes, 1024u);
}

TEST_F(SandboxPolicyTest, SandboxContextDiskWriteTracking) {
    SandboxPolicy policy;
    policy.name = "disk-test";
    policy.limits = ResourceLimits::strict();

    SandboxContext ctx(std::move(policy));

    EXPECT_TRUE(ctx.recordFileWrite(512));
    EXPECT_TRUE(ctx.recordFileWrite(1024));

    auto stats = ctx.getStats();
    EXPECT_EQ(stats.totalDiskWriteBytes, 1536u);
}

TEST_F(SandboxPolicyTest, SandboxContextResetStats) {
    SandboxPolicy policy;
    policy.name = "reset-test";

    SandboxContext ctx(std::move(policy));
    ctx.recordMemoryAlloc(1024);
    ctx.resetStats();

    auto stats = ctx.getStats();
    EXPECT_EQ(stats.fileAccessChecks, 0u);
    EXPECT_EQ(stats.networkAccessChecks, 0u);
}

TEST_F(SandboxPolicyTest, SandboxContextInitialStats) {
    SandboxPolicy policy;
    SandboxContext ctx(std::move(policy));

    auto stats = ctx.getStats();
    EXPECT_EQ(stats.fileAccessChecks, 0u);
    EXPECT_EQ(stats.fileAccessDenied, 0u);
    EXPECT_EQ(stats.networkAccessChecks, 0u);
    EXPECT_EQ(stats.networkAccessDenied, 0u);
    EXPECT_EQ(stats.currentMemoryBytes, 0u);
    EXPECT_EQ(stats.peakMemoryBytes, 0u);
    EXPECT_EQ(stats.totalDiskWriteBytes, 0u);
    EXPECT_TRUE(stats.violations.empty());
}

// =========================================================================
// SCOPED SANDBOX
// =========================================================================

TEST_F(SandboxPolicyTest, ScopedSandboxNotActiveByDefault) {
    EXPECT_FALSE(ScopedSandbox::isActive());
    EXPECT_EQ(ScopedSandbox::current(), nullptr);
}

TEST_F(SandboxPolicyTest, ScopedSandboxIsActiveInScope) {
    EXPECT_FALSE(ScopedSandbox::isActive());

    {
        SandboxPolicy policy;
        policy.name = "scoped-test";
        ScopedSandbox sandbox(std::move(policy));

        EXPECT_TRUE(ScopedSandbox::isActive());
        EXPECT_NE(ScopedSandbox::current(), nullptr);
        EXPECT_EQ(ScopedSandbox::current()->name(), "scoped-test");
    }

    EXPECT_FALSE(ScopedSandbox::isActive());
    EXPECT_EQ(ScopedSandbox::current(), nullptr);
}

TEST_F(SandboxPolicyTest, ScopedSandboxContextAccess) {
    SandboxPolicy policy;
    policy.name = "ctx-access";
    ScopedSandbox sandbox(std::move(policy));

    EXPECT_EQ(sandbox.context().name(), "ctx-access");
}

// =========================================================================
// CONVENIENCE FUNCTIONS
// =========================================================================

TEST_F(SandboxPolicyTest, SandboxCheckFileOutsideSandbox) {
    EXPECT_TRUE(sandboxCheckFile(fs::path("/any/path"), FilePermission::Read));
    EXPECT_TRUE(sandboxCheckFile(fs::path("/any/path"), FilePermission::Write));
}

TEST_F(SandboxPolicyTest, SandboxCheckNetworkOutsideSandbox) {
    EXPECT_TRUE(sandboxCheckNetwork("any.host", 443));
    EXPECT_TRUE(sandboxCheckNetwork("any.host", 80));
}

} // namespace testing
} // namespace makine

#endif // !__MINGW32__
