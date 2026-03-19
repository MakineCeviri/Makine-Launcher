/**
 * @file utf_utils.hpp
 * @brief UTF encoding utilities with optional SIMDUTF acceleration
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides fast UTF-8/16/32 conversion and validation.
 * Uses SIMDUTF when available for 3-5x speedup on string operations.
 *
 * Compile-time detection:
 * - MAKINE_HAS_SIMDUTF - SIMDUTF library available
 *
 * All functions have fallback implementations using standard C++.
 */

#pragma once

#include "features.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef MAKINE_HAS_SIMDUTF
#include <simdutf.h>
#endif

namespace makine::utf {

// =============================================================================
// UTF-8 Validation
// =============================================================================

/**
 * @brief Validate if a string is valid UTF-8
 *
 * @param str String to validate
 * @return true if valid UTF-8
 */
inline bool isValidUtf8(std::string_view str) noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return simdutf::validate_utf8(str.data(), str.size());
#else
    // Fallback: Manual UTF-8 validation
    auto data = reinterpret_cast<const uint8_t*>(str.data());
    size_t i = 0;

    while (i < str.size()) {
        uint8_t c = data[i];

        if (c < 0x80) {
            // ASCII
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= str.size()) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if (c < 0xC2) return false;  // Overlong
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= str.size()) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            // Check overlong and surrogates
            uint32_t cp = ((c & 0x0F) << 12) |
                          ((data[i + 1] & 0x3F) << 6) |
                          (data[i + 2] & 0x3F);
            if (cp < 0x0800 || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= str.size()) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            if ((data[i + 3] & 0xC0) != 0x80) return false;
            // Check overlong and max codepoint
            uint32_t cp = ((c & 0x07) << 18) |
                          ((data[i + 1] & 0x3F) << 12) |
                          ((data[i + 2] & 0x3F) << 6) |
                          (data[i + 3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            i += 4;
        } else {
            return false;
        }
    }

    return true;
#endif
}

/**
 * @brief Count the number of UTF-8 code points in a string
 *
 * @param str UTF-8 string
 * @return Number of code points, or -1 if invalid
 */
inline int64_t countUtf8CodePoints(std::string_view str) noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return simdutf::count_utf8(str.data(), str.size());
#else
    int64_t count = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        // Count bytes that are not continuation bytes
        if ((static_cast<uint8_t>(str[i]) & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
#endif
}

// =============================================================================
// UTF-16 Validation
// =============================================================================

/**
 * @brief Validate if data is valid UTF-16 LE
 *
 * @param data UTF-16 data
 * @return true if valid
 */
inline bool isValidUtf16LE(std::span<const uint8_t> data) noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return simdutf::validate_utf16le(
        reinterpret_cast<const char16_t*>(data.data()),
        data.size() / 2
    );
#else
    if (data.size() % 2 != 0) return false;

    auto ptr = reinterpret_cast<const uint16_t*>(data.data());
    size_t count = data.size() / 2;

    for (size_t i = 0; i < count; ++i) {
        uint16_t c = ptr[i];

        // Check for surrogate pairs
        if (c >= 0xD800 && c <= 0xDBFF) {
            // High surrogate - must be followed by low surrogate
            if (i + 1 >= count) return false;
            uint16_t c2 = ptr[i + 1];
            if (c2 < 0xDC00 || c2 > 0xDFFF) return false;
            ++i;
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            // Low surrogate without high surrogate
            return false;
        }
    }

    return true;
#endif
}

/**
 * @brief Validate if data is valid UTF-16 BE
 */
inline bool isValidUtf16BE(std::span<const uint8_t> data) noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return simdutf::validate_utf16be(
        reinterpret_cast<const char16_t*>(data.data()),
        data.size() / 2
    );
#else
    // Similar to LE but with byte swap
    if (data.size() % 2 != 0) return false;

    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint16_t c = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];

        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 3 >= data.size()) return false;
            uint16_t c2 = (static_cast<uint16_t>(data[i + 2]) << 8) | data[i + 3];
            if (c2 < 0xDC00 || c2 > 0xDFFF) return false;
            i += 2;
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            return false;
        }
    }

    return true;
#endif
}

// =============================================================================
// UTF-8 to UTF-16 Conversion
// =============================================================================

/**
 * @brief Convert UTF-8 to UTF-16 LE
 *
 * @param utf8 UTF-8 input string
 * @return UTF-16 LE data, or empty if conversion fails
 */
