/**
 * @file renpy_rpa.hpp
 * @brief Ren'Py RPA archive format structures
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Reference: Ren'Py RPA-2.0 / RPA-3.0 archive format
 * Used by: Ren'Py visual novels (most ship scripts inside .rpa archives)
 *
 * Format overview:
 * - First line: "RPA-3.0 <hex_offset> <hex_key>\n" (or "RPA-2.0 <hex_offset>\n")
 * - Data region: raw file data stored contiguously
 * - Index: zlib-compressed Python pickle dict at the given offset
 *   Keys = file paths (bytes), Values = list of (offset, length, prefix) tuples
 * - RPA-3.0 XORs offset/length values with the key for light obfuscation
 */

#pragma once

#include "../makine/error.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace makine::formats {

// ============================================================================
// RPA VERSION
// ============================================================================

enum class RpaVersion : uint8_t {
    V2 = 2,   // RPA-2.0 (no XOR key)
    V3 = 3    // RPA-3.0 (XOR obfuscation on offset/length)
};

// ============================================================================
// RPA HEADER
// ============================================================================

struct RpaHeader {
    RpaVersion version;
    uint64_t indexOffset;   // Byte offset to the compressed index
    uint64_t xorKey;        // XOR key (V3 only, 0 for V2)
};

// ============================================================================
// RPA INDEX ENTRY
// ============================================================================

struct RpaIndexEntry {
    std::string path;                   // Relative file path inside the archive
    uint64_t dataOffset;                // Offset to file data (after XOR decode)
    uint64_t dataLength;                // Length of file data (after XOR decode)
    std::vector<uint8_t> prefix;        // Optional prefix bytes prepended to data
};

// ============================================================================
// RPA ARCHIVE
// ============================================================================

struct RpaArchive {
    RpaHeader header;
    std::vector<RpaIndexEntry> entries;

    [[nodiscard]] const RpaIndexEntry* findEntry(const std::string& path) const {
        for (const auto& entry : entries) {
            if (entry.path == path) return &entry;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<const RpaIndexEntry*> findByExtension(
        const std::string& ext
    ) const {
        std::vector<const RpaIndexEntry*> results;
        for (const auto& entry : entries) {
            auto dotPos = entry.path.rfind('.');
            if (dotPos != std::string::npos) {
                auto entryExt = entry.path.substr(dotPos + 1);
                if (entryExt == ext) {
                    results.push_back(&entry);
                }
            }
        }
        return results;
    }
};

// ============================================================================
// FREE FUNCTIONS
// ============================================================================

namespace fs = std::filesystem;

/**
 * @brief Parse an RPA archive and return its index
 * @param rpaPath Path to the .rpa file
 * @return Parsed archive with header and file entries
 */
[[nodiscard]] Result<RpaArchive> parseRpaArchive(const fs::path& rpaPath);

/**
 * @brief Extract a single file from an RPA archive
 * @param rpaPath Path to the .rpa file
 * @param entry Index entry describing the file to extract
 * @return Raw file bytes (prefix + data)
 */
[[nodiscard]] Result<std::vector<uint8_t>> extractRpaEntry(
    const fs::path& rpaPath,
    const RpaIndexEntry& entry
);

} // namespace makine::formats
