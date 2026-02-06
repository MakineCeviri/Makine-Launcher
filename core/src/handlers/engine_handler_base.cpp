/**
 * @file engine_handler_base.cpp
 * @brief EngineHandlerBase implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/engine_handler.hpp"
#include "makineai/logging.hpp"

#include <fstream>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

namespace makineai {

// ============================================================================
// LOGGING IMPLEMENTATION
// ============================================================================

void EngineHandlerBase::logDebug(std::string_view message) const {
    MAKINEAI_LOG_DEBUG(log::HANDLER, "[{}] {}", handlerName_, message);
}

void EngineHandlerBase::logInfo(std::string_view message) const {
    MAKINEAI_LOG_INFO(log::HANDLER, "[{}] {}", handlerName_, message);
}

void EngineHandlerBase::logWarning(std::string_view message) const {
    MAKINEAI_LOG_WARN(log::HANDLER, "[{}] {}", handlerName_, message);
}

void EngineHandlerBase::logError(std::string_view message) const {
    MAKINEAI_LOG_ERROR(log::HANDLER, "[{}] {}", handlerName_, message);
}

// ============================================================================
// FILE VALIDATION
// ============================================================================

std::optional<Error> EngineHandlerBase::validateFileReadable(const fs::path& path) const {
    if (!fs::exists(path)) {
        return Error(ErrorCode::FileNotFound, "File not found")
            .withFile(path.string());
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Error(ErrorCode::FileAccessDenied, "Cannot open file for reading")
            .withFile(path.string());
    }

    return std::nullopt;
}

std::optional<Error> EngineHandlerBase::validateFileWritable(const fs::path& path) const {
    // Check if parent directory exists
    auto parent = path.parent_path();
    if (!fs::exists(parent)) {
        return Error(ErrorCode::InvalidPath, "Parent directory does not exist")
            .withFile(path.string());
    }

    // Check if we can write to the directory
    auto testFile = parent / ".makineai_write_test";
    std::ofstream test(testFile, std::ios::binary);
    if (!test.is_open()) {
        return Error(ErrorCode::FileWriteFailed, "Directory is not writable")
            .withFile(parent.string());
    }
    test.close();
    fs::remove(testFile);

    // If file exists, check if it's writable
    if (fs::exists(path)) {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            return Error(ErrorCode::FileWriteFailed, "File is not writable")
                .withFile(path.string());
        }
    }

    return std::nullopt;
}

std::optional<Error> EngineHandlerBase::validatePathSafe(
    const fs::path& path,
    const fs::path& gameDir
) const {
    std::error_code ec;

    // Normalize both paths
    auto normalizedPath = fs::weakly_canonical(path, ec);
    if (ec) {
        return Error(ErrorCode::InvalidPath, "Cannot normalize path")
            .withFile(path.string())
            .withDetail("error", ec.message());
    }

    auto normalizedGameDir = fs::weakly_canonical(gameDir, ec);
    if (ec) {
        return Error(ErrorCode::InvalidPath, "Cannot normalize game directory")
            .withFile(gameDir.string())
            .withDetail("error", ec.message());
    }

    // Check if path starts with game directory
    auto [gameEnd, pathIt] = std::mismatch(
        normalizedGameDir.begin(), normalizedGameDir.end(),
        normalizedPath.begin(), normalizedPath.end()
    );

    if (gameEnd != normalizedGameDir.end()) {
        return Error(ErrorCode::SecurityViolation, "Path escapes game directory")
            .withFile(path.string())
            .withGame(gameDir.filename().string())
            .withDetail("reason", "Path traversal attempt blocked");
    }

    // Check for suspicious components
    for (const auto& component : path) {
        auto str = component.string();
        if (str == ".." || str.find("..") != std::string::npos) {
            return Error(ErrorCode::SecurityViolation, "Path contains '..'")
                .withFile(path.string());
        }
    }

    return std::nullopt;
}

bool EngineHandlerBase::isFileLocked(const fs::path& path) const {
    if (!fs::exists(path)) {
        return false;
    }

#ifdef _WIN32
    // Try to open file with exclusive access
    HANDLE hFile = CreateFileW(
        path.wstring().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,  // No sharing
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_SHARING_VIOLATION;
    }

    CloseHandle(hFile);
    return false;
#else
    int fd = open(path.c_str(), O_RDWR);
    if (fd == -1) {
        return errno == EACCES || errno == EAGAIN;
    }

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    bool locked = fcntl(fd, F_GETLK, &fl) == 0 && fl.l_type != F_UNLCK;
    close(fd);
    return locked;
#endif
}

// ============================================================================
// ATOMIC FILE OPERATIONS
// ============================================================================

std::optional<Error> EngineHandlerBase::writeFileAtomic(
    const fs::path& path,
    const ByteBuffer& content
) const {
    // Validate path safety
    if (auto err = validateFileWritable(path)) {
        return err;
    }

    // Create temp file
    auto tempPath = path;
    tempPath += ".makineai_tmp";

    // Write to temp file
    {
        std::ofstream file(tempPath, std::ios::binary);
        if (!file.is_open()) {
            return Error(ErrorCode::FileWriteFailed, "Cannot create temp file")
                .withFile(tempPath.string());
        }

        file.write(reinterpret_cast<const char*>(content.data()), content.size());
        file.flush();

        if (!file.good()) {
            fs::remove(tempPath);
            return Error(ErrorCode::FileWriteFailed, "Write failed")
                .withFile(tempPath.string());
        }
    }

    // Rename temp to target (atomic on most filesystems)
    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec) {
        fs::remove(tempPath);
        return Error(ErrorCode::FileWriteFailed, "Cannot rename temp file")
            .withFile(path.string())
            .withDetail("error", ec.message());
    }

    return std::nullopt;
}

std::optional<Error> EngineHandlerBase::writeFileAtomic(
    const fs::path& path,
    std::string_view content
) const {
    ByteBuffer buffer(content.begin(), content.end());
    return writeFileAtomic(path, buffer);
}

// ============================================================================
// PLACEHOLDER UTILITIES
// ============================================================================

std::vector<PlaceholderInfo> EngineHandlerBase::extractPlaceholders(
    const std::string& text
) const {
    std::vector<PlaceholderInfo> placeholders;

    // Printf-style: %s, %d, %f, %02d, etc.
    static const std::regex printfRegex(R"(%[-+0 #]*\d*\.?\d*[hlL]?[diouxXeEfFgGaAcspn%])");
    for (std::sregex_iterator it(text.begin(), text.end(), printfRegex); it != std::sregex_iterator{}; ++it) {
        PlaceholderInfo info;
        info.type = PlaceholderType::Printf;
        info.original = it->str();
        info.startIndex = static_cast<size_t>(it->position());
        info.endIndex = info.startIndex + it->length();
        placeholders.push_back(std::move(info));
    }

    // Named: {name}, ${var}
    static const std::regex namedRegex(R"(\$?\{(\w+)\})");
    for (std::sregex_iterator it(text.begin(), text.end(), namedRegex); it != std::sregex_iterator{}; ++it) {
        PlaceholderInfo info;
        info.type = PlaceholderType::Named;
        info.original = it->str();
        info.startIndex = static_cast<size_t>(it->position());
        info.endIndex = info.startIndex + it->length();
        info.variableName = (*it)[1].str();
        placeholders.push_back(std::move(info));
    }

    // Indexed: {0}, {1}
    static const std::regex indexedRegex(R"(\{(\d+)\})");
    for (std::sregex_iterator it(text.begin(), text.end(), indexedRegex); it != std::sregex_iterator{}; ++it) {
        PlaceholderInfo info;
        info.type = PlaceholderType::Indexed;
        info.original = it->str();
        info.startIndex = static_cast<size_t>(it->position());
        info.endIndex = info.startIndex + it->length();
        info.variableName = (*it)[1].str();
        placeholders.push_back(std::move(info));
    }

    // Unity/BBCode: <color=#fff>, [b], [/b]
    static const std::regex tagRegex(R"(</?[a-zA-Z][^>]*>|\[[/a-zA-Z][^\]]*\])");
    for (std::sregex_iterator it(text.begin(), text.end(), tagRegex); it != std::sregex_iterator{}; ++it) {
        PlaceholderInfo info;
        info.original = it->str();
        info.type = info.original[0] == '<' ? PlaceholderType::Unity : PlaceholderType::BBCode;
        info.startIndex = static_cast<size_t>(it->position());
        info.endIndex = info.startIndex + it->length();
        placeholders.push_back(std::move(info));
    }

    // Sort by position
    std::sort(placeholders.begin(), placeholders.end(),
        [](const auto& a, const auto& b) { return a.startIndex < b.startIndex; });

    return placeholders;
}

std::vector<ValidationIssue> EngineHandlerBase::validatePlaceholders(
    const std::string& source,
    const std::string& target
) const {
    std::vector<ValidationIssue> issues;

    auto sourcePH = extractPlaceholders(source);
    auto targetPH = extractPlaceholders(target);

    // Check count mismatch
    if (sourcePH.size() != targetPH.size()) {
        ValidationIssue issue;
        issue.message = "Placeholder count mismatch: source has " +
                        std::to_string(sourcePH.size()) + ", target has " +
                        std::to_string(targetPH.size());
        issue.severity = ValidationSeverity::Error;
        issues.push_back(std::move(issue));
        return issues;
    }

    // Build maps for comparison
    std::map<std::string, int> sourceMap, targetMap;
    for (const auto& ph : sourcePH) {
        sourceMap[ph.original]++;
    }
    for (const auto& ph : targetPH) {
        targetMap[ph.original]++;
    }

    // Check for missing/extra placeholders
    for (const auto& [placeholder, count] : sourceMap) {
        auto it = targetMap.find(placeholder);
        if (it == targetMap.end()) {
            ValidationIssue issue;
            issue.message = "Missing placeholder in target: " + placeholder;
            issue.severity = ValidationSeverity::Error;
            issues.push_back(std::move(issue));
        } else if (it->second != count) {
            ValidationIssue issue;
            issue.message = "Placeholder count mismatch for '" + placeholder + "': " +
                           std::to_string(count) + " in source, " +
                           std::to_string(it->second) + " in target";
            issue.severity = ValidationSeverity::Warning;
            issues.push_back(std::move(issue));
        }
    }

    for (const auto& [placeholder, count] : targetMap) {
        if (sourceMap.find(placeholder) == sourceMap.end()) {
            ValidationIssue issue;
            issue.message = "Extra placeholder in target: " + placeholder;
            issue.severity = ValidationSeverity::Warning;
            issues.push_back(std::move(issue));
        }
    }

    return issues;
}

// ============================================================================
// BACKUP MANAGEMENT
// ============================================================================

fs::path EngineHandlerBase::getBackupPath(
    const fs::path& gameDir,
    const std::string& backupId
) const {
    // Default: store backups in user's data directory
    // This can be overridden by derived classes
#ifdef _WIN32
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        fs::path backupRoot = fs::path(localAppData) / "MakineAI" / "backups";
        CoTaskMemFree(localAppData);

        // Use game directory name + hash for uniqueness
        auto gameName = gameDir.filename().string();
        auto gameHash = std::to_string(std::hash<std::string>{}(gameDir.string())).substr(0, 8);
        return backupRoot / (gameName + "_" + gameHash) / backupId;
    }
#endif

    // Fallback: backup directory next to game
    return gameDir.parent_path() / "MakineAI_Backups" / backupId;
}

std::vector<std::string> EngineHandlerBase::listBackups(const fs::path& gameDir) const {
    std::vector<std::string> backups;

    auto backupRoot = getBackupPath(gameDir, "").parent_path();
    if (!fs::exists(backupRoot)) {
        return backups;
    }

    for (const auto& entry : fs::directory_iterator(backupRoot)) {
        if (entry.is_directory()) {
            backups.push_back(entry.path().filename().string());
        }
    }

    std::sort(backups.begin(), backups.end());
    return backups;
}

Result<void> EngineHandlerBase::copyFileToBackup(
    const fs::path& source,
    const fs::path& backupDir,
    const fs::path& gameDir
) {
    // Get relative path
    auto relativePath = fs::relative(source, gameDir);
    auto destPath = backupDir / relativePath;

    // Create parent directories
    std::error_code ec;
    fs::create_directories(destPath.parent_path(), ec);
    if (ec) {
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Cannot create backup directory")
            .withFile(destPath.parent_path().string()));
    }

    // Copy file
    fs::copy_file(source, destPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Cannot copy file to backup")
            .withFile(source.string())
            .withDetail("error", ec.message()));
    }

    return {};
}

Result<void> EngineHandlerBase::restoreFileFromBackup(
    const fs::path& backupFile,
    const fs::path& gameDir
) {
    // Validate the backup file exists
    if (!fs::exists(backupFile)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Backup file not found")
            .withFile(backupFile.string()));
    }

    // Read backup
    std::ifstream input(backupFile, std::ios::binary);
    if (!input.is_open()) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot read backup file")
            .withFile(backupFile.string()));
    }

    ByteBuffer content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());

    // Write atomically
    if (auto err = writeFileAtomic(gameDir, content)) {
        return std::unexpected(*err);
    }

    return {};
}

// ============================================================================
// DEFAULT BACKUP/RESTORE IMPLEMENTATION
// ============================================================================

Result<HandlerBackupResult> EngineHandlerBase::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& specificFiles
) {
    MAKINEAI_TIMED_SCOPE(log::HANDLER, "createBackup");

    HandlerBackupResult result;
    result.backupId = backupId;

    // Determine files to backup
    std::vector<fs::path> filesToBackup;

    if (!specificFiles.empty()) {
        // Use specified files
        for (const auto& file : specificFiles) {
            auto path = gameDir / file;
            if (fs::exists(path)) {
                filesToBackup.push_back(path);
            }
        }
    } else {
        // Find all translatable files
        auto filesResult = findGameFiles(gameDir);
        if (!filesResult) {
            result.success = false;
            result.errorMessage = filesResult.error().message();
            return result;
        }

        for (const auto& gf : *filesResult) {
            filesToBackup.push_back(gf.path);
        }
    }

    if (filesToBackup.empty()) {
        result.success = false;
        result.errorMessage = "No files to backup";
        return result;
    }

    // Create backup directory
    auto backupDir = getBackupPath(gameDir, backupId);
    result.backupPath = backupDir.string();

    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec) {
        result.success = false;
        result.errorMessage = "Cannot create backup directory: " + ec.message();
        return result;
    }

    // Copy files
    logInfo("Creating backup: " + backupId);
    for (const auto& file : filesToBackup) {
        auto copyResult = copyFileToBackup(file, backupDir, gameDir);
        if (!copyResult) {
            logWarning("Failed to backup: " + file.string());
            continue;
        }

        result.backedUpFiles.push_back(fs::relative(file, gameDir).string());
        result.totalSize += fs::file_size(file, ec);
    }

    // Write manifest
    {
        std::ofstream manifest(backupDir / "manifest.txt");
        manifest << "# MakineAI Backup Manifest\n";
        manifest << "# Created: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
        manifest << "# Game: " << gameDir.filename().string() << "\n\n";
        for (const auto& file : result.backedUpFiles) {
            manifest << file << "\n";
        }
    }

    result.success = !result.backedUpFiles.empty();
    logInfo("Backup complete: " + std::to_string(result.backedUpFiles.size()) + " files");

    return result;
}

Result<HandlerRestoreResult> EngineHandlerBase::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    MAKINEAI_TIMED_SCOPE(log::HANDLER, "restoreBackup");

    HandlerRestoreResult result;

    auto backupDir = getBackupPath(gameDir, backupId);
    if (!fs::exists(backupDir)) {
        result.success = false;
        result.errorMessage = "Backup not found: " + backupId;
        return result;
    }

    // Read manifest
    std::vector<std::string> filesToRestore;
    auto manifestPath = backupDir / "manifest.txt";
    if (fs::exists(manifestPath)) {
        std::ifstream manifest(manifestPath);
        std::string line;
        while (std::getline(manifest, line)) {
            if (line.empty() || line[0] == '#') continue;
            filesToRestore.push_back(line);
        }
    } else {
        // No manifest - restore all files
        for (const auto& entry : fs::recursive_directory_iterator(backupDir)) {
            if (entry.is_regular_file()) {
                auto rel = fs::relative(entry.path(), backupDir);
                filesToRestore.push_back(rel.string());
            }
        }
    }

    logInfo("Restoring backup: " + backupId);

    // Restore files
    for (const auto& relPath : filesToRestore) {
        auto backupFile = backupDir / relPath;
        auto targetFile = gameDir / relPath;

        // Validate path safety
        if (auto err = validatePathSafe(targetFile, gameDir)) {
            logWarning("Skipping unsafe path: " + relPath);
            continue;
        }

        // Create parent directories
        std::error_code ec;
        fs::create_directories(targetFile.parent_path(), ec);

        // Read backup content
        std::ifstream input(backupFile, std::ios::binary);
        if (!input.is_open()) {
            logWarning("Cannot read backup file: " + relPath);
            continue;
        }

        ByteBuffer content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());

        // Write atomically
        if (auto err = writeFileAtomic(targetFile, content)) {
            logWarning("Failed to restore: " + relPath);
            continue;
        }

        result.restoredFiles.push_back(relPath);
    }

    result.success = !result.restoredFiles.empty();
    logInfo("Restore complete: " + std::to_string(result.restoredFiles.size()) + " files");

    return result;
}

// ============================================================================
// DEFAULT VALIDATION IMPLEMENTATION
// ============================================================================

Result<ValidationResult> EngineHandlerBase::validatePatch(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations
) {
    MAKINEAI_TIMED_SCOPE(log::HANDLER, "validatePatch");

    ValidationResult result;
    result.checkedCount = static_cast<int>(translations.size());

    for (const auto& entry : translations) {
        // Skip untranslated entries
        if (!entry.targetText.has_value() || entry.targetText->empty()) {
            result.passedCount++;
            continue;
        }

        // Validate placeholders
        auto placeholderIssues = validatePlaceholders(entry.sourceText, *entry.targetText);
        for (auto& issue : placeholderIssues) {
            issue.entryKey = entry.entryKey.value_or(entry.filePath);
            issue.file = entry.filePath;
            result.issues.push_back(std::move(issue));

            if (issue.severity == ValidationSeverity::Error) {
                result.failedCount++;
                result.isValid = false;
            }
        }

        // Check string length (warn if translation is much longer)
        if (entry.targetText->length() > entry.sourceText.length() * 2 &&
            entry.sourceText.length() > 10) {
            ValidationIssue issue;
            issue.file = entry.filePath;
            issue.entryKey = entry.entryKey.value_or("");
            issue.message = "Translation is more than 2x longer than source";
            issue.severity = ValidationSeverity::Warning;
            result.issues.push_back(std::move(issue));
        }

        if (placeholderIssues.empty()) {
            result.passedCount++;
        }
    }

    return result;
}

} // namespace makineai
