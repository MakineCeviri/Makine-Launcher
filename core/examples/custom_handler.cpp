/**
 * @file custom_handler.cpp
 * @brief Example: Creating a custom engine handler
 * @copyright (c) 2026 MakineAI Team
 *
 * This example demonstrates how to create a custom engine handler
 * for a hypothetical game engine format.
 *
 * The handler implements the IEngineHandler interface and can be
 * registered with the handler registry.
 *
 * Build:
 *   cmake --build . --target example_custom_handler
 */

#include <makineai/handlers/engine_handler.hpp>
#include <makineai/types.hpp>
#include <makineai/error.hpp>
#include <makineai/logging.hpp>

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

using namespace makineai;
using namespace makineai::handlers;

/**
 * @brief Custom handler for a hypothetical "SimpleText" engine
 *
 * The SimpleText engine stores strings in .stxt files with format:
 * ```
 * [SECTION_NAME]
 * key1=English text here
 * key2=Another string
 * ```
 *
 * This handler:
 * - Detects games with .stxt files
 * - Extracts strings from .stxt files
 * - Applies translations by modifying the values
 */
class SimpleTextHandler : public EngineHandlerBase {
public:
    // =========== IDENTIFICATION ===========

    /**
     * @brief Get handler name
     */
    [[nodiscard]] std::string name() const override {
        return "SimpleTextHandler";
    }

    /**
     * @brief Get engine name
     */
    [[nodiscard]] std::string engineName() const override {
        return "SimpleText Engine";
    }

    /**
     * @brief Get supported engine type
     */
    [[nodiscard]] GameEngine engineType() const override {
        return GameEngine::Custom;  // Use Custom for non-standard engines
    }

    // =========== DETECTION ===========

