// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// A game told apart by its files, the way OyunBul does it.
//
// An exe name says little: Little Nightmares ships "game.exe", HITMAN
// "launcher.exe", and the folder a store or the player put the game in differs
// on every PC. OyunBul — the finder translators ship next to their patch
// programs — fingerprints the copy a patch was made on instead: the main exe,
// the other exes beside it (another store's edition names its exe
// differently) and ten files spread over the game's tree, each as a relative
// path, a size and a hash of its first and last 64 KB. A folder is the game
// when the exe is there and most samples sit at their paths; it is the
// version the patch was made for when the exe and most samples are identical.
//
// The same rules here, byte for byte: a game.bin OyunBul wrote judges a folder
// here exactly as OyunBul judges it (tests/ui/test_gameprint.cpp checks one it
// wrote). The fingerprint reaches the launcher two ways: as the catalogue's
// fingerprint {exe, altExes, keyFiles: [{rel, size, hash}]}, and as the
// game.bin a translator put into the package. Format: docs/patch-programs.md.

#include "rawfile.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtEndian>

#include <cmath>
#include <filesystem>
#include <optional>
#include <system_error>

namespace makine::gameprint {

inline constexpr qint64 kChunk = 64 * 1024;
inline constexpr int kHashBytes = 16;

// One fingerprinted file. `rel` is relative to the game's root, "/"-separated.
struct Entry {
    QString rel;
    qint64 size = 0;
    QByteArray hash;   // kHashBytes, partialHash()
};

struct Print {
    QString name;        // the game as the patch maker named it
    QString rootName;    // the root folder's name on the patch maker's PC
    QString exeVersion;  // the main exe's file version there, when it had one
    Entry exe;
    QList<Entry> altExes;   // other exes beside the main one
    QList<Entry> samples;
    double matchRatio = 0.6;   // samples at their paths for "this is the game"
    double exactRatio = 0.8;   // samples identical for "the same version"

    bool isValid() const { return !exe.rel.isEmpty() && exe.hash.size() == kHashBytes; }
};

// SHA-256 of the length (8 bytes little-endian), the first 64 KB and the last
// 64 KB, cut to 16 bytes: reads at most 128 KB even from a multi-GB archive.
// Empty when the file cannot be read.
inline QByteArray partialHash(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const qint64 len = f.size();
    QCryptographicHash sha(QCryptographicHash::Sha256);
    uchar lenLe[8];
    qToLittleEndian<qint64>(len, lenLe);
    sha.addData(QByteArrayView(reinterpret_cast<const char*>(lenLe), 8));
    const qint64 head = qMin(kChunk, len);
    QByteArray buf = f.read(head);
    if (buf.size() != head) return {};
    sha.addData(buf);
    if (len > kChunk) {
        const qint64 tail = qMax(kChunk, len - kChunk);
        if (!f.seek(tail)) return {};
        buf = f.read(len - tail);
        if (buf.size() != len - tail) return {};
        sha.addData(buf);
    }
    return sha.result().left(kHashBytes);
}

// Is `path` a file, and how big? A data file named *.lnk is not a shortcut
// here either (rawfile.h).
inline bool statFile(const QString& path, qint64* size)
{
    if (rawfile::isLnk(path)) {
        std::error_code ec;
        const std::filesystem::path p(path.toStdWString());
        if (!std::filesystem::is_regular_file(p, ec)) return false;
        *size = static_cast<qint64>(std::filesystem::file_size(p, ec));
        return !ec;
    }
    const QFileInfo fi(path);
    if (!fi.isFile()) return false;
    *size = fi.size();
    return true;
}

// ── game.bin ─────────────────────────────────────────────────────────────────
//
// Little-endian; strings as .NET's BinaryWriter writes them (7-bit encoded
// byte count, UTF-8); the last 32 bytes are the SHA-256 of everything before:
//   "OYUNBUL1", Name, RootName, Created (int64 ticks), FileCount (int32),
//   TotalBytes (int64), ExeVersion, Exe, AltExes (int32 + entries),
//   Samples (int32 + entries); an entry is Rel, Size (int64), Hash (16 bytes).

inline constexpr char kMagic[] = "OYUNBUL1";

namespace detail {

class Reader {
public:
    Reader(const QByteArray& b, qsizetype from, qsizetype end) : m_b(b), m_at(from), m_end(end) {}
    bool ok() const { return m_ok; }

