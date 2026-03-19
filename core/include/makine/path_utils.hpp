/**
 * @file path_utils.hpp
 * @brief Path validation, sanitization, and security utilities
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides comprehensive path manipulation utilities with a focus on security:
 * - Path traversal prevention
 * - Platform-specific sanitization
 * - Safe path joining
 * - Encoding validation
 *
 * All functions are designed to be safe with arbitrary input, including
 * potentially malicious paths.
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace makine::path {

namespace fs = std::filesystem;

// =============================================================================
// Path Normalization
// =============================================================================

/**
 * @brief Normalize a path string to canonical form
 *
 * Resolves '..' and '.' components, normalizes separators.
 * Does NOT resolve symlinks or check if path exists.
 *
 * @param pathStr Raw path string
 * @return Normalized path
 */
inline fs::path normalize(std::string_view pathStr) {
    if (pathStr.empty()) {
        return fs::path{};
    }

    try {
        fs::path p(pathStr);
        fs::path normalized;

        for (const auto& part : p) {
            if (part == "..") {
                if (!normalized.empty() && normalized.filename() != "..") {
                    normalized = normalized.parent_path();
                }
            } else if (part != ".") {
                normalized /= part;
            }
        }

        return normalized;
    } catch (...) {
        return fs::path{};
    }
}

// =============================================================================
// Path Traversal Detection
// =============================================================================

/**
 * @brief Check if a path is contained within a base directory
 *
 * Prevents directory traversal attacks by ensuring the resolved
 * path stays within the allowed base directory.
 *
 * @param childPath Path to check (can be relative)
 * @param basePath Base directory that should contain childPath
 * @return true if childPath is within basePath
 */
inline bool isContainedIn(const fs::path& childPath, const fs::path& basePath) {
    try {
        // Normalize both paths
        fs::path normalizedChild = normalize(childPath.string());
        fs::path normalizedBase = normalize(basePath.string());

        // Convert to absolute if needed
        if (normalizedChild.is_relative()) {
            normalizedChild = normalizedBase / normalizedChild;
            normalizedChild = normalize(normalizedChild.string());
        }

        // Check if child starts with base
        auto childStr = normalizedChild.string();
        auto baseStr = normalizedBase.string();

        // Ensure base ends with separator for proper prefix matching
        if (!baseStr.empty() && baseStr.back() != '/' && baseStr.back() != '\\') {
            baseStr += fs::path::preferred_separator;
        }

        return childStr.find(baseStr) == 0 || childStr == normalizedBase.string();
    } catch (...) {
        return false;
    }
}

/**
 * @brief Check if path contains traversal patterns
 *
 * @param pathStr Path to check
 * @return true if path contains suspicious patterns
 */
inline bool containsTraversalPattern(std::string_view pathStr) {
    // Direct patterns
    if (pathStr.find("..") != std::string_view::npos) {
        return true;
    }

    // URL-encoded patterns
    if (pathStr.find("%2e") != std::string_view::npos ||
        pathStr.find("%2E") != std::string_view::npos) {
        return true;
    }

    // Double-encoded
    if (pathStr.find("%252e") != std::string_view::npos ||
        pathStr.find("%252E") != std::string_view::npos) {
        return true;
    }

    // Unicode overlong encoding (invalid UTF-8 for '.')
    // 0xC0 0xAE is overlong encoding of '.'
    auto data = pathStr.data();
    for (size_t i = 0; i + 1 < pathStr.size(); ++i) {
        if (static_cast<uint8_t>(data[i]) == 0xC0 &&
            static_cast<uint8_t>(data[i + 1]) == 0xAE) {
            return true;
        }
    }

    return false;
}

// =============================================================================
// Path Sanitization
// =============================================================================

/**
 * @brief Remove dangerous characters and sequences from path
 *
 * @param pathStr Raw path string
 * @return Sanitized path string
 */
inline std::string sanitize(std::string_view pathStr) {
    std::string result;
    result.reserve(pathStr.size());

    bool lastWasSeparator = false;

    for (size_t i = 0; i < pathStr.size(); ++i) {
        char c = pathStr[i];

        // Skip null bytes
        if (c == '\0') {
            continue;
        }

        // Skip control characters
        if (c < 32 && c != '\t') {
            continue;
        }

        // Normalize separators
        if (c == '/' || c == '\\') {
            if (!lastWasSeparator) {
                result += fs::path::preferred_separator;
                lastWasSeparator = true;
            }
            continue;
        }

        lastWasSeparator = false;
        result += c;
    }

    return result;
}

/**
 * @brief Sanitize path for Windows filesystem
 *
 * Removes characters that are invalid on Windows:
 * < > : " | ? * and control characters
 *
 * @param pathStr Path to sanitize
 * @return Windows-safe path
 */
