// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Apply a VPatch binary delta.
//
// Two catalogue packages are built on this format and neither could install.
// Fahrenheit: Indigo Prophecy Remastered (312840) ships four .pat files plus
// VPatch.dll and nothing that can run them — the package is unusable by hand
// too, since VPatch.dll is an NSIS plugin with no command line. Assassin's
// Creed III Remastered (911400) ships a 6 KB patch.exe, which is a 32-bit
// binary with no manifest whose embedded "VPatch" string trips Windows'
// installer-detection heuristic: measured 2026-09-20, every attempt to start
// it returns ERROR_ELEVATION_REQUIRED, and neither renaming it nor placing an
// external .manifest beside it suppresses that. Running it would raise a UAC
// prompt per step in the middle of an automated install — the Elden Ring
// failure again. Both packages currently refuse at the pre-flight gate.
//
// So the launcher applies the format itself. The algorithm and the on-disk
// layout below are ported from the reference applier, NSIS
// Contrib/VPatch/Source/Plugin/apply_patch.c (Koen van de Sande,
// zlib-licensed), and verified against that project's own test vectors —
// oldfile.txt + patch.pat must produce newfile.txt byte for byte, which is
// what test_vpatchapply.cpp asserts.
//
// Layout, little-endian throughout:
//
//   "VPAT"                       magic; when it is not at offset 0, the last
//                                4 bytes of the file are its offset
//   u32  patches                 bit 31 set = MD5 mode; count = bits 0..23
//   per patch:
//     u32  blocks
//     MD5 mode: 16 bytes source digest, 16 bytes target digest
//     else:     u32 source CRC-32, u32 target CRC-32
//     u32  patchSize             bytes of block data, used to skip a patch
//                                whose source checksum does not match
//     blocks:
//       u8 type 1/2/3            copy from SOURCE: length u8/u16/u32, then a
//                                u32 absolute source offset
//       u8 type 5/6/7            literal bytes from the PATCH: length u8/u16/u32
//       u8 type 255              8-byte FILETIME for the target's mtime
//
// A file can hold several patches for different source versions; the applier
// picks the one whose source checksum matches, which is how 911400 patches
// four different .forge archives from one "tr" file. Nothing is written until
// a patch matches, and Success is returned only after the produced bytes hash
// to the target digest the patch declares — so a wrong result is reported as
// an error rather than left on disk.
//
// Qt-only by design (QCryptographicHash for MD5): it sits beside the other
// install rules, needs no core objects, and its test links against Qt6::Core.

#include <QByteArray>
#include <QCryptographicHash>
#include <QIODevice>
#include <QString>
#include <QtGlobal>

#include <array>

