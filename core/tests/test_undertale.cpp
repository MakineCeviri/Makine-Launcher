/**
 * @file test_undertale.cpp
 * @brief Real game test with Undertale (GameMaker)
 */

#include <iostream>
#include <filesystem>
#include "makineai/handlers/gamemaker_handler.hpp"

namespace fs = std::filesystem;
using namespace makineai;

int main(int argc, char** argv) {
    std::cout << "=== MakineAI Undertale Test ===" << std::endl;

    // Path to Undertale
    fs::path gamePath = "C:/Users/Administrator/Desktop/Makine/Undertale v1.001";
    fs::path dataWin = gamePath / "data.win";

    if (!fs::exists(dataWin)) {
        std::cerr << "ERROR: data.win not found at: " << dataWin << std::endl;
        return 1;
    }

    std::cout << "Found data.win: " << fs::file_size(dataWin) << " bytes" << std::endl;

    // Create handler
    GameMakerHandler handler;

    // Check if handler can handle this game
    if (!handler.canHandleGame(gamePath)) {
        std::cerr << "ERROR: GameMaker handler cannot handle this game" << std::endl;
        return 1;
    }
    std::cout << "GameMaker handler confirmed game is compatible" << std::endl;

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
