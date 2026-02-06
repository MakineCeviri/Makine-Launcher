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
        testConfig.dataDirectory = std::filesystem::temp_directory_path() / "makineai_test_data";
        testConfig.cacheDirectory = std::filesystem::temp_directory_path() / "makineai_test_cache";
        testConfig.logsDirectory = std::filesystem::temp_directory_path() / "makineai_test_logs";
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
        // Cleanup
        auto& core = makineai::Core::instance();
        if (core.isInitialized()) {
            core.shutdown();
        }

        // Flush and drop all loggers to release file handles
        spdlog::drop_all();
        spdlog::shutdown();

        // Small delay to ensure file handles are released
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto tempPath = std::filesystem::temp_directory_path();
        std::error_code ec;
        std::filesystem::remove_all(tempPath / "makineai_test_data", ec);
        std::filesystem::remove_all(tempPath / "makineai_test_cache", ec);
        std::filesystem::remove_all(tempPath / "makineai_test_logs", ec);
        // Ignore errors during cleanup
    }
};

int main(int argc, char** argv) {
    // Initialize Google Test first
    ::testing::InitGoogleTest(&argc, argv);

    // Register our environment (SetUp is called after test discovery)
    ::testing::AddGlobalTestEnvironment(new MakineAITestEnvironment());

    return RUN_ALL_TESTS();
}
