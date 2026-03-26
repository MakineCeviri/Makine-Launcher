#pragma once

/**
 * @file mio_utils.hpp
 * @brief Memory-mapped I/O utilities with MIO integration
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides a unified API for memory-mapped file access.
 * Uses MIO when available, falls back to std::ifstream otherwise.
 *
 * Benefits of memory mapping:
 * - Zero-copy file reading
 * - OS-level page caching
 * - Efficient random access
 * - Reduced memory allocations
 *
 * Usage:
 *   auto result = mio::mapFile(path);
 *   if (result) {
 *       auto view = result->view();
 *       // Use view.data() and view.size()
 *   }
 */

#include "makine/error.hpp"
#include "makine/features.hpp"
#include "makine/logging.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef MAKINE_HAS_MIO
#include <mio/mmap.hpp>
#endif

namespace makine::mio_utils {

namespace fs = std::filesystem;

// =============================================================================
// FILE VIEW - UNIFIED INTERFACE
// =============================================================================

/**
 * @brief Read-only view into file data
 *
 * Provides a uniform interface regardless of whether the data is
 * memory-mapped or loaded into a vector.
 */
class FileView {
public:
    FileView() = default;

    /// Construct from raw pointer and size
    FileView(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    /// Construct from vector (takes ownership)
    explicit FileView(std::vector<uint8_t> buffer)
        : ownedBuffer_(std::make_shared<std::vector<uint8_t>>(std::move(buffer))) {
        data_ = ownedBuffer_->data();
        size_ = ownedBuffer_->size();
    }

    [[nodiscard]] const uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] std::span<const uint8_t> span() const noexcept {
        return {data_, size_};
    }

    [[nodiscard]] std::string_view stringView() const noexcept {
        return {reinterpret_cast<const char*>(data_), size_};
    }

    /// Get a subview starting at offset
    /// Note: Subviews share ownership with the parent to prevent dangling pointers
    [[nodiscard]] FileView subview(size_t offset, size_t length = SIZE_MAX) const {
        if (offset >= size_) {
            return {};
        }
        size_t actualLength = std::min(length, size_ - offset);
        FileView result(data_ + offset, actualLength);
        result.ownedBuffer_ = ownedBuffer_;  // Share ownership
        return result;
    }

    /// Read value at offset (bounds-checked)
    template <typename T>
    [[nodiscard]] Result<T> readAt(size_t offset) const {
        if (offset + sizeof(T) > size_) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Read beyond file bounds: offset " + std::to_string(offset) +
                ", size " + std::to_string(sizeof(T)) +
                ", file size " + std::to_string(size_)));
        }
        T value;
        std::memcpy(&value, data_ + offset, sizeof(T));
        return value;
    }

    /// Read array at offset (bounds-checked)
    template <typename T>
    [[nodiscard]] Result<std::span<const T>> readArrayAt(size_t offset, size_t count) const {
        size_t byteSize = count * sizeof(T);
        if (offset + byteSize > size_) {
            return std::unexpected(Error(ErrorCode::InvalidFormat,
                "Read beyond file bounds"));
        }
        return std::span<const T>(reinterpret_cast<const T*>(data_ + offset), count);
    }

    /// Search for byte pattern
    [[nodiscard]] size_t find(const uint8_t* pattern, size_t patternSize,
                              size_t startOffset = 0) const {
        if (patternSize == 0 || startOffset + patternSize > size_) {
            return SIZE_MAX;
        }

        for (size_t i = startOffset; i <= size_ - patternSize; ++i) {
            if (std::memcmp(data_ + i, pattern, patternSize) == 0) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    /// Search for string pattern
    [[nodiscard]] size_t find(std::string_view pattern, size_t startOffset = 0) const {
        return find(reinterpret_cast<const uint8_t*>(pattern.data()),
                    pattern.size(), startOffset);
    }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    std::shared_ptr<std::vector<uint8_t>> ownedBuffer_;
};

// =============================================================================
// MAPPED FILE - MIO IMPLEMENTATION
// =============================================================================

#ifdef MAKINE_HAS_MIO

/**
 * @brief Memory-mapped file using MIO
 */
class MappedFile {
public:
    MappedFile() = default;

    /// Map entire file
    [[nodiscard]] static Result<MappedFile> open(const fs::path& path) {
        if (!fs::exists(path)) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "File not found: " + path.string()));
        }

        MappedFile mapped;
        std::error_code ec;

        // Use make_mmap_source factory function (mio API)
        mapped.mmap_ = ::mio::make_mmap_source(path.string(), ec);

        if (ec) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot map file: " + path.string() + " (" + ec.message() + ")"));
        }

        mapped.path_ = path;
        MAKINE_LOG_TRACE(log::CORE, "Memory-mapped file: {} ({} bytes)",
                           path.string(), mapped.mmap_.size());

        return mapped;
    }

    /// Map portion of file
    [[nodiscard]] static Result<MappedFile> open(const fs::path& path, size_t offset, size_t length) {
        if (!fs::exists(path)) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "File not found: " + path.string()));
        }

        auto fileSize = fs::file_size(path);
        if (offset >= fileSize) {
            return std::unexpected(Error(ErrorCode::InvalidArgument,
                "Offset beyond file size"));
        }

        MappedFile mapped;
        std::error_code ec;

        size_t actualLength = std::min(length, static_cast<size_t>(fileSize - offset));
        // Use make_mmap_source factory function with offset and length (mio API)
        mapped.mmap_ = ::mio::make_mmap_source(path.string(), offset, actualLength, ec);

        if (ec) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot map file: " + path.string() + " (" + ec.message() + ")"));
        }

        mapped.path_ = path;
        return mapped;
    }

    [[nodiscard]] FileView view() const {
        return FileView(reinterpret_cast<const uint8_t*>(mmap_.data()), mmap_.size());
    }

    [[nodiscard]] const uint8_t* data() const {
        return reinterpret_cast<const uint8_t*>(mmap_.data());
    }

    [[nodiscard]] size_t size() const { return mmap_.size(); }
    [[nodiscard]] bool empty() const { return mmap_.empty(); }
    [[nodiscard]] bool isValid() const { return mmap_.is_open(); }
    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    ::mio::mmap_source mmap_;
    fs::path path_;
};

