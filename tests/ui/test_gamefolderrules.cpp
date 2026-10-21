// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Rules for a game folder the user picks by hand. Each layout below is one a
// user actually lands in when they open "the folder with the .exe" — the root
// the rules must find is the one a store would have installed the game into.

#include "gamefolderrules.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace makine::folderrules;

namespace {

class GameFolderRulesTest : public ::testing::Test {
protected:
    QTemporaryDir tmp;

    QString path(const QString& rel) const { return QDir::cleanPath(tmp.path() + '/' + rel); }

    // Creates every folder on the way and an empty file at `rel`.
    QString touch(const QString& rel) const
    {
        const QString full = path(rel);
        QDir().mkpath(QFileInfo(full).absolutePath());
        QFile f(full);
        EXPECT_TRUE(f.open(QIODevice::WriteOnly));
        return full;
    }

    QString mkdir(const QString& rel) const
    {
        QDir().mkpath(path(rel));
        return path(rel);
    }
};

} // namespace

// --- toLocalPath -----------------------------------------------------------

TEST(GameFolderPathTest, AcceptsUrlsFromQml)
{
    EXPECT_EQ(toLocalPath(QStringLiteral("file:///D:/Oyunlar/Elden Ring")),
              QStringLiteral("D:/Oyunlar/Elden Ring"));
    // Stripping "file:///" by hand left these encoded.
    EXPECT_EQ(toLocalPath(QStringLiteral("file:///D:/Oyunlar/A%23B")), QStringLiteral("D:/Oyunlar/A#B"));
    EXPECT_EQ(toLocalPath(QStringLiteral("file:///D:/Oyunlar/%C5%9Eeytan")),
              QString::fromUtf8("D:/Oyunlar/Şeytan"));
}

TEST(GameFolderPathTest, AcceptsNativeAndNetworkPaths)
{
    EXPECT_EQ(toLocalPath(QStringLiteral("C:\\Games\\RDR2\\")), QStringLiteral("C:/Games/RDR2"));
    EXPECT_EQ(toLocalPath(QStringLiteral("file://nas/oyunlar/Hades")), QStringLiteral("//nas/oyunlar/Hades"));
    EXPECT_TRUE(toLocalPath(QStringLiteral("   ")).isEmpty());
}

// --- resolveGameRoot -------------------------------------------------------

TEST_F(GameFolderRulesTest, FromSoftwareGameFolderClimbsToRoot)
{
    touch(QStringLiteral("lib/ELDEN RING/Game/eldenring.exe"));
    touch(QStringLiteral("lib/ELDEN RING/_CommonRedist/vc_redist.x64.exe"));
    touch(QStringLiteral("lib/Hades/Hades.exe"));

    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("lib/ELDEN RING/Game"))),
              path(QStringLiteral("lib/ELDEN RING")));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("lib/ELDEN RING"))),
              path(QStringLiteral("lib/ELDEN RING")));
}

TEST_F(GameFolderRulesTest, GameFolderInsideLibraryStaysPut)
{
    // The user's own folder called "Game" in a folder of games: climbing would
    // bind the whole library.
    touch(QStringLiteral("Oyunlar/Game/game.exe"));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("Oyunlar/Game"))), path(QStringLiteral("Oyunlar/Game")));

    touch(QStringLiteral("stuff/Game/game.exe"));
    touch(QStringLiteral("stuff/Other/other.exe"));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("stuff/Game"))), path(QStringLiteral("stuff/Game")));
}

TEST_F(GameFolderRulesTest, UnrealBinariesClimbToRootWithEngine)
{
    touch(QStringLiteral("lib/Hogwarts Legacy/HogwartsLegacy.exe"));
    touch(QStringLiteral("lib/Hogwarts Legacy/Engine/Binaries/Win64/CrashReportClient.exe"));
    touch(QStringLiteral("lib/Hogwarts Legacy/Phoenix/Binaries/Win64/HogwartsLegacy.exe"));
    touch(QStringLiteral("lib/Other Game/other.exe"));
    const QString root = path(QStringLiteral("lib/Hogwarts Legacy"));

    EXPECT_EQ(resolveGameRoot(root + QStringLiteral("/Phoenix/Binaries/Win64")), root);
    EXPECT_EQ(resolveGameRoot(root + QStringLiteral("/Phoenix/Binaries")), root);
    EXPECT_EQ(resolveGameRoot(root + QStringLiteral("/Phoenix")), root);
    EXPECT_EQ(resolveGameRoot(root), root);
}

