/**
 * @file file_integrity.hpp
 * @brief File integrity verification — hash computation and comparison
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Pure C++ implementation using OpenSSL for SHA-256 hashing.
 * Used by QML IntegrityService as backend and by package verification.
 */

#pragma once

#include "makine/types/common.hpp"
#include "makine/error.hpp"

#include <string>
#include <string_view>

namespace makine::integrity {

/**
 * @brief Compute SHA-256 hash of a file using chunked reading
 * @param filePath Path to the file to hash
 * @param chunkSize Read buffer size (default 64KB)
 * @return Lowercase hex-encoded SHA-256 hash, or error
 */
[[nodiscard]] Result<std::string> computeFileHash(
    const fs::path& filePath,
    size_t chunkSize = 65536
);

/**
 * @brief Read expected hash from a .sha256 file
 *
 * Supports two common formats:
 *   - "<hash>  <filename>\\n"  (GNU coreutils format)
 *   - "<hash>\\n"              (plain hash)
 *
 * @param hashFilePath Path to the .sha256 file
 * @return Lowercase hex hash (64 chars) or error
 */
[[nodiscard]] Result<std::string> readHashFile(const fs::path& hashFilePath);

/**
 * @brief Verify file integrity against a .sha256 sidecar file
 *
 * Looks for <filePath>.sha256, reads expected hash, computes actual hash,
 * and compares using constant-time comparison.
 *
 * @param filePath Path to the file to verify
 * @return Result with verification status:
 *   - true: hash matches
 *   - false: hash mismatch
 *   - error: missing hash file or I/O failure
 */
[[nodiscard]] Result<bool> verifyFile(const fs::path& filePath);

/**
 * @brief Constant-time hex string comparison
 *
 * Prevents timing attacks when comparing hash values.
 * Both strings must be the same length.
 *
 * @return true if strings are equal
 */
[[nodiscard]] bool secureCompareHex(std::string_view a, std::string_view b) noexcept;

/**
 * @brief Validate that a string is a valid lowercase hex SHA-256 hash (64 chars)
 */
[[nodiscard]] bool isValidSha256Hex(std::string_view hex) noexcept;

} // namespace makine::integrity
