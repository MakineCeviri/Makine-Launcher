/**
 * @file mock_scanner.hpp
 * @brief Mock implementations of game scanner interfaces for testing
 *
 * Provides Google Mock implementations of IGameScanner and related interfaces
 * for unit testing without requiring actual game installations.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <gmock/gmock.h>
#include "makine/game_detector.hpp"

namespace makine::testing {

/**
 * @brief Mock implementation of IGameScanner
 *
 * Usage:
 * @code
 * MockScanner scanner;
 * EXPECT_CALL(scanner, scan())
 *     .WillOnce(Return(std::vector<GameInfo>{{...}}));
 * @endcode
 */
class MockScanner : public scanners::IGameScanner {
public:
    MOCK_METHOD(std::string_view, name, (), (const, noexcept, override));
    MOCK_METHOD(GameStore, storeType, (), (const, noexcept, override));
    MOCK_METHOD(bool, isAvailable, (), (const, override));
    MOCK_METHOD(Result<std::vector<GameInfo>>, scan, (), (const, override));
};

/**
 * @brief Mock implementation of GameDetector
 *
 * Allows testing code that depends on game detection without actual games.
 */
class MockGameDetector {
public:
    MOCK_METHOD(Result<scanners::EngineDetectionResult>, detectEngine,
                (const fs::path& gamePath), (const));
    MOCK_METHOD(Result<std::vector<GameInfo>>, scanAll, (), (const));
    MOCK_METHOD(Result<std::vector<GameInfo>>, scanStore, (GameStore store), (const));
};

/**
 * @brief Fake scanner that returns predefined games
 *
 * Useful for deterministic testing without mocking.
 */
class FakeScanner : public scanners::IGameScanner {
public:
    explicit FakeScanner(
        std::string_view name,
        GameStore store,
        std::vector<GameInfo> games = {}
    )
        : name_(name)
        , store_(store)
        , games_(std::move(games))
        , available_(true)
    {}

    [[nodiscard]] std::string_view name() const noexcept override {
        return name_;
    }

    [[nodiscard]] GameStore storeType() const noexcept override {
        return store_;
    }

    [[nodiscard]] bool isAvailable() const override {
        return available_;
    }

    [[nodiscard]] Result<std::vector<GameInfo>> scan() const override {
        if (!available_) {
            return std::unexpected(Error(ErrorCode::NotFound, "Scanner not available"));
        }
        return games_;
    }

    // Test helpers
    void setAvailable(bool available) { available_ = available; }
    void setGames(std::vector<GameInfo> games) { games_ = std::move(games); }
    void addGame(GameInfo game) { games_.push_back(std::move(game)); }
    void clearGames() { games_.clear(); }

private:
    std::string name_;
    GameStore store_;
    std::vector<GameInfo> games_;
    bool available_;
};

/**
 * @brief Create a fake GameInfo for testing
 */
inline GameInfo createFakeGameInfo(
    const std::string& name,
    GameEngine engine,
    GameStore store = GameStore::Unknown,
    const std::string& installPath = "C:\\Games\\FakeGame"
) {
    GameInfo info;
    info.name = name;
    info.engine = engine;
    info.store = store;
    info.installPath = installPath;
    info.gameId.storeId = "fake_" + name;
    return info;
}

/**
 * @brief Create a fake EngineDetectionResult for testing
 */
inline scanners::EngineDetectionResult createFakeDetectionResult(
    GameEngine engine,
    float confidence = 0.9f,
    const std::string& version = "1.0.0"
) {
    scanners::EngineDetectionResult result;
    result.engine = engine;
    result.confidence = confidence;
    result.version = version;
    return result;
}

} // namespace makine::testing
