/**
 * @file unreal_handler.cpp
 * @brief Unreal Engine handler implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/handlers/unreal_handler.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstring>
#include <map>

namespace makineai {

// ============================================================================
// LOCRES PARSER
// ============================================================================

LocResParser::LocResParser(const std::vector<uint8_t>& data)
    : data_(data)
{
}

uint8_t LocResParser::readUint8() {
    if (offset_ >= data_.size()) return 0;
    return data_[offset_++];
}

int32_t LocResParser::readInt32() {
    if (offset_ + 4 > data_.size()) return 0;

    int32_t value = static_cast<int32_t>(data_[offset_]) |
                    (static_cast<int32_t>(data_[offset_ + 1]) << 8) |
                    (static_cast<int32_t>(data_[offset_ + 2]) << 16) |
                    (static_cast<int32_t>(data_[offset_ + 3]) << 24);
    offset_ += 4;
    return value;
}

uint32_t LocResParser::readUint32() {
    if (offset_ + 4 > data_.size()) return 0;

    uint32_t value = static_cast<uint32_t>(data_[offset_]) |
                     (static_cast<uint32_t>(data_[offset_ + 1]) << 8) |
                     (static_cast<uint32_t>(data_[offset_ + 2]) << 16) |
                     (static_cast<uint32_t>(data_[offset_ + 3]) << 24);
    offset_ += 4;
    return value;
}

int64_t LocResParser::readInt64() {
    if (offset_ + 8 > data_.size()) return 0;

    int64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= (static_cast<int64_t>(data_[offset_ + i]) << (i * 8));
    }
    offset_ += 8;
    return value;
}

std::string LocResParser::readFString() {
    int32_t length = readInt32();

    if (length == 0) return "";

    std::string result;

    if (length < 0) {
        // UTF-16 LE
        int32_t charCount = -length;
        if (offset_ + charCount * 2 > data_.size()) return "";

        std::vector<uint8_t> bytes(data_.begin() + offset_,
                                    data_.begin() + offset_ + charCount * 2);
        offset_ += charCount * 2;

        // Decode UTF-16 LE
        for (int32_t i = 0; i < charCount - 1; i++) {  // -1 for null terminator
            uint16_t charCode = bytes[i * 2] | (static_cast<uint16_t>(bytes[i * 2 + 1]) << 8);
            if (charCode == 0) break;

            // Simple UTF-16 to UTF-8 (BMP only)
            if (charCode < 0x80) {
                result += static_cast<char>(charCode);
            } else if (charCode < 0x800) {
                result += static_cast<char>(0xC0 | (charCode >> 6));
                result += static_cast<char>(0x80 | (charCode & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (charCode >> 12));
                result += static_cast<char>(0x80 | ((charCode >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (charCode & 0x3F));
            }
        }
    } else {
        // UTF-8/ASCII
        if (offset_ + length > data_.size()) return "";

        // Null terminator'ı çıkar
        result = std::string(reinterpret_cast<const char*>(&data_[offset_]), length - 1);
        offset_ += length;
    }

    return result;
}

LocResFile LocResParser::parse() {
    LocResFile result;

    try {
        // Magic number check
        uint32_t magic = readUint32();
        if (magic != LOCRES_MAGIC) {
            result.errorMessage = "Invalid LocRes magic number";
            return result;
        }

        // Version
        result.version = readUint8();
        if (result.version > 3) {
            result.errorMessage = "Unsupported LocRes version: " + std::to_string(result.version);
            return result;
        }

        // Localized string array (v1+)
        if (result.version >= 1) {
            int64_t stringCount = readInt64();
            result.localizedStrings.reserve(static_cast<size_t>(stringCount));

            for (int64_t i = 0; i < stringCount; i++) {
                result.localizedStrings.push_back(readFString());
            }
        }

        // Namespace count
        uint32_t namespaceCount = readUint32();

        for (uint32_t ns = 0; ns < namespaceCount; ns++) {
            // Namespace string (v2+ has string, earlier has hash)
            std::string nsName;
            if (result.version >= 2) {
                nsName = readFString();
            } else {
                uint32_t hash = readUint32();
                nsName = "NS_" + std::to_string(hash);
            }

            // Key count
            uint32_t keyCount = readUint32();

            for (uint32_t k = 0; k < keyCount; k++) {
                LocResEntry entry;
                entry.ns = nsName;

                // Key string
                if (result.version >= 2) {
                    entry.key = readFString();
                } else {
                    uint32_t keyHash = readUint32();
                    entry.key = "Key_" + std::to_string(keyHash);
                }

                // Source string hash
                entry.sourceHash = readUint32();

                // String value
                if (result.version >= 1 && !result.localizedStrings.empty()) {
                    int32_t stringIndex = readInt32();
                    if (stringIndex >= 0 &&
                        stringIndex < static_cast<int32_t>(result.localizedStrings.size())) {
                        entry.value = result.localizedStrings[stringIndex];
                    }
                } else {
                    entry.value = readFString();
                }

                result.entries.push_back(std::move(entry));
            }
        }

        result.valid = true;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Parse error: ") + e.what();
    }

    return result;
}

// ============================================================================
// LOCRES FILE SERIALIZATION
// ============================================================================

std::vector<uint8_t> LocResFile::toBytes() const {
    std::vector<uint8_t> buffer;

    auto writeUint8 = [&buffer](uint8_t value) {
        buffer.push_back(value);
    };

    auto writeUint32 = [&buffer](uint32_t value) {
        buffer.push_back(value & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 24) & 0xFF);
    };

    auto writeInt64 = [&buffer](int64_t value) {
        for (int i = 0; i < 8; i++) {
            buffer.push_back((value >> (i * 8)) & 0xFF);
        }
    };

    auto writeFString = [&buffer, &writeUint32](const std::string& text) {
        // Length (null terminator dahil)
        writeUint32(static_cast<uint32_t>(text.size() + 1));
        // String bytes
        buffer.insert(buffer.end(), text.begin(), text.end());
        // Null terminator
        buffer.push_back(0);
    };

    // Magic
    writeUint32(LOCRES_MAGIC);

    // Version (always write as v2 for best compatibility)
    writeUint8(2);

    // Localized strings (v1+ - write empty, use inline strings)
    writeInt64(0);

    // Group entries by namespace
    std::map<std::string, std::vector<const LocResEntry*>> namespaceMap;
    for (const auto& entry : entries) {
        namespaceMap[entry.ns].push_back(&entry);
    }

    // Namespace count
    writeUint32(static_cast<uint32_t>(namespaceMap.size()));

    for (const auto& [nsName, nsEntries] : namespaceMap) {
        // Namespace string
        writeFString(nsName);

        // Key count
        writeUint32(static_cast<uint32_t>(nsEntries.size()));

        for (const auto* entry : nsEntries) {
            // Key string
            writeFString(entry->key);

            // Source hash
            writeUint32(entry->sourceHash);

            // Value string (inline)
            writeFString(entry->value);
        }
    }

    return buffer;
}

// ============================================================================
// GAME DETECTION
// ============================================================================

bool UnrealHandler::canHandleGame(const fs::path& gameDir) {
    if (!fs::exists(gameDir)) return false;

    // Unreal Engine markers
    const std::vector<std::string> markers = {
        "Engine/Binaries",
        "Engine/Content",
        "Engine/Shaders",
        "Content/Paks",
        "Content/Movies"
    };

    for (const auto& marker : markers) {
        if (fs::exists(gameDir / marker)) {
            return true;
        }
    }

    // Shipping build - packed game
    for (const auto& entry : fs::directory_iterator(gameDir)) {
        if (entry.is_directory()) {
            // GameName/Content/Paks pattern
            if (fs::exists(entry.path() / "Content" / "Paks")) {
                return true;
            }

            // GameName/Binaries pattern
            if (fs::exists(entry.path() / "Binaries")) {
                return true;
            }
        }
    }

    // Search for .int files
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".int") {
                return true;
            }
        }
    }

    // Search for .pak files
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".pak") {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// FILE DISCOVERY
// ============================================================================

Result<std::vector<GameFile>> UnrealHandler::findGameFiles(const fs::path& gameDir) {
    std::vector<GameFile> files;

    // Localization directories
    const std::vector<std::string> locDirs = {
        "Localization",
        "Content/Localization",
        "Content/Localization/Game",
        "Content/Localization/Engine"
    };

    for (const auto& locDir : locDirs) {
        auto dir = gameDir / locDir;
        if (fs::exists(dir)) {
            scanLocalizationDir(dir, gameDir, files);
        }
    }

    // Root .int files
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".int") {
            auto relativePath = fs::relative(entry.path(), gameDir).string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            // Check if already added
            bool exists = false;
            for (const auto& f : files) {
                if (f.relativePath == relativePath) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                GameFile gf;
                gf.path = entry.path();
                gf.relativePath = relativePath;
                gf.type = GameFileType::Localization;
                gf.size = entry.file_size();
                gf.encoding = "utf-16le";
                files.push_back(std::move(gf));
            }
        }
    }

    // .locres files
    for (const auto& entry : fs::recursive_directory_iterator(gameDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".locres") {
            auto relativePath = fs::relative(entry.path(), gameDir).string();
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            GameFile gf;
            gf.path = entry.path();
            gf.relativePath = relativePath;
            gf.type = GameFileType::Localization;
            gf.size = entry.file_size();
            gf.encoding = "binary";
            files.push_back(std::move(gf));
        }
    }

    spdlog::debug("Unreal: Found {} localization files", files.size());
    return files;
}

void UnrealHandler::scanLocalizationDir(
    const fs::path& dir,
    const fs::path& gameDir,
    std::vector<GameFile>& files
) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".int" || ext == ".ini" || ext == ".txt") {
                auto relativePath = fs::relative(entry.path(), gameDir).string();
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

                GameFile gf;
                gf.path = entry.path();
                gf.relativePath = relativePath;
                gf.type = GameFileType::Localization;
                gf.size = entry.file_size();
                gf.encoding = detectLocFileEncoding(entry.path());
                files.push_back(std::move(gf));
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to scan localization dir {}: {}", dir.string(), e.what());
    }
}

std::string UnrealHandler::detectLocFileEncoding(const fs::path& file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) return "utf-8";

    uint8_t bytes[4];
    ifs.read(reinterpret_cast<char*>(bytes), 4);
    auto count = ifs.gcount();

    // UTF-16 LE BOM
    if (count >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        return "utf-16le";
    }

    // UTF-16 BE BOM
    if (count >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        return "utf-16be";
    }

    // UTF-8 BOM
    if (count >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return "utf-8-bom";
    }

    // Unreal .int files are usually UTF-16LE
    auto ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".int") {
        // Null byte check (UTF-16 indicator)
        if (count >= 2 && (bytes[1] == 0 || bytes[0] == 0)) {
            return bytes[1] == 0 ? "utf-16le" : "utf-16be";
        }
    }

    return "utf-8";
}

// ============================================================================
// STRING EXTRACTION
// ============================================================================

Result<ExtractionResult> UnrealHandler::extractStrings(
    const fs::path& gameDir,
    const ExtractionOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Unreal: Starting string extraction from {}", gameDir.string());
    auto timer = metrics().timer("unreal_extract_strings");

    ExtractionResult result;
    int filesProcessed = 0;

    auto filesResult = findGameFiles(gameDir);
    if (!filesResult) {
        MAKINEAI_LOG_ERROR(log::HANDLER, "Unreal: Failed to find game files: {}", filesResult.error().message());
        return std::unexpected(filesResult.error());
    }

    const auto& gameFiles = *filesResult;
    MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Found {} potential localization files", gameFiles.size());

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

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Processing file: {}", gameFile.relativePath);

        try {
            auto ext = gameFile.path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            ExtractionBatch batch;

            if (ext == ".locres") {
                batch = extractFromLocResFile(gameFile.path, gameFile, options);
            } else if (ext == ".int") {
                batch = extractFromIntFile(gameFile.path, gameFile, options);
            } else if (ext == ".ini" || ext == ".txt") {
                batch = extractFromIniFile(gameFile.path, gameFile, options);
            } else {
                MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Skipping unsupported extension: {}", ext);
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

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Extracted {} strings from {}",
                batch.entries.size(), gameFile.relativePath);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unreal: Extraction error in {}: {}",
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
    metrics().increment("unreal_files_processed", filesProcessed);
    metrics().increment("unreal_strings_extracted", result.entries.size());

    MAKINEAI_LOG_INFO(log::HANDLER, "Unreal: Extraction complete - {} strings from {} files in {}ms",
        result.entries.size(), filesProcessed, result.duration.count());

    return result;
}

// ============================================================================
// LOCRES EXTRACTION
// ============================================================================

UnrealHandler::ExtractionBatch UnrealHandler::extractFromLocResFile(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file, std::ios::binary);
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );

    LocResParser parser(bytes);
    auto locRes = parser.parse();

    if (!locRes.valid) {
        spdlog::warn("LocRes parse failed: {}", gameFile.relativePath);
        return batch;
    }

    spdlog::debug("LocRes v{}: {} entries ({})",
        locRes.version, locRes.entries.size(), gameFile.relativePath);

    for (const auto& entry : locRes.entries) {
        batch.total++;

        if (isValidString(entry.value, options)) {
            TranslationEntry te;
            te.filePath = gameFile.relativePath;
            te.entryKey = entry.fullKey();
            te.sourceText = entry.value;
            te.context = "Namespace: " + entry.ns;
            te.category = guessCategoryFromLocRes(entry.ns, entry.key);
            batch.entries.push_back(std::move(te));
        } else {
            batch.skipped++;
        }
    }

    return batch;
}

EntryCategory UnrealHandler::guessCategoryFromLocRes(const std::string& ns, const std::string& key) {
    auto nsLower = ns;
    auto keyLower = key;
    std::transform(nsLower.begin(), nsLower.end(), nsLower.begin(), ::tolower);
    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

    // Dialog/conversation
    if (nsLower.find("dialog") != std::string::npos ||
        nsLower.find("conversation") != std::string::npos ||
        nsLower.find("speech") != std::string::npos ||
        keyLower.find("_line") != std::string::npos ||
        keyLower.find("_say") != std::string::npos) {
        return EntryCategory::Dialog;
    }

    // UI elements
    if (nsLower.find("ui") != std::string::npos ||
        nsLower.find("hud") != std::string::npos ||
        nsLower.find("menu") != std::string::npos ||
        nsLower.find("widget") != std::string::npos ||
        keyLower.find("_btn") != std::string::npos ||
        keyLower.find("_button") != std::string::npos ||
        keyLower.find("_label") != std::string::npos) {
        return EntryCategory::UI;
    }

    // Items
    if (nsLower.find("item") != std::string::npos ||
        nsLower.find("inventory") != std::string::npos ||
        nsLower.find("equipment") != std::string::npos ||
        nsLower.find("weapon") != std::string::npos) {
        return EntryCategory::Item;
    }

    // Quests
    if (nsLower.find("quest") != std::string::npos ||
        nsLower.find("mission") != std::string::npos ||
        nsLower.find("objective") != std::string::npos ||
        nsLower.find("journal") != std::string::npos) {
        return EntryCategory::Quest;
    }

    // Skills
    if (nsLower.find("skill") != std::string::npos ||
        nsLower.find("ability") != std::string::npos ||
        nsLower.find("perk") != std::string::npos ||
        nsLower.find("talent") != std::string::npos) {
        return EntryCategory::Skill;
    }

    // Tutorials
    if (nsLower.find("tutorial") != std::string::npos ||
        nsLower.find("help") != std::string::npos ||
        nsLower.find("tip") != std::string::npos ||
        nsLower.find("hint") != std::string::npos) {
        return EntryCategory::Tutorial;
    }

    // System
    if (nsLower.find("system") != std::string::npos ||
        nsLower.find("error") != std::string::npos ||
        nsLower.find("warning") != std::string::npos ||
        nsLower.find("notification") != std::string::npos) {
        return EntryCategory::System;
    }

    return EntryCategory::Other;
}

// ============================================================================
// INT FILE EXTRACTION
// ============================================================================

std::string UnrealHandler::decodeUtf16Le(const std::vector<uint8_t>& bytes) {
    std::string result;

    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        uint16_t charCode = bytes[i] | (static_cast<uint16_t>(bytes[i + 1]) << 8);
        if (charCode == 0) continue;

        // Simple UTF-16 to UTF-8 (BMP only)
        if (charCode < 0x80) {
            result += static_cast<char>(charCode);
        } else if (charCode < 0x800) {
            result += static_cast<char>(0xC0 | (charCode >> 6));
            result += static_cast<char>(0x80 | (charCode & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (charCode >> 12));
            result += static_cast<char>(0x80 | ((charCode >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (charCode & 0x3F));
        }
    }

    return result;
}

std::vector<uint8_t> UnrealHandler::encodeUtf16Le(const std::string& text) {
    std::vector<uint8_t> bytes;

    // BOM
    bytes.push_back(0xFF);
    bytes.push_back(0xFE);

    // Simple UTF-8 to UTF-16 (BMP only)
    size_t i = 0;
    while (i < text.size()) {
        uint32_t codePoint = 0;
        uint8_t c = static_cast<uint8_t>(text[i]);

        if ((c & 0x80) == 0) {
            codePoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            codePoint = (c & 0x1F) << 6;
            if (i + 1 < text.size()) {
                codePoint |= (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            codePoint = (c & 0x0F) << 12;
            if (i + 1 < text.size()) {
                codePoint |= (static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6;
            }
            if (i + 2 < text.size()) {
                codePoint |= (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            }
            i += 3;
        } else {
            i += 1;
            continue;
        }

        // Write UTF-16 LE
        if (codePoint <= 0xFFFF) {
            bytes.push_back(codePoint & 0xFF);
            bytes.push_back((codePoint >> 8) & 0xFF);
        }
    }

    return bytes;
}

std::string UnrealHandler::unescapeString(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            switch (text[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += text[i]; break;
            }
        } else {
            result += text[i];
        }
    }

    return result;
}

std::string UnrealHandler::escapeString(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.2);

    for (char c : text) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }

    return result;
}

UnrealHandler::ExtractionBatch UnrealHandler::extractFromIntFile(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file, std::ios::binary);
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    std::string content;

    // UTF-16 LE BOM check
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        content = decodeUtf16Le(std::vector<uint8_t>(bytes.begin() + 2, bytes.end()));
    }
    // UTF-16 LE without BOM
    else if (bytes.size() >= 2 && bytes[1] == 0) {
        content = decodeUtf16Le(bytes);
    }
    // UTF-8
    else {
        content = std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }

    std::istringstream iss(content);
    std::string line;
    std::string currentSection = "General";
    int lineNumber = 0;

    while (std::getline(iss, line)) {
        lineNumber++;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        // Comment line
        if (line[0] == ';' || line[0] == '#' ||
            (line.size() >= 2 && line[0] == '/' && line[1] == '/')) {
            continue;
        }

        // Section header [SectionName]
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        // Key=Value pair
        auto equalsPos = line.find('=');
        if (equalsPos != std::string::npos && equalsPos > 0) {
            auto key = line.substr(0, equalsPos);
            auto value = line.substr(equalsPos + 1);

            // Trim
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            // Remove quotes
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            batch.total++;

            if (isValidString(value, options)) {
                TranslationEntry entry;
                entry.filePath = gameFile.relativePath;
                entry.entryKey = currentSection + "." + key;
                entry.sourceText = unescapeString(value);
                entry.context = "Section: " + currentSection;
                entry.category = guessCategory(currentSection, key);
                entry.lineNumber = lineNumber;
                batch.entries.push_back(std::move(entry));
            } else {
                batch.skipped++;
            }
        }
    }

    return batch;
}

// ============================================================================
// INI FILE EXTRACTION
// ============================================================================

UnrealHandler::ExtractionBatch UnrealHandler::extractFromIniFile(
    const fs::path& file,
    const GameFile& gameFile,
    const ExtractionOptions& options
) {
    ExtractionBatch batch;

    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Cannot open file");
    }

    std::string line;
    std::string currentSection = "General";
    int lineNumber = 0;

    while (std::getline(ifs, line)) {
        lineNumber++;

        // Trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        // Comment
        if (line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Section header
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        // Key=Value
        auto equalsPos = line.find('=');
        if (equalsPos != std::string::npos && equalsPos > 0) {
            auto key = line.substr(0, equalsPos);
            auto value = line.substr(equalsPos + 1);

            // Trim
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            // Remove quotes
            if ((value.size() >= 2 && value.front() == '"' && value.back() == '"') ||
                (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')) {
                value = value.substr(1, value.size() - 2);
            }

            batch.total++;

            if (isValidString(value, options)) {
                TranslationEntry entry;
                entry.filePath = gameFile.relativePath;
                entry.entryKey = currentSection + "." + key;
                entry.sourceText = value;
                entry.context = "Section: " + currentSection;
                entry.category = guessCategory(currentSection, key);
                entry.lineNumber = lineNumber;
                batch.entries.push_back(std::move(entry));
            } else {
                batch.skipped++;
            }
        }
    }

    return batch;
}

EntryCategory UnrealHandler::guessCategory(const std::string& section, const std::string& key) {
    auto sectionLower = section;
    auto keyLower = key;
    std::transform(sectionLower.begin(), sectionLower.end(), sectionLower.begin(), ::tolower);
    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

    if (sectionLower.find("dialog") != std::string::npos ||
        sectionLower.find("conversation") != std::string::npos) {
        return EntryCategory::Dialog;
    }
    if (sectionLower.find("ui") != std::string::npos ||
        sectionLower.find("menu") != std::string::npos ||
        sectionLower.find("hud") != std::string::npos) {
        return EntryCategory::UI;
    }
    if (sectionLower.find("item") != std::string::npos ||
        sectionLower.find("inventory") != std::string::npos) {
        return EntryCategory::Item;
    }
    if (sectionLower.find("quest") != std::string::npos ||
        sectionLower.find("mission") != std::string::npos ||
        sectionLower.find("objective") != std::string::npos) {
        return EntryCategory::Quest;
    }
    if (sectionLower.find("skill") != std::string::npos ||
        sectionLower.find("ability") != std::string::npos) {
        return EntryCategory::Skill;
    }
    if (sectionLower.find("tutorial") != std::string::npos ||
        sectionLower.find("help") != std::string::npos ||
        sectionLower.find("tip") != std::string::npos) {
        return EntryCategory::Tutorial;
    }

    if (keyLower.find("name") != std::string::npos ||
        keyLower.find("title") != std::string::npos) {
        return EntryCategory::UI;
    }
    if (keyLower.find("msg") != std::string::npos ||
        keyLower.find("message") != std::string::npos ||
        keyLower.find("text") != std::string::npos) {
        return EntryCategory::Dialog;
    }

    return EntryCategory::Other;
}

bool UnrealHandler::isValidString(const std::string& text, const ExtractionOptions& options) {
    auto trimmed = text;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

    if (trimmed.empty()) return false;

    if (static_cast<int>(trimmed.length()) < options.minLength) return false;
    if (static_cast<int>(trimmed.length()) > options.maxLength) return false;

    // Only numbers/symbols
    if (std::regex_match(trimmed, std::regex(R"(^[\d\s\-\+\*\/\=\.\,\!\?\:\;\(\)\[\]\{\}\\\/]+$)"))) {
        return false;
    }

    // File path
    if (std::regex_match(trimmed, std::regex(R"(^[A-Za-z]:[/\\])"))) return false;
    if (trimmed[0] == '/' && trimmed.find('.') != std::string::npos) return false;

    // URL
    if (std::regex_match(trimmed, std::regex(R"(^https?://)"))) return false;

    // Short identifier
    if (std::regex_match(trimmed, std::regex(R"(^[A-Za-z_][A-Za-z0-9_]*$)")) && trimmed.length() < 3) {
        return false;
    }

    // Hex color code
    if (std::regex_match(trimmed, std::regex(R"(^#[0-9A-Fa-f]{6,8}$)"))) return false;

    return true;
}

// ============================================================================
// TRANSLATION APPLICATION
// ============================================================================

Result<HandlerPatchResult> UnrealHandler::applyTranslations(
    const fs::path& gameDir,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    MAKINEAI_LOG_INFO(log::HANDLER, "Unreal: Starting translation application to {}", gameDir.string());
    auto timer = metrics().timer("unreal_apply_translations");

    HandlerPatchResult result;
    result.success = true;
    int filesPatched = 0;

    // Create backup
    if (options.createBackup) {
        auto backupId = options.backupId.empty()
            ? std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
            : options.backupId;

        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Creating backup with ID: {}", backupId);
        auto backupResult = createBackup(gameDir, backupId);
        if (!backupResult) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unreal: Backup creation failed");
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

    MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: {} translations grouped into {} files",
        translations.size(), translationsByFile.size());

    // Apply to each file
    for (const auto& [filePath, fileTranslations] : translationsByFile) {
        MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Patching file: {} ({} translations)",
            filePath, fileTranslations.size());

        auto fullPath = gameDir / filePath;
        if (!fs::exists(fullPath)) {
            MAKINEAI_LOG_WARN(log::HANDLER, "Unreal: File not found: {}", filePath);
            result.errors.push_back(PatchError{filePath, "File not found"});
            continue;
        }

        try {
            auto ext = fullPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            PatchBatch batch;

            if (ext == ".locres") {
                batch = applyToLocResFile(fullPath, fileTranslations, options);
            } else if (ext == ".int") {
                batch = applyToIntFile(fullPath, fileTranslations, options);
            } else if (ext == ".ini" || ext == ".txt") {
                batch = applyToIniFile(fullPath, fileTranslations, options);
            } else {
                MAKINEAI_LOG_WARN(log::HANDLER, "Unreal: Unsupported file type for patching: {}", ext);
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

            MAKINEAI_LOG_DEBUG(log::HANDLER, "Unreal: Patched {} - {} applied, {} skipped",
                filePath, batch.applied, batch.skipped);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::HANDLER, "Unreal: Patch error in {}: {}", filePath, e.what());
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
    metrics().increment("unreal_files_patched", filesPatched);
    metrics().increment("unreal_translations_applied", result.appliedCount);

    MAKINEAI_LOG_INFO(log::HANDLER, "Unreal: Translation complete - {} applied, {} skipped in {} files ({}ms)",
        result.appliedCount, result.skippedCount, filesPatched, result.duration.count());

    return result;
}

UnrealHandler::PatchBatch UnrealHandler::applyToLocResFile(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file, std::ios::binary);
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    LocResParser parser(bytes);
    auto locRes = parser.parse();

    if (!locRes.valid) {
        spdlog::warn("LocRes parse failed, cannot apply translations");
        batch.skipped = static_cast<int>(translations.size());
        return batch;
    }

    // Build translation map
    std::unordered_map<std::string, std::string> translationMap;
    for (const auto& t : translations) {
        if (t.targetText && t.entryKey && !t.entryKey->empty()) {
            translationMap[*t.entryKey] = *t.targetText;
        }
    }

    // Apply translations
    for (auto& entry : locRes.entries) {
        auto it = translationMap.find(entry.fullKey());
        if (it != translationMap.end()) {
            entry.value = it->second;
            batch.applied++;
        }
    }

    // Write back atomically
    if (batch.applied > 0) {
        auto newBytes = locRes.toBytes();
        fs::path tempPath = file.string() + ".makineai_tmp";

        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                batch.errors.push_back("Cannot create temp file");
                return batch;
            }

            ofs.write(reinterpret_cast<const char*>(newBytes.data()), newBytes.size());
            ofs.flush();

            if (!ofs.good()) {
                ofs.close();
                std::error_code ec;
                fs::remove(tempPath, ec);
                batch.errors.push_back("Write failed - possible disk full");
                return batch;
            }
        }

        std::error_code ec;
        fs::rename(tempPath, file, ec);
        if (ec) {
            fs::remove(tempPath, ec);
            batch.errors.push_back("Rename failed: " + ec.message());
            return batch;
        }
    }

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

UnrealHandler::PatchBatch UnrealHandler::applyToIntFile(
    const fs::path& file,
    const std::vector<TranslationEntry>& translations,
    const PatchOptions& options
) {
    PatchBatch batch;

    if (options.dryRun) {
        batch.applied = static_cast<int>(translations.size());
        return batch;
    }

    std::ifstream ifs(file, std::ios::binary);
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );
    ifs.close();

    std::string content;
    bool isUtf16 = false;

    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        content = decodeUtf16Le(std::vector<uint8_t>(bytes.begin() + 2, bytes.end()));
        isUtf16 = true;
    } else if (bytes.size() >= 2 && bytes[1] == 0) {
        content = decodeUtf16Le(bytes);
        isUtf16 = true;
    } else {
        content = std::string(reinterpret_cast<char*>(bytes.data()), bytes.size());
    }

    // Build translation map
    std::unordered_map<std::string, std::string> translationMap;
    for (const auto& t : translations) {
        if (t.targetText && t.entryKey && !t.entryKey->empty()) {
            translationMap[*t.entryKey] = *t.targetText;
        }
    }

    std::istringstream iss(content);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    std::string currentSection = "General";

    for (size_t i = 0; i < lines.size(); i++) {
        auto trimmed = lines[i];
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r") + 1);

        // Section change
        if (!trimmed.empty() && trimmed[0] == '[' && trimmed.back() == ']') {
            currentSection = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        // Key=Value
        auto equalsPos = trimmed.find('=');
        if (equalsPos != std::string::npos && equalsPos > 0) {
            auto key = trimmed.substr(0, equalsPos);
            key.erase(key.find_last_not_of(" \t") + 1);

            auto fullKey = currentSection + "." + key;

            auto it = translationMap.find(fullKey);
            if (it != translationMap.end()) {
                auto escapedValue = escapeString(it->second);

                // Preserve leading whitespace
                auto leading = lines[i].substr(0, lines[i].find_first_not_of(" \t"));
                lines[i] = leading + key + "=\"" + escapedValue + "\"";
                batch.applied++;
            }
        }
    }

    // Rebuild content
    std::string newContent;
    for (size_t i = 0; i < lines.size(); i++) {
        newContent += lines[i];
        if (i < lines.size() - 1) newContent += "\n";
    }

    // Write back atomically
    fs::path tempPath = file.string() + ".makineai_tmp";

    {
        std::ofstream ofs(tempPath, isUtf16 ? (std::ios::binary | std::ios::trunc) : std::ios::trunc);
        if (!ofs) {
            batch.errors.push_back("Cannot create temp file");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }

        if (isUtf16) {
            auto encoded = encodeUtf16Le(newContent);
            ofs.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        } else {
            ofs << newContent;
        }
        ofs.flush();

        if (!ofs.good()) {
            ofs.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            batch.errors.push_back("Write failed - possible disk full");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }
    }

    std::error_code ec;
    fs::rename(tempPath, file, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        batch.errors.push_back("Rename failed: " + ec.message());
    }

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

UnrealHandler::PatchBatch UnrealHandler::applyToIniFile(
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

    std::string currentSection = "General";

    for (size_t i = 0; i < lines.size(); i++) {
        auto trimmed = lines[i];
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r") + 1);

        // Section
        if (!trimmed.empty() && trimmed[0] == '[' && trimmed.back() == ']') {
            currentSection = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        // Key=Value
        auto equalsPos = trimmed.find('=');
        if (equalsPos != std::string::npos && equalsPos > 0) {
            auto key = trimmed.substr(0, equalsPos);
            key.erase(key.find_last_not_of(" \t") + 1);

            auto fullKey = currentSection + "." + key;

            auto it = translationMap.find(fullKey);
            if (it != translationMap.end()) {
                auto leading = lines[i].substr(0, lines[i].find_first_not_of(" \t"));
                lines[i] = leading + key + "=" + it->second;
                batch.applied++;
            }
        }
    }

    // Write back atomically
    fs::path tempPath = file.string() + ".makineai_tmp";

    {
        std::ofstream ofs(tempPath, std::ios::trunc);
        if (!ofs) {
            batch.errors.push_back("Cannot create temp file");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }

        for (size_t i = 0; i < lines.size(); i++) {
            ofs << lines[i];
            if (i < lines.size() - 1) ofs << "\n";
        }
        ofs.flush();

        if (!ofs.good()) {
            ofs.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            batch.errors.push_back("Write failed - possible disk full");
            batch.skipped = static_cast<int>(translations.size()) - batch.applied;
            return batch;
        }
    }

    std::error_code ec;
    fs::rename(tempPath, file, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        batch.errors.push_back("Rename failed: " + ec.message());
    }

    batch.skipped = static_cast<int>(translations.size()) - batch.applied;
    return batch;
}

// ============================================================================
// BACKUP/RESTORE
// ============================================================================

Result<HandlerBackupResult> UnrealHandler::createBackup(
    const fs::path& gameDir,
    const std::string& backupId,
    const std::vector<std::string>& specificFiles
) {
    HandlerBackupResult result;
    result.backupId = backupId;

    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        result.success = false;
        result.errorMessage = "Invalid backup ID";
        return result;
    }

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

Result<HandlerRestoreResult> UnrealHandler::restoreBackup(
    const fs::path& gameDir,
    const std::string& backupId
) {
    HandlerRestoreResult result;

    // Security check
    if (backupId.find("..") != std::string::npos ||
        backupId.find('/') != std::string::npos ||
        backupId.find('\\') != std::string::npos) {
        result.success = false;
        result.errorMessage = "Invalid backup ID";
        return result;
    }

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

Result<ValidationResult> UnrealHandler::validatePatch(
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

        // Check for unescaped quotes
        const auto& target = *translation.targetText;
        if (target.find('"') != std::string::npos &&
            target.find("\\\"") == std::string::npos) {
            result.issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Unescaped quote character",
                .severity = ValidationSeverity::Warning
            });
        }

        // Length check
        if (target.length() > translation.sourceText.length() * 2) {
            result.issues.push_back(ValidationIssue{
                .file = translation.filePath,
                .entryKey = translation.entryKey.value_or(""),
                .message = "Translation is more than 2x longer than original",
                .severity = ValidationSeverity::Warning
            });
        }

        result.passedCount++;
    }

    result.isValid = (result.failedCount == 0);
    return result;
}

} // namespace makineai
