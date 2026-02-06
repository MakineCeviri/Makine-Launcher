/**
 * @file gamemaker_handler.hpp
 * @brief Handler for GameMaker games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - GameMaker: Studio 1.x
 * - GameMaker Studio 2
 * - data.win binary format
 * - External language files (CSV, JSON, TXT/INI)
 */

#pragma once

#include "engine_handler.hpp"

#include <map>
#include <optional>

namespace makineai {

/**
 * @brief GameMaker data file magic number
 * "FORM" in little-endian
 */
constexpr uint32_t GAMEMAKER_FORM_MAGIC = 0x4D524F46; // "FORM"

/**
 * @brief GameMaker string entry from data.win
 */
struct GameMakerString {
    size_t index = 0;
    size_t offset = 0;
    std::string content;
};

/**
 * @brief GameMaker data.win parse result
 */
struct GameMakerParseResult {
    bool success = false;
    std::string error;
    std::vector<GameMakerString> strings;
    int version = 0;
};

/**
 * @brief GameMaker data.win parser
 *
 * Parses the FORM/STRG chunk to extract strings.
 */
class GameMakerDataParser {
public:
    /**
     * @brief Parse data.win file and extract strings
     */
    static GameMakerParseResult parse(const fs::path& file);

    /**
     * @brief Apply translations to data.win file
     * @return Number of strings replaced
     */
    static int applyTranslations(
        const fs::path& file,
        const std::map<std::string, std::string>& translations,
        bool createBackup = true
    );

private:
    static std::string readString(const std::vector<uint8_t>& data, size_t offset);
    static size_t findChunk(const std::vector<uint8_t>& data, const char* chunkName);
};

/**
 * @brief GameMaker Handler
 *
 * Handles string extraction and patching for GameMaker games.
 */
class GameMakerHandler : public IEngineHandler {
public:
    GameMakerHandler() = default;
    ~GameMakerHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "GameMaker";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"win", "unx", "ios", "droid", "ini"};
    }

    [[nodiscard]] bool canHandleGame(const fs::path& gameDir) override;

    [[nodiscard]] Result<std::vector<GameFile>> findGameFiles(
        const fs::path& gameDir
    ) override;

    [[nodiscard]] Result<ExtractionResult> extractStrings(
        const fs::path& gameDir,
        const ExtractionOptions& options = {}
    ) override;

    [[nodiscard]] Result<HandlerPatchResult> applyTranslations(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options = {}
    ) override;

    [[nodiscard]] Result<HandlerBackupResult> createBackup(
        const fs::path& gameDir,
        const std::string& backupId,
        const std::vector<std::string>& specificFiles = {}
    ) override;

    [[nodiscard]] Result<HandlerRestoreResult> restoreBackup(
        const fs::path& gameDir,
        const std::string& backupId
    ) override;

    [[nodiscard]] Result<ValidationResult> validatePatch(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations
    ) override;

private:
    std::optional<fs::path> dataWinPath_;

    // File discovery
    std::optional<fs::path> findDataFile(const fs::path& gameDir);
    std::vector<fs::path> findLanguageFiles(const fs::path& gameDir);

    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    ExtractionBatch extractFromDataWin(
        const fs::path& file,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromLangFile(
        const fs::path& file,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromCsv(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromJson(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromIni(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    // Patching helpers
    int patchLangFile(
        const fs::path& file,
        const std::map<std::string, std::string>& translations,
        bool dryRun
    );

    // Category guessing
    static EntryCategory categorizeText(const std::string& text);
    static bool isValidString(const std::string& text, const ExtractionOptions& options);
};

} // namespace makineai
