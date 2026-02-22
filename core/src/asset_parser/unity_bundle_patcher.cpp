/**
 * @file unity_bundle_patcher.cpp
 * @brief Unity AssetBundle patcher implementation
 * @copyright (c) 2026 MakineAI Team
 *
 * Patches Unity .bundle files by replacing serialized asset data
 * identified by PathID. Supports LZ4/LZ4HC compressed bundles.
 */

#include "makineai/unity_bundle_patcher.hpp"
#include "makineai/logging.hpp"

#include <lz4.h>
#include <lz4hc.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>

namespace makineai {

using namespace formats;

// =============================================================================
// ENDIAN HELPERS
// =============================================================================

namespace {

inline uint16_t readBE16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

inline uint32_t readBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           static_cast<uint32_t>(p[3]);
}

inline uint64_t readBE64(const uint8_t* p) {
    return (static_cast<uint64_t>(readBE32(p)) << 32) | readBE32(p + 4);
}

inline void writeBE32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >>  8) & 0xFF);
    p[3] = static_cast<uint8_t>( v        & 0xFF);
}

inline void writeBE16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>( v       & 0xFF);
}

inline void writeBE64(uint8_t* p, uint64_t v) {
    writeBE32(p, static_cast<uint32_t>(v >> 32));
    writeBE32(p + 4, static_cast<uint32_t>(v & 0xFFFFFFFF));
}

// Big-endian buffer reader for blocks info parsing
// (Unity blocks info is always big-endian regardless of platform)
class BlocksInfoReader {
public:
    BlocksInfoReader(const uint8_t* data, size_t size)
        : data_(data), size_(size) {}

    size_t pos() const { return pos_; }
    bool hasBytes(size_t n) const { return pos_ + n <= size_; }
    void skip(size_t n) { pos_ += n; }

    uint16_t readU16() {
        if (!hasBytes(2)) return 0;
        uint16_t val = readBE16(data_ + pos_);
        pos_ += 2;
        return val;
    }

    uint32_t readU32() {
        if (!hasBytes(4)) return 0;
        uint32_t val = readBE32(data_ + pos_);
        pos_ += 4;
        return val;
    }

    uint64_t readU64() {
        if (!hasBytes(8)) return 0;
        uint64_t val = readBE64(data_ + pos_);
        pos_ += 8;
        return val;
    }

    std::string readNullTerminated() {
        std::string result;
        while (pos_ < size_ && data_[pos_] != 0) {
            result += static_cast<char>(data_[pos_++]);
        }
        if (pos_ < size_) pos_++; // skip null
        return result;
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_{0};
};

// Read from buffer with bounds checking
class BufferReader {
public:
    BufferReader(const uint8_t* data, size_t size, bool bigEndian = true)
        : data_(data), size_(size), bigEndian_(bigEndian) {}

    size_t pos() const { return pos_; }
    size_t remaining() const { return pos_ < size_ ? size_ - pos_ : 0; }
    bool hasBytes(size_t n) const { return pos_ + n <= size_; }

    void seek(size_t pos) { pos_ = pos; }
    void skip(size_t n) { pos_ += n; }

    void align(size_t alignment) {
        size_t rem = pos_ % alignment;
        if (rem != 0) pos_ += alignment - rem;
    }

    uint8_t readU8() {
        if (pos_ >= size_) return 0;
        return data_[pos_++];
    }

    int16_t readI16() {
        if (!hasBytes(2)) return 0;
        int16_t val;
        if (bigEndian_) {
            val = static_cast<int16_t>(readBE16(data_ + pos_));
        } else {
            std::memcpy(&val, data_ + pos_, 2);
        }
        pos_ += 2;
        return val;
    }

    uint32_t readU32() {
        if (!hasBytes(4)) return 0;
        uint32_t val;
        if (bigEndian_) {
            val = readBE32(data_ + pos_);
        } else {
            std::memcpy(&val, data_ + pos_, 4);
        }
        pos_ += 4;
        return val;
    }

    int32_t readI32() {
        return static_cast<int32_t>(readU32());
    }

    uint64_t readU64() {
        if (!hasBytes(8)) return 0;
        uint64_t val;
        if (bigEndian_) {
            val = readBE64(data_ + pos_);
        } else {
            std::memcpy(&val, data_ + pos_, 8);
        }
        pos_ += 8;
        return val;
    }

    int64_t readI64() {
        return static_cast<int64_t>(readU64());
    }

    std::string readNullTerminated() {
        std::string result;
        while (pos_ < size_ && data_[pos_] != 0) {
            result += static_cast<char>(data_[pos_++]);
        }
        if (pos_ < size_) pos_++; // skip null
        return result;
    }

    const uint8_t* ptr() const { return data_ + pos_; }

    void setEndian(bool big) { bigEndian_ = big; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_{0};
    bool bigEndian_;
};

} // anonymous namespace

// =============================================================================
// parseDatFilename
// =============================================================================

Result<DatFileInfo> parseDatFilename(const std::string& filename) {
    // Real format: "<assetName>-CAB-<32hexhash>-<pathId>.dat"
    // Examples:
    //   Strings_en-CAB-a7aaff7e46a74497fbaaad0364c220c4--7525944118299420156.dat
    //   Front Page Neue SDF-CAB-82a07709f640b59077f6a50d0fbebdff-2184995384792399705.dat
    //   Hahmlet-ExtraBold Atlas-CAB-3af0ea354f8f4657e00b014f9ff61c28-306740094964527751.dat
    // Note: PathID can be negative, so "-CAB-<hash>--<digits>" means pathId is negative

    // Strip .dat extension
    std::string name = filename;
    if (name.size() > 4 && name.substr(name.size() - 4) == ".dat") {
        name = name.substr(0, name.size() - 4);
    }

    // Find "-CAB-" to split asset name from CAB identifier
    auto cabPos = name.find("-CAB-");
    if (cabPos == std::string::npos) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "No '-CAB-' found in .dat filename: " + filename));
    }

    DatFileInfo info;
    info.assetName = name.substr(0, cabPos);

    // CAB path: "CAB-" + 32 hex chars
    constexpr size_t kCabHashLen = 32;
    size_t cabStart = cabPos + 1; // skip the leading '-'
    size_t hashEnd = cabStart + 4 + kCabHashLen; // "CAB-" + 32 hex

    if (hashEnd > name.size()) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "CAB hash too short in .dat filename: " + filename));
    }

    info.cabPath = name.substr(cabStart, 4 + kCabHashLen); // "CAB-<32hex>"

    // Verify hex chars
    for (size_t i = 4; i < info.cabPath.size(); ++i) {
        char c = info.cabPath[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Invalid hex in CAB hash: " + filename));
        }
    }

    // After the CAB hash, expect "-" then PathID
    if (hashEnd >= name.size() || name[hashEnd] != '-') {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Missing PathID separator in .dat filename: " + filename));
    }

    std::string pathIdStr = name.substr(hashEnd + 1);

    try {
        info.pathId = std::stoll(pathIdStr);
    } catch (...) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Invalid PathID in .dat filename: " + pathIdStr));
    }

    if (info.cabPath.empty() || info.assetName.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Empty CAB path or asset name in .dat filename: " + filename));
    }

    return info;
}

