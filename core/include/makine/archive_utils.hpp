/**
 * @file archive_utils.hpp
 * @brief Archive extraction utilities with multiple backend support
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides unified interface for archive extraction.
 * Supports multiple backends:
 * - MAKINE_HAS_BIT7Z - bit7z library (7-zip)
 * - MAKINE_HAS_LIBARCHIVE - libarchive
 * - MAKINE_HAS_MINIZIP - minizip-ng (ZIP only)
 *
 * Fallback: ZIP extraction using minizip-ng (always available via vcpkg).
 */

#pragma once

#include "features.hpp"
#include "constants.hpp"
#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef MAKINE_HAS_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitextractor.hpp>
#endif

#ifdef MAKINE_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace makine::archive {

namespace fs = std::filesystem;

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Supported archive formats
 */
enum class Format {
    Unknown,
    ZIP,
    SevenZip,
    RAR,
    TAR,
    GZIP,
    BZIP2,
    XZ,
    LZMA
};

/**
 * @brief Convert format to string
 */
inline const char* formatToString(Format fmt) noexcept {
    switch (fmt) {
        case Format::ZIP: return "ZIP";
        case Format::SevenZip: return "7z";
        case Format::RAR: return "RAR";
        case Format::TAR: return "TAR";
        case Format::GZIP: return "GZIP";
        case Format::BZIP2: return "BZIP2";
        case Format::XZ: return "XZ";
        case Format::LZMA: return "LZMA";
        default: return "Unknown";
    }
}

/**
 * @brief Archive entry information
 */
struct ArchiveEntry {
    std::string path;
    uint64_t compressedSize = 0;
    uint64_t uncompressedSize = 0;
    bool isDirectory = false;
    bool isEncrypted = false;
    int64_t modTime = 0;  ///< Unix timestamp
};

/**
 * @brief Progress callback
 * @param bytesProcessed Bytes processed so far
 * @param totalBytes Total bytes (0 if unknown)
 * @return true to continue, false to cancel
 */
using ProgressCallback = std::function<bool(uint64_t bytesProcessed, uint64_t totalBytes)>;

// =============================================================================
// Format Detection
// =============================================================================

/**
 * @brief Detect archive format from magic bytes
 */
inline Format detectFormat(std::span<const uint8_t> data) {
    if (data.size() < 4) {
        return Format::Unknown;
    }

    // ZIP: PK\x03\x04 or PK\x05\x06 (empty) or PK\x07\x08 (spanned)
    if (data[0] == 0x50 && data[1] == 0x4B) {
        return Format::ZIP;
    }

    // 7z: 7z\xBC\xAF\x27\x1C
    if (data.size() >= 6 &&
        data[0] == 0x37 && data[1] == 0x7A &&
        data[2] == 0xBC && data[3] == 0xAF &&
        data[4] == 0x27 && data[5] == 0x1C) {
        return Format::SevenZip;
    }

    // RAR: Rar!\x1A\x07
    if (data.size() >= 6 &&
        data[0] == 0x52 && data[1] == 0x61 &&
        data[2] == 0x72 && data[3] == 0x21 &&
        data[4] == 0x1A && data[5] == 0x07) {
        return Format::RAR;
    }

    // GZIP: \x1F\x8B
    if (data[0] == 0x1F && data[1] == 0x8B) {
        return Format::GZIP;
    }

    // BZIP2: BZ
    if (data[0] == 0x42 && data[1] == 0x5A) {
        return Format::BZIP2;
    }

    // XZ: \xFD7zXZ\x00
    if (data.size() >= 6 &&
        data[0] == 0xFD && data[1] == 0x37 &&
        data[2] == 0x7A && data[3] == 0x58 &&
        data[4] == 0x5A && data[5] == 0x00) {
        return Format::XZ;
    }

    // TAR: Check for tar header at position 257
    // (actually ustar magic)
    // Simplified: just check for reasonable ASCII in first bytes

    return Format::Unknown;
}

/**
 * @brief Detect format from file
 */
inline Format detectFormat(const fs::path& archivePath) {
    std::ifstream file(archivePath, std::ios::binary);
    if (!file) {
        return Format::Unknown;
    }

    std::array<uint8_t, 8> header;
    file.read(reinterpret_cast<char*>(header.data()), header.size());

    return detectFormat(std::span<const uint8_t>(header.data(), file.gcount()));
}