inline std::string sanitizeForWindows(std::string_view pathStr) {
    static const std::unordered_set<char> invalidChars = {
        '<', '>', ':', '"', '|', '?', '*'
    };

    std::string result;
    result.reserve(pathStr.size());

    for (char c : pathStr) {
        // Skip control characters except tab
        if (static_cast<unsigned char>(c) < 32 && c != '\t') {
            continue;
        }

        // Skip null bytes
        if (c == '\0') {
            continue;
        }

        // Skip Windows-invalid characters (except : for drive letters)
        if (invalidChars.count(c)) {
            continue;
        }

        result += c;
    }

    // Remove trailing spaces and dots (Windows doesn't allow them)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    return result;
}

/**
 * @brief Sanitize path for Unix filesystem
 *
 * @param pathStr Path to sanitize
 * @return Unix-safe path
 */
inline std::string sanitizeForUnix(std::string_view pathStr) {
    std::string result;
    result.reserve(pathStr.size());

    for (char c : pathStr) {
        // Skip null bytes
        if (c == '\0') {
            continue;
        }

        result += c;
    }

    return result;
}

// =============================================================================
// Component Extraction
// =============================================================================

/**
 * @brief Safely extract filename from path
 *
 * @param p Path to extract from
 * @return Filename or empty string if extraction fails
 */
inline std::string safeFilename(const fs::path& p) {
    try {
        return p.filename().string();
    } catch (...) {
        return "";
    }
}

/**
 * @brief Safely extract extension from path
 *
 * @param p Path to extract from
 * @return Extension (including dot) or empty string
 */
inline std::string safeExtension(const fs::path& p) {
    try {
        return p.extension().string();
    } catch (...) {
        return "";
    }
}

/**
 * @brief Safely extract stem (filename without extension) from path
 *
 * @param p Path to extract from
 * @return Stem or empty string
 */
inline std::string safeStem(const fs::path& p) {
    try {
        return p.stem().string();
    } catch (...) {
        return "";
    }
}

// =============================================================================
// Path Validation
// =============================================================================

/**
 * @brief Check if string is a valid filename
 *
 * @param name Filename to check
 * @return true if valid
 */
inline bool isValidFilename(std::string_view name) {
    if (name.empty() || name.size() > 255) {
        return false;
    }

    // Check for reserved characters
    static const std::string invalidChars = "<>:\"/\\|?*";
    for (char c : name) {
        if (c == '\0' || static_cast<unsigned char>(c) < 32) {
            return false;
        }
        if (invalidChars.find(c) != std::string::npos) {
            return false;
        }
    }

    // Check for trailing space or dot
    if (name.back() == ' ' || name.back() == '.') {
        return false;
    }

    return true;
}

/**
 * @brief Check if string is a valid path
 *
 * @param pathStr Path to check
 * @return true if valid
 */
