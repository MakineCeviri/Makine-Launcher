/**
 * @file unreal_pak_parser.cpp
 * @brief Unreal Engine PAK format parser implementation
 *
 * Supports:
 * - UE4/UE5 PAK v8-v11
 * - .locres localization files
 *
 * Note: For Unreal games, we primarily use file replacement
 * rather than direct binary patching.
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "formats/unreal_pak.hpp"
#include <fstream>
#include <algorithm>

namespace makineai::parsers {

namespace {
    // PAK magic: 0x5A6F12E1
    constexpr uint32_t PAK_MAGIC = 0x5A6F12E1;
}

class UnrealPakParser : public IAssetFormatParser {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Unreal PAK";
    }

    [[nodiscard]] StringList supportedExtensions() const override {
        return {".pak", ".locres"};
    }

    [[nodiscard]] bool canParse(const fs::path& file) const override {
        if (!fs::exists(file)) return false;

        auto ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext != ".pak" && ext != ".locres") {
            return false;
        }

        // For .pak files, check magic at footer
        if (ext == ".pak") {
            std::ifstream fs(file, std::ios::binary);
            if (!fs) return false;

            fs.seekg(-44, std::ios::end);
            uint32_t magic;
            fs.read(reinterpret_cast<char*>(&magic), sizeof(magic));

            bool isPak = (magic == PAK_MAGIC);
            if (isPak) {
                MAKINEAI_LOG_DEBUG(log::PARSER, "Unreal PAK format detected: {}", file.filename().string());
            }
            return isPak;
        }

        MAKINEAI_LOG_DEBUG(log::PARSER, "Unreal LocRes format detected: {}", file.filename().string());
        return true; // .locres files
    }

    [[nodiscard]] Result<ParseResult> parse(const fs::path& file) const override {
        MAKINEAI_LOG_INFO(log::PARSER, "Starting Unreal asset parse: {}", file.filename().string());
        auto timer = Metrics::instance().timer("asset_parse_unreal");

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Unreal;

        try {
            auto ext = file.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".locres") {
                return parseLocRes(file);
            }

            // For PAK files, we don't extract strings directly
            // Unreal games use runtime translation or locres replacement
            result.success = true;
            result.message = "PAK file detected - use locres replacement for translation";
            result.metadata["format"] = "unreal_pak";

            Metrics::instance().increment("assets_parsed_unreal");
            MAKINEAI_LOG_INFO(log::PARSER, "Unreal PAK parse complete: {}", file.filename().string());

        } catch (const std::exception& e) {
            result.message = e.what();
            MAKINEAI_LOG_ERROR(log::PARSER, "Failed to parse PAK {}: {}", file.string(), e.what());
            Metrics::instance().increment("parse_failures_unreal");
        }

        return result;
    }

    [[nodiscard]] VoidResult write(
        const fs::path& file,
        const std::vector<StringEntry>& strings
    ) const override {
        // PAK writing is complex - we use file replacement instead
        MAKINEAI_LOG_WARN(log::PARSER, "Direct PAK writing not supported - use file replacement");
        return std::unexpected(Error(ErrorCode::NotSupported,
            "Use file replacement for Unreal PAK files"));
    }

private:
    [[nodiscard]] Result<ParseResult> parseLocRes(const fs::path& file) const {
        MAKINEAI_LOG_INFO(log::PARSER, "Parsing Unreal LocRes: {}", file.filename().string());

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Unreal;
        result.metadata["format"] = "unreal_locres";

        try {
            std::ifstream fs(file, std::ios::binary | std::ios::ate);
            if (!fs) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open locres file: {}", file.string());
                Metrics::instance().increment("parse_failures_unreal");
                return std::unexpected(Error(ErrorCode::FileNotFound,
                    "Cannot open locres file"));
            }

            auto fileSize = fs.tellg();
            fs.seekg(0);

            // LocRes magic: 0x0E14DAD9
            uint32_t magic;
            fs.read(reinterpret_cast<char*>(&magic), 4);

            if (magic != 0x0E14DAD9) {
                MAKINEAI_LOG_ERROR(log::PARSER, "Invalid LocRes magic: 0x{:08X}", magic);
                Metrics::instance().increment("parse_failures_unreal");
                return std::unexpected(Error(ErrorCode::InvalidFormat,
                    "Invalid LocRes magic"));
            }

            // Version
            uint8_t version;
            fs.read(reinterpret_cast<char*>(&version), 1);
            result.formatVersion = std::to_string(version);
            MAKINEAI_LOG_DEBUG(log::PARSER, "LocRes version: {}", version);

            // Skip localized string offset table
            int64_t stringTableOffset;
            fs.read(reinterpret_cast<char*>(&stringTableOffset), 8);

            // Namespace count
            uint32_t namespaceCount;
            fs.read(reinterpret_cast<char*>(&namespaceCount), 4);
            MAKINEAI_LOG_DEBUG(log::PARSER, "LocRes namespaces: {}", namespaceCount);

            for (uint32_t ns = 0; ns < namespaceCount && fs.good(); ns++) {
                // Namespace name
                std::string namespaceName = readFString(fs);

                // Key count
                uint32_t keyCount;
                fs.read(reinterpret_cast<char*>(&keyCount), 4);

                for (uint32_t k = 0; k < keyCount && fs.good(); k++) {
                    StringEntry entry;

                    // Key
                    std::string key = readFString(fs);
                    entry.key = namespaceName + "/" + key;

                    // Source string hash (skip)
                    uint32_t hash;
                    fs.read(reinterpret_cast<char*>(&hash), 4);

                    // String value
                    entry.offset = static_cast<uint64_t>(fs.tellg());
                    entry.original = readFString(fs);
                    entry.context = "Namespace: " + namespaceName;

                    if (!entry.original.empty()) {
                        result.strings.push_back(std::move(entry));
                    }
                }
            }

            result.success = true;
            result.message = "Parsed " + std::to_string(result.strings.size()) + " strings";

            Metrics::instance().increment("assets_parsed_unreal");
            MAKINEAI_LOG_INFO(log::PARSER, "Parsed LocRes: {} strings from {}",
                        result.strings.size(), file.filename().string());

        } catch (const std::exception& e) {
            MAKINEAI_LOG_ERROR(log::PARSER, "LocRes parse exception: {}", e.what());
            Metrics::instance().increment("parse_failures_unreal");
            return std::unexpected(Error(ErrorCode::ParseError, e.what()));
        }

        return result;
    }

    std::string readFString(std::ifstream& fs) const {
        int32_t length;
        fs.read(reinterpret_cast<char*>(&length), 4);

        if (length == 0) return "";

        bool isUnicode = length < 0;
        size_t charCount = isUnicode ? static_cast<size_t>(-length) : static_cast<size_t>(length);

        if (charCount > 0x100000) {
            // Sanity check
            return "";
        }

        std::string result;
        if (isUnicode) {
            std::vector<char16_t> wstr(charCount);
            fs.read(reinterpret_cast<char*>(wstr.data()), charCount * 2);

            // Convert UTF-16 to UTF-8 (simplified)
            for (char16_t c : wstr) {
                if (c == 0) break;
                if (c < 0x80) {
                    result += static_cast<char>(c);
                } else if (c < 0x800) {
                    result += static_cast<char>(0xC0 | (c >> 6));
                    result += static_cast<char>(0x80 | (c & 0x3F));
                } else {
                    result += static_cast<char>(0xE0 | (c >> 12));
                    result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (c & 0x3F));
                }
            }
        } else {
            result.resize(charCount);
            fs.read(result.data(), charCount);

            // Remove null terminator
            while (!result.empty() && result.back() == '\0') {
                result.pop_back();
            }
        }

        return result;
    }
};

} // namespace makineai::parsers

// Factory function in makineai namespace (to match parsers_factory.hpp declaration)
namespace makineai {
std::unique_ptr<parsers::IAssetFormatParser> createUnrealPakParser() {
    return std::make_unique<parsers::UnrealPakParser>();
}
} // namespace makineai
