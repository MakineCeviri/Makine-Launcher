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

#include "makineai/types.hpp"
#include "makineai/error.hpp"
#include "makineai/logging.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
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
    bool useStringClassifier = true;        // Apply StringClassifier to filter results
    float minClassifierConfidence = 0.5f;   // Minimum confidence for translatable
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
// HANDLER BASE CLASS
// ============================================================================

/**
 * @brief Progress reporter for handler operations
 *
 * Provides a convenient way to report progress during long operations.
 */
class HandlerProgressReporter {
public:
    using Callback = std::function<void(uint32_t current, uint32_t total, std::string_view message)>;

    explicit HandlerProgressReporter(uint32_t totalSteps, Callback callback = nullptr)
        : total_(totalSteps), callback_(std::move(callback)) {}

    void advance(std::string_view message = "") {
        ++current_;
        if (callback_) {
            callback_(current_, total_, message);
        }
    }

    void setStep(uint32_t step, std::string_view message = "") {
        current_ = step;
        if (callback_) {
            callback_(current_, total_, message);
        }
    }

    [[nodiscard]] uint32_t current() const noexcept { return current_; }
    [[nodiscard]] uint32_t total() const noexcept { return total_; }
    [[nodiscard]] double percent() const noexcept {
        return total_ > 0 ? (static_cast<double>(current_) / total_) * 100.0 : 0.0;
    }

private:
    uint32_t current_ = 0;
    uint32_t total_;
    Callback callback_;
};

/**
 * @brief Abstract base class for engine handlers
 *
 * Provides default implementations and common utilities for all handlers.
 * Concrete handlers should inherit from this instead of IEngineHandler directly.
 *
 * Features:
 * - Default backup/restore implementation using file copy
 * - Logging macros (via handlerLog methods)
 * - File validation utilities
 * - Progress reporting support
 *
 * @code
 * class MyHandler : public EngineHandlerBase {
 * public:
 *     MyHandler() : EngineHandlerBase("MyEngine") {}
 *     // Implement pure virtual methods...
 * };
 * @endcode
 */
class EngineHandlerBase : public IEngineHandler {
public:
    /**
     * @brief Constructor
     * @param name Handler/engine name for logging
     */
    explicit EngineHandlerBase(std::string name) : handlerName_(std::move(name)) {}

    // --------------------------------------------------------------------------
    // IEngineHandler interface with default implementations
    // --------------------------------------------------------------------------

    [[nodiscard]] std::string engineName() const override { return handlerName_; }

    /**
     * @brief Default backup implementation using file copy
     *
     * Creates a backup of specified files (or all translatable files) in
     * the backup directory.
     */
    [[nodiscard]] Result<HandlerBackupResult> createBackup(
        const fs::path& gameDir,
        const std::string& backupId,
        const std::vector<std::string>& specificFiles = {}
    ) override;

    /**
     * @brief Default restore implementation
     *
     * Restores files from a previously created backup.
     */
    [[nodiscard]] Result<HandlerRestoreResult> restoreBackup(
        const fs::path& gameDir,
        const std::string& backupId
    ) override;

    /**
     * @brief Default validation implementation
     *
     * Validates translations by checking:
     * - Entry key exists in game files
     * - Placeholder count matches
     * - String length within limits
     */
    [[nodiscard]] Result<ValidationResult> validatePatch(
        const fs::path& gameDir,
        const std::vector<TranslationEntry>& translations
    ) override;

protected:
    // --------------------------------------------------------------------------
    // Logging utilities
    // --------------------------------------------------------------------------

    /// @brief Log debug message
    void logDebug(std::string_view message) const;

    /// @brief Log info message
    void logInfo(std::string_view message) const;

    /// @brief Log warning message
    void logWarning(std::string_view message) const;

    /// @brief Log error message
    void logError(std::string_view message) const;

