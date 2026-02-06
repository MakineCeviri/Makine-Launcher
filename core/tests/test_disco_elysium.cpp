/**
 * @file test_disco_elysium.cpp
 * @brief Test Unity handler on Disco Elysium
 */

#include <iostream>
#include <iomanip>
#include "makineai/handlers/unity_handler.hpp"
#include "makineai/string_classifier.hpp"

using namespace makineai;

int main() {
    std::cout << "=== MakineAI - Disco Elysium Test ===" << std::endl;
    std::cout << std::endl;

    fs::path gamePath = "D:/SteamLibrary/steamapps/common/Disco Elysium";

    if (!fs::exists(gamePath)) {
        std::cerr << "Game not found at: " << gamePath << std::endl;
        return 1;
    }

    std::cout << "Game: " << gamePath << std::endl;

    UnityHandler handler;

    if (!handler.canHandleGame(gamePath)) {
        std::cerr << "Unity handler cannot process this game" << std::endl;
        return 1;
    }

    std::cout << "Engine: Unity (IL2CPP detected)" << std::endl;
    std::cout << std::endl;

    // Extract strings
    ExtractionOptions opts;
    opts.minLength = 5;
    opts.maxLength = 5000;

    std::cout << "Extracting strings..." << std::endl;
    auto result = handler.extractStrings(gamePath, opts);

    if (!result) {
        std::cerr << "Extraction failed: " << result.error().message() << std::endl;
        return 1;
    }

    std::cout << "Raw extraction: " << result->entries.size() << " strings" << std::endl;
    std::cout << std::endl;

    // Apply string classifier
    std::cout << "Classifying strings..." << std::endl;
    StringClassifier classifier;
    auto stats = classifier.getStats(result->entries);

    std::cout << std::endl;
    std::cout << "=== Classification Results ===" << std::endl;
    std::cout << "Total:          " << stats.total << std::endl;
    std::cout << "Translatable:   " << stats.translatable
              << " (" << std::fixed << std::setprecision(1) << stats.translatablePercent() << "%)" << std::endl;
    std::cout << std::endl;

    std::cout << "Translatable breakdown:" << std::endl;
    std::cout << "  Dialogue:     " << stats.dialogue << std::endl;
    std::cout << "  UI Text:      " << stats.uiText << std::endl;
    std::cout << "  Item Names:   " << stats.itemNames << std::endl;
    std::cout << "  Descriptions: " << stats.descriptions << std::endl;

    std::cout << std::endl;
    std::cout << "Filtered breakdown:" << std::endl;
    std::cout << "  Identifiers:  " << stats.identifiers << std::endl;
    std::cout << "  Code:         " << stats.code << std::endl;
    std::cout << "  Garbage:      " << stats.garbage << std::endl;
    std::cout << "  Debug:        " << stats.debug << std::endl;
    std::cout << "  File Paths:   " << stats.filePaths << std::endl;
    std::cout << "  Unknown:      " << stats.unknown << std::endl;

    // Filter and show sample translatable strings
    auto filtered = classifier.filterTranslatable(result->entries);
    std::cout << std::endl;
    std::cout << "=== Sample Translatable Strings ===" << std::endl;

    int sampleCount = 0;
    for (const auto& entry : filtered) {
        if (sampleCount >= 20) break;

        auto classification = classifier.classify(entry.sourceText);
        if (classification.category == StringCategory::Dialogue ||
            classification.category == StringCategory::UIText) {

            std::string display = entry.sourceText.substr(0, 80);
            for (auto& c : display) {
                if (c == '\n' || c == '\r') c = ' ';
            }
            if (entry.sourceText.length() > 80) display += "...";

            std::cout << "  [" << StringClassifier::categoryToString(classification.category)
                      << "] \"" << display << "\"" << std::endl;
            sampleCount++;
        }
    }

    std::cout << std::endl;
    std::cout << "=== Test Complete ===" << std::endl;
    std::cout << "Final: " << filtered.size() << " translatable strings (after filtering)" << std::endl;

    return 0;
}
