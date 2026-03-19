/**
 * @file patch_engine.hpp
 * @brief Translation patch application and backup management
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace makine {

/**
 * @brief Single patch operation
 */
struct PatchOperation {
    enum class Type {
        Copy,       // Copy file from source to target
        Replace,    // Replace file content
        Modify,     // Modify specific bytes in file
        Delete,     // Delete file
        CreateDir   // Create directory
    };

    Type type;
    fs::path source;      // Source file (for Copy)
    fs::path target;      // Target file path
    ByteBuffer data;      // New content (for Replace/Modify)
    uint64_t offset = 0;  // File offset (for Modify)
};

/**
 * @brief Backup metadata
 */
struct BackupMetadata {
    std::string backupId;
    std::string gameId;
    std::string gameName;
    std::string patchVersion;
    fs::path backupPath;
    uint64_t createdAt;       // Unix timestamp
    uint64_t sizeBytes;
    uint32_t fileCount;
    StringList files;         // List of backed up files
};

/**
 * @brief Interface for backup storage
 */
class IBackupStorage {
public:
    virtual ~IBackupStorage() = default;

    [[nodiscard]] virtual Result<BackupMetadata> createBackup(
        const fs::path& gameDir,
        const StringList& filesToBackup,
        const std::string& backupId
    ) = 0;

    [[nodiscard]] virtual VoidResult restoreBackup(
        const fs::path& gameDir,
        const std::string& backupId
    ) = 0;

    [[nodiscard]] virtual VoidResult deleteBackup(const std::string& backupId) = 0;

    [[nodiscard]] virtual Result<std::vector<BackupMetadata>> listBackups() = 0;

    [[nodiscard]] virtual Result<BackupMetadata> getBackup(const std::string& backupId) = 0;

    [[nodiscard]] virtual bool hasBackup(const std::string& backupId) const = 0;
};

/**
 * @brief Main patch engine for applying translations
 */
class PatchEngine {
public:
    PatchEngine();
    ~PatchEngine();

    /**
     * @brief Apply a list of patch operations atomically
     *
     * Either all operations succeed or none do. Automatically
     * creates backup before patching.
     */
    [[nodiscard]] Result<PatchResult> apply(
        const std::vector<PatchOperation>& operations,
        const GameInfo& game,
        const std::string& patchVersion,
        ProgressCallback progress = nullptr,
        CancellationToken* cancel = nullptr
    );

    /**
     * @brief Create backup of specified files
     */
    [[nodiscard]] Result<BackupResult> backup(
        const fs::path& gameDir,
        const StringList& files,
        const std::string& backupId
    );

    /**
     * @brief Restore from backup
     */
    [[nodiscard]] Result<RestoreResult> restore(
        const fs::path& gameDir,
        const std::string& backupId
    );

    /**
     * @brief Check if game has a backup
     */
    [[nodiscard]] bool hasBackup(const std::string& gameId) const;

    /**
     * @brief Get backup metadata for a game
     */
    [[nodiscard]] Result<BackupMetadata> getBackupInfo(const std::string& gameId) const;

    /**
     * @brief List all backups
     */
    [[nodiscard]] Result<std::vector<BackupMetadata>> listBackups() const;

    /**
     * @brief Delete a backup
     */
    [[nodiscard]] VoidResult deleteBackup(const std::string& backupId);

    /**
     * @brief Verify game files against backup
     */
    [[nodiscard]] Result<bool> verifyIntegrity(
        const fs::path& gameDir,
        const std::string& backupId
    ) const;

    /**
     * @brief Set backup storage backend
     */
    void setBackupStorage(std::unique_ptr<IBackupStorage> storage);

    /**
     * @brief Get backup storage directory
     */
    [[nodiscard]] fs::path backupDirectory() const { return backupDir_; }

    /**
     * @brief Set backup storage directory
     */
    void setBackupDirectory(const fs::path& dir);

private:
    fs::path backupDir_;
    std::unique_ptr<IBackupStorage> backupStorage_;

    // Atomic operation helpers
    [[nodiscard]] VoidResult executeOperation(const PatchOperation& op);
    [[nodiscard]] VoidResult rollbackOperations(
        const fs::path& gameDir,
        const std::string& backupId,
        const std::vector<PatchOperation>& ops,
        size_t completedCount
    );
};

