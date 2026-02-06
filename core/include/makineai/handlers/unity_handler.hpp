/**
 * @file unity_handler.hpp
 * @brief Handler for Unity engine games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - Unity Mono builds (Managed/*.dll)
 * - Unity IL2CPP builds (GameAssembly.dll + global-metadata.dat)
 * - Multiple localization formats (JSON, XML, CSV, TXT)
 * - StreamingAssets and Resources folders
 */

#pragma once

#include "engine_handler.hpp"

#include <nlohmann/json.hpp>

namespace makineai {

/**
 * @brief Unity build type
 */
enum class UnityBuildType {
    Unknown,
    Mono,       // Standard Mono/.NET build
    IL2CPP      // IL2CPP AOT compiled build
};

/**
 * @brief IL2CPP string info
 */
struct IL2CppString {
    int64_t index;          // String index in metadata
    int64_t offset;         // Byte offset in file
    int64_t length;         // String length
    std::string text;       // String content
    std::string context;    // Guessed context
};

/**
 * @brief IL2CPP metadata info
 */
struct IL2CppMetadata {
    int version = 0;
    int stringCount = 0;
    std::vector<IL2CppString> strings;
    bool success = false;
    std::string errorMessage;
};

/**
 * @brief Unity Engine Handler
 *
 * Handles string extraction and patching for Unity games.
 */
class UnityHandler : public IEngineHandler {
public:
    UnityHandler();
    ~UnityHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "Unity";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"json", "xml", "csv", "txt", "asset", "assets", "dat"};
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

    // ========== Unity-Specific Methods ==========

    /**
     * @brief Get detected build type
     */
    [[nodiscard]] UnityBuildType buildType() const { return buildType_; }

    /**
     * @brief Get data folder path
     */
    [[nodiscard]] std::optional<fs::path> dataFolder() const { return dataFolder_; }

    /**
     * @brief Get IL2CPP metadata path (if IL2CPP build)
     */
    [[nodiscard]] std::optional<fs::path> metadataPath() const { return metadataPath_; }

    /**
     * @brief Parse IL2CPP metadata file
     */
    [[nodiscard]] static IL2CppMetadata parseIL2CppMetadata(const fs::path& path);

    /**
     * @brief Get translatable strings from IL2CPP metadata
     */
    [[nodiscard]] static std::vector<IL2CppString> getTranslatableStrings(
        const IL2CppMetadata& metadata,
        int minLength = 2,
        int maxLength = 10000,
        bool excludeCodeStrings = true
    );

private:
    // Detection
    void detectBuildType(const fs::path& gameDir);
    fs::path findDataFolder(const fs::path& gameDir);

    // File scanning
    void scanForLocalizationFiles(
        const fs::path& dir,
        const fs::path& gameDir,
        std::vector<GameFile>& files,
        bool recursive = false
    );

    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    ExtractionBatch extractFromJson(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromXml(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromCsv(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromTxt(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromIL2Cpp(
        const fs::path& metadataPath,
        const ExtractionOptions& options
    );

    // Patching helpers
    struct PatchBatch {
        int applied = 0;
        int skipped = 0;
        std::vector<std::string> errors;
    };

    PatchBatch applyToJson(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToXml(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToCsv(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToTxt(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToIL2Cpp(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    // JSON recursive helpers
    void extractJsonValue(
        const nlohmann::json& value,
        const std::string& path,
        const GameFile& gameFile,
        const ExtractionOptions& options,
        ExtractionBatch& batch
    );

    nlohmann::json applyJsonTranslations(
        const nlohmann::json& value,
        const std::string& path,
        const std::unordered_map<std::string, std::string>& translations,
        int& applied
    );

    // IL2CPP category guessing
    static EntryCategory guessCategoryFromIL2Cpp(const std::string& context);

    // State
    std::optional<fs::path> dataFolder_;
    std::optional<fs::path> metadataPath_;
    UnityBuildType buildType_ = UnityBuildType::Unknown;
};

} // namespace makineai
