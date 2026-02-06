/**
 * @file bethesda_ba2.hpp
 * @brief Bethesda BA2 archive format structures
 * @copyright (c) 2026 MakineAI Team
 *
 * Reference: Bethesda Softworks BA2 format
 * Used by: Fallout 4, Fallout 76, Starfield
 */

#pragma once

#include "../makineai/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace makineai::formats {

// BA2 magic signature
constexpr char kBa2Magic[] = "BTDX";
constexpr uint32_t kBa2MagicInt = 0x58445442; // "BTDX" as uint32

/**
 * @brief BA2 archive types
 */
enum class Ba2Type : uint32_t {
    General = 0x47454E4C,   // "GNRL" - General files
    DX10 = 0x44583130,      // "DX10" - DirectX textures
    GNMF = 0x474E4D46       // "GNMF" - PS4 textures (rare)
};

/**
 * @brief BA2 file header
 */
struct Ba2Header {
    char magic[4];              // "BTDX"
    uint32_t version;           // 1 = FO4, 2 = Starfield
    Ba2Type type;
    uint32_t numFiles;
    uint64_t nameTableOffset;

    bool isValid() const {
        return magic[0] == 'B' && magic[1] == 'T' &&
               magic[2] == 'D' && magic[3] == 'X';
    }

    bool isStarfield() const { return version >= 2; }
};

/**
 * @brief General file record
 */
struct Ba2FileRecordGeneral {
    uint32_t nameHash;          // Hash of filename
    char extension[4];          // File extension
    uint32_t dirHash;           // Hash of directory
    uint32_t flags;             // Unknown flags
    uint64_t offset;            // Data offset in file
    uint32_t packedSize;        // Compressed size (0 if not compressed)
    uint32_t unpackedSize;      // Original size
    uint32_t align;             // Alignment (usually 0xBAADF00D)
};

/**
 * @brief DX10 texture file record
 */
struct Ba2FileRecordDX10 {
    uint32_t nameHash;
    char extension[4];
    uint32_t dirHash;
    uint8_t unknownTex;
    uint8_t numChunks;
    uint16_t chunkHeaderSize;
    uint16_t height;
    uint16_t width;
    uint8_t numMips;
    uint8_t format;             // DXGI_FORMAT value
    uint16_t flags;
};

/**
 * @brief DX10 texture chunk
 */
struct Ba2TextureChunk {
    uint64_t offset;
    uint32_t packedSize;
    uint32_t unpackedSize;
    uint16_t startMip;
    uint16_t endMip;
    uint32_t align;
};

/**
 * @brief Full BA2 file entry (parsed)
 */
struct Ba2Entry {
    std::string fullPath;       // Full path including extension
    std::string directory;
    std::string filename;
    std::string extension;
    uint64_t offset;
    uint32_t packedSize;
    uint32_t unpackedSize;
    bool isCompressed;

    // For textures
    bool isTexture = false;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t mipCount = 0;
    uint8_t dxgiFormat = 0;
    std::vector<Ba2TextureChunk> textureChunks;
};

/**
 * @brief Full BA2 archive structure
 */
struct Ba2Archive {
    Ba2Header header;
    std::vector<Ba2Entry> entries;
    std::vector<std::string> nameTable;

    [[nodiscard]] const Ba2Entry* findEntry(const std::string& path) const {
        for (const auto& entry : entries) {
            if (entry.fullPath == path) return &entry;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<const Ba2Entry*> findByExtension(
        const std::string& ext
    ) const {
        std::vector<const Ba2Entry*> results;
        for (const auto& entry : entries) {
            if (entry.extension == ext) {
                results.push_back(&entry);
            }
        }
        return results;
    }
};

/**
 * @brief Starfield-specific structures
 */
namespace Starfield {

    /**
     * @brief Starfield string file (.strings, .dlstrings, .ilstrings)
     */
    struct StringFile {
        uint32_t count;
        uint32_t dataSize;

        struct Entry {
            uint32_t id;            // String ID
            std::string text;       // String content
        };
        std::vector<Entry> strings;
    };

    /**
     * @brief String file types
     */
    enum class StringType {
        Strings,        // .strings - General strings
        DLStrings,      // .dlstrings - Dialogue strings
        ILStrings       // .ilstrings - Interface strings
    };

    /**
     * @brief ESM/ESP plugin string entry
     */
    struct PluginString {
        uint32_t formId;        // Form ID (record ID)
        uint32_t stringId;      // String table ID
        std::string editorId;   // Editor ID (for reference)
        std::string text;       // Actual text
        std::string type;       // FULL, DESC, etc.
    };

    /**
     * @brief Common record types with translatable text
     */
    namespace RecordTypes {
        constexpr uint32_t BOOK = 0x4B4F4F42;   // Books
        constexpr uint32_t MESG = 0x4753454D;   // Messages
        constexpr uint32_t NOTE = 0x45544F4E;   // Notes
        constexpr uint32_t TERM = 0x4D524554;   // Terminals
        constexpr uint32_t DIAL = 0x4C414944;   // Dialogue
        constexpr uint32_t INFO = 0x4F464E49;   // Dialogue lines
        constexpr uint32_t QUST = 0x54535551;   // Quests
        constexpr uint32_t NPC_ = 0x5F43504E;   // NPCs
        constexpr uint32_t WEAP = 0x50414557;   // Weapons
        constexpr uint32_t ARMO = 0x4F4D5241;   // Armor
        constexpr uint32_t MISC = 0x4353494D;   // Misc items
        constexpr uint32_t ALCH = 0x48434C41;   // Consumables
    }
}

/**
 * @brief Fallout 4 specific structures
 */
namespace Fallout4 {
    // Similar to Starfield but version 1
    using StringFile = Starfield::StringFile;
    using PluginString = Starfield::PluginString;
}

} // namespace makineai::formats
