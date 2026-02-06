/**
 * @file apply_translation.cpp
 * @brief Example: Applying translations to a game
 * @copyright (c) 2026 MakineAI Team
 *
 * This example demonstrates the complete workflow for applying
 * translations to a game using MakineAI.
 *
 * Workflow:
 * 1. Detect game and engine
 * 2. Create backup
 * 3. Extract strings
 * 4. Find translations (TM lookup)
 * 5. Apply translations
 * 6. Verify integrity
 *
 * Build:
 *   cmake --build . --target example_apply_translation
 *
 * Run:
 *   ./example_apply_translation <game_path>
 */

#include <makineai/core.hpp>
#include <makineai/game_detector.hpp>
#include <makineai/patch_engine.hpp>
#include <makineai/translation_memory.hpp>
#include <makineai/handlers/engine_handler.hpp>

#include <iostream>
#include <iomanip>

using namespace makineai;

/**
 * @brief Print a progress bar
 */
void printProgress(float progress, const std::string& message) {
    int barWidth = 40;
    int pos = static_cast<int>(progress * barWidth);

    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::setw(3) << static_cast<int>(progress * 100) << "% "
              << message << std::string(20, ' ') << std::flush;
}

/**
 * @brief Example: Complete translation workflow
 */
int applyTranslation(const fs::path& gamePath) {
    std::cout << "MakineAI Translation Application Example\n";
    std::cout << "=========================================\n\n";

    // Step 1: Initialize core
    std::cout << "Step 1: Initializing MakineAI...\n";
    auto& core = Core::instance();
    auto initResult = core.initialize();
    if (!initResult) {
        std::cerr << "Failed to initialize: " << initResult.error().message() << "\n";
        return 1;
    }
    std::cout << "  Done.\n\n";

    // Step 2: Detect game
    std::cout << "Step 2: Detecting game at " << gamePath << "...\n";
    auto& detector = core.gameDetector();

    auto detectionResult = detector.detectGame(gamePath);
    if (!detectionResult) {
        std::cerr << "Failed to detect game: " << detectionResult.error().message() << "\n";
        return 1;
    }

    auto game = *detectionResult;
    std::cout << "  Game: " << game.name << "\n";
    std::cout << "  Engine: " << static_cast<int>(game.engine) << "\n";
    std::cout << "\n";

    // Step 3: Get appropriate handler
    std::cout << "Step 3: Getting engine handler...\n";
    auto handlerResult = handlers::EngineHandlerBase::createForEngine(game.engine);
    if (!handlerResult) {
        std::cerr << "No handler for engine: " << handlerResult.error().message() << "\n";
        return 1;
    }

    auto& handler = *handlerResult;
    std::cout << "  Handler: " << handler->engineName() << "\n\n";

    // Step 4: Create backup
    std::cout << "Step 4: Creating backup...\n";
    auto backupResult = handler->createBackup(game);
    if (!backupResult) {
        std::cerr << "Backup failed: " << backupResult.error().message() << "\n";
        return 1;
    }

    std::cout << "  Backup created at: " << backupResult->backupPath << "\n\n";

    // Step 5: Extract strings
    std::cout << "Step 5: Extracting strings...\n";
    auto extractResult = handler->extractStrings(game, [](float p, const std::string& m) {
        printProgress(p, m);
    });

    if (!extractResult) {
        std::cerr << "\nExtraction failed: " << extractResult.error().message() << "\n";
        return 1;
    }

    auto strings = *extractResult;
    std::cout << "\n  Extracted " << strings.size() << " strings.\n\n";

    // Step 6: Find translations from TM
    std::cout << "Step 6: Finding translations...\n";
    std::vector<TranslationEntry> translations;
    int exactMatches = 0;
    int fuzzyMatches = 0;

    for (size_t i = 0; i < strings.size(); ++i) {
        const auto& str = strings[i];

        // Show progress every 100 strings
        if (i % 100 == 0) {
            printProgress(static_cast<float>(i) / strings.size(), "Looking up...");
        }

        // Try exact match first
        auto exactResult = TranslationMemoryService::findExactMatch(str.sourceText);
        if (exactResult && *exactResult) {
            TranslationEntry entry;
            entry.sourceText = str.sourceText;
            entry.targetText = (*exactResult)->targetText;
            entry.status = EntryStatus::Translated;
            translations.push_back(entry);
            exactMatches++;
            continue;
        }

        // Try fuzzy match
        TranslationMemoryService::MatchContext ctx;
        ctx.gameId = game.id.full();

        auto fuzzyResult = TranslationMemoryService::findBestMatch(str.sourceText, ctx);
        if (fuzzyResult && *fuzzyResult && (*fuzzyResult)->score >= 70.0) {
            TranslationEntry entry;
            entry.sourceText = str.sourceText;
            entry.targetText = (*fuzzyResult)->entry.targetText;
            entry.status = EntryStatus::NeedsReview;
            translations.push_back(entry);
            fuzzyMatches++;
        }
    }

    std::cout << "\n  Exact matches: " << exactMatches << "\n";
    std::cout << "  Fuzzy matches: " << fuzzyMatches << "\n";
    std::cout << "  Total: " << translations.size() << "/" << strings.size() << "\n\n";

    if (translations.empty()) {
        std::cout << "No translations found. Nothing to apply.\n";
        return 0;
    }

    // Step 7: Apply translations
    std::cout << "Step 7: Applying translations...\n";
    auto applyResult = handler->applyTranslations(game, translations, [](float p, const std::string& m) {
        printProgress(p, m);
    });

    if (!applyResult) {
        std::cerr << "\nApply failed: " << applyResult.error().message() << "\n";

        // Rollback
        std::cout << "\nRolling back...\n";
        auto restoreResult = handler->restoreBackup(game, backupResult->backupId);
        if (restoreResult) {
            std::cout << "Rollback successful.\n";
        }
        return 1;
    }

    std::cout << "\n  Applied " << applyResult->stringsPatched << " translations.\n\n";

    // Step 8: Verify integrity
    std::cout << "Step 8: Verifying integrity...\n";
    auto verifyResult = core.patchEngine().verifyIntegrity(game);
    if (!verifyResult) {
        std::cerr << "Verification failed: " << verifyResult.error().message() << "\n";
        return 1;
    }

    if (!*verifyResult) {
        std::cerr << "Integrity check failed! Rolling back...\n";
        handler->restoreBackup(game, backupResult->backupId);
        return 1;
    }

    std::cout << "  Integrity verified.\n\n";

    // Done
    std::cout << "Translation applied successfully!\n";
    std::cout << "\nTo restore original files, use:\n";
    std::cout << "  makineai restore --backup=" << backupResult->backupId << "\n";

    core.shutdown();
    return 0;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <game_path>\n";
        std::cerr << "\nExample:\n";
        std::cerr << "  " << argv[0] << " \"C:/Program Files (x86)/Steam/steamapps/common/MyGame\"\n";
        return 1;
    }

    fs::path gamePath = argv[1];
    if (!fs::exists(gamePath)) {
        std::cerr << "Path does not exist: " << gamePath << "\n";
        return 1;
    }

    try {
        return applyTranslation(gamePath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
