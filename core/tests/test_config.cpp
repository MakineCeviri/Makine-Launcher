/**
 * @file test_config.cpp
 * @brief Unit tests for Configuration System
 *
 * Tests: default values, validation (errors & warnings),
 * JSON round-trip serialization, file save/load,
 * environment variable overrides, and ConfigValidationResult.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makine/config.hpp>
#include <fstream>
#include <cstdlib>

namespace makine {
namespace testing {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

// =============================================================================
// DEFAULT VALUES TESTS
// =============================================================================

class DefaultConfigTest : public ::testing::Test {};

TEST_F(DefaultConfigTest, ScanningDefaults) {
    ScanningConfig c;
    EXPECT_EQ(c.maxParallelScans, 4u);
    EXPECT_EQ(c.scanTimeoutMs, kDefaultTimeoutMs);
    EXPECT_TRUE(c.scanSteam);
    EXPECT_TRUE(c.scanEpic);
    EXPECT_TRUE(c.scanGOG);
    EXPECT_EQ(c.maxFilesToScan, 10000u);
    EXPECT_EQ(c.minEngineConfidence, 30u);
    EXPECT_EQ(c.cacheValiditySeconds, 300u);
}

TEST_F(DefaultConfigTest, PatchingDefaults) {
    PatchingConfig c;
    EXPECT_TRUE(c.alwaysCreateBackup);
    EXPECT_EQ(c.minDiskSpaceMB, 500u);
    EXPECT_EQ(c.maxRetries, 3u);
    EXPECT_TRUE(c.atomicWrites);
    EXPECT_TRUE(c.verifyAfterPatch);
    EXPECT_EQ(c.maxBackupAgeDays, 30u);
    EXPECT_EQ(c.maxBackupSizeMB, 0u);
    EXPECT_TRUE(c.compressBackups);
}

TEST_F(DefaultConfigTest, TranslationDefaults) {
    TranslationConfig c;
    // All deferred fields removed; struct is empty but still participates in CoreConfig
    EXPECT_EQ(c, TranslationConfig{});
}

TEST_F(DefaultConfigTest, SecurityDefaults) {
    SecurityConfig c;
    EXPECT_TRUE(c.verifySignatures);
    EXPECT_TRUE(c.verifyChecksums);
    EXPECT_TRUE(c.trustedKeysPath.empty());
    EXPECT_FALSE(c.allowUntrustedPackages);
    EXPECT_TRUE(c.enableAuditLog);
    EXPECT_TRUE(c.blockPathTraversal);
}

TEST_F(DefaultConfigTest, NetworkDefaults) {
    NetworkConfig c;
    EXPECT_EQ(c.connectionTimeoutMs, kShortTimeoutMs);
    EXPECT_EQ(c.readTimeoutMs, kDefaultTimeoutMs);
    EXPECT_EQ(c.maxDownloadRetries, 3u);
    EXPECT_EQ(c.userAgent, "Makine-Launcher/0.1.0");
    EXPECT_TRUE(c.proxyUrl.empty());
    EXPECT_TRUE(c.verifySsl);
    EXPECT_EQ(c.maxConcurrentDownloads, 4u);
}

TEST_F(DefaultConfigTest, LoggingDefaults) {
    LoggingConfig c;
    EXPECT_EQ(c.level, "info");
    EXPECT_TRUE(c.logFilePath.empty());
    EXPECT_EQ(c.maxLogSizeMB, 10u);
    EXPECT_EQ(c.maxLogFiles, 5u);
    EXPECT_TRUE(c.consoleTimestamps);
    EXPECT_TRUE(c.coloredOutput);
}

TEST_F(DefaultConfigTest, DatabaseDefaults) {
    DatabaseConfig c;
    EXPECT_EQ(c.databasePath, "makine.db");
    EXPECT_TRUE(c.enableWAL);
    EXPECT_EQ(c.connectionPoolSize, 4u);
    EXPECT_EQ(c.queryTimeoutMs, 5000u);
    EXPECT_FALSE(c.vacuumOnStartup);
}

TEST_F(DefaultConfigTest, CoreConfigDefaults) {
    CoreConfig c;
    EXPECT_EQ(c.apiBaseUrl, "https://api.makineceviri.org/v1");
    EXPECT_TRUE(c.autoUpdateRuntime);
    EXPECT_FALSE(c.enableAnalytics);
}

// =============================================================================
// VALIDATION TESTS
// =============================================================================

class ConfigValidationTest : public ::testing::Test {
protected:
    CoreConfig validConfig() {
        CoreConfig c;
        c.scanning.maxParallelScans = 4;
        c.logging.level = "info";
        c.network.verifySsl = true;
        c.network.connectionTimeoutMs = 10000;
        c.dataDirectory = "";  // empty = no warning
        return c;
    }
};

TEST_F(ConfigValidationTest, ValidConfigPasses) {
    auto result = validateConfig(validConfig());
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.errors, IsEmpty());
}

TEST_F(ConfigValidationTest, ZeroParallelScansIsError) {
    auto c = validConfig();
    c.scanning.maxParallelScans = 0;
    auto result = validateConfig(c);
    EXPECT_FALSE(result.valid);
    EXPECT_THAT(result.errors, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, HighParallelScansWarns) {
    auto c = validConfig();
    c.scanning.maxParallelScans = 32;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, LowScanTimeoutWarns) {
    auto c = validConfig();
    c.scanning.scanTimeoutMs = 500;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, LowDiskSpaceWarns) {
    auto c = validConfig();
    c.patching.minDiskSpaceMB = 50;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, HighPatchRetriesWarns) {
    auto c = validConfig();
    c.patching.maxRetries = 20;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, InvalidLogLevelIsError) {
    auto c = validConfig();
    c.logging.level = "verbose";
    auto result = validateConfig(c);
    EXPECT_FALSE(result.valid);
}

TEST_F(ConfigValidationTest, AllValidLogLevelsPass) {
    for (const auto& level : {"trace", "debug", "info", "warn", "error", "critical"}) {
        auto c = validConfig();
        c.logging.level = level;
        auto result = validateConfig(c);
        EXPECT_TRUE(result.valid) << "Level '" << level << "' should be valid";
    }
}

TEST_F(ConfigValidationTest, DisabledSslWarns) {
    auto c = validConfig();
    c.network.verifySsl = false;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, LowConnectionTimeoutWarns) {
    auto c = validConfig();
    c.network.connectionTimeoutMs = 50;
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid);
    EXPECT_THAT(result.warnings, Not(IsEmpty()));
}

TEST_F(ConfigValidationTest, MultipleErrorsAccumulate) {
    auto c = validConfig();
    c.scanning.maxParallelScans = 0;         // error
    c.logging.level = "invalid";             // error
    auto result = validateConfig(c);
    EXPECT_FALSE(result.valid);
    EXPECT_GE(result.errors.size(), 2u);
}

// =============================================================================
// VALIDATION RESULT TESTS
// =============================================================================

class ValidationResultTest : public ::testing::Test {};

TEST_F(ValidationResultTest, DefaultIsValid) {
    ConfigValidationResult r;
    EXPECT_TRUE(r.valid);
    EXPECT_TRUE(r.isClean());
}

TEST_F(ValidationResultTest, WarningsMakeNotClean) {
    ConfigValidationResult r;
    r.warnings.push_back("some warning");
    EXPECT_TRUE(r.valid);
    EXPECT_FALSE(r.isClean());
}

TEST_F(ValidationResultTest, ErrorsMakeInvalidAndNotClean) {
    ConfigValidationResult r;
    r.valid = false;
    r.errors.push_back("some error");
    EXPECT_FALSE(r.valid);
    EXPECT_FALSE(r.isClean());
}

// =============================================================================
// FILE SAVE/LOAD TESTS
// =============================================================================

class ConfigFileTest : public ::testing::Test {
protected:
    fs::path tempDir;
    fs::path configFile;

    void SetUp() override {
        tempDir = fs::temp_directory_path() / "makine_test_config";
        fs::create_directories(tempDir);
        configFile = tempDir / "test_config.json";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }
};

TEST_F(ConfigFileTest, SaveAndLoad) {
    CoreConfig original;
    original.scanning.maxParallelScans = 8;
    original.network.proxyUrl = "http://test:1234";

    original.saveToFile(configFile);
    ASSERT_TRUE(fs::exists(configFile));

    auto loaded = CoreConfig::loadFromFile(configFile);
    EXPECT_EQ(loaded.scanning.maxParallelScans, 8u);
    EXPECT_EQ(loaded.network.proxyUrl, "http://test:1234");
}

TEST_F(ConfigFileTest, LoadNonExistentReturnsDefaults) {
    auto loaded = CoreConfig::loadFromFile(tempDir / "does_not_exist.json");
    CoreConfig defaults = CoreConfig::getDefaults();

    EXPECT_EQ(loaded.scanning.maxParallelScans, defaults.scanning.maxParallelScans);
}

TEST_F(ConfigFileTest, SaveCreatesParentDirectories) {
    fs::path nested = tempDir / "a" / "b" / "c" / "config.json";
    CoreConfig config;
    config.saveToFile(nested);
    EXPECT_TRUE(fs::exists(nested));
}

TEST_F(ConfigFileTest, LoadMalformedJsonReturnsDefaults) {
    // Write invalid JSON
    {
        std::ofstream f(configFile);
        f << "this is not json {{{";
    }

    auto loaded = CoreConfig::loadFromFile(configFile);
    CoreConfig defaults = CoreConfig::getDefaults();
    EXPECT_EQ(loaded.scanning.maxParallelScans, defaults.scanning.maxParallelScans);
}

// =============================================================================
// STRUCT EQUALITY TESTS
// =============================================================================

class ConfigEqualityTest : public ::testing::Test {};

TEST_F(ConfigEqualityTest, DefaultStructsAreEqual) {
    EXPECT_EQ(ScanningConfig{}, ScanningConfig{});
    EXPECT_EQ(PatchingConfig{}, PatchingConfig{});
    EXPECT_EQ(TranslationConfig{}, TranslationConfig{});
    EXPECT_EQ(SecurityConfig{}, SecurityConfig{});
    EXPECT_EQ(NetworkConfig{}, NetworkConfig{});
    EXPECT_EQ(LoggingConfig{}, LoggingConfig{});
    EXPECT_EQ(DatabaseConfig{}, DatabaseConfig{});
}

TEST_F(ConfigEqualityTest, ModifiedStructsAreNotEqual) {
    ScanningConfig a, b;
    b.maxParallelScans = 99;
    EXPECT_NE(a, b);
}

// =============================================================================
// ADDITIONAL EDGE CASES
// =============================================================================

TEST_F(ConfigFileTest, LoadConfigFromTruncatedFile) {
    // Write a truncated JSON file
    {
        std::ofstream f(configFile);
        f << R"({"scanning": {"maxParallelScans": 8}, "trans)";
    }

    auto loaded = CoreConfig::loadFromFile(configFile);
    // Should return defaults when JSON is truncated
    CoreConfig defaults = CoreConfig::getDefaults();
    EXPECT_EQ(loaded.scanning.maxParallelScans, defaults.scanning.maxParallelScans);
}

TEST_F(ConfigValidationTest, MultipleWarningsAccumulate) {
    auto c = validConfig();
    c.scanning.maxParallelScans = 32;     // warning: high
    c.scanning.scanTimeoutMs = 500;       // warning: low timeout
    c.network.verifySsl = false;          // warning: SSL disabled
    auto result = validateConfig(c);
    EXPECT_TRUE(result.valid); // warnings only, no errors
    EXPECT_GE(result.warnings.size(), 3u);
}

TEST_F(ConfigFileTest, LoadConfigWithUnknownFields) {
    // Write JSON with extra/unknown fields
    {
        std::ofstream f(configFile);
        f << R"({
            "scanning": {"maxParallelScans": 4},
            "unknownSection": {"foo": "bar"},
            "network": {"proxyUrl": "http://test", "unknownField": 42}
        })";
    }

    auto loaded = CoreConfig::loadFromFile(configFile);
    // Should not crash, unknown fields are ignored
    EXPECT_EQ(loaded.network.proxyUrl, "http://test");
}

TEST_F(ConfigFileTest, SaveAndLoadPreservesAllFields) {
    CoreConfig original;
    original.scanning.maxParallelScans = 12;
    original.scanning.scanSteam = false;
    original.scanning.maxFilesToScan = 5000;
    original.patching.alwaysCreateBackup = false;
    original.patching.maxRetries = 5;
    original.security.verifySignatures = false;
    original.network.proxyUrl = "http://proxy:8080";
    original.logging.level = "debug";
    original.database.enableWAL = false;

    original.saveToFile(configFile);
    auto loaded = CoreConfig::loadFromFile(configFile);

    EXPECT_EQ(loaded.scanning.maxParallelScans, 12u);
    EXPECT_FALSE(loaded.scanning.scanSteam);
    EXPECT_EQ(loaded.scanning.maxFilesToScan, 5000u);
    EXPECT_FALSE(loaded.patching.alwaysCreateBackup);
    EXPECT_EQ(loaded.patching.maxRetries, 5u);
    EXPECT_FALSE(loaded.security.verifySignatures);
    EXPECT_EQ(loaded.network.proxyUrl, "http://proxy:8080");
    EXPECT_EQ(loaded.logging.level, "debug");
    EXPECT_FALSE(loaded.database.enableWAL);
}

TEST_F(ConfigValidationTest, ExtremelyHighParallelScansStillValid) {
    auto c = validConfig();
    c.scanning.maxParallelScans = 1000;
    auto result = validateConfig(c);
    // Should still be valid (just warning)
    EXPECT_TRUE(result.valid);
}

} // namespace testing
} // namespace makine