TEST_F(GameFolderRulesTest, Unreal3BinariesClimbOneProjectLess)
{
    touch(QStringLiteral("lib/Batman/Binaries/Win32/BmLauncher.exe"));
    mkdir(QStringLiteral("lib/Batman/Engine"));
    touch(QStringLiteral("lib/Other/other.exe"));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("lib/Batman/Binaries/Win32"))),
              path(QStringLiteral("lib/Batman")));
}

TEST_F(GameFolderRulesTest, BinFoldersClimbToRoot)
{
    touch(QStringLiteral("lib/The Witcher 3/bin/x64/witcher3.exe"));
    touch(QStringLiteral("lib/Baldurs Gate 3/bin/bg3.exe"));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("lib/The Witcher 3/bin/x64"))),
              path(QStringLiteral("lib/The Witcher 3")));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("lib/Baldurs Gate 3/bin"))),
              path(QStringLiteral("lib/Baldurs Gate 3")));
}

TEST_F(GameFolderRulesTest, XboxWrapperDescendsIntoContent)
{
    touch(QStringLiteral("XboxGames/Starfield/Content/Starfield.exe"));
    const QString content = path(QStringLiteral("XboxGames/Starfield/Content"));
    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("XboxGames/Starfield"))), content);
    EXPECT_EQ(resolveGameRoot(content), content);
}

// --- looksLikeGameCollection ----------------------------------------------

TEST_F(GameFolderRulesTest, CollectionNeedsTwoGamesNotRedistributables)
{
    touch(QStringLiteral("lib/Hades/Hades.exe"));
    touch(QStringLiteral("lib/Celeste/Celeste.exe"));
    EXPECT_TRUE(looksLikeGameCollection(path(QStringLiteral("lib"))));

    touch(QStringLiteral("ER/Game/eldenring.exe"));
    touch(QStringLiteral("ER/_CommonRedist/vc_redist.x64.exe"));
    EXPECT_FALSE(looksLikeGameCollection(path(QStringLiteral("ER"))));

    touch(QStringLiteral("UE/Engine/Binaries/Win64/CrashReportClient.exe"));
    touch(QStringLiteral("UE/Proj/Binaries/Win64/Proj.exe"));
    EXPECT_FALSE(looksLikeGameCollection(path(QStringLiteral("UE"))));

    touch(QStringLiteral("RDR2/RDR2.exe"));
    touch(QStringLiteral("RDR2/x64/tool.exe"));
    touch(QStringLiteral("RDR2/redux/tool2.exe"));
    EXPECT_FALSE(looksLikeGameCollection(path(QStringLiteral("RDR2"))));  // runnable at the top
}

// --- checkGameFolder -------------------------------------------------------

TEST_F(GameFolderRulesTest, RejectsWhatCannotHoldAGame)
{
    const QStringList none;
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("nope")), none, none), FolderProblem::Missing);
    EXPECT_EQ(checkGameFolder(QString(), none, none), FolderProblem::Missing);
    EXPECT_EQ(checkGameFolder(QDir::rootPath(), none, none), FolderProblem::DriveRoot);

    EXPECT_EQ(checkGameFolder(mkdir(QStringLiteral("empty")), none, none), FolderProblem::NoExecutable);
    touch(QStringLiteral("deep/a/b/c/d/e/game.exe"));  // five levels down
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("deep")), none, none), FolderProblem::NoExecutable);
    touch(QStringLiteral("DL2/ph/work/bin/x64/DyingLightGame_x64_rwdi.exe"));  // four
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("DL2")), none, none), FolderProblem::None);
}