// =============================================================================
// findBundles
// =============================================================================

Result<std::map<std::string, fs::path>> UnityBundlePatcher::findBundles(
    const fs::path& gameDir,
    const std::vector<std::string>& cabPaths)
{
    if (!fs::exists(gameDir) || !fs::is_directory(gameDir)) {
        return std::unexpected(Error(ErrorCode::DirectoryNotFound,
            "Game directory not found: " + gameDir.string()));
    }

    std::map<std::string, fs::path> result;

    // Build a set of CAB paths we're looking for
    std::set<std::string> wanted(cabPaths.begin(), cabPaths.end());
    if (wanted.empty()) return result;

    MAKINEAI_LOG_INFO(log::PARSER, "Scanning for {} CAB paths in {}",
        wanted.size(), gameDir.string());

    // Recursively scan for files with UnityFS magic
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(gameDir, ec)) {
        if (!entry.is_regular_file()) continue;

        // Quick extension filter — Unity bundles are typically extensionless or .bundle
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!ext.empty() && ext != ".bundle" && ext != ".assets") continue;

        // Check file size (bundles should be at least 64 bytes)
        if (entry.file_size() < 64) continue;

        // Check UnityFS magic
        std::ifstream stream(entry.path(), std::ios::binary);
        if (!stream) continue;

        char magic[8] = {0};
        stream.read(magic, 7);
        if (std::string_view(magic) != kUnityFSMagic) continue;

        // Read header to get blocks info
        stream.seekg(0);
        UnityFSHeader header{};
        stream.read(header.signature, 7);
        header.signature[7] = '\0';

        uint32_t version;
        stream.read(reinterpret_cast<char*>(&version), 4);
        header.formatVersion = _byteswap_ulong(version);

        std::getline(stream, header.unityVersion, '\0');
        std::getline(stream, header.generatorVersion, '\0');

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

        if (!stream.good()) continue;

        // Handle blocks info at end of file (flag 0x80)
        if (header.isBlocksInfoAtEnd()) {
            stream.seekg(-static_cast<std::streamoff>(header.compressedBlocksInfoSize),
                std::ios::end);
        }

        // Read and decompress blocks info
        ByteBuffer compressedInfo(header.compressedBlocksInfoSize);
        stream.read(reinterpret_cast<char*>(compressedInfo.data()),
            header.compressedBlocksInfoSize);
        if (!stream.good()) continue;

        ByteBuffer blocksInfoData;
        auto compression = header.compressionType();
        if (compression == UnityCompression::None) {
            blocksInfoData = std::move(compressedInfo);
        } else if (compression == UnityCompression::LZ4 || compression == UnityCompression::LZ4HC) {
            blocksInfoData.resize(header.uncompressedBlocksInfoSize);
            int ret = LZ4_decompress_safe(
                reinterpret_cast<const char*>(compressedInfo.data()),
                reinterpret_cast<char*>(blocksInfoData.data()),
                static_cast<int>(compressedInfo.size()),
                static_cast<int>(blocksInfoData.size()));
            if (ret < 0) continue;
        } else {
            continue; // Skip LZMA bundles
        }

        // Parse nodes to extract CAB paths (blocks info is big-endian)
        BlocksInfoReader bir(blocksInfoData.data(), blocksInfoData.size());

        bir.skip(16); // skip hash
        uint32_t blockCount = bir.readU32();

        // Validate block count to prevent garbage reads
        if (blockCount > 10000) continue; // Sanity check

        for (uint32_t i = 0; i < blockCount; ++i) {
            bir.readU32(); // uncompressedSize
            bir.readU32(); // compressedSize
            bir.readU16(); // flags (2 bytes, not 4!)
        }

        uint32_t nodeCount = bir.readU32();
        if (nodeCount > 10000) continue; // Sanity check

        for (uint32_t i = 0; i < nodeCount; ++i) {
            bir.readU64(); // offset
            bir.readU64(); // size
            bir.readU32(); // flags

            std::string path = bir.readNullTerminated();

            // Check if this node matches a wanted CAB path
            if (wanted.count(path)) {
                result[path] = entry.path();
                wanted.erase(path);
                MAKINEAI_LOG_DEBUG(log::PARSER, "Found CAB {} in {}",
                    path, entry.path().string());
            }
        }

        if (wanted.empty()) break; // Found all
    }

    if (!wanted.empty()) {
        MAKINEAI_LOG_WARN(log::PARSER, "{} CAB paths not found in game directory",
            wanted.size());
    }

    return result;
}

// =============================================================================
// decompressDataBlocks
// =============================================================================