#else  // !MAKINE_HAS_MIO

// =============================================================================
// MAPPED FILE - FALLBACK IMPLEMENTATION
// =============================================================================

/**
 * @brief File loaded into memory (fallback when MIO unavailable)
 */
class MappedFile {
public:
    MappedFile() = default;

    /// Load entire file into memory
    [[nodiscard]] static Result<MappedFile> open(const fs::path& path) {
        if (!fs::exists(path)) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "File not found: " + path.string()));
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file: " + path.string()));
        }

        auto size = file.tellg();
        file.seekg(0);

        MappedFile mapped;
        mapped.buffer_.resize(static_cast<size_t>(size));

        if (!file.read(reinterpret_cast<char*>(mapped.buffer_.data()), size)) {
            return std::unexpected(Error(ErrorCode::IOError,
                "Failed to read file: " + path.string()));
        }

        mapped.path_ = path;
        MAKINE_LOG_TRACE(log::CORE, "Loaded file into memory: {} ({} bytes)",
                           path.string(), mapped.buffer_.size());

        return mapped;
    }

    /// Load portion of file into memory
    [[nodiscard]] static Result<MappedFile> open(const fs::path& path, size_t offset, size_t length) {
        if (!fs::exists(path)) {
            return std::unexpected(Error(ErrorCode::FileNotFound,
                "File not found: " + path.string()));
        }

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::unexpected(Error(ErrorCode::FileAccessDenied,
                "Cannot open file: " + path.string()));
        }

        auto fileSize = file.tellg();
        if (offset >= static_cast<size_t>(fileSize)) {
            return std::unexpected(Error(ErrorCode::InvalidArgument,
                "Offset beyond file size"));
        }

        size_t actualLength = std::min(length,
            static_cast<size_t>(fileSize) - offset);

        file.seekg(static_cast<std::streamoff>(offset));

        MappedFile mapped;
        mapped.buffer_.resize(actualLength);

        if (!file.read(reinterpret_cast<char*>(mapped.buffer_.data()),
                       static_cast<std::streamsize>(actualLength))) {
            return std::unexpected(Error(ErrorCode::IOError,
                "Failed to read file: " + path.string()));
        }

        mapped.path_ = path;
        return mapped;
    }

    [[nodiscard]] FileView view() const {
        return FileView(buffer_.data(), buffer_.size());
    }

    [[nodiscard]] const uint8_t* data() const { return buffer_.data(); }
    [[nodiscard]] size_t size() const { return buffer_.size(); }
    [[nodiscard]] bool empty() const { return buffer_.empty(); }
    [[nodiscard]] bool isValid() const { return !buffer_.empty(); }
    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    std::vector<uint8_t> buffer_;
    fs::path path_;
};

#endif  // MAKINE_HAS_MIO

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Map a file for reading
 */
inline Result<MappedFile> mapFile(const fs::path& path) {
    return MappedFile::open(path);
}

/**
 * @brief Map a portion of a file
 */
inline Result<MappedFile> mapFile(const fs::path& path, size_t offset, size_t length) {
    return MappedFile::open(path, offset, length);
}

/**
 * @brief Read entire file into a vector (always copies, no mapping)
 */
inline Result<std::vector<uint8_t>> readFile(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "File not found: " + path.string()));
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open file: " + path.string()));
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::unexpected(Error(ErrorCode::IOError,
            "Failed to read file: " + path.string()));
    }

    return buffer;
}

/**
 * @brief Read first N bytes of a file
 */
inline Result<std::vector<uint8_t>> readFileHead(const fs::path& path, size_t maxBytes) {
    if (!fs::exists(path)) {
        return std::unexpected(Error(ErrorCode::FileNotFound,
            "File not found: " + path.string()));
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot open file: " + path.string()));
    }

    auto fileSize = fs::file_size(path);
    size_t readSize = std::min(maxBytes, static_cast<size_t>(fileSize));

    std::vector<uint8_t> buffer(readSize);

    if (!file.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(readSize))) {
        return std::unexpected(Error(ErrorCode::IOError,
            "Failed to read file: " + path.string()));
    }

    return buffer;
}

/**
 * @brief Check if MIO (memory mapping) is available
 */
[[nodiscard]] inline constexpr bool hasMio() {
#ifdef MAKINE_HAS_MIO
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get memory mapping backend info
 */
[[nodiscard]] inline std::string backendInfo() {
#ifdef MAKINE_HAS_MIO
    return "MIO (memory-mapped I/O)";
#else
    return "std::ifstream (buffered I/O)";
#endif
}

}  // namespace makine::mio_utils