// =============================================================================
// Archive Interface
// =============================================================================

/**
 * @brief Abstract archive interface
 */
class IArchive {
public:
    virtual ~IArchive() = default;

    /**
     * @brief List all entries in the archive
     */
    [[nodiscard]] virtual Result<std::vector<ArchiveEntry>> list() = 0;

    /**
     * @brief Extract entire archive to directory
     *
     * @param destDir Destination directory
     * @param password Optional password for encrypted archives
     * @param progress Optional progress callback
     */
    [[nodiscard]] virtual Result<void> extractAll(
        const fs::path& destDir,
        const std::optional<std::string>& password = std::nullopt,
        ProgressCallback progress = nullptr
    ) = 0;

    /**
     * @brief Extract single file from archive
     *
     * @param entryPath Path within archive
     * @param destPath Destination file path
     * @param password Optional password
     */
    [[nodiscard]] virtual Result<void> extractFile(
        const std::string& entryPath,
        const fs::path& destPath,
        const std::optional<std::string>& password = std::nullopt
    ) = 0;

    /**
     * @brief Extract file to memory
     */
    [[nodiscard]] virtual Result<std::vector<uint8_t>> extractToMemory(
        const std::string& entryPath,
        const std::optional<std::string>& password = std::nullopt
    ) = 0;

    /**
     * @brief Check if archive contains a file
     */
    virtual bool contains(const std::string& entryPath) = 0;

    /**
     * @brief Get archive format
     */
    virtual Format format() const = 0;
};

// =============================================================================
// libarchive Implementation
// =============================================================================

#ifdef MAKINE_HAS_LIBARCHIVE

class LibArchiveExtractor : public IArchive {
public:
    explicit LibArchiveExtractor(fs::path archivePath)
        : archivePath_(std::move(archivePath)) {}

    [[nodiscard]] Result<std::vector<ArchiveEntry>> list() override {
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (archive_read_open_filename(a, archivePath_.string().c_str(), kArchiveReadBufferSize) != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            return Error(ErrorCode::FileReadError, err);
        }

        std::vector<ArchiveEntry> entries;
        struct archive_entry* entry;

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            ArchiveEntry e;
            e.path = archive_entry_pathname(entry);
            e.compressedSize = archive_entry_size(entry);  // Not always accurate
            e.uncompressedSize = archive_entry_size(entry);
            e.isDirectory = archive_entry_filetype(entry) == AE_IFDIR;
            e.isEncrypted = archive_entry_is_encrypted(entry);
            e.modTime = archive_entry_mtime(entry);
            entries.push_back(std::move(e));

            archive_read_data_skip(a);
        }