namespace makine::vpatch {

enum class Result {
    Success,     // target written and its checksum matches the patch
    UpToDate,    // source already IS the target of some patch in the file
    NoMatch,     // no patch in the file applies to this source
    Corrupt,     // the patch file is not readable as VPatch
    Error,       // I/O failed, or the result did not match its checksum
};

inline const char* resultName(Result r)
{
    switch (r) {
    case Result::Success:  return "success";
    case Result::UpToDate: return "uptodate";
    case Result::NoMatch:  return "nomatch";
    case Result::Corrupt:  return "corrupt";
    case Result::Error:    return "error";
    }
    return "error";
}

namespace detail {

inline bool readExact(QIODevice& d, char* out, qint64 n)
{
    qint64 got = 0;
    while (got < n) {
        const qint64 r = d.read(out + got, n - got);
        if (r <= 0) return false;
        got += r;
    }
    return true;
}

template <typename T>
inline bool readLE(QIODevice& d, T& value)
{
    unsigned char b[sizeof(T)];
    if (!readExact(d, reinterpret_cast<char*>(b), sizeof(T))) return false;
    quint64 x = 0;
    for (int i = static_cast<int>(sizeof(T)) - 1; i >= 0; --i)
        x = (x << 8) | b[i];
    value = static_cast<T>(x);
    return true;
}

// CRC-32 as the reference computes it: reflected polynomial 0xEDB88320,
// initial and final value 0xFFFFFFFF. Only the pre-MD5 patch format uses it;
// both catalogue packages are MD5 mode, but a CRC patch must not be silently
// misread as matching.
inline quint32 crc32Of(QIODevice& d)
{
    static const auto table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();

    if (!d.seek(0)) return 0;
    quint32 c = 0xFFFFFFFFu;
    QByteArray buf;
    while (!(buf = d.read(1 << 16)).isEmpty()) {
        for (const char ch : buf)
            c = table[(c ^ static_cast<unsigned char>(ch)) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

inline QByteArray md5Of(QIODevice& d)
{
    if (!d.seek(0)) return {};
    QCryptographicHash h(QCryptographicHash::Md5);
    if (!h.addData(&d)) return {};
    return h.result();
}

// Move `count` bytes from `in` to `out` through a fixed buffer, so a block
// claiming four gigabytes costs 64 KiB of memory rather than four gigabytes.
inline bool pipe(QIODevice& in, QIODevice& out, quint32 count)
{
    QByteArray buf;
    quint32 left = count;
    while (left > 0) {
        const qint64 want = qMin<qint64>(left, 1 << 16);
        buf = in.read(want);
        if (buf.size() != want) return false;
        if (out.write(buf) != buf.size()) return false;
        left -= static_cast<quint32>(want);
    }
    return true;
}

// Block length prefix: one, two or four bytes depending on the block type.
inline bool readBlockLength(QIODevice& patch, quint8 type, quint32& length)
{
    if (type == 1 || type == 5) {
        quint8 v; if (!readLE(patch, v)) return false; length = v; return true;
    }
    if (type == 2 || type == 6) {
        quint16 v; if (!readLE(patch, v)) return false; length = v; return true;
    }
    quint32 v; if (!readLE(patch, v)) return false; length = v; return true;
}

} // namespace detail

// `dest` must be open for reading AND writing: the produced bytes are hashed
// back before Success is returned.
inline Result apply(QIODevice& patch, QIODevice& source, QIODevice& dest)
{
    using namespace detail;

    constexpr quint32 kMagic = 0x54415056;   // "VPAT", little-endian

    if (!patch.seek(0)) return Result::Error;
    quint32 magic = 0;
    if (!readLE(patch, magic)) return Result::Corrupt;
    if (magic != kMagic) {
        // Older generators append the header offset as the last four bytes
        // instead of putting the header first.
        const qint64 size = patch.size();
        if (size < 8 || !patch.seek(size - 4)) return Result::Corrupt;
        quint32 offset = 0;
        if (!readLE(patch, offset)) return Result::Corrupt;
        if (offset > static_cast<quint32>(size) || !patch.seek(offset)) return Result::Corrupt;
        if (!readLE(patch, magic) || magic != kMagic) return Result::Corrupt;
    }

    quint32 header = 0;
    if (!readLE(patch, header)) return Result::Corrupt;
    const bool md5Mode = (header & 0x80000000u) != 0;
    quint32 remaining = header & 0x00FFFFFFu;

    QByteArray sourceMd5;
    quint32 sourceCrc = 0;
    if (md5Mode) {
        sourceMd5 = md5Of(source);
        if (sourceMd5.size() != 16) return Result::Error;
    } else {
        sourceCrc = crc32Of(source);
    }

    bool alreadyUpToDate = false;

    while (remaining-- > 0) {
        quint32 blocks = 0;
        if (!readLE(patch, blocks)) return Result::Corrupt;

        QByteArray wantSourceMd5, wantTargetMd5;
        quint32 wantSourceCrc = 0, wantTargetCrc = 0;
        if (md5Mode) {
            wantSourceMd5.resize(16);
            wantTargetMd5.resize(16);
            if (!readExact(patch, wantSourceMd5.data(), 16)) return Result::Corrupt;
            if (!readExact(patch, wantTargetMd5.data(), 16)) return Result::Corrupt;
        } else {
            if (!readLE(patch, wantSourceCrc)) return Result::Corrupt;
            if (!readLE(patch, wantTargetCrc)) return Result::Corrupt;
        }

        quint32 patchSize = 0;
        if (!readLE(patch, patchSize)) return Result::Corrupt;

        const bool isTarget = md5Mode ? (sourceMd5 == wantTargetMd5)
                                      : (sourceCrc == wantTargetCrc);
        const bool matches  = md5Mode ? (sourceMd5 == wantSourceMd5)
                                      : (sourceCrc == wantSourceCrc);
        if (isTarget) alreadyUpToDate = true;

        if (!matches) {
            // Skip this patch's block data and look at the next one.
            if (!patch.seek(patch.pos() + patchSize)) return Result::Corrupt;
            continue;
        }

        while (blocks-- > 0) {
            quint8 type = 0;
            if (!readLE(patch, type)) return Result::Corrupt;

            if (type == 1 || type == 2 || type == 3) {
                quint32 length = 0;
                if (!readBlockLength(patch, type, length)) return Result::Corrupt;
                if (length == 0) return Result::Corrupt;
                quint32 offset = 0;
                if (!readLE(patch, offset)) return Result::Corrupt;
                if (!source.seek(offset)) return Result::Error;
                if (!pipe(source, dest, length)) return Result::Error;
            } else if (type == 5 || type == 6 || type == 7) {
                quint32 length = 0;
                if (!readBlockLength(patch, type, length)) return Result::Corrupt;
                if (length == 0) return Result::Corrupt;
                if (!pipe(patch, dest, length)) return Result::Corrupt;
            } else if (type == 255) {
                // Target modification time. We do not set it: the file we
                // produce is a game asset, and its mtime carries no meaning
                // the launcher or the game reads.
                char skip[8];
                if (!readExact(patch, skip, 8)) return Result::Corrupt;
            } else {
                return Result::Corrupt;
            }
        }

        // The patch says what it should have produced. Believe the bytes.
        if (md5Mode) {
            if (md5Of(dest) != wantTargetMd5) return Result::Error;
        } else {
            if (crc32Of(dest) != wantTargetCrc) return Result::Error;
        }
        return Result::Success;
    }

    return alreadyUpToDate ? Result::UpToDate : Result::NoMatch;
}

} // namespace makine::vpatch
