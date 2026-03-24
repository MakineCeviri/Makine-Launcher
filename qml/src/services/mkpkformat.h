/**
 * @file mkpkformat.h
 * @brief MKPK binary format: decrypt (AES-256-GCM) + decompress (zstd) + tar extract
 * @copyright (c) 2026 MakineCeviri Team
 *
 * MKPK format v1 (encrypted — translation packages):
 *   [Magic: 4B "MKPK"] [Version: 1B = 0x01] [Nonce: 12B] [Ciphertext+AuthTag: NB]
 *
 * MKPK format v2 (unencrypted — plugins, open-source content):
 *   [Magic: 4B "MKPK"] [Version: 1B = 0x02] [zstd compressed tar data]
 *
 * Inner payload: tar.zst (zstandard compressed tar archive)
 * v1 encryption: AES-256-GCM with MAGIC as Associated Data
 * v2: no encryption, direct zstd + tar
 *
 * This header is self-contained: no Qt dependency for core logic.
 * Only the extract_tar() helper uses filesystem operations.
 */

#pragma once

#ifndef MAKINE_UI_ONLY

#include "encryption_key.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <spdlog/fmt/fmt.h>

#include <openssl/evp.h>
#include <zstd.h>

namespace makine::mkpk {

// ============================================================================
// Error type
// ============================================================================

struct MkpkError {
    std::string message;
    explicit MkpkError(std::string msg) : message(std::move(msg)) {}
};

// ============================================================================
// AES-256-GCM Decryption
// ============================================================================

/// Decrypt MKPK ciphertext with AES-256-GCM.
/// @param key       32-byte AES key
/// @param nonce     12-byte GCM nonce
/// @param aad       Associated data (MKPK magic bytes)
/// @param aad_len   Length of AAD
/// @param ct        Ciphertext + 16-byte GCM auth tag appended
/// @param ct_len    Total length (ciphertext + tag)
/// @param out       Output buffer (must be at least ct_len - 16 bytes)
/// @return true on success, false if authentication fails (tampered data)
inline bool aes256gcm_decrypt(
    const uint8_t* key, const uint8_t* nonce,
    const uint8_t* aad, size_t aad_len,
    const uint8_t* ct, size_t ct_len,
    uint8_t* out)
{
    if (ct_len < crypto::MKPK_TAG_SIZE)
        return false;

    const size_t data_len = ct_len - crypto::MKPK_TAG_SIZE;
    const uint8_t* tag = ct + data_len;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool ok = false;
    int outlen = 0;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN,
                                static_cast<int>(crypto::MKPK_NONCE_SIZE), nullptr) != 1)
            break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) != 1)
            break;

        // Associated data (not encrypted, but authenticated)
        if (aad && aad_len > 0) {
            if (EVP_DecryptUpdate(ctx, nullptr, &outlen, aad, static_cast<int>(aad_len)) != 1)
                break;
        }

        // Decrypt ciphertext
        if (EVP_DecryptUpdate(ctx, out, &outlen, ct, static_cast<int>(data_len)) != 1)
            break;

        // Set expected auth tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                                static_cast<int>(crypto::MKPK_TAG_SIZE),
                                const_cast<uint8_t*>(tag)) != 1)
            break;

        // Verify tag — if this fails, data was tampered
        int final_len = 0;
        ok = (EVP_DecryptFinal_ex(ctx, out + outlen, &final_len) > 0);
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

// ============================================================================
// MKPK Parse + Decrypt
// ============================================================================

