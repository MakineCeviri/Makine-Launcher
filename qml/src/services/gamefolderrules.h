// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// What a folder the user points the launcher at actually is.
//
// Adding a game by hand used to take the chosen folder as-is, and a patch
// installed later went wherever that folder pointed. The field shows the ways
// that goes wrong: users pick their Desktop, the folder the .exe sits in rather
// than the game's own folder ("ELDEN RING/Game", "<Project>/Binaries/Win64"),
// or the folder that holds all their games. Each rule lives here, free of
// GameService, so it can be exercised on a real directory tree — see
// tests/ui/test_gamefolderrules.cpp.

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "gamenamerules.h"
#include "rawfile.h"

namespace makine::folderrules {

// QML hands over a URL ("file:///D:/Oyunlar/A%23B"), the process scanner a
// native path. Cutting "file:///" off by hand kept the percent-encoding and
// broke network shares; QUrl undoes both.
inline QString toLocalPath(const QString& input)
{
    const QString s = input.trimmed();
    if (s.isEmpty()) return {};
    const QString local = s.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)
                              ? QUrl(s).toLocalFile()
                              : s;
    return local.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(local));
}

inline QString parentOf(const QString& path) { return QFileInfo(path).absolutePath(); }
inline QString lowerNameOf(const QString& path) { return QFileInfo(path).fileName().toLower(); }

inline bool isSameOrUnder(const QString& path, const QString& base)
{
    if (base.isEmpty()) return false;
    const QString p = QDir::cleanPath(path);
    const QString b = QDir::cleanPath(base);
    return p.compare(b, Qt::CaseInsensitive) == 0
        || p.startsWith(b + QLatin1Char('/'), Qt::CaseInsensitive);
}

inline bool hasExecutableAtTop(const QString& dir)
{
    return !QDir(dir).entryList({QStringLiteral("*.exe")}, QDir::Files).isEmpty();
}

