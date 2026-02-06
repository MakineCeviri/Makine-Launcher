/**
 * @file test_translation_api.cpp
 * @brief Test Translation API with LibreTranslate (free, no API key needed)
 */

#include <iostream>
#include <iomanip>
#include "makineai/translation_api.hpp"

using namespace makineai;

void printResult(const std::string& original, const TranslationResult& result) {
    std::cout << "  Original: " << original << std::endl;
    std::cout << "  Translated: " << result.translatedText << std::endl;
    std::cout << "  Confidence: " << std::fixed << std::setprecision(2)
              << (result.confidence * 100) << "%" << std::endl;
    std::cout << "  Latency: " << result.latency.count() << "ms" << std::endl;
    if (result.detectedLanguage) {
        std::cout << "  Detected: " << *result.detectedLanguage << std::endl;
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "=== MakineAI Translation API Test ===" << std::endl << std::endl;

    // Create API instance
    TranslationAPI api;

    // Configure for LibreTranslate (free, no API key)
    TranslationAPIConfig config;
    config.provider = TranslationProvider::LibreTranslate;
    config.endpoint = "https://libretranslate.com";
    config.cacheResults = true;
    api.configure(config);

    // Test 1: Simple translation
    std::cout << "Test 1: Simple Translation (EN -> TR)" << std::endl;
    std::cout << "--------------------------------------" << std::endl;

    auto result1 = api.translate("Hello, how are you?", "tr", "en");
    if (result1) {
        printResult("Hello, how are you?", *result1);
    } else {
        std::cerr << "Error: " << result1.error().message() << std::endl;
    }

    // Test 2: Game dialogue
    std::cout << "Test 2: Game Dialogue" << std::endl;
    std::cout << "---------------------" << std::endl;

    auto result2 = api.translate(
        "You have defeated the dragon! Your kingdom is saved.",
        "tr", "en"
    );
    if (result2) {
        printResult("You have defeated the dragon! Your kingdom is saved.", *result2);
    } else {
        std::cerr << "Error: " << result2.error().message() << std::endl;
    }

    // Test 3: UI text
    std::cout << "Test 3: UI Text" << std::endl;
    std::cout << "---------------" << std::endl;

    auto result3 = api.translate("New Game", "tr", "en");
    if (result3) {
        printResult("New Game", *result3);
    } else {
        std::cerr << "Error: " << result3.error().message() << std::endl;
    }

    // Test 4: Text with formatting (should preserve {tags})
    std::cout << "Test 4: Formatted Text" << std::endl;
    std::cout << "----------------------" << std::endl;

    auto result4 = api.translate(
        "Welcome, {player_name}! You have {gold} gold.",
        "tr", "en"
    );
    if (result4) {
        printResult("Welcome, {player_name}! You have {gold} gold.", *result4);
    } else {
        std::cerr << "Error: " << result4.error().message() << std::endl;
    }

    // Test 5: Batch translation
    std::cout << "Test 5: Batch Translation" << std::endl;
    std::cout << "-------------------------" << std::endl;

    std::vector<std::string> texts = {
        "Start",
        "Continue",
        "Options",
        "Exit"
    };

    auto batchResult = api.translateBatch(texts, "tr", "en",
        [](int completed, int total) {
            std::cout << "  Progress: " << completed << "/" << total << std::endl;
        });

    if (batchResult) {
        for (size_t i = 0; i < texts.size(); ++i) {
            std::cout << "  " << texts[i] << " -> "
                      << batchResult->results[i].translatedText << std::endl;
        }
        std::cout << "  Total latency: " << batchResult->totalLatency.count() << "ms" << std::endl;
    } else {
        std::cerr << "Error: " << batchResult.error().message() << std::endl;
    }

    // Test 6: Cache test
    std::cout << std::endl << "Test 6: Cache Test" << std::endl;
    std::cout << "------------------" << std::endl;

    auto cached1 = api.translate("Hello, how are you?", "tr", "en");
    if (cached1) {
        std::cout << "  Cached result latency: " << cached1->latency.count() << "ms" << std::endl;
        std::cout << "  (Should be 0ms if cached)" << std::endl;
    }

    // Print usage stats
    std::cout << std::endl << "=== Usage Statistics ===" << std::endl;
    auto stats = api.getTotalUsageStats();
    std::cout << "  Total requests: " << stats.totalRequests << std::endl;
    std::cout << "  Successful: " << stats.successfulRequests << std::endl;
    std::cout << "  Failed: " << stats.failedRequests << std::endl;
    std::cout << "  Total characters: " << stats.totalCharacters << std::endl;
    std::cout << "  Total latency: " << stats.totalLatency.count() << "ms" << std::endl;
    std::cout << "  Cache size: " << api.getCacheSize() << " entries" << std::endl;

    std::cout << std::endl << "=== Test completed ===" << std::endl;
    return 0;
}