inline bool isValidPath(std::string_view pathStr) {
    if (pathStr.empty()) {
        return false;
    }

    // Check for null bytes
    if (pathStr.find('\0') != std::string_view::npos) {
        return false;
    }

    // Check for traversal patterns
    if (containsTraversalPattern(pathStr)) {
        return false;
    }

    try {
        fs::path p(pathStr);
        // Basic validity check - can it be constructed?
        (void)p.string();
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Check if name is a Windows reserved name
 *
 * Reserved names: CON, PRN, AUX, NUL, COM1-9, LPT1-9
 *
 * @param name Name to check
 * @return true if reserved
 */
inline bool isReservedName(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    // Extract stem (without extension)
    std::string stem;
    auto dotPos = name.rfind('.');
    if (dotPos != std::string_view::npos) {
        stem = std::string(name.substr(0, dotPos));
    } else {
        stem = std::string(name);
    }

    // Convert to uppercase for comparison
    std::transform(stem.begin(), stem.end(), stem.begin(), ::toupper);

    // Check reserved names
    static const std::unordered_set<std::string> reserved = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };

    return reserved.count(stem) > 0;
}

// =============================================================================
// Safe Path Joining
// =============================================================================

/**
 * @brief Safely join base and relative paths
 *
 * Ensures result stays within base directory.
 *
 * @param base Base directory
 * @param relative Relative path to append
 * @return Joined path that is guaranteed to be within base
 */
inline fs::path safeJoin(const fs::path& base, const fs::path& relative) {
    // First, normalize relative to remove any traversal
    auto normalizedRel = normalize(relative.string());

    // Remove leading separators
    auto relStr = normalizedRel.string();
    while (!relStr.empty() && (relStr[0] == '/' || relStr[0] == '\\')) {
        relStr = relStr.substr(1);
    }

    // Join
    fs::path result = base / relStr;

    // Verify containment
    if (!isContainedIn(result, base)) {
        // Return just the filename if traversal was attempted
        return base / normalizedRel.filename();
    }

    return result;
}

// =============================================================================
// Symlink and Device Detection
// =============================================================================

/**
 * @brief Check if path might be a symlink target
 *
 * Looks for patterns like /proc/self, /dev, etc.
 *
 * @param pathStr Path to check
 * @return true if path looks like a symlink target
 */
inline bool mightBeSymlinkTarget(std::string_view pathStr) {
    // Common Unix symlink targets
    static const std::vector<std::string_view> patterns = {
        "/proc/", "/sys/", "/dev/", "/etc/", "/root/",
        "\\\\?\\", "\\\\.\\"  // Windows special prefixes
    };

    for (const auto& pattern : patterns) {
        if (pathStr.find(pattern) != std::string_view::npos) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Check if path is a device file path
 *
 * @param pathStr Path to check
 * @return true if path is a device file
 */
inline bool isDeviceFilePath(std::string_view pathStr) {
    // Unix device paths
    if (pathStr.find("/dev/") == 0) {
        return true;
    }

    // Windows device paths
    if (pathStr.find("\\\\.\\") == 0 ||
        pathStr.find("\\\\?\\") == 0) {
        return true;
    }

    // Windows device names without prefix
    std::string upper(pathStr);
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper.find("PHYSICALDRIVE") != std::string::npos ||
        upper.find("GLOBALROOT") != std::string::npos) {
        return true;
    }

    return false;
}

// =============================================================================
// Encoding Handling
// =============================================================================

/**
 * @brief Convert path to UTF-8
 *
 * @param pathStr Path string (possibly in other encoding)
 * @return UTF-8 encoded path
 */
inline std::string toUtf8(std::string_view pathStr) {
    // For now, assume input is already UTF-8 or ASCII
    // In a full implementation, this would detect and convert encodings
    return std::string(pathStr);
}

/**
 * @brief Check if path has invalid encoding
 *
 * @param pathStr Path to check
 * @return true if encoding issues detected
 */
inline bool hasInvalidEncoding(std::string_view pathStr) {
    // Check for invalid UTF-8 sequences
    auto data = reinterpret_cast<const uint8_t*>(pathStr.data());
    size_t i = 0;

    while (i < pathStr.size()) {
        uint8_t c = data[i];

        if (c < 0x80) {
            // ASCII - OK
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= pathStr.size() || (data[i + 1] & 0xC0) != 0x80) {
                return true;
            }
            // Check for overlong encoding
            if (c < 0xC2) {
                return true;
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= pathStr.size() ||
                (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80) {
                return true;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= pathStr.size() ||
                (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return true;
            }
            i += 4;
        } else {
            // Invalid start byte
            return true;
        }
    }

    return false;
}

// =============================================================================
// Case Sensitivity
// =============================================================================

/**
 * @brief Convert path to lowercase
 *
 * @param pathStr Path to convert
 * @return Lowercase path
 */
inline std::string toLowerPath(std::string_view pathStr) {
    std::string result(pathStr);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

/**
 * @brief Compare paths case-insensitively
 *
 * @param path1 First path
 * @param path2 Second path
 * @return true if paths are equal (case-insensitive)
 */
inline bool equalsCaseInsensitive(std::string_view path1, std::string_view path2) {
    if (path1.size() != path2.size()) {
        return false;
    }

    for (size_t i = 0; i < path1.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(path1[i])) !=
            std::tolower(static_cast<unsigned char>(path2[i]))) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// Path Length Limits
// =============================================================================

/**
 * @brief Maximum Windows path length (without long path prefix)
 */
constexpr size_t WINDOWS_MAX_PATH = 260;

/**
 * @brief Maximum Windows path length with long path prefix
 */
constexpr size_t WINDOWS_MAX_PATH_LONG = 32767;

/**
 * @brief Maximum Unix path length (common limit)
 */
constexpr size_t UNIX_MAX_PATH = 4096;

/**
 * @brief Check if path exceeds Windows path limit
 *
 * @param pathStr Path to check
 * @return true if path exceeds limit
 */
inline bool exceedsWindowsPathLimit(std::string_view pathStr) {
    return pathStr.size() > WINDOWS_MAX_PATH;
}

/**
 * @brief Check if path exceeds Unix path limit
 *
 * @param pathStr Path to check
 * @return true if path exceeds limit
 */
inline bool exceedsUnixPathLimit(std::string_view pathStr) {
    return pathStr.size() > UNIX_MAX_PATH;
}

/**
 * @brief Truncate path to specified length
 *
 * Attempts to truncate intelligently by removing middle components.
 *
 * @param pathStr Path to truncate
 * @param maxLen Maximum length
 * @return Truncated path
 */
inline std::string truncateToLimit(std::string_view pathStr, size_t maxLen) {
    if (pathStr.size() <= maxLen) {
        return std::string(pathStr);
    }

    if (maxLen < 10) {
        return std::string(pathStr.substr(0, maxLen));
    }

    // Keep beginning and end
    size_t keepStart = maxLen / 3;
    size_t keepEnd = maxLen / 3;
    size_t ellipsis = 3;  // "..."

    while (keepStart + ellipsis + keepEnd > maxLen) {
        if (keepStart > keepEnd) {
            --keepStart;
        } else {
            --keepEnd;
        }
    }

    return std::string(pathStr.substr(0, keepStart)) + "..." +
           std::string(pathStr.substr(pathStr.size() - keepEnd));
}

} // namespace makine::path
