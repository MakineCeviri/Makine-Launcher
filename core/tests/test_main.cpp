/**
 * @file test_main.cpp
 * @brief Main test runner for MakineAI Core
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <makineai/core.hpp>
#include <filesystem>
#include <cstring>
#include <thread>
#include <chrono>

// Global test environment to handle Core initialization
class MakineAITestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize logging for tests
        spdlog::set_level(spdlog::level::warn);

        // Initialize MakineAI Core with test configuration
        makineai::CoreConfig testConfig;
        testConfig.dataDirectory = (std::filesystem::temp_directory_path() / "makineai_test_data").string();
        testConfig.cacheDirectory = (std::filesystem::temp_directory_path() / "makineai_test_cache").string();
        testConfig.logsDirectory = (std::filesystem::temp_directory_path() / "makineai_test_logs").string();
        testConfig.logLevel = spdlog::level::warn;

        // Create test directories
        std::filesystem::create_directories(testConfig.dataDirectory);
        std::filesystem::create_directories(testConfig.cacheDirectory);
        std::filesystem::create_directories(testConfig.logsDirectory);

        // Initialize core
        auto& core = makineai::Core::instance();
        auto initResult = core.initialize(testConfig);
        if (!initResult) {
            std::cerr << "Failed to initialize MakineAI Core: "
                      << initResult.error().message() << std::endl;
        }
    }

    void TearDown() override {
        // Cleanup core (this also resets its internal logger)
        auto& core = makineai::Core::instance();
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
        std::filesystem::remove_all(tempPath / "makineai_test_data", ec);
        std::filesystem::remove_all(tempPath / "makineai_test_cache", ec);
        std::filesystem::remove_all(tempPath / "makineai_test_logs", ec);
    }
};

int main(int argc, char** argv) {
    // Initialize Google Test first
    ::testing::InitGoogleTest(&argc, argv);

    // Register our environment (SetUp is called after test discovery)
    ::testing::AddGlobalTestEnvironment(new MakineAITestEnvironment());

    return RUN_ALL_TESTS();
}