/// Parse and decrypt a complete MKPK file in memory.
/// Returns the compressed (tar.zst) payload.
inline std::vector<uint8_t> decrypt_mkpk(
    const uint8_t* data, size_t size,
    MkpkError* err = nullptr)
{
    // Validate minimum size
    if (size < crypto::MKPK_HEADER_SIZE + crypto::MKPK_TAG_SIZE) {
        if (err) *err = MkpkError("File too small for MKPK format");
        return {};
    }

    // Validate magic
    if (std::memcmp(data, crypto::MKPK_MAGIC, 4) != 0) {
        if (err) *err = MkpkError("Invalid MKPK magic bytes");
        return {};
    }

    // Validate version
    if (data[4] != crypto::MKPK_VERSION) {
        if (err) *err = MkpkError(fmt::format("Unsupported MKPK version: {}", data[4]));
        return {};
    }

    const uint8_t* nonce = data + 5;
    const uint8_t* ciphertext = data + crypto::MKPK_HEADER_SIZE;
    const size_t ct_len = size - crypto::MKPK_HEADER_SIZE;

    // Get decryption key (runtime deobfuscation)
    auto key = crypto::decryption_key();

    // Allocate output (ciphertext minus tag)
    std::vector<uint8_t> compressed(ct_len - crypto::MKPK_TAG_SIZE);

    bool ok = aes256gcm_decrypt(
        key.data(), nonce,
        crypto::MKPK_MAGIC, 4,   // AAD = "MKPK"
        ciphertext, ct_len,
        compressed.data());

    // Zero key from memory immediately
    volatile uint8_t* p = key.data();
    for (size_t i = 0; i < key.size(); ++i) p[i] = 0;

    if (!ok) {
        if (err) *err = MkpkError("Decryption failed: authentication tag mismatch (wrong key or tampered data)");
        return {};
    }

    return compressed;
}

// ============================================================================
// Zstandard Decompression
// ============================================================================

/// Decompress zstd data. Returns raw tar bytes.
inline std::vector<uint8_t> zstd_decompress(
    const uint8_t* data, size_t size,
    MkpkError* err = nullptr)
{
    // Get decompressed size from frame header
    const unsigned long long frame_size = ZSTD_getFrameContentSize(data, size);

    if (frame_size == ZSTD_CONTENTSIZE_ERROR) {
        if (err) *err = MkpkError("Not valid zstd compressed data");
        return {};
    }

    if (frame_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        // Streaming decompression for unknown size
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        if (!dctx) {
            if (err) *err = MkpkError("Failed to create zstd decompression context");
            return {};
        }

        std::vector<uint8_t> result;
        result.reserve(size * 4); // estimate 4x expansion

        ZSTD_inBuffer input = { data, size, 0 };
        std::vector<uint8_t> chunk(ZSTD_DStreamOutSize());

        while (input.pos < input.size) {
            ZSTD_outBuffer output = { chunk.data(), chunk.size(), 0 };
            size_t ret = ZSTD_decompressStream(dctx, &output, &input);
            if (ZSTD_isError(ret)) {
                ZSTD_freeDCtx(dctx);
                if (err) *err = MkpkError(std::string("zstd decompression error: ") + ZSTD_getErrorName(ret));
                return {};
            }
            result.insert(result.end(), chunk.data(), chunk.data() + output.pos);
        }

        ZSTD_freeDCtx(dctx);
        return result;
    }

    // Known size — single-shot decompression
    if (frame_size > 10ULL * 1024 * 1024 * 1024) { // 10 GB safety limit
        if (err) *err = MkpkError("Decompressed size exceeds 10 GB safety limit");
        return {};
    }

    std::vector<uint8_t> result(static_cast<size_t>(frame_size));
    size_t actual = ZSTD_decompress(result.data(), result.size(), data, size);

    if (ZSTD_isError(actual)) {
        if (err) *err = MkpkError(std::string("zstd decompression error: ") + ZSTD_getErrorName(actual));
        return {};
    }

    result.resize(actual);
    return result;
}

// ============================================================================
// Minimal Tar Extractor
// ============================================================================

