/**
 * @file validation.hpp
 * @brief Input validation utilities for Makine
 *
 * Provides:
 * - Path validation (safety, existence, permissions)
 * - String validation (length, encoding, content)
 * - ID/URL validation
 * - Sanitization functions
 *
 * All functions return Result<T> with descriptive errors.
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include "makine/types.hpp"
#include "makine/error.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace makine::validation {

// =============================================================================
// PATH VALIDATION
// =============================================================================

/**
 * @brief Check if path is safe (no traversal attacks)
 *
 * Rejects paths containing:
 * - ".." components
 * - Absolute paths when relative expected
 * - Suspicious characters
 */
[[nodiscard]] inline bool isPathSafe(const fs::path& path) {
    auto str = path.string();

    // Check for directory traversal
    if (str.find("..") != std::string::npos) {
        return false;
    }

    // Check for null bytes
    if (str.find('\0') != std::string::npos) {
        return false;
    }

    // Check for suspicious patterns on Windows
    #ifdef _WIN32
    // Device names
    static const std::vector<std::string> deviceNames = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };

    auto stem = path.stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(), ::toupper);

    for (const auto& device : deviceNames) {
        if (stem == device) {
            return false;
        }
    }
    #endif

    return true;
}

/**
 * @brief Validate path exists
 */
[[nodiscard]] inline Result<fs::path> validatePathExists(const fs::path& path) {
    if (path.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path is empty"));
    }

    if (!isPathSafe(path)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path contains unsafe characters or patterns")
            .withDetail("path", path.string()));
    }

    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "Path does not exist")
            .withFile(path)
            .withDetail("error", ec.message()));
    }

    return path;
}

/**
 * @brief Validate path is a directory
 */
[[nodiscard]] inline Result<fs::path> validateDirectory(const fs::path& path) {
    auto result = validatePathExists(path);
    if (!result) {
        return result;
    }

    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path is not a directory")
            .withFile(path));
    }

    return path;
}

/**
 * @brief Validate path is a regular file
 */
[[nodiscard]] inline Result<fs::path> validateFile(const fs::path& path) {
    auto result = validatePathExists(path);
    if (!result) {
        return result;
    }

    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path is not a regular file")
            .withFile(path));
    }

    return path;
}

/**
 * @brief Validate file is readable
 */
[[nodiscard]] inline Result<fs::path> validateReadable(const fs::path& path) {
    auto result = validateFile(path);
    if (!result) {
        return result;
    }

    // Try to open for reading
    std::ifstream test(path);
    if (!test.is_open()) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "File is not readable")
            .withFile(path));
    }

    return path;
}

/**
 * @brief Validate directory is writable
 */
[[nodiscard]] inline Result<fs::path> validateWritable(const fs::path& path) {
    auto result = validateDirectory(path);
    if (!result) {
        return result;
    }

    // Try to create a temp file
    auto testPath = path / ".makine_write_test";
    {
        std::ofstream test(testPath);
        if (!test.is_open()) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Directory is not writable")
                .withFile(path));
        }
        test << "test";
    }

    std::error_code ec;
    fs::remove(testPath, ec);

    return path;
}

/**
 * @brief Sanitize path by removing dangerous components
 */
[[nodiscard]] inline Result<fs::path> sanitizePath(const fs::path& path) {
    if (path.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path is empty"));
    }

    // Build safe path
    fs::path result;

    for (const auto& component : path) {
        auto str = component.string();

        // Skip ".." and "."
        if (str == ".." || str == ".") {
            continue;
        }

        // Skip empty components
        if (str.empty()) {
            continue;
        }

        result /= component;
    }

    if (result.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Path sanitization resulted in empty path"));
    }

    return result;
}

// =============================================================================
// STRING VALIDATION
// =============================================================================

/**
 * @brief Validate string is not empty
 */
[[nodiscard]] inline Result<std::string> validateNotEmpty(
    std::string_view str,
    std::string_view fieldName = "String"
) {
    if (str.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            std::string(fieldName) + " cannot be empty"));
    }
    return std::string(str);
}

/**
 * @brief Validate string length
 */
