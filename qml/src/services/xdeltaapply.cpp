// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// xdelta3.h pins WINVER/_WIN32_WINNT to Windows 2000 unless WINVER is already
// set; keep the toolchain's own level instead.
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "xdelta3.h"

#include "xdeltaapply.h"

#include <cstring>

namespace makine::xdelta {

namespace {

// VCD_ADLER32 in xdelta3.c: the window carries a checksum of its target bytes.
constexpr usize_t kWindowHasChecksum = 1u << 2;

// Delta bytes handed to the decoder at a time.
constexpr std::size_t kInputChunk = std::size_t(1) << 20;

struct Stream {
    xd3_stream s;

    Stream() { std::memset(&s, 0, sizeof s); }
    ~Stream() { xd3_free_stream(&s); }
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    bool open()
    {
        xd3_config config;
        xd3_init_config(&config, 0);
        if (xd3_config_stream(&s, &config) != 0) return false;
        s.flags |= XD3_FLUSH;   // all of the delta is at hand, as xd3_decode_memory does
        return true;
    }

    // Hand the decoder the next chunk; false once the delta is used up.
    bool feed(const std::uint8_t* delta, std::size_t size, std::size_t& pos)
    {
        if (pos == size) return false;
        const std::size_t n = size - pos < kInputChunk ? size - pos : kInputChunk;
        xd3_avail_input(&s, delta + pos, usize_t(n));
        pos += n;
        return true;
    }
};

// Decoder errors that mean "this delta was made against another file", as
// opposed to a delta that cannot be read at all.
bool sourceMismatch(const char* msg)
{
    return msg && (std::strcmp(msg, "target window checksum mismatch") == 0
                   || std::strcmp(msg, "source file too short") == 0);
}

// Adler-32 as zlib computes it (xdelta3's own copy is file-static).
std::uint32_t adler32(const std::uint8_t* p, std::size_t n)
{
    std::uint32_t a = 1, b = 0;
    while (n > 0) {
        std::size_t run = n < 5552 ? n : 5552;   // longest run before b can overflow
        n -= run;
        while (run--) { a += *p++; b += a; }
        a %= 65521;
        b %= 65521;
    }
    return (b << 16) | a;
}

Result decode(const std::uint8_t* delta, std::size_t deltaSize,
              const std::uint8_t* source, std::size_t sourceSize, const Sink& sink)
{
    // The whole source as one block, as xd3_decode_memory does: the decoder
    // never has to ask for another.
    xd3_source src;
    std::memset(&src, 0, sizeof src);
    src.blksize = usize_t(sourceSize);
    src.onblk = usize_t(sourceSize);
    src.curblk = source;
    src.curblkno = 0;
    src.max_winsize = xoff_t(sourceSize);

    Stream st;
    if (!st.open() || xd3_set_source_and_size(&st.s, &src, xoff_t(sourceSize)) != 0)
        return Result::Corrupt;

    std::size_t pos = 0;
    int windows = 0;
    st.feed(delta, deltaSize, pos);
    for (;;) {
        switch (xd3_decode_input(&st.s)) {
        case XD3_INPUT:
            if (st.feed(delta, deltaSize, pos)) continue;
            // A delta cut off mid-window fails here, not silently.
            return (windows > 0 && xd3_close_stream(&st.s) == 0) ? Result::Success
                                                                  : Result::Corrupt;
        case XD3_OUTPUT:
            if (!sink(st.s.next_out, std::size_t(st.s.avail_out))) return Result::WriteError;
            xd3_consume_output(&st.s);
            continue;
        case XD3_GOTHEADER:
        case XD3_WINSTART:
            if ((st.s.dec_win_ind & kWindowHasChecksum) == 0) return Result::Corrupt;
            ++windows;
            continue;
        case XD3_WINFINISH:
            continue;
        default:   // XD3_GETSRCBLK cannot happen with a one-block source
            return sourceMismatch(st.s.msg) ? Result::NoMatch : Result::Corrupt;
        }
    }
}

// Is `file` already the delta's target? Each window's checksum is compared
// with the bytes at its place in the file and the windows must cover the file
// exactly. Windows are skipped, not decoded, so no source is needed.
bool isTarget(const std::uint8_t* delta, std::size_t deltaSize,
              const std::uint8_t* file, std::size_t fileSize)
{
    Stream st;
    if (!st.open()) return false;

    std::size_t pos = 0;
    std::uint64_t covered = 0;
    st.feed(delta, deltaSize, pos);
    for (;;) {
        switch (xd3_decode_input(&st.s)) {
        case XD3_INPUT:
            if (st.feed(delta, deltaSize, pos)) continue;
            return covered > 0 && covered == fileSize && xd3_close_stream(&st.s) == 0;
        case XD3_GOTHEADER:
        case XD3_WINSTART: {
            const std::uint64_t start = st.s.dec_winstart;
            const std::uint64_t length = st.s.dec_tgtlen;
            if ((st.s.dec_win_ind & kWindowHasChecksum) == 0 || start != covered
                || length > fileSize - covered
                || adler32(file + start, std::size_t(length)) != st.s.dec_adler32)
                return false;
            covered += length;
            xd3_set_flags(&st.s, st.s.flags | XD3_SKIP_WINDOW);
            continue;
        }
        case XD3_OUTPUT:
            xd3_consume_output(&st.s);
            continue;
        case XD3_WINFINISH:
            continue;
        default:
            return false;
        }
    }
}

} // namespace

const char* resultName(Result r)
{
    switch (r) {
    case Result::Success:    return "success";
    case Result::UpToDate:   return "up-to-date";
    case Result::NoMatch:    return "no-match";
    case Result::Corrupt:    return "corrupt";
    case Result::WriteError: return "write-error";
    }
    return "?";
}

Result apply(const std::uint8_t* delta, std::size_t deltaSize,
             const std::uint8_t* source, std::size_t sourceSize, const Sink& sink)
{
    if (!delta || deltaSize == 0) return Result::Corrupt;
    if (!source || sourceSize == 0) return Result::NoMatch;

    const Result r = decode(delta, deltaSize, source, sourceSize, sink);
    // The archive a delta does not fit may be the one it already produced.
    if (r == Result::NoMatch && isTarget(delta, deltaSize, source, sourceSize))
        return Result::UpToDate;
    return r;
}

} // namespace makine::xdelta
