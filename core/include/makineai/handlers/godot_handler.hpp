/**
 * @file godot_handler.hpp
 * @brief Handler for Godot Engine games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - Godot 3.x and 4.x
 * - .pck package files (read/extract)
 * - CSV translation files (key,en,tr,...)
 * - .po/.mo gettext files
 * - .tres/.tscn resource/scene files
 * - project.godot detection
 */

#pragma once

#include "engine_handler.hpp"

#include <map>

namespace makineai {

// ============================================================================
// PCK PARSER
// ============================================================================

/**
 * @brief Godot PCK magic number
 * "GDPC" in file header
 */
constexpr uint32_t GODOT_PCK_MAGIC = 0x43504447; // "GDPC" little-endian

/**
 * @brief Entry in a Godot PCK archive
 */
struct GodotPckEntry {
    std::string path;       // e.g. "res://translations/tr.csv"
    uint64_t offset = 0;
    uint64_t size = 0;
    std::array<uint8_t, 16> md5{};
};

/**
 * @brief PCK file header info
 */
struct GodotPckHeader {
    uint32_t formatVersion = 0;
    uint32_t majorVersion = 0;
    uint32_t minorVersion = 0;
    uint32_t patchVersion = 0;
    uint32_t fileCount = 0;
};

/**
 * @brief PCK parse result
 */
struct GodotPckResult {
    bool success = false;
    std::string error;
    GodotPckHeader header;
    std::vector<GodotPckEntry> entries;
};

/**
 * @brief Godot PCK file parser
 *
 * Parses .pck files to list and extract contained resources.
 * Supports both Godot 3.x and 4.x PCK formats.
 */
class GodotPckParser {
public:
    /**
     * @brief Parse PCK file and list entries
     */
    static GodotPckResult parse(const fs::path& pckFile);

    /**
     * @brief Extract a single file from PCK
     * @return File content or empty on error
     */
    static std::string extractFile(
        const fs::path& pckFile,
        const GodotPckEntry& entry
    );

    /**
     * @brief Check if file is a valid PCK
     */
    static bool isPckFile(const fs::path& file);
};

// ============================================================================
// GODOT HANDLER
// ============================================================================

/**
 * @brief Godot Engine Handler
 *
 * Handles string extraction and patching for Godot games.
 * Detection: project.godot file, .pck files, or Godot executable signature.
 */
class GodotHandler : public IEngineHandler {
public:
    GodotHandler() = default;
    ~GodotHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "Godot";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"pck", "csv", "po", "tres", "tscn", "translation"};
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
    // Detection helpers
    std::optional<fs::path> findProjectFile(const fs::path& gameDir);
    std::vector<fs::path> findPckFiles(const fs::path& gameDir);
    bool hasGodotSignature(const fs::path& executable);

    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    ExtractionBatch extractFromCsv(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromPo(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromTresOrTscn(
        const fs::path& file,
        const std::string& content,
        const ExtractionOptions& options
    );

    // Translation file discovery
    std::vector<fs::path> findTranslationFiles(const fs::path& gameDir);
    std::vector<fs::path> findTranslationFilesInPck(const fs::path& pckFile);

    // Patching helpers
    std::string applyCsvTranslations(
        const std::string& content,
        const std::map<std::string, std::string>& translations
    );

    std::string applyPoTranslations(
        const std::string& content,
        const std::map<std::string, std::string>& translations
    );

    // Validation
    static bool isValidString(const std::string& text, const ExtractionOptions& options);
};

} // namespace makineai
