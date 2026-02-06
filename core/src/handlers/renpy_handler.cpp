/**
 * @file renpy_handler.cpp
 * @brief Ren'Py engine handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/renpy_handler.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace makineai {

// ============================================================================
// GAME DETECTION
// ============================================================================

bool RenpyHandler::canHandleGame(const fs::path& gameDir) {
    if (!fs::exists(gameDir)) return false;

    // Ren'Py markers
    const std::vector<std::string> markers = {
        "renpy",
        "game",
        "script.rpy"
    };

    for (const auto& marker : markers) {
        auto markerPath = gameDir / marker;
        if (fs::exists(markerPath)) {
            return true;
        }
    }

    // Check for .rpa archive files
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".rpa") {
                return true;
            }
        }
    }

    // Search for .rpy files
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".rpy") {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// FILE DISCOVERY
// ============================================================================

Result<std::vector<GameFile>> RenpyHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".rpy") {
            auto relativePath = fs::relative(entry.path(), gameDir).string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            GameFile gf;
            gf.path = entry.path();
            gf.relativePath = relativePath;
            gf.type = GameFileType::Script;
            gf.size = entry.file_size();
            gf.encoding = "utf-8";
            files.push_back(std::move(gf));
        }
    }

    spdlog::debug("Ren'Py: Found {} script files", files.size());
    return files;
}

// ============================================================================
// STRING EXTRACTION
// ============================================================================

Result<ExtractionResult> RenpyHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Ren'Py: Starting string extraction from {}", gameDir.string());
    auto timer = metrics().timer("renpy_extract_strings");

    ExtractionResult result;
    int filesProcessed = 0;

    auto filesResult = findGameFiles(gameDir);
    if (!filesResult) {
        MAKINEAI_LOG_ERROR(log::HANDLER, "Ren'Py: Failed to find game files: {}", filesResult.error().message());
        return std::unexpected(filesResult.error());
    }

    const auto& gameFiles = *filesResult;
    MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Found {} potential script files", gameFiles.size());

    for (const auto& gameFile : gameFiles) {
        // File filters
        if (!options.includeFiles.empty()) {
            bool included = false;
            for (const auto& inc : options.includeFiles) {
                if (gameFile.relativePath.find(inc) != std::string::npos) {
                    included = true;
                    break;
                }
            }
            if (!included) continue;
        }

        if (!options.excludeFiles.empty()) {
            bool excluded = false;
            for (const auto& exc : options.excludeFiles) {
                if (gameFile.relativePath.find(exc) != std::string::npos) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;
        }

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Processing file: {}", gameFile.relativePath);

        try {
            auto batch = extractFromRpyFile(gameFile.path, gameFile, options);

            result.entries.insert(result.entries.end(),
                batch.entries.begin(), batch.entries.end());
            result.totalStrings += batch.total;
            result.skippedStrings += batch.skipped;

            GameFile processedFile = gameFile;
            processedFile.stringCount = static_cast<int>(batch.entries.size());
            result.processedFiles.push_back(std::move(processedFile));
            filesProcessed++;

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Extracted {} strings from {}",
                batch.entries.size(), gameFile.relativePath);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Ren'Py: Extraction error in {}: {}",
                gameFile.relativePath, e.what());
            result.errors.push_back({
                gameFile.relativePath,
                std::string("Extraction error: ") + e.what(),
                ExtractionSeverity::Error
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("renpy_files_processed", filesProcessed);
    metrics().increment("renpy_strings_extracted", result.entries.size());

    MAKINEAI_LOG_INFO(log::HANDLER, "Ren'Py: Extraction complete - {} strings from {} files in {}ms",
        result.entries.size(), filesProcessed, result.duration.count());

    return result;
}

RenpyHandler::ExtractionBatch RenpyHandler::extractFromRpyFile(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    // Patterns
    std::regex dialoguePattern(R"(^(\s*)([a-zA-Z_][a-zA-Z0-9_]*\s+)?("[^"]*"|'[^']*')(\s*#.*)?$)");
    std::regex menuChoicePattern(R"(^(\s+)("[^"]+"|'[^']+')(\s*if\s+.+)?:)");
    std::regex characterDefPattern(R"(define\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*Character\s*\(\s*("[^"]+"|'[^']+'))");
    std::regex labelPattern(R"(^label\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*:)");
    std::regex menuStartPattern(R"(^\s*menu\s*:)");

    std::string line;
    std::string currentLabel = "";
    bool inMenu = false;
    int menuIndent = 0;
    int lineNumber = 0;

    while (std::getline(ifs, line)) {
        lineNumber++;

        // Label detection
        std::smatch labelMatch;
        if (std::regex_search(line, labelMatch, labelPattern)) {
            currentLabel = labelMatch[1].str();
            continue;
        }

        // Menu start
        if (std::regex_search(line, menuStartPattern)) {
            inMenu = true;
            menuIndent = static_cast<int>(line.find("menu"));
            continue;
        }

        // Menu end (indent decreased)
        if (inMenu) {
            auto trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));

            if (!trimmed.empty()) {
                int currentIndent = static_cast<int>(line.length() - line.size() + line.find_first_not_of(" \t"));
                if (currentIndent <= menuIndent && trimmed[0] != '"' && trimmed[0] != '\'') {
                    inMenu = false;
                }
            }
        }

        // Character definition
        std::smatch charMatch;
        if (std::regex_search(line, charMatch, characterDefPattern)) {
            auto charName = charMatch[1].str();
            auto displayName = extractQuotedString(charMatch[2].str());

            if (isValidString(displayName, options)) {
                TranslationEntry entry;
                entry.filePath = gameFile.relativePath;
                entry.entryKey = "char_" + charName;
                entry.sourceText = displayName;
                entry.context = "Character name: " + charName;
                entry.category = EntryCategory::UI;
                entry.lineNumber = lineNumber;
                batch.entries.push_back(std::move(entry));
                batch.total++;
            }
            continue;
        }

        // Menu choice
        if (inMenu) {
            std::smatch menuMatch;
            if (std::regex_search(line, menuMatch, menuChoicePattern)) {
                auto choiceText = extractQuotedString(menuMatch[2].str());

                if (isValidString(choiceText, options)) {
                    TranslationEntry entry;
                    entry.filePath = gameFile.relativePath;
                    entry.entryKey = "menu_" + (currentLabel.empty() ? "unknown" : currentLabel) +
                                     "_" + std::to_string(lineNumber);
                    entry.sourceText = choiceText;
                    entry.context = "Menu choice" + (currentLabel.empty() ? "" : " (label: " + currentLabel + ")");
                    entry.category = EntryCategory::Dialog;
                    entry.lineNumber = lineNumber;
                    batch.entries.push_back(std::move(entry));
                    batch.total++;
                }
                continue;
            }
        }

        // Dialogue/Narration
        std::smatch dialogueMatch;
        if (std::regex_search(line, dialogueMatch, dialoguePattern)) {
            auto speaker = dialogueMatch[2].str();
            // Trim speaker
            speaker.erase(0, speaker.find_first_not_of(" \t"));
            speaker.erase(speaker.find_last_not_of(" \t") + 1);

            auto textPart = dialogueMatch[3].str();
            auto text = extractQuotedString(textPart);

            if (isValidString(text, options)) {
                TranslationEntry entry;
                entry.filePath = gameFile.relativePath;
                entry.entryKey = (currentLabel.empty() ? "script" : currentLabel) +
                                 "_" + std::to_string(lineNumber);
                entry.sourceText = text;

                if (!speaker.empty()) {
                    std::string ctx = "Speaker: " + speaker;
                    if (!currentLabel.empty()) {
                        ctx += ", Label: " + currentLabel;
                    }
                    entry.context = ctx;
                    entry.category = EntryCategory::Dialog;
                } else {
                    if (!currentLabel.empty()) {
                        entry.context = "Label: " + currentLabel;
                    }
                    entry.category = EntryCategory::Other; // Narration
                }

                entry.lineNumber = lineNumber;
                batch.entries.push_back(std::move(entry));
                batch.total++;
            }
        }
    }

    return batch;
}

std::string RenpyHandler::extractQuotedString(const std::string& quoted) {
    auto text = quoted;

    // Trim
    text.erase(0, text.find_first_not_of(" \t"));
    text.erase(text.find_last_not_of(" \t") + 1);

    // Remove quotes
    if ((text.size() >= 2 && text.front() == '"' && text.back() == '"') ||
        (text.size() >= 2 && text.front() == '\'' && text.back() == '\'')) {
        text = text.substr(1, text.size() - 2);
    }

    // Unescape
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            switch (text[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case '"': result += '"'; i++; break;
                case '\'': result += '\''; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += text[i]; break;
            }
        } else {
            result += text[i];
        }
    }

    return result;
}

bool RenpyHandler::isValidString(const std::string& text, const ExtractionOptions& options) {
    auto trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty()) return false;

    if (static_cast<int>(trimmed.length()) < options.minLength) return false;
    if (static_cast<int>(trimmed.length()) > options.maxLength) return false;

    // Only placeholders/code
    if (std::regex_match(trimmed, std::regex(R"(^[\[\]{}\$%\d\s\.\,\-\_]+$)"))) {
        return false;
    }

    return true;
}

// ============================================================================
// TRANSLATION APPLICATION
// ============================================================================

Result<HandlerPatchResult> RenpyHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Ren'Py: Starting translation application to {}", gameDir.string());
    auto timer = metrics().timer("renpy_apply_translations");

    HandlerPatchResult result;
    result.success = true;
    int filesPatched = 0;

    // Create backup
    if (options.createBackup) {
        auto backupId = options.backupId.empty()
            ? std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
            : options.backupId;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Creating backup with ID: {}", backupId);
        auto backupResult = createBackup(gameDir, backupId);
        if (!backupResult) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Ren'Py: Backup creation failed");
            return std::unexpected(Error{ErrorCode::IOError, "Backup creation failed"});
        }
        result.backupId = backupId;
    }

    // Group translations by file
    std::unordered_map<std::string, std::vector<TranslationEntry>> translationsByFile;
    for (const auto& entry : translations) {
        if (!entry.targetText || entry.targetText->empty()) continue;
        translationsByFile[entry.filePath].push_back(entry);
    }

    MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: {} translations grouped into {} files",
        translations.size(), translationsByFile.size());

    // Apply to each file
    for (const auto& [filePath, fileTranslations] : translationsByFile) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Patching file: {} ({} translations)",
            filePath, fileTranslations.size());

        auto fullPath = gameDir / filePath;
        if (!fs::exists(fullPath)) {
            MAKINEAI_LOG_WARN(log::HANDLER, "Ren'Py: File not found: {}", filePath);
            result.errors.push_back({filePath, "File not found", ExtractionSeverity::Warning});
            continue;
        }

        try {
            std::ifstream ifs(fullPath);
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(ifs, line)) {
                lines.push_back(line);
            }
            ifs.close();

            int changedCount = 0;

            for (const auto& translation : fileTranslations) {
                if (!translation.lineNumber || *translation.lineNumber == 0) continue;

                int lineIndex = *translation.lineNumber - 1;
                if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size())) {
                    result.skippedCount++;
                    continue;
                }

                auto newLine = replaceStringInLine(
                    lines[lineIndex],
                    translation.sourceText,
                    *translation.targetText
                );

                if (newLine && *newLine != lines[lineIndex]) {
                    lines[lineIndex] = *newLine;
                    changedCount++;
                    result.appliedCount++;
                } else {
                    result.skippedCount++;
                }
            }

            // Write back if changed
            if (changedCount > 0 && !options.dryRun) {
                std::ofstream ofs(fullPath);
                for (size_t i = 0; i < lines.size(); i++) {
                    ofs << lines[i];
                    if (i < lines.size() - 1) ofs << "\n";
                }

                // Audit log for file modification
                AuditLogger::logFileAccess(fullPath, "patch",
                    true, "Applied " + std::to_string(changedCount) + " translations");
            }

            result.patchedFiles.push_back({
                filePath,
                changedCount,
                static_cast<int>(fileTranslations.size()),
                !result.backupId.empty()
            });
            filesPatched++;

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Ren'Py: Patched {} - {} applied",
                filePath, changedCount);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Ren'Py: Patch error in {}: {}", filePath, e.what());
            result.errors.push_back({
                filePath,
                std::string("Patch error: ") + e.what(),
                ExtractionSeverity::Error
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("renpy_files_patched", filesPatched);
    metrics().increment("renpy_translations_applied", result.appliedCount);

    MAKINEAI_LOG_INFO(log::HANDLER, "Ren'Py: Translation complete - {} applied, {} skipped in {} files ({}ms)",
        result.appliedCount, result.skippedCount, filesPatched, result.duration.count());

    return result;
}

std::optional<std::string> RenpyHandler::replaceStringInLine(
    const std::string& line,
    const std::string& source,
    const std::string& target
) {
    // Try double quote version
    auto escapedSourceDouble = escapeString(source, '"');
    auto escapedTargetDouble = escapeString(target, '"');

    std::string searchDouble = "\"" + escapedSourceDouble + "\"";
    std::string replaceDouble = "\"" + escapedTargetDouble + "\"";

    auto pos = line.find(searchDouble);
    if (pos != std::string::npos) {
        std::string newLine = line;
        newLine.replace(pos, searchDouble.length(), replaceDouble);
        return newLine;
    }

    // Try single quote version
    auto escapedSourceSingle = escapeString(source, '\'');
    auto escapedTargetSingle = escapeString(target, '\'');

    std::string searchSingle = "'" + escapedSourceSingle + "'";
    std::string replaceSingle = "'" + escapedTargetSingle + "'";

    pos = line.find(searchSingle);
    if (pos != std::string::npos) {
        std::string newLine = line;
        newLine.replace(pos, searchSingle.length(), replaceSingle);
        return newLine;
    }

    return std::nullopt;
}

std::string RenpyHandler::escapeString(const std::string& text, char quote) {
    std::string result;
    result.reserve(text.size() * 1.2);

    for (char c : text) {
        if (c == '\\') {
            result += "\\\\";
        } else if (c == quote) {
            result += "\\";
            result += quote;
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            result += c;
        }
    }

    return result;
}

// ============================================================================
// BACKUP/RESTORE
// ============================================================================

Result<HandlerBackupResult> RenpyHandler::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& specificFiles
) {
    HandlerBackupResult result;
    result.backupId = backupId;

    auto backupDir = gameDir / "_makineai_backups" / backupId;

    try {
        fs::create_directories(backupDir);
        result.backupPath = backupDir.string();

        auto filesResult = findGameFiles(gameDir);
        if (!filesResult) {
            result.success = false;
            result.errorMessage = "Failed to find game files";
            return result;
        }

        for (const auto& gameFile : *filesResult) {
            if (!specificFiles.empty()) {
                bool found = false;
                for (const auto& f : specificFiles) {
                    if (gameFile.relativePath.find(f) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }

            auto backupFile = backupDir / gameFile.relativePath;
            fs::create_directories(backupFile.parent_path());
            fs::copy_file(gameFile.path, backupFile, fs::copy_options::overwrite_existing);

            result.totalSize += gameFile.size;
            result.backedUpFiles.push_back(gameFile.relativePath);
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

Result<HandlerRestoreResult> RenpyHandler::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    HandlerRestoreResult result;

    auto backupDir = gameDir / "_makineai_backups" / backupId;
    if (!fs::exists(backupDir)) {
        result.success = false;
        result.errorMessage = "Backup not found: " + backupId;
        return result;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(backupDir)) {
            if (!entry.is_regular_file()) continue;

            auto relativePath = fs::relative(entry.path(), backupDir);
            auto targetPath = gameDir / relativePath;

            fs::create_directories(targetPath.parent_path());
            fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing);

            result.restoredFiles.push_back(relativePath.string());
        }

        fs::remove_all(backupDir);
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

// ============================================================================
// VALIDATION
// ============================================================================

Result<ValidationResult> RenpyHandler::validatePatch(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations
) {
    ValidationResult result;
    result.isValid = true;

    auto filesResult = findGameFiles(gameDir);
    if (!filesResult) {
        result.isValid = false;
        return result;
    }

    const auto& gameFiles = *filesResult;

    for (const auto& translation : translations) {
        if (!translation.targetText) continue;
        result.checkedCount++;

        // Check if file exists
        bool fileExists = false;
        for (const auto& gf : gameFiles) {
            if (gf.relativePath == translation.filePath) {
                fileExists = true;
                break;
            }
        }

        if (!fileExists) {
            result.failedCount++;
            result.issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Source file not found",
                .severity = ValidationSeverity::Error
            });
            continue;
        }

        result.passedCount++;
    }

    result.isValid = (result.failedCount == 0);
    return result;
}

} // namespace makineai
