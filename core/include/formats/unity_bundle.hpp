/**
 * @file unity_bundle.hpp
 * @brief Unity AssetBundle (UnityFS) format structures
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Reference: UnityFS format documentation
 * Supports Unity 5.x and later bundle formats with LZ4/LZMA compression
 */

#pragma once

#include "../makine/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace makine::formats {

// Magic signatures
constexpr char kUnityFSMagic[] = "UnityFS";
constexpr char kUnityWebMagic[] = "UnityWeb";
constexpr char kUnityRawMagic[] = "UnityRaw";
constexpr char kUnityArchiveMagic[] = "UnityArchive";

/**
 * @brief Compression types used in Unity bundles
 */
enum class UnityCompression : uint32_t {
    None = 0,
    LZMA = 1,
    LZ4 = 2,
    LZ4HC = 3,
    LZHAM = 4
};

/**
 * @brief UnityFS bundle header
 */
struct UnityFSHeader {
    char signature[8];          // "UnityFS\0"
    uint32_t formatVersion;     // Format version (6 or 7)
    std::string unityVersion;   // e.g., "5.x.x" or "2019.4.1f1"
    std::string generatorVersion;
    uint64_t totalSize;         // Total bundle file size
    uint32_t compressedBlocksInfoSize;
    uint32_t uncompressedBlocksInfoSize;
    uint32_t flags;

    bool isCompressed() const {
        return (flags & 0x3F) != 0;
    }

    UnityCompression compressionType() const {
        return static_cast<UnityCompression>(flags & 0x3F);
    }

    bool hasDirectoryInfo() const {
        return (flags & 0x40) != 0;
    }

    bool isBlocksInfoAtEnd() const {
        return (flags & 0x80) != 0;
    }
};

/**
 * @brief Storage block in UnityFS
 */
struct UnityStorageBlock {
    uint32_t uncompressedSize;
    uint32_t compressedSize;
    uint16_t flags;

    UnityCompression compression() const {
        return static_cast<UnityCompression>(flags & 0x3F);
    }
};

/**
 * @brief Node (file entry) in UnityFS
 */
struct UnityNode {
    uint64_t offset;
    uint64_t size;
    uint32_t flags;
    std::string path;

    bool isDirectory() const { return (flags & 0x4) != 0; }
    bool isSerializedFile() const { return (flags & 0x4) == 0; }
};

/**
 * @brief Unity serialized file header
 */
struct UnitySerializedHeader {
    uint32_t metadataSize;
    uint32_t fileSize;
    uint32_t version;       // Serialized file format version
    uint32_t dataOffset;
    uint8_t endianness;     // 0 = little, 1 = big
    uint8_t reserved[3];
};

/**
 * @brief Unity type tree node
 */
struct UnityTypeNode {
    int16_t version;
    uint8_t level;
    uint8_t typeFlags;
    uint32_t typeStringOffset;
    uint32_t nameStringOffset;
    int32_t size;
    int32_t index;
    int32_t flags;
    uint64_t refTypeHash;
};

/**
 * @brief Unity object info
 */
struct UnityObjectInfo {
    int64_t pathId;
    uint64_t byteStart;
    uint32_t byteSize;
    int32_t typeId;
    int16_t classId;
    int16_t scriptTypeIndex;
    uint8_t stripped;
};

/**
 * @brief Unity MonoBehaviour script reference
 */
struct UnityScriptRef {
    int32_t fileId;
    int64_t pathId;
    std::string className;
    std::string nameSpace;
    std::string assemblyName;
};

/**
 * @brief Text asset extracted from Unity
 */
struct UnityTextAsset {
    int64_t pathId;
    std::string name;
    std::string text;
    fs::path sourcePath;    // Bundle/asset file path
};

/**
 * @brief Font asset reference
 */
struct UnityFontAsset {
    int64_t pathId;
    std::string name;
    ByteBuffer fontData;
    fs::path sourcePath;
};

/**
 * @brief Parse result for Unity bundle
 */
struct UnityBundleData {
    UnityFSHeader header;
    std::vector<UnityStorageBlock> blocks;
    std::vector<UnityNode> nodes;
    std::vector<UnityTextAsset> textAssets;
    ByteBuffer rawData;
};

/**
 * @brief Common Unity asset class IDs
 */
namespace ClassId {
    constexpr int32_t Object = 0;
    constexpr int32_t GameObject = 1;
    constexpr int32_t Component = 2;
    constexpr int32_t Transform = 4;
    constexpr int32_t Behaviour = 8;
    constexpr int32_t MonoBehaviour = 114;
    constexpr int32_t TextAsset = 49;
    constexpr int32_t Font = 128;
    constexpr int32_t Texture2D = 28;
    constexpr int32_t Sprite = 213;
    constexpr int32_t AudioClip = 83;
    constexpr int32_t Material = 21;
    constexpr int32_t Shader = 48;
    constexpr int32_t Mesh = 43;
    constexpr int32_t AnimationClip = 74;
    constexpr int32_t AssetBundle = 142;
    constexpr int32_t SpriteAtlas = 687078895;
}

} // namespace makine::formats
