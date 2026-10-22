// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// A patch's own scripts, pointed at the game folder the launcher found.
// The Heavy Rain fixtures are the translator's files verbatim (HeavyRainTool,
// 2026-10-13): CRLF, "chcp 65001" after the folder line.

#include <gtest/gtest.h>
#include "runstage.h"

using namespace makine::runstage;

namespace {

const QByteArray kHeavyRainLoad =
    "@echo off\r\n"
    "rem Oyun klasorunu gerekirse degistirin:\r\n"
    "set OYUN=D:\\SteamLibrary\\steamapps\\common\\HEAVY RAIN\r\n"
    "chcp 65001 >nul\r\n"
    "cd /d \"%~dp0\"\r\n"
    "python \"%~dp0hr_ceviri.py\" yukle --oyun \"%OYUN%\"\r\n"
    "if errorlevel 1 echo. & echo Hata olustu. Python kurulu mu? (python --version)\r\n"
    "pause\r\n";

} // namespace

TEST(RunStage, HardCodedGameFolderReadsTheLaunchersFolder)
{
    const Rewrite r = rewriteScript(kHeavyRainLoad, QStringLiteral("Heavy Rain"),
                                    QStringLiteral("C:/Users/Administrator/Desktop/heavy rain"));
    EXPECT_EQ(r.variables, QStringList{QStringLiteral("OYUN")});
    EXPECT_EQ(r.folders, QStringList{QStringLiteral("D:\\SteamLibrary\\steamapps\\common\\HEAVY RAIN")});
    // Only that line changed; every other byte is the translator's.
    QByteArray want = kHeavyRainLoad;
    want.replace("set OYUN=D:\\SteamLibrary\\steamapps\\common\\HEAVY RAIN",
                 "set \"OYUN=%MAKINE_OYUN%\"");
    EXPECT_EQ(r.text, want);
}

TEST(RunStage, QuotedAndSpacedFormsToo)
{
    // set "X=…", set X= "…" (the shape a person writes by hand), set X="…", a
    // trailing "\". A value that carried quotes keeps them: the script uses
    // %X% unquoted, and the player's folder may have spaces.
    const QList<QPair<QByteArray, QByteArray>> forms = {
        {"set \"OYUN=D:\\Games\\HEAVY RAIN\"", "set \"OYUN=%MAKINE_OYUN%\""},
        {"set OYUN= \"D:\\Games\\HEAVY RAIN\"", "set OYUN=\"%MAKINE_OYUN%\""},
        {"set OYUN=\"D:\\Games\\HEAVY RAIN\"", "set OYUN=\"%MAKINE_OYUN%\""},
        {"@set GAME_DIR=D:\\Games\\HEAVY RAIN\\", "@set \"GAME_DIR=%MAKINE_OYUN%\""},
    };
    for (const auto& [line, want] : forms) {
        const Rewrite r = rewriteScript(line + "\r\n", QStringLiteral("Heavy Rain"));
        ASSERT_EQ(r.variables.size(), 1) << line.constData();
        EXPECT_EQ(r.text, want + "\r\n") << line.constData();
    }
}

TEST(RunStage, FolderWrittenOutElsewhereIsReplacedToo)
{
    const QByteArray bat =
        "cd /d \"D:\\Games\\Assassin's Creed Shadows\"\r\n"
        "copy /y \"%~dp0tr.forge\" \"D:\\Games\\Assassin's Creed Shadows\\DataPC_patch.forge\"\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Assassin's Creed Shadows"));
    EXPECT_EQ(r.text,
              QByteArray("cd /d \"%MAKINE_OYUN%\"\r\n"
                         "copy /y \"%~dp0tr.forge\" \"%MAKINE_OYUN%\\DataPC_patch.forge\"\r\n"));
    EXPECT_EQ(r.folders.size(), 1);
}

TEST(RunStage, StoreLayoutNamesTheFolderWhateverItIsCalled)
{
    // For a variable that says it holds the game folder the name does not
    // have to match: steamapps\common\<x> is a game.
    EXPECT_EQ(gameFolderDepth(QStringLiteral("D:\\SteamLibrary\\steamapps\\common\\HR\\Resources"),
                              QStringLiteral("Heavy Rain"), true), 5);
    // Anywhere else it does: the store keeps other things there too.
    EXPECT_EQ(gameFolderDepth(QStringLiteral("D:\\SteamLibrary\\steamapps\\common\\HR\\Resources"),
                              QStringLiteral("Heavy Rain")), 0);
    EXPECT_EQ(gameFolderDepth(QStringLiteral("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Steamworks Shared\\_CommonRedist"),
                              QStringLiteral("Heavy Rain")), 0);
    EXPECT_EQ(gameFolderDepth(QStringLiteral("C:\\Program Files\\Epic Games\\HeavyRain"),
                              QStringLiteral("Heavy Rain")), 4);
    EXPECT_EQ(gameFolderDepth(QStringLiteral("E:\\Oyunlar\\Heavy Rain\\Resources"),
                              QStringLiteral("Heavy Rain")), 3);
}