// =============================================================================
// BINARY TEXT PATCHING UTILITIES
// =============================================================================

/**
 * @brief Result of binary text patching
 */
struct BinaryPatchResult {
    int appliedCount = 0;       // Number of translations applied
    int skippedCount = 0;       // Number of translations skipped
    std::string error;          // Error message if failed
    bool success = true;
};

/**
 * @brief Options for binary text patching
 */
struct BinaryPatchOptions {
    bool createBackup = true;           // Create .makine_backup before patching
    bool skipCodeContext = true;        // Skip strings in code context (class names, etc.)
    bool allowShorterOnly = true;       // Only allow translations <= original length
    uint8_t paddingByte = 0x20;         // Padding byte for shorter translations (space)
    int maxOccurrences = -1;            // Max occurrences to patch (-1 = unlimited)
};

/**
 * @brief Smart binary text patcher
 *
 * Provides intelligent binary string replacement that:
 * - Detects code context vs UI text
 * - Handles length differences safely
 * - Creates automatic backups
 * - Supports multiple occurrences
 */
class BinaryTextPatcher {
public:
    /**
     * @brief Apply translations to a binary file
     *
     * @param filePath Path to file to patch
     * @param translations Map of original -> translated strings
     * @param options Patching options
     * @return BinaryPatchResult with statistics
     */
    [[nodiscard]] static BinaryPatchResult patchFile(
        const fs::path& filePath,
        const std::unordered_map<std::string, std::string>& translations,
        const BinaryPatchOptions& options = {}
    );

    /**
     * @brief Apply translations to binary data in memory
     *
     * @param data Binary data to patch (modified in place)
     * @param translations Map of original -> translated strings
     * @param options Patching options
     * @return BinaryPatchResult with statistics
     */
    [[nodiscard]] static BinaryPatchResult patchBuffer(
        ByteBuffer& data,
        const std::unordered_map<std::string, std::string>& translations,
        const BinaryPatchOptions& options = {}
    );

    /**
     * @brief Check if a string occurrence is in code context
     *
     * Returns true if the string appears to be a code identifier
     * (class name, function name, etc.) rather than UI text.
     *
     * Checks surrounding bytes for:
     * - Alphanumeric characters
     * - Underscore
     * - Dots (member access)
     * - Parentheses (function calls)
     *
     * @param data Full binary data
     * @param position Position of string start
     * @param length Length of string
     * @return true if string is in code context (should NOT be patched)
     */
    [[nodiscard]] static bool isCodeContext(
        const ByteBuffer& data,
        size_t position,
        size_t length
    );

    /**
     * @brief Check if a byte is a code character
     *
     * Code characters include:
     * - Letters (a-z, A-Z)
     * - Digits (0-9)
     * - Underscore (_)
     * - Dot (.)
     * - Parentheses ()
     *
     * @param byte The byte to check
     * @return true if byte is a code character
     */
    [[nodiscard]] static bool isCodeCharacter(uint8_t byte) noexcept;

private:
    // Find all occurrences of a pattern in binary data
    [[nodiscard]] static std::vector<size_t> findAllOccurrences(
        const ByteBuffer& data,
        const ByteBuffer& pattern
    );
};

/**
 * @brief Default file-based backup storage
 */
class FileBackupStorage : public IBackupStorage {
public:
    explicit FileBackupStorage(const fs::path& baseDir);

    [[nodiscard]] Result<BackupMetadata> createBackup(
        const fs::path& gameDir,
        const StringList& filesToBackup,
        const std::string& backupId
    ) override;

    [[nodiscard]] VoidResult restoreBackup(
        const fs::path& gameDir,
        const std::string& backupId
    ) override;

    [[nodiscard]] VoidResult deleteBackup(const std::string& backupId) override;

    [[nodiscard]] Result<std::vector<BackupMetadata>> listBackups() override;

    [[nodiscard]] Result<BackupMetadata> getBackup(const std::string& backupId) override;

    [[nodiscard]] bool hasBackup(const std::string& backupId) const override;

private:
    fs::path baseDir_;
    static constexpr std::string_view kMetadataFile = "backup.json";
};

} // namespace makine
