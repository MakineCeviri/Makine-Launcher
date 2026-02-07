/**
 * @file rpa_archive.cpp
 * @brief Ren'Py RPA archive parser implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "formats/renpy_rpa.hpp"
#include "pickle_reader.hpp"
#include "makineai/logging.hpp"

#include <zlib.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace makineai::formats {

namespace {

/**
 * @brief Parse the first line of an RPA file to extract header info
 *
 * RPA-3.0 format: "RPA-3.0 <hex_offset> <hex_key>\n"
 * RPA-2.0 format: "RPA-2.0 <hex_offset>\n"
 */
Result<RpaHeader> parseRpaHeader(std::istream& stream) {
    std::string firstLine;
    if (!std::getline(stream, firstLine)) {
        return std::unexpected(Error{ErrorCode::InvalidFormat, "RPA: cannot read first line"});
    }

    // Remove trailing \r if present (Windows line endings)
    if (!firstLine.empty() && firstLine.back() == '\r') {
        firstLine.pop_back();
    }

    RpaHeader header{};

    if (firstLine.starts_with("RPA-3.0 ")) {
        header.version = RpaVersion::V3;

        // Parse: "RPA-3.0 <hex_offset> <hex_key>"
        std::istringstream iss(firstLine.substr(8)); // skip "RPA-3.0 "
        std::string offsetStr, keyStr;
        if (!(iss >> offsetStr >> keyStr)) {
            return std::unexpected(Error{ErrorCode::InvalidFormat,
                "RPA-3.0: malformed header line"});
        }

        try {
            header.indexOffset = std::stoull(offsetStr, nullptr, 16);
            header.xorKey = std::stoull(keyStr, nullptr, 16);
        } catch (const std::exception&) {
            return std::unexpected(Error{ErrorCode::InvalidFormat,
                "RPA-3.0: invalid hex values in header"});
        }

    } else if (firstLine.starts_with("RPA-2.0 ")) {
        header.version = RpaVersion::V2;

        std::string offsetStr = firstLine.substr(8);
        // Trim whitespace
        offsetStr.erase(offsetStr.find_last_not_of(" \t") + 1);

        try {
            header.indexOffset = std::stoull(offsetStr, nullptr, 16);
        } catch (const std::exception&) {
            return std::unexpected(Error{ErrorCode::InvalidFormat,
                "RPA-2.0: invalid hex offset in header"});
        }

        header.xorKey = 0;

    } else {
        return std::unexpected(Error{ErrorCode::InvalidFormat,
            "RPA: unrecognized version (expected RPA-2.0 or RPA-3.0)"});
    }

    return header;
}

/**
 * @brief Decompress zlib data
 */
Result<std::vector<uint8_t>> zlibDecompress(std::span<const uint8_t> compressed) {
    if (compressed.empty()) {
        return std::unexpected(Error{ErrorCode::DecompressionFailed, "RPA: empty compressed data"});
    }

    // Start with 4x estimated output
    std::vector<uint8_t> output(compressed.size() * 4);

    z_stream zs{};
    zs.next_in = const_cast<Bytef*>(compressed.data());
    zs.avail_in = static_cast<uInt>(compressed.size());

    if (inflateInit(&zs) != Z_OK) {
        return std::unexpected(Error{ErrorCode::DecompressionFailed, "RPA: zlib inflateInit failed"});
    }

    int ret;
    do {
        if (zs.total_out >= output.size()) {
            output.resize(output.size() * 2);
        }
        zs.next_out = output.data() + zs.total_out;
        zs.avail_out = static_cast<uInt>(output.size() - zs.total_out);
        ret = inflate(&zs, Z_NO_FLUSH);
    } while (ret == Z_OK);

    if (ret != Z_STREAM_END) {
        inflateEnd(&zs);
        return std::unexpected(Error{ErrorCode::DecompressionFailed,
            "RPA: zlib inflate failed with code " + std::to_string(ret)});
    }

    output.resize(zs.total_out);
    inflateEnd(&zs);
    return output;
}

/**
 * @brief Convert the unpickled dict into RPA index entries
 *
 * The pickle dict looks like:
 *   { b"path/to/file.rpy": [(offset, length, prefix_bytes), ...], ... }
 *
 * For RPA-3.0, offset and length are XOR'd with the key.
 */