    QByteArray bytes(qsizetype n)
    {
        if (!m_ok || n < 0 || m_at + n > m_end) { m_ok = false; return {}; }
        const QByteArray out = m_b.mid(m_at, n);
        m_at += n;
        return out;
    }
    qint32 i32()
    {
        const QByteArray b = bytes(4);
        return m_ok ? qFromLittleEndian<qint32>(b.constData()) : 0;
    }
    qint64 i64()
    {
        const QByteArray b = bytes(8);
        return m_ok ? qFromLittleEndian<qint64>(b.constData()) : 0;
    }
    QString str()
    {
        qint64 len = 0;
        for (int shift = 0; ; shift += 7) {
            const QByteArray b = bytes(1);
            if (!m_ok || shift > 28) { m_ok = false; return {}; }
            const uchar c = uchar(b[0]);
            len |= qint64(c & 0x7F) << shift;
            if (!(c & 0x80)) break;
        }
        return QString::fromUtf8(bytes(len));
    }
    Entry entry()
    {
        Entry e;
        e.rel = str().replace(QLatin1Char('\\'), QLatin1Char('/'));
        e.size = i64();
        e.hash = bytes(kHashBytes);
        return e;
    }
    QList<Entry> entries()
    {
        const qint32 n = i32();
        QList<Entry> out;
        if (n < 0 || n > 1000) { m_ok = false; return out; }
        for (qint32 i = 0; i < n && m_ok; ++i) out.append(entry());
        return out;
    }

private:
    const QByteArray& m_b;
    qsizetype m_at;
    qsizetype m_end;
    bool m_ok = true;
};

} // namespace detail

inline bool looksLikeGameBin(const QByteArray& head)
{
    return head.startsWith(QByteArray(kMagic));
}

// The print in a game.bin, or nullopt when it is none or damaged.
inline std::optional<Print> fromGameBin(const QByteArray& bytes)
{
    const qsizetype magic = qsizetype(sizeof(kMagic) - 1);
    if (bytes.size() < magic + 32 || !looksLikeGameBin(bytes)) return std::nullopt;
    const qsizetype body = bytes.size() - 32;
    if (QCryptographicHash::hash(bytes.left(body), QCryptographicHash::Sha256) != bytes.mid(body))
        return std::nullopt;
    detail::Reader r(bytes, magic, body);
    Print p;
    p.name = r.str();
    p.rootName = r.str();
    r.i64();   // created
    r.i32();   // file count
    r.i64();   // total bytes
    p.exeVersion = r.str();
    p.exe = r.entry();
    p.altExes = r.entries();
    p.samples = r.entries();
    if (!r.ok() || !p.isValid()) return std::nullopt;
    return p;
}

inline std::optional<Print> fromGameBinFile(const QString& path)
{
    QFile f(path);
    if (f.size() > 1024 * 1024 || !f.open(QIODevice::ReadOnly)) return std::nullopt;
    return fromGameBin(f.readAll());
}

// Every game.bin at most `maxDepth` folders below `dir`: a package may keep it
// next to its program, or one per variant folder.
// Folder by folder, never below maxDepth: a package can hold thousands of files.
inline QStringList findGameBins(const QString& dir, int maxDepth = 3)
{
    QStringList out;
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) return out;
    QStringList level{QDir::cleanPath(dir)};
    for (int depth = 0; depth <= maxDepth && !level.isEmpty(); ++depth) {
        QStringList next;
        for (const QString& d : level) {
            const QDir here(d);
            for (const QString& f : here.entryList({QStringLiteral("game.bin")}, QDir::Files))
                out.append(here.filePath(f));
            if (depth < maxDepth)
                for (const QString& sub : here.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
                    next.append(here.filePath(sub));
        }
        level = next;
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

// ── Judging a folder ─────────────────────────────────────────────────────────

struct Verdict {
    QString root;
    QString exeRel;          // the exe found (or guessed) under root, "/"-separated
    bool exeFound = false;   // the main exe or one of the alternatives
    bool exeAlt = false;     // found, but an alternative rather than the main one
    bool exeGuessed = false; // none of them; the likeliest exe in the main one's folder
    bool exeSame = false;    // identical to the patch maker's
    int n = 0;               // samples in the print
    int pathHits = 0;        // at their paths
    int sizeHits = 0;        // ... with their size
    int hashHits = 0;        // ... identical
    bool match = false;      // this folder is the game
    bool exact = false;      // and the version the patch was made for
    bool rootLike = false;   // most samples, but no exe at all

    int score() const { return (exeSame ? n + 1 : 0) + pathHits + sizeHits + hashHits; }
    QString exeDir() const
    {
        const int cut = exeRel.lastIndexOf(QLatin1Char('/'));
        return cut < 0 ? root : root + QLatin1Char('/') + exeRel.left(cut);
    }
    QString exePath() const { return root + QLatin1Char('/') + exeRel; }
};

inline int need(int n, double ratio)
{
    return static_cast<int>(std::ceil(n * ratio - 1e-9));
}

// How unlike the game's own exe a name is: 2 for a helper (crash reporter,
// installer, anti-cheat service), 1 for a launcher or tool, 0 otherwise.
inline int exePenalty(const QString& name)
{
    static const QStringList kHelper = {
        QStringLiteral("unins"), QStringLiteral("crash"), QStringLiteral("report"),
        QStringLiteral("setup"), QStringLiteral("install"), QStringLiteral("redist"),
        QStringLiteral("easyanticheat"), QStringLiteral("beservice"), QStringLiteral("battleye"),
        QStringLiteral("updater"), QStringLiteral("helper"), QStringLiteral("diag"),
        QStringLiteral("cefprocess"), QStringLiteral("handler"), QStringLiteral("bugsplat"),
        QStringLiteral("touchup"), QStringLiteral("activation"), QStringLiteral("cleanup"),
    };
    static const QStringList kLauncher = {
        QStringLiteral("launcher"), QStringLiteral("config"), QStringLiteral("settings"),
        QStringLiteral("tool"), QStringLiteral("editor"), QStringLiteral("server"),
    };
    const auto any = [&name](const QStringList& words) {
        for (const QString& w : words)
            if (name.contains(w, Qt::CaseInsensitive)) return true;
        return false;
    };
    return any(kHelper) ? 2 : any(kLauncher) ? 1 : 0;
}

// The likeliest game exe in `relDir` under `root`, relative to root; empty
// when there is none.
inline QString guessExe(const QString& root, const QString& relDir)
{
    const QDir d(relDir.isEmpty() ? root : root + QLatin1Char('/') + relDir);
    QString best;
    int bestPenalty = 2;
    qint64 bestSize = -1;
    for (const QFileInfo& fi : d.entryInfoList({QStringLiteral("*.exe")}, QDir::Files)) {
        const int penalty = exePenalty(fi.fileName());
        if (penalty >= 2) continue;
        if (penalty < bestPenalty || (penalty == bestPenalty && fi.size() > bestSize)) {
            best = fi.fileName();
            bestPenalty = penalty;
            bestSize = fi.size();
        }
    }
    if (best.isEmpty()) return {};
    return relDir.isEmpty() ? best : relDir + QLatin1Char('/') + best;
}

inline QString dirOf(const QString& rel)
{
    const int cut = rel.lastIndexOf(QLatin1Char('/'));
    return cut < 0 ? QString() : rel.left(cut);
}

inline Verdict verify(const Print& p, const QString& root)
{
    Verdict v;
    v.root = QDir::cleanPath(root);
    v.n = static_cast<int>(p.samples.size());
    qint64 size = 0;
    QList<Entry> exes{p.exe};
    exes += p.altExes;
    for (int i = 0; i < exes.size(); ++i) {
        const Entry& e = exes[i];
        const QString path = v.root + QLatin1Char('/') + e.rel;
        if (!statFile(path, &size)) continue;
        v.exeFound = true;
        v.exeAlt = i > 0;
        v.exeRel = e.rel;
        v.exeSame = size == e.size && partialHash(path) == e.hash;
        break;
    }
    for (const Entry& e : p.samples) {
        const QString path = v.root + QLatin1Char('/') + e.rel;
        if (!statFile(path, &size)) continue;
        ++v.pathHits;
        if (size != e.size) continue;
        ++v.sizeHits;
        if (partialHash(path) == e.hash) ++v.hashHits;
    }
    if (v.n == 0) {
        v.match = v.exeSame;
    } else if (v.exeFound) {
        v.match = v.pathHits >= need(v.n, p.matchRatio);
    } else if (v.n >= 3 && v.pathHits >= need(v.n, 0.8)) {
        // None of the known exes, but the data is in place: another store's
        // edition. The likeliest exe beside where the main one would be.
        const QString guess = guessExe(v.root, dirOf(p.exe.rel));
        if (!guess.isEmpty()) {
            v.match = v.exeGuessed = true;
            v.exeRel = guess;
        } else {
            v.rootLike = true;
        }
    }
    v.exact = v.match && v.exeSame && v.hashHits >= need(v.n, p.exactRatio);
    if (v.exeRel.isEmpty()) v.exeRel = p.exe.rel;
    return v;
}

// The best of several prints for one folder (a package may carry one per
// variant): the version it was made for first, then any match, then the most
// alike.
inline Verdict best(const QList<Print>& prints, const QString& root)
{
    Verdict top;
    bool any = false;
    const auto rank = [](const Verdict& x) { return (x.exact ? 2 : 0) + (x.match ? 1 : 0); };
    for (const Print& p : prints) {
        const Verdict v = verify(p, root);
        if (!any || rank(v) > rank(top) || (rank(v) == rank(top) && v.score() > top.score())) {
            top = v;
            any = true;
        }
    }
    return top;
}

// The folder around `dir` the print's paths start from, when `dir` is not it:
// the scan stopped one level off (a "<Game>/<Game>" repack) or the player
// opened a folder inside the game. Looks at `dir`'s parents and for the exe
// at most `maxDepth` folders below it. Empty when nothing there matches.
inline QString findRoot(const QList<Print>& prints, const QString& dir, int maxDepth = 3)
{
    const QString base = QDir::cleanPath(dir);
    QStringList candidates;
    for (QString up = base; !up.isEmpty() && !QDir(up).isRoot(); up = QFileInfo(up).absolutePath()) {
        candidates.append(up);
        if (candidates.size() > 4) break;
    }
    QStringList wanted;
    for (const Print& p : prints)
        for (const Entry& e : QList<Entry>{p.exe} + p.altExes)
            wanted.append(e.rel);
    QStringList level{base};
    int visited = 0;
    for (int depth = 0; depth < maxDepth && !level.isEmpty(); ++depth) {
        QStringList next;
        for (const QString& d : level) {
            for (const QString& sub : QDir(d).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
                if (++visited > 2000) break;
                const QString child = d + QLatin1Char('/') + sub;
                next.append(child);
                for (const QString& rel : wanted)
                    if (QFileInfo::exists(child + QLatin1Char('/') + rel)
                        && !candidates.contains(child, Qt::CaseInsensitive))
                        candidates.append(child);
            }
        }
        level = next;
    }
    candidates.removeAll(base);
    Verdict top;
    for (const QString& c : candidates) {
        const Verdict v = best(prints, c);
        if (!v.match) continue;
        if (top.root.isEmpty() || (v.exact && !top.exact)
            || (v.exact == top.exact && v.score() > top.score()))
            top = v;
    }
    return top.root;
}

// game.path as OyunBul writes it: the exe's folder, the game's root, the exe.
// A patch's own .bat reads it with three "set /p". UTF-8, CRLF.
inline QByteArray pathFile(const QString& root, const QString& exeRel)
{
    const QString r = QDir::toNativeSeparators(QDir::cleanPath(root));
    const QString exe = QDir::toNativeSeparators(QDir::cleanPath(root + QLatin1Char('/') + exeRel));
    const QString exeDir = QDir::toNativeSeparators(QFileInfo(exe).absolutePath());
    return (exeDir + QStringLiteral("\r\n") + r + QStringLiteral("\r\n") + exe + QStringLiteral("\r\n")).toUtf8();
}

// One line for the log and the install console.
inline QString describe(const Verdict& v)
{
    QString s = v.exact ? QStringLiteral("TAM") : v.match ? QStringLiteral("SURUM FARKLI")
                                                          : QStringLiteral("ESLESMEDI");
    s += QStringLiteral(" (%1/%2 ornek yerinde, %3/%2 birebir, exe %4%5)")
             .arg(v.pathHits).arg(v.n).arg(v.hashHits)
             .arg(v.exeSame ? QStringLiteral("ayni")
                  : v.exeGuessed ? QStringLiteral("tahmini")
                  : v.exeFound ? QStringLiteral("farkli") : QStringLiteral("yok"),
                  v.exeRel.isEmpty() ? QString() : QStringLiteral(": ") + v.exeRel);
    return s;
}

} // namespace makine::gameprint
