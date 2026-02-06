/**
 * @file rpgmaker_handler.hpp
 * @brief Handler for RPG Maker games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - RPG Maker MV/MZ (.json format)
 * - RPG Maker VX Ace (.rvdata2 Ruby Marshal format)
 * - Event command extraction (Show Text, Choices, Scroll Text)
 * - System terms, actors, items, skills, etc.
 */

#pragma once

#include "engine_handler.hpp"

#include <nlohmann/json.hpp>
#include <optional>

namespace makineai {

/**
 * @brief RPG Maker version
 */
enum class RpgMakerVersion {
    XP,      // XP (.rxdata) - Limited support
    VX,      // VX (.rvdata) - Limited support
    VXAce,   // VX Ace (.rvdata2)
    MVMZ     // MV & MZ (.json)
};

/**
 * @brief Event command codes used by RPG Maker
 */
namespace EventCode {
    constexpr int ShowTextHeader = 101;      // Start of text message
    constexpr int ShowTextBody = 401;        // Text message line
    constexpr int ShowChoices = 102;         // Choice selection
    constexpr int ScrollTextHeader = 105;    // Start of scroll text
    constexpr int ScrollTextBody = 405;      // Scroll text line
    constexpr int ChangeName = 320;          // Change actor name
    constexpr int ChangeNickname = 324;      // Change actor nickname
    constexpr int ChangeProfile = 325;       // Change actor profile
}

/**
 * @brief RPG Maker Handler
 *
 * Handles string extraction and patching for RPG Maker games.
 * Supports MV/MZ (JSON format) and VX Ace (Ruby Marshal format).
 */
class RpgMakerHandler : public IEngineHandler {
public:
    RpgMakerHandler() = default;
    ~RpgMakerHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "RPG Maker";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"json", "rvdata2", "rvdata", "rxdata"};
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
    RpgMakerVersion detectedVersion_ = RpgMakerVersion::MVMZ;

    // Directory helpers
    std::optional<fs::path> findDataDir(const fs::path& gameDir);
    GameFileType getFileType(const std::string& filename);

    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    // JSON extraction (MV/MZ)
    ExtractionBatch extractFromJsonFile(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    void extractFromJson(
        const nlohmann::json& json,
        const std::string& filePath,
        const std::string& fileName,
        const ExtractionOptions& options,
        ExtractionBatch& batch
    );

    void extractFromEventList(
        const nlohmann::json& events,
        const std::string& filePath,
        const std::string& basePath,
        const ExtractionOptions& options,
        ExtractionBatch& batch
    );

    void extractTerms(
        const nlohmann::json& terms,
        const std::string& filePath,
        const std::string& basePath,
        const ExtractionOptions& options,
        ExtractionBatch& batch
    );

    // Patching helpers
    struct PatchBatch {
        int applied = 0;
        int skipped = 0;
    };

    PatchBatch applyToJsonFile(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    bool applyTranslationToJson(
        nlohmann::json& json,
        const TranslationEntry& translation
    );

    bool applyEventTranslation(
        nlohmann::json& json,
        const TranslationEntry& translation
    );

    bool applyFieldTranslation(
        nlohmann::json& json,
        const TranslationEntry& translation
    );

    bool applyToEventList(
        nlohmann::json& events,
        int targetEventIndex,
        const std::string& eventType,
        std::optional<int> subIndex,
        const std::string& sourceText,
        const std::string& targetText
    );

    bool applyTextTranslation(
        nlohmann::json& events,
        const std::vector<size_t>& textLineIndices,
        const std::string& sourceText,
        const std::string& targetText
    );

    // Event list helpers
    static bool isEventList(const nlohmann::json& list);
    nlohmann::json* findEventListAtPath(nlohmann::json& json, const std::string& path);
    nlohmann::json* findEventList(nlohmann::json& json);

    // Validation helpers
    static bool isValidString(const std::string& text, const ExtractionOptions& options);
    static bool containsDisplayText(const std::string& text);

    // Category guessing
    static std::string guessContext(const std::string& path, const std::string& fileName);
    static EntryCategory guessCategory(const std::string& path, const std::string& fileName);
    static std::string getFieldContext(const std::string& field);
    static EntryCategory getFieldCategory(const std::string& field);
};

} // namespace makineai
