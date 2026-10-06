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

} // namespace makine::folderrules