// An .exe at most `maxDepth` folders below `dir`. Unreal keeps it three levels
// down (<Project>/Binaries/Win64), Dying Light 2 four (ph/work/bin/x64). Never
// descends past the limit: the folder may be a whole library of games.
inline bool hasExecutableWithin(const QString& dir, int maxDepth)
{
    if (hasExecutableAtTop(dir)) return true;
    if (maxDepth <= 0) return false;
    const QDir d(dir);
    for (const QString& child : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
        if (hasExecutableWithin(d.filePath(child), maxDepth - 1)) return true;
    return false;
}

// Folders a game install carries next to the game itself, often with
// executables of their own. They say nothing about how many games sit
// side by side.
inline bool isAuxiliaryFolder(const QString& name)
{
    static const QSet<QString> kAuxiliary = {
        QStringLiteral("_commonredist"), QStringLiteral("commonredist"),
        QStringLiteral("redist"), QStringLiteral("_redist"),
        QStringLiteral("redistributables"), QStringLiteral("directx"),
        QStringLiteral("vcredist"), QStringLiteral("support"),
        QStringLiteral("prerequisites"), QStringLiteral("installers"),
        QStringLiteral("engine"), QStringLiteral("launcher"), QStringLiteral("tools"),
        QStringLiteral("easyanticheat"), QStringLiteral("battleye"),
        QStringLiteral("bin"), QStringLiteral("bin32"), QStringLiteral("bin64"),
        QStringLiteral("binaries"), QStringLiteral("x86"), QStringLiteral("x64"),
        QStringLiteral("win32"), QStringLiteral("win64"),
    };
    return kAuxiliary.contains(name.toLower());
}

// Does `dir` hold several games side by side ("D:/Oyunlar") rather than one?
// Bound to a single game, such a folder would take the patch next to every
// game in it.
inline bool looksLikeGameCollection(const QString& dir)
{
    if (hasExecutableAtTop(dir)) return false;
    const QDir d(dir);
    int gameLike = 0;
    for (const QString& child : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (isAuxiliaryFolder(child)) continue;
        if (hasExecutableWithin(d.filePath(child), 3) && ++gameLike >= 2)
            return true;
    }
    return false;
}

// Could `dir` be one game's own folder? A drive root, a store library or the
// folder a user keeps their games in never is — climbing into one would hand
// the patch the whole library.
inline bool canBeGameRoot(const QString& dir)
{
    if (QDir(dir).isRoot()) return false;
    static const QSet<QString> kLibraries = {
        QStringLiteral("games"), QStringLiteral("oyunlar"), QStringLiteral("oyun"),
        QStringLiteral("common"), QStringLiteral("steamapps"), QStringLiteral("steamlibrary"),
        QStringLiteral("xboxgames"), QStringLiteral("gog games"), QStringLiteral("epic games"),
        QStringLiteral("program files"), QStringLiteral("program files (x86)"),
        QStringLiteral("desktop"), QStringLiteral("downloads"), QStringLiteral("documents"),
    };
    if (kLibraries.contains(lowerNameOf(dir))) return false;
    return !looksLikeGameCollection(dir);
}

// An archive unpacked into a folder of its own name: <Name>/<Name>/<game>.exe,
// nothing runnable in the outer one. A scan that stops at the outer folder
// installs the patch next to the game instead of into it (Red Dead Redemption 2
// on a repack: the patch's dinput8.dll never loaded). The inner folder, or
// `dir` itself when it is no such wrapper.
inline QString unwrapSameNameFolder(const QString& dir)
{
    const QString clean = QDir::cleanPath(dir);
    if (hasExecutableAtTop(clean)) return clean;
    const QString inner = clean + QLatin1Char('/') + QFileInfo(clean).fileName();
    return QFileInfo(inner).isDir() && hasExecutableAtTop(inner) ? inner : clean;
}

// The folder patch recipes are written against — the one a store installs the
// game into — given a folder somewhere around it. Users open the folder the
// .exe sits in, which is often one to three levels too deep; a patch installed
// there lands in "Game/Game/…" and the game never sees it. Only layouts that
// are recognisable on their own are climbed out of; anything else stays put.
inline QString resolveGameRoot(const QString& dir)
{
    const QString clean = QDir::cleanPath(dir);
    const QString name = lowerNameOf(clean);
    const QString parent = parentOf(clean);
    const QString parentName = lowerNameOf(parent);

    // Unreal: <Root>/<Project>/Binaries/Win64/<Project>-Win64-Shipping.exe, with
    // Engine/ next to <Project>. Unreal 3 drops the project level and keeps
    // Binaries/ and Engine/ in the root itself.
    const auto unrealRoot = [](const QString& project) {
        const QString root = parentOf(project);
        return QDir(root + QStringLiteral("/Engine")).exists() && canBeGameRoot(root)
                   ? root : project;
    };
    if ((name == QLatin1String("win64") || name == QLatin1String("win32")
         || name == QLatin1String("wingdk"))
        && parentName == QLatin1String("binaries")) {
        const QString project = parentOf(parent);
        if (canBeGameRoot(project)) return unrealRoot(project);
    }
    if (name == QLatin1String("binaries") && canBeGameRoot(parent))
        return unrealRoot(parent);
    if (QDir(clean + QStringLiteral("/Binaries")).exists()
        && QDir(parent + QStringLiteral("/Engine")).exists() && canBeGameRoot(parent))
        return parent;

    // <Root>/bin/x64/<game>.exe (REDengine) and <Root>/bin/<game>.exe.
    if ((name == QLatin1String("x64") || name == QLatin1String("x86"))
        && (parentName == QLatin1String("bin") || parentName == QLatin1String("bin64"))) {
        const QString root = parentOf(parent);
        if (canBeGameRoot(root)) return root;
    }
    if ((name == QLatin1String("bin") || name == QLatin1String("bin64")) && canBeGameRoot(parent))
        return parent;

    // FromSoftware: <Root>/Game/<game>.exe and nothing runnable in <Root>. A
    // folder the user named "Game" inside their games library stays put:
    // canBeGameRoot refuses the library.
    if (name == QLatin1String("game") && hasExecutableAtTop(clean)
        && !hasExecutableAtTop(parent) && canBeGameRoot(parent))
        return parent;

    if (const QString inner = unwrapSameNameFolder(clean); inner != clean)
        return inner;

    // Microsoft Store / Game Pass: XboxGames/<Game>/Content/<game>.exe. Here the
    // game is Content, one level BELOW the folder carrying its name.
    const QString content = clean + QStringLiteral("/Content");
    if (!hasExecutableAtTop(clean) && hasExecutableAtTop(content))
        return content;

    return clean;
}

// Why a folder cannot hold a game, before anything is matched against it.
enum class FolderProblem {
    None,
    Missing,       // not an existing folder
    DriveRoot,     // "D:/" — never a game's own folder
    Protected,     // Windows, ProgramData, the user's home/Desktop/Documents/Downloads
    NoExecutable,  // nothing runnable anywhere inside
    Collection,    // several games side by side, or the folder games are kept in
};

// `protectedTrees`: folders a patch must never be written into, nor anywhere
// below them. `protectedFolders`: folders that are no game's own folder though
// their subfolders may well be (the user's Desktop, Program Files).
inline FolderProblem checkGameFolder(const QString& dir,
                                     const QStringList& protectedTrees,
                                     const QStringList& protectedFolders)
{
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) return FolderProblem::Missing;
    if (QDir(dir).isRoot()) return FolderProblem::DriveRoot;
    for (const QString& tree : protectedTrees)
        if (isSameOrUnder(dir, tree)) return FolderProblem::Protected;
    for (const QString& folder : protectedFolders)
        if (!folder.isEmpty()
            && QDir::cleanPath(dir).compare(QDir::cleanPath(folder), Qt::CaseInsensitive) == 0)
            return FolderProblem::Protected;
    if (!hasExecutableWithin(dir, 4)) return FolderProblem::NoExecutable;
    return FolderProblem::None;
}

