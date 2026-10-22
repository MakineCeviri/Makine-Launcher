// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// A patch's own program, given the game folder the launcher found.
//
// Translators write their tools on their own machine. Heavy Rain's scripts
// begin with `set OYUN=D:\SteamLibrary\steamapps\common\HEAVY RAIN`, AC
// Shadows' ACS.exe falls back to D:\Games\Assassin's Creed Shadows. Run as
// shipped on a player's PC, such a tool works on a folder that is not there —
// or, worse, on one that is and holds something else.
//
// The launcher knows the folder: it found the game and checked it against the
// patch. So a "run" step does not start the package's files where they lie. It
// starts a copy of them in a stage folder under %TEMP%, in which every line of
// a .bat/.cmd that sets the game folder now sets the one the launcher found.
// The folder reaches the program three ways, whichever it reads:
//   - the step's arguments (${gamePath}),
//   - the MAKINE_OYUN environment variable, which rewritten lines read — an
//     environment value is UTF-16, so a folder with Turkish letters survives
//     the code page a .bat runs under, a literal written into it would not,
//   - <game>/.makine, the launcher's note in the game folder itself.
//
// Free of the package manager so the rewrite is tested on real scripts — see
// tests/ui/test_runstage.cpp.

#include "gamenamerules.h"

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace makine::runstage {

// The environment variables a program step is started with.
inline constexpr char kGameVar[]    = "MAKINE_OYUN";     // the game folder
inline constexpr char kPackageVar[] = "MAKINE_PAKET";    // the staged package
inline constexpr char kGameIdVar[]  = "MAKINE_OYUN_ID";  // the catalogue id
// The launcher's note in the game folder.
inline constexpr char kNoteFile[]   = ".makine";

// Names translators give the variable that holds the game folder.
inline bool isGameVarName(QStringView name)
{
    QString n = name.toString().toLower();
    n.remove(QLatin1Char('_'));
    static const QStringList kNames = {
        QStringLiteral("oyun"),        QStringLiteral("oyunklasoru"), QStringLiteral("oyundizini"),
        QStringLiteral("oyunyolu"),    QStringLiteral("oyunkok"),     QStringLiteral("game"),
        QStringLiteral("gamedir"),     QStringLiteral("gamepath"),    QStringLiteral("gamefolder"),
        QStringLiteral("gameroot"),    QStringLiteral("installdir"),  QStringLiteral("steamgame"),
        QStringLiteral("gamelocation"),
    };
    return kNames.contains(n);
}

// Path segments, '\' or '/' separated, empty ones dropped.
inline QStringList segmentsOf(const QString& path)
{
    return QString(path).replace(QLatin1Char('/'), QLatin1Char('\\'))
        .split(QLatin1Char('\\'), Qt::SkipEmptyParts);
}

// Is `segment` the game's folder name ("HEAVY RAIN" for Heavy Rain)? Strict:
// a library folder or a word the title happens to contain is not.
inline bool segmentNamesGame(const QString& segment, const QString& gameName)
{
    using namespace namerules;
    const QString seg = normalizeGameName(segment, Separators::Compact);
    const QString game = normalizeGameName(gameName, Separators::Compact);
    if (seg.isEmpty() || game.isEmpty()) return false;
    if (seg == game) return true;
    return seg.size() * 2 >= game.size() && seg.size() >= 4
        && gameNamesLikelyMatch(segment, gameName);
}

// Where in `path` (a Windows path on the translator's machine) the game's own
// folder ends, as a number of leading segments; 0 when it names no game
// folder. A store library tells it by layout (steamapps\common\<Game>, Epic
// Games\<Game>, …), anything else by the game's name. The layout alone is
// enough only with `layoutIsEnough` (a variable that says it holds the game
// folder): steamapps\common\Steamworks Shared and Epic Games\Launcher sit
// in the same places and are not the game.
inline int gameFolderDepth(const QString& path, const QString& gameName,
                           bool layoutIsEnough = false)
{
    const QStringList segs = segmentsOf(path);
    if (segs.size() < 2 || segs.first().size() != 2 || segs.first().at(1) != QLatin1Char(':'))
        return 0;
    // The store folder's game segment, when it names this game or need not.
    const auto store = [&](int at) {
        return layoutIsEnough || segmentNamesGame(segs[at], gameName) ? at + 1 : 0;
    };
    for (int i = 1; i + 1 < segs.size(); ++i) {
        const QString s = segs[i].toLower();
        const QString next = segs[i + 1].toLower();
        int depth = 0;
        if (s == QLatin1String("steamapps") && next == QLatin1String("common") && i + 2 < segs.size())
            depth = store(i + 2);
        else if (s == QLatin1String("epic games") || s == QLatin1String("gog games")
                 || s == QLatin1String("xboxgames"))
            depth = store(i + 1);
        else if (s == QLatin1String("gog galaxy") && next == QLatin1String("games") && i + 2 < segs.size())
            depth = store(i + 2);
        else if (s == QLatin1String("ubisoft game launcher") && next == QLatin1String("games")
                 && i + 2 < segs.size())
            depth = store(i + 2);
        if (depth > 0) return depth;
    }
    for (int i = segs.size() - 1; i >= 1; --i)
        if (segmentNamesGame(segs[i], gameName)) return i + 1;
    return 0;
}

// The hard-coded game folders a script names, as written in it.
struct Rewrite {
    QByteArray text;
    QStringList folders;   // the translator's folders that now read MAKINE_OYUN
    QStringList variables; // the variables set to them
};

namespace detail {

inline bool isPathEnd(QChar c)
{
    return c == QLatin1Char('"') || c == QLatin1Char('\r') || c == QLatin1Char('\n')
        || c == QLatin1Char('&') || c == QLatin1Char('|') || c == QLatin1Char('>')
        || c == QLatin1Char('<') || c == QLatin1Char(')');
}

// Replace every occurrence of `folder` (case-insensitive, followed by a path
// boundary) in `text` with %MAKINE_OYUN%.
inline int replaceFolder(QString& text, const QString& folder)
{
    const QString with = QStringLiteral("%") + QLatin1String(kGameVar) + QStringLiteral("%");
    int n = 0;
    qsizetype from = 0;
    while ((from = text.indexOf(folder, from, Qt::CaseInsensitive)) >= 0) {
        const qsizetype end = from + folder.size();
        const bool boundary = end >= text.size() || text[end] == QLatin1Char('\\')
            || text[end] == QLatin1Char('/') || isPathEnd(text[end]) || text[end].isSpace();
        if (!boundary) { from = end; continue; }
        text.replace(from, folder.size(), with);
        from += with.size();
        ++n;
    }
    return n;
}

} // namespace detail

// A .bat/.cmd with the game folder taken from MAKINE_OYUN wherever it was
// hard-coded. `script` is the file's bytes; every byte that does not belong to
// a replaced folder comes out as it went in (the script's own code page and
// line endings stay). Two forms are found:
//   set OYUN=D:\SteamLibrary\steamapps\common\HEAVY RAIN    (and set "OYUN=…",
//   set OYUN= "…"): a variable translators name the game folder, or any
//   variable set to a store game folder or a folder named like the game;
//   and the same folder written out anywhere else in the script
//   (cd /d "D:\Games\Assassin's Creed Shadows", xcopy … "D:\…\HEAVY RAIN\").
// A line that already sets the folder the launcher found is left alone.
inline Rewrite rewriteScript(const QByteArray& script, const QString& gameName,
                             const QString& gamePath = {})
{
    Rewrite out;
    // One character per byte: what is not touched is written back unchanged.
    QString text = QString::fromLatin1(script);
    const QString eol = text.contains(QLatin1String("\r\n")) ? QStringLiteral("\r\n")
                                                              : QStringLiteral("\n");
    const QString found = QDir::toNativeSeparators(QDir::cleanPath(gamePath));

    // Pass 1: the lines that set a variable to the game folder.
    QStringList lines = text.split(eol);
    for (QString& line : lines) {
        qsizetype i = 0;
        while (i < line.size() && (line[i] == QLatin1Char(' ') || line[i] == QLatin1Char('\t')
                                   || line[i] == QLatin1Char('@')))
            ++i;
        const QString lead = line.left(i);
        if (line.mid(i, 3).compare(QLatin1String("set"), Qt::CaseInsensitive) != 0
            || i + 3 >= line.size() || !line[i + 3].isSpace())
            continue;
        qsizetype p = i + 3;
        while (p < line.size() && line[p].isSpace()) ++p;
        const bool quotedAll = p < line.size() && line[p] == QLatin1Char('"');
        if (quotedAll) ++p;
        const qsizetype eq = line.indexOf(QLatin1Char('='), p);
        if (eq <= p) continue;
        const QString name = line.mid(p, eq - p);
        if (name.isEmpty() || name.contains(QLatin1Char(' ')) || name.startsWith(QLatin1Char('/')))
            continue;   // "set /p", "set /a"
        QString value = line.mid(eq + 1);
        if (quotedAll) {
            const qsizetype close = value.lastIndexOf(QLatin1Char('"'));
            if (close < 0) continue;
            if (!value.mid(close + 1).trimmed().isEmpty()) continue;  // more after it
            value.truncate(close);
        }
        value = value.trimmed();
        const bool valueQuoted = value.size() >= 2 && value.startsWith(QLatin1Char('"'))
                                 && value.endsWith(QLatin1Char('"'));
        if (valueQuoted)
            value = value.mid(1, value.size() - 2).trimmed();
        while (value.endsWith(QLatin1Char('\\')) && value.size() > 3) value.chop(1);
        if (value.isEmpty() || value.contains(QLatin1Char('%')))
            continue;   // unset, or taken from an argument / another variable
        const bool absolute = value.size() >= 3 && value[0].isLetter() && value[1] == QLatin1Char(':')
            && (value[2] == QLatin1Char('\\') || value[2] == QLatin1Char('/'));
        if (absolute) {
            const int depth = gameFolderDepth(value, gameName, isGameVarName(name));
            const bool whole = depth == segmentsOf(value).size();
            if (!(isGameVarName(name) && (depth == 0 || whole)) && !whole)
                continue;
        } else if (!isGameVarName(name)
                   || !(value.contains(QLatin1Char('\\')) || value.contains(QLatin1Char('/'))
                        || value.contains(QLatin1Char('.')))) {
            // A relative value is the game folder only when the variable says
            // so and it reads like a path: "..", "game.path" (a placeholder
            // a translator meant to be filled in), not "HeavyRain".
            continue;
        }
        if (!found.isEmpty() && QDir::cleanPath(value).compare(QDir::cleanPath(found), Qt::CaseInsensitive) == 0)
            continue;   // already right
        out.folders.append(value);
        out.variables.append(name);
        // The value keeps the quotes it had: a script that wrote
        // set OYUN="D:\…" uses %OYUN% unquoted, and the player's folder
        // (C:\Program Files (x86)\…) must stay one argument there.
        const QString var = QStringLiteral("%") + QLatin1String(kGameVar) + QStringLiteral("%");
        line = lead + (valueQuoted && !quotedAll
                           ? QStringLiteral("set ") + name + QStringLiteral("=\"") + var + QStringLiteral("\"")
                           : QStringLiteral("set \"") + name + QLatin1Char('=') + var + QStringLiteral("\""));
    }
    text = lines.join(eol);

    // Pass 2: the folder written out elsewhere — the ones pass 1 found, and
    // any absolute path that runs through a folder named like the game.
    // Only absolute folders are searched for elsewhere: a relative one
    // ("..", "game.path") also stands for other things in a script.
    QStringList folders;
    for (const QString& f : out.folders)
        if (f.size() >= 3 && f[1] == QLatin1Char(':')) folders.append(f);
    for (qsizetype at = 0; (at = text.indexOf(QLatin1String(":\\"), at)) >= 0; at += 2) {
        if (at == 0 || !text[at - 1].isLetter()) continue;
        if (at >= 2 && (text[at - 2].isLetterOrNumber() || text[at - 2] == QLatin1Char('%')))
            continue;
        // Quoted or not, up to the end of the path: unquoted folders with
        // spaces ("cd /d D:\Oyunlar\Heavy Rain") run to the end of the line,
        // and cutting at the space would name a folder that is not there.
        qsizetype end = at + 2;
        while (end < text.size() && !detail::isPathEnd(text[end]))
            ++end;
        const QString path = text.mid(at - 1, end - at + 1);
        const int depth = gameFolderDepth(path, gameName);
        if (depth < 2) continue;
        const QString folder = segmentsOf(path).mid(0, depth).join(QLatin1Char('\\'));
        if (!found.isEmpty() && folder.compare(found, Qt::CaseInsensitive) == 0) continue;
        if (!folders.contains(folder, Qt::CaseInsensitive)) folders.append(folder);
    }
    int elsewhere = 0;
    for (const QString& f : folders) {
        elsewhere += detail::replaceFolder(text, f);
        if (!out.folders.contains(f, Qt::CaseInsensitive)) out.folders.append(f);
    }
    if (out.variables.isEmpty() && elsewhere == 0) {
        out.text = script;
        out.folders.clear();
        return out;
    }
    out.text = text.toLatin1();
    return out;
}

// A small file that holds nothing but the game folder ("game.path" next to
// Heavy Rain's scripts: D:\SteamLibrary\steamapps\common\HEAVY RAIN): the
// launcher's folder in its place, UTF-8, its line ending kept. Empty when the
// file is anything else.
inline QByteArray rewritePathFile(const QByteArray& content, const QString& gameName,
                                  const QString& gamePath)
{
    if (content.size() > 1024 || gamePath.isEmpty()) return {};
    QString text = QString::fromUtf8(content);
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);
    const QString value = text.trimmed();
    if (value.contains(QLatin1Char('\n')) || value.size() < 3 || !value[0].isLetter()
        || value[1] != QLatin1Char(':') || (value[2] != QLatin1Char('\\') && value[2] != QLatin1Char('/')))
        return {};
    QString folder = value;
    while (folder.endsWith(QLatin1Char('\\')) || folder.endsWith(QLatin1Char('/'))) folder.chop(1);
    if (gameFolderDepth(folder, gameName) != segmentsOf(folder).size()) return {};
    const QString found = QDir::toNativeSeparators(QDir::cleanPath(gamePath));
    if (folder.compare(found, Qt::CaseInsensitive) == 0) return {};
    const QString tail = text.endsWith(QLatin1String("\r\n")) ? QStringLiteral("\r\n")
                       : text.endsWith(QLatin1Char('\n')) ? QStringLiteral("\n") : QString();
    return (found + tail).toUtf8();
}

