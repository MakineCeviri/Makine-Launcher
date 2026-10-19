// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Rebuild a file from an xdelta3 (VCDIFF) delta and the file it was made
// against — the legacy installer's `c1 -d -s <old> <delta> <new>` (c1.exe is
// xdelta3 3.0.4), done in-process with the vendored decoder in
// third_party/xdelta3.
//
// Assassin's Creed Odyssey 1.5.6 is translated this way: five deltas, one per
// .forge archive. Running c1.exe meant one more unsigned binary for antivirus
// to quarantine and a failure mode ("target window checksum mismatch", exit 1)
// that surfaced as nothing more than an exit code.
//
// A result is Success only when every window of the delta carried an Adler-32
// checksum and the decoded bytes matched it. A delta without checksums is
// refused: decoding it against the wrong archive would produce garbage that
// nothing could tell apart from a translation.
//
// Kept free of Qt and of xdelta3.h (which pulls in <windows.h> with its own
// WINVER): the executor passes plain memory and a sink.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace makine::xdelta {

enum class Result {
    Success,     // target produced and verified window by window
    UpToDate,    // the source already IS the target: a re-install or an update
    NoMatch,     // the source is not the file the delta was made against
    Corrupt,     // not a usable VCDIFF delta, or one without checksums
    WriteError,  // the sink refused bytes
};

const char* resultName(Result r);

// Receives the target in order, chunk by chunk. Return false to abort.
using Sink = std::function<bool(const std::uint8_t* data, std::size_t size)>;

Result apply(const std::uint8_t* delta, std::size_t deltaSize,
             const std::uint8_t* source, std::size_t sourceSize,
             const Sink& sink);

} // namespace makine::xdelta
