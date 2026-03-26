/**
 * @file test_version.cpp
 * @brief Unit tests for version.hpp and constants.hpp
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/version.hpp>
#include <makine/constants.hpp>

#include <string>
#include <cstring>

namespace makine {
namespace testing {

// ===========================================================================
// Version Constants
// ===========================================================================

TEST(VersionConstantsTest, VersionNumbersNonNegative) {
    EXPECT_GE(VERSION_MAJOR, 0);
    EXPECT_GE(VERSION_MINOR, 0);
    EXPECT_GE(VERSION_PATCH, 0);
}

TEST(VersionConstantsTest, VersionStringNonEmpty) {
    EXPECT_NE(VERSION_STRING, nullptr);
    EXPECT_GT(std::strlen(VERSION_STRING), 0u);
}

TEST(VersionConstantsTest, VersionShortNonEmpty) {
    EXPECT_NE(VERSION_SHORT, nullptr);
    EXPECT_GT(std::strlen(VERSION_SHORT), 0u);
}

TEST(VersionConstantsTest, VersionNumberConsistency) {
    int expected = VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;
    EXPECT_EQ(VERSION_NUMBER, expected);
}

TEST(VersionConstantsTest, CurrentVersionMatchesConstants) {
    EXPECT_EQ(CURRENT_VERSION.major, VERSION_MAJOR);
    EXPECT_EQ(CURRENT_VERSION.minor, VERSION_MINOR);
    EXPECT_EQ(CURRENT_VERSION.patch, VERSION_PATCH);
}

// ===========================================================================
// ABI Version
// ===========================================================================

TEST(ABIVersionTest, ABIVersionPositive) {
    EXPECT_GT(ABI_VERSION, 0);
}

TEST(ABIVersionTest, MinABIVersionPositive) {
    EXPECT_GT(MIN_ABI_VERSION, 0);
}

TEST(ABIVersionTest, MinABINotExceedCurrent) {
    EXPECT_LE(MIN_ABI_VERSION, ABI_VERSION);
}

TEST(ABIVersionTest, CurrentABIIsCompatible) {
    EXPECT_TRUE(isABICompatible(ABI_VERSION));
}

TEST(ABIVersionTest, MinABIIsCompatible) {
    EXPECT_TRUE(isABICompatible(MIN_ABI_VERSION));
}

TEST(ABIVersionTest, TooOldABINotCompatible) {
    if (MIN_ABI_VERSION > 1) {
        EXPECT_FALSE(isABICompatible(MIN_ABI_VERSION - 1));
    }
    EXPECT_FALSE(isABICompatible(0));
}

TEST(ABIVersionTest, FutureABINotCompatible) {
    EXPECT_FALSE(isABICompatible(ABI_VERSION + 1));
}

TEST(ABIVersionTest, GetABIVersionMatchesConstant) {
    EXPECT_EQ(getABIVersion(), ABI_VERSION);
}

// ===========================================================================
// isVersionAtLeast
// ===========================================================================

TEST(VersionAtLeastTest, CurrentVersionIsAtLeastItself) {
    EXPECT_TRUE(isVersionAtLeast(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH));
}

TEST(VersionAtLeastTest, CurrentVersionIsAtLeastZero) {
    EXPECT_TRUE(isVersionAtLeast(0, 0, 0));
}

TEST(VersionAtLeastTest, FutureVersionNotAtLeast) {
    EXPECT_FALSE(isVersionAtLeast(VERSION_MAJOR + 1));
}

TEST(VersionAtLeastTest, SameMajorHigherMinor) {
    if (VERSION_MINOR < 99) {
        EXPECT_FALSE(isVersionAtLeast(VERSION_MAJOR, VERSION_MINOR + 1));
    }
}

// ===========================================================================
// SemanticVersion — Parsing
// ===========================================================================

TEST(SemanticVersionTest, ParseSimple) {
    auto v = SemanticVersion::parse("1.2.3");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
    EXPECT_TRUE(v.prerelease.empty());
}

TEST(SemanticVersionTest, ParseWithPrerelease) {
    auto v = SemanticVersion::parse("0.1.0-alpha");
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 1);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.prerelease, "alpha");
}

TEST(SemanticVersionTest, ParseWithComplexPrerelease) {
    auto v = SemanticVersion::parse("1.0.0-rc.1");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    EXPECT_EQ(v.prerelease, "rc.1");
}

TEST(SemanticVersionTest, ParseZeroVersion) {
    auto v = SemanticVersion::parse("0.0.0");
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
}

TEST(SemanticVersionTest, ParseLargeVersion) {
    auto v = SemanticVersion::parse("100.200.300");
    EXPECT_EQ(v.major, 100);
    EXPECT_EQ(v.minor, 200);
    EXPECT_EQ(v.patch, 300);
}

// ===========================================================================
// SemanticVersion — toString
// ===========================================================================

TEST(SemanticVersionTest, ToStringSimple) {
    SemanticVersion v(1, 2, 3);
    EXPECT_EQ(v.toString(), "1.2.3");
}

TEST(SemanticVersionTest, ToStringWithPrerelease) {
    SemanticVersion v(0, 1, 0, "alpha");
    EXPECT_EQ(v.toString(), "0.1.0-alpha");
}

TEST(SemanticVersionTest, RoundTripParseToString) {
    std::string original = "2.5.10";
    auto v = SemanticVersion::parse(original);
    EXPECT_EQ(v.toString(), original);
}

TEST(SemanticVersionTest, RoundTripWithPrerelease) {
    std::string original = "1.0.0-beta.2";
    auto v = SemanticVersion::parse(original);
    EXPECT_EQ(v.toString(), original);
}

// ===========================================================================
// SemanticVersion — toNumber
// ===========================================================================

TEST(SemanticVersionTest, ToNumberCalculation) {
    SemanticVersion v(1, 2, 3);
    EXPECT_EQ(v.toNumber(), 10203);
}

TEST(SemanticVersionTest, ToNumberZero) {
    SemanticVersion v(0, 0, 0);
    EXPECT_EQ(v.toNumber(), 0);
}

// ===========================================================================
// SemanticVersion — Comparison
// ===========================================================================

TEST(SemanticVersionTest, EqualVersions) {
    SemanticVersion a(1, 2, 3);
    SemanticVersion b(1, 2, 3);
    EXPECT_EQ(a, b);
}

TEST(SemanticVersionTest, DifferentMajor) {
    SemanticVersion a(1, 0, 0);
    SemanticVersion b(2, 0, 0);
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
    EXPECT_NE(a, b);
}

TEST(SemanticVersionTest, DifferentMinor) {
    SemanticVersion a(1, 0, 0);
    SemanticVersion b(1, 1, 0);
    EXPECT_LT(a, b);
}

TEST(SemanticVersionTest, DifferentPatch) {
    SemanticVersion a(1, 0, 0);
    SemanticVersion b(1, 0, 1);
    EXPECT_LT(a, b);
}

TEST(SemanticVersionTest, PrereleaseIsLessThanRelease) {
    SemanticVersion prerelease(1, 0, 0, "alpha");
    SemanticVersion release(1, 0, 0);
    EXPECT_LT(prerelease, release);
}

TEST(SemanticVersionTest, PrereleaseComparedAlphabetically) {
    SemanticVersion a(1, 0, 0, "alpha");
    SemanticVersion b(1, 0, 0, "beta");
    EXPECT_LT(a, b);
}

TEST(SemanticVersionTest, LessOrEqual) {
    SemanticVersion a(1, 0, 0);
    SemanticVersion b(1, 0, 0);
    EXPECT_LE(a, b);
    SemanticVersion c(1, 0, 1);
    EXPECT_LE(a, c);
}

TEST(SemanticVersionTest, GreaterOrEqual) {
    SemanticVersion a(1, 0, 0);
    SemanticVersion b(1, 0, 0);
    EXPECT_GE(a, b);
    SemanticVersion c(0, 9, 9);
    EXPECT_GE(a, c);
}

// ===========================================================================
// BuildInfo
// ===========================================================================

TEST(BuildInfoTest, GetBuildInfoNonEmpty) {
    auto info = getBuildInfo();
    EXPECT_EQ(info.versionMajor, VERSION_MAJOR);
    EXPECT_EQ(info.versionMinor, VERSION_MINOR);
    EXPECT_EQ(info.versionPatch, VERSION_PATCH);
    EXPECT_FALSE(info.buildDate.empty());
    EXPECT_FALSE(info.buildTime.empty());
    EXPECT_FALSE(info.platform.empty());
    EXPECT_FALSE(info.architecture.empty());
    EXPECT_FALSE(info.cppStandard.empty());
}

TEST(BuildInfoTest, CompilerDetected) {
    auto info = getBuildInfo();
    EXPECT_NE(info.compiler, Compiler::Unknown);
    EXPECT_FALSE(info.compilerVersion.empty());
}

TEST(BuildInfoTest, ToTextNonEmpty) {
    auto info = getBuildInfo();
    auto text = info.toText();
    EXPECT_FALSE(text.empty());
    EXPECT_NE(text.find("Makine"), std::string::npos);
    EXPECT_NE(text.find("ABI Version"), std::string::npos);
}

TEST(BuildInfoTest, ToJsonNonEmpty) {
    auto info = getBuildInfo();
    auto json = info.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"version\""), std::string::npos);
    EXPECT_NE(json.find("\"abi\""), std::string::npos);
    EXPECT_NE(json.find("\"build\""), std::string::npos);
    EXPECT_NE(json.find("\"platform\""), std::string::npos);
}

// ===========================================================================
// Constants (from constants.hpp)
// ===========================================================================

TEST(ConstantsTest, ArchiveReadBufferSizePositive) {
    EXPECT_GT(kArchiveReadBufferSize, 0u);
}

TEST(ConstantsTest, PBKDF2IterationsPositive) {
    EXPECT_GT(kPBKDF2Iterations, 0u);
}

TEST(ConstantsTest, DefaultCacheMaxSizePositive) {
    EXPECT_GT(kDefaultCacheMaxSize, 0u);
}

TEST(ConstantsTest, DefaultTimeoutMsPositive) {
    EXPECT_GT(kDefaultTimeoutMs, 0u);
}

TEST(ConstantsTest, ShortTimeoutMsPositive) {
    EXPECT_GT(kShortTimeoutMs, 0u);
}

TEST(ConstantsTest, ShortTimeoutLessThanDefault) {
    EXPECT_LT(kShortTimeoutMs, kDefaultTimeoutMs);
}

TEST(ConstantsTest, MaxDatabaseEntriesPositive) {
    EXPECT_GT(kMaxDatabaseEntries, 0u);
}

// ===========================================================================
// Version Macros
// ===========================================================================

TEST(VersionMacroTest, VersionAtLeastMacroCurrentVersion) {
    EXPECT_TRUE(MAKINE_VERSION_AT_LEAST(0, 0, 0));
    EXPECT_TRUE(MAKINE_VERSION_AT_LEAST(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH));
}

TEST(VersionMacroTest, VersionEncodeMacro) {
    EXPECT_EQ(MAKINE_VERSION_ENCODE(1, 2, 3), 10203);
    EXPECT_EQ(MAKINE_VERSION_ENCODE(0, 0, 0), 0);
}

} // namespace testing
} // namespace makine
