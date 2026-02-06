/**
 * @file basic_scan.cpp
 * @brief Example: Scanning for installed games
 * @copyright (c) 2026 MakineAI Team
 *
 * This example demonstrates how to use MakineAI to scan for installed games
 * from multiple stores (Steam, Epic, GOG).
 *
 * Build:
 *   cmake --build . --target example_basic_scan
 *
 * Run:
 *   ./example_basic_scan
 */

#include <makineai/core.hpp>
#include <makineai/game_detector.hpp>
#include <makineai/scanner_base.hpp>

#include <iostream>
#include <iomanip>

using namespace makineai;

/**
 * @brief Print a divider line
 */
void printDivider() {
    std::cout << std::string(60, '-') << "\n";
}

/**
 * @brief Convert GameEngine to string
 */
std::string engineToString(GameEngine engine) {
    switch (engine) {
        case GameEngine::Unity: return "Unity";
        case GameEngine::UnityIL2CPP: return "Unity IL2CPP";
        case GameEngine::Unreal: return "Unreal Engine";
        case GameEngine::RPGMaker: return "RPG Maker";
        case GameEngine::RenPy: return "Ren'Py";
        case GameEngine::GameMaker: return "GameMaker";
        case GameEngine::Godot: return "Godot";
        case GameEngine::CryEngine: return "CryEngine";
        case GameEngine::Source: return "Source";
        case GameEngine::Bethesda: return "Bethesda";
        case GameEngine::Custom: return "Custom";
        case GameEngine::Unknown: return "Unknown";
        default: return "?";
    }
}

/**
 * @brief Convert GameStore to string
 */
std::string storeToString(GameStore store) {
    switch (store) {
        case GameStore::Steam: return "Steam";
        case GameStore::Epic: return "Epic Games";
        case GameStore::GOG: return "GOG Galaxy";
        case GameStore::XboxGamePass: return "Xbox Game Pass";
        case GameStore::Manual: return "Manual";
        case GameStore::Unknown: return "Unknown";
        default: return "?";
    }
}

/**
 * @brief Example 1: Simple scan using GameDetector
 */
void example_simple_scan() {
    std::cout << "\n=== Example 1: Simple Game Scan ===\n";
    printDivider();

    // Initialize core
    auto& core = Core::instance();
    auto initResult = core.initialize();
    if (!initResult) {
        std::cerr << "Failed to initialize: " << initResult.error().message() << "\n";
        return;
    }

    // Get game detector
    auto& detector = core.gameDetector();

    // Scan all stores
    std::cout << "Scanning for installed games...\n\n";
    auto result = detector.scanAll();

    if (!result) {
        std::cerr << "Scan failed: " << result.error().message() << "\n";
        return;
    }

    const auto& games = *result;
    if (games.empty()) {
        std::cout << "No games found.\n";
        return;
    }

    std::cout << "Found " << games.size() << " games:\n\n";

    for (const auto& game : games) {
        std::cout << "  " << game.name << "\n";
        std::cout << "    Store:  " << storeToString(game.store) << "\n";
        std::cout << "    Engine: " << engineToString(game.engine) << "\n";
        std::cout << "    Path:   " << game.installPath.string() << "\n";
        if (!game.version.empty()) {
            std::cout << "    Version: " << game.version << "\n";
        }
        std::cout << "\n";
    }

    core.shutdown();
}

/**
 * @brief Example 2: Scan with progress reporting
 */
void example_scan_with_progress() {
    std::cout << "\n=== Example 2: Scan with Progress ===\n";
    printDivider();

    // Initialize core
    auto& core = Core::instance();
    auto initResult = core.initialize();
    if (!initResult) {
        std::cerr << "Failed to initialize: " << initResult.error().message() << "\n";
        return;
    }

    // Get game detector
    auto& detector = core.gameDetector();

    // Scan with progress callback
    std::cout << "Scanning with progress reporting...\n\n";

    auto result = detector.scanAll([](float progress, const std::string& message) {
        int percent = static_cast<int>(progress * 100);
        std::cout << "\r[" << std::setw(3) << percent << "%] " << message
                  << std::string(20, ' ') << std::flush;
    });

    if (!result) {
        std::cerr << "\n\nScan failed: " << result.error().message() << "\n";
        return;
    }

    std::cout << "\n\nFound " << result->size() << " games.\n";

    core.shutdown();
}

/**
 * @brief Example 3: Filter games by engine
 */
void example_filter_by_engine() {
    std::cout << "\n=== Example 3: Filter by Engine ===\n";
    printDivider();

    // Initialize core
    auto& core = Core::instance();
    auto initResult = core.initialize();
    if (!initResult) {
        std::cerr << "Failed to initialize: " << initResult.error().message() << "\n";
        return;
    }

    // Get game detector
    auto& detector = core.gameDetector();

    // Scan all games
    auto result = detector.scanAll();
    if (!result) {
        std::cerr << "Scan failed: " << result.error().message() << "\n";
        return;
    }

    const auto& allGames = *result;

    // Filter Unity games
    std::vector<GameInfo> unityGames;
    for (const auto& game : allGames) {
        if (game.engine == GameEngine::Unity ||
            game.engine == GameEngine::UnityIL2CPP) {
            unityGames.push_back(game);
        }
    }

    std::cout << "Unity games (" << unityGames.size() << "/" << allGames.size() << "):\n\n";

    for (const auto& game : unityGames) {
        std::cout << "  - " << game.name;
        if (game.engine == GameEngine::UnityIL2CPP) {
            std::cout << " (IL2CPP)";
        }
        std::cout << "\n";
    }

    if (unityGames.empty()) {
        std::cout << "  (No Unity games found)\n";
    }

    core.shutdown();
}

/**
 * @brief Example 4: Using ScannerRegistry (advanced)
 */
void example_scanner_registry() {
    std::cout << "\n=== Example 4: Scanner Registry ===\n";
    printDivider();

    // Get scanner registry
    auto& registry = scanners::ScannerRegistry::instance();

    // List available scanners
    std::cout << "Available scanners:\n\n";
    for (const auto& scanner : registry.getScanners()) {
        std::cout << "  - " << scanner->name()
                  << " (Store: " << storeToString(scanner->store()) << ")"
                  << " [Available: " << (scanner->isAvailable() ? "Yes" : "No") << "]\n";
    }

    std::cout << "\nScanning with all available scanners...\n";

    // Create scan context
    ScanContext ctx;
    ctx.useCache = true;
    ctx.maxCacheAgeSeconds = 300;  // 5 minutes

    // Scan all
    auto result = registry.scanAll(ctx);

    std::cout << "\nResults:\n";
    std::cout << "  Games found: " << result.games.size() << "\n";
    std::cout << "  Errors:      " << result.errors.size() << "\n";
    std::cout << "  Warnings:    " << result.warnings.size() << "\n";
    std::cout << "  Duration:    " << result.durationMs << " ms\n";
    std::cout << "  From cache:  " << (result.fromCache ? "Yes" : "No") << "\n";
}

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "MakineAI Game Scanning Examples\n";
    std::cout << "================================\n";

    try {
        example_simple_scan();
        example_scan_with_progress();
        example_filter_by_engine();
        example_scanner_registry();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\nAll examples completed.\n";
    return 0;
}
