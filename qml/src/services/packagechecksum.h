// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Does a downloaded package match what the catalogue says it should be?
//
// Until this existed there was no answer: translationdownloader.cpp never read
// a checksum, config.verifyChecksums (default true) was never read outside
// config.cpp, and compressedChecksum never reached the C++ side at all. The
// only integrity gate on a 600 MB download was the AES-GCM auth tag inside
// MKPK — which catches corruption, but only after the whole file has been
// decrypted, and reports it as "paket bozuk" with no way to tell a truncated
// download from a bad catalogue entry.
//
// The manifests themselves were the reason it stayed unwired: measured
// 2026-09-20, 197 of 201 hosted packages carried a size that no longer matched
// the object in R2, because the 2026-03-13 bulk repack (ZSTD_LEVEL 9 -> 19)
// was never followed by a manifest refresh. Verifying against those would have
// failed every install. They have since been rewritten from the real objects.
//
// Two spellings are in the wild and both must parse: package_pipeline.py
// writes "sha256:<hex>" (sha256_digest prefixes it), while the entries already
// in index.json carry a bare hex digest. A reader that accepted only one would
// silently verify nothing for half the catalogue.

#include <QLatin1String>
#include <QString>

namespace makine::pkgchecksum {

// The bare lowercase hex digest, or empty when the value is not one.
//
// Empty is the honest answer for "no checksum published" and for a value this
// build does not understand (a future "blake3:…"), and callers must treat it
// as "cannot verify" rather than "does not match" — refusing an install over
// an algorithm we simply do not read would break every one of those packages.
inline QString normalizeChecksum(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) return {};

    const int colon = s.indexOf(QLatin1Char(':'));
    if (colon >= 0) {
        if (s.left(colon).trimmed().compare(QLatin1String("sha256"),
                                            Qt::CaseInsensitive) != 0)
            return {};                       // some other algorithm — not ours
        s = s.mid(colon + 1).trimmed();
    }

    if (s.size() != 64) return {};
    for (const QChar c : s) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
              || (c >= QLatin1Char('A') && c <= QLatin1Char('F'))))
            return {};
    }
    return s.toLower();
}

// Is verification possible at all for this catalogue value?
inline bool isVerifiable(const QString& rawExpected)
{
    return !normalizeChecksum(rawExpected).isEmpty();
}

// Does the computed digest match? False only when both sides are real and
// differ — an unverifiable expectation is not a mismatch.
inline bool checksumMatches(const QString& rawExpected, const QString& actualHex)
{
    const QString want = normalizeChecksum(rawExpected);
    if (want.isEmpty()) return true;         // nothing published to check against
    return want == normalizeChecksum(actualHex);
}

} // namespace makine::pkgchecksum