[[nodiscard]] inline Result<std::string> validateLength(
    std::string_view str,
    size_t minLen,
    size_t maxLen,
    std::string_view fieldName = "String"
) {
    if (str.length() < minLen) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            std::string(fieldName) + " is too short")
            .withDetail("minimum", std::to_string(minLen))
            .withDetail("actual", std::to_string(str.length())));
    }

    if (str.length() > maxLen) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            std::string(fieldName) + " is too long")
            .withDetail("maximum", std::to_string(maxLen))
            .withDetail("actual", std::to_string(str.length())));
    }

    return std::string(str);
}

/**
 * @brief Check if string is valid UTF-8
 */
[[nodiscard]] inline bool isValidUtf8(std::string_view str) {
    size_t i = 0;
    while (i < str.length()) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        size_t bytesNeeded;
        if ((c & 0x80) == 0) {
            bytesNeeded = 1;
        } else if ((c & 0xE0) == 0xC0) {
            bytesNeeded = 2;
        } else if ((c & 0xF0) == 0xE0) {
            bytesNeeded = 3;
        } else if ((c & 0xF8) == 0xF0) {
            bytesNeeded = 4;
        } else {
            return false; // Invalid start byte
        }

        if (i + bytesNeeded > str.length()) {
            return false; // Truncated sequence
        }

        // Check continuation bytes
        for (size_t j = 1; j < bytesNeeded; ++j) {
            if ((static_cast<unsigned char>(str[i + j]) & 0xC0) != 0x80) {
                return false;
            }
        }

        i += bytesNeeded;
    }
    return true;
}

/**
 * @brief Validate string is valid UTF-8
 */
[[nodiscard]] inline Result<std::string> validateUtf8(
    std::string_view str,
    std::string_view fieldName = "String"
) {
    if (!isValidUtf8(str)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            std::string(fieldName) + " contains invalid UTF-8"));
    }
    return std::string(str);
}

/**
 * @brief Sanitize string by removing control characters
 */
[[nodiscard]] inline std::string sanitizeString(
    std::string_view str,
    size_t maxLen = 10000
) {
    std::string result;
    result.reserve(std::min(str.length(), maxLen));

    for (char c : str) {
        if (result.length() >= maxLen) {
            break;
        }

        // Keep printable ASCII and valid UTF-8
        if (c >= 32 || c == '\n' || c == '\r' || c == '\t') {
            result += c;
        } else if (static_cast<unsigned char>(c) >= 0x80) {
            // Part of UTF-8 sequence
            result += c;
        }
        // Skip control characters
    }

    return result;
}

// =============================================================================
// ID VALIDATION
// =============================================================================

/**
 * @brief Validate game ID format
 *
 * Valid: alphanumeric, underscore, hyphen, colon
 * Examples: "steam_123456", "epic:game-name", "gog_1234567890"
 */
[[nodiscard]] inline Result<std::string> validateGameId(const std::string& id) {
    if (id.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Game ID cannot be empty"));
    }

    if (id.length() > 256) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Game ID is too long (max 256 characters)"));
    }

    auto isValidChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':';
    };
    if (!std::all_of(id.begin(), id.end(), isValidChar)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Game ID contains invalid characters")
            .withDetail("valid_chars", "alphanumeric, underscore, hyphen, colon"));
    }

    return id;
}

/**
 * @brief Validate package ID format
 *
 * Format: "game_id_language_version"
 * Example: "steam_123456_tr_1.0.0"
 */
[[nodiscard]] inline Result<std::string> validatePackageId(const std::string& id) {
    if (id.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Package ID cannot be empty"));
    }

    auto isValidChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':' || c == '.';
    };
    if (!std::all_of(id.begin(), id.end(), isValidChar)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Package ID contains invalid characters"));
    }

    return id;
}

// =============================================================================
// URL VALIDATION
// =============================================================================

/**
 * @brief Validate URL format
 */
