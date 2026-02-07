/**
 * @file godot_handler.cpp
 * @brief Godot Engine handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/godot_handler.hpp"
#include "makineai/logging.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace makineai {

// ============================================================================
// PCK PARSER
// ============================================================================

bool GodotPckParser::isPckFile(const fs::path& file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return false;

    uint32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return magic == GODOT_PCK_MAGIC;
}

GodotPckResult GodotPckParser::parse(const fs::path& pckFile) {
    GodotPckResult result;

    std::ifstream ifs(pckFile, std::ios::binary);
    if (!ifs) {
        result.error = "Cannot open PCK file: " + pckFile.string();
        return result;
    }

    // Read magic
    uint32_t magic = 0;
    ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != GODOT_PCK_MAGIC) {
        result.error = "Invalid PCK magic number";
        return result;
    }

    // Read header
    ifs.read(reinterpret_cast<char*>(&result.header.formatVersion), 4);
    ifs.read(reinterpret_cast<char*>(&result.header.majorVersion), 4);
    ifs.read(reinterpret_cast<char*>(&result.header.minorVersion), 4);
    ifs.read(reinterpret_cast<char*>(&result.header.patchVersion), 4);

    // Godot 4.x PCK v2 has extra flags field
    if (result.header.formatVersion >= 2) {
        uint32_t flags = 0;
        ifs.read(reinterpret_cast<char*>(&flags), 4);
        // Skip 64 bytes reserved
        ifs.seekg(64, std::ios::cur);
    } else {
        // v1: skip 16 reserved bytes
        ifs.seekg(16, std::ios::cur);
    }

    // File count
    uint32_t fileCount = 0;
    ifs.read(reinterpret_cast<char*>(&fileCount), 4);
    result.header.fileCount = fileCount;

    if (fileCount > 100000) {
        result.error = "Suspiciously large file count: " + std::to_string(fileCount);
        return result;
    }

    // Read file table
    result.entries.reserve(fileCount);
    for (uint32_t i = 0; i < fileCount; ++i) {
        GodotPckEntry entry;

        // Path length
        uint32_t pathLen = 0;
        ifs.read(reinterpret_cast<char*>(&pathLen), 4);
        if (pathLen > 4096) {
            result.error = "Path too long at entry " + std::to_string(i);
            return result;
        }

        // Path string (padded to 4-byte alignment)
        std::string path(pathLen, '\0');
        ifs.read(path.data(), pathLen);
        // Remove null padding
        if (auto pos = path.find('\0'); pos != std::string::npos) {
            path.resize(pos);
        }
        entry.path = std::move(path);

        // Offset and size
        ifs.read(reinterpret_cast<char*>(&entry.offset), 8);
        ifs.read(reinterpret_cast<char*>(&entry.size), 8);

        // MD5
        ifs.read(reinterpret_cast<char*>(entry.md5.data()), 16);

        result.entries.push_back(std::move(entry));
    }

    if (!ifs) {
        result.error = "Unexpected end of file while reading file table";
        return result;
    }

    result.success = true;
    return result;
}

std::string GodotPckParser::extractFile(
    const fs::path& pckFile,
    const GodotPckEntry& entry
) {
    std::ifstream ifs(pckFile, std::ios::binary);
    if (!ifs) return {};

    ifs.seekg(static_cast<std::streamoff>(entry.offset));
    if (!ifs) return {};

    std::string data(entry.size, '\0');
    ifs.read(data.data(), static_cast<std::streamsize>(entry.size));
    if (!ifs) return {};

    return data;
}

// ============================================================================
// GODOT HANDLER — DETECTION
// ============================================================================

bool GodotHandler::canHandleGame(const fs::path& gameDir) {
    // Check for project.godot
    if (findProjectFile(gameDir)) return true;

    // Check for .pck files
    auto pckFiles = findPckFiles(gameDir);
    for (const auto& pck : pckFiles) {
        if (GodotPckParser::isPckFile(pck)) return true;
    }

    return false;
}

std::optional<fs::path> GodotHandler::findProjectFile(const fs::path& gameDir) {
    auto projectFile = gameDir / "project.godot";
    if (fs::exists(projectFile)) return projectFile;

    // Some exported games don't have project.godot,
    // check for override.cfg (Godot export marker)
    auto overrideCfg = gameDir / "override.cfg";
    if (fs::exists(overrideCfg)) return overrideCfg;

    return std::nullopt;
}

std::vector<fs::path> GodotHandler::findPckFiles(const fs::path& gameDir) {
    std::vector<fs::path> result;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(gameDir, ec)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".pck") {
            result.push_back(entry.path());
        }
    }
    return result;
}

bool GodotHandler::hasGodotSignature(const fs::path& executable) {
    std::ifstream ifs(executable, std::ios::binary);
    if (!ifs) return false;

    // Godot embeds PCK at end of executable — check for GDPC magic near EOF
    ifs.seekg(-256, std::ios::end);
    if (!ifs) return false;

    std::vector<char> buf(256);
    ifs.read(buf.data(), 256);
    auto bytes = static_cast<size_t>(ifs.gcount());

    for (size_t i = 0; i + 4 <= bytes; ++i) {
        uint32_t val = 0;
        std::memcpy(&val, buf.data() + i, 4);
        if (val == GODOT_PCK_MAGIC) return true;
    }
    return false;
}

// ============================================================================
// FILE DISCOVERY
// ============================================================================

Result<std::vector<GameFile>> GodotHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    // Find loose translation files
    auto translationFiles = findTranslationFiles(gameDir);
    for (const auto& f : translationFiles) {
        GameFile gf;
        gf.path = f;
        gf.relativePath = fs::relative(f, gameDir).generic_string();
        gf.size = fs::file_size(f);

        auto ext = f.extension().string();
        if (ext == ".csv" || ext == ".po") {
            gf.type = GameFileType::Localization;
        } else if (ext == ".tres" || ext == ".tscn") {
            gf.type = GameFileType::Resource;
        } else {
            gf.type = GameFileType::Data;
        }
        files.push_back(std::move(gf));
    }

    // Find PCK files
    auto pckFiles = findPckFiles(gameDir);
    for (const auto& pck : pckFiles) {
        GameFile gf;
        gf.path = pck;
        gf.relativePath = fs::relative(pck, gameDir).generic_string();
        gf.type = GameFileType::Binary;
        gf.size = fs::file_size(pck);
        files.push_back(std::move(gf));
    }

    return files;
}

std::vector<fs::path> GodotHandler::findTranslationFiles(const fs::path& gameDir) {
    std::vector<fs::path> result;
    std::error_code ec;

    // Common translation directories
    const std::vector<std::string> searchDirs = {
        "translations", "localization", "locale", "lang", "i18n", "l10n"
    };

    // Search in root and common subdirectories
    auto searchIn = [&](const fs::path& dir) {
        if (!fs::exists(dir, ec)) return;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext == ".csv" || ext == ".po" || ext == ".tres" || ext == ".tscn") {
                result.push_back(entry.path());
            }
        }
    };

    // Search root
    for (const auto& entry : fs::directory_iterator(gameDir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".csv" || ext == ".po") {
            result.push_back(entry.path());
        }
    }

    // Search known subdirectories
    for (const auto& subdir : searchDirs) {
        searchIn(gameDir / subdir);
    }

    return result;
}

std::vector<fs::path> GodotHandler::findTranslationFilesInPck(const fs::path& pckFile) {
    std::vector<fs::path> result;
    auto pck = GodotPckParser::parse(pckFile);
    if (!pck.success) return result;

    for (const auto& entry : pck.entries) {
        // Check if entry is a translation file
        auto path = entry.path;
        auto dotPos = path.rfind('.');
        if (dotPos == std::string::npos) continue;

        auto ext = path.substr(dotPos);
        if (ext == ".csv" || ext == ".po" || ext == ".translation") {
            result.push_back(fs::path(path));
        }
    }
    return result;
}

// ============================================================================
// STRING EXTRACTION
// ============================================================================

Result<ExtractionResult> GodotHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    ExtractionResult result;
    auto startTime = std::chrono::steady_clock::now();

    MAKINEAI_LOG_INFO(log::HANDLER, "Godot: extracting strings from {}", gameDir.string());

    // Extract from loose files
    auto translationFiles = findTranslationFiles(gameDir);
    for (const auto& file : translationFiles) {
        std::ifstream ifs(file);
        if (!ifs) continue;

        std::string content(
            std::istreambuf_iterator<char>{ifs},
            std::istreambuf_iterator<char>{}
        );

        ExtractionBatch batch;
        auto ext = file.extension().string();

        if (ext == ".csv") {
            batch = extractFromCsv(file, content, options);
        } else if (ext == ".po") {
            batch = extractFromPo(file, content, options);
        } else if (ext == ".tres" || ext == ".tscn") {
            batch = extractFromTresOrTscn(file, content, options);
        }

        result.entries.insert(result.entries.end(),
            batch.entries.begin(), batch.entries.end());
        result.totalStrings += batch.total;
        result.skippedStrings += batch.skipped;

        GameFile gf;
        gf.path = file;
        gf.relativePath = fs::relative(file, gameDir).generic_string();
        gf.type = GameFileType::Localization;
        gf.stringCount = static_cast<int>(batch.entries.size());
        result.processedFiles.push_back(std::move(gf));
    }

    // Extract from PCK files
    auto pckFiles = findPckFiles(gameDir);
    for (const auto& pckPath : pckFiles) {
        auto pck = GodotPckParser::parse(pckPath);
        if (!pck.success) {
            result.errors.push_back({
                pckPath.string(), pck.error, ExtractionSeverity::Warning
            });
            continue;
        }

        for (const auto& entry : pck.entries) {
            auto dotPos = entry.path.rfind('.');
            if (dotPos == std::string::npos) continue;
            auto ext = entry.path.substr(dotPos);

            if (ext != ".csv" && ext != ".po") continue;

            auto content = GodotPckParser::extractFile(pckPath, entry);
            if (content.empty()) continue;

            ExtractionBatch batch;
            fs::path virtualPath = pckPath / entry.path;

            if (ext == ".csv") {
                batch = extractFromCsv(virtualPath, content, options);
            } else if (ext == ".po") {
                batch = extractFromPo(virtualPath, content, options);
            }

            result.entries.insert(result.entries.end(),
                batch.entries.begin(), batch.entries.end());
            result.totalStrings += batch.total;
            result.skippedStrings += batch.skipped;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    MAKINEAI_LOG_INFO(log::HANDLER, "Godot: extracted {} strings ({} skipped) in {}ms",
        result.entries.size(), result.skippedStrings, result.duration.count());

    return result;
}

// --- CSV Extraction ---

GodotHandler::ExtractionBatch GodotHandler::extractFromCsv(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;
    std::istringstream stream(content);
    std::string line;

    // Read header to find column indices
    if (!std::getline(stream, line)) return batch;

    auto header = HandlerUtils::parseCsvLine(line);
    int keyCol = -1, enCol = -1;

    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        auto col = header[i];
        std::transform(col.begin(), col.end(), col.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (col == "key" || col == "keys" || col == "id") {
            keyCol = i;
        } else if (col == "en" || col == "english" || col == "source") {
            enCol = i;
        }
    }

    // If no English column, use the second column as source
    if (enCol < 0 && header.size() >= 2) {
        enCol = 1;
    }

    if (keyCol < 0) keyCol = 0;

    auto relPath = file.generic_string();

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto fields = HandlerUtils::parseCsvLine(line);
        batch.total++;

        if (static_cast<int>(fields.size()) <= std::max(keyCol, enCol)) continue;

        auto& key = fields[keyCol];
        auto& source = fields[enCol];

        if (!isValidString(source, options)) {
            batch.skipped++;
            continue;
        }

        TranslationEntry entry;
        entry.key = key;
        entry.sourceText = source;
        entry.file = relPath;
        entry.context = "csv";
        entry.category = HandlerUtils::guessCategory(key);
        batch.entries.push_back(std::move(entry));
    }

    return batch;
}

// --- PO Extraction ---

GodotHandler::ExtractionBatch GodotHandler::extractFromPo(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;
    auto relPath = file.generic_string();

    // Simple PO parser: extract msgid/msgstr pairs
    std::istringstream stream(content);
    std::string line;
    std::string currentMsgid;
    std::string currentMsgstr;
    std::string currentContext;
    bool inMsgid = false, inMsgstr = false;

    auto flushEntry = [&]() {
        if (currentMsgid.empty()) return;
        batch.total++;

        if (!isValidString(currentMsgid, options)) {
            batch.skipped++;
            currentMsgid.clear();
            currentMsgstr.clear();
            currentContext.clear();
            return;
        }

        TranslationEntry entry;
        entry.key = currentContext.empty()
            ? currentMsgid
            : currentContext + "|" + currentMsgid;
        entry.sourceText = currentMsgid;
        if (!currentMsgstr.empty()) {
            entry.translatedText = currentMsgstr;
        }
        entry.file = relPath;
        entry.context = currentContext.empty() ? "po" : currentContext;
        entry.category = HandlerUtils::guessCategory(currentMsgid);
        batch.entries.push_back(std::move(entry));

        currentMsgid.clear();
        currentMsgstr.clear();
        currentContext.clear();
    };

    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            flushEntry();
            inMsgid = inMsgstr = false;
            continue;
        }

        if (line[0] == '#') continue;

        if (line.starts_with("msgctxt ")) {
            auto quoted = line.substr(8);
            if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
                currentContext = quoted.substr(1, quoted.size() - 2);
            }
            continue;
        }

        if (line.starts_with("msgid ")) {
            flushEntry();
            auto quoted = line.substr(6);
            if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
                currentMsgid = quoted.substr(1, quoted.size() - 2);
            }
            inMsgid = true;
            inMsgstr = false;
            continue;
        }

        if (line.starts_with("msgstr ")) {
            auto quoted = line.substr(7);
            if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"') {
                currentMsgstr = quoted.substr(1, quoted.size() - 2);
            }
            inMsgstr = true;
            inMsgid = false;
            continue;
        }

        // Continuation line
        if (line.size() >= 2 && line.front() == '"' && line.back() == '"') {
            auto text = line.substr(1, line.size() - 2);
            if (inMsgid) currentMsgid += text;
            else if (inMsgstr) currentMsgstr += text;
        }
    }
    flushEntry();

    return batch;
}

// --- TRES/TSCN Extraction ---

GodotHandler::ExtractionBatch GodotHandler::extractFromTresOrTscn(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;
    auto relPath = file.generic_string();

    // Extract quoted strings from text = "..." lines in TRES/TSCN format
    static const std::regex textPattern(R"((?:text|dialog|message|tooltip|hint_tooltip)\s*=\s*"([^"]*)")");

    auto begin = std::sregex_iterator(content.begin(), content.end(), textPattern);
    auto end = std::sregex_iterator();

    int index = 0;
    for (auto it = begin; it != end; ++it) {
        auto match = *it;
        auto text = match[1].str();
        batch.total++;

        if (!isValidString(text, options)) {
            batch.skipped++;
            continue;
        }

        TranslationEntry entry;
        entry.key = relPath + ":" + std::to_string(index++);
        entry.sourceText = text;
        entry.file = relPath;
        entry.context = "scene";
        entry.category = EntryCategory::UI;
        batch.entries.push_back(std::move(entry));
    }

    return batch;
}

bool GodotHandler::isValidString(const std::string& text, const ExtractionOptions& options) {
    return HandlerUtils::isValidString(text, options);
}

// ============================================================================
// PATCH APPLICATION
// ============================================================================

Result<HandlerPatchResult> GodotHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    HandlerPatchResult result;
    auto startTime = std::chrono::steady_clock::now();

    // Build translation map
    std::map<std::string, std::string> translationMap;
    for (const auto& entry : translations) {
        if (!entry.translatedText.empty()) {
            translationMap[entry.key] = entry.translatedText;
        }
    }

    if (translationMap.empty()) {
        result.success = true;
        return result;
    }

    // Apply to loose translation files
    auto translationFiles = findTranslationFiles(gameDir);
    for (const auto& file : translationFiles) {
        std::ifstream ifs(file);
        if (!ifs) continue;

        std::string content(
            std::istreambuf_iterator<char>{ifs},
            std::istreambuf_iterator<char>{}
        );
        ifs.close();

        std::string newContent;
        auto ext = file.extension().string();

        if (ext == ".csv") {
            newContent = applyCsvTranslations(content, translationMap);
        } else if (ext == ".po") {
            newContent = applyPoTranslations(content, translationMap);
        } else {
            continue;
        }

        if (newContent == content) continue;

        if (!options.dryRun) {
            std::ofstream ofs(file);
            if (!ofs) {
                result.errors.push_back({
                    file.string(), "Cannot write file", ExtractionSeverity::Error
                });
                continue;
            }
            ofs << newContent;
        }

        PatchedFile pf;
        pf.path = file.string();
        pf.changedStrings = static_cast<int>(translationMap.size());
        result.patchedFiles.push_back(std::move(pf));
        result.appliedCount += static_cast<int>(translationMap.size());
    }

    auto endTime = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    result.success = result.errors.empty();
    return result;
}

std::string GodotHandler::applyCsvTranslations(
    const std::string& content,
    const std::map<std::string, std::string>& translations
) {
    std::istringstream stream(content);
    std::ostringstream output;
    std::string line;

    // Copy header
    if (std::getline(stream, line)) {
        output << line << '\n';

        // Find or add "tr" column
        auto header = HandlerUtils::parseCsvLine(line);
        int keyCol = 0, trCol = -1;

        for (int i = 0; i < static_cast<int>(header.size()); ++i) {
            auto col = header[i];
            std::transform(col.begin(), col.end(), col.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (col == "key" || col == "keys" || col == "id") keyCol = i;
            if (col == "tr" || col == "turkish") trCol = i;
        }

        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') {
                output << line << '\n';
                continue;
            }

            auto fields = HandlerUtils::parseCsvLine(line);
            if (static_cast<int>(fields.size()) <= keyCol) {
                output << line << '\n';
                continue;
            }

            auto it = translations.find(fields[keyCol]);
            if (it != translations.end()) {
                // Update or add Turkish column
                if (trCol >= 0 && trCol < static_cast<int>(fields.size())) {
                    fields[trCol] = it->second;
                } else {
                    // Append as new column
                    fields.push_back(it->second);
                }
            }

            // Rebuild CSV line
            for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
                if (i > 0) output << ',';
                output << HandlerUtils::escapeCsvField(fields[i]);
            }
            output << '\n';
        }
    }

    return output.str();
}

std::string GodotHandler::applyPoTranslations(
    const std::string& content,
    const std::map<std::string, std::string>& translations
) {
    std::istringstream stream(content);
    std::ostringstream output;
    std::string line;
    std::string currentMsgid;
    bool inMsgid = false;
    bool needsReplace = false;

    while (std::getline(stream, line)) {
        if (line.starts_with("msgid ")) {
            currentMsgid.clear();
            if (line.size() >= 8) {
                currentMsgid = line.substr(7, line.size() - 8); // strip quotes
            }
            inMsgid = true;
            needsReplace = translations.count(currentMsgid) > 0;
            output << line << '\n';
            continue;
        }

        if (line.starts_with("msgstr ") && needsReplace) {
            auto it = translations.find(currentMsgid);
            if (it != translations.end()) {
                output << "msgstr \"" << it->second << "\"\n";
            } else {
                output << line << '\n';
            }
            inMsgid = false;
            needsReplace = false;
            continue;
        }

        output << line << '\n';
        if (inMsgid && line.size() >= 2 && line.front() == '"' && line.back() == '"') {
            currentMsgid += line.substr(1, line.size() - 2);
            needsReplace = translations.count(currentMsgid) > 0;
        }
    }

    return output.str();
}

// ============================================================================
// BACKUP / RESTORE / VALIDATE
// ============================================================================

Result<HandlerBackupResult> GodotHandler::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& /*specificFiles*/
) {
    HandlerBackupResult result;
    auto backupDir = gameDir / ".makineai_backups" / backupId;

    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec) {
        result.success = false;
        result.errorMessage = "Cannot create backup directory: " + ec.message();
        return result;
    }

    auto files = findTranslationFiles(gameDir);
    for (const auto& file : files) {
        auto rel = fs::relative(file, gameDir);
        auto dest = backupDir / rel;
        fs::create_directories(dest.parent_path(), ec);
        fs::copy_file(file, dest, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.backedUpFiles.push_back(rel.generic_string());
            result.totalSize += fs::file_size(file, ec);
        }
    }

    result.success = true;
    result.backupId = backupId;
    result.backupPath = backupDir.string();
    return result;
}

Result<HandlerRestoreResult> GodotHandler::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    HandlerRestoreResult result;
    auto backupDir = gameDir / ".makineai_backups" / backupId;

    if (!fs::exists(backupDir)) {
        result.success = false;
        result.errorMessage = "Backup not found: " + backupId;
        return result;
    }

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(backupDir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto rel = fs::relative(entry.path(), backupDir);
        auto dest = gameDir / rel;
        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.restoredFiles.push_back(rel.generic_string());
        }
    }

    result.success = true;
    return result;
}

Result<ValidationResult> GodotHandler::validatePatch(
    const fs::path& /*gameDir*/,
    const std::vector<TranslationEntry>& translations
) {
    ValidationResult result;

    for (const auto& entry : translations) {
        result.checkedCount++;

        if (entry.translatedText.empty()) {
            result.issues.push_back({
                entry.file, entry.key,
                "Empty translation", ValidationSeverity::Warning
            });
            continue;
        }

        result.passedCount++;
    }

    result.failedCount = result.checkedCount - result.passedCount;
    result.isValid = result.failedCount == 0;
    return result;
}

} // namespace makineai
