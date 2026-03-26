/**
 * @file test_runtime_manager.cpp
 * @brief Unit tests for RuntimeManager stub
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/runtime_manager.hpp>

namespace makine {
namespace testing {

TEST(RuntimeManagerTest, NeedsRuntimeAlwaysFalse) {
    RuntimeManager manager;
    GameInfo game;
    game.engine = GameEngine::Unity_Mono;
    EXPECT_FALSE(manager.needsRuntime(game));
}

TEST(RuntimeManagerTest, NeedsRuntimeOtherEngines) {
    RuntimeManager manager;

    GameInfo unreal;
    unreal.engine = GameEngine::Unreal;
    EXPECT_FALSE(manager.needsRuntime(unreal));

    GameInfo unknown;
    unknown.engine = GameEngine::Unknown;
    EXPECT_FALSE(manager.needsRuntime(unknown));
}

TEST(RuntimeManagerTest, DefaultConstruction) {
    RuntimeManager manager;
    // Should construct and destruct without issues
    SUCCEED();
}

} // namespace testing
} // namespace makine
