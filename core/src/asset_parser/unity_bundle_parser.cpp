/**
 * @file unity_bundle_parser.cpp
 * @brief Unity AssetBundle parser implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/core.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "formats/unity_bundle.hpp"

#include <lz4.h>
#include <fstream>

namespace makineai::parsers {

using namespace formats;

/**
 * @brief Unity AssetBundle (UnityFS) parser
 */
class UnityBundleParser : public IAssetFormatParser {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "Unity AssetBundle";
    }

    [[nodiscard]] StringList supportedExtensions() const override {
        return {".bundle", ".assets", ".resource", ".resS"};
    }

    [[nodiscard]] bool canParse(const fs::path& file) const override {
        if (!fs::exists(file) || !fs::is_regular_file(file)) {
            return false;
        }

        // Check extension
        auto ext = file.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext != ".bundle" && ext != ".assets") {
            // Check for CAB-* pattern (Unity bundle without extension)
            auto filename = file.filename().string();
            if (!filename.starts_with("CAB-")) {
                return false;
            }
        }

        // Check magic bytes
        std::ifstream stream(file, std::ios::binary);
        if (!stream) return false;

        char magic[8] = {0};
        stream.read(magic, 7);
        bool isUnity = std::string_view(magic) == kUnityFSMagic ||
                       std::string_view(magic) == kUnityWebMagic ||
                       std::string_view(magic) == kUnityRawMagic;

        if (isUnity) {
            MAKINEAI_LOG_DEBUG(log::PARSER, "Unity format detected: magic={}", std::string_view(magic));
        }

        return isUnity;
    }

    [[nodiscard]] Result<ParseResult> parse(const fs::path& file) const override {
        MAKINEAI_LOG_INFO(log::PARSER, "Starting Unity bundle parse: {}", file.filename().string());
        auto timer = Metrics::instance().timer("asset_parse_unity");

        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Unity_Mono;  // Will be refined

        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            MAKINEAI_LOG_ERROR(log::PARSER, "Cannot open Unity bundle: {}", file.string());
            Metrics::instance().increment("parse_failures_unity");
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file: " + file.string()));
        }

        // Read header
        auto headerResult = readHeader(stream);
        if (!headerResult) {
            MAKINEAI_LOG_ERROR(log::PARSER, "Failed to read Unity header: {}", headerResult.error().message());
            Metrics::instance().increment("parse_failures_unity");
            return std::unexpected(headerResult.error());
        }

        auto& header = *headerResult;
        result.formatVersion = std::to_string(header.formatVersion);
        result.metadata["unityVersion"] = header.unityVersion;
        MAKINEAI_LOG_DEBUG(log::PARSER, "Unity version: {}, format: {}", header.unityVersion, header.formatVersion);

        // Read blocks info
        auto blocksResult = readBlocksInfo(stream, header);
        if (!blocksResult) {
            MAKINEAI_LOG_WARN(log::PARSER, "Failed to read Unity blocks info: {}", blocksResult.error().message());
            Metrics::instance().increment("parse_failures_unity");
            return std::unexpected(blocksResult.error());
        }

        auto& [blocks, nodes] = *blocksResult;
        MAKINEAI_LOG_DEBUG(log::PARSER, "Unity bundle: {} blocks, {} nodes", blocks.size(), nodes.size());

        // Decompress and read data
        for (const auto& node : nodes) {
            if (node.isSerializedFile()) {
                auto stringsResult = extractStrings(stream, blocks, node, file);
                if (stringsResult) {
                    for (auto& entry : *stringsResult) {
                        result.strings.push_back(std::move(entry));
                    }
                }
            }
        }

        result.success = true;
        result.message = "Parsed " + std::to_string(result.strings.size()) + " strings";

        Metrics::instance().increment("assets_parsed_unity");
        MAKINEAI_LOG_INFO(log::PARSER, "Unity bundle parse complete: {} strings", result.strings.size());

        return result;
    }

    [[nodiscard]] VoidResult write(
        const fs::path& /*file*/,
        const std::vector<StringEntry>& /*strings*/
    ) const override {
        // Unity bundles should NOT be binary patched!
        // This is left as a stub - use RuntimeManager instead
        return std::unexpected(Error(ErrorCode::NotImplemented,
            "Unity bundles should not be binary patched. "
            "Use RuntimeManager for Unity game translations."));
    }

