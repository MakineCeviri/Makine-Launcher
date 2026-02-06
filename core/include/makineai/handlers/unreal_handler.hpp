/**
 * @file unreal_handler.hpp
 * @brief Handler for Unreal Engine games
 * @copyright (c) 2026 MakineAI Team
 *
 * Supports:
 * - UE4 and UE5 games
 * - .int localization files (INI-like format)
 * - .locres binary localization format
 * - .ini and .txt configuration files
 */

#pragma once

#include "engine_handler.hpp"

#include <cstdint>

namespace makineai {

/**
 * @brief LocRes magic number (UE4/UE5)
 */
constexpr uint32_t LOCRES_MAGIC = 0x0E14DA7A;

/**
 * @brief Unreal Engine version
 */
enum class UnrealVersion {
    UE4,    // Unreal Engine 4.x
    UE5     // Unreal Engine 5.x
};

/**
 * @brief LocRes entry (namespace::key -> value)
 */
struct LocResEntry {
    std::string ns;         // Namespace
    std::string key;        // Key within namespace
    uint32_t sourceHash = 0; // Source string hash
    std::string value;      // Localized string

    [[nodiscard]] std::string fullKey() const {
        return ns + "::" + key;
    }
};

/**
 * @brief Parsed LocRes file
 */
struct LocResFile {
    int version = 0;
    std::vector<LocResEntry> entries;
    std::vector<std::string> localizedStrings;
    bool valid = false;
    std::string errorMessage;

    /**
     * @brief Serialize back to binary
     */
    [[nodiscard]] std::vector<uint8_t> toBytes() const;
};

/**
 * @brief LocRes binary format parser
 *
 * Unreal Engine's localization resource format.
 */
class LocResParser {
public:
    explicit LocResParser(const std::vector<uint8_t>& data);

    /**
     * @brief Parse the LocRes file
     */
    [[nodiscard]] LocResFile parse();

private:
    const std::vector<uint8_t>& data_;
    size_t offset_ = 0;

    uint8_t readUint8();
    int32_t readInt32();
    uint32_t readUint32();
    int64_t readInt64();

    /**
     * @brief Read FString (Unreal's string format)
     *
     * Format: length (int32) + chars + null terminator
     * Negative length = UTF-16, positive = UTF-8/ASCII
     */
    std::string readFString();
};

/**
 * @brief Unreal Engine Handler
 *
 * Handles string extraction and patching for UE4/UE5 games.
 */
class UnrealHandler : public IEngineHandler {
public:
    UnrealHandler() = default;
    ~UnrealHandler() override = default;

    // ========== IEngineHandler Interface ==========

    [[nodiscard]] std::string engineName() const override {
        return "Unreal Engine";
    }

    [[nodiscard]] std::vector<std::string> supportedExtensions() const override {
        return {"int", "locres", "ini", "txt"};
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
    // File scanning
    void scanLocalizationDir(
        const fs::path& dir,
        const fs::path& gameDir,
        std::vector<GameFile>& files
    );

    std::string detectLocFileEncoding(const fs::path& file);

    // Extraction helpers
    struct ExtractionBatch {
        std::vector<TranslationEntry> entries;
        int total = 0;
        int skipped = 0;
    };

    ExtractionBatch extractFromLocResFile(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromIntFile(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    ExtractionBatch extractFromIniFile(
        const fs::path& file,
        const GameFile& gameFile,
        const ExtractionOptions& options
    );

    // Patching helpers
    struct PatchBatch {
        int applied = 0;
        int skipped = 0;
    };

    PatchBatch applyToLocResFile(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToIntFile(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    PatchBatch applyToIniFile(
        const fs::path& file,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options
    );

    // UTF-16 LE helpers
    static std::string decodeUtf16Le(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> encodeUtf16Le(const std::string& text);

    // String escape helpers
    static std::string unescapeString(const std::string& text);
    static std::string escapeString(const std::string& text);

    // Category guessing
    static EntryCategory guessCategoryFromLocRes(const std::string& ns, const std::string& key);
    static EntryCategory guessCategory(const std::string& section, const std::string& key);

    // Validation
    static bool isValidString(const std::string& text, const ExtractionOptions& options);
};

} // namespace makineai
