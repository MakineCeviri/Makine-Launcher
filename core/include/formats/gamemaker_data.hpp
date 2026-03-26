/**
 * @file gamemaker_data.hpp
 * @brief GameMaker data.win IFF format structures
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Reference: GameMaker Studio IFF/FORM chunk format
 * Used by: GameMaker Studio 1.x and 2.x games
 */

#pragma once

#include "../makine/types.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace makine::formats {

// FORM magic signature
constexpr uint32_t kFormMagic = 0x4D524F46; // "FORM"

/**
 * @brief GameMaker chunk types
 */
enum class GmChunkType : uint32_t {
    FORM = 0x4D524F46,  // Container chunk
    GEN8 = 0x384E4547,  // General info
    OPTN = 0x4E54504F,  // Options
    EXTN = 0x4E545845,  // Extensions
    SOND = 0x444E4F53,  // Sounds
    AGRP = 0x50524741,  // Audio groups
    SPRT = 0x54525053,  // Sprites
    BGND = 0x444E4742,  // Backgrounds
    PATH = 0x48544150,  // Paths
    SCPT = 0x54504353,  // Scripts
    SHDR = 0x52444853,  // Shaders
    FONT = 0x544E4F46,  // Fonts
    TMLN = 0x4E4C4D54,  // Timelines
    OBJT = 0x544A424F,  // Objects
    ROOM = 0x4D4F4F52,  // Rooms
    DAFL = 0x4C464144,  // Data files
    TPAG = 0x47415054,  // Texture pages
    CODE = 0x45444F43,  // Code/bytecode
    VARI = 0x49524156,  // Variables
    FUNC = 0x434E5546,  // Functions
    STRG = 0x47525453,  // Strings
    TXTR = 0x52545854,  // Textures
    AUDO = 0x4F445541,  // Audio
    LANG = 0x474E414C,  // Languages (GMS2)
    GLOB = 0x424F4C47,  // Global variables (GMS2)
    EMBI = 0x49424D45,  // Embedded images (GMS2)
    SEQN = 0x4E514553,  // Sequences (GMS2.3+)
    TAGS = 0x53474154,  // Tags (GMS2.3+)
    FEAT = 0x54414546,  // Features (GMS2022+)
    PSEM = 0x4D455350,  // Particle system (GMS2022+)
    PSYS = 0x53595350   // Particle system data (GMS2022+)
};

/**
 * @brief Chunk header
 */
struct GmChunkHeader {
    uint32_t name;      // Chunk type (FourCC)
    uint32_t size;      // Chunk size (not including header)
};

/**
 * @brief GEN8 general info chunk
 */
struct GmGen8Info {
    uint8_t isDebug;
    uint32_t byteCodeVersion;
    uint32_t filenameOffset;
    uint32_t configOffset;
    uint32_t lastObjectId;
    uint32_t lastTileId;
    uint32_t gameId;
    uint8_t guid[16];
    uint32_t nameOffset;
    uint32_t major;
    uint32_t minor;
    uint32_t release;
    uint32_t build;
    uint32_t defaultWindowWidth;
    uint32_t defaultWindowHeight;
    uint32_t infoFlags;
    uint32_t licenseHash;
    uint32_t licenseMD5[4];
    uint64_t timestamp;
    uint32_t displayNameOffset;
    uint64_t activeTargets;
    uint32_t functionClassifications;
    uint32_t steamAppId;
    uint32_t debuggerPort;

    // Parsed strings
    std::string filename;
    std::string config;
    std::string name;
    std::string displayName;

    bool isGMS2() const { return byteCodeVersion >= 14; }
    bool isGMS2_3() const { return byteCodeVersion >= 15; }
};

/**
 * @brief String entry in STRG chunk
 */
struct GmString {
    uint32_t index;         // String index
    std::string value;      // String content
    uint32_t offset;        // Offset in file (for patching)

    // Analysis flags
    bool isAssetName = false;   // spr_, obj_, rm_, etc.
    bool isCodeString = false;  // Appears in code
    bool isUIString = false;    // Likely UI text
};

/**
 * @brief Script entry
 */
