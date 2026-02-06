/**
 * @file engine_handler.hpp
 * @brief Base interface for game engine handlers
 * @copyright (c) 2026 MakineAI Team
 *
 * Each game engine (Unity, Unreal, Ren'Py, etc.) has unique file formats
 * and string storage methods. Engine handlers provide a uniform interface
 * for string extraction and patch application.
 */

#pragma once

#include "../types.hpp"
#include "../error.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace makineai {

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Game file type classification
 */
enum class GameFileType {
    Localization,   // Dedicated localization files (*.json, *.po)
    Resource,       // Resource/asset files containing strings
    Binary,         // Binary files with embedded strings
    Script,         // Script files (*.rpy, *.rb)
    Data,           // Generic data files
    Config          // Configuration files
};

// EntryCategory is defined in types.hpp

/**
 * @brief Extraction error severity
 */
enum class ExtractionSeverity {
    Info,       // Informational
    Warning,    // Non-critical issue
    Error,      // Extraction failed for file
    Fatal       // Extraction cannot continue
};

/**
 * @brief Discovered game file
 */
struct GameFile {
    fs::path path;              // Absolute path
    std::string relativePath;   // Path relative to game directory
    GameFileType type = GameFileType::Data;
    uint64_t size = 0;
    std::string encoding = "utf-8";
    int stringCount = 0;        // Known string count (after extraction)
};

// TranslationEntry is defined in types.hpp
// Additional IL2CPP metadata fields (offset, length) are defined there

/**
 * @brief Extraction error
 */
struct ExtractionError {
    std::string file;
    std::string message;
    ExtractionSeverity severity = ExtractionSeverity::Error;
    int lineNumber = 0;
};

/**
 * @brief Extraction options
 */
struct ExtractionOptions {
    int minLength = 1;          // Minimum string length
    int maxLength = 10000;      // Maximum string length
    std::vector<std::string> includeFiles;  // File filters (include)
    std::vector<std::string> excludeFiles;  // File filters (exclude)
    bool extractIl2CppStrings = true;       // Extract from IL2CPP metadata
    bool skipCodeStrings = true;            // Skip code-like strings
};

/**
 * @brief String extraction result
 */
struct ExtractionResult {
    std::vector<TranslationEntry> entries;
    std::vector<ExtractionError> errors;
    std::vector<GameFile> processedFiles;
    int totalStrings = 0;
    int skippedStrings = 0;
    std::chrono::milliseconds duration{0};

    [[nodiscard]] bool hasErrors() const {
        for (const auto& e : errors) {
            if (e.severity >= ExtractionSeverity::Error) return true;
        }
        return false;
    }

    [[nodiscard]] bool hasFatalErrors() const {
        for (const auto& e : errors) {
            if (e.severity == ExtractionSeverity::Fatal) return true;
        }
        return false;
    }
};

/**
 * @brief Patch options
 */
struct PatchOptions {
    bool createBackup = true;   // Create backup before patching
    std::string backupId;       // Custom backup ID
    bool dryRun = false;        // Simulate without writing
    bool preserveEncoding = true; // Maintain original file encoding
};

/**
 * @brief Patched file info
 */
struct PatchedFile {
    std::string path;
    int changedStrings = 0;
    int totalStrings = 0;
    bool backed = false;
};

/**
 * @brief Patch error
 */
struct PatchError {
    std::string file;
    std::string message;
    ExtractionSeverity severity = ExtractionSeverity::Error;
};

/**
 * @brief Patch application result
 */
struct HandlerPatchResult {
    bool success = true;
    std::vector<PatchedFile> patchedFiles;
    std::vector<PatchError> errors;
    std::string backupId;
    int appliedCount = 0;
    int skippedCount = 0;
    std::chrono::milliseconds duration{0};
};

/**
 * @brief Backup creation result
 */
struct HandlerBackupResult {
    bool success = true;
    std::string backupId;
    std::string backupPath;
    std::vector<std::string> backedUpFiles;
    uint64_t totalSize = 0;
    std::string errorMessage;
};

/**
 * @brief Backup restore result
 */
struct HandlerRestoreResult {
    bool success = true;
    std::vector<std::string> restoredFiles;
    std::string errorMessage;
};

/**
 * @brief Validation issue severity
 */
enum class ValidationSeverity {
    Info,
    Warning,
    Error
};

/**
 * @brief Validation issue
 */
struct ValidationIssue {
    std::string file;
    std::string entryKey;
    std::string message;
    ValidationSeverity severity = ValidationSeverity::Warning;
};

/**
 * @brief Patch validation result
 */
struct ValidationResult {
    bool isValid = true;
    std::vector<ValidationIssue> issues;
    int checkedCount = 0;
    int passedCount = 0;
    int failedCount = 0;
};

// ============================================================================
// ENGINE HANDLER INTERFACE
// ============================================================================