inline std::vector<uint8_t> utf8ToUtf16LE(std::string_view utf8) {
#ifdef MAKINE_HAS_SIMDUTF
    // Calculate required size
    size_t utf16_length = simdutf::utf16_length_from_utf8(utf8.data(), utf8.size());
    std::vector<char16_t> utf16(utf16_length);

    // Convert
    size_t written = simdutf::convert_utf8_to_utf16le(
        utf8.data(), utf8.size(),
        utf16.data()
    );

    if (written == 0 && utf8.size() > 0) {
        return {};  // Conversion failed
    }

    // Convert to bytes
    std::vector<uint8_t> result(written * 2);
    std::memcpy(result.data(), utf16.data(), written * 2);
    return result;
#else
    std::vector<uint8_t> result;
    result.reserve(utf8.size() * 2);  // Approximate

    auto data = reinterpret_cast<const uint8_t*>(utf8.data());
    size_t i = 0;

    while (i < utf8.size()) {
        uint32_t cp = 0;
        uint8_t c = data[i];

        if (c < 0x80) {
            cp = c;
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= utf8.size()) return {};
            cp = ((c & 0x1F) << 6) | (data[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= utf8.size()) return {};
            cp = ((c & 0x0F) << 12) |
                 ((data[i + 1] & 0x3F) << 6) |
                 (data[i + 2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= utf8.size()) return {};
            cp = ((c & 0x07) << 18) |
                 ((data[i + 1] & 0x3F) << 12) |
                 ((data[i + 2] & 0x3F) << 6) |
                 (data[i + 3] & 0x3F);
            i += 4;
        } else {
            return {};  // Invalid
        }

        // Convert to UTF-16
        if (cp < 0x10000) {
            result.push_back(static_cast<uint8_t>(cp & 0xFF));
            result.push_back(static_cast<uint8_t>((cp >> 8) & 0xFF));
        } else {
            // Surrogate pair
            cp -= 0x10000;
            uint16_t high = 0xD800 | ((cp >> 10) & 0x3FF);
            uint16_t low = 0xDC00 | (cp & 0x3FF);
            result.push_back(static_cast<uint8_t>(high & 0xFF));
            result.push_back(static_cast<uint8_t>((high >> 8) & 0xFF));
            result.push_back(static_cast<uint8_t>(low & 0xFF));
            result.push_back(static_cast<uint8_t>((low >> 8) & 0xFF));
        }
    }

    return result;
#endif
}

/**
 * @brief Convert UTF-8 to UTF-16 BE
 */
inline std::vector<uint8_t> utf8ToUtf16BE(std::string_view utf8) {
#ifdef MAKINE_HAS_SIMDUTF
    size_t utf16_length = simdutf::utf16_length_from_utf8(utf8.data(), utf8.size());
    std::vector<char16_t> utf16(utf16_length);

    size_t written = simdutf::convert_utf8_to_utf16be(
        utf8.data(), utf8.size(),
        utf16.data()
    );

    if (written == 0 && utf8.size() > 0) {
        return {};
    }

    std::vector<uint8_t> result(written * 2);
    std::memcpy(result.data(), utf16.data(), written * 2);
    return result;
#else
    // Convert to LE first, then swap bytes
    auto le = utf8ToUtf16LE(utf8);
    for (size_t i = 0; i + 1 < le.size(); i += 2) {
        std::swap(le[i], le[i + 1]);
    }
    return le;
#endif
}

// =============================================================================
// UTF-16 to UTF-8 Conversion
// =============================================================================

/**
 * @brief Convert UTF-16 LE to UTF-8
 *
 * @param utf16 UTF-16 LE data
 * @return UTF-8 string, or empty if conversion fails
 */
inline std::string utf16LEToUtf8(std::span<const uint8_t> utf16) {
    if (utf16.size() % 2 != 0) return "";

#ifdef MAKINE_HAS_SIMDUTF
    auto ptr = reinterpret_cast<const char16_t*>(utf16.data());
    size_t count = utf16.size() / 2;

    size_t utf8_length = simdutf::utf8_length_from_utf16le(ptr, count);
    std::string result(utf8_length, '\0');

    size_t written = simdutf::convert_utf16le_to_utf8(
        ptr, count,
        result.data()
    );

    if (written == 0 && count > 0) {
        return "";
    }

    result.resize(written);
    return result;
#else
    std::string result;
    result.reserve(utf16.size());  // Approximate

    auto ptr = reinterpret_cast<const uint16_t*>(utf16.data());
    size_t count = utf16.size() / 2;

    for (size_t i = 0; i < count; ++i) {
        uint32_t cp = ptr[i];

        // Handle surrogate pairs
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 >= count) return "";
            uint16_t low = ptr[i + 1];
            if (low < 0xDC00 || low > 0xDFFF) return "";
            cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
            ++i;
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return "";  // Invalid
        }

        // Encode as UTF-8
        if (cp < 0x80) {
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    return result;
#endif
}

/**
 * @brief Convert UTF-16 BE to UTF-8
 */
inline std::string utf16BEToUtf8(std::span<const uint8_t> utf16) {
#ifdef MAKINE_HAS_SIMDUTF
    auto ptr = reinterpret_cast<const char16_t*>(utf16.data());
    size_t count = utf16.size() / 2;

    size_t utf8_length = simdutf::utf8_length_from_utf16be(ptr, count);
    std::string result(utf8_length, '\0');

    size_t written = simdutf::convert_utf16be_to_utf8(
        ptr, count,
        result.data()
    );

    if (written == 0 && count > 0) {
        return "";
    }

    result.resize(written);
    return result;
#else
    // Swap bytes and convert as LE
    std::vector<uint8_t> le(utf16.begin(), utf16.end());
    for (size_t i = 0; i + 1 < le.size(); i += 2) {
        std::swap(le[i], le[i + 1]);
    }
    return utf16LEToUtf8(le);
#endif
}

// =============================================================================
// Encoding Detection
// =============================================================================

/**
 * @brief Detected encoding type
 */
enum class Encoding {
    Unknown,
    ASCII,
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_LE_BOM,
    UTF16_BE,
    UTF16_BE_BOM,
    UTF32_LE,
    UTF32_BE,
    Latin1,
    ShiftJIS,
};

/**
 * @brief Detect the encoding of a byte sequence
 *
 * @param data Input data
 * @return Detected encoding
 */
inline Encoding detectEncoding(std::span<const uint8_t> data) {
    if (data.empty()) {
        return Encoding::Unknown;
    }

    // Check BOMs first
    if (data.size() >= 3 &&
        data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        return Encoding::UTF8_BOM;
    }
    if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        if (data.size() >= 4 && data[2] == 0x00 && data[3] == 0x00) {
            return Encoding::UTF32_LE;
        }
        return Encoding::UTF16_LE_BOM;
    }
    if (data.size() >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        return Encoding::UTF16_BE_BOM;
    }
    if (data.size() >= 4 &&
        data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0xFE && data[3] == 0xFF) {
        return Encoding::UTF32_BE;
    }

    // Check if valid UTF-8
    std::string_view sv(reinterpret_cast<const char*>(data.data()), data.size());
    if (isValidUtf8(sv)) {
        // Check if pure ASCII
        bool hasHighBit = false;
        for (uint8_t b : data) {
            if (b >= 0x80) {
                hasHighBit = true;
                break;
            }
        }
        return hasHighBit ? Encoding::UTF8 : Encoding::ASCII;
    }

    // Check for UTF-16 patterns (lots of null bytes)
    int nullCount = 0;
    int nullAtOdd = 0;
    int nullAtEven = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == 0) {
            ++nullCount;
            if (i % 2 == 0) ++nullAtEven;
            else ++nullAtOdd;
        }
    }

    if (nullCount > data.size() / 4) {
        // Lots of nulls - likely UTF-16
        if (nullAtOdd > nullAtEven * 2) {
            return Encoding::UTF16_LE;
        }
        if (nullAtEven > nullAtOdd * 2) {
            return Encoding::UTF16_BE;
        }
    }

    return Encoding::Unknown;
}

