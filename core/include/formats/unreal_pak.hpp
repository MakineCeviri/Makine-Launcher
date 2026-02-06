/**
 * @file unreal_pak.hpp
 * @brief Unreal Engine PAK file format structures
 * @copyright (c) 2026 MakineAI Team
 *
 * Reference: Unreal Engine PAK format
 * Supports UE4 and UE5 pak files (versions 1-11)
 */

#pragma once

#include "../makineai/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace makineai::formats {

// PAK magic signature (at end of file)
constexpr uint32_t kPakMagic = 0x5A6F12E1;

/**
 * @brief PAK compression methods
 */
enum class PakCompression : uint32_t {
    None = 0,
    Zlib = 1,
    Gzip = 2,
    Oodle = 3,      // UE4.25+
    LZ4 = 4
};

/**
 * @brief PAK file version
 */
enum class PakVersion : int32_t {
    Initial = 1,
    NoTimestamps = 2,
    CompressionEncryption = 3,
    IndexEncryption = 4,
    RelativeChunkOffsets = 5,
    DeleteRecords = 6,
    EncryptionKeyGuid = 7,
    FNameBasedCompression = 8,   // UE4.23+
    FrozenIndex = 9,             // UE4.25+
    PathHashIndex = 10,          // UE4.26+
    Fnv64BugFix = 11             // UE5+
};

/**
 * @brief PAK file info structure (at end of file)
 */
struct PakInfo {
    uint32_t magic;
    int32_t version;
    uint64_t indexOffset;
    uint64_t indexSize;
    uint8_t indexHash[20];      // SHA1 hash
    bool encryptedIndex;
    uint8_t encryptionGuid[16]; // Version 7+
    uint32_t compressionMethods[5];

    bool isValid() const { return magic == kPakMagic; }
};

/**
 * @brief PAK entry (file in the archive)
 */
struct PakEntry {
    std::string filename;
    uint64_t offset;            // Position in pak file
    uint64_t size;              // Uncompressed size
    uint64_t compressedSize;
    PakCompression compression;
    uint8_t hash[20];           // SHA1 hash
    uint32_t compressionBlockSize;
    bool encrypted;
    bool deleted;               // Version 6+

    // Compression blocks for large files
    struct Block {
        uint64_t start;
        uint64_t end;
    };
    std::vector<Block> compressionBlocks;
};

/**
 * @brief PAK mount point
 */
struct PakMountPoint {
    std::string path;           // e.g., "../../../"
};

/**
 * @brief Full PAK file structure
 */
struct PakFile {
    PakInfo info;
    PakMountPoint mountPoint;
    std::vector<PakEntry> entries;

    // Index into entries by filename
    [[nodiscard]] const PakEntry* findEntry(const std::string& filename) const {
        for (const auto& entry : entries) {
            if (entry.filename == filename) return &entry;
        }
        return nullptr;
    }
};

/**
 * @brief Unreal localization text entry
 */
struct LocTextEntry {
    std::string key;            // Namespace/Key format
    std::string sourceString;
    std::string translation;
    std::string sourceHash;     // Hash of source for change detection
};

/**
 * @brief Unreal .locres file header
 */
struct LocResHeader {
    uint8_t magic[16];
    uint8_t version;
    uint64_t stringDataOffset;
    uint64_t stringCount;
};

/**
 * @brief Unreal string table (.uasset text)
 */
struct UAssetStringTable {
    std::string assetPath;
    std::vector<LocTextEntry> entries;
};

/**
 * @brief Common Unreal Engine paths for localization
 */
namespace LocPaths {
    constexpr std::string_view LocalizationPath = "Content/Localization/";
    constexpr std::string_view GameLocPath = "Content/Localization/Game/";
    constexpr std::string_view EngineLocPath = "Content/Localization/Engine/";
    constexpr std::string_view DialoguePath = "Content/Dialogue/";
    constexpr std::string_view StringTablePath = "Content/StringTables/";
}

/**
 * @brief Supported Unreal archive formats
 */
enum class UnrealArchiveFormat {
    Pak,        // Standard .pak
    IoStore,    // UE5 .utoc/.ucas
    Zen         // UE5.1+ Zen loader format
};

/**
 * @brief UE5 IoStore container info
 */
struct IoStoreInfo {
    std::string tocPath;        // .utoc file
    std::string casPath;        // .ucas file
    uint32_t version;
    uint64_t totalSize;
    std::vector<std::string> chunkIds;
};

} // namespace makineai::formats
