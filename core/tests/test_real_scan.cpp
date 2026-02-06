/**
 * @file test_real_scan.cpp
 * @brief Real-world scanner test (not part of regular test suite)
 */

#include <iostream>
#include <makineai/game_detector.hpp>

int main() {
    std::cout << "=== MakineAI Real Scanner Test ===" << std::endl;

    // Test Steam Scanner
    std::cout << "\n--- Steam Scanner ---" << std::endl;
    makineai::SteamScanner steam;

    if (steam.isAvailable()) {
        std::cout << "Steam: Available" << std::endl;
        auto result = steam.scan();
        if (result) {
            std::cout << "Found " << result->size() << " Steam games:" << std::endl;
            int count = 0;
            for (const auto& game : *result) {
                if (++count > 10) {
                    std::cout << "  ... and " << (result->size() - 10) << " more" << std::endl;
                    break;
                }
                std::cout << "  - " << game.name << " (" << game.id.storeId << ")" << std::endl;
            }
        } else {
            std::cout << "Scan failed: " << result.error().message() << std::endl;
        }
    } else {
        std::cout << "Steam: Not available" << std::endl;
    }

    // Test Epic Scanner
    std::cout << "\n--- Epic Games Scanner ---" << std::endl;
    makineai::EpicScanner epic;

    if (epic.isAvailable()) {
        std::cout << "Epic: Available" << std::endl;
        auto result = epic.scan();
        if (result) {
            std::cout << "Found " << result->size() << " Epic games:" << std::endl;
            for (const auto& game : *result) {
                std::cout << "  - " << game.name << std::endl;
            }
        } else {
            std::cout << "Scan failed: " << result.error().message() << std::endl;
        }
    } else {
        std::cout << "Epic: Not available" << std::endl;
    }

    // Test GOG Scanner
    std::cout << "\n--- GOG Galaxy Scanner ---" << std::endl;
    makineai::GOGScanner gog;

    if (gog.isAvailable()) {
        std::cout << "GOG: Available" << std::endl;
        auto result = gog.scan();
        if (result) {
            std::cout << "Found " << result->size() << " GOG games:" << std::endl;
            for (const auto& game : *result) {
                std::cout << "  - " << game.name << std::endl;
            }
        } else {
            std::cout << "Scan failed: " << result.error().message() << std::endl;
        }
    } else {
        std::cout << "GOG: Not available" << std::endl;
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