        archive_read_free(a);
        return entries;
    }

    [[nodiscard]] Result<void> extractAll(
        const fs::path& destDir,
        const std::optional<std::string>& password,
        ProgressCallback progress
    ) override {
        struct archive* a = archive_read_new();
        struct archive* ext = archive_write_disk_new();

        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (password) {
            archive_read_add_passphrase(a, password->c_str());
        }

        archive_write_disk_set_options(ext,
            ARCHIVE_EXTRACT_TIME |
            ARCHIVE_EXTRACT_PERM |
            ARCHIVE_EXTRACT_ACL |
            ARCHIVE_EXTRACT_FFLAGS
        );
        archive_write_disk_set_standard_lookup(ext);

        if (archive_read_open_filename(a, archivePath_.string().c_str(), kArchiveReadBufferSize) != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            archive_write_free(ext);
            return Error(ErrorCode::FileReadError, err);
        }

        uint64_t totalProcessed = 0;
        struct archive_entry* entry;
        int r;

        while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
            // Update path to destination
            fs::path fullPath = destDir / archive_entry_pathname(entry);
            archive_entry_set_pathname(entry, fullPath.string().c_str());

            r = archive_write_header(ext, entry);
            if (r != ARCHIVE_OK) {
                // Non-fatal, continue
            } else {
                // Copy data
                const void* buff;
                size_t size;
                la_int64_t offset;

                while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
                    archive_write_data_block(ext, buff, size, offset);
                    totalProcessed += size;

                    if (progress && !progress(totalProcessed, 0)) {
                        archive_read_free(a);
                        archive_write_free(ext);
                        return Error(ErrorCode::OperationCancelled, "Extraction cancelled");
                    }
                }

                archive_write_finish_entry(ext);
            }
        }

        archive_read_free(a);
        archive_write_free(ext);

        if (r != ARCHIVE_EOF) {
            return Error(ErrorCode::FileReadError, "Archive read error");
        }

        return {};
    }

    [[nodiscard]] Result<void> extractFile(
        const std::string& entryPath,
        const fs::path& destPath,
        const std::optional<std::string>& password
    ) override {
        auto data = extractToMemory(entryPath, password);
        if (!data) {
            return data.error();
        }

        // Create parent directories
        std::error_code ec;
        fs::create_directories(destPath.parent_path(), ec);

        std::ofstream out(destPath, std::ios::binary);
        if (!out) {
            return Error(ErrorCode::FileWriteError, "Failed to create output file");
        }

        out.write(reinterpret_cast<const char*>(data->data()), data->size());
        return {};
    }

    [[nodiscard]] Result<std::vector<uint8_t>> extractToMemory(
        const std::string& entryPath,
        const std::optional<std::string>& password
    ) override {
        struct archive* a = archive_read_new();
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (password) {
            archive_read_add_passphrase(a, password->c_str());
        }

        if (archive_read_open_filename(a, archivePath_.string().c_str(), kArchiveReadBufferSize) != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            return Error(ErrorCode::FileReadError, err);
        }

        struct archive_entry* entry;
        std::vector<uint8_t> result;

        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            if (entryPath == archive_entry_pathname(entry)) {
                size_t size = archive_entry_size(entry);
                result.resize(size);

                ssize_t bytesRead = archive_read_data(a, result.data(), size);
                if (bytesRead < 0) {
                    std::string err = archive_error_string(a);
                    archive_read_free(a);
                    return Error(ErrorCode::FileReadError, err);
                }

                result.resize(bytesRead);
                archive_read_free(a);
                return result;
            }

            archive_read_data_skip(a);
        }

        archive_read_free(a);
        return Error(ErrorCode::FileNotFound, "Entry not found in archive");
    }

    bool contains(const std::string& entryPath) override {
        auto entries = list();
        if (!entries) return false;

        for (const auto& e : *entries) {
            if (e.path == entryPath) return true;
        }
        return false;
    }

    Format format() const override {
        return detectFormat(archivePath_);
    }

private:
    fs::path archivePath_;
};

#endif // MAKINE_HAS_LIBARCHIVE

// =============================================================================
// bit7z Implementation
// =============================================================================

#ifdef MAKINE_HAS_BIT7Z

class Bit7zExtractor : public IArchive {
public:
    explicit Bit7zExtractor(fs::path archivePath, const fs::path& lib7zPath = "")
        : archivePath_(std::move(archivePath)) {
        // Initialize bit7z library
        // Note: bit7z requires path to 7z.dll
        lib_ = std::make_unique<bit7z::Bit7zLibrary>(
            lib7zPath.empty() ? "7z.dll" : lib7zPath.string()
        );
    }

    [[nodiscard]] Result<std::vector<ArchiveEntry>> list() override {
        try {
            bit7z::BitArchiveReader reader(*lib_, archivePath_.wstring());

            std::vector<ArchiveEntry> entries;
            for (const auto& item : reader) {
                ArchiveEntry e;
                e.path = item.path();
                e.compressedSize = item.packSize();
                e.uncompressedSize = item.size();
                e.isDirectory = item.isDir();
                e.isEncrypted = item.isEncrypted();
                entries.push_back(std::move(e));
            }
            return entries;
        } catch (const bit7z::BitException& ex) {
            return Error(ErrorCode::FileReadError, ex.what());
        }
    }