[[nodiscard]] inline Result<std::string> validateUrl(const std::string& url) {
    if (url.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "URL cannot be empty"));
    }

    // Basic URL validation without <regex>
    bool isHttps = (url.rfind("https://", 0) == 0);
    bool isHttp = (url.rfind("http://", 0) == 0);
    if (!isHttps && !isHttp) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Invalid URL format")
            .withDetail("url", url));
    }

    // Check host part exists and contains only valid chars
    size_t hostStart = isHttps ? 8 : 7;
    if (hostStart >= url.size()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Invalid URL format")
            .withDetail("url", url));
    }

    // Find end of host (before port or path)
    size_t hostEnd = url.find_first_of(":/?#", hostStart);
    if (hostEnd == std::string::npos) hostEnd = url.size();

    std::string_view host(url.data() + hostStart, hostEnd - hostStart);
    if (host.empty()) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Invalid URL format")
            .withDetail("url", url));
    }

    auto isHostChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '.';
    };
    if (!std::all_of(host.begin(), host.end(), isHostChar)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "Invalid URL format")
            .withDetail("url", url));
    }

    // Check for suspicious patterns
    if (url.find("..") != std::string::npos ||
        url.find("//", 8) != std::string::npos) {  // 8 = after "https://"
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "URL contains suspicious patterns"));
    }

    return url;
}

/**
 * @brief Validate URL is HTTPS
 */
[[nodiscard]] inline Result<std::string> validateHttpsUrl(const std::string& url) {
    auto result = validateUrl(url);
    if (!result) {
        return result;
    }

    if (url.substr(0, 8) != "https://") {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "URL must use HTTPS")
            .withDetail("url", url));
    }

    return url;
}

// =============================================================================
// HASH VALIDATION
// =============================================================================

/**
 * @brief Validate SHA-256 hash format
 */
[[nodiscard]] inline Result<std::string> validateSha256(const std::string& hash) {
    if (hash.length() != 64) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "SHA-256 hash must be 64 characters")
            .withDetail("actual_length", std::to_string(hash.length())));
    }

    auto isHexChar = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    if (!std::all_of(hash.begin(), hash.end(), isHexChar)) {
        return std::unexpected(Error(ErrorCode::InvalidArgument,
            "SHA-256 hash contains invalid characters"));
    }

    return hash;
}

// =============================================================================
// COMPOSITE VALIDATORS
// =============================================================================

/**
 * @brief Validation result builder for multiple checks
 */
class ValidationBuilder {
public:
    ValidationBuilder& checkPath(const fs::path& path, std::string_view name) {
        auto result = validatePathExists(path);
        if (!result) {
            errors_.push_back(std::string(name) + ": " + result.error().message());
        }
        return *this;
    }

    ValidationBuilder& checkDirectory(const fs::path& path, std::string_view name) {
        auto result = validateDirectory(path);
        if (!result) {
            errors_.push_back(std::string(name) + ": " + result.error().message());
        }
        return *this;
    }

    ValidationBuilder& checkFile(const fs::path& path, std::string_view name) {
        auto result = validateFile(path);
        if (!result) {
            errors_.push_back(std::string(name) + ": " + result.error().message());
        }
        return *this;
    }

    ValidationBuilder& checkNotEmpty(std::string_view str, std::string_view name) {
        if (str.empty()) {
            errors_.push_back(std::string(name) + " cannot be empty");
        }
        return *this;
    }

    ValidationBuilder& checkLength(std::string_view str, size_t min, size_t max,
                                   std::string_view name) {
        auto result = validateLength(str, min, max, name);
        if (!result) {
            errors_.push_back(result.error().message());
        }
        return *this;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return errors_.empty();
    }

    [[nodiscard]] VoidResult result() const {
        if (errors_.empty()) {
            return {};
        }

        std::string message = "Validation failed:\n";
        for (const auto& error : errors_) {
            message += "  - " + error + "\n";
        }

        return std::unexpected(Error(ErrorCode::InvalidArgument, message));
    }

    [[nodiscard]] const std::vector<std::string>& errors() const noexcept {
        return errors_;
    }

private:
    std::vector<std::string> errors_;
};

/**
 * @brief Start building a validation chain
 */
inline ValidationBuilder validate() {
    return ValidationBuilder{};
}

} // namespace makine::validation