Result<ByteBuffer> UnityBundlePatcher::decompressDataBlocks(
    std::ifstream& stream,
    const std::vector<UnityStorageBlock>& blocks,
    uint64_t dataOffset)
{
    // Calculate total uncompressed size
    uint64_t totalSize = 0;
    for (const auto& block : blocks) {
        totalSize += block.uncompressedSize;
    }

    ByteBuffer result;
    result.reserve(static_cast<size_t>(totalSize));

    stream.seekg(static_cast<std::streamoff>(dataOffset));

    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        auto comp = block.compression();

        if (comp == UnityCompression::None) {
            size_t prevSize = result.size();
            result.resize(prevSize + block.uncompressedSize);
            stream.read(reinterpret_cast<char*>(result.data() + prevSize),
                block.uncompressedSize);
            if (!stream.good()) {
                return std::unexpected(Error(ErrorCode::IOError,
                    "Failed to read uncompressed block " + std::to_string(i)));
            }
        } else if (comp == UnityCompression::LZ4 || comp == UnityCompression::LZ4HC) {
            ByteBuffer compressed(block.compressedSize);
            stream.read(reinterpret_cast<char*>(compressed.data()),
                block.compressedSize);
            if (!stream.good()) {
                return std::unexpected(Error(ErrorCode::IOError,
                    "Failed to read compressed block " + std::to_string(i)));
            }

            size_t prevSize = result.size();
            result.resize(prevSize + block.uncompressedSize);
            int ret = LZ4_decompress_safe(
                reinterpret_cast<const char*>(compressed.data()),
                reinterpret_cast<char*>(result.data() + prevSize),
                static_cast<int>(block.compressedSize),
                static_cast<int>(block.uncompressedSize));

            if (ret < 0) {
                return std::unexpected(Error(ErrorCode::DecompressionFailed,
                    "LZ4 decompression failed for block " + std::to_string(i)));
            }
        } else {
            return std::unexpected(Error(ErrorCode::UnsupportedVersion,
                "Unsupported block compression: " + std::to_string(static_cast<int>(comp))));
        }
    }

    return result;
}

// =============================================================================
// parseSerializedFile
// =============================================================================

Result<UnityBundlePatcher::SerializedFileInfo> UnityBundlePatcher::parseSerializedFile(
    const ByteBuffer& data,
    uint64_t offset,
    uint64_t size)
{
    if (offset + size > data.size() || size < 20) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "SerializedFile region out of bounds"));
    }

    const uint8_t* base = data.data() + offset;
    BufferReader reader(base, static_cast<size_t>(size), true); // Start big-endian

    SerializedFileInfo info;

    // Header fields (always big-endian initially)
    info.metadataSize = reader.readU32();
    uint32_t fileSize32 = reader.readU32();
    info.version = reader.readU32();
    uint32_t dataOffset32 = reader.readU32();

    // Version >= 22 uses 64-bit sizes
    if (info.version >= 22) {
        // Re-read as 64-bit header
        reader.seek(0);
        info.metadataSize = reader.readU32();
        info.fileSize = reader.readU64();
        info.version = reader.readU32();
        info.dataOffset = reader.readU64();

        // Endianness and reserved
        info.endianness = reader.readU8();
        reader.skip(3); // reserved
    } else {
        info.fileSize = fileSize32;
        info.dataOffset = dataOffset32;

        // Endianness byte at offset 16
        info.endianness = reader.readU8();
        reader.skip(3); // reserved
    }

    // Switch to file's endianness for remaining fields
    bool bigEndian = (info.endianness != 0);
    reader.setEndian(bigEndian);

    // Skip Unity version string (null-terminated)
    if (info.version >= 7) {
        reader.readNullTerminated(); // unity version string
    }

    // Skip platform
    if (info.version >= 8) {
        reader.readU32(); // target platform
    }

    // Type tree
    if (info.version >= 13) {
        bool hasTypeTrees = (reader.readU8() != 0);
        uint32_t typeCount = reader.readU32();

        for (uint32_t t = 0; t < typeCount; ++t) {
            // Read type info
            int32_t classId;
            if (info.version >= 17) {
                classId = reader.readI32();
            } else {
                classId = reader.readI16();
                reader.skip(2); // padding
            }

            if (info.version >= 16) {
                reader.readU8(); // isStrippedType
            }

            if (info.version >= 17) {
                reader.readI16(); // scriptTypeIndex
            }

            // Hash
            if (info.version >= 13) {
                if ((info.version < 16 && classId < 0) ||
                    (info.version >= 16 && classId == 114)) {
                    reader.skip(32); // script hash (16) + type hash (16)
                } else {
                    reader.skip(16); // type hash
                }
            }

            // Type tree nodes
            if (hasTypeTrees) {
                if (info.version >= 12) {
                    // Blob type tree
                    uint32_t nodeCount = reader.readU32();
                    uint32_t stringBufferSize = reader.readU32();
                    reader.skip(static_cast<size_t>(nodeCount) * 24); // nodes
                    reader.skip(stringBufferSize);
                } else {
                    // Old recursive type tree - skip by reading recursively
                    // (rarely encountered in modern bundles)
                    std::function<void()> skipTypeTree = [&]() {
                        reader.readNullTerminated(); // type
                        reader.readNullTerminated(); // name
                        reader.readI32(); // size
                        reader.readI32(); // index
                        reader.readI32(); // isArray
                        reader.readI32(); // version
                        reader.readI32(); // flags
                        uint32_t childCount = reader.readU32();
                        for (uint32_t c = 0; c < childCount; ++c) {
                            skipTypeTree();
                        }
                    };
                    skipTypeTree();
                }
            }

            // Type dependencies (version >= 21)
            if (info.version >= 21) {
                uint32_t depCount = reader.readU32();
                reader.skip(static_cast<size_t>(depCount) * 4);
            }
        }
    }

    // Object info table
    uint32_t objectCount = reader.readU32();
    info.objects.reserve(objectCount);

    for (uint32_t i = 0; i < objectCount; ++i) {
        if (info.version >= 14) {
            reader.align(4);
        }

        ObjectInfo obj;

        if (info.version >= 14) {
            obj.pathId = reader.readI64();
        } else {
            obj.pathId = reader.readI32();
        }

        if (info.version >= 22) {
            obj.byteStart = reader.readU64();
        } else {
            obj.byteStart = reader.readU32();
        }

        obj.byteSize = reader.readU32();
        obj.typeIndex = reader.readI32();

        // Older versions have extra fields
        if (info.version < 17) {
            reader.readI16(); // classId
            reader.readI16(); // scriptTypeIndex
        }

        if (info.version == 15 || info.version == 16) {
            reader.readU8(); // stripped
        }

        info.objects.push_back(obj);
    }

    MAKINEAI_LOG_DEBUG(log::PARSER,
        "SerializedFile v{}: {} objects, dataOffset={}, fileSize={}",
        info.version, info.objects.size(), info.dataOffset, info.fileSize);

    return info;
}

