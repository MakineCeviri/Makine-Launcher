/**
 * @file unity_bundle_patcher.hpp
 * @brief Unity AssetBundle patcher for translation package installation
 * @copyright (c) 2026 MakineAI Team
 *
 * Patches Unity bundle files by replacing serialized asset data.
 * Used for "unityPatch" install type packages (e.g. DREDGE).
 * .dat files in the package contain replacement serialized data
 * identified by CAB path and PathID.
 */

#pragma once

#include "makineai/error.hpp"
#include "makineai/types/common.hpp"
#include "formats/unity_bundle.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace makineai {

/**
 * @brief A single asset replacement entry parsed from a .dat file
 */
struct UnityPatchEntry {
    int64_t pathId{0};         // Object PathID within the SerializedFile
    std::string cabPath;       // CAB identifier (e.g. "CAB-a7aaff7e...")
    std::string assetName;     // Logical asset name (e.g. "Strings_en")
    ByteBuffer data;           // Replacement serialized data (.dat content)
};

/**
 * @brief Parsed .dat filename components
 */
struct DatFileInfo {
    std::string cabPath;       // "CAB-xxxxx"
    int64_t pathId{0};         // PathID extracted from filename
    std::string assetName;     // Asset name portion
};

/**
 * @brief Parse a .dat filename to extract CAB path, PathID, and asset name
 *
 * Format: "<assetName>-CAB-<32hexhash>-<pathId>.dat"
 * PathID can be negative (e.g. "--7525944118299420156").
 *
 * @param filename The .dat filename (without directory path)
 * @return Parsed components, or error if format is invalid
 */
Result<DatFileInfo> parseDatFilename(const std::string& filename);

/**
 * @brief Unity AssetBundle patcher
 *
 * Patches .bundle files by:
 * 1. Finding bundles in the game directory matching CAB paths
 * 2. Decompressing data blocks
 * 3. Parsing SerializedFile to locate objects by PathID
 * 4. Replacing object data
 * 5. Recompressing and rebuilding the bundle
 */
class UnityBundlePatcher {
public:
    /**
     * @brief Find bundle files containing the specified CAB paths
     * @param gameDir Root game directory to scan
     * @param cabPaths List of CAB identifiers to find
     * @return Map of cabPath -> bundle file path
     */
    Result<std::map<std::string, fs::path>> findBundles(
        const fs::path& gameDir,
        const std::vector<std::string>& cabPaths
    );

    /**
     * @brief Patch a single bundle file with replacement assets
     * @param bundlePath Path to the .bundle file
     * @param patches List of asset replacements to apply
     * @param progress Optional progress callback
     * @return Success or error
     */
    VoidResult patchBundle(
        const fs::path& bundlePath,
        const std::vector<UnityPatchEntry>& patches,
        ProgressCallback progress = nullptr
    );

private:
    // Object info parsed from a SerializedFile
    struct ObjectInfo {
        int64_t pathId{0};
        uint64_t byteStart{0};
        uint32_t byteSize{0};
        int32_t typeIndex{0};
    };

    // Parsed SerializedFile metadata
    struct SerializedFileInfo {
        uint64_t dataOffset{0};  // Offset to data section within SF
        uint32_t metadataSize{0};
        uint64_t fileSize{0};
        uint32_t version{0};
        uint8_t endianness{0};   // 0=LE, 1=BE
        std::vector<ObjectInfo> objects;
    };

    // Decompress all data blocks into a contiguous buffer
    Result<ByteBuffer> decompressDataBlocks(
        std::ifstream& stream,
        const std::vector<formats::UnityStorageBlock>& blocks,
        uint64_t dataOffset
    );

    // Parse SerializedFile header and object info table
    Result<SerializedFileInfo> parseSerializedFile(
        const ByteBuffer& data,
        uint64_t offset,
        uint64_t size
    );

    // Replace object data within decompressed SerializedFile buffer
    Result<ByteBuffer> replaceObjects(
        const ByteBuffer& sfData,
        uint64_t sfOffset,
        uint64_t sfSize,
        const SerializedFileInfo& sfInfo,
        const std::vector<UnityPatchEntry>& patches
    );

    // Recompress data and rebuild the bundle file
    VoidResult rebuildBundle(
        const fs::path& bundlePath,
        const formats::UnityFSHeader& header,
        const std::vector<formats::UnityStorageBlock>& origBlocks,
        const std::vector<formats::UnityNode>& nodes,
        const ByteBuffer& newData,
        uint64_t headerEndOffset
    );
};

} // namespace makineai