private:
    struct BlocksInfo {
        std::vector<UnityStorageBlock> blocks;
        std::vector<UnityNode> nodes;
    };

    [[nodiscard]] Result<UnityFSHeader> readHeader(std::ifstream& stream) const {
        UnityFSHeader header{};

        // Read signature
        stream.read(header.signature, 7);
        header.signature[7] = '\0';

        if (std::string_view(header.signature) != kUnityFSMagic) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Not a UnityFS file"));
        }

        // Read format version (big endian)
        uint32_t version;
        stream.read(reinterpret_cast<char*>(&version), 4);
        header.formatVersion = _byteswap_ulong(version);

        // Read version strings
        std::getline(stream, header.unityVersion, '\0');
        std::getline(stream, header.generatorVersion, '\0');

        // Read sizes (big endian)
        uint64_t totalSize;
        stream.read(reinterpret_cast<char*>(&totalSize), 8);
        header.totalSize = _byteswap_uint64(totalSize);

        uint32_t compressedSize, uncompressedSize;
        stream.read(reinterpret_cast<char*>(&compressedSize), 4);
        stream.read(reinterpret_cast<char*>(&uncompressedSize), 4);
        header.compressedBlocksInfoSize = _byteswap_ulong(compressedSize);
        header.uncompressedBlocksInfoSize = _byteswap_ulong(uncompressedSize);

        uint32_t flags;
        stream.read(reinterpret_cast<char*>(&flags), 4);
        header.flags = _byteswap_ulong(flags);

        return header;
    }

    [[nodiscard]] Result<BlocksInfo> readBlocksInfo(
        std::ifstream& stream,
        const UnityFSHeader& header
    ) const {
        BlocksInfo info;

        // Read compressed blocks info
        ByteBuffer compressedInfo(header.compressedBlocksInfoSize);
        stream.read(reinterpret_cast<char*>(compressedInfo.data()),
            header.compressedBlocksInfoSize);

        // Decompress if needed
        ByteBuffer blocksInfoData;
        auto compression = header.compressionType();

        if (compression == UnityCompression::None) {
            blocksInfoData = std::move(compressedInfo);
        } else if (compression == UnityCompression::LZ4 ||
                   compression == UnityCompression::LZ4HC) {
            blocksInfoData.resize(header.uncompressedBlocksInfoSize);
            int result = LZ4_decompress_safe(
                reinterpret_cast<const char*>(compressedInfo.data()),
                reinterpret_cast<char*>(blocksInfoData.data()),
                static_cast<int>(compressedInfo.size()),
                static_cast<int>(blocksInfoData.size())
            );
            if (result < 0) {
                return std::unexpected(Error(ErrorCode::DecompressionFailed,
                    "LZ4 decompression failed"));
            }
        } else {
            return std::unexpected(Error(ErrorCode::UnsupportedVersion,
                "Unsupported compression: " + std::to_string(static_cast<int>(compression))));
        }

        // Parse blocks info (big-endian format)
        size_t offset = 0;
        auto readBE32_buf = [&]() -> uint32_t {
            if (offset + 4 > blocksInfoData.size()) return 0;
            uint32_t val = (static_cast<uint32_t>(blocksInfoData[offset]) << 24) |
                           (static_cast<uint32_t>(blocksInfoData[offset+1]) << 16) |
                           (static_cast<uint32_t>(blocksInfoData[offset+2]) << 8) |
                           static_cast<uint32_t>(blocksInfoData[offset+3]);
            offset += 4;
            return val;
        };
        auto readBE16_buf = [&]() -> uint16_t {
            if (offset + 2 > blocksInfoData.size()) return 0;
            uint16_t val = static_cast<uint16_t>(
                (blocksInfoData[offset] << 8) | blocksInfoData[offset+1]);
            offset += 2;
            return val;
        };
        auto readBE64_buf = [&]() -> uint64_t {
            if (offset + 8 > blocksInfoData.size()) return 0;
            uint64_t hi = readBE32_buf();
            uint64_t lo = readBE32_buf();
            return (hi << 32) | lo;
        };

        // Skip hash
        offset += 16;

        // Read block count
        uint32_t blockCount = readBE32_buf();
        if (blockCount > 10000) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Invalid block count: " + std::to_string(blockCount)));
        }
        info.blocks.reserve(blockCount);

        for (uint32_t i = 0; i < blockCount; ++i) {
            UnityStorageBlock block;
            block.uncompressedSize = readBE32_buf();
            block.compressedSize = readBE32_buf();
            block.flags = readBE16_buf(); // 2 bytes, not 4!
            info.blocks.push_back(block);
        }

        // Read node count
        uint32_t nodeCount = readBE32_buf();
        if (nodeCount > 10000) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Invalid node count: " + std::to_string(nodeCount)));
        }
        info.nodes.reserve(nodeCount);

        for (uint32_t i = 0; i < nodeCount; ++i) {
            UnityNode node;
            node.offset = readBE64_buf();
            node.size = readBE64_buf();
            node.flags = readBE32_buf();

            // Read path string
            while (offset < blocksInfoData.size() && blocksInfoData[offset] != 0) {
                node.path += static_cast<char>(blocksInfoData[offset++]);
            }
            ++offset;  // Skip null terminator

            info.nodes.push_back(node);
        }

        return info;
    }

    [[nodiscard]] Result<std::vector<StringEntry>> extractStrings(
        std::ifstream& /*stream*/,
        const std::vector<UnityStorageBlock>& /*blocks*/,
        const UnityNode& /*node*/,
        const fs::path& /*sourcePath*/
    ) const {
        /**
         * Unity Serialized File Parsing - KNOWN LIMITATION
         *
         * Full implementation requires:
         * 1. Type tree parsing (variable based on Unity version)
         * 2. Object info table reading
         * 3. TextAsset object identification (classId = 49)
         * 4. String data extraction with proper encoding
         *
         * Unity's serialization format is complex and version-dependent.
         * For production use, we recommend:
         * - Using UnityHandler.extractStrings() for JSON/XML/IL2CPP extraction
         * - Using RuntimeManager with BepInEx/XUnity for runtime translation
         *
         * The handler approach is preferred because:
         * - It works with all Unity versions
         * - It doesn't require binary bundle modification
         * - Translations can be updated without game reinstall
         *
         * This parser can still identify bundle structure and metadata,
         * which is useful for game detection and analysis.
         */
        std::vector<StringEntry> strings;

        // Bundle structure is parsed - string extraction from serialized
        // files requires the handler approach or runtime translation
        MAKINEAI_LOG_DEBUG(log::PARSER, "Unity serialized file parsing: Use UnityHandler for string extraction");

        return strings;
    }
};

} // namespace makineai::parsers

// Factory function in makineai namespace (to match parsers_factory.hpp declaration)
namespace makineai {
std::unique_ptr<parsers::IAssetFormatParser> createUnityBundleParser() {
    return std::make_unique<parsers::UnityBundleParser>();
}
} // namespace makineai
