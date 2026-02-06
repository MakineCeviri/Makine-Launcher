/**
 * @file gamemaker_handler.cpp
 * @brief GameMaker game handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/gamemaker_handler.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"

#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace makineai {

using json = nlohmann::json;

// ========== GameMakerDataParser ==========

GameMakerParseResult GameMakerDataParser::parse(const fs::path& file) {
    GameMakerParseResult result;

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) {
        result.error = "Failed to open file";
        return result;
    }

    // Read entire file
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    ifs.close();

    if (data.size() < 8) {
        result.error = "File too small";
        return result;
    }

    // Check FORM magic
    uint32_t magic = *reinterpret_cast<uint32_t*>(data.data());
    if (magic != GAMEMAKER_FORM_MAGIC) {
        result.error = "Invalid FORM magic";
        return result;
    }

    // Find STRG chunk
    size_t strgOffset = findChunk(data, "STRG");
    if (strgOffset == 0) {
        result.error = "STRG chunk not found";
        return result;
    }

    // Parse STRG chunk
    // Format: chunk_name(4) + chunk_size(4) + count(4) + offsets[count] + strings
    size_t pos = strgOffset + 8; // Skip name and size

    if (pos + 4 > data.size()) {
        result.error = "Invalid STRG chunk";
        return result;
    }

    uint32_t stringCount = *reinterpret_cast<uint32_t*>(data.data() + pos);
    pos += 4;

    if (stringCount > 1000000) {
        result.error = "Too many strings (possibly corrupt file)";
        return result;
    }

    // Read string offsets
    std::vector<uint32_t> offsets(stringCount);
    for (uint32_t i = 0; i < stringCount; i++) {
        if (pos + 4 > data.size()) break;
        offsets[i] = *reinterpret_cast<uint32_t*>(data.data() + pos);
        pos += 4;
    }

    // Read strings
    for (uint32_t i = 0; i < stringCount; i++) {
        if (offsets[i] >= data.size()) continue;

        std::string str = readString(data, offsets[i]);
        if (!str.empty()) {
            result.strings.push_back(GameMakerString{
                .index = i,
                .offset = offsets[i],
                .content = str
            });
        }
    }

    result.success = true;
    spdlog::debug("GameMaker: Parsed {} strings from STRG chunk", result.strings.size());

    return result;
}

std::string GameMakerDataParser::readString(const std::vector<uint8_t>& data, size_t offset) {
    // GameMaker string format: length(4) + chars (null-terminated)
    if (offset + 4 > data.size()) return "";

    uint32_t length = *reinterpret_cast<const uint32_t*>(data.data() + offset);
    if (length > 100000 || offset + 4 + length > data.size()) return "";

    // Read string
    std::string str(reinterpret_cast<const char*>(data.data() + offset + 4), length);

    // Remove null terminator if present
    while (!str.empty() && str.back() == '\0') {
        str.pop_back();
    }

    return str;
}

size_t GameMakerDataParser::findChunk(const std::vector<uint8_t>& data, const char* chunkName) {
    // Search for chunk by name
    // FORM file format: chunks are stored sequentially
    // Each chunk: name(4) + size(4) + data

    size_t pos = 8; // Skip FORM header

    while (pos + 8 <= data.size()) {
        // Check chunk name
        if (std::memcmp(data.data() + pos, chunkName, 4) == 0) {
            return pos;
        }

        // Get chunk size and skip
        uint32_t chunkSize = *reinterpret_cast<const uint32_t*>(data.data() + pos + 4);
        pos += 8 + chunkSize;

        // Align to 4 bytes
        pos = (pos + 3) & ~3;
    }

    return 0; // Not found
}

int GameMakerDataParser::applyTranslations(
    const fs::path& file,
    const std::map<std::string, std::string>& translations,
    bool createBackup
) {
    // Create backup
    if (createBackup) {
        fs::path backupPath = file;
        backupPath += ".makineai_backup";
        if (!fs::exists(backupPath)) {
            fs::copy_file(file, backupPath);
        }
    }

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return 0;

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    ifs.close();

    // Find STRG chunk
    size_t strgOffset = findChunk(data, "STRG");
    if (strgOffset == 0) return 0;

    size_t pos = strgOffset + 8;
    if (pos + 4 > data.size()) return 0;

    uint32_t stringCount = *reinterpret_cast<uint32_t*>(data.data() + pos);
    pos += 4;

    // Read offsets
    std::vector<uint32_t> offsets(stringCount);
    for (uint32_t i = 0; i < stringCount; i++) {
        if (pos + 4 > data.size()) break;
        offsets[i] = *reinterpret_cast<uint32_t*>(data.data() + pos);
        pos += 4;
    }

    int appliedCount = 0;

    // Apply translations (in-place, same length only)
    for (uint32_t i = 0; i < stringCount; i++) {
        if (offsets[i] + 4 >= data.size()) continue;

        uint32_t length = *reinterpret_cast<uint32_t*>(data.data() + offsets[i]);
        if (length > 100000 || offsets[i] + 4 + length > data.size()) continue;

        std::string original(reinterpret_cast<char*>(data.data() + offsets[i] + 4), length);
        while (!original.empty() && original.back() == '\0') original.pop_back();

        auto it = translations.find(original);
        if (it != translations.end()) {
            const std::string& replacement = it->second;

            // Only replace if same length or shorter
            if (replacement.length() <= length) {
                std::memcpy(data.data() + offsets[i] + 4, replacement.c_str(), replacement.length());
                // Null-fill remaining space
                if (replacement.length() < length) {
                    std::memset(data.data() + offsets[i] + 4 + replacement.length(), 0,
                               length - replacement.length());
                }
                appliedCount++;
            }
        }
    }

    // Write back atomically
    if (appliedCount > 0) {
        fs::path tempPath = file.string() + ".makineai_tmp";

        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                spdlog::error("GameMaker: Cannot create temp file for patching");
                return 0;
            }

            ofs.write(reinterpret_cast<char*>(data.data()), data.size());
            ofs.flush();

            if (!ofs.good()) {
                ofs.close();
                std::error_code ec;
                fs::remove(tempPath, ec);
                spdlog::error("GameMaker: Write failed - possible disk full");
                return 0;
            }
        } // File closed

        // Atomic rename
        std::error_code ec;
        fs::rename(tempPath, file, ec);
        if (ec) {
            fs::remove(tempPath, ec);
            spdlog::error("GameMaker: Rename failed: {}", ec.message());
            return 0;
        }
    }

    return appliedCount;
}

// ========== GameMakerHandler ==========

bool GameMakerHandler::canHandleGame(const fs::path& gameDir) {
    // Check for data files
    std::vector<std::string> dataFiles = {
        "data.win", "game.win", "game.unx", "game.ios", "game.droid"
    };

    for (const auto& filename : dataFiles) {
        auto path = gameDir / filename;
        if (fs::exists(path)) {
            // Verify it's a valid GameMaker file
            auto result = GameMakerDataParser::parse(path);
            if (result.success) {
                dataWinPath_ = path;
                return true;
            }
        }
    }

    // Check options.ini (older GameMaker)
    auto optionsIni = gameDir / "options.ini";
    if (fs::exists(optionsIni)) {
        std::ifstream ifs(optionsIni);
        std::string content((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());

        if (content.find("[GameMaker]") != std::string::npos ||
            content.find("displayname=") != std::string::npos) {
            return true;
        }
    }

    // Check for Steam API + data files
    if (fs::exists(gameDir / "steam_api.dll") || fs::exists(gameDir / "steam_api64.dll")) {
        for (const auto& filename : dataFiles) {
            auto path = gameDir / filename;
            if (fs::exists(path)) {
                dataWinPath_ = path;
                return true;
            }
        }
    }

    return false;
}

std::optional<fs::path> GameMakerHandler::findDataFile(const fs::path& gameDir) {
    std::vector<std::string> dataFiles = {
        "data.win", "game.win", "game.unx", "game.ios", "game.droid"
    };

    for (const auto& filename : dataFiles) {
        auto path = gameDir / filename;
        if (fs::exists(path)) {
            return path;
        }
    }
    return std::nullopt;
}

std::vector<fs::path> GameMakerHandler::findLanguageFiles(const fs::path& gameDir) {
    std::vector<fs::path> files;
    std::vector<std::string> langDirs = {"lang", "localization", "languages", "text"};
    std::vector<std::string> validExts = {".txt", ".csv", ".json", ".ini"};

    for (const auto& dirName : langDirs) {
        auto dir = gameDir / dirName;
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            for (const auto& validExt : validExts) {
                if (ext == validExt) {
                    files.push_back(entry.path());
                    break;
                }
            }
        }
    }

    return files;
}

Result<std::vector<GameFile>> GameMakerHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    // Main data file
    if (dataWinPath_) {
        files.push_back(GameFile{
            .path = dataWinPath_->string(),
            .relativePath = fs::relative(*dataWinPath_, gameDir).string(),
            .type = GameFileType::Binary,
            .size = fs::file_size(*dataWinPath_)
        });
    }

    // External language files
    auto langFiles = findLanguageFiles(gameDir);
    for (const auto& langFile : langFiles) {
        files.push_back(GameFile{
            .path = langFile.string(),
            .relativePath = fs::relative(langFile, gameDir).string(),
            .type = GameFileType::Localization,
            .size = fs::file_size(langFile),
            .encoding = "utf-8"
        });
    }

    return files;
}

Result<ExtractionResult> GameMakerHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "GameMaker: Starting string extraction from {}", gameDir.string());
    auto timer = metrics().timer("gamemaker_extract_strings");

    std::vector<TranslationEntry> entries;
    std::vector<ExtractionError> errors;
    std::vector<GameFile> processedFiles;
    int totalStrings = 0;
    int skippedStrings = 0;
    int filesProcessed = 0;

    // Extract from data.win
    if (dataWinPath_) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Processing data file: {}", dataWinPath_->string());
        auto batch = extractFromDataWin(*dataWinPath_, options);

        totalStrings += batch.total;
        skippedStrings += batch.skipped;
        entries.insert(entries.end(), batch.entries.begin(), batch.entries.end());

        processedFiles.push_back(GameFile{
            .path = dataWinPath_->string(),
            .relativePath = dataWinPath_->filename().string(),
            .type = GameFileType::Binary,
            .stringCount = static_cast<int>(batch.entries.size())
        });
        filesProcessed++;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Extracted {} strings from data file",
            batch.entries.size());
    }

    // Extract from language files
    auto langFiles = findLanguageFiles(gameDir);
    MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Found {} language files", langFiles.size());

    for (const auto& langFile : langFiles) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Processing language file: {}", langFile.string());
        try {
            auto batch = extractFromLangFile(langFile, options);

            totalStrings += batch.total;
            skippedStrings += batch.skipped;
            entries.insert(entries.end(), batch.entries.begin(), batch.entries.end());

            processedFiles.push_back(GameFile{
                .path = langFile.string(),
                .relativePath = fs::relative(langFile, gameDir).string(),
                .type = GameFileType::Localization,
                .stringCount = static_cast<int>(batch.entries.size())
            });
            filesProcessed++;

            MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Extracted {} strings from {}",
                batch.entries.size(), langFile.filename().string());
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "GameMaker: Language file error in {}: {}",
                langFile.string(), e.what());
            errors.push_back(ExtractionError{
                .file = langFile.string(),
                .message = std::string("Language file error: ") + e.what(),
                .severity = ExtractionSeverity::Warning
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("gamemaker_files_processed", filesProcessed);
    metrics().increment("gamemaker_strings_extracted", entries.size());

    MAKINEAI_LOG_INFO(log::HANDLER, "GameMaker: Extraction complete - {} strings from {} files in {}ms",
        entries.size(), filesProcessed, duration.count());

    return ExtractionResult{
        .entries = entries,
        .errors = errors,
        .processedFiles = processedFiles,
        .totalStrings = totalStrings,
        .skippedStrings = skippedStrings,
        .duration = duration
    };
}

GameMakerHandler::ExtractionBatch GameMakerHandler::extractFromDataWin(
    const fs::path& file,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    auto result = GameMakerDataParser::parse(file);
    if (!result.success) {
        return batch;
    }

    for (const auto& str : result.strings) {
        batch.total++;

        if (!isValidString(str.content, options)) {
            batch.skipped++;
            continue;
        }

        batch.entries.push_back(TranslationEntry{
            .filePath = file.string(),
            .entryKey = "gm_str_" + std::to_string(str.index),
            .sourceText = str.content,
            .context = "GameMaker String #" + std::to_string(str.index) +
                       " (offset: " + std::to_string(str.offset) + ")",
            .category = categorizeText(str.content)
        });
    }

    return batch;
}

GameMakerHandler::ExtractionBatch GameMakerHandler::extractFromLangFile(
    const fs::path& file,
    const ExtractionOptions& options
) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return {};

    std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    ifs.close();

    auto ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".csv") {
        return extractFromCsv(file, content, options);
    } else if (ext == ".json") {
        return extractFromJson(file, content, options);
    } else {
        return extractFromIni(file, content, options);
    }
}

GameMakerHandler::ExtractionBatch GameMakerHandler::extractFromCsv(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(iss, line)) {
        lineNum++;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') continue;

        // Split by comma
        size_t commaPos = line.find(',');
        if (commaPos == std::string::npos) continue;

        std::string key = line.substr(0, commaPos);
        std::string value = line.substr(commaPos + 1);

        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t\""));
        key.erase(key.find_last_not_of(" \t\"") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);

        if (key.empty() || value.empty()) continue;

        batch.total++;

        if (!isValidString(value, options)) {
            batch.skipped++;
            continue;
        }

        batch.entries.push_back(TranslationEntry{
            .filePath = file.string(),
            .entryKey = key,
            .sourceText = value,
            .context = "Line " + std::to_string(lineNum),
            .category = categorizeText(value)
        });
    }

    return batch;
}

GameMakerHandler::ExtractionBatch GameMakerHandler::extractFromJson(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    try {
        auto data = json::parse(content);

        std::function<void(const json&, const std::string&)> extractFromObj;
        extractFromObj = [&](const json& obj, const std::string& prefix) {
            if (obj.is_object()) {
                for (auto& [key, value] : obj.items()) {
                    std::string path = prefix.empty() ? key : prefix + "." + key;
                    if (value.is_string()) {
                        std::string text = value.get<std::string>();
                        batch.total++;

                        if (!isValidString(text, options)) {
                            batch.skipped++;
                            continue;
                        }

                        batch.entries.push_back(TranslationEntry{
                            .filePath = file.string(),
                            .entryKey = path,
                            .sourceText = text,
                            .context = "JSON path: " + path,
                            .category = categorizeText(text)
                        });
                    } else if (value.is_object() || value.is_array()) {
                        extractFromObj(value, path);
                    }
                }
            } else if (obj.is_array()) {
                for (size_t i = 0; i < obj.size(); i++) {
                    std::string path = prefix + "[" + std::to_string(i) + "]";
                    if (obj[i].is_string()) {
                        std::string text = obj[i].get<std::string>();
                        batch.total++;

                        if (!isValidString(text, options)) {
                            batch.skipped++;
                            continue;
                        }

                        batch.entries.push_back(TranslationEntry{
                            .filePath = file.string(),
                            .entryKey = path,
                            .sourceText = text,
                            .context = "JSON path: " + path,
                            .category = categorizeText(text)
                        });
                    } else if (obj[i].is_object() || obj[i].is_array()) {
                        extractFromObj(obj[i], path);
                    }
                }
            }
        };

        extractFromObj(data, "");
    } catch (...) {
        // Fall back to regex-based extraction
        std::regex kvPair("\"([^\"]+)\"\\s*:\\s*\"([^\"]+)\"");
        std::sregex_iterator it(content.begin(), content.end(), kvPair);
        std::sregex_iterator end;

        while (it != end) {
            std::string key = (*it)[1].str();
            std::string value = (*it)[2].str();
            batch.total++;

            if (isValidString(value, options)) {
                batch.entries.push_back(TranslationEntry{
                    .filePath = file.string(),
                    .entryKey = key,
                    .sourceText = value,
                    .category = categorizeText(value)
                });
            } else {
                batch.skipped++;
            }
            ++it;
        }
    }

    return batch;
}

GameMakerHandler::ExtractionBatch GameMakerHandler::extractFromIni(
    const fs::path& file,
    const std::string& content,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(iss, line)) {
        lineNum++;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#' || line[0] == '[') continue;

        // Find = separator
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Trim
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key.empty() || value.empty()) continue;

        batch.total++;

        if (!isValidString(value, options)) {
            batch.skipped++;
            continue;
        }

        batch.entries.push_back(TranslationEntry{
            .filePath = file.string(),
            .entryKey = key,
            .sourceText = value,
            .context = "Line " + std::to_string(lineNum),
            .category = categorizeText(value)
        });
    }

    return batch;
}

// ========== Patching ==========

Result<HandlerPatchResult> GameMakerHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "GameMaker: Starting translation application to {}", gameDir.string());
    auto timer = metrics().timer("gamemaker_apply_translations");

    std::vector<PatchedFile> patchedFiles;
    std::vector<PatchError> errors;
    std::string backupId;
    int appliedCount = 0;
    int skippedCount = 0;
    int filesPatched = 0;

    // Build translation map
    std::map<std::string, std::string> translationMap;
    for (const auto& entry : translations) {
        if (entry.targetText && !entry.targetText->empty()) {
            translationMap[entry.sourceText] = *entry.targetText;
        }
    }

    if (translationMap.empty()) {
        MAKINEAI_LOG_WARN(log::HANDLER, "GameMaker: No valid translations provided");
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - std::chrono::steady_clock::now() + timer.elapsed());

        return HandlerPatchResult{
            .success = true,
            .appliedCount = 0,
            .skippedCount = static_cast<int>(translations.size()),
            .duration = duration
        };
    }

    MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: {} valid translations to apply", translationMap.size());

    // Create backup
    if (options.createBackup) {
        backupId = options.backupId.empty() ?
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) :
            options.backupId;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Creating backup with ID: {}", backupId);
        auto backupResult = createBackup(gameDir, backupId);
        if (!backupResult || !backupResult->success) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "GameMaker: Backup creation failed");
            return std::unexpected(Error{ErrorCode::BackupFailed, "Failed to create backup"});
        }
    }

    // Apply to data.win
    if (dataWinPath_ && !options.dryRun) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Patching data file: {}", dataWinPath_->string());
        int applied = GameMakerDataParser::applyTranslations(
            *dataWinPath_,
            translationMap,
            false // Already backed up
        );

        if (applied > 0) {
            appliedCount += applied;
            patchedFiles.push_back(PatchedFile{
                .path = dataWinPath_->string(),
                .changedStrings = applied,
                .backed = !backupId.empty()
            });
            filesPatched++;

            // Audit log for data file modification
            AuditLogger::logFileAccess(*dataWinPath_, "patch_binary",
                true, "Applied " + std::to_string(applied) + " translations to data.win");

            MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Applied {} translations to data file", applied);
        }
    }

    // Apply to language files
    auto langFiles = findLanguageFiles(gameDir);
    MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Processing {} language files", langFiles.size());

    for (const auto& langFile : langFiles) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Patching language file: {}", langFile.string());
        try {
            int applied = patchLangFile(langFile, translationMap, options.dryRun);
            if (applied > 0) {
                appliedCount += applied;
                patchedFiles.push_back(PatchedFile{
                    .path = langFile.string(),
                    .changedStrings = applied,
                    .backed = true
                });
                filesPatched++;

                // Audit log for language file modification
                AuditLogger::logFileAccess(langFile, "patch",
                    true, "Applied " + std::to_string(applied) + " translations");

                MAKINEAI_LOG_DEBUG(log::HANDLER, "GameMaker: Applied {} translations to {}",
                    applied, langFile.filename().string());
            }
        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "GameMaker: Patch error in {}: {}",
                langFile.string(), e.what());
            errors.push_back(PatchError{
                .file = langFile.string(),
                .message = std::string("Patch error: ") + e.what()
            });
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::steady_clock::now() + timer.elapsed());

    // Record metrics
    metrics().increment("gamemaker_files_patched", filesPatched);
    metrics().increment("gamemaker_translations_applied", appliedCount);

    MAKINEAI_LOG_INFO(log::HANDLER, "GameMaker: Translation complete - {} applied, {} skipped in {} files ({}ms)",
        appliedCount, skippedCount, filesPatched, duration.count());

    return HandlerPatchResult{
        .success = errors.empty(),
        .patchedFiles = patchedFiles,
        .errors = errors,
        .backupId = backupId,
        .appliedCount = appliedCount,
        .skippedCount = skippedCount,
        .duration = duration
    };
}

int GameMakerHandler::patchLangFile(
    const fs::path& file,
    const std::map<std::string, std::string>& translations,
    bool dryRun
) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return 0;

    std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
    ifs.close();

    std::string newContent = content;
    int applied = 0;

    for (const auto& [source, target] : translations) {
        size_t pos = 0;
        while ((pos = newContent.find(source, pos)) != std::string::npos) {
            newContent.replace(pos, source.length(), target);
            pos += target.length();
            applied++;
        }
    }

    if (applied > 0 && !dryRun) {
        // Create backup
        fs::path backupPath = file;
        backupPath += ".makineai_backup";
        if (!fs::exists(backupPath)) {
            fs::copy_file(file, backupPath);
        }

        // Atomic write
        fs::path tempPath = file.string() + ".makineai_tmp";

        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                spdlog::error("GameMaker: Cannot create temp file for YAML");
                return 0;
            }

            ofs << newContent;
            ofs.flush();

            if (!ofs.good()) {
                ofs.close();
                std::error_code ec;
                fs::remove(tempPath, ec);
                spdlog::error("GameMaker: YAML write failed");
                return 0;
            }
        }

        std::error_code ec;
        fs::rename(tempPath, file, ec);
        if (ec) {
            fs::remove(tempPath, ec);
            spdlog::error("GameMaker: YAML rename failed: {}", ec.message());
            return 0;
        }
    }

    return applied;
}

// ========== Backup/Restore ==========

Result<HandlerBackupResult> GameMakerHandler::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& specificFiles
) {
    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        return HandlerBackupResult{
            .success = false,
            .backupId = backupId,
            .errorMessage = "Invalid backup ID (security)"
        };
    }

    fs::path backupDir = gameDir / ".makineai_backup" / backupId;
    std::vector<std::string> backedUpFiles;
    uint64_t totalSize = 0;

    try {
        fs::create_directories(backupDir);

        // Backup data.win
        if (dataWinPath_ &&
            (specificFiles.empty() || std::find(specificFiles.begin(), specificFiles.end(),
                                                dataWinPath_->string()) != specificFiles.end())) {
            fs::path backupPath = backupDir / dataWinPath_->filename();
            fs::copy_file(*dataWinPath_, backupPath, fs::copy_options::overwrite_existing);
            backedUpFiles.push_back(dataWinPath_->string());
            totalSize += fs::file_size(*dataWinPath_);
        }

        // Backup language files
        auto langFiles = findLanguageFiles(gameDir);
        for (const auto& langFile : langFiles) {
            if (specificFiles.empty() || std::find(specificFiles.begin(), specificFiles.end(),
                                                   langFile.string()) != specificFiles.end()) {
                fs::path backupPath = backupDir / langFile.filename();
                fs::copy_file(langFile, backupPath, fs::copy_options::overwrite_existing);
                backedUpFiles.push_back(langFile.string());
                totalSize += fs::file_size(langFile);
            }
        }

        return HandlerBackupResult{
            .success = true,
            .backupId = backupId,
            .backupPath = backupDir.string(),
            .backedUpFiles = backedUpFiles,
            .totalSize = totalSize
        };
    } catch (const std::exception& e) {
        return HandlerBackupResult{
            .success = false,
            .backupId = backupId,
            .backupPath = backupDir.string(),
            .errorMessage = e.what()
        };
    }
}

Result<HandlerRestoreResult> GameMakerHandler::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = "Invalid backup ID (security)"
        };
    }

    fs::path backupDir = gameDir / ".makineai_backup" / backupId;
    std::vector<std::string> restoredFiles;

    if (!fs::exists(backupDir)) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = "Backup not found: " + backupId
        };
    }

    try {
        for (const auto& entry : fs::directory_iterator(backupDir)) {
            if (!entry.is_regular_file()) continue;

            std::string filename = entry.path().filename().string();

            // Restore data.win
            if (dataWinPath_ && dataWinPath_->filename().string() == filename) {
                fs::copy_file(entry.path(), *dataWinPath_, fs::copy_options::overwrite_existing);
                restoredFiles.push_back(dataWinPath_->string());
            }
        }

        // Also check for direct backups
        if (dataWinPath_) {
            fs::path directBackup = *dataWinPath_;
            directBackup += ".makineai_backup";
            if (fs::exists(directBackup)) {
                fs::copy_file(directBackup, *dataWinPath_, fs::copy_options::overwrite_existing);
                restoredFiles.push_back(dataWinPath_->string());
            }
        }

        return HandlerRestoreResult{
            .success = true,
            .restoredFiles = restoredFiles
        };
    } catch (const std::exception& e) {
        return HandlerRestoreResult{
            .success = false,
            .errorMessage = e.what()
        };
    }
}

// ========== Validation ==========

Result<ValidationResult> GameMakerHandler::validatePatch(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations
) {
    std::vector<ValidationIssue> issues;
    int checked = 0;
    int passed = 0;
    int failed = 0;

    if (!dataWinPath_) {
        return ValidationResult{
            .isValid = false,
            .issues = {ValidationIssue{
                .file = gameDir.string(),
                .message = "data.win file not found",
                .severity = ValidationSeverity::Error
            }}
        };
    }

    // Parse current file
    auto result = GameMakerDataParser::parse(*dataWinPath_);
    if (!result.success) {
        return ValidationResult{
            .isValid = false,
            .issues = {ValidationIssue{
                .file = dataWinPath_->string(),
                .message = "Failed to parse: " + result.error,
                .severity = ValidationSeverity::Error
            }}
        };
    }

    // Build set of current strings
    std::set<std::string> currentStrings;
    for (const auto& str : result.strings) {
        currentStrings.insert(str.content);
    }

    // Check translations
    for (const auto& entry : translations) {
        if (!entry.targetText || entry.targetText->empty()) continue;
        checked++;

        if (currentStrings.count(*entry.targetText) > 0) {
            passed++;
        } else if (currentStrings.count(entry.sourceText) > 0) {
            // Original still exists - translation not applied
            failed++;
            issues.push_back(ValidationIssue{
                .file = dataWinPath_->string(),
                .entryKey = entry.entryKey.value_or(""),
                .message = "Translation not applied: \"" + entry.sourceText + "\"",
                .severity = ValidationSeverity::Warning
            });
        }
    }

    return ValidationResult{
        .isValid = (failed == 0),
        .issues = issues,
        .checkedCount = checked,
        .passedCount = passed,
        .failedCount = failed
    };
}

// ========== Helper Functions ==========

EntryCategory GameMakerHandler::categorizeText(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // UI elements
    if (lower.find("button") != std::string::npos ||
        lower.find("click") != std::string::npos ||
        lower.find("press") != std::string::npos ||
        lower.find("select") != std::string::npos) {
        return EntryCategory::UI;
    }

    // Menu items
    if (lower.find("menu") != std::string::npos ||
        lower.find("option") != std::string::npos ||
        lower.find("setting") != std::string::npos ||
        lower.find("quit") != std::string::npos ||
        lower.find("exit") != std::string::npos ||
        lower.find("start") != std::string::npos ||
        lower.find("continue") != std::string::npos ||
        lower.find("load") != std::string::npos ||
        lower.find("save") != std::string::npos) {
        return EntryCategory::UI;
    }

    // Dialog (long text with punctuation)
    if (text.length() > 50 ||
        text.find('.') != std::string::npos ||
        text.find('?') != std::string::npos ||
        text.find('!') != std::string::npos) {
        return EntryCategory::Dialog;
    }

    // Item/object names (short, starts with uppercase)
    if (text.length() < 30 && !text.empty() && std::isupper(text[0])) {
        return EntryCategory::Item;
    }

    return EntryCategory::Other;
}

bool GameMakerHandler::isValidString(const std::string& text, const ExtractionOptions& options) {
    // Empty or whitespace only
    std::string trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    if (trimmed.empty()) return false;

    // Minimum length
    if (text.length() < static_cast<size_t>(options.minLength)) return false;

    // Maximum length
    if (options.maxLength > 0 && text.length() > static_cast<size_t>(options.maxLength)) return false;

    // Only numbers/symbols
    static std::regex numbersOnly(R"(^[\d\s\-\+\*\/\=\.\,\!\?\:\;\(\)\[\]\{\}]+$)");
    if (std::regex_match(text, numbersOnly)) return false;

    // File paths / URLs
    if (text.find('/') != std::string::npos || text.find('\\') != std::string::npos) {
        if (text.find('.') != std::string::npos) {
            // Likely a file path
            return false;
        }
    }

    // Only contains non-printable or control characters
    bool hasText = false;
    for (unsigned char c : text) {
        if (std::isalpha(c)) {
            hasText = true;
            break;
        }
    }
    if (!hasText) return false;

    return true;
}

} // namespace makineai