Result<std::vector<RpaIndexEntry>> convertPickleToEntries(
    const PickleValue& pickleDict, uint64_t xorKey
) {
    if (!pickleDict.isDict()) {
        return std::unexpected(Error{ErrorCode::InvalidFormat,
            "RPA: index is not a dict"});
    }

    std::vector<RpaIndexEntry> entries;
    entries.reserve(pickleDict.asDict().size());

    for (const auto& [key, value] : pickleDict.asDict()) {
        // Key is the file path (bytes or string)
        std::string path;
        if (key.isBytes()) {
            const auto& bytes = key.asBytes();
            path.assign(bytes.begin(), bytes.end());
        } else if (key.isString()) {
            path = key.asString();
        } else {
            continue; // Skip non-path keys
        }

        // Normalize path separators
        std::replace(path.begin(), path.end(), '\\', '/');

        // Value is a list of tuples: [(offset, length, prefix), ...]
        // We take the first tuple (multiple chunks are rare)
        if (!value.isList() || value.asList().empty()) {
            continue;
        }

        const auto& firstTuple = value.asList()[0];
        if (!firstTuple.isList()) continue;

        const auto& tupleItems = firstTuple.asList();
        if (tupleItems.size() < 2) continue;

        if (!tupleItems[0].isInt() || !tupleItems[1].isInt()) continue;

        RpaIndexEntry entry;
        entry.path = path;
        entry.dataOffset = static_cast<uint64_t>(tupleItems[0].asInt()) ^ xorKey;
        entry.dataLength = static_cast<uint64_t>(tupleItems[1].asInt()) ^ xorKey;

        // Optional prefix bytes (third element)
        if (tupleItems.size() >= 3) {
            if (tupleItems[2].isBytes()) {
                entry.prefix = tupleItems[2].asBytes();
            } else if (tupleItems[2].isString()) {
                const auto& s = tupleItems[2].asString();
                entry.prefix.assign(s.begin(), s.end());
            }
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

} // anonymous namespace

// ============================================================================
// PUBLIC API
// ============================================================================

Result<RpaArchive> parseRpaArchive(const fs::path& rpaPath) {
    MAKINEAI_LOG_DEBUG(log::PARSER, "RPA: Parsing archive: {}", rpaPath.string());

    std::ifstream file(rpaPath, std::ios::binary);
    if (!file) {
        return std::unexpected(Error{ErrorCode::FileNotFound,
            "RPA: cannot open file: " + rpaPath.string()});
    }

    // Parse header (first line)
    auto headerResult = parseRpaHeader(file);
    if (!headerResult) {
        return std::unexpected(headerResult.error());
    }
    auto header = *headerResult;

    MAKINEAI_LOG_DEBUG(log::PARSER, "RPA: version={}, indexOffset={:#x}, xorKey={:#x}",
        static_cast<int>(header.version), header.indexOffset, header.xorKey);

    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = static_cast<uint64_t>(file.tellg());

    if (header.indexOffset >= fileSize) {
        return std::unexpected(Error{ErrorCode::InvalidFormat,
            "RPA: index offset beyond file end"});
    }

    // Read compressed index data
    file.seekg(static_cast<std::streamoff>(header.indexOffset));
    auto indexSize = fileSize - header.indexOffset;

    std::vector<uint8_t> compressedIndex(indexSize);
    file.read(reinterpret_cast<char*>(compressedIndex.data()), static_cast<std::streamsize>(indexSize));
    if (!file) {
        return std::unexpected(Error{ErrorCode::IOError,
            "RPA: failed to read compressed index"});
    }

    // Decompress
    auto decompResult = zlibDecompress(compressedIndex);
    if (!decompResult) {
        return std::unexpected(decompResult.error());
    }

    MAKINEAI_LOG_DEBUG(log::PARSER, "RPA: Index decompressed: {} -> {} bytes",
        compressedIndex.size(), decompResult->size());

    // Parse pickle
    auto pickleResult = parsePickle(*decompResult);
    if (!pickleResult) {
        return std::unexpected(pickleResult.error());
    }

    // Convert to entries
    auto entriesResult = convertPickleToEntries(*pickleResult, header.xorKey);
    if (!entriesResult) {
        return std::unexpected(entriesResult.error());
    }

    RpaArchive archive;
    archive.header = header;
    archive.entries = std::move(*entriesResult);

    MAKINEAI_LOG_INFO(log::PARSER, "RPA: Parsed {} entries from {}",
        archive.entries.size(), rpaPath.filename().string());

    return archive;
}

Result<std::vector<uint8_t>> extractRpaEntry(
    const fs::path& rpaPath,
    const RpaIndexEntry& entry
) {
    std::ifstream file(rpaPath, std::ios::binary);
    if (!file) {
        return std::unexpected(Error{ErrorCode::FileNotFound,
            "RPA: cannot open file: " + rpaPath.string()});
    }

    file.seekg(static_cast<std::streamoff>(entry.dataOffset));
    if (!file) {
        return std::unexpected(Error{ErrorCode::InvalidOffset,
            "RPA: invalid data offset for " + entry.path});
    }

    std::vector<uint8_t> data(entry.dataLength);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(entry.dataLength));
    if (!file) {
        return std::unexpected(Error{ErrorCode::IOError,
            "RPA: failed to read entry data for " + entry.path});
    }

    // Prepend prefix bytes
    if (!entry.prefix.empty()) {
        std::vector<uint8_t> result;
        result.reserve(entry.prefix.size() + data.size());
        result.insert(result.end(), entry.prefix.begin(), entry.prefix.end());
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    return data;
}

} // namespace makineai::formats
