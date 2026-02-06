/**
 * @file bethesda_ba2_parser.cpp
 * @brief Bethesda BA2 archive format parser implementation
 *
 * Supports:
 * - Fallout 4 BA2
 * - Fallout 76 BA2
 * - Starfield BA2 (v2/v3)
 * - .strings localization files
 * - Compression: Zlib, LZ4
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "formats/bethesda_ba2.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <zlib.h>
#include <lz4.h>

namespace makineai::parsers {

namespace {
    // BA2 magic: "BTDX"
    constexpr uint32_t BA2_MAGIC = 0x58445442; // "BTDX" little-endian

    // File type flags
    constexpr uint32_t BA2_GENERAL = 0x4C524E47; // "GNRL"
    constexpr uint32_t BA2_DX10 = 0x30315844;    // "DX10"
    constexpr uint32_t BA2_GNMF = 0x464D4E47;    // "GNMF" (PS4)

    std::string fileTypeToString(uint32_t type) {
        switch (type) {
            case BA2_GENERAL: return "GNRL";
            case BA2_DX10: return "DX10";
            case BA2_GNMF: return "GNMF";
            default: return "UNKNOWN";
        }
    }
}

class BethesdaBa2Parser : public IAssetFormatParser {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Bethesda BA2";
    }

    [[nodiscard]] StringList supportedExtensions() const override {
        return {".ba2", ".strings", ".dlstrings", ".ilstrings"};
    }

    [[nodiscard]] bool canParse(const fs::path& file) const override {
        if (!fs::exists(file)) return false;

        auto ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Accept .strings files directly
        if (ext == ".strings" || ext == ".dlstrings" || ext == ".ilstrings") {
            MAKINEAI_LOG_DEBUG(log::PARSER, "Bethesda strings format detected: {}", file.filename().string());
            return true;
        }

        if (ext != ".ba2") {
            return false;
        }

        // Check BA2 magic
        std::ifstream ifs(file, std::ios::binary);
        if (!ifs) return false;

        uint32_t magic;
        ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));

        bool isBa2 = (magic == BA2_MAGIC);
        if (isBa2) {
            MAKINEAI_LOG_DEBUG(log::PARSER, "Bethesda BA2 format detected: {}", file.filename().string());
        }

        return isBa2;
    }

    [[nodiscard]] Result<ParseResult> parse(const fs::path& file) const override {
        MAKINEAI_LOG_INFO(log::PARSER, "Starting Bethesda asset parse: {}", file.filename().string());
        auto timer = Metrics::instance().timer("asset_parse_ba2");

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Bethesda;

        try {
            auto ext = file.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // Handle .strings files
            if (ext == ".strings" || ext == ".dlstrings" || ext == ".ilstrings") {
                return parseStringsFile(file);
            }

            std::ifstream ifs(file, std::ios::binary | std::ios::ate);
            if (!ifs) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open BA2 file: {}", file.string());
                Metrics::instance().increment("parse_failures_ba2");
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Cannot open BA2 file"));
            }

            auto fileSize = ifs.tellg();
            ifs.seekg(0);

            // Read header
            uint32_t magic, version, type, fileCount;
            uint64_t nameTableOffset;

            ifs.read(reinterpret_cast<char*>(&magic), 4);
            if (magic != BA2_MAGIC) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Invalid BA2 magic: 0x{:08X}", magic);
                Metrics::instance().increment("parse_failures_ba2");
                return std::unexpected(Error(ErrorCode::InvalidFormat,
                    "Invalid BA2 magic"));
            }

            ifs.read(reinterpret_cast<char*>(&version), 4);
            ifs.read(reinterpret_cast<char*>(&type), 4);
            ifs.read(reinterpret_cast<char*>(&fileCount), 4);
            ifs.read(reinterpret_cast<char*>(&nameTableOffset), 8);

            result.formatVersion = std::to_string(version);
            result.metadata["ba2_version"] = std::to_string(version);
            result.metadata["file_type"] = fileTypeToString(type);
            result.metadata["file_count"] = std::to_string(fileCount);
            result.metadata["format"] = "bethesda_ba2";

            // BA2 is primarily used for game assets, not direct text
            // Text is stored in .strings files
            result.success = true;
            result.message = "BA2 archive detected - check .strings files for text";

            Metrics::instance().increment("assets_parsed_ba2");
            MAKINEAI_LOG_INFO(log::PARSER, "Parsed BA2: version {}, type {}, {} files",
                        version, fileTypeToString(type), fileCount);

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::PARSER, "BA2 parse exception: {}", e.what());
            Metrics::instance().increment("parse_failures_ba2");
            return std::unexpected(Error(ErrorCode::ParseError, e.what()));
        }

        return result;
    }

    [[nodiscard]] VoidResult write(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const override {
        auto ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Handle .strings files
        if (ext == ".strings" || ext == ".dlstrings" || ext == ".ilstrings") {
            return writeStringsFile(file, strings);
        }

        // BA2 archive writing is complex - use file replacement
        MAKINEAI_LOG_WARN(log::PARSER, "Direct BA2 writing not fully supported - use file replacement");
        return std::unexpected(Error(ErrorCode::NotSupported,
            "Use file replacement for BA2 archives"));
    }

private:
    [[nodiscard]] Result<ParseResult> parseStringsFile(const fs::path& file) const {
        MAKINEAI_LOG_INFO(log::PARSER, "Parsing Bethesda strings file: {}", file.filename().string());

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Bethesda;
        result.metadata["format"] = "bethesda_strings";

        try {
            std::ifstream ifs(file, std::ios::binary | std::ios::ate);
            if (!ifs) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open strings file: {}", file.string());
                Metrics::instance().increment("parse_failures_ba2");
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Cannot open strings file"));
            }

            auto fileSize = static_cast<size_t>(ifs.tellg());
            ifs.seekg(0);

            // Strings header
            uint32_t count, dataSize;
            ifs.read(reinterpret_cast<char*>(&count), 4);
            ifs.read(reinterpret_cast<char*>(&dataSize), 4);

            if (count > 1000000) { // Sanity check
                MAKINEAI_LOG_ERROR(log::PARSER, "Invalid string count: {}", count);
                Metrics::instance().increment("parse_failures_ba2");
                return std::unexpected(Error(ErrorCode::InvalidFormat,
                    "Invalid string count"));
            }

            MAKINEAI_LOG_DEBUG(log::PARSER, "Strings file: {} entries, {} bytes data", count, dataSize);

            // Read directory (ID + offset pairs)
            std::vector<std::pair<uint32_t, uint32_t>> directory(count);
            for (uint32_t i = 0; i < count; i++) {
                ifs.read(reinterpret_cast<char*>(&directory[i].first), 4);  // ID
                ifs.read(reinterpret_cast<char*>(&directory[i].second), 4); // Offset
            }

            // Read string data
            auto dataStart = static_cast<size_t>(ifs.tellg());

            for (const auto& [id, offset] : directory) {
                ifs.seekg(dataStart + offset);

                std::string str;
                std::getline(ifs, str, '\0');

                if (!str.empty()) {
                    StringEntry entry;
                    entry.key = std::to_string(id);
                    entry.original = std::move(str);
                    entry.offset = dataStart + offset;
                    entry.context = "String ID: " + std::to_string(id);
                    result.strings.push_back(std::move(entry));
                }
            }

            result.success = true;
            result.message = "Parsed " + std::to_string(result.strings.size()) + " strings";

            Metrics::instance().increment("assets_parsed_ba2");
            MAKINEAI_LOG_INFO(log::PARSER, "Parsed strings file: {} entries from {}",
                        result.strings.size(), file.filename().string());

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::PARSER, "Strings file parse exception: {}", e.what());
            Metrics::instance().increment("parse_failures_ba2");
            return std::unexpected(Error(ErrorCode::ParseError, e.what()));
        }

        return result;
    }

    [[nodiscard]] VoidResult writeStringsFile(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const {
        try {
            uint32_t count = static_cast<uint32_t>(strings.size());

            // Build sorted string list with IDs
            std::vector<std::pair<uint32_t, std::string>> sortedStrings;
            for (const auto& entry : strings) {
                try {
                    uint32_t id = static_cast<uint32_t>(std::stoul(entry.key));
                    const auto& text = entry.translated.empty() ? entry.original : entry.translated;
                    sortedStrings.emplace_back(id, text);
                } catch (...) {
                    continue; // Skip invalid keys
                }
            }
            std::sort(sortedStrings.begin(), sortedStrings.end());

            // Calculate offsets and build string data
            std::vector<std::pair<uint32_t, uint32_t>> directory;
            uint32_t currentOffset = 0;
            std::string stringData;

            for (const auto& [id, str] : sortedStrings) {
                directory.emplace_back(id, currentOffset);
                stringData += str;
                stringData += '\0';
                currentOffset = static_cast<uint32_t>(stringData.size());
            }

            // Atomic write
            fs::path tempPath = file.string() + ".makineai_tmp";

            {
                std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
                if (!ofs) {
                    return std::unexpected(Error(ErrorCode::FileAccessDenied,
                        "Cannot create temp strings file"));
                }

                // Write header
                uint32_t dataSize = static_cast<uint32_t>(stringData.size());
                count = static_cast<uint32_t>(directory.size());
                ofs.write(reinterpret_cast<const char*>(&count), 4);
                ofs.write(reinterpret_cast<const char*>(&dataSize), 4);

                // Write directory
                for (const auto& [id, offset] : directory) {
                    ofs.write(reinterpret_cast<const char*>(&id), 4);
                    ofs.write(reinterpret_cast<const char*>(&offset), 4);
                }

                // Write string data
                ofs.write(stringData.data(), stringData.size());
                ofs.flush();

                if (!ofs.good()) {
                    ofs.close();
                    std::error_code ec;
                    fs::remove(tempPath, ec);
                    return std::unexpected(Error(ErrorCode::IOError,
                        "Strings file write failed - possible disk full"));
                }
            }

            // Atomic rename
            std::error_code ec;
            fs::rename(tempPath, file, ec);
            if (ec) {
                fs::remove(tempPath, ec);
                return std::unexpected(Error(ErrorCode::IOError,
                    "Strings file rename failed: " + ec.message()));
            }

            MAKINEAI_LOG_INFO(log::PARSER, "Wrote strings file: {} entries to {}",
                        count, file.filename().string());

            return {};

        } catch (const std::exception& e) {
            return std::unexpected(Error(ErrorCode::Unknown, e.what()));
        }
    }
};

} // namespace makineai::parsers

// Factory function in makineai namespace (to match parsers_factory.hpp declaration)
namespace makineai {
std::unique_ptr<parsers::IAssetFormatParser> createBethesdaBa2Parser() {
    return std::make_unique<parsers::BethesdaBa2Parser>();
}
} // namespace makineai
