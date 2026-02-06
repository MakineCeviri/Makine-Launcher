/**
 * @file test_unity.cpp
 * @brief Real game test with The Red Lantern (Unity)
 */

#include <iostream>
#include <filesystem>
#include "makineai/handlers/unity_handler.hpp"

namespace fs = std::filesystem;
using namespace makineai;

int main(int argc, char** argv) {
    std::cout << "=== MakineAI Unity Test ===" << std::endl;

    // Path to The Red Lantern (Unity game with BepInEx)
    fs::path gamePath = "D:/Games/The Red Lantern";

    if (!fs::exists(gamePath)) {
        std::cerr << "ERROR: Game not found at: " << gamePath << std::endl;
        return 1;
    }

    std::cout << "Found game directory: " << gamePath << std::endl;

    // Check for Unity files
    fs::path dataFolder = gamePath / "TheRedLantern_Data";
    if (!fs::exists(dataFolder)) {
        std::cerr << "ERROR: Unity data folder not found" << std::endl;
        return 1;
    }
    std::cout << "Found Unity data folder" << std::endl;

    // Create handler
    UnityHandler handler;

    // Check if handler can handle this game
    if (!handler.canHandleGame(gamePath)) {
        std::cerr << "ERROR: Unity handler cannot handle this game" << std::endl;
        return 1;
    }
    std::cout << "Unity handler confirmed game is compatible" << std::endl;

    // Extract strings
    ExtractionOptions opts;
    opts.minLength = 5;
    opts.maxLength = 5000;

    std::cout << "Extracting strings..." << std::endl;

    auto extractResult = handler.extractStrings(gamePath, opts);
    if (!extractResult) {
        std::cerr << "ERROR: Extraction failed: " << extractResult.error().message() << std::endl;
        return 1;
    }

    auto& result = *extractResult;
    std::cout << "Extracted " << result.entries.size() << " strings!" << std::endl;
    std::cout << "Total strings found: " << result.totalStrings << std::endl;
    std::cout << "Skipped: " << result.skippedStrings << std::endl;

    if (!result.errors.empty()) {
        std::cout << "Warnings/Errors: " << result.errors.size() << std::endl;
    }

    // Print first 20 strings
    std::cout << "\nSample strings:" << std::endl;
    int count = 0;
    for (const auto& entry : result.entries) {
        if (count++ >= 20) break;
        std::string text = entry.sourceText.substr(0, 60);
        std::string ctx = entry.context.value_or("unknown");
        std::cout << "  [" << ctx << "] " << text;
        if (entry.sourceText.length() > 60) std::cout << "...";
        std::cout << std::endl;
    }

    std::cout << "\n=== Test completed successfully ===" << std::endl;
    return 0;
}
