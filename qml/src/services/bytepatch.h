// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Find-and-replace inside a game binary, for the "patchBytes" install step.
//
// Elden Ring (1245620) shows why this exists. Its translation is loose files —
// Game/engus/*.msgbnd.dcx and Game/fn/* — that the game only reads once two
// UTF-16 mount strings in eldenring.exe point at the game folder instead of
// the archives:
//
//     data0:/font  ->  dvdroot:/fn
//     data0:/msg   ->  dvdroot:/
//
// The legacy installer (MC_Yama_Yukleyici_V8, decompiled 2026-10-08) did this
// with ERING_TR.exe, a compiled AutoIt script run from Game/: it writes the
// edited copy as start_protected_game.exe, so Steam's Play button starts the
// translated game without Easy Anti-Cheat. The launcher never ran that tool
// where it works, and an AutoIt binary is what antivirus quarantines first, so
// the launcher does the edit itself.
//
// The rules are deliberately strict, because a wrong guess corrupts a game
// executable:
//   - find and replace must be the same length (offsets elsewhere in the file
//     must not move);
//   - each find must occur exactly once — none means another game build,
//     several means we cannot tell which one the game uses;
//   - nothing is changed unless every pair is located first.
// A pair whose find is absent but whose replacement occurs exactly once is
// already applied (a re-install), which is not an error.
//
// Byte strings are written with an explicit encoding so recipes stay readable
// and unambiguous: "utf16:" (UTF-16LE text), "ascii:" (7-bit text) or "hex:".
// JSON's \u0000 spells a terminating NUL, which keeps "data0:/msg" from also
// matching inside a longer string.

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringView>

namespace makine::bytepatch {

// Decode one prefixed byte string. Empty = malformed or unknown encoding.
inline QByteArray decode(const QString& spec)
{
    if (spec.startsWith(QLatin1String("utf16:"))) {
        QByteArray out;
        for (const QChar c : QStringView(spec).mid(6)) {
            out.append(static_cast<char>(c.unicode() & 0xFF));
            out.append(static_cast<char>(c.unicode() >> 8));
        }
        return out;
    }
    if (spec.startsWith(QLatin1String("ascii:"))) {
        for (const QChar c : QStringView(spec).mid(6))
            if (c.unicode() > 0x7F) return {};
        return spec.mid(6).toLatin1();
    }
    if (spec.startsWith(QLatin1String("hex:"))) {
        const QByteArray hex = spec.mid(4).toLatin1();
        if (hex.size() % 2 != 0) return {};
        for (const char ch : hex) {
            const bool digit = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')
                            || (ch >= 'A' && ch <= 'F');
            if (!digit) return {};
        }
        return QByteArray::fromHex(hex);
    }
    return {};
}

struct Pair {
    QByteArray find;
    QByteArray replace;
};

// Decode a recipe's find/replace strings. False if any pair is unusable.
inline bool decodePairs(const QList<QPair<QString, QString>>& specs, QList<Pair>* out)
{
    if (specs.isEmpty()) return false;
    QList<Pair> pairs;
    for (const auto& s : specs) {
        Pair p{decode(s.first), decode(s.second)};
        if (p.find.isEmpty() || p.find.size() != p.replace.size() || p.find == p.replace)
            return false;
        pairs.append(p);
    }
    if (out) *out = pairs;
    return true;
}

enum class Result { Patched, AlreadyPatched, NoMatch, Ambiguous, Invalid };

inline const char* resultName(Result r)
{
    switch (r) {
    case Result::Patched:        return "patched";
    case Result::AlreadyPatched: return "already-patched";
    case Result::NoMatch:        return "no-match";
    case Result::Ambiguous:      return "ambiguous";
    case Result::Invalid:        return "invalid";
    }
    return "?";
}

// Occurrences of needle (overlapping ones too); *first gets the first offset.
inline int countOf(const QByteArray& hay, const QByteArray& needle, qsizetype* first = nullptr)
{
    int n = 0;
    for (qsizetype at = hay.indexOf(needle); at >= 0; at = hay.indexOf(needle, at + 1)) {
        if (n == 0 && first) *first = at;
        if (++n > 1) break;     // two already means ambiguous
    }
    return n;
}

// Apply every pair to data in place, or change nothing.
inline Result apply(QByteArray& data, const QList<Pair>& pairs)
{
    if (pairs.isEmpty()) return Result::Invalid;
    for (const Pair& p : pairs)
        if (p.find.isEmpty() || p.find.size() != p.replace.size() || p.find == p.replace)
            return Result::Invalid;

    QList<qsizetype> at;
    bool anyToApply = false;
    for (const Pair& p : pairs) {
        qsizetype pos = -1;
        const int found = countOf(data, p.find, &pos);
        if (found > 1) return Result::Ambiguous;
        if (found == 1) { at.append(pos); anyToApply = true; continue; }
        const int done = countOf(data, p.replace);
        if (done == 1) { at.append(-1); continue; }
        return done > 1 ? Result::Ambiguous : Result::NoMatch;
    }
    if (!anyToApply) return Result::AlreadyPatched;

    for (qsizetype i = 0; i < pairs.size(); ++i)
        if (at[i] >= 0) data.replace(at[i], pairs[i].find.size(), pairs[i].replace);
    return Result::Patched;
}

} // namespace makine::bytepatch
