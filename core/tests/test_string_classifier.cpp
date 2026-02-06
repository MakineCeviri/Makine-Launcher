/**
 * @file test_string_classifier.cpp
 * @brief Test string classifier on real game data
 *
 * Demonstrates filtering of non-translatable strings:
 * - Code/technical strings
 * - File paths
 * - Variable names
 * - Debug messages
 */

#include <iostream>
#include <iomanip>
#include <map>
#include "makineai/string_classifier.hpp"
#include "makineai/handlers/unity_handler.hpp"

using namespace makineai;

void printCategorySamples(
    const std::vector<std::pair<std::string, ClassificationResult>>& results,
    StringCategory category,
    int maxSamples = 5)
{
    std::cout << "\n  Samples:" << std::endl;
    int count = 0;
    for (const auto& [text, result] : results) {
        if (result.category == category && count < maxSamples) {
            // Truncate long strings
            std::string display = text.substr(0, 60);
            if (text.length() > 60) display += "...";
            // Replace newlines
            for (auto& c : display) {
                if (c == '\n' || c == '\r') c = ' ';
            }
            std::cout << "    - \"" << display << "\"" << std::endl;
            count++;
        }
    }
}

int main(int argc, char** argv) {
    std::cout << "=== MakineAI String Classifier Test ===" << std::endl;
    std::cout << "Demonstrates filtering of non-translatable strings" << std::endl;
    std::cout << std::endl;

    // Test with hardcoded samples first
    std::cout << "=== Test 1: Classification Examples ===" << std::endl;

    StringClassifier classifier;

    std::vector<std::string> testStrings = {
        // Translatable
        "Hello, how are you today?",
        "Press START to begin",
        "Health Potion",
        "You have defeated the dragon!",
        "New Game",
        "Continue",
        "Exit",

        // Code/Technical
        "Assets/Prefabs/Player.prefab",
        "C:\\Users\\Admin\\Documents\\game.exe",
        "https://api.game.com/v2/auth",
        "playerHealth_MaxValue",
        "DEBUG: Loading asset bundle",
        "[ERROR] Failed to connect",
        "NullReferenceException",

        // Markup/JSON
        "{\"name\": \"test\"}",
        "<color=#FF0000>Red</color>",
        "<?xml version=\"1.0\"?>",

        // Identifiers
        "PlayerController",
        "gameObject_001",
        "CONST_MAX_HEALTH",
        "m_privateField",

        // Numeric
        "123456789",
        "v1.2.3-beta",
        "2024-01-15",

        // Garbage
        "xkcd!@#$%^&*()",
        "\x00\x01\x02\x03",
        "aaaaaaaaaaaaaaaa"
    };

    std::map<StringCategory, std::vector<std::string>> categorized;

    for (const auto& text : testStrings) {
        auto result = classifier.classify(text);
        categorized[result.category].push_back(text);

        std::cout << std::left << std::setw(35) << text.substr(0, 34)
                  << " -> " << std::setw(12) << StringClassifier::categoryToString(result.category)
                  << " (" << (result.isTranslatable ? "TRANSLATE" : "SKIP") << ")"
                  << std::endl;
    }

    // Summary
    std::cout << std::endl << "Classification Summary:" << std::endl;
    for (const auto& [category, texts] : categorized) {
        std::cout << "  " << StringClassifier::categoryToString(category)
                  << ": " << texts.size() << std::endl;
    }

    // Test 2: Try with real game data if available
    std::cout << std::endl << "=== Test 2: Real Game Data (if available) ===" << std::endl;

    // Try common Unity game locations
    std::vector<std::string> gamePaths = {
        "D:/Games/Afterparty",
        "D:/Games/Storyteller",
        "D:/Games/The Red Lantern",
        "D:/SteamLibrary/steamapps/common/Afterparty"
    };

    UnityHandler unityHandler;
    bool foundGame = false;

    for (const auto& gamePath : gamePaths) {
        if (!std::filesystem::exists(gamePath)) continue;
        if (!unityHandler.canHandleGame(gamePath)) continue;

        std::cout << "Found Unity game: " << gamePath << std::endl;
        foundGame = true;

        ExtractionOptions opts;
        opts.minLength = 3;
        opts.maxLength = 5000;

        auto extractResult = unityHandler.extractStrings(gamePath, opts);
        if (!extractResult) {
            std::cout << "Extraction failed: " << extractResult.error().message() << std::endl;
            continue;
        }

        std::cout << "Extracted " << extractResult->entries.size() << " raw strings" << std::endl;

        // Classify all strings
        std::cout << "Classifying..." << std::endl;

        std::vector<std::pair<std::string, ClassificationResult>> classifiedStrings;
        ClassificationStats stats;
        stats.total = static_cast<int>(extractResult->entries.size());

        for (const auto& entry : extractResult->entries) {
            auto result = classifier.classify(entry.sourceText);
            classifiedStrings.push_back({entry.sourceText, result});

            if (result.isTranslatable) stats.translatable++;

            switch (result.category) {
                case StringCategory::Dialogue: stats.dialogue++; break;
                case StringCategory::UIText: stats.uiText++; break;
                case StringCategory::ItemName: stats.itemNames++; break;
                case StringCategory::Description: stats.descriptions++; break;
                case StringCategory::Code: stats.code++; break;
                case StringCategory::FilePath: stats.filePaths++; break;
                case StringCategory::Identifier: stats.identifiers++; break;
                case StringCategory::Debug: stats.debug++; break;
                case StringCategory::Garbage: stats.garbage++; break;
                default: stats.unknown++; break;
            }
        }

        // Print detailed statistics
        std::cout << std::endl << "=== Classification Statistics ===" << std::endl;
        std::cout << "Total strings:       " << stats.total << std::endl;
        std::cout << "Translatable:        " << stats.translatable
                  << " (" << std::fixed << std::setprecision(1) << stats.translatablePercent() << "%)" << std::endl;
        std::cout << "Filtered out:        " << (stats.total - stats.translatable)
                  << " (" << std::fixed << std::setprecision(1) << (100.0f - stats.translatablePercent()) << "%)" << std::endl;

        std::cout << std::endl << "Translatable breakdown:" << std::endl;
        std::cout << "  Dialogue:          " << stats.dialogue << std::endl;
        std::cout << "  UI Text:           " << stats.uiText << std::endl;
        std::cout << "  Item Names:        " << stats.itemNames << std::endl;
        std::cout << "  Descriptions:      " << stats.descriptions << std::endl;

        std::cout << std::endl << "Filtered breakdown:" << std::endl;
        std::cout << "  Code:              " << stats.code << std::endl;
        std::cout << "  File Paths:        " << stats.filePaths << std::endl;
        std::cout << "  Identifiers:       " << stats.identifiers << std::endl;
        std::cout << "  Debug Messages:    " << stats.debug << std::endl;
        std::cout << "  Garbage:           " << stats.garbage << std::endl;
        std::cout << "  Unknown:           " << stats.unknown << std::endl;

        // Show samples from each category
        std::cout << std::endl << "=== Sample Strings by Category ===" << std::endl;

        std::cout << std::endl << "TRANSLATABLE - Dialogue:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::Dialogue, 5);

        std::cout << std::endl << "TRANSLATABLE - UI Text:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::UIText, 5);

        std::cout << std::endl << "FILTERED - Code:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::Code, 5);

        std::cout << std::endl << "FILTERED - File Paths:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::FilePath, 5);

        std::cout << std::endl << "FILTERED - Identifiers:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::Identifier, 5);

        std::cout << std::endl << "FILTERED - Garbage:" << std::endl;
        printCategorySamples(classifiedStrings, StringCategory::Garbage, 5);

        // Use filtered entries
        auto filteredEntries = classifier.filterTranslatable(extractResult->entries);
        std::cout << std::endl << "=== Final Result ===" << std::endl;
        std::cout << "Original: " << extractResult->entries.size() << " strings" << std::endl;
        std::cout << "After filtering: " << filteredEntries.size() << " translatable strings" << std::endl;
        std::cout << "Reduction: " << std::fixed << std::setprecision(1)
                  << ((1.0 - (double)filteredEntries.size() / extractResult->entries.size()) * 100)
                  << "%" << std::endl;

        break; // Test with first found game
    }

    if (!foundGame) {
        std::cout << "No Unity game found in common locations." << std::endl;
        std::cout << "Run test_game_scan first to see available games." << std::endl;
    }

    std::cout << std::endl << "=== Test Complete ===" << std::endl;
    return 0;
}