TEST_F(GameFolderRulesTest, ProtectedTreesAndFolders)
{
    touch(QStringLiteral("Windows/System32/app.exe"));
    touch(QStringLiteral("Users/me/Desktop/shortcut-target.exe"));
    touch(QStringLiteral("Users/me/Desktop/Hades/Hades.exe"));
    const QStringList trees{path(QStringLiteral("Windows"))};
    const QStringList folders{path(QStringLiteral("Users/me/Desktop"))};

    EXPECT_EQ(checkGameFolder(path(QStringLiteral("Windows/System32")), trees, folders),
              FolderProblem::Protected);
    // The Desktop itself is no game's folder — a game folder ON it is.
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("Users/me/Desktop")), trees, folders),
              FolderProblem::Protected);
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("Users/me/Desktop/Hades")), trees, folders),
              FolderProblem::None);
    // A sibling whose name merely starts the same is not under the tree.
    touch(QStringLiteral("WindowsGames/Hades/Hades.exe"));
    EXPECT_EQ(checkGameFolder(path(QStringLiteral("WindowsGames/Hades")), trees, folders),
              FolderProblem::None);
}

// --- judgeGameFolder -------------------------------------------------------

TEST_F(GameFolderRulesTest, VerdictBindsTheRootTheGameWasInstalledInto)
{
    touch(QStringLiteral("lib/ELDEN RING/Game/eldenring.exe"));
    touch(QStringLiteral("lib/Hades/Hades.exe"));
    const QStringList none;

    const FolderVerdict v = judgeGameFolder(path(QStringLiteral("lib/ELDEN RING/Game")), none, none);
    EXPECT_EQ(v.problem, FolderProblem::None);
    EXPECT_EQ(v.root, path(QStringLiteral("lib/ELDEN RING")));
}

TEST_F(GameFolderRulesTest, VerdictRefusesTheFolderGamesAreKeptIn)
{
    touch(QStringLiteral("Oyunlar/ELDEN RING/Game/eldenring.exe"));
    touch(QStringLiteral("Oyunlar/Hades/Hades.exe"));
    const QStringList none;

    EXPECT_EQ(judgeGameFolder(path(QStringLiteral("Oyunlar")), none, none).problem,
              FolderProblem::Collection);
    // Same layout under a name that says nothing: two games side by side.
    touch(QStringLiteral("stuff/A/a.exe"));
    touch(QStringLiteral("stuff/B/b.exe"));
    EXPECT_EQ(judgeGameFolder(path(QStringLiteral("stuff")), none, none).problem,
              FolderProblem::Collection);
}

TEST_F(GameFolderRulesTest, VerdictDoesNotClimbIntoAProtectedFolder)
{
    // A "bin" folder straight in the user's home: climbing out of it would bind
    // the game to the home folder itself.
    touch(QStringLiteral("Users/me/bin/tool.exe"));
    const QStringList folders{path(QStringLiteral("Users/me"))};

    EXPECT_EQ(judgeGameFolder(path(QStringLiteral("Users/me/bin")), {}, folders).problem,
              FolderProblem::Protected);
}

// --- pickRoot: the root a specific patch needs -------------------------------

TEST_F(GameFolderRulesTest, PickAnywhereInsideAnUnrealGameLandsOnItsRoot)
{
    touch(QStringLiteral("common/Lies of P/LiesofP/Binaries/Win64/LiesofP-Win64-Shipping.exe"));
    mkdir(QStringLiteral("common/Lies of P/LiesofP/Content/Paks"));
    mkdir(QStringLiteral("common/Lies of P/Engine"));
    touch(QStringLiteral("common/Lies of P/LiesofP.exe"));
    Expectations e;
    e.exeNames = {QStringLiteral("liesofp.exe")};
    e.targetDirs = {QStringLiteral("LiesofP/Content/Paks")};
    const QString root = path(QStringLiteral("common/Lies of P"));

    for (const char* pick : {"common/Lies of P/LiesofP/Binaries/Win64",
                             "common/Lies of P/LiesofP/Content",
                             "common/Lies of P/LiesofP",
                             "common/Lies of P"}) {
        EXPECT_EQ(pickRoot(path(QString::fromLatin1(pick)), QStringLiteral("Lies of P"), e).root, root)
            << pick;
    }
    // The exe itself, picked as a file.
    EXPECT_EQ(pickRoot(path(QStringLiteral("common/Lies of P/LiesofP.exe")),
                       QStringLiteral("Lies of P"), e).root, root);
}