TEST(RunStage, OtherFoldersStayAsTheyAre)
{
    // A tool's own folder, a variable that is no game folder, an expanded path,
    // "set /p": nothing here is the game.
    const QByteArray bat =
        "set ARAC=D:\\Araclar\\quickbms\r\n"
        "set CIKTI=%~dp0cikti\r\n"
        "set /p CEVAP=Devam? \r\n"
        "set YOL=C:\\Windows\\System32\r\n"
        "echo D:\\Games\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Heavy Rain"));
    EXPECT_TRUE(r.variables.isEmpty());
    EXPECT_TRUE(r.folders.isEmpty());
    EXPECT_EQ(r.text, bat);
}

TEST(RunStage, StoreFoldersOfOtherThingsStayAsTheyAre)
{
    const QByteArray bat =
        "set REDIST=C:\\Program Files (x86)\\Steam\\steamapps\\common\\Steamworks Shared\\_CommonRedist\r\n"
        "\"C:\\Program Files\\Epic Games\\Launcher\\Portal\\Binaries\\x.exe\"\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Heavy Rain"));
    EXPECT_TRUE(r.folders.isEmpty());
    EXPECT_EQ(r.text, bat);
}

TEST(RunStage, ALineThatIsAlreadyRightIsLeftAlone)
{
    const QByteArray bat = "set OYUN=C:\\Oyunlar\\Heavy Rain\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Heavy Rain"), QStringLiteral("C:/Oyunlar/Heavy Rain"));
    EXPECT_TRUE(r.variables.isEmpty());
    EXPECT_EQ(r.text, bat);
}

TEST(RunStage, NonAsciiBytesSurvive)
{
    // cp857 "ı" (0x8D) and UTF-8 "ş" in a comment: written back as they were.
    QByteArray bat = "rem Oyun klas\x94r\x81 \xc5\x9f\r\nset OYUN=D:\\Games\\HEAVY RAIN\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Heavy Rain"));
    EXPECT_TRUE(r.text.startsWith("rem Oyun klas\x94r\x81 \xc5\x9f\r\n"));
    EXPECT_TRUE(r.text.endsWith("set \"OYUN=%MAKINE_OYUN%\"\r\n"));
}

TEST(RunStage, APlaceholderValueOfAGameVariableIsTheFolderToo)
{
    // The Heavy Rain package on the CDN (2026-10-20): "game.path" was meant to
    // be filled in; the variable named it the game folder all the same.
    const QByteArray bat = "set OYUN=game.path\r\npython x.py --oyun \"%OYUN%\"\r\n";
    const Rewrite r = rewriteScript(bat, QStringLiteral("Heavy Rain"));
    EXPECT_EQ(r.text, QByteArray("set \"OYUN=%MAKINE_OYUN%\"\r\npython x.py --oyun \"%OYUN%\"\r\n"));
    // A name is not a path, and a value from an argument is left to it.
    for (const QByteArray other : {QByteArray("set GAME=HeavyRain\r\n"), QByteArray("set OYUN=%~1\r\n")})
        EXPECT_EQ(rewriteScript(other, QStringLiteral("Heavy Rain")).text, other);
}

TEST(RunStage, AFileHoldingTheFolderGetsTheLaunchersFolder)
{
    const QString found = QStringLiteral("C:/Users/Administrator/Desktop/heavy rain");
    const QString game = QStringLiteral("Heavy Rain");
    EXPECT_EQ(rewritePathFile("D:\\SteamLibrary\\steamapps\\common\\HEAVY RAIN", game, found),
              QByteArray("C:\\Users\\Administrator\\Desktop\\heavy rain"));
    EXPECT_EQ(rewritePathFile("D:\\Games\\HEAVY RAIN\r\n", game, found),
              QByteArray("C:\\Users\\Administrator\\Desktop\\heavy rain\r\n"));
    // Other contents stay as they are.
    EXPECT_TRUE(rewritePathFile("KarakterSiniri=108\r\n", game, found).isEmpty());
    EXPECT_TRUE(rewritePathFile("D:\\Araclar\\quickbms", game, found).isEmpty());
}

TEST(RunStage, NoteFileSetsTheFolderForABatch)
{
    const QByteArray note = noteFile(QStringLiteral("C:/Users/Şükrü/Desktop/heavy rain"),
                                     QStringLiteral("960910"), QStringLiteral("Heavy Rain"));
    const QString text = QString::fromUtf8(note);
    EXPECT_TRUE(text.contains(QStringLiteral("set \"OYUN=C:\\Users\\Şükrü\\Desktop\\heavy rain\"\r\n")));
    EXPECT_TRUE(text.contains(QStringLiteral("set \"MAKINE_OYUN_ID=960910\"\r\n")));
}

TEST(RunStage, WindowedProgramFromItsHeader)
{
    // A minimal PE header: MZ, e_lfanew at 0x3C, "PE\0\0", subsystem at +92.
    QByteArray pe(0x200, '\0');
    pe[0] = 'M'; pe[1] = 'Z';
    pe[0x3C] = char(0x80);
    pe.replace(0x80, 4, QByteArray("PE\0\0", 4));
    pe[0x80 + 24 + 68] = 2;   // GUI
    EXPECT_TRUE(isWindowedProgram(pe));
    pe[0x80 + 24 + 68] = 3;   // console
    EXPECT_FALSE(isWindowedProgram(pe));
    EXPECT_FALSE(isWindowedProgram(QByteArray("@echo off\r\n")));
}