    /// @brief Log with format string (debug level)
    template<typename... Args>
    void logDebugFmt(std::string_view fmt, Args&&... args) const {
        logDebug(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    /// @brief Log with format string (info level)
    template<typename... Args>
    void logInfoFmt(std::string_view fmt, Args&&... args) const {
        logInfo(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    // --------------------------------------------------------------------------
    // File validation utilities
    // --------------------------------------------------------------------------

    /**
     * @brief Validate file exists and is readable
     * @param path File path to validate
     * @return Error if validation fails
     */
    [[nodiscard]] std::optional<Error> validateFileReadable(const fs::path& path) const;

    /**
     * @brief Validate file is writable
     * @param path File path to validate
     * @return Error if validation fails
     */
    [[nodiscard]] std::optional<Error> validateFileWritable(const fs::path& path) const;

    /**
     * @brief Validate path is within game directory (security check)
     * @param path Path to validate
     * @param gameDir Game directory
     * @return Error if path escapes game directory
     */
    [[nodiscard]] std::optional<Error> validatePathSafe(
        const fs::path& path,
        const fs::path& gameDir
    ) const;

    /**
     * @brief Check if file is locked by another process
     * @param path File path to check
     * @return true if file appears to be locked
     */
    [[nodiscard]] bool isFileLocked(const fs::path& path) const;

    // --------------------------------------------------------------------------
    // Atomic file operations
    // --------------------------------------------------------------------------

    /**
     * @brief Write file atomically (write to temp, then rename)
     * @param path Destination path
     * @param content Content to write
     * @return Error if write fails
     */
    [[nodiscard]] std::optional<Error> writeFileAtomic(
        const fs::path& path,
        const ByteBuffer& content
    ) const;

    /**
     * @brief Write file atomically (string version)
     */
    [[nodiscard]] std::optional<Error> writeFileAtomic(
        const fs::path& path,
        std::string_view content
    ) const;

    // --------------------------------------------------------------------------
    // Progress reporting
    // --------------------------------------------------------------------------

    /**
     * @brief Create progress reporter for an operation
     * @param totalSteps Total number of steps
     * @param callback Optional progress callback
     * @return Progress reporter instance
     */
    [[nodiscard]] HandlerProgressReporter createProgressReporter(
        uint32_t totalSteps,
        HandlerProgressReporter::Callback callback = nullptr
    ) const {
        return HandlerProgressReporter(totalSteps, std::move(callback));
    }

    // --------------------------------------------------------------------------
    // String utilities for handlers
    // --------------------------------------------------------------------------

    /**
     * @brief Extract placeholders from text
     * @param text Text to analyze
     * @return List of placeholder infos
     */
    [[nodiscard]] std::vector<PlaceholderInfo> extractPlaceholders(
        const std::string& text
    ) const;

    /**
     * @brief Validate placeholder consistency between source and target
     * @param source Source text
     * @param target Target text
     * @return List of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> validatePlaceholders(
        const std::string& source,
        const std::string& target
    ) const;

    // --------------------------------------------------------------------------
    // Backup management
    // --------------------------------------------------------------------------

    /**
     * @brief Get backup directory path for a game
     * @param gameDir Game directory
     * @param backupId Backup identifier
     * @return Backup directory path
     */
    [[nodiscard]] fs::path getBackupPath(
        const fs::path& gameDir,
        const std::string& backupId
    ) const;

    /**
     * @brief List existing backups for a game
     * @param gameDir Game directory
     * @return List of backup IDs
     */
    [[nodiscard]] std::vector<std::string> listBackups(const fs::path& gameDir) const;

private:
    std::string handlerName_;

    // Internal backup helpers
    Result<void> copyFileToBackup(
        const fs::path& source,
        const fs::path& backupDir,
        const fs::path& gameDir
    );

    Result<void> restoreFileFromBackup(
        const fs::path& backupFile,
        const fs::path& gameDir
    );
};

// ============================================================================
// HANDLER LOGGING MACROS
// ============================================================================

/// @brief Log debug message from handler
#define HANDLER_LOG_DEBUG(msg) logDebug(msg)

/// @brief Log info message from handler
#define HANDLER_LOG_INFO(msg) logInfo(msg)

/// @brief Log warning message from handler
#define HANDLER_LOG_WARNING(msg) logWarning(msg)

/// @brief Log error message from handler
#define HANDLER_LOG_ERROR(msg) logError(msg)

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

    /**
     * @brief Apply StringClassifier to filter extraction results
     *
     * Filters out non-translatable strings (code, paths, identifiers, etc.)
     *
     * @param entries Extracted entries
     * @param minConfidence Minimum classifier confidence (0.0 - 1.0)
     * @return Filtered entries containing only translatable strings
     */
    static std::vector<TranslationEntry> filterWithClassifier(
        const std::vector<TranslationEntry>& entries,
        float minConfidence = 0.5f
    );

    /**
     * @brief Get classification statistics for extraction results
     *
     * @param entries Extracted entries
     * @return Statistics showing category distribution
     */
    static std::string getClassificationStats(
        const std::vector<TranslationEntry>& entries
    );
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