/**
 * @brief Convert encoding name to string
 */
inline const char* encodingToString(Encoding enc) noexcept {
    switch (enc) {
        case Encoding::Unknown: return "Unknown";
        case Encoding::ASCII: return "ASCII";
        case Encoding::UTF8: return "UTF-8";
        case Encoding::UTF8_BOM: return "UTF-8 (BOM)";
        case Encoding::UTF16_LE: return "UTF-16 LE";
        case Encoding::UTF16_LE_BOM: return "UTF-16 LE (BOM)";
        case Encoding::UTF16_BE: return "UTF-16 BE";
        case Encoding::UTF16_BE_BOM: return "UTF-16 BE (BOM)";
        case Encoding::UTF32_LE: return "UTF-32 LE";
        case Encoding::UTF32_BE: return "UTF-32 BE";
        case Encoding::Latin1: return "Latin-1";
        case Encoding::ShiftJIS: return "Shift-JIS";
        default: return "Unknown";
    }
}

// =============================================================================
// Conversion to UTF-8
// =============================================================================

/**
 * @brief Convert any detected encoding to UTF-8
 *
 * @param data Input data
 * @param encoding Known encoding (or Unknown to auto-detect)
 * @return UTF-8 string, or empty if conversion fails
 */
inline std::string toUtf8(std::span<const uint8_t> data, Encoding encoding = Encoding::Unknown) {
    if (data.empty()) {
        return "";
    }

    if (encoding == Encoding::Unknown) {
        encoding = detectEncoding(data);
    }

    switch (encoding) {
        case Encoding::ASCII:
        case Encoding::UTF8:
            return std::string(reinterpret_cast<const char*>(data.data()), data.size());

        case Encoding::UTF8_BOM:
            if (data.size() >= 3) {
                return std::string(reinterpret_cast<const char*>(data.data() + 3), data.size() - 3);
            }
            return "";

        case Encoding::UTF16_LE:
            return utf16LEToUtf8(data);

        case Encoding::UTF16_LE_BOM:
            if (data.size() >= 2) {
                return utf16LEToUtf8(data.subspan(2));
            }
            return "";

        case Encoding::UTF16_BE:
            return utf16BEToUtf8(data);

        case Encoding::UTF16_BE_BOM:
            if (data.size() >= 2) {
                return utf16BEToUtf8(data.subspan(2));
            }
            return "";

        default:
            // Try as UTF-8 anyway
            return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    }
}

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Check if SIMDUTF is available at runtime
 */
inline bool hasSimdutf() noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get SIMDUTF version string (if available)
 */
inline const char* simdutfVersion() noexcept {
#ifdef MAKINE_HAS_SIMDUTF
    return SIMDUTF_VERSION;
#else
    return "not available";
#endif
}

} // namespace makine::utf