namespace tar {

// Parse octal string from tar header (handles both null and space termination)
inline size_t parse_octal(const char* str, size_t len)
{
    size_t result = 0;
    for (size_t i = 0; i < len && str[i] >= '0' && str[i] <= '7'; ++i)
        result = result * 8 + static_cast<size_t>(str[i] - '0');
    return result;
}

// Check if a 512-byte block is all zeros (end-of-archive marker)
inline bool is_zero_block(const uint8_t* block)
{
    for (int i = 0; i < 512; ++i)
        if (block[i] != 0) return false;
    return true;
}

/// Extract tar archive from memory to filesystem.
/// @param data     Raw tar data
/// @param size     Size of tar data
/// @param dest     Destination directory (created if needed)
/// @param on_file  Optional callback (filename, bytes_written) for progress
/// @return Number of files extracted, or -1 on error
inline int extract(
    const uint8_t* data, size_t size,
    const std::filesystem::path& dest,
    MkpkError* err = nullptr,
    std::function<void(const std::string&, size_t)> on_file = nullptr)
{
    namespace fs = std::filesystem;

    if (size < 512) {
        if (err) *err = MkpkError("Tar data too small");
        return -1;
    }

    // Helper: convert tar header filename to fs::path on Windows.
    // Tar archives may use UTF-8, Windows-1254 (Turkish), or another ANSI codepage.
    // Try UTF-8 first, then Turkish codepage explicitly (Makine is a Turkish app),
    // then system ANSI as last resort.
    auto tarpath = [](const std::string& s) -> fs::path {
#ifdef _WIN32
        if (s.empty()) return {};
        const int len = static_cast<int>(s.size());

        // Try 1: Strict UTF-8 (MB_ERR_INVALID_CHARS rejects non-UTF-8 bytes)
        int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       s.data(), len, nullptr, 0);
        if (wlen > 0) {
            std::wstring wide(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                s.data(), len, wide.data(), wlen);
            return fs::path(wide);
        }

        // Try 2: Turkish codepage 1254 — explicit because CP_ACP may be UTF-8
        // on modern Windows 11, which would fail for Windows-1254 encoded bytes.
        // Covers: ü (0xFC), ç (0xE7), ş (0xFE), ğ (0xF0), ı (0xFD), ö (0xF6)
        constexpr UINT CP_TURKISH = 1254;
        wlen = MultiByteToWideChar(CP_TURKISH, 0, s.data(), len, nullptr, 0);
        if (wlen > 0) {
            std::wstring wide(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_TURKISH, 0, s.data(), len, wide.data(), wlen);
            return fs::path(wide);
        }

        // Try 3: System ANSI codepage (CP_ACP — fallback for non-Turkish archives)
        wlen = MultiByteToWideChar(CP_ACP, 0, s.data(), len, nullptr, 0);
        if (wlen > 0) {
            std::wstring wide(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_ACP, 0, s.data(), len, wide.data(), wlen);
            return fs::path(wide);
        }

        // Fallback: use as-is (will likely fail but preserves original for error msg)
        return fs::path(s);
#else
        return fs::path(s);
#endif
    };

    fs::create_directories(dest);

    int file_count = 0;
    size_t pos = 0;

    while (pos + 512 <= size) {
        const uint8_t* header = data + pos;

        // Two consecutive zero blocks = end of archive
        if (is_zero_block(header)) {
            if (pos + 1024 <= size && is_zero_block(header + 512))
                break;
            pos += 512;
            continue;
        }

        // Parse header fields
        // name: bytes 0-99, size: bytes 124-135, typeflag: byte 156
        // prefix: bytes 345-499 (POSIX ustar)
        char name[256] = {};
        std::memcpy(name, header, 100);

        // Check for ustar prefix (long paths)
        char prefix[156] = {};
        if (std::memcmp(header + 257, "ustar", 5) == 0) {
            std::memcpy(prefix, header + 345, 155);
        }

        std::string fullname;
        if (prefix[0]) {
            fullname = std::string(prefix) + "/" + std::string(name);
        } else {
            fullname = std::string(name);
        }

        // Remove trailing slashes
        while (!fullname.empty() && fullname.back() == '/')
            fullname.pop_back();

        if (fullname.empty()) {
            pos += 512;
            continue;
        }

        size_t file_size = parse_octal(reinterpret_cast<const char*>(header + 124), 12);
        char typeflag = static_cast<char>(header[156]);

        pos += 512; // Move past header

        // Sanitize Windows-illegal characters in tar filenames.
        // Some packaging tools corrupt non-ASCII chars to '?' which is
        // illegal in Windows paths (wildcard). Replace with '_'.
#ifdef _WIN32
        for (char& c : fullname) {
            if (c == '?' || c == '*' || c == '"' || c == '<' || c == '>' || c == '|')
                c = '_';
        }
#endif

        // Security: prevent path traversal
        fs::path target = dest / tarpath(fullname);
        auto canonical_dest = fs::weakly_canonical(dest);
        auto canonical_target = fs::weakly_canonical(target);
        if (canonical_target.string().find(canonical_dest.string()) != 0) {
            if (err) *err = MkpkError("Path traversal attempt blocked: " + fullname);
            return -1;
        }

        if (typeflag == '5' || (typeflag == '\0' && fullname.back() == '/')) {
            // Directory
            fs::create_directories(target);
        } else if (typeflag == '0' || typeflag == '\0') {
            // Regular file
            if (pos + file_size > size) {
                if (err) *err = MkpkError("Tar truncated: file extends beyond archive");
                return -1;
            }

            fs::create_directories(target.parent_path());

            std::ofstream ofs(target, std::ios::binary);
            if (!ofs) {
                // On Windows, file may be locked by the running game — retry once after 200ms
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                ofs.open(target, std::ios::binary);
                if (!ofs) {
                    if (err) *err = MkpkError("Cannot write file (locked?): " + target.string());
                    return -1;
                }
            }

            ofs.write(reinterpret_cast<const char*>(data + pos), static_cast<std::streamsize>(file_size));
            if (!ofs.good()) {
                ofs.close();
                if (err) *err = MkpkError("Write failed (disk full?): " + target.string());
                return -1;
            }
            ofs.close();

            if (on_file)
                on_file(fullname, file_size);

            ++file_count;
        }
        // Skip other types (symlinks, etc.)

        // Advance past file data (512-byte aligned)
        size_t blocks = (file_size + 511) / 512;
        pos += blocks * 512;
    }