// =============================================================================
// replaceObjects
// =============================================================================

Result<ByteBuffer> UnityBundlePatcher::replaceObjects(
    const ByteBuffer& sfData,
    uint64_t sfOffset,
    uint64_t sfSize,
    const SerializedFileInfo& sfInfo,
    const std::vector<UnityPatchEntry>& patches)
{
    // Work on a copy of the serialized file region
    ByteBuffer result(sfData.begin() + static_cast<ptrdiff_t>(sfOffset),
                      sfData.begin() + static_cast<ptrdiff_t>(sfOffset + sfSize));

    bool needsRebuild = false;

    // Check if any patch changes the object size
    for (const auto& patch : patches) {
        for (const auto& obj : sfInfo.objects) {
            if (obj.pathId == patch.pathId) {
                if (patch.data.size() != obj.byteSize) {
                    needsRebuild = true;
                    break;
                }
            }
        }
        if (needsRebuild) break;
    }

    if (!needsRebuild) {
        // Same-size replacement: just overwrite in place
        for (const auto& patch : patches) {
            for (const auto& obj : sfInfo.objects) {
                if (obj.pathId == patch.pathId) {
                    uint64_t writeOffset = sfInfo.dataOffset + obj.byteStart;
                    if (writeOffset + patch.data.size() > result.size()) {
                        return std::unexpected(Error(ErrorCode::InvalidOffset,
                            "Object data overflows SerializedFile boundary for PathID " +
                            std::to_string(patch.pathId)));
                    }
                    std::memcpy(result.data() + writeOffset,
                        patch.data.data(), patch.data.size());

                    MAKINEAI_LOG_DEBUG(log::PARSER,
                        "Replaced PathID {} (same size: {} bytes)",
                        patch.pathId, patch.data.size());
                    break;
                }
            }
        }
        return result;
    }

    // Different-size replacement: rebuild the data section
    MAKINEAI_LOG_INFO(log::PARSER,
        "Size mismatch detected, rebuilding SerializedFile data section");

    // Sort objects by byteStart for sequential rebuild
    auto sortedObjects = sfInfo.objects;
    std::sort(sortedObjects.begin(), sortedObjects.end(),
        [](const ObjectInfo& a, const ObjectInfo& b) {
            return a.byteStart < b.byteStart;
        });

    // Build a map of pathId -> replacement data
    std::map<int64_t, const ByteBuffer*> replacements;
    for (const auto& patch : patches) {
        replacements[patch.pathId] = &patch.data;
    }

    // Build new data section
    ByteBuffer newDataSection;
    newDataSection.reserve(result.size()); // rough estimate

    // Track updated object info (byteStart and byteSize)
    struct ObjectUpdate {
        int64_t pathId;
        uint64_t newByteStart;
        uint32_t newByteSize;
    };
    std::vector<ObjectUpdate> updates;

    uint64_t currentOffset = 0;
    for (const auto& obj : sortedObjects) {
        // Align to 8 bytes (Unity uses 8-byte alignment for objects in data section)
        size_t rem = currentOffset % 8;
        if (rem != 0) {
            size_t padding = 8 - rem;
            newDataSection.resize(newDataSection.size() + padding, 0);
            currentOffset += padding;
        }

        auto repIt = replacements.find(obj.pathId);
        if (repIt != replacements.end()) {
            // Use replacement data
            const ByteBuffer& repData = *repIt->second;
            updates.push_back({obj.pathId, currentOffset, static_cast<uint32_t>(repData.size())});
            newDataSection.insert(newDataSection.end(), repData.begin(), repData.end());
            currentOffset += repData.size();

            MAKINEAI_LOG_DEBUG(log::PARSER,
                "Replaced PathID {} ({} -> {} bytes)",
                obj.pathId, obj.byteSize, repData.size());
        } else {
            // Copy original data
            uint64_t srcOffset = sfInfo.dataOffset + obj.byteStart;
            if (srcOffset + obj.byteSize > result.size()) {
                return std::unexpected(Error(ErrorCode::InvalidOffset,
                    "Original object data out of bounds for PathID " +
                    std::to_string(obj.pathId)));
            }
            updates.push_back({obj.pathId, currentOffset, obj.byteSize});
            newDataSection.insert(newDataSection.end(),
                result.data() + srcOffset,
                result.data() + srcOffset + obj.byteSize);
            currentOffset += obj.byteSize;
        }
    }

    // Rebuild the full SerializedFile: metadata (header + type trees + object table) + new data
    // We keep the metadata portion and patch the object table entries in-place
    ByteBuffer newSF(result.begin(), result.begin() + static_cast<ptrdiff_t>(sfInfo.dataOffset));
    newSF.insert(newSF.end(), newDataSection.begin(), newDataSection.end());

    // Now patch the object table in the metadata section
    // We need to re-parse to find exact byte positions of each object entry
    // For simplicity, we scan for pathId values and update byteStart/byteSize

    // Update header fields
    bool bigEndian = (sfInfo.endianness != 0);

    if (sfInfo.version >= 22) {
        // 64-bit header: metadataSize(4) + fileSize(8) + version(4) + dataOffset(8)
        // metadataSize stays same (we didn't change metadata)
        uint64_t newFileSize = sfInfo.dataOffset + newDataSection.size();
        if (bigEndian) {
            writeBE64(newSF.data() + 4, newFileSize);
        } else {
            std::memcpy(newSF.data() + 4, &newFileSize, 8);
        }
    } else {
        // 32-bit header: metadataSize(4) + fileSize(4) + version(4) + dataOffset(4)
        auto newFileSize = static_cast<uint32_t>(sfInfo.dataOffset + newDataSection.size());
        if (bigEndian) {
            writeBE32(newSF.data() + 4, newFileSize);
        } else {
            std::memcpy(newSF.data() + 4, &newFileSize, 4);
        }
    }

    // Update object entries in the metadata
    // Re-parse to find their positions, then patch byteStart/byteSize
    {
        BufferReader patcher(newSF.data(), newSF.size(), bigEndian);

        // Skip header
        if (sfInfo.version >= 22) {
            patcher.skip(4 + 8 + 4 + 8 + 1 + 3); // metadataSize + fileSize + version + dataOffset + endian + reserved
        } else {
            patcher.skip(4 + 4 + 4 + 4 + 1 + 3);
        }

        // Skip unity version string
        if (sfInfo.version >= 7) {
            patcher.readNullTerminated();
        }

        // Skip platform
        if (sfInfo.version >= 8) {
            patcher.readU32();
        }

        // Skip type tree
        if (sfInfo.version >= 13) {
            bool hasTypeTrees = (patcher.readU8() != 0);
            uint32_t typeCount = patcher.readU32();

            for (uint32_t t = 0; t < typeCount; ++t) {
                if (sfInfo.version >= 17) {
                    int32_t classId = patcher.readI32();
                    (void)classId;
                } else {
                    patcher.readI16();
                    patcher.skip(2);
                }

                if (sfInfo.version >= 16) patcher.readU8();
                int16_t scriptTypeIndex = 0;
                if (sfInfo.version >= 17) scriptTypeIndex = patcher.readI16();

                if (sfInfo.version >= 13) {
                    bool hasScriptHash = (sfInfo.version < 16 && scriptTypeIndex < 0) ||
                                         (sfInfo.version >= 16 && patcher.readU8() == 114);
                    // Actually re-read: the classId check above consumed the byte
                    // This is getting complex - let's use a simpler approach
                    (void)hasScriptHash;
                }

                // Since re-parsing type trees exactly is complex and error-prone,
                // we'll use a different strategy: scan for object entries directly
                break; // Abort this approach
            }
        }

        // Alternative approach: find object entries by scanning
        // Since we know the object count and the format, we can search for
        // the pattern of pathId values in sequence
    }

    // Simpler approach: binary search for each pathId in the metadata region
    // and patch the byteStart/byteSize that follow it
    for (const auto& update : updates) {
        // Find this pathId in the original metadata (before dataOffset)
        // PathIDs are stored as int64 (version >= 14), aligned to 4 bytes
        bool found = false;

        for (size_t searchPos = 0; searchPos + 8 <= sfInfo.dataOffset; searchPos++) {
            int64_t candidate;
            if (bigEndian) {
                candidate = static_cast<int64_t>(readBE64(newSF.data() + searchPos));
            } else {
                std::memcpy(&candidate, newSF.data() + searchPos, 8);
            }

            if (candidate != update.pathId) continue;

            // Verify this is a valid object entry by checking if it's followed
            // by plausible byteStart and byteSize values
            size_t afterPathId = searchPos + 8;

            if (sfInfo.version >= 22) {
                // byteStart is uint64 + byteSize is uint32
                if (afterPathId + 12 > sfInfo.dataOffset) continue;

                // Check if the original byteStart/byteSize match any known object
                uint64_t origStart;
                uint32_t origSize;
                if (bigEndian) {
                    origStart = readBE64(newSF.data() + afterPathId);
                    origSize = readBE32(newSF.data() + afterPathId + 8);
                } else {
                    std::memcpy(&origStart, newSF.data() + afterPathId, 8);
                    std::memcpy(&origSize, newSF.data() + afterPathId + 8, 4);
                }

                // Verify against known objects
                bool isValidEntry = false;
                for (const auto& obj : sfInfo.objects) {
                    if (obj.pathId == update.pathId &&
                        obj.byteStart == origStart &&
                        obj.byteSize == origSize) {
                        isValidEntry = true;
                        break;
                    }
                }

                if (!isValidEntry) continue;

                // Patch byteStart and byteSize
                if (bigEndian) {
                    writeBE64(newSF.data() + afterPathId, update.newByteStart);
                    writeBE32(newSF.data() + afterPathId + 8, update.newByteSize);
                } else {
                    std::memcpy(newSF.data() + afterPathId, &update.newByteStart, 8);
                    std::memcpy(newSF.data() + afterPathId + 8, &update.newByteSize, 4);
                }
                found = true;
                break;
            } else {
                // byteStart is uint32 + byteSize is uint32
                if (afterPathId + 8 > sfInfo.dataOffset) continue;

                uint32_t origStart, origSize;
                if (bigEndian) {
                    origStart = readBE32(newSF.data() + afterPathId);
                    origSize = readBE32(newSF.data() + afterPathId + 4);
                } else {
                    std::memcpy(&origStart, newSF.data() + afterPathId, 4);
                    std::memcpy(&origSize, newSF.data() + afterPathId + 4, 4);
                }

                bool isValidEntry = false;
                for (const auto& obj : sfInfo.objects) {
                    if (obj.pathId == update.pathId &&
                        obj.byteStart == origStart &&
                        obj.byteSize == origSize) {
                        isValidEntry = true;
                        break;
                    }
                }

                if (!isValidEntry) continue;

                auto newStart32 = static_cast<uint32_t>(update.newByteStart);
                if (bigEndian) {
                    writeBE32(newSF.data() + afterPathId, newStart32);
                    writeBE32(newSF.data() + afterPathId + 4, update.newByteSize);
                } else {
                    std::memcpy(newSF.data() + afterPathId, &newStart32, 4);
                    std::memcpy(newSF.data() + afterPathId + 4, &update.newByteSize, 4);
                }
                found = true;
                break;
            }
        }

        if (!found) {
            MAKINEAI_LOG_WARN(log::PARSER,
                "Could not locate object entry for PathID {} in metadata", update.pathId);
        }
    }

    return newSF;
}

