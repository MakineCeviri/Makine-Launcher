/**
 * @file test_main.cpp
 * @brief Main test runner for Makine Core
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <makine/core.hpp>
#include <filesystem>
#include <cstring>
#include <thread>
#include <chrono>

// Global test environment to handle Core initialization
class MakineTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize logging for tests
        spdlog::set_level(spdlog::level::warn);

        // Initialize Makine Core with test configuration
        makine::CoreConfig testConfig;
        testConfig.dataDirectory = (std::filesystem::temp_directory_path() / "makine_test_data").string();
        testConfig.cacheDirectory = (std::filesystem::temp_directory_path() / "makine_test_cache").string();
        testConfig.logsDirectory = (std::filesystem::temp_directory_path() / "makine_test_logs").string();
        testConfig.logLevel = spdlog::level::warn;

        // Create test directories
        std::filesystem::create_directories(testConfig.dataDirectory);
        std::filesystem::create_directories(testConfig.cacheDirectory);
        std::filesystem::create_directories(testConfig.logsDirectory);

        // Remove stale DPAPI-encrypted DB files (CryptUnprotectData hangs on MinGW)
        {
            std::error_code ec;
            auto dataDir = std::filesystem::path(testConfig.dataDirectory);
            std::filesystem::remove(dataDir / "makine.db.enc", ec);
            std::filesystem::remove(dataDir / "makine.db", ec);
            std::filesystem::remove(dataDir / "makine.db-wal", ec);
            std::filesystem::remove(dataDir / "makine.db-shm", ec);
        }

        // Initialize core
        auto& core = makine::Core::instance();
        auto initResult = core.initialize(testConfig);
        if (!initResult) {
            std::cerr << "Failed to initialize Makine Core: "
                      << initResult.error().message() << std::endl;
        }
    }

    void TearDown() override {
        // Cleanup core (this also resets its internal logger)
        auto& core = makine::Core::instance();
        if (core.isInitialized()) {
            core.shutdown();
        }

        // Do NOT call spdlog::drop_all() / spdlog::shutdown() here.
        // Core::shutdown() already resets its logger, and calling these
        // causes SEGFAULT when singleton destructors try to log during
        // static destruction after this point.

        // Clean up temp directories
        auto tempPath = std::filesystem::temp_directory_path();
        std::error_code ec;
        std::filesystem::remove_all(tempPath / "makine_test_data", ec);
        std::filesystem::remove_all(tempPath / "makine_test_cache", ec);
        std::filesystem::remove_all(tempPath / "makine_test_logs", ec);
    }
};

int main(int argc, char** argv) {
    // Initialize Google Test first
    ::testing::InitGoogleTest(&argc, argv);

    // Register our environment (SetUp is called after test discovery)
    ::testing::AddGlobalTestEnvironment(new MakineTestEnvironment());

    return RUN_ALL_TESTS();
}