struct GmScript {
    uint32_t index;
    std::string name;
    uint32_t codeIndex;
};

/**
 * @brief Object entry
 */
struct GmObject {
    uint32_t index;
    std::string name;
    int32_t spriteIndex;
    int32_t parentIndex;
    bool visible;
    bool solid;
    bool persistent;
    int32_t depth;
    int32_t maskIndex;
    // Events and actions omitted for brevity
};

/**
 * @brief Room entry
 */
struct GmRoom {
    uint32_t index;
    std::string name;
    std::string caption;        // Translatable room title
    uint32_t width;
    uint32_t height;
    uint32_t speed;
    bool persistent;
    uint32_t backgroundColor;
    bool drawBackgroundColor;
    uint32_t creationCodeId;
    // Instances, tiles, layers omitted for brevity
};

/**
 * @brief Language entry (GMS2)
 */
struct GmLanguage {
    std::string name;           // e.g., "English", "Turkish"
    std::string region;         // e.g., "en-US", "tr-TR"
    std::unordered_map<std::string, std::string> strings;
};

/**
 * @brief Full parsed GameMaker data
 */
struct GmDataFile {
    GmGen8Info info;
    std::vector<GmString> strings;
    std::vector<GmScript> scripts;
    std::vector<GmObject> objects;
    std::vector<GmRoom> rooms;
    std::vector<GmLanguage> languages;

    // Quick lookup
    std::unordered_map<std::string, size_t> stringIndex;

    [[nodiscard]] const GmString* findString(const std::string& value) const {
        auto it = stringIndex.find(value);
        if (it != stringIndex.end() && it->second < strings.size()) {
            return &strings[it->second];
        }
        return nullptr;
    }

    // Filter strings for translation
    [[nodiscard]] std::vector<const GmString*> getTranslatableStrings() const {
        std::vector<const GmString*> result;
        for (const auto& str : strings) {
            if (!str.isAssetName && !str.isCodeString && str.isUIString) {
                result.push_back(&str);
            }
        }
        return result;
    }
};

/**
 * @brief Asset name prefixes to filter out
 */
namespace AssetPrefixes {
    constexpr std::string_view kSprite = "spr_";
    constexpr std::string_view kObject = "obj_";
    constexpr std::string_view kRoom = "rm_";
    constexpr std::string_view kBackground = "bg_";
    constexpr std::string_view kSound = "snd_";
    constexpr std::string_view kFont = "fnt_";
    constexpr std::string_view kPath = "pth_";
    constexpr std::string_view kScript = "scr_";
    constexpr std::string_view kShader = "sh_";
    constexpr std::string_view kTimeline = "tl_";
    constexpr std::string_view kTileSet = "ts_";
    constexpr std::string_view kSequence = "seq_";
    constexpr std::string_view kParticle = "ps_";
    constexpr std::string_view kAnimCurve = "ac_";
}

/**
 * @brief Check if string looks like an asset name
 */
[[nodiscard]] inline bool isAssetName(std::string_view str) {
    using namespace AssetPrefixes;
    return str.starts_with(kSprite) || str.starts_with(kObject) ||
           str.starts_with(kRoom) || str.starts_with(kBackground) ||
           str.starts_with(kSound) || str.starts_with(kFont) ||
           str.starts_with(kPath) || str.starts_with(kScript) ||
           str.starts_with(kShader) || str.starts_with(kTimeline) ||
           str.starts_with(kTileSet) || str.starts_with(kSequence) ||
           str.starts_with(kParticle) || str.starts_with(kAnimCurve);
}

/**
 * @brief Check if string looks like code/internal
 */
[[nodiscard]] inline bool isCodeString(std::string_view str) {
    // Common code patterns
    if (str.empty() || str.length() < 2) return true;
    if (str.find("@@") != std::string_view::npos) return true;
    if (str.find("__") == 0) return true;
    if (str.find("gml_") == 0) return true;

    // All caps likely constant
    bool allCaps = true;
    for (char c : str) {
        if (std::isalpha(c) && !std::isupper(c)) {
            allCaps = false;
            break;
        }
    }
    if (allCaps && str.length() > 2) return true;

    return false;
}

} // namespace makine::formats