// =============================================================================
// rebuildBundle
// =============================================================================

VoidResult UnityBundlePatcher::rebuildBundle(
    const fs::path& bundlePath,
    const UnityFSHeader& header,
    const std::vector<UnityStorageBlock>& origBlocks,
    const std::vector<UnityNode>& nodes,
    const ByteBuffer& newData,
    uint64_t headerEndOffset)
{
    // Split new data into blocks matching original sizes
    std::vector<ByteBuffer> compressedBlocks;
    uint64_t dataPos = 0;
    std::vector<UnityStorageBlock> newBlocks;
    newBlocks.reserve(origBlocks.size());

    for (const auto& origBlock : origBlocks) {
        uint32_t blockUncompSize = origBlock.uncompressedSize;

        // Handle case where new data might be larger/smaller
        uint32_t actualSize = static_cast<uint32_t>(
            std::min<uint64_t>(blockUncompSize, newData.size() - dataPos));

        if (dataPos >= newData.size()) {
            // Pad with zeros if we ran out of data (shouldn't happen in normal case)
            ByteBuffer zeros(blockUncompSize, 0);
            compressedBlocks.push_back(std::move(zeros));

            UnityStorageBlock newBlock;
            newBlock.uncompressedSize = blockUncompSize;
            newBlock.compressedSize = blockUncompSize;
            newBlock.flags = 0; // No compression for padding blocks
            newBlocks.push_back(newBlock);
            dataPos += blockUncompSize;
            continue;
        }

        const uint8_t* blockData = newData.data() + dataPos;

        auto comp = origBlock.compression();
        if (comp == UnityCompression::LZ4 || comp == UnityCompression::LZ4HC) {
            // Compress with LZ4HC (better ratio than LZ4)
            int maxCompSize = LZ4_compressBound(static_cast<int>(actualSize));
            ByteBuffer compressed(static_cast<size_t>(maxCompSize));

            int compressedSize = LZ4_compress_HC(
                reinterpret_cast<const char*>(blockData),
                reinterpret_cast<char*>(compressed.data()),
                static_cast<int>(actualSize),
                maxCompSize,
                LZ4HC_CLEVEL_DEFAULT);

            if (compressedSize <= 0) {
                return std::unexpected(Error(ErrorCode::CompressionFailed,
                    "LZ4HC compression failed for block"));
            }

            compressed.resize(static_cast<size_t>(compressedSize));
            compressedBlocks.push_back(std::move(compressed));

            UnityStorageBlock newBlock;
            newBlock.uncompressedSize = actualSize;
            newBlock.compressedSize = static_cast<uint32_t>(compressedSize);
            newBlock.flags = origBlock.flags; // Preserve compression flag
            newBlocks.push_back(newBlock);
        } else {
            // No compression
            ByteBuffer uncompressed(blockData, blockData + actualSize);
            compressedBlocks.push_back(std::move(uncompressed));

            UnityStorageBlock newBlock;
            newBlock.uncompressedSize = actualSize;
            newBlock.compressedSize = actualSize;
            newBlock.flags = 0;
            newBlocks.push_back(newBlock);
        }

        dataPos += blockUncompSize;
    }

    // If new data is larger, add extra blocks
    while (dataPos < newData.size()) {
        uint32_t remaining = static_cast<uint32_t>(
            std::min<uint64_t>(newData.size() - dataPos, 128 * 1024)); // 128KB blocks

        const uint8_t* blockData = newData.data() + dataPos;

        // Use LZ4HC for extra blocks if original used compression
        bool useCompression = !origBlocks.empty() &&
            (origBlocks[0].compression() == UnityCompression::LZ4 ||
             origBlocks[0].compression() == UnityCompression::LZ4HC);

        if (useCompression) {
            int maxCompSize = LZ4_compressBound(static_cast<int>(remaining));
            ByteBuffer compressed(static_cast<size_t>(maxCompSize));
            int compressedSize = LZ4_compress_HC(
                reinterpret_cast<const char*>(blockData),
                reinterpret_cast<char*>(compressed.data()),
                static_cast<int>(remaining), maxCompSize, LZ4HC_CLEVEL_DEFAULT);

            if (compressedSize <= 0) {
                return std::unexpected(Error(ErrorCode::CompressionFailed,
                    "LZ4HC compression failed for extra block"));
            }

            compressed.resize(static_cast<size_t>(compressedSize));

            UnityStorageBlock newBlock;
            newBlock.uncompressedSize = remaining;
            newBlock.compressedSize = static_cast<uint32_t>(compressedSize);
            newBlock.flags = origBlocks[0].flags;
            newBlocks.push_back(newBlock);
            compressedBlocks.push_back(std::move(compressed));
        } else {
            ByteBuffer uncompressed(blockData, blockData + remaining);

            UnityStorageBlock newBlock;
            newBlock.uncompressedSize = remaining;
            newBlock.compressedSize = remaining;
            newBlock.flags = 0;
            newBlocks.push_back(newBlock);
            compressedBlocks.push_back(std::move(uncompressed));
        }

        dataPos += remaining;
    }

    // Build blocks info buffer
    ByteBuffer blocksInfo;
    {
        // Hash (16 zero bytes — Unity doesn't validate this for loading)
        blocksInfo.resize(16, 0);

        // Block count (big-endian)
        auto blockCount = static_cast<uint32_t>(newBlocks.size());
        size_t pos = blocksInfo.size();
        blocksInfo.resize(pos + 4);
        writeBE32(blocksInfo.data() + pos, blockCount);

        // Block entries (big-endian, flags = 2 bytes)
        for (const auto& block : newBlocks) {
            pos = blocksInfo.size();
            blocksInfo.resize(pos + 10); // 4 + 4 + 2 = 10 bytes per block
            writeBE32(blocksInfo.data() + pos, block.uncompressedSize);
            writeBE32(blocksInfo.data() + pos + 4, block.compressedSize);
            writeBE16(blocksInfo.data() + pos + 8, block.flags);
        }

        // Node count (big-endian)
        auto nodeCount = static_cast<uint32_t>(nodes.size());
        pos = blocksInfo.size();
        blocksInfo.resize(pos + 4);
        writeBE32(blocksInfo.data() + pos, nodeCount);

        // Node entries (big-endian)
        for (const auto& node : nodes) {
            pos = blocksInfo.size();
            blocksInfo.resize(pos + 20); // 8 + 8 + 4 = 20 bytes
            // Update node size if it's the SerializedFile that changed
            uint64_t nodeSize = node.size;
            // For single-node bundles, set to total new data size
            if (nodes.size() == 1) {
                nodeSize = newData.size();
            }
            writeBE64(blocksInfo.data() + pos, node.offset);
            writeBE64(blocksInfo.data() + pos + 8, nodeSize);
            writeBE32(blocksInfo.data() + pos + 16, node.flags);

            // Path string + null terminator
            for (char c : node.path) {
                blocksInfo.push_back(static_cast<uint8_t>(c));
            }
            blocksInfo.push_back(0);
        }
    }

    // Compress blocks info
    ByteBuffer compressedBlocksInfo;
    auto blocksInfoCompression = header.compressionType();

    if (blocksInfoCompression == UnityCompression::LZ4 ||
        blocksInfoCompression == UnityCompression::LZ4HC) {
        int maxSize = LZ4_compressBound(static_cast<int>(blocksInfo.size()));
        compressedBlocksInfo.resize(static_cast<size_t>(maxSize));
        int compSize = LZ4_compress_HC(
            reinterpret_cast<const char*>(blocksInfo.data()),
            reinterpret_cast<char*>(compressedBlocksInfo.data()),
            static_cast<int>(blocksInfo.size()), maxSize, LZ4HC_CLEVEL_DEFAULT);

        if (compSize <= 0) {
            return std::unexpected(Error(ErrorCode::CompressionFailed,
                "Failed to compress blocks info"));
        }
        compressedBlocksInfo.resize(static_cast<size_t>(compSize));
    } else {
        compressedBlocksInfo = blocksInfo;
    }

    // Write to temporary file then rename (atomic)
    fs::path tmpPath = bundlePath;
    tmpPath += ".makineai_tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out) {
            return std::unexpected(Error(ErrorCode::FileCreateFailed,
                "Cannot create temp file: " + tmpPath.string()));
        }

        // Write header
        out.write(header.signature, 7);

        uint32_t ver = _byteswap_ulong(header.formatVersion);
        out.write(reinterpret_cast<const char*>(&ver), 4);

        // Version strings (null-terminated)
        out.write(header.unityVersion.c_str(), header.unityVersion.size() + 1);
        out.write(header.generatorVersion.c_str(), header.generatorVersion.size() + 1);

        // Total size (placeholder, update later)
        uint64_t totalSizePlaceholder = 0;
        auto totalSizePos = out.tellp();
        uint64_t totalSizeBE = _byteswap_uint64(totalSizePlaceholder);
        out.write(reinterpret_cast<const char*>(&totalSizeBE), 8);

        // Compressed/uncompressed blocks info sizes
        uint32_t compBlocksInfoSizeBE = _byteswap_ulong(
            static_cast<uint32_t>(compressedBlocksInfo.size()));
        uint32_t uncompBlocksInfoSizeBE = _byteswap_ulong(
            static_cast<uint32_t>(blocksInfo.size()));
        out.write(reinterpret_cast<const char*>(&compBlocksInfoSizeBE), 4);
        out.write(reinterpret_cast<const char*>(&uncompBlocksInfoSizeBE), 4);

        // Flags — clear isBlocksInfoAtEnd (0x80) since we always write inline
        uint32_t outFlags = header.flags & ~static_cast<uint32_t>(0x80);
        uint32_t flagsBE = _byteswap_ulong(outFlags);
        out.write(reinterpret_cast<const char*>(&flagsBE), 4);

        // Blocks info
        out.write(reinterpret_cast<const char*>(compressedBlocksInfo.data()),
            compressedBlocksInfo.size());

        // Data blocks
        for (const auto& block : compressedBlocks) {
            out.write(reinterpret_cast<const char*>(block.data()), block.size());
        }

        // Update total size
        auto endPos = out.tellp();
        uint64_t totalSize = static_cast<uint64_t>(endPos);
        out.seekp(totalSizePos);
        totalSizeBE = _byteswap_uint64(totalSize);
        out.write(reinterpret_cast<const char*>(&totalSizeBE), 8);

        out.close();
        if (!out.good()) {
            std::error_code ec;
            fs::remove(tmpPath, ec);
            return std::unexpected(Error(ErrorCode::FileWriteFailed,
                "Failed to write bundle: " + tmpPath.string()));
        }
    }

    // Atomic rename
    std::error_code ec;
    fs::rename(tmpPath, bundlePath, ec);
    if (ec) {
        fs::remove(tmpPath, ec);
        return std::unexpected(Error(ErrorCode::FileWriteFailed,
            "Failed to rename temp file to bundle: " + ec.message()));
    }

    MAKINEAI_LOG_INFO(log::PARSER, "Bundle patched successfully: {}",
        bundlePath.filename().string());

    return {};
}