// The folder a hand-picked game is bound to — `root` — or why there is none.
struct FolderVerdict {
    QString root;
    FolderProblem problem = FolderProblem::None;
};

inline FolderVerdict judgeGameFolder(const QString& dir,
                                     const QStringList& protectedTrees,
                                     const QStringList& protectedFolders)
{
    if (const FolderProblem p = checkGameFolder(dir, protectedTrees, protectedFolders);
        p != FolderProblem::None)
        return {QString(), p};
    const QString root = resolveGameRoot(dir);
    // Climbing out of "bin" may land on a folder that is itself off limits
    // (the user's home); the climb is only worth what the root it reaches is.
    if (root != QDir::cleanPath(dir)) {
        if (const FolderProblem p = checkGameFolder(root, protectedTrees, protectedFolders);
            p != FolderProblem::None)
            return {QString(), p};
    }
    if (!canBeGameRoot(root)) return {QString(), FolderProblem::Collection};
    return {root, FolderProblem::None};
}

// ── The root a specific patch needs ──────────────────────────────────────────
//
// resolveGameRoot knows engine layouts; it does not know the game. When the
// user points at a folder for a catalogue game, the patch itself says what its
// root must look like: the executables the catalogue lists for it, the folders
// its recipe writes into, the files it edits in place. Each folder around the
// pick is weighed against that, so the patch lands where its paths resolve —
// not wherever the user happened to click.

// What a patch expects under its game's root. Paths are relative, "/"-separated.
struct Expectations {
    // Lowercase, from the catalogue fingerprint: file names, or paths relative
    // to the root ("game/eldenring.exe") where the store's launch entry gives one.
    QStringList exeNames;
    QStringList targetDirs;   // folders the recipe or package writes into
    QStringList targetFiles;  // game files the recipe edits in place (must exist)
    // Files the recipe says every install of the game has ("requires"): a
    // root without one of them is not this game's, whatever else it holds.
    // Also counted in targetFiles, so pickRoot weighs them.
    QStringList requiredFiles;
};

// Which of `e.requiredFiles` `root` lacks. Empty when it has them all (or the
// recipe names none) — the one answer that lets an install go ahead.
inline QStringList missingRequired(const QString& root, const Expectations& e)
{
    QStringList missing;
    for (const QString& f : e.requiredFiles)
        if (!rawfile::exists(root + QLatin1Char('/') + f)) missing << f;
    return missing;
}


struct RootEvidence {
    QString root;
    QString exe;              // an expected exe found under root, relative ("" = none)
    bool exeExact = false;    // found where the catalogue says it sits, not just by name
    int dirsFound = 0;
    int dirsTotal = 0;
    int filesFound = 0;
    int filesTotal = 0;
    int score = 0;
};

// The first of `exeNames` found at most `maxDepth` folders below `dir`,
// relative to it. Bounded in entries visited: `dir` may be a big install.
inline QString findExpectedExe(const QString& dir, const QStringList& exeNames, int maxDepth = 4)
{
    QSet<QString> wanted;
    for (const QString& e : exeNames)
        wanted.insert(e.mid(e.lastIndexOf(QLatin1Char('/')) + 1));
    if (wanted.isEmpty()) return {};
    QStringList level{QString()};
    int visited = 0;
    for (int depth = 0; depth <= maxDepth && !level.isEmpty(); ++depth) {
        QStringList next;
        for (const QString& rel : level) {
            const QDir d(rel.isEmpty() ? dir : dir + QLatin1Char('/') + rel);
            for (const QString& f : d.entryList({QStringLiteral("*.exe")}, QDir::Files))
                if (wanted.contains(f.toLower()))
                    return rel.isEmpty() ? f : rel + QLatin1Char('/') + f;
            for (const QString& sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
                if (++visited > 2000) return {};
                next << (rel.isEmpty() ? sub : rel + QLatin1Char('/') + sub);
            }
        }
        level = next;
    }
    return {};
}

