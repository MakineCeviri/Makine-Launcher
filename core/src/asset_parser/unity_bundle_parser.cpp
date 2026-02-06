/**
 * @file unity_bundle_parser.cpp
 * @brief Unity AssetBundle parser implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/asset_parser.hpp"
#include "makineai/core.hpp"
#include "formats/unity_bundle.hpp"

#include <lz4.h>
#include <fstream>

namespace makineai {

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
        return std::string_view(magic) == kUnityFSMagic ||
               std::string_view(magic) == kUnityWebMagic ||
               std::string_view(magic) == kUnityRawMagic;
    }

    [[nodiscard]] Result<ParseResult> parse(const fs::path& file) const override {
        ParseResult result;
        result.success = false;
        result.detectedEngine = GameEngine::Unity_Mono;  // Will be refined

        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file: " + file.string()));
        }

        // Read header
        auto headerResult = readHeader(stream);
        if (!headerResult) {
            return std::unexpected(headerResult.error());
        }

        auto& header = *headerResult;
        result.formatVersion = std::to_string(header.formatVersion);
        result.metadata["unityVersion"] = header.unityVersion;

        // Read blocks info
        auto blocksResult = readBlocksInfo(stream, header);
        if (!blocksResult) {
            return std::unexpected(blocksResult.error());
        }

        auto& [blocks, nodes] = *blocksResult;

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

        // Parse blocks info
        size_t offset = 0;
        auto read32 = [&]() -> uint32_t {
            if (offset + 4 > blocksInfoData.size()) return 0;
            uint32_t val;
            std::memcpy(&val, &blocksInfoData[offset], 4);
            offset += 4;
            return val;
        };
        auto read64 = [&]() -> uint64_t {
            if (offset + 8 > blocksInfoData.size()) return 0;
            uint64_t val;
            std::memcpy(&val, &blocksInfoData[offset], 8);
            offset += 8;
            return val;
        };

        // Skip hash
        offset += 16;

        // Read block count
        uint32_t blockCount = read32();
        info.blocks.reserve(blockCount);

        for (uint32_t i = 0; i < blockCount; ++i) {
            UnityStorageBlock block;
            block.uncompressedSize = read32();
            block.compressedSize = read32();
            block.flags = static_cast<uint16_t>(read32() & 0xFFFF);
            info.blocks.push_back(block);
        }

        // Read node count
        uint32_t nodeCount = read32();
        info.nodes.reserve(nodeCount);

        for (uint32_t i = 0; i < nodeCount; ++i) {
            UnityNode node;
            node.offset = read64();
            node.size = read64();
            node.flags = read32();

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
        // Full implementation would parse serialized file and extract TextAssets
        // This is a simplified placeholder
        std::vector<StringEntry> strings;

        // TODO: Implement full serialized file parsing
        // - Read object info
        // - Find TextAsset objects (classId = 49)
        // - Extract text content

        return strings;
    }
};

// Factory function for registration
std::unique_ptr<IAssetFormatParser> createUnityBundleParser() {
    return std::make_unique<UnityBundleParser>();
}

} // namespace makineai
