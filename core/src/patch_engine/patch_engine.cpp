/**
 * @file patch_engine.cpp
 * @brief Patch engine implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/patch_engine.hpp"
#include "makine/core.hpp"
#include "makine/logging.hpp"
#include "makine/metrics.hpp"
#include "makine/detail/scoped_metrics.hpp"
#include "makine/audit.hpp"
#include "makine/health.hpp"
#include "makine/validation.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace makine {

using json = nlohmann::json;

// FileBackupStorage implementation
FileBackupStorage::FileBackupStorage(const fs::path& baseDir)
    : baseDir_(baseDir)
{
    std::error_code ec;
    fs::create_directories(baseDir_, ec);
}

Result<BackupMetadata> FileBackupStorage::createBackup(
    const fs::path& gameDir,
    const StringList& filesToBackup,
    const std::string& backupId
) {
    ScopedMetrics m("backup_create");

    MAKINE_LOG_INFO(log::FILE, "Creating backup {} for {} files from {}",
        backupId, filesToBackup.size(), gameDir.string());

    // Validate input paths
    if (!validation::isPathSafe(gameDir)) {
        MAKINE_LOG_ERROR(log::FILE, "Unsafe game directory path: {}", gameDir.string());
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Game directory path contains unsafe characters"));
    }

    BackupMetadata metadata;
    metadata.backupId = backupId;
    metadata.backupPath = baseDir_ / backupId;
    metadata.createdAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    metadata.fileCount = 0;
    metadata.sizeBytes = 0;

    // Create backup directory
    std::error_code ec;
    fs::create_directories(metadata.backupPath, ec);
    if (ec) {
        MAKINE_LOG_ERROR(log::FILE, "Failed to create backup directory {}: {}",
            metadata.backupPath.string(), ec.message());
        AuditLogger::logFileAccess(metadata.backupPath, "create_dir", false, ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot create backup directory: " + ec.message()));
    }

    AuditLogger::logFileAccess(metadata.backupPath, "create_dir", true);

    // Copy files
    uint32_t skippedCount = 0;
    for (const auto& relPath : filesToBackup) {
        // Validate relative path
        if (!validation::isPathSafe(relPath)) {
            MAKINE_LOG_WARN(log::FILE, "Skipping unsafe path in backup: {}", relPath);
            skippedCount++;
            continue;
        }

        fs::path srcPath = gameDir / relPath;
        fs::path dstPath = metadata.backupPath / relPath;

        if (!fs::exists(srcPath)) {
            MAKINE_LOG_DEBUG(log::FILE, "Backup source does not exist: {}", srcPath.string());
            continue;
        }

        // Create parent directories
        fs::create_directories(dstPath.parent_path(), ec);
        if (ec) {
            MAKINE_LOG_WARN(log::FILE, "Cannot create directory for backup: {} - {}",
                relPath, ec.message());
            continue;
        }

        // Copy file
        fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            MAKINE_LOG_WARN(log::FILE, "Cannot backup file: {} - {}", relPath, ec.message());
            AuditLogger::logFileAccess(srcPath, "backup_copy", false, ec.message());
            continue;
        }

        AuditLogger::logFileAccess(srcPath, "backup_copy", true);
        metadata.files.push_back(relPath);
        metadata.fileCount++;
        metadata.sizeBytes += fs::file_size(dstPath, ec);
    }

    Metrics::instance().increment("backup_files_copied", metadata.fileCount);
    if (skippedCount > 0) {
        MAKINE_LOG_WARN(log::FILE, "Skipped {} unsafe paths during backup", skippedCount);
    }

    // Save metadata
    MAKINE_LOG_DEBUG(log::FILE, "Saving backup metadata for {}", backupId);

    json metaJson;
    metaJson["backupId"] = metadata.backupId;
    metaJson["gameId"] = metadata.gameId;
    metaJson["gameName"] = metadata.gameName;
    metaJson["patchVersion"] = metadata.patchVersion;
    metaJson["createdAt"] = metadata.createdAt;
    metaJson["sizeBytes"] = metadata.sizeBytes;
    metaJson["fileCount"] = metadata.fileCount;
    metaJson["files"] = metadata.files;

    // Atomic metadata write: write to temp, then rename
    fs::path metaPath = metadata.backupPath / kMetadataFile;
    fs::path tempMetaPath = metaPath.string() + ".tmp";

    {
        std::ofstream metaFile(tempMetaPath, std::ios::trunc);
        if (!metaFile) {
            MAKINE_LOG_ERROR(log::FILE, "Cannot create backup metadata file: {}",
                tempMetaPath.string());
            AuditLogger::logFileAccess(tempMetaPath, "write", false, "Cannot create file");
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot write backup metadata"));
        }

        metaFile << metaJson.dump(2);
        metaFile.flush();

        if (!metaFile.good()) {
            metaFile.close();
            fs::remove(tempMetaPath, ec);
            MAKINE_LOG_ERROR(log::FILE, "Backup metadata write failed - possible disk full");
            AuditLogger::logFileAccess(tempMetaPath, "write", false, "Write failed");
            return std::unexpected(Error(ErrorCode::IOError,
                "Backup metadata write failed - possible disk full"));
        }
    } // File closed here

    // Atomic rename
    fs::rename(tempMetaPath, metaPath, ec);
    if (ec) {
        fs::remove(tempMetaPath, ec);
        MAKINE_LOG_ERROR(log::FILE, "Backup metadata rename failed: {}", ec.message());
        AuditLogger::logFileAccess(metaPath, "rename", false, ec.message());
        return std::unexpected(Error(ErrorCode::IOError,
            "Backup metadata rename failed: " + ec.message()));
    }

    AuditLogger::logFileAccess(metaPath, "write", true, "Backup metadata saved");

    MAKINE_LOG_INFO(log::FILE, "Created backup {} with {} files ({} bytes)",
        backupId, metadata.fileCount, metadata.sizeBytes);

    Metrics::instance().recordHistogram("backup_size_bytes", metadata.sizeBytes);
    Metrics::instance().increment("backups_created");

    return metadata;
}

VoidResult FileBackupStorage::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    ScopedMetrics m("backup_restore");

    MAKINE_LOG_INFO(log::FILE, "Restoring backup {} to {}", backupId, gameDir.string());
    AuditLogger::logPatchOperation(backupId, true, "restore_start",
        "Restoring backup to " + gameDir.string());

    // Validate game directory path
    if (!validation::isPathSafe(gameDir)) {
        MAKINE_LOG_ERROR(log::FILE, "Unsafe game directory path: {}", gameDir.string());
        AuditLogger::logPatchOperation(backupId, false, "restore", "Unsafe path");
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Game directory path contains unsafe characters"));
    }

    auto metaResult = getBackup(backupId);
    if (!metaResult) {
        MAKINE_LOG_ERROR(log::FILE, "Backup not found: {}", backupId);
        AuditLogger::logPatchOperation(backupId, false, "restore", "Backup not found");
        return std::unexpected(metaResult.error());
    }

    const auto& metadata = *metaResult;
    uint32_t restored = 0;
    uint32_t failed = 0;
    std::vector<std::string> failedFiles;
    failedFiles.reserve(metadata.files.size());

    for (const auto& relPath : metadata.files) {
        fs::path srcPath = metadata.backupPath / relPath;
        fs::path dstPath = gameDir / relPath;

        // SECURITY: Path traversal check using validation
        if (!validation::isPathSafe(relPath)) {
            MAKINE_LOG_ERROR(log::SECURITY, "Path traversal detected in backup: {}", relPath);
            AuditLogger::logFileAccess(srcPath, "restore", false, "Path traversal attempt");
            failed++;
            failedFiles.push_back(relPath + " (path traversal)");
            continue;
        }

        if (!fs::exists(srcPath)) {
            MAKINE_LOG_ERROR(log::FILE, "Backup file missing: {}", relPath);
            failed++;
            failedFiles.push_back(relPath + " (missing)");
            continue;
        }

        std::error_code ec;
        fs::create_directories(dstPath.parent_path(), ec);
        if (ec) {
            MAKINE_LOG_ERROR(log::FILE, "Cannot create directory for: {} - {}",
                relPath, ec.message());
            failed++;
            failedFiles.push_back(relPath + " (dir create: " + ec.message() + ")");
            continue;
        }

        // Atomic restore: copy to temp file first, then rename
        fs::path tempPath = dstPath.string() + ".makine_restore_tmp";

        // Remove any stale temp file
        fs::remove(tempPath, ec);

        // Copy to temp
        fs::copy_file(srcPath, tempPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            MAKINE_LOG_ERROR(log::FILE, "Cannot copy backup file: {} - {}",
                relPath, ec.message());
            AuditLogger::logFileAccess(srcPath, "restore_copy", false, ec.message());
            fs::remove(tempPath, ec);
            failed++;
            failedFiles.push_back(relPath + " (copy: " + ec.message() + ")");
            continue;
        }

        // Verify copy size matches
        auto srcSize = fs::file_size(srcPath, ec);
        auto tmpSize = fs::file_size(tempPath, ec);
        if (srcSize != tmpSize) {
            MAKINE_LOG_ERROR(log::FILE, "Backup restore size mismatch: {} (expected {} got {})",
                relPath, srcSize, tmpSize);
            fs::remove(tempPath, ec);
            failed++;
            failedFiles.push_back(relPath + " (size mismatch)");
            continue;
        }

        // Atomic rename
        fs::rename(tempPath, dstPath, ec);
        if (ec) {
            MAKINE_LOG_ERROR(log::FILE, "Cannot rename restored file: {} - {}",
                relPath, ec.message());
            AuditLogger::logFileAccess(dstPath, "restore_rename", false, ec.message());
            fs::remove(tempPath, ec);
            failed++;
            failedFiles.push_back(relPath + " (rename: " + ec.message() + ")");
            continue;
        }

        AuditLogger::logFileAccess(dstPath, "restore", true);
        restored++;
    }

    MAKINE_LOG_INFO(log::FILE, "Restored {} files from backup {} ({} failed)",
        restored, backupId, failed);

    Metrics::instance().increment("backup_files_restored", restored);

    // CRITICAL: ANY failure means game may be corrupted
    // We must report this so manual intervention can occur
    if (failed > 0) {
        std::string errorMsg = fmt::format("Restore incomplete: {} of {} files failed. Failed files: ",
            failed, metadata.files.size());
        for (size_t i = 0; i < failedFiles.size() && i < 5; ++i) {
            if (i > 0) errorMsg += ", ";
            errorMsg += failedFiles[i];
        }
        if (failedFiles.size() > 5) {
            errorMsg += fmt::format(" (and {} more)", failedFiles.size() - 5);
        }

        MAKINE_LOG_ERROR(log::FILE, "Restore failed: {}", errorMsg);
        AuditLogger::logPatchOperation(backupId, false, "restore_end", errorMsg);
        Metrics::instance().increment("restore_failures");
        return std::unexpected(Error(ErrorCode::RestoreFailed, errorMsg));
    }

    AuditLogger::logPatchOperation(backupId, true, "restore_end",
        fmt::format("Restored {} files", restored));
    Metrics::instance().increment("restores_completed");

    return {};
}

VoidResult FileBackupStorage::deleteBackup(const std::string& backupId) {
    MAKINE_LOG_INFO(log::FILE, "Deleting backup: {}", backupId);

    fs::path backupPath = baseDir_ / backupId;

    if (!fs::exists(backupPath)) {
        MAKINE_LOG_WARN(log::FILE, "Backup not found for deletion: {}", backupId);
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Backup not found: " + backupId));
    }

    std::error_code ec;
    fs::remove_all(backupPath, ec);

    if (ec) {
        MAKINE_LOG_ERROR(log::FILE, "Cannot delete backup {}: {}", backupId, ec.message());
        AuditLogger::logFileAccess(backupPath, "delete", false, ec.message());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            fmt::format("Cannot delete backup: {}", ec.message())));
    }

    MAKINE_LOG_INFO(log::FILE, "Deleted backup: {}", backupId);
    AuditLogger::logFileAccess(backupPath, "delete", true);
    Metrics::instance().increment("backups_deleted");

    return {};
}

Result<std::vector<BackupMetadata>> FileBackupStorage::listBackups() {
    std::vector<BackupMetadata> backups;

    if (!fs::exists(baseDir_)) {
        return backups;
    }

    for (const auto& entry : fs::directory_iterator(baseDir_)) {
        if (entry.is_directory()) {
            auto metaPath = entry.path() / kMetadataFile;
            if (fs::exists(metaPath)) {
                auto result = getBackup(entry.path().filename().string());
                if (result) {
                    backups.push_back(std::move(*result));
                }
            }
        }
    }

    return backups;
}

Result<BackupMetadata> FileBackupStorage::getBackup(const std::string& backupId) {
    fs::path metaPath = baseDir_ / backupId / kMetadataFile;

    if (!fs::exists(metaPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Backup not found: " + backupId));
    }

    std::ifstream metaFile(metaPath);
    if (!metaFile) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot read backup metadata"));
    }

    try {
        json metaJson = json::parse(metaFile);

        BackupMetadata metadata;
        metadata.backupId = metaJson.value("backupId", backupId);
        metadata.gameId = metaJson.value("gameId", "");
        metadata.gameName = metaJson.value("gameName", "");
        metadata.patchVersion = metaJson.value("patchVersion", "");
        metadata.backupPath = baseDir_ / backupId;
        metadata.createdAt = metaJson.value("createdAt", 0ULL);
        metadata.sizeBytes = metaJson.value("sizeBytes", 0ULL);
        metadata.fileCount = metaJson.value("fileCount", 0U);
        metadata.files = metaJson.value("files", StringList{});

        return metadata;

    } catch (const json::exception& e) {
        return std::unexpected(Error(ErrorCode::ParseError,
            fmt::format("Invalid backup metadata: {}", e.what())));
    }
}

bool FileBackupStorage::hasBackup(const std::string& backupId) const {
    return fs::exists(baseDir_ / backupId / kMetadataFile);
}

// PatchEngine implementation
PatchEngine::PatchEngine() {
    backupStorage_ = std::make_unique<FileBackupStorage>(backupDir_);
}

PatchEngine::~PatchEngine() = default;

void PatchEngine::setBackupDirectory(const fs::path& dir) {
    backupDir_ = dir;
    backupStorage_ = std::make_unique<FileBackupStorage>(backupDir_);
}

Result<PatchResult> PatchEngine::apply(
    const std::vector<PatchOperation>& operations,
    const GameInfo& game,
    const std::string& patchVersion,
    ProgressCallback progress,
    CancellationToken* cancel
) {
    ScopedMetrics m("patch_apply");

    MAKINE_LOG_INFO(log::HANDLER, "Starting patch operation for game {} (version {})",
        game.name, patchVersion);
    MAKINE_LOG_DEBUG(log::HANDLER, "Patch contains {} operations", operations.size());

    // Audit log the start of patching
    AuditLogger::logPatchOperation(game.id.storeId, true, "apply_start",
        fmt::format("Starting patch with {} operations", operations.size()));

    PatchResult result;
    result.success = false;
    result.filesPatched = 0;
    result.filesFailed = 0;

    if (operations.empty()) {
        result.success = true;
        result.message = "No operations to apply";
        MAKINE_LOG_INFO(log::HANDLER, "No patch operations to apply");
        return result;
    }

    // Validate game install path
    auto pathValidation = validation::validateDirectory(game.installPath);
    if (!pathValidation) {
        result.message = "Invalid game install path: " + pathValidation.error().message();
        MAKINE_LOG_ERROR(log::HANDLER, "{}", result.message);
        AuditLogger::logPatchOperation(game.id.storeId, false, "apply", result.message);
        return result;
    }

    // HEALTH CHECK: Verify system health before patching
    MAKINE_LOG_DEBUG(log::HANDLER, "Performing pre-patch health check");
    HealthChecker::instance().setDataDirectory(game.installPath);
    auto healthStatus = HealthChecker::instance().check();

    if (!healthStatus.fileSystem.healthy) {
        result.message = "File system health check failed: " + healthStatus.fileSystem.status;
        MAKINE_LOG_ERROR(log::HANDLER, "{}", result.message);
        AuditLogger::logPatchOperation(game.id.storeId, false, "apply", "Health check failed");
        return result;
    }

    // SAFETY: Check disk space before starting
    // Calculate total size needed (backup + new files)
    uint64_t requiredSpace = 0;
    for (const auto& op : operations) {
        if (op.type == PatchOperation::Type::Replace ||
            op.type == PatchOperation::Type::Copy) {
            if (!op.data.empty()) {
                requiredSpace += op.data.size();
            } else if (!op.source.empty() && fs::exists(op.source)) {
                std::error_code ec;
                requiredSpace += fs::file_size(op.source, ec);
            }
        }
        // Add backup space (existing files that will be modified)
        if (op.type == PatchOperation::Type::Replace ||
            op.type == PatchOperation::Type::Modify ||
            op.type == PatchOperation::Type::Delete) {
            if (fs::exists(op.target)) {
                std::error_code ec;
                requiredSpace += fs::file_size(op.target, ec);
            }
        }
    }

    // Add 10% safety margin
    requiredSpace = static_cast<uint64_t>(requiredSpace * 1.1);

    MAKINE_LOG_DEBUG(log::HANDLER, "Patch requires approximately {} MB of disk space",
        requiredSpace / (1024 * 1024));

    // Check available space using health checker data or filesystem query
    std::error_code spaceEc;
    auto spaceInfo = fs::space(game.installPath, spaceEc);
    if (!spaceEc && spaceInfo.available < requiredSpace) {
        result.message = fmt::format("Insufficient disk space. Required: {} MB, Available: {} MB",
            requiredSpace / (1024 * 1024), spaceInfo.available / (1024 * 1024));
        MAKINE_LOG_ERROR(log::HANDLER, "{}", result.message);
        AuditLogger::logPatchOperation(game.id.storeId, false, "apply", result.message);
        Metrics::instance().increment("patch_insufficient_space");
        return result;
    }

    Metrics::instance().gauge("patch_required_space_mb", requiredSpace / (1024.0 * 1024.0));

    // Create backup ID
    std::string backupId = game.id.storeId + "_" + patchVersion;
    MAKINE_LOG_DEBUG(log::HANDLER, "Backup ID: {}", backupId);

    // Collect files to backup
    StringList filesToBackup;
    for (const auto& op : operations) {
        if (op.type == PatchOperation::Type::Replace ||
            op.type == PatchOperation::Type::Modify ||
            op.type == PatchOperation::Type::Delete) {
            auto relPath = fs::relative(op.target, game.installPath);
            filesToBackup.push_back(relPath.string());
        }
    }

    MAKINE_LOG_DEBUG(log::HANDLER, "Files to backup: {}", filesToBackup.size());

    // SAFETY: Pre-flight file lock check
    // Verify all target files are writable (game not running)
    MAKINE_LOG_DEBUG(log::HANDLER, "Checking for locked files");
    std::vector<std::string> lockedFiles;
    lockedFiles.reserve(operations.size());
    for (const auto& op : operations) {
        if (op.type == PatchOperation::Type::Replace ||
            op.type == PatchOperation::Type::Modify ||
            op.type == PatchOperation::Type::Delete) {
            if (fs::exists(op.target)) {
                // Try to open file for writing to check if locked
                std::ofstream testFile(op.target, std::ios::binary | std::ios::app);
                if (!testFile) {
                    MAKINE_LOG_WARN(log::HANDLER, "File is locked: {}",
                        op.target.filename().string());
                    lockedFiles.push_back(op.target.filename().string());
                }
            }
        }
    }

    if (!lockedFiles.empty()) {
        result.message = fmt::format("Cannot patch: {} file(s) are locked (game may be running). Locked files: ",
            lockedFiles.size());
        for (size_t i = 0; i < lockedFiles.size() && i < 3; ++i) {
            if (i > 0) result.message += ", ";
            result.message += lockedFiles[i];
        }
        if (lockedFiles.size() > 3) {
            result.message += fmt::format(" (and {} more)", lockedFiles.size() - 3);
        }
        MAKINE_LOG_ERROR(log::HANDLER, "{}", result.message);
        AuditLogger::logPatchOperation(game.id.storeId, false, "apply", "Files locked");
        Metrics::instance().increment("patch_files_locked");
        return result;
    }

    // Create backup
    if (!filesToBackup.empty()) {
        MAKINE_LOG_INFO(log::HANDLER, "Creating backup before patching");
        if (progress) {
            progress(0, static_cast<uint32_t>(operations.size()), "Creating backup...");
        }

        auto backupResult = backupStorage_->createBackup(
            game.installPath, filesToBackup, backupId);

        if (!backupResult) {
            result.message = "Backup failed: " + backupResult.error().message();
            MAKINE_LOG_ERROR(log::HANDLER, "{}", result.message);
            AuditLogger::logPatchOperation(game.id.storeId, false, "apply", result.message);
            return result;
        }

        result.backupPath = backupResult->backupPath;
        MAKINE_LOG_INFO(log::HANDLER, "Backup created at: {}", result.backupPath.string());
    }

    // Apply operations
    MAKINE_LOG_INFO(log::HANDLER, "Applying {} patch operations", operations.size());
    size_t completedOps = 0;
    for (const auto& op : operations) {
        if (cancel && cancel->isCancelled()) {
            MAKINE_LOG_WARN(log::HANDLER, "Patch operation cancelled by user, rolling back");
            // Rollback using backup
            auto rollbackResult = rollbackOperations(
                game.installPath, backupId, operations, completedOps);
            if (!rollbackResult) {
                MAKINE_LOG_ERROR(log::HANDLER, "Rollback failed: {}",
                    rollbackResult.error().message());
            }
            result.message = "Operation cancelled, changes rolled back";
            AuditLogger::logPatchOperation(game.id.storeId, false, "apply", "Cancelled by user");
            Metrics::instance().increment("patch_cancelled");
            return result;
        }

        if (progress) {
            progress(
                static_cast<uint32_t>(completedOps),
                static_cast<uint32_t>(operations.size()),
                "Patching: " + op.target.filename().string()
            );
        }

        MAKINE_LOG_DEBUG(log::HANDLER, "Executing operation {} of {}: {}",
            completedOps + 1, operations.size(), op.target.filename().string());

        auto opResult = executeOperation(op);
        if (!opResult) {
            // CRITICAL: Operation failed - rollback ALL completed operations immediately
            // We cannot leave the game in a partial/broken state
            MAKINE_LOG_ERROR(log::HANDLER,
                "Patch operation failed: {} - {}. Rolling back {} completed operations.",
                op.target.string(), opResult.error().message(), completedOps);

            AuditLogger::logFileAccess(op.target, "patch", false, opResult.error().message());
            Metrics::instance().increment("patch_operation_failures");

            result.filesFailed++;
            result.errors.push_back(op.target.string() + ": " + opResult.error().message());

            // Attempt rollback using backup
            MAKINE_LOG_INFO(log::HANDLER, "Initiating rollback");
            auto rollbackResult = rollbackOperations(
                game.installPath, backupId, operations, completedOps);
            if (!rollbackResult) {
                MAKINE_LOG_CRITICAL(log::HANDLER,
                    "CRITICAL: Rollback also failed: {}. Game may be corrupted!",
                    rollbackResult.error().message());
                result.errors.push_back("ROLLBACK FAILED: " + rollbackResult.error().message());
                result.message = "Patch failed AND rollback failed - manual restoration required from: " +
                    result.backupPath.string();
                AuditLogger::logPatchOperation(game.id.storeId, false, "apply",
                    "CRITICAL: Both patch and rollback failed");
                Metrics::instance().increment("patch_rollback_failures");
            } else {
                result.message = "Patch failed, changes rolled back successfully";
                AuditLogger::logPatchOperation(game.id.storeId, false, "apply",
                    "Patch failed, rolled back");
            }

            result.success = false;
            return result;
        }

        AuditLogger::logFileAccess(op.target, "patch", true);
        result.filesPatched++;
        completedOps++;
    }

    result.success = (result.filesFailed == 0);
    result.message = fmt::format("Patched {} files, {} failed",
        result.filesPatched, result.filesFailed);

    MAKINE_LOG_INFO(log::HANDLER, "Patch operation completed: {}", result.message);
    AuditLogger::logPatchOperation(game.id.storeId, true, "apply_end", result.message);

    // Record metrics
    Metrics::instance().increment("patch_operations_completed");
    Metrics::instance().recordHistogram("patch_files_count", result.filesPatched);

    if (progress) {
        progress(
            static_cast<uint32_t>(operations.size()),
            static_cast<uint32_t>(operations.size()),
            result.message
        );
    }

    return result;
}

VoidResult PatchEngine::executeOperation(const PatchOperation& op) {
    ScopedMetrics opMetrics("patch_single_operation");

    std::error_code ec;

    // Pre-flight check: verify target path is safe using validation
    if (!validation::isPathSafe(op.target)) {
        MAKINE_LOG_ERROR(log::SECURITY, "Path traversal detected in target: {}",
            op.target.string());
        AuditLogger::logFileAccess(op.target, "patch", false, "Path traversal blocked");
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Path traversal detected in target path"));
    }

    // Also validate source path if applicable
    if (!op.source.empty() && !validation::isPathSafe(op.source)) {
        MAKINE_LOG_ERROR(log::SECURITY, "Path traversal detected in source: {}",
            op.source.string());
        AuditLogger::logFileAccess(op.source, "patch", false, "Path traversal blocked");
        return std::unexpected(Error(ErrorCode::InvalidPath,
            "Path traversal detected in source path"));
    }

    switch (op.type) {
        case PatchOperation::Type::Copy:
            MAKINE_LOG_DEBUG(log::FILE, "Copy operation: {} -> {}",
                op.source.string(), op.target.string());

            if (!fs::exists(op.source)) {
                MAKINE_LOG_ERROR(log::FILE, "Source file not found: {}", op.source.string());
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Source file not found: " + op.source.string()));
            }

            // Check source is readable
            {
                std::ifstream testRead(op.source, std::ios::binary);
                if (!testRead) {
                    MAKINE_LOG_ERROR(log::FILE, "Cannot read source file: {}",
                        op.source.string());
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot read source file: " + op.source.string()));
                }
            }

            fs::create_directories(op.target.parent_path(), ec);
            if (ec) {
                MAKINE_LOG_ERROR(log::FILE, "Cannot create target directory: {}", ec.message());
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Cannot create target directory: " + ec.message()));
            }

            // Atomic copy: copy to temp file first, then rename
            {
                fs::path tempPath = op.target.string() + ".makine_tmp";

                // Remove stale temp file if exists
                fs::remove(tempPath, ec);

                fs::copy_file(op.source, tempPath, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    MAKINE_LOG_ERROR(log::FILE, "Copy to temp failed: {}", ec.message());
                    fs::remove(tempPath, ec);
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Copy to temp failed: " + ec.message()));
                }

                // Verify copy size matches
                auto srcSize = fs::file_size(op.source, ec);
                auto tmpSize = fs::file_size(tempPath, ec);
                if (srcSize != tmpSize) {
                    MAKINE_LOG_ERROR(log::FILE, "Copy verification failed: size mismatch ({} vs {})",
                        srcSize, tmpSize);
                    fs::remove(tempPath, ec);
                    return std::unexpected(Error(ErrorCode::ChecksumMismatch,
                        "Copy verification failed: size mismatch"));
                }

                // Atomic rename (or copy+delete on Windows if cross-volume)
                fs::rename(tempPath, op.target, ec);
                if (ec) {
                    // Fallback: copy then delete temp
                    MAKINE_LOG_DEBUG(log::FILE, "Rename failed, using copy fallback");
                    fs::copy_file(tempPath, op.target, fs::copy_options::overwrite_existing, ec);
                    fs::remove(tempPath, ec);
                    if (ec) {
                        MAKINE_LOG_ERROR(log::FILE, "Failed to finalize copy: {}", ec.message());
                        return std::unexpected(Error(ErrorCode::FileAccessDenied,
                            "Failed to finalize copy: " + ec.message()));
                    }
                }
            }

            Metrics::instance().increment("patch_copies_completed");
            break;

        case PatchOperation::Type::Replace:
            MAKINE_LOG_DEBUG(log::FILE, "Replace operation: {} ({} bytes)",
                op.target.string(), op.data.size());

            fs::create_directories(op.target.parent_path(), ec);
            if (ec) {
                MAKINE_LOG_ERROR(log::FILE, "Cannot create target directory: {}", ec.message());
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Cannot create target directory: " + ec.message()));
            }

            // Atomic write: write to temp file, verify, then rename
            {
                fs::path tempPath = op.target.string() + ".makine_tmp";

                std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
                if (!file) {
                    MAKINE_LOG_ERROR(log::FILE, "Cannot create temp file for write: {}",
                        tempPath.string());
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot create temp file for write"));
                }

                file.write(reinterpret_cast<const char*>(op.data.data()), op.data.size());

                // Explicit flush and sync
                file.flush();
                if (!file.good()) {
                    file.close();
                    fs::remove(tempPath, ec);
                    MAKINE_LOG_ERROR(log::FILE, "Write flush failed - possible disk full");
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Write flush failed - possible disk full"));
                }

                // Verify written size
                auto writtenPos = file.tellp();
                file.close();

                if (writtenPos < 0 || static_cast<size_t>(writtenPos) != op.data.size()) {
                    MAKINE_LOG_ERROR(log::FILE, "Write verification failed: expected {}, got {}",
                        op.data.size(), static_cast<std::streamoff>(writtenPos));
                    fs::remove(tempPath, ec);
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Write verification failed: incomplete write"));
                }

                // Atomic rename
                fs::rename(tempPath, op.target, ec);
                if (ec) {
                    MAKINE_LOG_DEBUG(log::FILE, "Rename failed, using copy fallback");
                    fs::copy_file(tempPath, op.target, fs::copy_options::overwrite_existing, ec);
                    fs::remove(tempPath, ec);
                    if (ec) {
                        MAKINE_LOG_ERROR(log::FILE, "Failed to finalize write: {}", ec.message());
                        return std::unexpected(Error(ErrorCode::FileAccessDenied,
                            "Failed to finalize write: " + ec.message()));
                    }
                }
            }

            Metrics::instance().increment("patch_replaces_completed");
            Metrics::instance().recordHistogram("patch_replace_bytes", op.data.size());
            break;

        case PatchOperation::Type::Modify:
            MAKINE_LOG_DEBUG(log::FILE, "Modify operation: {} at offset {} ({} bytes)",
                op.target.string(), op.offset, op.data.size());

            // For modify, we need to be extra careful - backup first
            {
                // Verify file exists and is writable
                if (!fs::exists(op.target)) {
                    MAKINE_LOG_ERROR(log::FILE, "Target file not found for modification: {}",
                        op.target.string());
                    return std::unexpected(Error(ErrorCode::FileNotFound,
                        "Target file not found for modification"));
                }

                // Read entire file, modify in memory, write atomically
                std::ifstream readFile(op.target, std::ios::binary);
                if (!readFile) {
                    MAKINE_LOG_ERROR(log::FILE, "Cannot open file for modification: {}",
                        op.target.string());
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot open file for modification"));
                }

                // Read all content
                readFile.seekg(0, std::ios::end);
                auto fileSize = readFile.tellg();
                readFile.seekg(0, std::ios::beg);

                std::vector<char> content(static_cast<size_t>(fileSize));
                readFile.read(content.data(), fileSize);
                readFile.close();

                MAKINE_LOG_DEBUG(log::FILE, "Read {} bytes for modification", static_cast<std::streamoff>(fileSize));

                // Bounds check for modification
                if (op.offset + op.data.size() > content.size()) {
                    MAKINE_LOG_ERROR(log::FILE,
                        "Modification bounds error: offset {} + size {} > file size {}",
                        op.offset, op.data.size(), content.size());
                    return std::unexpected(Error(ErrorCode::InvalidOffset,
                        "Modification offset + size exceeds file bounds"));
                }

                // Apply modification in memory
                std::memcpy(content.data() + op.offset, op.data.data(), op.data.size());

                // Write atomically via temp file
                fs::path tempPath = op.target.string() + ".makine_tmp";
                std::ofstream writeFile(tempPath, std::ios::binary | std::ios::trunc);
                if (!writeFile) {
                    MAKINE_LOG_ERROR(log::FILE, "Cannot create temp file for modification");
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot create temp file for modification"));
                }

                writeFile.write(content.data(), content.size());
                writeFile.flush();

                if (!writeFile.good()) {
                    writeFile.close();
                    fs::remove(tempPath, ec);
                    MAKINE_LOG_ERROR(log::FILE, "Modification write failed");
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Modification write failed"));
                }
                writeFile.close();

                // Atomic rename
                fs::rename(tempPath, op.target, ec);
                if (ec) {
                    MAKINE_LOG_DEBUG(log::FILE, "Rename failed, using copy fallback");
                    fs::copy_file(tempPath, op.target, fs::copy_options::overwrite_existing, ec);
                    fs::remove(tempPath, ec);
                }
            }

            Metrics::instance().increment("patch_modifications_completed");
            break;

        case PatchOperation::Type::Delete:
            MAKINE_LOG_DEBUG(log::FILE, "Delete operation: {}", op.target.string());

            if (fs::exists(op.target)) {
                // Verify it's a regular file, not a directory or symlink to system
                if (!fs::is_regular_file(op.target)) {
                    MAKINE_LOG_ERROR(log::FILE, "Delete target is not a regular file: {}",
                        op.target.string());
                    return std::unexpected(Error(ErrorCode::InvalidPath,
                        "Delete target is not a regular file"));
                }

                fs::remove(op.target, ec);
                if (ec) {
                    MAKINE_LOG_ERROR(log::FILE, "Delete failed: {} - {}",
                        op.target.string(), ec.message());
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Delete failed: " + ec.message()));
                }

                // Verify deletion
                if (fs::exists(op.target)) {
                    MAKINE_LOG_ERROR(log::FILE, "Delete verification failed: file still exists");
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Delete verification failed: file still exists"));
                }

                MAKINE_LOG_DEBUG(log::FILE, "Successfully deleted: {}", op.target.string());
            } else {
                MAKINE_LOG_DEBUG(log::FILE, "Delete target does not exist (skipped): {}",
                    op.target.string());
            }

            Metrics::instance().increment("patch_deletes_completed");
            break;

        case PatchOperation::Type::CreateDir:
            MAKINE_LOG_DEBUG(log::FILE, "CreateDir operation: {}", op.target.string());

            fs::create_directories(op.target, ec);
            if (ec) {
                MAKINE_LOG_ERROR(log::FILE, "Cannot create directory: {} - {}",
                    op.target.string(), ec.message());
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Cannot create directory: " + ec.message()));
            }

            // Verify creation
            if (!fs::exists(op.target) || !fs::is_directory(op.target)) {
                MAKINE_LOG_ERROR(log::FILE, "Directory creation verification failed: {}",
                    op.target.string());
                return std::unexpected(Error(ErrorCode::IOError,
                    "Directory creation verification failed"));
            }

            MAKINE_LOG_DEBUG(log::FILE, "Successfully created directory: {}",
                op.target.string());
            Metrics::instance().increment("patch_dirs_created");
            break;
    }

    return {};
}

VoidResult PatchEngine::rollbackOperations(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<PatchOperation>& ops,
    size_t completedCount
) {
    ScopedMetrics rollbackMetrics("patch_rollback");

    MAKINE_LOG_INFO(log::HANDLER, "Rolling back {} completed operations using backup {}",
        completedCount, backupId);
    AuditLogger::logPatchOperation(backupId, true, "rollback_start",
        fmt::format("Rolling back {} operations", completedCount));

    // First, try to restore from backup
    if (backupStorage_->hasBackup(backupId)) {
        MAKINE_LOG_INFO(log::HANDLER, "Restoring from backup: {}", backupId);
        auto restoreResult = backupStorage_->restoreBackup(gameDir, backupId);
        if (!restoreResult) {
            MAKINE_LOG_ERROR(log::HANDLER, "Backup restore failed: {}",
                restoreResult.error().message());
            // Continue with manual cleanup
        } else {
            MAKINE_LOG_INFO(log::HANDLER, "Successfully restored from backup");
        }
    } else {
        MAKINE_LOG_WARN(log::HANDLER, "No backup found for rollback: {}", backupId);
    }

    // Clean up any new files that were created by Copy operations
    std::error_code ec;
    uint32_t cleanedFiles = 0;
    uint32_t cleanedDirs = 0;

    for (size_t i = 0; i < completedCount && i < ops.size(); ++i) {
        const auto& op = ops[i];

        if (op.type == PatchOperation::Type::Copy) {
            // Remove the copied file if it's a new file (not in backup)
            if (fs::exists(op.target)) {
                // Check if this file existed before (would be in backup)
                auto relPath = fs::relative(op.target, gameDir);
                fs::path backupFilePath;

                auto backupMeta = backupStorage_->getBackup(backupId);
                if (backupMeta) {
                    backupFilePath = backupMeta->backupPath / relPath;
                }

                // Only remove if this was a NEW file (not in backup)
                if (backupFilePath.empty() || !fs::exists(backupFilePath)) {
                    fs::remove(op.target, ec);
                    if (!ec) {
                        MAKINE_LOG_DEBUG(log::FILE, "Removed new file during rollback: {}",
                            op.target.string());
                        cleanedFiles++;
                    }
                }
            }
        }
        else if (op.type == PatchOperation::Type::CreateDir) {
            // Try to remove created directory if empty
            if (fs::exists(op.target) && fs::is_empty(op.target)) {
                fs::remove(op.target, ec);
                if (!ec) {
                    MAKINE_LOG_DEBUG(log::FILE, "Removed empty directory during rollback: {}",
                        op.target.string());
                    cleanedDirs++;
                }
            }
        }
    }

    MAKINE_LOG_INFO(log::HANDLER, "Rollback completed: cleaned {} files, {} directories",
        cleanedFiles, cleanedDirs);
    AuditLogger::logPatchOperation(backupId, true, "rollback_end",
        fmt::format("Rollback completed: cleaned {} files", cleanedFiles));
    Metrics::instance().increment("patch_rollbacks_completed");

    return {};
}

Result<BackupResult> PatchEngine::backup(
    const fs::path& gameDir,
    const StringList& files,
    const std::string& backupId
) {
    MAKINE_LOG_INFO(log::HANDLER, "Creating manual backup {} for {} files",
        backupId, files.size());

    auto result = backupStorage_->createBackup(gameDir, files, backupId);
    if (!result) {
        MAKINE_LOG_ERROR(log::HANDLER, "Manual backup failed: {}", result.error().message());
        return std::unexpected(result.error());
    }

    BackupResult br;
    br.success = true;
    br.message = "Backup created";
    br.backupPath = result->backupPath;
    br.sizeBytes = result->sizeBytes;
    br.fileCount = result->fileCount;

    MAKINE_LOG_INFO(log::HANDLER, "Manual backup created: {} files, {} bytes",
        br.fileCount, br.sizeBytes);
    return br;
}

Result<RestoreResult> PatchEngine::restore(
    const fs::path& gameDir,
    const std::string& backupId
) {
    MAKINE_LOG_INFO(log::HANDLER, "Performing manual restore from backup {}", backupId);

    auto result = backupStorage_->restoreBackup(gameDir, backupId);
    if (!result) {
        MAKINE_LOG_ERROR(log::HANDLER, "Manual restore failed: {}", result.error().message());
        return std::unexpected(result.error());
    }

    RestoreResult rr;
    rr.success = true;
    rr.message = "Restore completed";

    auto meta = backupStorage_->getBackup(backupId);
    if (meta) {
        rr.filesRestored = meta->fileCount;
    }

    MAKINE_LOG_INFO(log::HANDLER, "Manual restore completed: {} files restored",
        rr.filesRestored);
    return rr;
}

bool PatchEngine::hasBackup(const std::string& gameId) const {
    return backupStorage_->hasBackup(gameId);
}

Result<BackupMetadata> PatchEngine::getBackupInfo(const std::string& gameId) const {
    return backupStorage_->getBackup(gameId);
}

Result<std::vector<BackupMetadata>> PatchEngine::listBackups() const {
    return backupStorage_->listBackups();
}

VoidResult PatchEngine::deleteBackup(const std::string& backupId) {
    return backupStorage_->deleteBackup(backupId);
}

Result<bool> PatchEngine::verifyIntegrity(
    const fs::path& gameDir,
    const std::string& backupId
) const {
    ScopedMetrics m("integrity_verify");

    MAKINE_LOG_INFO(log::HANDLER, "Verifying integrity for backup {} against {}",
        backupId, gameDir.string());

    if (!backupStorage_->hasBackup(backupId)) {
        MAKINE_LOG_ERROR(log::HANDLER, "Backup not found for integrity check: {}", backupId);
        return std::unexpected(Error(ErrorCode::BackupNotFound,
            "Backup not found: " + backupId));
    }

    auto metaResult = backupStorage_->getBackup(backupId);
    if (!metaResult) {
        MAKINE_LOG_ERROR(log::HANDLER, "Cannot read backup metadata: {}",
            metaResult.error().message());
        return std::unexpected(metaResult.error());
    }

    const auto& metadata = *metaResult;
    uint32_t filesChecked = 0;
    uint32_t filesMatched = 0;
    uint32_t filesMismatched = 0;

    // Compare each backed up file with current game file
    for (const auto& relPath : metadata.files) {
        fs::path backupFilePath = metadata.backupPath / relPath;
        fs::path currentFilePath = gameDir / relPath;

        // Check if current file exists
        if (!fs::exists(currentFilePath)) {
            MAKINE_LOG_DEBUG(log::FILE, "Integrity check: File missing in game: {}", relPath);
            filesMismatched++;
            continue;  // File was deleted - game is modified
        }

        // Check if backup file exists
        if (!fs::exists(backupFilePath)) {
            MAKINE_LOG_WARN(log::FILE, "Integrity check: Backup file missing: {}", relPath);
            continue;  // Skip this file, backup is incomplete
        }

        filesChecked++;

        // Compare file sizes first (fast check)
        std::error_code ec;
        auto currentSize = fs::file_size(currentFilePath, ec);
        auto backupSize = fs::file_size(backupFilePath, ec);

        if (currentSize != backupSize) {
            MAKINE_LOG_DEBUG(log::FILE, "Integrity check: Size mismatch for {}: {} vs {}",
                relPath, currentSize, backupSize);
            filesMismatched++;
            continue;  // Files are different
        }

        // Compare file contents (for thorough verification)
        std::ifstream currentFile(currentFilePath, std::ios::binary);
        std::ifstream backupFile(backupFilePath, std::ios::binary);

        if (!currentFile || !backupFile) {
            MAKINE_LOG_WARN(log::FILE, "Integrity check: Cannot read files for comparison: {}",
                relPath);
            continue;
        }

        // Compare in chunks
        constexpr size_t bufferSize = 8192;
        std::vector<char> currentBuffer(bufferSize);
        std::vector<char> backupBuffer(bufferSize);
        bool contentMatches = true;

        while (currentFile && backupFile) {
            currentFile.read(currentBuffer.data(), bufferSize);
            backupFile.read(backupBuffer.data(), bufferSize);

            auto currentRead = currentFile.gcount();
            auto backupRead = backupFile.gcount();

            if (currentRead != backupRead ||
                std::memcmp(currentBuffer.data(), backupBuffer.data(), currentRead) != 0) {
                MAKINE_LOG_DEBUG(log::FILE, "Integrity check: Content mismatch for {}", relPath);
                contentMatches = false;
                break;
            }

            if (currentFile.eof() || backupFile.eof()) break;
        }

        if (contentMatches) {
            filesMatched++;
        } else {
            filesMismatched++;
        }
    }

    bool integrityPassed = (filesMismatched == 0);

    MAKINE_LOG_INFO(log::HANDLER,
        "Integrity check completed: {} files checked, {} matched, {} mismatched (result: {})",
        filesChecked, filesMatched, filesMismatched, integrityPassed ? "PASS" : "FAIL");

    Metrics::instance().increment("integrity_checks_completed");
    Metrics::instance().recordHistogram("integrity_files_checked", filesChecked);

    if (integrityPassed) {
        MAKINE_LOG_DEBUG(log::HANDLER, "Integrity check passed for backup {}", backupId);
    } else {
        MAKINE_LOG_INFO(log::HANDLER, "Integrity check failed - game files are modified");
    }

    return integrityPassed;
}

void PatchEngine::setBackupStorage(std::unique_ptr<IBackupStorage> storage) {
    backupStorage_ = std::move(storage);
}

// =============================================================================
// BINARY TEXT PATCHER IMPLEMENTATION
// =============================================================================

bool BinaryTextPatcher::isCodeCharacter(uint8_t byte) noexcept {
    // a-z
    if (byte >= 0x61 && byte <= 0x7A) return true;
    // A-Z
    if (byte >= 0x41 && byte <= 0x5A) return true;
    // 0-9
    if (byte >= 0x30 && byte <= 0x39) return true;
    // _ (underscore)
    if (byte == 0x5F) return true;
    // . (dot - member access)
    if (byte == 0x2E) return true;
    // ( and ) (parentheses - function calls)
    if (byte == 0x28 || byte == 0x29) return true;

    return false;
}

bool BinaryTextPatcher::isCodeContext(
    const ByteBuffer& data,
    size_t position,
    size_t length
) {
    // Check byte before the string
    if (position > 0) {
        uint8_t prevChar = data[position - 1];
        if (isCodeCharacter(prevChar)) {
            return true;  // Preceded by code character = likely code
        }
    }

    // Check byte after the string
    size_t endPos = position + length;
    if (endPos < data.size()) {
        uint8_t nextChar = data[endPos];
        if (isCodeCharacter(nextChar)) {
            return true;  // Followed by code character = likely code
        }
    }

    // Safe context: surrounded by null, space, newline, quote, etc.
    return false;
}

std::vector<size_t> BinaryTextPatcher::findAllOccurrences(
    const ByteBuffer& data,
    const ByteBuffer& pattern
) {
    std::vector<size_t> occurrences;

    if (pattern.empty() || data.size() < pattern.size()) {
        return occurrences;
    }

    for (size_t i = 0; i <= data.size() - pattern.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (data[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            occurrences.push_back(i);
        }
    }

    return occurrences;
}

BinaryPatchResult BinaryTextPatcher::patchBuffer(
    ByteBuffer& data,
    const std::unordered_map<std::string, std::string>& translations,
    const BinaryPatchOptions& options
) {
    BinaryPatchResult result;
    result.success = true;

    for (const auto& [original, translated] : translations) {
        // Convert strings to byte buffers
        ByteBuffer originalBytes(original.begin(), original.end());
        ByteBuffer translatedBytes(translated.begin(), translated.end());

        // Check length constraint
        if (options.allowShorterOnly && translatedBytes.size() > originalBytes.size()) {
            result.skippedCount++;
            continue;
        }

        // Find all occurrences
        auto occurrences = findAllOccurrences(data, originalBytes);

        int patchedCount = 0;
        for (size_t pos : occurrences) {
            // Check max occurrences limit
            if (options.maxOccurrences >= 0 && patchedCount >= options.maxOccurrences) {
                break;
            }

            // Check code context
            if (options.skipCodeContext && isCodeContext(data, pos, originalBytes.size())) {
                continue;  // Skip - this is likely code, not UI text
            }

            // Apply the translation
            for (size_t j = 0; j < originalBytes.size(); ++j) {
                if (j < translatedBytes.size()) {
                    data[pos + j] = translatedBytes[j];
                } else {
                    // Pad with padding byte (default: space)
                    data[pos + j] = options.paddingByte;
                }
            }

            patchedCount++;
            result.appliedCount++;
        }
    }

    return result;
}

BinaryPatchResult BinaryTextPatcher::patchFile(
    const fs::path& filePath,
    const std::unordered_map<std::string, std::string>& translations,
    const BinaryPatchOptions& options
) {
    ScopedMetrics m("binary_patch_file");

    MAKINE_LOG_INFO(log::HANDLER, "Binary patching file {} with {} translations",
        filePath.string(), translations.size());

    BinaryPatchResult result;

    // Validate path
    if (!validation::isPathSafe(filePath)) {
        MAKINE_LOG_ERROR(log::SECURITY, "Unsafe path for binary patching: {}",
            filePath.string());
        result.success = false;
        result.error = "Unsafe file path";
        return result;
    }

    // Check file exists
    if (!fs::exists(filePath)) {
        MAKINE_LOG_ERROR(log::FILE, "Binary patch target not found: {}", filePath.string());
        result.success = false;
        result.error = "File not found: " + filePath.string();
        return result;
    }

    try {
        // Create backup if requested
        if (options.createBackup) {
            std::string backupPath = filePath.string() + ".makine_backup";
            if (!fs::exists(backupPath)) {
                MAKINE_LOG_DEBUG(log::FILE, "Creating backup before binary patch: {}", backupPath);
                fs::copy_file(filePath, backupPath);
                AuditLogger::logFileAccess(backupPath, "backup_create", true);
            }
        }

        // Read file into memory
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            MAKINE_LOG_ERROR(log::FILE, "Cannot open file for reading: {}", filePath.string());
            result.success = false;
            result.error = "Cannot open file for reading";
            return result;
        }

        inFile.seekg(0, std::ios::end);
        size_t fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        // M-5: Reject unreasonably large files to prevent OOM
        constexpr size_t kMaxPatchFileSize = 512ULL * 1024 * 1024; // 512 MB
        if (fileSize > kMaxPatchFileSize) {
            inFile.close();
            MAKINE_LOG_ERROR(log::FILE, "File too large for binary patching: {} bytes (max {})",
                fileSize, kMaxPatchFileSize);
            result.success = false;
            result.error = "File too large for binary patching (max 512 MB)";
            return result;
        }

        MAKINE_LOG_DEBUG(log::FILE, "Read {} bytes for binary patching", fileSize);

        ByteBuffer data(fileSize);
        inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
        inFile.close();

        // Apply patches
        result = patchBuffer(data, translations, options);

        // Write back atomically if any changes were made
        if (result.appliedCount > 0) {
            MAKINE_LOG_DEBUG(log::FILE, "Writing {} patched translations back to file",
                result.appliedCount);

            fs::path tempPath = filePath.string() + ".makine_tmp";

            {
                std::ofstream outFile(tempPath, std::ios::binary | std::ios::trunc);
                if (!outFile) {
                    MAKINE_LOG_ERROR(log::FILE, "Cannot create temp file for writing");
                    result.success = false;
                    result.error = "Cannot create temp file for writing";
                    return result;
                }

                outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
                outFile.flush();

                if (!outFile.good()) {
                    outFile.close();
                    std::error_code ec;
                    fs::remove(tempPath, ec);
                    MAKINE_LOG_ERROR(log::FILE, "Write failed - possible disk full");
                    result.success = false;
                    result.error = "Write failed - possible disk full";
                    return result;
                }
            }

            std::error_code ec;
            fs::rename(tempPath, filePath, ec);
            if (ec) {
                MAKINE_LOG_ERROR(log::FILE, "Rename failed: {}", ec.message());
                fs::remove(tempPath, ec);
                result.success = false;
                result.error = "Rename failed: " + ec.message();
                return result;
            }

            MAKINE_LOG_INFO(log::HANDLER, "Binary patched {} translations in {}",
                result.appliedCount, filePath.string());
            AuditLogger::logFileAccess(filePath, "binary_patch", true,
                fmt::format("{} translations applied", result.appliedCount));
        } else {
            MAKINE_LOG_DEBUG(log::HANDLER, "No translations applied to {}", filePath.string());
        }

        result.success = true;
        Metrics::instance().increment("binary_patches_applied", result.appliedCount);
        Metrics::instance().increment("binary_patches_skipped", result.skippedCount);
    }
    catch (const std::exception& e) {
        MAKINE_LOG_ERROR(log::HANDLER, "Binary patch exception: {}", e.what());
        AuditLogger::logFileAccess(filePath, "binary_patch", false, e.what());
        result.success = false;
        result.error = std::string("Patch error: ") + e.what();
    }

    return result;
}

} // namespace makine