// How well `root` matches what the patch expects. The weights rank the kinds
// of proof: a file the recipe rewrites in place either is there or the patch
// cannot run; the game's own exe says "this is the game"; folders a recipe
// copies into are weaker, since some are created by the install itself; and
// any exe at all only says "some game", enough to climb out of a data folder
// the user opened ("<Game>/LocalCacheWinGame") when nothing else speaks.
inline RootEvidence weighRoot(const QString& root, const Expectations& e)
{
    RootEvidence r;
    r.root = QDir::cleanPath(root);
    // A path the catalogue gives pins the root exactly: the same exe found one
    // folder deeper or higher belongs to a folder next to the right one.
    for (const QString& p : e.exeNames) {
        if (p.contains(QLatin1Char('/')) && QFileInfo(r.root + QLatin1Char('/') + p).isFile()) {
            r.exe = p;
            r.exeExact = true;
            break;
        }
    }
    if (r.exe.isEmpty()) r.exe = findExpectedExe(r.root, e.exeNames);
    r.dirsTotal = static_cast<int>(e.targetDirs.size());
    for (const QString& d : e.targetDirs)
        if (QFileInfo(r.root + QLatin1Char('/') + d).isDir()) ++r.dirsFound;
    r.filesTotal = static_cast<int>(e.targetFiles.size());
    for (const QString& f : e.targetFiles)
        if (QFileInfo(r.root + QLatin1Char('/') + f).isFile()) ++r.filesFound;
    r.score = (r.exeExact ? 45 : !r.exe.isEmpty() ? 40 : hasExecutableWithin(r.root, 4) ? 5 : 0)
            + (r.dirsTotal > 0 ? 30 * r.dirsFound / r.dirsTotal : 0)
            + (r.filesTotal > 0 ? 50 * r.filesFound / r.filesTotal : 0);
    return r;
}

// Folders that could be the root, best guess first: the engine-layout root of
// the pick, the pick, up to three folders above it (the user opened something
// deep inside the game), and — when the pick is a library rather than a game —
// the subfolders named like the game.
inline QStringList rootCandidates(const QString& picked, const QString& gameName)
{
    QStringList out;
    const auto add = [&out](const QString& p) {
        const QString c = QDir::cleanPath(p);
        if (!c.isEmpty() && QFileInfo(c).isDir() && !out.contains(c, Qt::CaseInsensitive)
            && canBeGameRoot(c))
            out << c;
    };
    const QString base = QFileInfo(picked).isFile() ? parentOf(picked) : QDir::cleanPath(picked);
    if (canBeGameRoot(base)) {
        add(resolveGameRoot(base));
        add(base);
        QString up = base;
        for (int i = 0; i < 3; ++i) {
            up = parentOf(up);
            if (QDir(up).isRoot() || !canBeGameRoot(up)) break;
            add(up);
        }
    } else if (!gameName.isEmpty()) {
        // A library is never climbed out of: what is above it is even less a game.
        const QDir d(base);
        for (const QString& child : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (namerules::gameNamesLikelyMatch(child, gameName)) {
                add(resolveGameRoot(d.filePath(child)));
                add(d.filePath(child));
            }
        }
    }
    return out;
}

// The root among the candidates that the patch fits best. On a tie the earlier
// candidate wins, so with nothing to weigh against this is resolveGameRoot's
// answer. Empty `root` when no candidate can be a game's folder at all.
inline RootEvidence pickRoot(const QString& picked, const QString& gameName, const Expectations& e)
{
    RootEvidence best;
    for (const QString& c : rootCandidates(picked, gameName)) {
        const RootEvidence r = weighRoot(c, e);
        if (best.root.isEmpty() || r.score > best.score)
            best = r;
    }
    return best;
}

// The folder around `root` that has every required file, when `root` itself
// does not: the scan's folder one level off (a "<Game>/<Game>" repack, Game
// Pass's Content). Empty when there is none, or nothing is required.
inline QString rootWithRequired(const QString& root, const QString& gameName, const Expectations& e)
{
    if (e.requiredFiles.isEmpty() || missingRequired(root, e).isEmpty()) return {};
    for (const QString& c : rootCandidates(root, gameName))
        if (missingRequired(c, e).isEmpty()) return c;
    return {};
}

} // namespace makine::folderrules
