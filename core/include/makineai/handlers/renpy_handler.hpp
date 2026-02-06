/**
 * @file renpy_handler.hpp
 * @brief Handler for Ren'Py visual novel games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - .rpy script files
 * - Dialogue extraction
 * - Character definitions
 * - Menu choices
 */

#pragma once

#include "engine_handler.hpp"

namespace makineai {

/**
 * @brief Ren'Py Engine Handler
 *
 * Handles string extraction and patching for Ren'Py visual novels.
 */
class RenpyHandler : public IEngineHandler {
public:
    RenpyHandler() = default;
    ~RenpyHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "Ren'Py";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"rpy"};
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
    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    ExtractionBatch extractFromRpyFile(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    // String helpers
    static std::string extractQuotedString(const std::string& quoted);
    static bool isValidString(const std::string& text, const ExtractionOptions& options);

    // Patching helpers
    static std::optional<std::string> replaceStringInLine(
        const std::string& line,
        const std::string& source,
        const std::string& target
    );

    static std::string escapeString(const std::string& text, char quote);
};

} // namespace makineai
