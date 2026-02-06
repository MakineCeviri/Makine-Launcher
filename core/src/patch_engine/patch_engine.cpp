/**
 * @file patch_engine.cpp
 * @brief Patch engine implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/patch_engine.hpp"
#include "makineai/core.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>

namespace makineai {

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
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot create backup directory: " + ec.message()));
    }

    // Copy files
    for (const auto& relPath : filesToBackup) {
        fs::path srcPath = gameDir / relPath;
        fs::path dstPath = metadata.backupPath / relPath;

        if (!fs::exists(srcPath)) {
            continue;
        }

        // Create parent directories
        fs::create_directories(dstPath.parent_path(), ec);
        if (ec) {
            logger()->warn("Cannot create directory for: {}", relPath);
            continue;
        }

        // Copy file
        fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            logger()->warn("Cannot backup file: {} - {}", relPath, ec.message());
            continue;
        }

        metadata.files.push_back(relPath);
        metadata.fileCount++;
        metadata.sizeBytes += fs::file_size(dstPath, ec);
    }

    // Save metadata
    json metaJson;
    metaJson["backupId"] = metadata.backupId;
    metaJson["gameId"] = metadata.gameId;
    metaJson["gameName"] = metadata.gameName;
    metaJson["patchVersion"] = metadata.patchVersion;
    metaJson["createdAt"] = metadata.createdAt;
    metaJson["sizeBytes"] = metadata.sizeBytes;
    metaJson["fileCount"] = metadata.fileCount;
    metaJson["files"] = metadata.files;

    std::ofstream metaFile(metadata.backupPath / kMetadataFile);
    if (!metaFile) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot write backup metadata"));
    }
    metaFile << metaJson.dump(2);

    logger()->info("Created backup {} with {} files ({} bytes)",
        backupId, metadata.fileCount, metadata.sizeBytes);

    return metadata;
}

VoidResult FileBackupStorage::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    auto metaResult = getBackup(backupId);
    if (!metaResult) {
        return std::unexpected(metaResult.error());
    }

    const auto& metadata = *metaResult;
    uint32_t restored = 0;
    uint32_t failed = 0;

    for (const auto& relPath : metadata.files) {
        fs::path srcPath = metadata.backupPath / relPath;
        fs::path dstPath = gameDir / relPath;

        if (!fs::exists(srcPath)) {
            logger()->warn("Backup file missing: {}", relPath);
            failed++;
            continue;
        }

        std::error_code ec;
        fs::create_directories(dstPath.parent_path(), ec);
        fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing, ec);

        if (ec) {
            logger()->warn("Cannot restore file: {} - {}", relPath, ec.message());
            failed++;
        } else {
            restored++;
        }
    }

    logger()->info("Restored {} files from backup {} ({} failed)",
        restored, backupId, failed);

    if (failed > 0 && restored == 0) {
        return std::unexpected(Error(ErrorCode::RestoreFailed,
            "Failed to restore any files from backup"));
    }

    return {};
}

VoidResult FileBackupStorage::deleteBackup(const std::string& backupId) {
    fs::path backupPath = baseDir_ / backupId;

    if (!fs::exists(backupPath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Backup not found: " + backupId));
    }

    std::error_code ec;
    fs::remove_all(backupPath, ec);

    if (ec) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot delete backup: " + ec.message()));
    }

    logger()->info("Deleted backup: {}", backupId);
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
            "Invalid backup metadata: " + std::string(e.what())));
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

Result<PatchResult> PatchEngine::apply(
    const std::vector<PatchOperation>& operations,
    const GameInfo& game,
    const std::string& patchVersion,
    ProgressCallback progress,
    CancellationToken* cancel
) {
    PatchResult result;
    result.success = false;
    result.filesPatched = 0;
    result.filesFailed = 0;

    if (operations.empty()) {
        result.success = true;
        result.message = "No operations to apply";
        return result;
    }

    // Create backup ID
    std::string backupId = game.id.storeId + "_" + patchVersion;

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

    // Create backup
    if (!filesToBackup.empty()) {
        if (progress) {
            progress(0, static_cast<uint32_t>(operations.size()), "Creating backup...");
        }

        auto backupResult = backupStorage_->createBackup(
            game.installPath, filesToBackup, backupId);

        if (!backupResult) {
            result.message = "Backup failed: " + backupResult.error().message();
            return result;
        }

        result.backupPath = backupResult->backupPath;
    }

    // Apply operations
    size_t completedOps = 0;
    for (const auto& op : operations) {
        if (cancel && cancel->isCancelled()) {
            // Rollback
            auto rollbackResult = rollbackOperations(operations, completedOps);
            if (!rollbackResult) {
                logger()->error("Rollback failed: {}", rollbackResult.error().message());
            }
            result.message = "Operation cancelled, changes rolled back";
            return result;
        }

        if (progress) {
            progress(
                static_cast<uint32_t>(completedOps),
                static_cast<uint32_t>(operations.size()),
                "Patching: " + op.target.filename().string()
            );
        }

        auto opResult = executeOperation(op);
        if (!opResult) {
            result.filesFailed++;
            result.errors.push_back(op.target.string() + ": " +
                opResult.error().message());
            logger()->warn("Patch operation failed: {} - {}",
                op.target.string(), opResult.error().message());
        } else {
            result.filesPatched++;
        }

        completedOps++;
    }

    result.success = (result.filesFailed == 0);
    result.message = "Patched " + std::to_string(result.filesPatched) +
        " files, " + std::to_string(result.filesFailed) + " failed";

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
    std::error_code ec;

    switch (op.type) {
        case PatchOperation::Type::Copy:
            if (!fs::exists(op.source)) {
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Source file not found"));
            }
            fs::create_directories(op.target.parent_path(), ec);
            fs::copy_file(op.source, op.target,
                fs::copy_options::overwrite_existing, ec);
            if (ec) {
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Copy failed: " + ec.message()));
            }
            break;

        case PatchOperation::Type::Replace:
            fs::create_directories(op.target.parent_path(), ec);
            {
                std::ofstream file(op.target, std::ios::binary);
                if (!file) {
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot write file"));
                }
                file.write(reinterpret_cast<const char*>(op.data.data()),
                    op.data.size());
            }
            break;

        case PatchOperation::Type::Modify:
            {
                std::fstream file(op.target, std::ios::in | std::ios::out |
                    std::ios::binary);
                if (!file) {
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot open file for modification"));
                }
                file.seekp(op.offset);
                file.write(reinterpret_cast<const char*>(op.data.data()),
                    op.data.size());
            }
            break;

        case PatchOperation::Type::Delete:
            if (fs::exists(op.target)) {
                fs::remove(op.target, ec);
                if (ec) {
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Delete failed: " + ec.message()));
                }
            }
            break;

        case PatchOperation::Type::CreateDir:
            fs::create_directories(op.target, ec);
            if (ec) {
                return std::unexpected(Error(ErrorCode::FileAccessDenied,
                    "Cannot create directory: " + ec.message()));
            }
            break;
    }

    return {};
}

VoidResult PatchEngine::rollbackOperations(
    const std::vector<PatchOperation>& /*ops*/,
    size_t /*completedCount*/
) {
    // TODO: Implement rollback using backup
    logger()->warn("Rollback not fully implemented yet");
    return {};
}

