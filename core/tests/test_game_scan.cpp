/**
 * @file test_game_scan.cpp
 * @brief Scan D:/Games and test handlers
 */

#include <iostream>
#include <filesystem>
#include <chrono>
#include "makineai/handlers/unity_handler.hpp"
#include "makineai/handlers/unreal_handler.hpp"
#include "makineai/handlers/gamemaker_handler.hpp"
#include "makineai/handlers/renpy_handler.hpp"
#include "makineai/handlers/rpgmaker_handler.hpp"

namespace fs = std::filesystem;
using namespace makineai;

// Detect game engine from directory structure
std::string detectEngine(const fs::path& gamePath) {
    // Unity indicators
    for (const auto& entry : fs::directory_iterator(gamePath)) {
        std::string name = entry.path().filename().string();

        // Unity IL2CPP
        if (name == "GameAssembly.dll") {
            return "Unity IL2CPP";
        }
        // Unity Mono
        if (name == "MonoBleedingEdge" && fs::is_directory(entry)) {
            return "Unity Mono";
        }
        // Unity data folder
        if (name.find("_Data") != std::string::npos && fs::is_directory(entry)) {
            // Check for Unity files inside
            auto dataPath = entry.path();
            if (fs::exists(dataPath / "globalgamemanagers") ||
                fs::exists(dataPath / "mainData") ||
                fs::exists(dataPath / "level0")) {
                if (fs::exists(gamePath / "GameAssembly.dll")) {
                    return "Unity IL2CPP";
                }
                if (fs::exists(gamePath / "MonoBleedingEdge")) {
                    return "Unity Mono";
                }
                return "Unity";
            }
        }
        // Unreal
        if (name == "Engine" && fs::is_directory(entry)) {
            return "Unreal Engine";
        }
        if (name.find(".pak") != std::string::npos) {
            return "Unreal Engine";
        }
        // GameMaker
        if (name == "data.win" || name == "game.ios" || name == "game.droid") {
            return "GameMaker";
        }
        // Ren'Py
        if (name == "renpy" && fs::is_directory(entry)) {
            return "Ren'Py";
        }
        if (name.find(".rpa") != std::string::npos) {
            return "Ren'Py";
        }
        // RPG Maker
        if (name == "www" && fs::is_directory(entry)) {
            return "RPG Maker MV/MZ";
        }
        if (name == "Data" && fs::is_directory(entry)) {
            auto dataPath = entry.path();
            if (fs::exists(dataPath / "Actors.json") || fs::exists(dataPath / "Actors.rvdata2")) {
                return "RPG Maker";
            }
        }
    }
    return "Unknown";
}

void testUnityHandler(const fs::path& gamePath) {
    UnityHandler handler;

    if (!handler.canHandleGame(gamePath)) {
        std::cout << "    Unity handler: Cannot handle this game" << std::endl;
        return;
    }

    std::cout << "    Unity handler: Compatible" << std::endl;

    ExtractionOptions opts;
    opts.minLength = 3;
    opts.maxLength = 5000;

    auto start = std::chrono::steady_clock::now();
    auto result = handler.extractStrings(gamePath, opts);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result) {
        std::cout << "    Extracted: " << result->entries.size() << " strings" << std::endl;
        std::cout << "    Total found: " << result->totalStrings << std::endl;
        std::cout << "    Duration: " << duration.count() << "ms" << std::endl;

        // Show sample
        if (!result->entries.empty()) {
            std::cout << "    Sample: \"" << result->entries[0].sourceText.substr(0, 50) << "...\"" << std::endl;
        }
    } else {
        std::cout << "    Error: " << result.error().message() << std::endl;
    }
}

void testGameMakerHandler(const fs::path& gamePath) {
    GameMakerHandler handler;

    if (!handler.canHandleGame(gamePath)) {
        std::cout << "    GameMaker handler: Cannot handle this game" << std::endl;
        return;
    }

    std::cout << "    GameMaker handler: Compatible" << std::endl;

    ExtractionOptions opts;
    opts.minLength = 5;
    opts.maxLength = 5000;

    auto start = std::chrono::steady_clock::now();
    auto result = handler.extractStrings(gamePath, opts);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result) {
        std::cout << "    Extracted: " << result->entries.size() << " strings" << std::endl;
        std::cout << "    Duration: " << duration.count() << "ms" << std::endl;
    } else {
        std::cout << "    Error: " << result.error().message() << std::endl;
    }
}

int main(int argc, char** argv) {
    std::cout << "=== MakineAI Game Scanner ===" << std::endl << std::endl;

    fs::path gamesDir = "D:/Games";

    if (!fs::exists(gamesDir)) {
        std::cerr << "ERROR: D:/Games not found" << std::endl;
        return 1;
    }

    std::cout << "Scanning: " << gamesDir << std::endl << std::endl;

    int totalGames = 0;
    int unityGames = 0;
    int unrealGames = 0;
    int otherGames = 0;

    for (const auto& entry : fs::directory_iterator(gamesDir)) {
        if (!fs::is_directory(entry)) continue;

        std::string gameName = entry.path().filename().string();
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Game: " << gameName << std::endl;

        std::string engine = detectEngine(entry.path());
        std::cout << "Engine: " << engine << std::endl;

        totalGames++;

        if (engine.find("Unity") != std::string::npos) {
            unityGames++;
            testUnityHandler(entry.path());
        }
        else if (engine == "GameMaker") {
            testGameMakerHandler(entry.path());
        }
        else if (engine.find("Unreal") != std::string::npos) {
            unrealGames++;
            std::cout << "    [Unreal handler test not implemented yet]" << std::endl;
        }
        else {
            otherGames++;
            std::cout << "    [No handler available]" << std::endl;
        }

        std::cout << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total games: " << totalGames << std::endl;
    std::cout << "  Unity games: " << unityGames << std::endl;
    std::cout << "  Unreal games: " << unrealGames << std::endl;
    std::cout << "  Other/Unknown: " << otherGames << std::endl;

    return 0;
}