// <game>/.makine: what the launcher knows about the folder, in the form a .bat
// can take in (`for /f "usebackq delims=" %%L in ("%OYUN%\.makine") do %%L`)
// and a person can read. UTF-8 — paths may hold any letter.
inline QByteArray noteFile(const QString& gamePath, const QString& gameId, const QString& gameName)
{
    const QString game = QDir::toNativeSeparators(QDir::cleanPath(gamePath));
    QString s;
    s += QStringLiteral("@rem Makine Launcher: bu klas\u00f6r %1 oyununun klas\u00f6r\u00fc.\r\n")
             .arg(gameName.isEmpty() ? gameId : gameName);
    s += QStringLiteral("@rem Yaman\u0131n program\u0131 oyunun yerini buradan okuyabilir. "
                        "Yama kald\u0131r\u0131l\u0131nca launcher bu dosyay\u0131 siler.\r\n");
    s += QStringLiteral("set \"OYUN=%1\"\r\n").arg(game);
    s += QStringLiteral("set \"%1=%2\"\r\n").arg(QLatin1String(kGameVar), game);
    s += QStringLiteral("set \"%1=%2\"\r\n").arg(QLatin1String(kGameIdVar), gameId);
    return s.toUtf8();
}

// Is the file at `path` a Windows GUI program (it opens a window, prints
// nothing)? Read from its PE header. False for scripts, console programs and
// anything unreadable.
inline bool isWindowedProgram(const QByteArray& head)
{
    if (head.size() < 0x40 || head[0] != 'M' || head[1] != 'Z') return false;
    const auto u32 = [&head](qsizetype at) -> quint32 {
        if (at + 4 > head.size()) return 0;
        return quint32(uchar(head[at])) | quint32(uchar(head[at + 1])) << 8
             | quint32(uchar(head[at + 2])) << 16 | quint32(uchar(head[at + 3])) << 24;
    };
    const qsizetype pe = u32(0x3C);
    if (pe <= 0 || pe + 24 + 70 > head.size()) return false;
    if (head.mid(pe, 4) != QByteArray("PE\0\0", 4)) return false;
    // Subsystem sits 68 bytes into the optional header, for PE32 and PE32+.
    const qsizetype sub = pe + 24 + 68;
    const int subsystem = uchar(head[sub]) | uchar(head[sub + 1]) << 8;
    return subsystem == 2;   // IMAGE_SUBSYSTEM_WINDOWS_GUI
}

} // namespace makine::runstage