// =============================================================================
// patchBundle
// =============================================================================

VoidResult UnityBundlePatcher::patchBundle(
    const fs::path& bundlePath,
    const std::vector<UnityPatchEntry>& patches,
    ProgressCallback progress)
{
    if (patches.empty()) return {};

    if (!fs::exists(bundlePath)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Bundle file not found: " + bundlePath.string()));
    }

    if (progress) progress(0, 5, "Reading bundle header...");

    // Read header
    std::ifstream stream(bundlePath, std::ios::binary);
    if (!stream) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open bundle: " + bundlePath.string()));
    }

    // Parse header (reusing logic from unity_bundle_parser.cpp)
    UnityFSHeader header{};
    stream.read(header.signature, 7);
    header.signature[7] = '\0';

    if (std::string_view(header.signature) != kUnityFSMagic) {
        return std::unexpected(Error(ErrorCode::InvalidFormat,
            "Not a UnityFS file: " + bundlePath.string()));
    }

    uint32_t version;
    stream.read(reinterpret_cast<char*>(&version), 4);
    header.formatVersion = _byteswap_ulong(version);

    std::getline(stream, header.unityVersion, '\0');
    std::getline(stream, header.generatorVersion, '\0');

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

    if (!stream.good()) {
        return std::unexpected(Error(ErrorCode::ParseError,
            "Failed to read bundle header"));
    }

    if (progress) progress(1, 5, "Parsing block info...");

    // Save header end position (data blocks start here when blocks info is at end)
    uint64_t headerEndPos = static_cast<uint64_t>(stream.tellg());

    // Handle blocks info at end of file (flag 0x80)
    if (header.isBlocksInfoAtEnd()) {
        stream.seekg(-static_cast<std::streamoff>(header.compressedBlocksInfoSize),
            std::ios::end);
    }

    // Read and decompress blocks info
    ByteBuffer compressedInfo(header.compressedBlocksInfoSize);
    stream.read(reinterpret_cast<char*>(compressedInfo.data()),
        header.compressedBlocksInfoSize);

    ByteBuffer blocksInfoData;
    auto compression = header.compressionType();

    if (compression == UnityCompression::None) {
        blocksInfoData = std::move(compressedInfo);
    } else if (compression == UnityCompression::LZ4 || compression == UnityCompression::LZ4HC) {
        blocksInfoData.resize(header.uncompressedBlocksInfoSize);
        int ret = LZ4_decompress_safe(
            reinterpret_cast<const char*>(compressedInfo.data()),
            reinterpret_cast<char*>(blocksInfoData.data()),
            static_cast<int>(compressedInfo.size()),
            static_cast<int>(blocksInfoData.size()));
        if (ret < 0) {
            return std::unexpected(Error(ErrorCode::DecompressionFailed,
                "Failed to decompress blocks info"));
        }
    } else {
        return std::unexpected(Error(ErrorCode::UnsupportedVersion,
            "LZMA compression not supported for patching"));
    }

    // Parse blocks and nodes (blocks info is big-endian)
    std::vector<UnityStorageBlock> blocks;
    std::vector<UnityNode> nodes;

    {
        BlocksInfoReader bir(blocksInfoData.data(), blocksInfoData.size());

        bir.skip(16); // skip hash

        uint32_t blockCount = bir.readU32();
        if (blockCount > 10000) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Invalid block count: " + std::to_string(blockCount)));
        }

        blocks.reserve(blockCount);
        for (uint32_t i = 0; i < blockCount; ++i) {
            UnityStorageBlock block;
            block.uncompressedSize = bir.readU32();
            block.compressedSize = bir.readU32();
            block.flags = bir.readU16(); // 2 bytes, not 4!
            blocks.push_back(block);
        }

        uint32_t nodeCount = bir.readU32();
        if (nodeCount > 10000) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Invalid node count: " + std::to_string(nodeCount)));
        }

        nodes.reserve(nodeCount);
        for (uint32_t i = 0; i < nodeCount; ++i) {
            UnityNode node;
            node.offset = bir.readU64();
            node.size = bir.readU64();
            node.flags = bir.readU32();
            node.path = bir.readNullTerminated();
            nodes.push_back(node);
        }
    }

    // Data blocks start right after header if blocks info is at end,
    // otherwise they start right after blocks info (current stream position)
    uint64_t dataOffset = header.isBlocksInfoAtEnd()
        ? headerEndPos
        : static_cast<uint64_t>(stream.tellg());

    if (progress) progress(2, 5, "Decompressing data blocks...");

    // Decompress all data blocks
    auto dataResult = decompressDataBlocks(stream, blocks, dataOffset);
    if (!dataResult) return std::unexpected(dataResult.error());
    ByteBuffer decompressedData = std::move(*dataResult);

    stream.close();

    if (progress) progress(3, 5, "Patching assets...");

    // For each node (SerializedFile), check if any patches target it
    for (const auto& node : nodes) {
        if (!node.isSerializedFile()) continue;

        // Find patches targeting this node's CAB path
        std::vector<UnityPatchEntry> nodePatches;
        for (const auto& patch : patches) {
            if (patch.cabPath == node.path) {
                nodePatches.push_back(patch);
            }
        }

        if (nodePatches.empty()) continue;

        MAKINEAI_LOG_INFO(log::PARSER,
            "Patching node '{}': {} replacements", node.path, nodePatches.size());

        // Parse SerializedFile
        auto sfResult = parseSerializedFile(decompressedData, node.offset, node.size);
        if (!sfResult) {
            MAKINEAI_LOG_ERROR(log::PARSER,
                "Failed to parse SerializedFile '{}': {}",
                node.path, sfResult.error().message());
            return std::unexpected(sfResult.error());
        }

        const auto& sfInfo = *sfResult;

        // Verify all pathIds exist
        for (const auto& patch : nodePatches) {
            bool found = false;
            for (const auto& obj : sfInfo.objects) {
                if (obj.pathId == patch.pathId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return std::unexpected(Error(ErrorCode::NotFound,
                    "PathID " + std::to_string(patch.pathId) +
                    " not found in " + node.path));
            }
        }

        // Replace objects
        auto replaceResult = replaceObjects(
            decompressedData, node.offset, node.size, sfInfo, nodePatches);
        if (!replaceResult) return std::unexpected(replaceResult.error());

        const ByteBuffer& newSF = *replaceResult;

        // Update decompressed data with new SerializedFile content
        if (newSF.size() == node.size) {
            // Same size: direct copy
            std::memcpy(decompressedData.data() + node.offset,
                newSF.data(), newSF.size());
        } else {
            // Different size: rebuild decompressed data buffer
            ByteBuffer newDecompressed;
            newDecompressed.reserve(decompressedData.size() +
                newSF.size() - static_cast<size_t>(node.size));

            // Before this node
            newDecompressed.insert(newDecompressed.end(),
                decompressedData.begin(),
                decompressedData.begin() + static_cast<ptrdiff_t>(node.offset));

            // New SerializedFile
            newDecompressed.insert(newDecompressed.end(),
                newSF.begin(), newSF.end());

            // After this node
            uint64_t afterNode = node.offset + node.size;
            if (afterNode < decompressedData.size()) {
                newDecompressed.insert(newDecompressed.end(),
                    decompressedData.begin() + static_cast<ptrdiff_t>(afterNode),
                    decompressedData.end());
            }

            decompressedData = std::move(newDecompressed);
        }
    }

    if (progress) progress(4, 5, "Rebuilding bundle...");

    // Rebuild the bundle with recompressed data
    auto rebuildResult = rebuildBundle(
        bundlePath, header, blocks, nodes, decompressedData, dataOffset);
    if (!rebuildResult) return std::unexpected(rebuildResult.error());

    if (progress) progress(5, 5, "Done");

    return {};
}

} // namespace makineai