    [[nodiscard]] Result<void> extractAll(
        const fs::path& destDir,
        const std::optional<std::string>& password,
        ProgressCallback progress
    ) override {
        try {
            bit7z::BitArchiveReader reader(*lib_, archivePath_.wstring());

            if (password) {
                reader.setPassword(std::wstring(password->begin(), password->end()));
            }

            if (progress) {
                reader.setProgressCallback([&progress](uint64_t done) -> bool {
                    return progress(done, 0);
                });
            }

            reader.extract(destDir.wstring());
            return {};
        } catch (const bit7z::BitException& ex) {
            return Error(ErrorCode::FileReadError, ex.what());
        }
    }

    [[nodiscard]] Result<void> extractFile(
        const std::string& entryPath,
        const fs::path& destPath,
        const std::optional<std::string>& password
    ) override {
        try {
            bit7z::BitArchiveReader reader(*lib_, archivePath_.wstring());

            if (password) {
                reader.setPassword(std::wstring(password->begin(), password->end()));
            }

            std::error_code ec;
            fs::create_directories(destPath.parent_path(), ec);

            reader.extractTo(destPath.wstring(), {std::wstring(entryPath.begin(), entryPath.end())});
            return {};
        } catch (const bit7z::BitException& ex) {
            return Error(ErrorCode::FileReadError, ex.what());
        }
    }

    [[nodiscard]] Result<std::vector<uint8_t>> extractToMemory(
        const std::string& entryPath,
        const std::optional<std::string>& password
    ) override {
        try {
            bit7z::BitArchiveReader reader(*lib_, archivePath_.wstring());

            if (password) {
                reader.setPassword(std::wstring(password->begin(), password->end()));
            }

            std::vector<bit7z::byte_t> buffer;
            reader.extractTo(buffer, {std::wstring(entryPath.begin(), entryPath.end())});

            return std::vector<uint8_t>(buffer.begin(), buffer.end());
        } catch (const bit7z::BitException& ex) {
            return Error(ErrorCode::FileReadError, ex.what());
        }
    }

    bool contains(const std::string& entryPath) override {
        auto entries = list();
        if (!entries) return false;

        for (const auto& e : *entries) {
            if (e.path == entryPath) return true;
        }
        return false;
    }

    Format format() const override {
        return detectFormat(archivePath_);
    }

private:
    fs::path archivePath_;
    std::unique_ptr<bit7z::Bit7zLibrary> lib_;
};

#endif // MAKINE_HAS_BIT7Z

// =============================================================================
// Factory
// =============================================================================

/**
 * @brief Create appropriate archive extractor
 */
[[nodiscard]] inline std::unique_ptr<IArchive> openArchive(const fs::path& archivePath) {
    Format fmt = detectFormat(archivePath);

#ifdef MAKINE_HAS_BIT7Z
    // bit7z supports most formats
    if (fmt == Format::SevenZip || fmt == Format::RAR || fmt == Format::ZIP) {
        return std::make_unique<Bit7zExtractor>(archivePath);
    }
#endif

#ifdef MAKINE_HAS_LIBARCHIVE
    // libarchive supports tar, gzip, bzip2, xz, zip
    return std::make_unique<LibArchiveExtractor>(archivePath);
#endif

    // No suitable backend available
    return nullptr;
}

/**
 * @brief Check available archive backends
 */
[[nodiscard]] inline std::vector<std::string> availableBackends() {
    std::vector<std::string> backends;

#ifdef MAKINE_HAS_BIT7Z
    backends.push_back("bit7z");
#endif

#ifdef MAKINE_HAS_LIBARCHIVE
    backends.push_back("libarchive");
#endif

#ifdef MAKINE_HAS_MINIZIP
    backends.push_back("minizip");
#endif

    return backends;
}

/**
 * @brief Check if format is supported
 */
[[nodiscard]] inline bool isFormatSupported(Format fmt) {
#ifdef MAKINE_HAS_BIT7Z
    // bit7z supports almost everything
    return true;
#endif

#ifdef MAKINE_HAS_LIBARCHIVE
    switch (fmt) {
        case Format::ZIP:
        case Format::TAR:
        case Format::GZIP:
        case Format::BZIP2:
        case Format::XZ:
            return true;
        default:
            break;
    }
#endif

#ifdef MAKINE_HAS_MINIZIP
    if (fmt == Format::ZIP) return true;
#endif

    return false;
}

} // namespace makine::archive
