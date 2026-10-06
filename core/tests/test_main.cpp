// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file test_main.cpp
 * @brief Main test runner for Makine Core
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // The catalogue logs every load at info; keep test output to failures.
    spdlog::set_level(spdlog::level::warn);
    return RUN_ALL_TESTS();
}
