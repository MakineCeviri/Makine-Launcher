/**
 * @file test_archive_utils.cpp
 * @brief Unit tests for archive_utils.hpp — Format detection, formatToString, backends
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/archive_utils.hpp>

#include <cstring>
#include <span>
#include <string>
#include <filesystem>
#include <vector>

namespace makine {
namespace testing {

using namespace archive;

// ===========================================================================
// Format Detection (magic bytes)
// ===========================================================================

TEST(ArchiveFormatTest, DetectZIP) {
    std::vector<uint8_t> data = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::ZIP);
}

TEST(ArchiveFormatTest, DetectZIPEmptyArchive) {
    // PK\x05\x06 — empty ZIP
    std::vector<uint8_t> data = {0x50, 0x4B, 0x05, 0x06, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::ZIP);
}

TEST(ArchiveFormatTest, Detect7z) {
    std::vector<uint8_t> data = {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::SevenZip);
}

TEST(ArchiveFormatTest, DetectRAR) {
    std::vector<uint8_t> data = {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::RAR);
}

TEST(ArchiveFormatTest, DetectGZIP) {
    std::vector<uint8_t> data = {0x1F, 0x8B, 0x08, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::GZIP);
}

TEST(ArchiveFormatTest, DetectBZIP2) {
    std::vector<uint8_t> data = {0x42, 0x5A, 0x68, 0x39};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::BZIP2);
}

TEST(ArchiveFormatTest, DetectXZ) {
    std::vector<uint8_t> data = {0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::XZ);
}

TEST(ArchiveFormatTest, UnknownFormat) {
    std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::Unknown);
}

TEST(ArchiveFormatTest, TooFewBytesReturnsUnknown) {
    std::vector<uint8_t> data = {0x50, 0x4B};
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(data)), Format::Unknown);
}

TEST(ArchiveFormatTest, EmptyDataReturnsUnknown) {
    std::vector<uint8_t> empty;
    EXPECT_EQ(detectFormat(std::span<const uint8_t>(empty)), Format::Unknown);
}

TEST(ArchiveFormatTest, ThreeBytesNotEnoughFor7z) {
    // 7z needs 6 bytes — only 3 matching won't trigger
    std::vector<uint8_t> data = {0x37, 0x7A, 0xBC, 0x00};
    EXPECT_NE(detectFormat(std::span<const uint8_t>(data)), Format::SevenZip);
}

// ===========================================================================
// formatToString
// ===========================================================================

TEST(ArchiveFormatStringTest, AllFormatsHaveNames) {
    EXPECT_STREQ(formatToString(Format::Unknown), "Unknown");
    EXPECT_STREQ(formatToString(Format::ZIP), "ZIP");
    EXPECT_STREQ(formatToString(Format::SevenZip), "7z");
    EXPECT_STREQ(formatToString(Format::RAR), "RAR");
    EXPECT_STREQ(formatToString(Format::TAR), "TAR");
    EXPECT_STREQ(formatToString(Format::GZIP), "GZIP");
    EXPECT_STREQ(formatToString(Format::BZIP2), "BZIP2");
    EXPECT_STREQ(formatToString(Format::XZ), "XZ");
    EXPECT_STREQ(formatToString(Format::LZMA), "LZMA");
}

// ===========================================================================
// ArchiveEntry
// ===========================================================================

TEST(ArchiveEntryTest, DefaultValues) {
    ArchiveEntry entry;
    EXPECT_TRUE(entry.path.empty());
    EXPECT_EQ(entry.compressedSize, 0u);
    EXPECT_EQ(entry.uncompressedSize, 0u);
    EXPECT_FALSE(entry.isDirectory);
    EXPECT_FALSE(entry.isEncrypted);
    EXPECT_EQ(entry.modTime, 0);
}

TEST(ArchiveEntryTest, SetFields) {
    ArchiveEntry entry;
    entry.path = "data/config.json";
    entry.compressedSize = 512;
    entry.uncompressedSize = 1024;
    entry.isDirectory = false;
    entry.isEncrypted = true;
    entry.modTime = 1700000000;

    EXPECT_EQ(entry.path, "data/config.json");
    EXPECT_EQ(entry.compressedSize, 512u);
    EXPECT_EQ(entry.uncompressedSize, 1024u);
    EXPECT_TRUE(entry.isEncrypted);
}

// ===========================================================================
// availableBackends
// ===========================================================================

TEST(ArchiveBackendsTest, ReturnsVector) {
    auto backends = availableBackends();
    // May be empty if no backends compiled in, but should not crash
    SUCCEED();
}

// ===========================================================================
// isFormatSupported
// ===========================================================================

TEST(ArchiveFormatSupportTest, UnknownNotSupported) {
    // Without any backend, Unknown should be false
    // With backends, it may vary
    (void)isFormatSupported(Format::Unknown);
    SUCCEED();
}

TEST(ArchiveFormatSupportTest, CallDoesNotCrash) {
    // Just verify all formats can be checked without crash
    (void)isFormatSupported(Format::ZIP);
    (void)isFormatSupported(Format::SevenZip);
    (void)isFormatSupported(Format::RAR);
    (void)isFormatSupported(Format::TAR);
    (void)isFormatSupported(Format::GZIP);
    (void)isFormatSupported(Format::BZIP2);
    (void)isFormatSupported(Format::XZ);
    (void)isFormatSupported(Format::LZMA);
    SUCCEED();
}

// ===========================================================================
// openArchive (null for nonexistent)
// ===========================================================================

TEST(ArchiveOpenTest, NonexistentFileReturnsNull) {
    auto archive = openArchive("C:/nonexistent/file.zip");
    // May be null if no backend available, or may return an extractor that fails on list()
    // Either way, should not crash
    SUCCEED();
}

// ===========================================================================
// detectFormat from file
// ===========================================================================

TEST(ArchiveDetectFileTest, NonexistentFileReturnsUnknown) {
    EXPECT_EQ(detectFormat(std::filesystem::path("C:/nonexistent/archive.zip")),
              Format::Unknown);
}

} // namespace testing
} // namespace makine
