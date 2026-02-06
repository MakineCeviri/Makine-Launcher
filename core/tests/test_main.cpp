/**
 * @file test_main.cpp
 * @brief Main test runner for MakineAI Core
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    // Initialize logging for tests
    spdlog::set_level(spdlog::level::warn); // Reduce log noise during tests

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
