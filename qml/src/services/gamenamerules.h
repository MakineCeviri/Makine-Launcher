// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// Does a locally detected game name belong to the catalog entry claiming it?
//
// This guard exists because crack/repack ACFs reuse a real appid for a
// different game — Little Nightmares' 424840 shipped as The Genesis Order —
// and offering that game's translation would patch the wrong files. But a
// guard that also rejects correct matches hides games the launcher supports,
// and the user is simply told their game is not detected.
//
// Lives outside gameservice.cpp so both directions can be exercised directly:
// the rejections it must keep making are as much of the contract as the
// matches it must stop breaking.

#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace makine::namerules {

// How to treat characters that are neither letters nor digits.
//
// No single reading is right, and the field shows all three (breadcrumb counts
// sampled 2026-09-19):
//   Drop     "S.T.A.L.K.E.R." must lose its dots, not gain six word breaks
//   AsSpace  Steam's "Watch_Dogs" against the catalog's "Watch Dogs" — dropping
//            the underscore gives "watchdogs", which shares no token with
//            "watch dogs", and a correct match was refused 6 times
//   Compact  a folder named "ItTakesTwo" against "It Takes Two": nothing to
//            drop or space, the two just disagree about where words end
enum class Separators { Drop, AsSpace, Compact };

inline QString normalizeGameName(const QString& s, Separators mode)
{
    QString out;
    out.reserve(s.size());
    for (const QChar ch : s) {
        if (ch.isLetterOrNumber()) {
            out.append(ch.toLower());
        } else if (mode == Separators::Compact) {
            continue;                              // no word breaks at all
        } else if (ch.isSpace() || mode == Separators::AsSpace) {
            if (!out.isEmpty() && !out.endsWith(QLatin1Char(' ')))
                out.append(QLatin1Char(' '));
        }
    }
    return out.trimmed();
}

// Do two already-normalized names describe the same game?
inline bool normalizedNamesMatch(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty()) return true;       // can't decide → trust catalog
    if (a.contains(b) || b.contains(a)) return true;   // substring either direction

    const QStringList tokensA = a.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList tokensB = b.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokensA.isEmpty() || tokensB.isEmpty()) return true;

    const QSet<QString> setB(tokensB.begin(), tokensB.end());
    int common = 0;
    for (const QString& t : tokensA)
        if (setB.contains(t)) ++common;

    const int minTokens = std::min(tokensA.size(), tokensB.size());
    return common >= std::max(1, minTokens / 2);
}

// Accept under ANY reading. A genuinely different game fails all three — every
// rejection the field produced for a real repack still fails — so widening here
// costs nothing the guard was protecting.
inline bool gameNamesLikelyMatch(const QString& localName, const QString& catalogName)
{
    for (const Separators mode : {Separators::Drop, Separators::AsSpace, Separators::Compact}) {
        if (normalizedNamesMatch(normalizeGameName(localName, mode),
                                 normalizeGameName(catalogName, mode)))
            return true;
    }
    return false;
}

} // namespace makine::namerules