/**
 * @brief Base interface for game engine handlers
 *
 * Each handler implements engine-specific logic for:
 * - Detecting if a game uses this engine
 * - Finding translatable files
 * - Extracting strings
 * - Applying translations
 * - Backup/restore operations
 */
class IEngineHandler {
public:
    virtual ~IEngineHandler() = default;

    /**
     * @brief Get engine name
     */
    [[nodiscard]] virtual std::string engineName() const = 0;

    /**
     * @brief Get supported file extensions
     */
    [[nodiscard]] virtual std::vector<std::string> supportedExtensions() const = 0;

    /**
     * @brief Check if this handler can process the game
     *
     * @param gameDir Game installation directory
     * @return true if this handler supports the game
     */
    [[nodiscard]] virtual bool canHandleGame(const fs::path& gameDir) = 0;

    /**
     * @brief Find all translatable files in game directory
     *
     * @param gameDir Game installation directory
     * @return List of game files
     */
    [[nodiscard]] virtual Result<std::vector<GameFile>> findGameFiles(
        const fs::path& gameDir
    ) = 0;

    /**
     * @brief Extract all translatable strings from game
     *
     * @param gameDir Game installation directory
     * @param options Extraction options
     * @return Extraction result with entries and errors
     */
    [[nodiscard]] virtual Result<ExtractionResult> extractStrings(
        const fs::path& gameDir,
        const ExtractionOptions& options = {}
    ) = 0;

    /**
     * @brief Apply translations to game files
     *
     * @param gameDir Game installation directory
     * @param translations Translations to apply
     * @param options Patch options
     * @return Patch result
     */
    [[nodiscard]] virtual Result<HandlerPatchResult> applyTranslations(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations,
        const PatchOptions& options = {}
    ) = 0;

    /**
     * @brief Create backup of game files
     *
     * @param gameDir Game installation directory
     * @param backupId Backup identifier
     * @param specificFiles Optional list of specific files to backup
     * @return Backup result
     */
    [[nodiscard]] virtual Result<HandlerBackupResult> createBackup(
        const fs::path& gameDir,
        const std::string& backupId,
        const std::vector<std::string>& specificFiles = {}
    ) = 0;

    /**
     * @brief Restore game files from backup
     *
     * @param gameDir Game installation directory
     * @param backupId Backup identifier
     * @return Restore result
     */
    [[nodiscard]] virtual Result<HandlerRestoreResult> restoreBackup(
        const fs::path& gameDir,
        const std::string& backupId
    ) = 0;

    /**
     * @brief Validate translations before applying
     *
     * @param gameDir Game installation directory
     * @param translations Translations to validate
     * @return Validation result
     */
    [[nodiscard]] virtual Result<ValidationResult> validatePatch(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations
    ) = 0;
};

// ============================================================================
// HANDLER UTILITIES
// ============================================================================

/**
 * @brief Common utilities for engine handlers
 */
class HandlerUtils {
public:
    /**
     * @brief Check if a file is a localization file by name
     */
    static bool isLocalizationFileName(const std::string& name);

    /**
     * @brief Guess context from file/key path
     */
    static std::string guessContext(const std::string& path);

    /**
     * @brief Guess category from file/key path
     */
    static EntryCategory guessCategory(const std::string& path);

    /**
     * @brief Check if string is valid for translation
     */
    static bool isValidString(
        const std::string& text,
        const ExtractionOptions& options = {}
    );

    /**
     * @brief Detect file encoding from BOM
     */
    static std::string detectEncoding(const fs::path& file);

    /**
     * @brief Decode XML entities
     */
    static std::string decodeXmlEntities(const std::string& text);

    /**
     * @brief Encode text for XML
     */
    static std::string encodeXmlEntities(const std::string& text);

    /**
     * @brief Parse CSV line respecting quotes
     */
    static std::vector<std::string> parseCsvLine(const std::string& line);

    /**
     * @brief Escape CSV field
     */
    static std::string escapeCsvField(const std::string& field);
};

// ============================================================================
// HANDLER FACTORY
// ============================================================================

/**
 * @brief Factory for creating engine handlers
 */
class EngineHandlerFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static EngineHandlerFactory& instance();

    /**
     * @brief Register a handler for an engine
     */
    void registerHandler(std::unique_ptr<IEngineHandler> handler);

    /**
     * @brief Get handler for a game
     *
     * @param gameDir Game directory
     * @return Appropriate handler or nullptr
     */
    [[nodiscard]] IEngineHandler* getHandlerForGame(const fs::path& gameDir);

    /**
     * @brief Get handler by engine name
     */
    [[nodiscard]] IEngineHandler* getHandler(const std::string& engineName);

    /**
     * @brief List all registered handlers
     */
    [[nodiscard]] std::vector<std::string> listHandlers() const;

private:
    EngineHandlerFactory();
    std::vector<std::unique_ptr<IEngineHandler>> handlers_;
};

} // namespace makineai
