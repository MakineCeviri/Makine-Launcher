/**
 * @file unity_handler.cpp
 * @brief Unity engine handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/unity_handler.hpp"
#include "makineai/handlers/unreal_handler.hpp"
#include "makineai/handlers/renpy_handler.hpp"
#include "makineai/handlers/rpgmaker_handler.hpp"
#include "makineai/handlers/gamemaker_handler.hpp"
#include "makineai/string_classifier.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <cstring>

namespace makineai {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UnityHandler::UnityHandler() = default;

// ============================================================================
// GAME DETECTION
// ============================================================================

bool UnityHandler::canHandleGame(const fs::path& gameDir) {
    // Reset state
    dataFolder_.reset();
    metadataPath_.reset();
    buildType_ = UnityBuildType::Unknown;

    if (!fs::exists(gameDir)) return false;

    // 1. Look for *_Data folder
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            const auto name = entry.path().filename().string();
            if (name.size() > 5 && name.substr(name.size() - 5) == "_Data") {
                dataFolder_ = entry.path();
                detectBuildType(gameDir);
                return true;
            }
        }
    }

    // 2. Check for UnityPlayer.dll
    if (fs::exists(gameDir / "UnityPlayer.dll")) {
        dataFolder_ = findDataFolder(gameDir);
        detectBuildType(gameDir);
        return true;
    }

    // 3. Check for level0 or globalgamemanagers
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            const auto name = entry.path().filename().string();
            const auto lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                return s;
            }(name);

            if (lower == "level0" || lower == "globalgamemanagers" ||
                lower == "globalgamemanagers.assets") {
                dataFolder_ = entry.path().parent_path();
                detectBuildType(gameDir);
                return true;
            }
        }
    }

    return false;
}

fs::path UnityHandler::findDataFolder(const fs::path& gameDir) {
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            const auto name = entry.path().filename().string();
            if (name.size() > 5 && name.substr(name.size() - 5) == "_Data") {
                return entry.path();
            }
        }
    }
    return gameDir;
}

void UnityHandler::detectBuildType(const fs::path& gameDir) {
    // IL2CPP indicators
    // 1. GameAssembly.dll (Windows)
    if (fs::exists(gameDir / "GameAssembly.dll")) {
        buildType_ = UnityBuildType::IL2CPP;

        // Find metadata file
        if (dataFolder_) {
            const auto metadataPath = *dataFolder_ / "il2cpp_data" / "Metadata" / "global-metadata.dat";
            if (fs::exists(metadataPath)) {
                metadataPath_ = metadataPath;
            }
        }
        return;
    }

    // 2. libil2cpp.so (Linux/Android)
    if (dataFolder_) {
        const auto linuxIl2cpp = *dataFolder_ / "Plugins" / "libil2cpp.so";
        if (fs::exists(linuxIl2cpp)) {
            buildType_ = UnityBuildType::IL2CPP;

            const auto metadataPath = *dataFolder_ / "il2cpp_data" / "Metadata" / "global-metadata.dat";
            if (fs::exists(metadataPath)) {
                metadataPath_ = metadataPath;
            }
            return;
        }

        // 3. Managed folder indicates Mono build
        const auto managedDir = *dataFolder_ / "Managed";
        if (fs::exists(managedDir)) {
            buildType_ = UnityBuildType::Mono;
            return;
        }
    }

    // Default to Mono (unknown is treated as Mono)
    buildType_ = UnityBuildType::Mono;
}

// ============================================================================
// FILE DISCOVERY
// ============================================================================

Result<std::vector<GameFile>> UnityHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    if (!canHandleGame(gameDir)) {
        return std::unexpected(Error{ErrorCode::NotSupported, "Not a Unity game"});
    }

    const auto dataDir = dataFolder_.value_or(gameDir);

    // StreamingAssets folder
    const auto streamingAssets = dataDir / "StreamingAssets";
    if (fs::exists(streamingAssets)) {
        scanForLocalizationFiles(streamingAssets, gameDir, files);
    }

    // Resources folder
    const auto resources = dataDir / "Resources";
    if (fs::exists(resources)) {
        scanForLocalizationFiles(resources, gameDir, files);
    }

    // Main data folder
    scanForLocalizationFiles(dataDir, gameDir, files);

    // Common localization folder names
    const std::vector<std::string> locDirs = {
        "Localization", "Localizations", "I18n", "Language", "Languages",
        "Text", "Texts", "Strings", "Translation", "Translations"
    };

    for (const auto& locDir : locDirs) {
        auto dir = dataDir / locDir;
        if (fs::exists(dir)) {
            scanForLocalizationFiles(dir, gameDir, files, true);
        }

        // Also check in StreamingAssets
        dir = dataDir / "StreamingAssets" / locDir;
        if (fs::exists(dir)) {
            scanForLocalizationFiles(dir, gameDir, files, true);
        }
    }

    spdlog::debug("Unity: Found {} localization files", files.size());
    return files;
}

void UnityHandler::scanForLocalizationFiles(
    const fs::path& dir,
    const fs::path& gameDir,
    std::vector<GameFile>& files,
    bool recursive
) {
    try {
        auto iterator = recursive
            ? fs::recursive_directory_iterator(dir)
            : fs::recursive_directory_iterator(dir, fs::directory_options::none);

        // If not recursive, only iterate first level
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            const auto& path = entry.path();
            auto ext = path.extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            auto name = path.filename().string();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            bool isLocalization = false;

            // Check by extension
            if (ext == "json" || ext == "xml" || ext == "csv" || ext == "txt") {
                if (HandlerUtils::isLocalizationFileName(name)) {
                    isLocalization = true;
                }
            }

            // Asset files
            if (ext == "asset" || ext == "assets") {
                if (HandlerUtils::isLocalizationFileName(name)) {
                    isLocalization = true;
                }
            }

            if (isLocalization) {
                auto relativePath = fs::relative(path, gameDir).string();
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                GameFile gf;
                gf.path = path;
                gf.relativePath = relativePath;
                gf.type = (ext == "asset" || ext == "assets")
                    ? GameFileType::Resource
                    : GameFileType::Localization;
                gf.size = entry.file_size();
                gf.encoding = HandlerUtils::detectEncoding(path);

                files.push_back(std::move(gf));
            }
        }

        // If recursive, also scan subdirectories
        if (recursive) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    scanForLocalizationFiles(entry.path(), gameDir, files, true);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to scan directory {}: {}", dir.string(), e.what());
    }
}

// ============================================================================
// STRING EXTRACTION
// ============================================================================

Result<ExtractionResult> UnityHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Unity: Starting string extraction from {}", gameDir.string());
    auto timer = metrics().timer("unity_extract_strings");

    ExtractionResult result;
    int filesProcessed = 0;

    auto filesResult = findGameFiles(gameDir);
    if (!filesResult) {
        MAKINEAI_LOG_ERROR(log::HANDLER, "Unity: Failed to find game files: {}", filesResult.error().message());
        return std::unexpected(filesResult.error());
    }

    const auto& gameFiles = *filesResult;
    MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Found {} potential localization files", gameFiles.size());

    // Extract from IL2CPP metadata if available
    if (buildType_ == UnityBuildType::IL2CPP && metadataPath_ && options.extractIl2CppStrings) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Processing IL2CPP metadata: {}", metadataPath_->string());
        try {
            auto il2cppBatch = extractFromIL2Cpp(*metadataPath_, options);
            result.entries.insert(result.entries.end(),
                il2cppBatch.entries.begin(), il2cppBatch.entries.end());
            result.totalStrings += il2cppBatch.total;
            result.skippedStrings += il2cppBatch.skipped;

            if (!il2cppBatch.entries.empty()) {
                GameFile metaFile;
                metaFile.path = *metadataPath_;
                metaFile.relativePath = "il2cpp_data/Metadata/global-metadata.dat";
                metaFile.type = GameFileType::Binary;
                metaFile.size = fs::file_size(*metadataPath_);
                metaFile.stringCount = static_cast<int>(il2cppBatch.entries.size());
                result.processedFiles.push_back(std::move(metaFile));
            }

            MAKINEAI_LOG_INFO(log::HANDLER, "Unity IL2CPP: Extracted {} strings", il2cppBatch.entries.size());
            metrics().increment("unity_il2cpp_strings_extracted", il2cppBatch.entries.size());
            filesProcessed++;
        } catch (const std::exception& e) {
            MAKINEAI_LOG_WARN(log::HANDLER, "Unity: IL2CPP metadata parse error: {}", e.what());
            result.errors.push_back({
                "global-metadata.dat",
                std::string("IL2CPP metadata parse error: ") + e.what(),
                ExtractionSeverity::Warning
            });
        }
    }

    // Extract from localization files
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

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Processing file: {}", gameFile.relativePath);

        try {
            auto ext = gameFile.path.extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            ExtractionBatch batch;

            if (ext == "json") {
                batch = extractFromJson(gameFile.path, gameFile, options);
            } else if (ext == "xml") {
                batch = extractFromXml(gameFile.path, gameFile, options);
            } else if (ext == "csv") {
                batch = extractFromCsv(gameFile.path, gameFile, options);
            } else if (ext == "txt") {
                batch = extractFromTxt(gameFile.path, gameFile, options);
            } else {
                MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Skipping unsupported extension: {}", ext);
                continue;
            }

            result.entries.insert(result.entries.end(),
                batch.entries.begin(), batch.entries.end());
            result.totalStrings += batch.total;
            result.skippedStrings += batch.skipped;

            GameFile processedFile = gameFile;
            processedFile.stringCount = static_cast<int>(batch.entries.size());
            result.processedFiles.push_back(std::move(processedFile));
            filesProcessed++;

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Extracted {} strings from {}",
                batch.entries.size(), gameFile.relativePath);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unity: Extraction error in {}: {}",
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
    metrics().increment("unity_files_processed", filesProcessed);
    metrics().increment("unity_strings_extracted", result.entries.size());

    MAKINEAI_LOG_INFO(log::HANDLER, "Unity: Extraction complete - {} strings from {} files in {}ms",
        result.entries.size(), filesProcessed, result.duration.count());

    return result;
}

// ============================================================================
// JSON EXTRACTION
// ============================================================================

UnityHandler::ExtractionBatch UnityHandler::extractFromJson(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    nlohmann::json json;
    ifs >> json;

    // Base path from filename without extension
    auto basePath = gameFile.relativePath;
    if (basePath.size() > 5 && basePath.substr(basePath.size() - 5) == ".json") {
        basePath = basePath.substr(0, basePath.size() - 5);
    }

    extractJsonValue(json, basePath, gameFile, options, batch);

    return batch;
}

void UnityHandler::extractJsonValue(
    const nlohmann::json& value,
    const std::string& path,
    const GameFile& gameFile,
    const ExtractionOptions& options,
    ExtractionBatch& batch
) {
    if (value.is_null()) return;

    if (value.is_string()) {
        batch.total++;
        const auto text = value.get<std::string>();

        if (HandlerUtils::isValidString(text, options)) {
            TranslationEntry entry;
            entry.filePath = gameFile.relativePath;
            entry.entryKey = path;
            entry.sourceText = text;
            entry.context = HandlerUtils::guessContext(path);
            entry.category = HandlerUtils::guessCategory(path);
            batch.entries.push_back(std::move(entry));
        } else {
            batch.skipped++;
        }
        return;
    }

    if (value.is_array()) {
        for (size_t i = 0; i < value.size(); i++) {
            extractJsonValue(value[i], path + "[" + std::to_string(i) + "]",
                gameFile, options, batch);
        }
        return;
    }

    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            extractJsonValue(it.value(), path + "." + it.key(),
                gameFile, options, batch);
        }
    }
}

// ============================================================================
// XML EXTRACTION
// ============================================================================

UnityHandler::ExtractionBatch UnityHandler::extractFromXml(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    const auto content = buffer.str();

    // Pattern: <string|entry|text name="key">value</...>
    std::regex stringPattern(
        R"(<(?:string|entry|text|message|item)[^>]*(?:name|id|key)=["']([^"']+)["'][^>]*>([^<]+)<)",
        std::regex::icase
    );

    auto begin = std::sregex_iterator(content.begin(), content.end(), stringPattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        const auto key = match[1].str();
        auto value = match[2].str();

        // Trim
        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        batch.total++;

        if (HandlerUtils::isValidString(value, options)) {
            TranslationEntry entry;
            entry.filePath = gameFile.relativePath;
            entry.entryKey = key;
            entry.sourceText = HandlerUtils::decodeXmlEntities(value);
            entry.context = "XML string";
            entry.category = HandlerUtils::guessCategory(key);
            batch.entries.push_back(std::move(entry));
        } else {
            batch.skipped++;
        }
    }

    // Also check <Text>value</Text> format
    std::regex simplePattern(
        R"(<(Text|Value|Content|Description|Name|Title|Label)>([^<]+)</)",
        std::regex::icase
    );

    begin = std::sregex_iterator(content.begin(), content.end(), simplePattern);
    int simpleIndex = 0;

    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        auto tag = match[1].str();
        auto value = match[2].str();

        value.erase(0, value.find_first_not_of(" \t\n\r"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        batch.total++;

        if (HandlerUtils::isValidString(value, options)) {
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

            TranslationEntry entry;
            entry.filePath = gameFile.relativePath;
            entry.entryKey = tag + "_" + std::to_string(simpleIndex++);
            entry.sourceText = HandlerUtils::decodeXmlEntities(value);
            entry.context = "XML " + tag;
            entry.category = HandlerUtils::guessCategory(tag);
            batch.entries.push_back(std::move(entry));
        } else {
            batch.skipped++;
        }
    }

    return batch;
}

// ============================================================================
// CSV EXTRACTION
// ============================================================================

UnityHandler::ExtractionBatch UnityHandler::extractFromCsv(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) {
        return batch;
    }

    // Parse header to find key and text columns
    auto header = HandlerUtils::parseCsvLine(lines[0]);
    int keyColumn = 0;
    int textColumn = 1;

    for (size_t i = 0; i < header.size(); i++) {
        auto col = header[i];
        std::transform(col.begin(), col.end(), col.begin(), ::tolower);

        if (col == "key" || col == "id" || col == "name") {
            keyColumn = static_cast<int>(i);
        }
        if (col == "text" || col == "value" || col == "en" || col == "english" || col == "content") {
            textColumn = static_cast<int>(i);
        }
    }

    // Process data rows
    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i].empty()) continue;

        auto columns = HandlerUtils::parseCsvLine(lines[i]);
        if (columns.size() <= static_cast<size_t>(textColumn)) continue;

        auto key = keyColumn < static_cast<int>(columns.size())
            ? columns[keyColumn]
            : "row_" + std::to_string(i);
        const auto& value = columns[textColumn];

        batch.total++;

        if (HandlerUtils::isValidString(value, options)) {
            TranslationEntry entry;
            entry.filePath = gameFile.relativePath;
            entry.entryKey = key;
            entry.sourceText = value;
            entry.context = "CSV row " + std::to_string(i + 1);
            entry.category = HandlerUtils::guessCategory(key);
            entry.lineNumber = static_cast<int>(i + 1);
            batch.entries.push_back(std::move(entry));
        } else {
            batch.skipped++;
        }
    }

    return batch;
}

// ============================================================================
// TXT EXTRACTION
// ============================================================================

UnityHandler::ExtractionBatch UnityHandler::extractFromTxt(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    std::regex kvPattern(R"(^([A-Za-z_][A-Za-z0-9_\.]*)\s*[=:]\s*(.+)$)");
    std::string line;
    int lineNum = 0;

    while (std::getline(ifs, line)) {
        lineNum++;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty and comment lines
        if (line.empty() || line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, kvPattern)) {
            auto key = match[1].str();
            auto value = match[2].str();

            batch.total++;

            if (HandlerUtils::isValidString(value, options)) {
                TranslationEntry entry;
                entry.filePath = gameFile.relativePath;
                entry.entryKey = key;
                entry.sourceText = value;
                entry.context = "TXT line " + std::to_string(lineNum);
                entry.category = HandlerUtils::guessCategory(key);
                entry.lineNumber = lineNum;
                batch.entries.push_back(std::move(entry));
            } else {
                batch.skipped++;
            }
        }
    }

    return batch;
}

// ============================================================================
// IL2CPP EXTRACTION
// ============================================================================

IL2CppMetadata UnityHandler::parseIL2CppMetadata(const fs::path& path) {
    IL2CppMetadata result;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        result.errorMessage = "Cannot open metadata file";
        return result;
    }

    // Read header
    uint32_t magic, version;
    ifs.read(reinterpret_cast<char*>(&magic), 4);
    ifs.read(reinterpret_cast<char*>(&version), 4);

    // Check magic (0xFAB11BAF)
    if (magic != 0xFAB11BAF) {
        result.errorMessage = "Invalid IL2CPP metadata magic";
        return result;
    }

    result.version = static_cast<int>(version);

    // Skip to string table offset and count (position varies by version)
    // For simplicity, we'll use a heuristic approach
    // Note: Full implementation would need version-specific parsing

    ifs.seekg(0, std::ios::end);
    auto fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    ifs.read(reinterpret_cast<char*>(data.data()), fileSize);

    // Find string-like patterns in the file
    std::vector<IL2CppString> strings;
    int64_t index = 0;

    for (size_t i = 0; i < data.size() - 2; ++i) {
        // Look for UTF-8 strings (printable ASCII followed by null)
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            size_t strEnd = i;
            while (strEnd < data.size() && data[strEnd] >= 0x20 && data[strEnd] <= 0x7E) {
                strEnd++;
            }

            // Check if followed by null
            if (strEnd < data.size() && data[strEnd] == 0 && strEnd - i >= 3) {
                std::string text(reinterpret_cast<char*>(&data[i]), strEnd - i);

                // Skip if looks like code/path
                bool isCode = false;
                if (text.find("::") != std::string::npos ||
                    text.find("()") != std::string::npos ||
                    text.find("\\") != std::string::npos ||
                    text.find("/") != std::string::npos ||
                    text.find(".dll") != std::string::npos ||
                    text.find(".exe") != std::string::npos) {
                    isCode = true;
                }

                if (!isCode) {
                    IL2CppString str;
                    str.index = index++;
                    str.offset = static_cast<int64_t>(i);
                    str.length = static_cast<int64_t>(strEnd - i);
                    str.text = text;

                    // Guess context
                    if (text.find("Error") != std::string::npos || text.find("error") != std::string::npos) {
                        str.context = "Error message";
                    } else if (text.find("Warning") != std::string::npos) {
                        str.context = "Warning";
                    } else if (text.find("Click") != std::string::npos || text.find("Button") != std::string::npos) {
                        str.context = "UI interaction";
                    } else {
                        str.context = "Dialog/Text";
                    }

                    strings.push_back(std::move(str));
                }

                i = strEnd;
            }
        }
    }

    result.strings = std::move(strings);
    result.stringCount = static_cast<int>(result.strings.size());
    result.success = true;

    return result;
}

std::vector<IL2CppString> UnityHandler::getTranslatableStrings(
    const IL2CppMetadata& metadata,
    int minLength,
    int maxLength,
    bool excludeCodeStrings
) {
    std::vector<IL2CppString> result;

    for (const auto& str : metadata.strings) {
        if (str.text.length() < static_cast<size_t>(minLength)) continue;
        if (str.text.length() > static_cast<size_t>(maxLength)) continue;

        if (excludeCodeStrings) {
            // Skip strings that look like code
            bool isCode = false;

            // Check for common code patterns
            if (std::regex_match(str.text, std::regex(R"(^[A-Z][a-z]+[A-Z][a-zA-Z]*$)"))) {
                isCode = true;  // CamelCase
            }
            if (std::regex_match(str.text, std::regex(R"(^[a-z]+_[a-z_]+$)"))) {
                isCode = true;  // snake_case
            }
            if (str.text.find("get_") == 0 || str.text.find("set_") == 0) {
                isCode = true;  // Property accessors
            }

            if (isCode) continue;
        }

        result.push_back(str);
    }

    return result;
}

UnityHandler::ExtractionBatch UnityHandler::extractFromIL2Cpp(
    const fs::path& metadataPath,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    auto metadata = parseIL2CppMetadata(metadataPath);
    if (!metadata.success) {
        spdlog::warn("IL2CPP metadata parse error: {}", metadata.errorMessage);
        return batch;
    }

    spdlog::debug("IL2CPP metadata version: {}, total strings: {}",
        metadata.version, metadata.stringCount);

    auto translatableStrings = getTranslatableStrings(
        metadata,
        options.minLength,
        options.maxLength,
        options.skipCodeStrings
    );

    for (const auto& il2cppStr : translatableStrings) {
        batch.total++;

        if (!HandlerUtils::isValidString(il2cppStr.text, options)) {
            batch.skipped++;
            continue;
        }

        // IL2CPP info in context
        std::string contextInfo = il2cppStr.context +
            " [IL2CPP@" + std::to_string(il2cppStr.offset) +
            ":" + std::to_string(il2cppStr.length) + "]";

        TranslationEntry entry;
        entry.filePath = "il2cpp_data/Metadata/global-metadata.dat";
        entry.entryKey = "il2cpp_" + std::to_string(il2cppStr.index);
        entry.sourceText = il2cppStr.text;
        entry.context = contextInfo;
        entry.category = guessCategoryFromIL2Cpp(il2cppStr.context);
        entry.offset = il2cppStr.offset;
        entry.length = il2cppStr.length;

        batch.entries.push_back(std::move(entry));
    }

    batch.skipped = batch.total - static_cast<int>(batch.entries.size());
    return batch;
}

EntryCategory UnityHandler::guessCategoryFromIL2Cpp(const std::string& context) {
    if (context == "Error message" || context == "Warning" || context == "Status") {
        return EntryCategory::System;
    }
    if (context == "UI interaction" || context == "Menu" || context == "Settings") {
        return EntryCategory::UI;
    }
    if (context == "Dialog/Text") {
        return EntryCategory::Dialog;
    }
    if (context == "Quest") {
        return EntryCategory::Quest;
    }
    if (context == "Inventory" || context == "Game mechanic") {
        return EntryCategory::Item;
    }
    if (context == "Help") {
        return EntryCategory::Tutorial;
    }
    return EntryCategory::Other;
}

// ============================================================================
// TRANSLATION APPLICATION
// ============================================================================

Result<HandlerPatchResult> UnityHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Unity: Starting translation application to {}", gameDir.string());
    auto timer = metrics().timer("unity_apply_translations");

    HandlerPatchResult result;
    result.success = true;
    int filesPatched = 0;

    // Create backup if requested
    if (options.createBackup) {
        auto backupId = options.backupId.empty()
            ? std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
            : options.backupId;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Creating backup with ID: {}", backupId);
        auto backupResult = createBackup(gameDir, backupId);
        if (!backupResult) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unity: Backup creation failed");
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

    MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: {} translations grouped into {} files",
        translations.size(), translationsByFile.size());

    // Apply to each file
    for (const auto& [filePath, fileTranslations] : translationsByFile) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Patching file: {} ({} translations)",
            filePath, fileTranslations.size());

        // Handle IL2CPP metadata
        if (filePath.find("global-metadata.dat") != std::string::npos) {
            MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Processing IL2CPP metadata patch");
            try {
                auto batch = applyToIL2Cpp(gameDir, fileTranslations, options);
                result.appliedCount += batch.applied;
                result.skippedCount += batch.skipped;

                if (batch.applied > 0) {
                    result.patchedFiles.push_back({
                        filePath,
                        batch.applied,
                        static_cast<int>(fileTranslations.size()),
                        !result.backupId.empty()
                    });
                    filesPatched++;

                    // Audit log for IL2CPP modification
                    AuditLogger::logFileAccess(gameDir / filePath, "patch_il2cpp",
                        true, "Applied " + std::to_string(batch.applied) + " translations");
                }

                for (const auto& err : batch.errors) {
                    MAKINEAI_LOG_WARN(log::HANDLER, "Unity: IL2CPP patch warning: {}", err);
                    result.errors.push_back({filePath, err, ExtractionSeverity::Error});
                }
            } catch (const std::exception& e) {
                MAKINEAI_LOG_ERROR(log::HANDLER, "Unity: IL2CPP patch error: {}", e.what());
                result.errors.push_back({
                    filePath,
                    std::string("IL2CPP patch error: ") + e.what(),
                    ExtractionSeverity::Error
                });
            }
            continue;
        }

        // Regular files
        auto fullPath = gameDir / filePath;
        if (!fs::exists(fullPath)) {
            MAKINEAI_LOG_WARN(log::HANDLER, "Unity: File not found: {}", filePath);
            result.errors.push_back({filePath, "File not found", ExtractionSeverity::Error});
            continue;
        }

        try {
            auto ext = fullPath.extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            PatchBatch batch;

            if (ext == "json") {
                batch = applyToJson(fullPath, fileTranslations, options);
            } else if (ext == "xml") {
                batch = applyToXml(fullPath, fileTranslations, options);
            } else if (ext == "csv") {
                batch = applyToCsv(fullPath, fileTranslations, options);
            } else if (ext == "txt") {
                batch = applyToTxt(fullPath, fileTranslations, options);
            } else {
                MAKINEAI_LOG_WARN(log::HANDLER, "Unity: Unsupported file type for patching: {}", ext);
                result.skippedCount += static_cast<int>(fileTranslations.size());
                continue;
            }

            result.appliedCount += batch.applied;
            result.skippedCount += batch.skipped;

            result.patchedFiles.push_back({
                filePath,
                batch.applied,
                static_cast<int>(fileTranslations.size()),
                !result.backupId.empty()
            });
            filesPatched++;

            // Audit log for file modification
            AuditLogger::logFileAccess(fullPath, "patch",
                true, "Applied " + std::to_string(batch.applied) + " translations");

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Unity: Patched {} - {} applied, {} skipped",
                filePath, batch.applied, batch.skipped);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unity: Patch error in {}: {}", filePath, e.what());
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
    metrics().increment("unity_files_patched", filesPatched);
    metrics().increment("unity_translations_applied", result.appliedCount);

    MAKINEAI_LOG_INFO(log::HANDLER, "Unity: Translation complete - {} applied, {} skipped in {} files ({}ms)",
        result.appliedCount, result.skippedCount, filesPatched, result.duration.count());

    return result;
}

// ============================================================================
// JSON PATCHING
// ============================================================================

UnityHandler::PatchBatch UnityHandler::applyToJson(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    nlohmann::json json;
    ifs >> json;
    ifs.close();

    // Build translation map
    std::unordered_map<std::string, std::string> translationMap;
    for (const auto& t : translations) {
        if (t.targetText && t.entryKey && !t.entryKey->empty()) {
            translationMap[*t.entryKey] = *t.targetText;
        }
    }

    // Get base path
    auto basePath = file.filename().string();
    if (basePath.size() > 5 && basePath.substr(basePath.size() - 5) == ".json") {
        basePath = basePath.substr(0, basePath.size() - 5);
    }

    json = applyJsonTranslations(json, basePath, translationMap, batch.applied);

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;

    // Write back
    std::ofstream ofs(file);
    ofs << std::setw(2) << json;

    return batch;
}

nlohmann::json UnityHandler::applyJsonTranslations(
    const nlohmann::json& value,
    const std::string& path,
    const std::unordered_map<std::string, std::string>& translations,
    int& applied
) {
    if (value.is_null()) return nullptr;

    if (value.is_string()) {
        auto it = translations.find(path);
        if (it != translations.end()) {
            applied++;
            return it->second;
        }
        return value;
    }

    if (value.is_array()) {
        nlohmann::json newArray = nlohmann::json::array();
        for (size_t i = 0; i < value.size(); i++) {
            auto itemPath = path + "[" + std::to_string(i) + "]";
            newArray.push_back(applyJsonTranslations(value[i], itemPath, translations, applied));
        }
        return newArray;
    }

    if (value.is_object()) {
        nlohmann::json newObj = nlohmann::json::object();
        for (auto it = value.begin(); it != value.end(); ++it) {
            auto itemPath = path + "." + it.key();
            newObj[it.key()] = applyJsonTranslations(it.value(), itemPath, translations, applied);
        }
        return newObj;
    }

    return value;
}

// ============================================================================
// XML PATCHING
// ============================================================================

UnityHandler::PatchBatch UnityHandler::applyToXml(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    auto content = buffer.str();
    ifs.close();

    for (const auto& t : translations) {
        if (!t.targetText || !t.entryKey || t.entryKey->empty()) continue;

        auto escapedSource = HandlerUtils::encodeXmlEntities(t.sourceText);
        auto escapedTarget = HandlerUtils::encodeXmlEntities(*t.targetText);

        // Key-based replacement
        std::string pattern = R"(<(?:string|entry|text|message|item)[^>]*(?:name|id|key)=["'])" +
            *t.entryKey + R"(["'][^>]*>)" + escapedSource + R"(<)";

        std::regex keyRegex(pattern, std::regex::icase);

        std::smatch match;
        if (std::regex_search(content, match, keyRegex)) {
            // Replace match with target text
            std::string matchStr = match[0].str();
            size_t lastGt = matchStr.rfind('>');
            if (lastGt != std::string::npos) {
                std::string prefix = matchStr.substr(0, lastGt + 1);
                std::string replacement = prefix + escapedTarget + "<";
                content = content.substr(0, match.position()) + replacement +
                          content.substr(match.position() + match.length());
            }
            batch.applied++;
        } else {
            // Direct text replacement
            auto oldText = ">" + escapedSource + "<";
            auto newText = ">" + escapedTarget + "<";
            auto pos = content.find(oldText);
            if (pos != std::string::npos) {
                content.replace(pos, oldText.length(), newText);
                batch.applied++;
            }
        }
    }

    std::ofstream ofs(file);
    ofs << content;

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

// ============================================================================
// CSV PATCHING
// ============================================================================

UnityHandler::PatchBatch UnityHandler::applyToCsv(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        lines.push_back(line);
    }
    ifs.close();

    // Build translation map
    std::unordered_map<std::string, std::string> translationMap;
    for (const auto& t : translations) {
        if (t.targetText && t.entryKey && !t.entryKey->empty()) {
            translationMap[*t.entryKey] = *t.targetText;
        }
    }

    for (size_t i = 0; i < lines.size(); i++) {
        auto columns = HandlerUtils::parseCsvLine(lines[i]);
        if (columns.empty()) continue;

        auto it = translationMap.find(columns[0]);
        if (it != translationMap.end() && columns.size() > 1) {
            columns[1] = HandlerUtils::escapeCsvField(it->second);

            // Rebuild line
            std::string newLine;
            for (size_t j = 0; j < columns.size(); j++) {
                if (j > 0) newLine += ",";
                newLine += columns[j];
            }
            lines[i] = newLine;
            batch.applied++;
        }
    }

    std::ofstream ofs(file);
    for (size_t i = 0; i < lines.size(); i++) {
        ofs << lines[i];
        if (i < lines.size() - 1) ofs << "\n";
    }

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

// ============================================================================
// TXT PATCHING
// ============================================================================

UnityHandler::PatchBatch UnityHandler::applyToTxt(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    auto content = buffer.str();
    ifs.close();

    for (const auto& t : translations) {
        if (!t.targetText || !t.entryKey || t.entryKey->empty()) continue;

        // Pattern: KEY=value or KEY:value
        std::string pattern = "^(" + *t.entryKey + R"(\s*[=:]\s*)" + t.sourceText + ")$";
        std::regex keyRegex(pattern, std::regex::multiline);

        if (std::regex_search(content, keyRegex)) {
            std::string replacement = *t.entryKey + "= " + *t.targetText;
            content = std::regex_replace(content, keyRegex, replacement);
            batch.applied++;
        }
    }

    std::ofstream ofs(file);
    ofs << content;

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

// ============================================================================
// IL2CPP PATCHING
// ============================================================================

UnityHandler::PatchBatch UnityHandler::applyToIL2Cpp(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    // Find metadata path
    fs::path metadataPath;
    if (metadataPath_) {
        metadataPath = *metadataPath_;
    } else if (dataFolder_) {
        metadataPath = *dataFolder_ / "il2cpp_data" / "Metadata" / "global-metadata.dat";
    }

    if (metadataPath.empty() || !fs::exists(metadataPath)) {
        batch.errors.push_back("IL2CPP metadata file not found");
        batch.skipped = static_cast<int>(translations.size());
        return batch;
    }

    // Read file
    std::ifstream ifs(metadataPath, std::ios::binary);
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    // Build offset->translation map
    std::unordered_map<int64_t, std::string> offsetMap;
    for (const auto& t : translations) {
        if (t.targetText && t.offset > 0) {
            offsetMap[t.offset] = *t.targetText;
        }
    }

    // Apply patches
    for (const auto& [offset, newText] : offsetMap) {
        // Bounds check: offset must be valid
        if (offset < 0 || offset >= static_cast<int64_t>(data.size())) {
            spdlog::warn("IL2CPP: Invalid offset {} (file size: {})", offset, data.size());
            batch.skipped++;
            continue;
        }

        // Find original string length
        size_t origLen = 0;
        for (size_t i = static_cast<size_t>(offset); i < data.size() && data[i] != 0; i++) {
            origLen++;
        }

        // CRITICAL: Bounds check before memcpy
        // Verify offset + origLen doesn't exceed buffer
        if (static_cast<size_t>(offset) + origLen > data.size()) {
            spdlog::warn("IL2CPP: String at offset {} extends beyond file bounds", offset);
            batch.skipped++;
            continue;
        }

        // Can only replace with same or shorter string
        if (newText.length() <= origLen) {
            std::memcpy(&data[offset], newText.c_str(), newText.length());

            // Pad with spaces if shorter
            for (size_t i = newText.length(); i < origLen; i++) {
                data[static_cast<size_t>(offset) + i] = 0x20;
            }

            batch.applied++;
        } else {
            batch.skipped++;
            spdlog::debug("IL2CPP: Skipped translation at offset {} (too long: {} > {})",
                offset, newText.length(), origLen);
        }
    }

    // CRITICAL: Atomic write with flush verification
    // Write to temp file first, then rename
    fs::path tempPath = metadataPath.string() + ".makineai_tmp";

    {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            batch.errors.push_back("Failed to create temp file for IL2CPP metadata");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }

        ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
        ofs.flush();

        if (!ofs.good()) {
            ofs.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            batch.errors.push_back("IL2CPP metadata write failed - possible disk full");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }

        // Verify written size
        auto writtenPos = ofs.tellp();
        if (writtenPos != static_cast<std::streampos>(data.size())) {
            ofs.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            batch.errors.push_back("IL2CPP metadata size verification failed");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }
    } // File closed here

    // Atomic rename
    std::error_code ec;
    fs::rename(tempPath, metadataPath, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        batch.errors.push_back("IL2CPP metadata rename failed: " + ec.message());
        batch.skipped = static_cast<int>(translations.size()) - batch.applied;
        return batch;
    }

    return batch;
}

// ============================================================================
// BACKUP/RESTORE
// ============================================================================

Result<HandlerBackupResult> UnityHandler::createBackup(
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

        if (!specificFiles.empty()) {
            // Backup specific files
            for (const auto& filePath : specificFiles) {
                auto srcPath = fs::path(filePath);
                if (!fs::exists(srcPath)) continue;

                // Calculate relative path
                auto relativePath = fs::relative(srcPath, gameDir);
                auto dstPath = backupDir / relativePath;

                fs::create_directories(dstPath.parent_path());
                fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);

                result.totalSize += fs::file_size(srcPath);
                result.backedUpFiles.push_back(relativePath.string());
            }
        } else {
            // Backup all game files
            auto filesResult = findGameFiles(gameDir);
            if (!filesResult) {
                result.success = false;
                result.errorMessage = "Failed to find game files";
                return result;
            }

            for (const auto& gameFile : *filesResult) {
                auto srcPath = gameFile.path;
                auto dstPath = backupDir / gameFile.relativePath;

                fs::create_directories(dstPath.parent_path());
                fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);

                result.totalSize += gameFile.size;
                result.backedUpFiles.push_back(gameFile.relativePath);
            }

            // Also backup IL2CPP metadata if present
            if (metadataPath_ && fs::exists(*metadataPath_)) {
                auto relativePath = fs::relative(*metadataPath_, gameDir);
                auto dstPath = backupDir / relativePath;

                fs::create_directories(dstPath.parent_path());
                fs::copy_file(*metadataPath_, dstPath, fs::copy_options::overwrite_existing);

                result.totalSize += fs::file_size(*metadataPath_);
                result.backedUpFiles.push_back(relativePath.string());
            }
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

Result<HandlerRestoreResult> UnityHandler::restoreBackup(
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

        // Delete backup after restore
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

Result<ValidationResult> UnityHandler::validatePatch(
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

        // Also check IL2CPP metadata
        if (translation.filePath.find("global-metadata.dat") != std::string::npos) {
            fileExists = metadataPath_ && fs::exists(*metadataPath_);
        }

        if (!fileExists) {
            result.failedCount++;
            result.issues.push_back({
                translation.filePath,
                translation.entryKey.value_or(""),
                "Source file not found",
                ValidationSeverity::Error
            });
            continue;
        }

        // Check for invalid control characters
        bool hasInvalidChars = false;
        for (char c : *translation.targetText) {
            if (c >= 0x00 && c <= 0x08) hasInvalidChars = true;
            if (c == 0x0B || c == 0x0C) hasInvalidChars = true;
            if (c >= 0x0E && c <= 0x1F) hasInvalidChars = true;
        }

        if (hasInvalidChars) {
            result.failedCount++;
            result.issues.push_back({
                translation.filePath,
                translation.entryKey.value_or(""),
                "Invalid control character in translation",
                ValidationSeverity::Error
            });
            continue;
        }

        result.passedCount++;
    }

    result.isValid = (result.failedCount == 0);
    return result;
}

// ============================================================================
// HANDLER UTILITIES
// ============================================================================

bool HandlerUtils::isLocalizationFileName(const std::string& name) {
    auto lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Localization keywords
    const std::vector<std::string> keywords = {
        "local", "lang", "text", "string", "dialog", "dialogue",
        "message", "i18n", "translation", "en.", "english", "tr.", "turkish",
        "localization", "language", "ui_", "ui-", "menu", "subtitle"
    };

    for (const auto& keyword : keywords) {
        if (lower.find(keyword) != std::string::npos) return true;
    }

    // Language codes
    const std::vector<std::string> langCodes = {
        "en", "tr", "de", "fr", "es", "it", "pt", "ru", "zh", "ja", "ko"
    };

    for (const auto& code : langCodes) {
        if (lower == code + ".json" || lower == code + ".xml" || lower == code + ".csv") {
            return true;
        }
        if (lower.find("_" + code + ".json") != std::string::npos ||
            lower.find("_" + code + ".xml") != std::string::npos ||
            lower.find("-" + code + ".json") != std::string::npos ||
            lower.find("-" + code + ".xml") != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::string HandlerUtils::guessContext(const std::string& path) {
    auto lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("dialog") != std::string::npos || lower.find("dialogue") != std::string::npos) {
        return "Diyalog";
    }
    if (lower.find("menu") != std::string::npos || lower.find("ui") != std::string::npos) {
        return "Menü/UI";
    }
    if (lower.find("item") != std::string::npos) {
        return "Eşya";
    }
    if (lower.find("skill") != std::string::npos || lower.find("ability") != std::string::npos) {
        return "Yetenek";
    }
    if (lower.find("quest") != std::string::npos || lower.find("mission") != std::string::npos) {
        return "Görev";
    }
    if (lower.find("npc") != std::string::npos || lower.find("character") != std::string::npos) {
        return "Karakter";
    }
    if (lower.find("tutorial") != std::string::npos || lower.find("help") != std::string::npos) {
        return "Yardım";
    }
    if (lower.find("error") != std::string::npos || lower.find("warning") != std::string::npos) {
        return "Sistem mesajı";
    }

    return "Genel";
}

EntryCategory HandlerUtils::guessCategory(const std::string& path) {
    auto lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("dialog") != std::string::npos || lower.find("dialogue") != std::string::npos ||
        lower.find("conversation") != std::string::npos) {
        return EntryCategory::Dialog;
    }
    if (lower.find("ui") != std::string::npos || lower.find("menu") != std::string::npos ||
        lower.find("button") != std::string::npos) {
        return EntryCategory::UI;
    }
    if (lower.find("item") != std::string::npos || lower.find("weapon") != std::string::npos ||
        lower.find("armor") != std::string::npos) {
        return EntryCategory::Item;
    }
    if (lower.find("skill") != std::string::npos || lower.find("ability") != std::string::npos ||
        lower.find("spell") != std::string::npos) {
        return EntryCategory::Skill;
    }
    if (lower.find("quest") != std::string::npos || lower.find("mission") != std::string::npos ||
        lower.find("objective") != std::string::npos) {
        return EntryCategory::Quest;
    }
    if (lower.find("tutorial") != std::string::npos || lower.find("help") != std::string::npos ||
        lower.find("tip") != std::string::npos) {
        return EntryCategory::Tutorial;
    }
    if (lower.find("system") != std::string::npos || lower.find("error") != std::string::npos ||
        lower.find("notification") != std::string::npos) {
        return EntryCategory::System;
    }

    return EntryCategory::Other;
}

bool HandlerUtils::isValidString(const std::string& text, const ExtractionOptions& options) {
    // Trim
    auto trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) return false;

    if (static_cast<int>(trimmed.length()) < options.minLength) return false;
    if (static_cast<int>(trimmed.length()) > options.maxLength) return false;

    // Only numbers/symbols
    if (std::regex_match(trimmed, std::regex(R"(^[\d\s\-\+\*\/\=\.\,\!\?\:\;\(\)\[\]\{\}\\\/]+$)"))) {
        return false;
    }

    // Paths
    if (std::regex_match(trimmed, std::regex(R"(^[A-Za-z]:\\)"))) return false;
    if (std::regex_match(trimmed, std::regex(R"(^https?://)"))) return false;

    // Code patterns
    if (std::regex_match(trimmed, std::regex(R"(^[a-zA-Z_][a-zA-Z0-9_]*\()"))) return false;

    return true;
}

std::string HandlerUtils::detectEncoding(const fs::path& file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return "utf-8";

    uint8_t bytes[4];
    ifs.read(reinterpret_cast<char*>(bytes), 4);
    auto count = ifs.gcount();

    if (count >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return "utf-8-bom";
    }
    if (count >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        return "utf-16le";
    }
    if (count >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        return "utf-16be";
    }

    return "utf-8";
}

std::string HandlerUtils::decodeXmlEntities(const std::string& text) {
    auto result = text;
    size_t pos;

    while ((pos = result.find("&amp;")) != std::string::npos)
        result.replace(pos, 5, "&");
    while ((pos = result.find("&lt;")) != std::string::npos)
        result.replace(pos, 4, "<");
    while ((pos = result.find("&gt;")) != std::string::npos)
        result.replace(pos, 4, ">");
    while ((pos = result.find("&quot;")) != std::string::npos)
        result.replace(pos, 6, "\"");
    while ((pos = result.find("&apos;")) != std::string::npos)
        result.replace(pos, 6, "'");
    while ((pos = result.find("&#10;")) != std::string::npos)
        result.replace(pos, 5, "\n");
    while ((pos = result.find("&#13;")) != std::string::npos)
        result.replace(pos, 5, "\r");
    while ((pos = result.find("&#9;")) != std::string::npos)
        result.replace(pos, 4, "\t");

    return result;
}

std::string HandlerUtils::encodeXmlEntities(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.1);

    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c;        break;
        }
    }

    return result;
}

std::vector<std::string> HandlerUtils::parseCsvLine(const std::string& line) {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                current += '"';
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            // Trim
            current.erase(0, current.find_first_not_of(" \t"));
            current.erase(current.find_last_not_of(" \t") + 1);
            result.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    // Last field
    current.erase(0, current.find_first_not_of(" \t"));
    current.erase(current.find_last_not_of(" \t") + 1);
    result.push_back(current);

    return result;
}

std::string HandlerUtils::escapeCsvField(const std::string& field) {
    if (field.find(',') != std::string::npos ||
        field.find('"') != std::string::npos ||
        field.find('\n') != std::string::npos) {

        std::string escaped = field;
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.insert(pos, "\"");
            pos += 2;
        }
        return "\"" + escaped + "\"";
    }
    return field;
}

std::vector<TranslationEntry> HandlerUtils::filterWithClassifier(
    const std::vector<TranslationEntry>& entries,
    float minConfidence)
{
    StringClassifier classifier;
    ClassifierConfig config;
    config.minConfidence = minConfidence;
    classifier.setConfig(config);

    return classifier.filterTranslatable(entries);
}

std::string HandlerUtils::getClassificationStats(
    const std::vector<TranslationEntry>& entries)
{
    StringClassifier classifier;
    auto stats = classifier.getStats(entries);

    std::ostringstream oss;
    oss << "Classification Statistics:\n"
        << "  Total: " << stats.total << "\n"
        << "  Translatable: " << stats.translatable << " (" << std::fixed << std::setprecision(1) << stats.translatablePercent() << "%)\n"
        << "  Dialogue: " << stats.dialogue << "\n"
        << "  UI Text: " << stats.uiText << "\n"
        << "  Item Names: " << stats.itemNames << "\n"
        << "  Descriptions: " << stats.descriptions << "\n"
        << "  Code: " << stats.code << "\n"
        << "  File Paths: " << stats.filePaths << "\n"
        << "  Identifiers: " << stats.identifiers << "\n"
        << "  Debug: " << stats.debug << "\n"
        << "  Garbage: " << stats.garbage << "\n"
        << "  Unknown: " << stats.unknown;

    return oss.str();
}

// ============================================================================
// ENGINE HANDLER FACTORY
// ============================================================================

EngineHandlerFactory& EngineHandlerFactory::instance() {
    static EngineHandlerFactory instance;
    return instance;
}

EngineHandlerFactory::EngineHandlerFactory() {
    // Register built-in handlers (order matters - most common first)
    registerHandler(std::make_unique<UnityHandler>());
    registerHandler(std::make_unique<RpgMakerHandler>());
    registerHandler(std::make_unique<UnrealHandler>());
    registerHandler(std::make_unique<RenpyHandler>());
    registerHandler(std::make_unique<GameMakerHandler>());
}

void EngineHandlerFactory::registerHandler(std::unique_ptr<IEngineHandler> handler) {
    handlers_.push_back(std::move(handler));
}

IEngineHandler* EngineHandlerFactory::getHandlerForGame(const fs::path& gameDir) {
    for (const auto& handler : handlers_) {
        if (handler->canHandleGame(gameDir)) {
            return handler.get();
        }
    }
    return nullptr;
}

IEngineHandler* EngineHandlerFactory::getHandler(const std::string& engineName) {
    for (const auto& handler : handlers_) {
        if (handler->engineName() == engineName) {
            return handler.get();
        }
    }
    return nullptr;
}

std::vector<std::string> EngineHandlerFactory::listHandlers() const {
    std::vector<std::string> names;
    for (const auto& handler : handlers_) {
        names.push_back(handler->engineName());
    }
    return names;
}

} // namespace makineai