    return file_count;
}

} // namespace tar

// ============================================================================
// Full Pipeline: MKPK file → extracted directory
// ============================================================================

/// Process a complete .makine file: decrypt → decompress → extract.
/// Supports v1 (encrypted) and v2 (unencrypted) formats.
/// @param mkpk_data  Raw .makine file contents
/// @param mkpk_size  Size in bytes
/// @param dest_dir   Destination directory for extracted files
/// @param err        Error output
/// @param on_file    Progress callback per extracted file
/// @return Number of files extracted, or -1 on error
inline int process_mkpkg(
    const uint8_t* mkpk_data, size_t mkpk_size,
    const std::filesystem::path& dest_dir,
    MkpkError* err = nullptr,
    std::function<void(const std::string&, size_t)> on_file = nullptr)
{
    // Validate minimum size (magic + version)
    if (mkpk_size < 5) {
        if (err) *err = MkpkError("File too small for MKPK format");
        return -1;
    }

    // Validate magic
    if (std::memcmp(mkpk_data, crypto::MKPK_MAGIC, 4) != 0) {
        if (err) *err = MkpkError("Invalid MKPK magic bytes");
        return -1;
    }

    const uint8_t version = mkpk_data[4];

    std::vector<uint8_t> compressed;

    if (version == 1) {
        // v1: encrypted — decrypt first
        compressed = decrypt_mkpk(mkpk_data, mkpk_size, err);
        if (compressed.empty())
            return -1;
    } else if (version == 2) {
        // v2: unencrypted — payload starts right after header (5 bytes)
        const uint8_t* payload = mkpk_data + 5;
        const size_t payload_size = mkpk_size - 5;
        compressed.assign(payload, payload + payload_size);
    } else {
        if (err) *err = MkpkError(fmt::format("Unsupported MKPK version: {}", version));
        return -1;
    }

    // Step 2: Decompress
    auto tar_data = zstd_decompress(compressed.data(), compressed.size(), err);
    if (tar_data.empty())
        return -1;

    // Free compressed data to reduce peak memory
    compressed.clear();
    compressed.shrink_to_fit();

    // Step 3: Extract
    return tar::extract(tar_data.data(), tar_data.size(), dest_dir, err, std::move(on_file));
}

/// Process a v2 (unencrypted) .makine file. Convenience wrapper for plugins.
/// @return Number of files extracted, or -1 on error
inline int process_plugin_package(
    const uint8_t* data, size_t size,
    const std::filesystem::path& dest_dir,
    MkpkError* err = nullptr,
    std::function<void(const std::string&, size_t)> on_file = nullptr)
{
    return process_mkpkg(data, size, dest_dir, err, std::move(on_file));
}

} // namespace makine::mkpk

#endif // !MAKINE_UI_ONLY