Result<BackupResult> PatchEngine::backup(
    const fs::path& gameDir,
    const StringList& files,
    const std::string& backupId
) {
    auto result = backupStorage_->createBackup(gameDir, files, backupId);
    if (!result) {
        return std::unexpected(result.error());
    }

    BackupResult br;
    br.success = true;
    br.message = "Backup created";
    br.backupPath = result->backupPath;
    br.sizeBytes = result->sizeBytes;
    br.fileCount = result->fileCount;
    return br;
}

Result<RestoreResult> PatchEngine::restore(
    const fs::path& gameDir,
    const std::string& backupId
) {
    auto result = backupStorage_->restoreBackup(gameDir, backupId);
    if (!result) {
        return std::unexpected(result.error());
    }

    RestoreResult rr;
    rr.success = true;
    rr.message = "Restore completed";

    auto meta = backupStorage_->getBackup(backupId);
    if (meta) {
        rr.filesRestored = meta->fileCount;
    }

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
    const fs::path& /*gameDir*/,
    const std::string& /*backupId*/
) const {
    // TODO: Implement integrity verification
    return true;
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
    BinaryPatchResult result;

    // Check file exists
    if (!fs::exists(filePath)) {
        result.success = false;
        result.error = "File not found: " + filePath.string();
        return result;
    }

    try {
        // Create backup if requested
        if (options.createBackup) {
            std::string backupPath = filePath.string() + ".makineai_backup";
            if (!fs::exists(backupPath)) {
                fs::copy_file(filePath, backupPath);
            }
        }

        // Read file into memory
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            result.success = false;
            result.error = "Cannot open file for reading";
            return result;
        }

        inFile.seekg(0, std::ios::end);
        size_t fileSize = inFile.tellg();
        inFile.seekg(0, std::ios::beg);

        ByteBuffer data(fileSize);
        inFile.read(reinterpret_cast<char*>(data.data()), fileSize);
        inFile.close();

        // Apply patches
        result = patchBuffer(data, translations, options);

        // Write back if any changes were made
        if (result.appliedCount > 0) {
            std::ofstream outFile(filePath, std::ios::binary);
            if (!outFile) {
                result.success = false;
                result.error = "Cannot open file for writing";
                return result;
            }

            outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
            outFile.close();

            logger()->info("Patched {} translations in {}", result.appliedCount, filePath.string());
        }

        result.success = true;
    }
    catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Patch error: ") + e.what();
    }

    return result;
}

} // namespace makineai