    /**
     * @brief Check if this handler can process the game
     *
     * We detect SimpleText games by looking for:
     * 1. A data/strings directory
     * 2. .stxt files in that directory
     */
    [[nodiscard]] bool canHandle(const GameInfo& game) const override {
        auto stringsDir = game.installPath / "data" / "strings";
        if (!fs::exists(stringsDir) || !fs::is_directory(stringsDir)) {
            return false;
        }

        // Look for .stxt files
        for (const auto& entry : fs::directory_iterator(stringsDir)) {
            if (entry.path().extension() == ".stxt") {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Get confidence level for handling this game
     */
    [[nodiscard]] int confidence(const GameInfo& game) const override {
        if (!canHandle(game)) {
            return 0;
        }

        // High confidence if we find the marker file
        auto markerFile = game.installPath / "data" / "engine.simpletext";
        if (fs::exists(markerFile)) {
            return 95;
        }

        // Medium confidence otherwise
        return 70;
    }

    // =========== STRING EXTRACTION ===========

    /**
     * @brief Extract translatable strings from the game
     */
    [[nodiscard]] Result<std::vector<StringEntry>> extractStrings(
        const GameInfo& game,
        ProgressCallback progress = nullptr
    ) const override {
        HANDLER_LOG_INFO("Extracting strings from SimpleText game: {}", game.name);

        std::vector<StringEntry> strings;
        auto stringsDir = game.installPath / "data" / "strings";

        // Collect all .stxt files
        std::vector<fs::path> stxtFiles;
        for (const auto& entry : fs::recursive_directory_iterator(stringsDir)) {
            if (entry.path().extension() == ".stxt") {
                stxtFiles.push_back(entry.path());
            }
        }

        if (stxtFiles.empty()) {
            return Error(ErrorCode::NotFound, "No .stxt files found");
        }

        HANDLER_LOG_INFO("Found {} .stxt files", stxtFiles.size());

        // Process each file
        for (size_t i = 0; i < stxtFiles.size(); ++i) {
            const auto& file = stxtFiles[i];

            if (progress) {
                float p = static_cast<float>(i) / stxtFiles.size();
                progress(p, "Processing: " + file.filename().string());
            }

            auto result = extractFromFile(file, game.installPath);
            if (result) {
                auto& fileStrings = *result;
                strings.insert(strings.end(), fileStrings.begin(), fileStrings.end());
            }
        }

        if (progress) {
            progress(1.0f, "Extraction complete");
        }

        HANDLER_LOG_INFO("Extracted {} strings", strings.size());
        return strings;
    }

    // =========== TRANSLATION APPLICATION ===========

    /**
     * @brief Apply translations to the game
     */
    [[nodiscard]] Result<PatchResult> applyTranslations(
        const GameInfo& game,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress = nullptr
    ) const override {
        HANDLER_LOG_INFO("Applying {} translations to {}", translations.size(), game.name);

        // Build lookup map for fast access
        std::unordered_map<std::string, std::string> translationMap;
        for (const auto& entry : translations) {
            if (!entry.targetText.empty()) {
                translationMap[entry.sourceText] = entry.targetText;
            }
        }

        auto stringsDir = game.installPath / "data" / "strings";
        int totalPatched = 0;

        // Collect all .stxt files
        std::vector<fs::path> stxtFiles;
        for (const auto& entry : fs::recursive_directory_iterator(stringsDir)) {
            if (entry.path().extension() == ".stxt") {
                stxtFiles.push_back(entry.path());
            }
        }

        // Process each file
        for (size_t i = 0; i < stxtFiles.size(); ++i) {
            const auto& file = stxtFiles[i];

            if (progress) {
                float p = static_cast<float>(i) / stxtFiles.size();
                progress(p, "Patching: " + file.filename().string());
            }

            auto result = patchFile(file, translationMap);
            if (result) {
                totalPatched += *result;
            }
        }

        if (progress) {
            progress(1.0f, "Patching complete");
        }

        PatchResult result;
        result.status = PatchStatus::Success;
        result.stringsPatched = totalPatched;
        result.message = "Applied " + std::to_string(totalPatched) + " translations";

        HANDLER_LOG_INFO("Patched {} strings", totalPatched);
        return result;
    }

    // =========== VALIDATION ===========

    /**
     * @brief Validate the patch can be applied
     */
    [[nodiscard]] Result<bool> validatePatch(
        const GameInfo& game,
        const std::vector<TranslationEntry>& translations
    ) const override {
        // Check game directory is writable
        auto stringsDir = game.installPath / "data" / "strings";
        if (auto err = validateFileWritable(stringsDir)) {
            return *err;
        }

        // Check we have translations
        if (translations.empty()) {
            return Error(ErrorCode::InvalidArgument, "No translations provided");
        }

        return true;
    }

private:
    /**
     * @brief Extract strings from a single .stxt file
     */
    Result<std::vector<StringEntry>> extractFromFile(
        const fs::path& filePath,
        const fs::path& basePath
    ) const {
        std::vector<StringEntry> strings;

        std::ifstream file(filePath);
        if (!file.is_open()) {
            return Error(ErrorCode::FileIOError,
                "Cannot open file: " + filePath.string());
        }

        std::string relativePath = fs::relative(filePath, basePath).string();
        std::string currentSection;
        std::string line;
        int lineNumber = 0;

        // Regex for key=value pairs
        std::regex kvRegex(R"(^([^=]+)=(.+)$)");
        std::regex sectionRegex(R"(^\[([^\]]+)\]$)");

        while (std::getline(file, line)) {
            lineNumber++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';') {
                continue;
            }

            // Check for section header
            std::smatch sectionMatch;
            if (std::regex_match(line, sectionMatch, sectionRegex)) {
                currentSection = sectionMatch[1].str();
                continue;
            }

            // Check for key=value
            std::smatch kvMatch;
            if (std::regex_match(line, kvMatch, kvRegex)) {
                std::string key = kvMatch[1].str();
                std::string value = kvMatch[2].str();

                // Skip if value looks like code/path
                if (value.find('/') != std::string::npos ||
                    value.find('\\') != std::string::npos ||
                    value.find("::") != std::string::npos) {
                    continue;
                }

                // Create string entry
                StringEntry entry;
                entry.sourceText = value;
                entry.context = currentSection + "." + key;
                entry.filePath = relativePath;
                entry.lineNumber = lineNumber;
                entry.category = EntryCategory::Dialogue;

                // Detect category from section name
                if (currentSection.find("menu") != std::string::npos ||
                    currentSection.find("ui") != std::string::npos) {
                    entry.category = EntryCategory::UI;
                } else if (currentSection.find("item") != std::string::npos) {
                    entry.category = EntryCategory::Item;
                }

                strings.push_back(entry);
            }
        }

        return strings;
    }

    /**
     * @brief Patch a single .stxt file with translations
     */
    Result<int> patchFile(
        const fs::path& filePath,
        const std::unordered_map<std::string, std::string>& translations
    ) const {
        // Read original file
        std::ifstream inFile(filePath);
        if (!inFile.is_open()) {
            return Error(ErrorCode::FileIOError,
                "Cannot open file: " + filePath.string());
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(inFile, line)) {
            lines.push_back(line);
        }
        inFile.close();

        // Patch lines
        int patchCount = 0;
        std::regex kvRegex(R"(^([^=]+)=(.+)$)");

        for (auto& l : lines) {
            std::smatch match;
            if (std::regex_match(l, match, kvRegex)) {
                std::string key = match[1].str();
                std::string value = match[2].str();

                auto it = translations.find(value);
                if (it != translations.end()) {
                    l = key + "=" + it->second;
                    patchCount++;
                }
            }
        }

        if (patchCount == 0) {
            return 0;  // Nothing to patch
        }

        // Write patched file atomically
        std::ostringstream buffer;
        for (const auto& l : lines) {
            buffer << l << "\n";
        }
        std::string patchedContent = buffer.str();

        auto writeResult = writeFileAtomic(filePath, std::string_view(patchedContent));

        if (!writeResult) {
            return writeResult.error();
        }

        return patchCount;
    }
};

/**
 * @brief Factory function to create handler
 */
std::unique_ptr<IEngineHandler> createSimpleTextHandler() {
    return std::make_unique<SimpleTextHandler>();
}

/**
 * @brief Test the handler
 */
int main() {
    std::cout << "Custom Handler Example\n";
    std::cout << "======================\n\n";

    // Create handler
    SimpleTextHandler handler;

    std::cout << "Handler name: " << handler.name() << "\n";
    std::cout << "Engine name:  " << handler.engineName() << "\n";
    std::cout << "Engine type:  " << static_cast<int>(handler.engineType()) << "\n\n";

    // Create test game info
    GameInfo testGame;
    testGame.name = "Test SimpleText Game";
    testGame.id = GameId{"simpletext", "test-game"};
    testGame.engine = GameEngine::Custom;
    testGame.installPath = fs::current_path() / "test_game";

    // Check if handler can handle
    std::cout << "Can handle test game: " << (handler.canHandle(testGame) ? "Yes" : "No") << "\n";
    std::cout << "Confidence: " << handler.confidence(testGame) << "%\n\n";

    std::cout << "To use this handler:\n";
    std::cout << "1. Register with HandlerRegistry\n";
    std::cout << "2. Or use directly via handler->extractStrings(game)\n";

    return 0;
}