TEST_F(GameFolderRulesTest, ALibraryPickFindsTheGameInside)
{
    touch(QStringLiteral("common/ELDEN RING/Game/eldenring.exe"));
    touch(QStringLiteral("common/ELDEN RING/Game/regulation.bin"));
    touch(QStringLiteral("common/Hades/Hades.exe"));
    Expectations e;
    e.exeNames = {QStringLiteral("eldenring.exe")};
    e.targetFiles = {QStringLiteral("Game/regulation.bin")};

    const RootEvidence r = pickRoot(path(QStringLiteral("common")), QStringLiteral("ELDEN RING"), e);
    EXPECT_EQ(r.root, path(QStringLiteral("common/ELDEN RING")));
    EXPECT_EQ(r.exe, QStringLiteral("Game/eldenring.exe"));
    EXPECT_EQ(r.filesFound, 1);
}

TEST_F(GameFolderRulesTest, TheRecipeOutvotesTheLayoutGuess)
{
    // A folder named "Game" with an exe and nothing runnable above it reads as
    // FromSoftware's layout — but this patch writes into "Data/", which only
    // exists one level down. The patch decides.
    touch(QStringLiteral("lib/Odd/Game/odd.exe"));
    mkdir(QStringLiteral("lib/Odd/Game/Data"));
    Expectations e;
    e.exeNames = {QStringLiteral("odd.exe")};
    e.targetDirs = {QStringLiteral("Data")};

    EXPECT_EQ(pickRoot(path(QStringLiteral("lib/Odd/Game")), QStringLiteral("Odd"), e).root,
              path(QStringLiteral("lib/Odd/Game")));
}

TEST_F(GameFolderRulesTest, WithNothingToWeighTheLayoutGuessStands)
{
    touch(QStringLiteral("lib/ELDEN RING/Game/eldenring.exe"));
    const RootEvidence r = pickRoot(path(QStringLiteral("lib/ELDEN RING/Game")),
                                    QStringLiteral("ELDEN RING"), Expectations{});
    EXPECT_EQ(r.root, path(QStringLiteral("lib/ELDEN RING")));
    EXPECT_TRUE(r.exe.isEmpty());
}

TEST_F(GameFolderRulesTest, AFolderWithNoExeClimbsToTheOneWithIt)
{
    // The user opened a data folder of another game entirely: the root found is
    // that game's, so the caller can say whose it is instead of "no exe here".
    touch(QStringLiteral("Games/Death Stranding 2/DS2.exe"));
    mkdir(QStringLiteral("Games/Death Stranding 2/LocalCacheWinGame"));
    Expectations e;
    e.exeNames = {QStringLiteral("someothergame.exe")};

    EXPECT_EQ(pickRoot(path(QStringLiteral("Games/Death Stranding 2/LocalCacheWinGame")),
                       QStringLiteral("Some Other Game"), e).root,
              path(QStringLiteral("Games/Death Stranding 2")));
}

TEST_F(GameFolderRulesTest, NoCandidateWhenThePickHoldsOnlyOtherGames)
{
    touch(QStringLiteral("Oyunlar/Hades/Hades.exe"));
    touch(QStringLiteral("Oyunlar/Celeste/Celeste.exe"));
    Expectations e;
    e.exeNames = {QStringLiteral("eldenring.exe")};

    const RootEvidence r = pickRoot(path(QStringLiteral("Oyunlar")), QStringLiteral("ELDEN RING"), e);
    EXPECT_TRUE(r.root.isEmpty());
}

TEST_F(GameFolderRulesTest, AFolderInsideAFolderOfItsOwnNameIsTheGame)
{
    // A repack unpacked into "<Name>/<Name>": the outer folder holds a readme
    // and an earlier patch, the game is the inner one.
    touch(QStringLiteral("Games/Red Dead Redemption 2/Red Dead Redemption 2/RDR2.exe"));
    touch(QStringLiteral("Games/Red Dead Redemption 2/dinput8.dll"));
    const QString inner = path(QStringLiteral("Games/Red Dead Redemption 2/Red Dead Redemption 2"));

    EXPECT_EQ(resolveGameRoot(path(QStringLiteral("Games/Red Dead Redemption 2"))), inner);
    Expectations e;
    e.exeNames = {QStringLiteral("rdr2.exe")};
    EXPECT_EQ(pickRoot(path(QStringLiteral("Games/Red Dead Redemption 2")),
                       QStringLiteral("Red Dead Redemption 2"), e).root, inner);
}
