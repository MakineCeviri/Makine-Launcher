/**
 * @file gamemaker_data_parser.cpp
 * @brief GameMaker Studio data.win parser implementation
 *
 * Supports:
 * - GameMaker Studio 1.x
 * - GameMaker Studio 2.x
 * - GameMaker Studio 2.3+
 * - data.win, data.ios, data.droid
 * - IFF/FORM chunk format
 *
 * Key chunks for translation:
 * - STRG: String table
 * - LANG: Language strings
 * - FONT: Font definitions
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "formats/gamemaker_data.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace makineai::parsers {

namespace {
    // IFF magic: "FORM"
    constexpr uint32_t FORM_MAGIC = 0x4D524F46; // "FORM" little-endian

    // Chunk types
    constexpr uint32_t CHUNK_GEN8 = 0x384E4547; // "GEN8"
    constexpr uint32_t CHUNK_STRG = 0x47525453; // "STRG"
    constexpr uint32_t CHUNK_LANG = 0x474E414C; // "LANG"

    std::string chunkTypeToString(uint32_t type) {
        char str[5] = {0};
        std::memcpy(str, &type, 4);
        return std::string(str);
    }
}

class GameMakerDataParser : public IAssetFormatParser {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "GameMaker Data";
    }

    [[nodiscard]] StringList supportedExtensions() const override {
        return {".win", ".ios", ".droid", ".unx"};
    }

    [[nodiscard]] bool canParse(const fs::path& file) const override {
        if (!fs::exists(file)) return false;

        auto filename = file.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

        // Check for known GameMaker data files
        if (filename != "data.win" && filename != "data.ios" &&
            filename != "data.droid" && filename != "data.unx") {
            return false;
        }

        // Check FORM magic
        std::ifstream ifs(file, std::ios::binary);
        if (!ifs) return false;

        uint32_t magic;
        ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));

        bool isGameMaker = (magic == FORM_MAGIC);
        if (isGameMaker) {
            MAKINEAI_LOG_DEBUG(log::PARSER, "GameMaker IFF/FORM format detected: {}", file.filename().string());
        }

        return isGameMaker;
    }

    [[nodiscard]] Result<ParseResult> parse(const fs::path& file) const override {
        MAKINEAI_LOG_INFO(log::PARSER, "Starting GameMaker data parse: {}", file.filename().string());
        auto timer = Metrics::instance().timer("asset_parse_gamemaker");

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::GameMaker;
        result.metadata["format"] = "gamemaker_data";

        try {
            std::ifstream ifs(file, std::ios::binary | std::ios::ate);
            if (!ifs) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open GameMaker data file: {}", file.string());
                Metrics::instance().increment("parse_failures_gamemaker");
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Cannot open GameMaker data file"));
            }

            auto fileSize = static_cast<size_t>(ifs.tellg());
            ifs.seekg(0);

            // Read FORM header
            uint32_t formMagic, formSize;
            ifs.read(reinterpret_cast<char*>(&formMagic), 4);
            if (formMagic != FORM_MAGIC) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Invalid FORM header: 0x{:08X}", formMagic);
                Metrics::instance().increment("parse_failures_gamemaker");
                return std::unexpected(Error(ErrorCode::InvalidFormat,
                    "Invalid FORM header"));
            }
            ifs.read(reinterpret_cast<char*>(&formSize), 4);

            result.metadata["form_size"] = std::to_string(formSize);
            MAKINEAI_LOG_DEBUG(log::PARSER, "GameMaker FORM size: {} bytes", formSize);

            // Parse chunks
            while (static_cast<size_t>(ifs.tellg()) < fileSize) {
                uint32_t chunkType, chunkSize;
                if (!ifs.read(reinterpret_cast<char*>(&chunkType), 4)) break;
                if (!ifs.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

                auto chunkStart = static_cast<size_t>(ifs.tellg());
                auto chunkEnd = chunkStart + chunkSize;

                if (chunkType == CHUNK_GEN8) {
                    parseGen8Chunk(ifs, chunkSize, result);
                } else if (chunkType == CHUNK_STRG) {
                    parseStrgChunk(ifs, chunkStart, chunkSize, result);
                } else if (chunkType == CHUNK_LANG) {
                    parseLangChunk(ifs, chunkSize, result);
                }

                // Skip to next chunk
                ifs.seekg(chunkEnd);
            }

            result.success = true;
            result.message = "Parsed " + std::to_string(result.strings.size()) + " strings";

            Metrics::instance().increment("assets_parsed_gamemaker");
            MAKINEAI_LOG_INFO(log::PARSER, "Parsed GameMaker data: {} strings from {}",
                        result.strings.size(), file.filename().string());

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::PARSER, "GameMaker parse exception: {}", e.what());
            Metrics::instance().increment("parse_failures_gamemaker");
            return std::unexpected(Error(ErrorCode::ParseError, e.what()));
        }

        return result;
    }

    [[nodiscard]] VoidResult write(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const override {
        MAKINEAI_LOG_INFO(log::PARSER, "Writing GameMaker data: {} strings to {}", strings.size(), file.filename().string());

        try {
            // Read original file
            std::vector<uint8_t> fileData;
            {
                std::ifstream ifs(file, std::ios::binary | std::ios::ate);
                if (!ifs) {
                    MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open file for writing: {}", file.string());
                    return std::unexpected(Error(ErrorCode::FileNotFound,
                        "Cannot open file for writing"));
                }

                auto size = static_cast<size_t>(ifs.tellg());
                fileData.resize(size);
                ifs.seekg(0);
                ifs.read(reinterpret_cast<char*>(fileData.data()), size);
            }

            // Apply string modifications
            bool modified = false;
            int modifiedCount = 0;
            int skippedCount = 0;

            for (const auto& entry : strings) {
                if (entry.offset > 0 && entry.maxLength > 0) {
                    // CRITICAL: Bounds check before memcpy
                    if (static_cast<size_t>(entry.offset) + entry.maxLength > fileData.size()) {
                        MAKINEAI_LOG_WARN(log::PARSER, "String offset {} + length {} exceeds file size {}",
                                    entry.offset, entry.maxLength, fileData.size());
                        skippedCount++;
                        continue;
                    }

                    const auto& text = entry.translated.empty() ? entry.original : entry.translated;

                    if (text.length() <= entry.maxLength) {
                        // Write new string
                        std::memcpy(fileData.data() + entry.offset, text.data(), text.length());

                        // Pad with nulls if shorter
                        if (text.length() < entry.maxLength) {
                            std::memset(fileData.data() + entry.offset + text.length(), 0,
                                       entry.maxLength - text.length());
                        }
                        modified = true;
                        modifiedCount++;
                    } else {
                        MAKINEAI_LOG_WARN(log::PARSER, "String too long for key {}: {} > {}",
                                    entry.key, text.length(), entry.maxLength);
                        skippedCount++;
                    }
                }
            }

            if (!modified) {
                MAKINEAI_LOG_WARN(log::PARSER, "No strings were modified");
                return std::unexpected(Error(ErrorCode::InvalidArgument,
                    "No strings were modified"));
            }

            MAKINEAI_LOG_DEBUG(log::PARSER, "Modified {} strings, skipped {}", modifiedCount, skippedCount);

            // Write modified file atomically
            fs::path tempPath = file.string() + ".makineai_tmp";

            {
                std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
                if (!ofs) {
                    MAKINEAI_LOG_ERROR(log::PARSER, "Cannot create temp file for writing");
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot create temp file for writing"));
                }

                ofs.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
                ofs.flush();

                if (!ofs.good()) {
                    ofs.close();
                    std::error_code ec;
                    fs::remove(tempPath, ec);
                    MAKINEAI_LOG_ERROR(log::PARSER, "Write failed - possible disk full");
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Write failed - possible disk full"));
                }
            } // File closed

            // Atomic rename
            std::error_code ec;
            fs::rename(tempPath, file, ec);
            if (ec) {
                fs::remove(tempPath, ec);
                MAKINEAI_LOG_ERROR(log::PARSER, "Rename failed: {}", ec.message());
                return std::unexpected(Error(ErrorCode::IOError,
                    "Rename failed: " + ec.message()));
            }

            MAKINEAI_LOG_INFO(log::PARSER, "GameMaker data written successfully: {}", file.filename().string());

            return {};

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::PARSER, "GameMaker write exception: {}", e.what());
            return std::unexpected(Error(ErrorCode::Unknown, e.what()));
        }
    }

private:
    void parseGen8Chunk(std::ifstream& ifs, uint32_t size, ParseResult& result) const {
        auto startPos = ifs.tellg();

        // GEN8 contains game metadata
        uint8_t debugMode;
        ifs.read(reinterpret_cast<char*>(&debugMode), 1);

        // Skip to version info
        ifs.seekg(startPos + std::streamoff(48));
        uint32_t majorVersion, minorVersion, releaseVersion, buildVersion;
        ifs.read(reinterpret_cast<char*>(&majorVersion), 4);
        ifs.read(reinterpret_cast<char*>(&minorVersion), 4);
        ifs.read(reinterpret_cast<char*>(&releaseVersion), 4);
        ifs.read(reinterpret_cast<char*>(&buildVersion), 4);

        result.formatVersion = std::to_string(majorVersion) + "." +
                              std::to_string(minorVersion) + "." +
                              std::to_string(releaseVersion) + "." +
                              std::to_string(buildVersion);
        result.metadata["debug_mode"] = debugMode ? "true" : "false";

        MAKINEAI_LOG_DEBUG(log::PARSER, "GEN8: version {}", result.formatVersion);
    }

    void parseStrgChunk(std::ifstream& ifs, size_t chunkStart, uint32_t size,
                        ParseResult& result) const {
        // String count
        uint32_t stringCount;
        ifs.read(reinterpret_cast<char*>(&stringCount), 4);

        result.metadata["string_count"] = std::to_string(stringCount);

        if (stringCount > 1000000) { // Sanity check
            MAKINEAI_LOG_WARN(log::PARSER, "Excessive string count: {}", stringCount);
            return;
        }

        // Read string offsets
        std::vector<uint32_t> offsets(stringCount);
        for (uint32_t i = 0; i < stringCount; i++) {
            ifs.read(reinterpret_cast<char*>(&offsets[i]), 4);
        }

        // Read strings
        for (uint32_t i = 0; i < stringCount; i++) {
            ifs.seekg(chunkStart + offsets[i]);

            // String length (4 bytes before string data)
            uint32_t strLen;
            ifs.read(reinterpret_cast<char*>(&strLen), 4);

            if (strLen > 0 && strLen < 0x100000) { // Sanity check
                StringEntry entry;
                entry.key = "string_" + std::to_string(i);
                entry.offset = static_cast<uint64_t>(ifs.tellg());
                entry.maxLength = strLen;

                entry.original.resize(strLen);
                ifs.read(entry.original.data(), strLen);

                // Remove null terminator if present
                while (!entry.original.empty() && entry.original.back() == '\0') {
                    entry.original.pop_back();
                }

                entry.context = "GameMaker string index: " + std::to_string(i);

                if (!entry.original.empty()) {
                    result.strings.push_back(std::move(entry));
                }
            }
        }

        MAKINEAI_LOG_DEBUG(log::PARSER, "STRG: {} strings", result.strings.size());
    }

    void parseLangChunk(std::ifstream& ifs, uint32_t size, ParseResult& result) const {
        // Language chunk (newer GameMaker versions)
        uint32_t languageCount;
        ifs.read(reinterpret_cast<char*>(&languageCount), 4);

        result.metadata["language_count"] = std::to_string(languageCount);

        MAKINEAI_LOG_DEBUG(log::PARSER, "LANG: {} languages", languageCount);
    }
};

} // namespace makineai::parsers

// Factory function in makineai namespace (to match parsers_factory.hpp declaration)
namespace makineai {
std::unique_ptr<parsers::IAssetFormatParser> createGameMakerDataParser() {
    return std::make_unique<parsers::GameMakerDataParser>();
}
} // namespace makineai
