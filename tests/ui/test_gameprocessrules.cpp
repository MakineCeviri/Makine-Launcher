// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "gameprocessrules.h"

#include <gtest/gtest.h>

using namespace gameprocessrules;

TEST(GameProcessRules, OtherGameIdIsStableAndLowerCase)
{
    EXPECT_EQ(otherGameId("Hades2.exe"), QStringLiteral("exe:hades2.exe"));
    EXPECT_EQ(otherGameId("hades2.exe"), otherGameId("HADES2.EXE"));
}

TEST(GameProcessRules, FullscreenGpuProcessIsAGame)
{
    EXPECT_TRUE(looksLikeForegroundGame("Hades2.exe", true, true));
}

TEST(GameProcessRules, NeedsBothFullscreenAndGpu)
{
    EXPECT_FALSE(looksLikeForegroundGame("Hades2.exe", false, true));
    EXPECT_FALSE(looksLikeForegroundGame("Hades2.exe", true, false));
}

TEST(GameProcessRules, BrowsersChatAndMakineAreNeverGames)
{
    for (const char* exe : {"chrome.exe", "MSEdge.exe", "Discord.exe", "Makine-Launcher.exe",
                            "makine-plugin-host.exe", "explorer.exe", "RemotePlay.exe", "obs64.exe"})
        EXPECT_FALSE(looksLikeForegroundGame(exe, true, true)) << exe;
}

TEST(GameProcessRules, ScanCadence)
{
    EXPECT_EQ(scanIntervalMs(AppState::Active, false), 30000);
    EXPECT_EQ(scanIntervalMs(AppState::Inactive, false), 120000);
    EXPECT_EQ(scanIntervalMs(AppState::Hidden, false), -1);
    // A plugin waiting for games: fast everywhere, the tray included
    EXPECT_EQ(scanIntervalMs(AppState::Active, true), 3000);
    EXPECT_EQ(scanIntervalMs(AppState::Inactive, true), 3000);
    EXPECT_EQ(scanIntervalMs(AppState::Hidden, true), 3000);
}

TEST(GameProcessRules, OnlyExecutables)
{
    EXPECT_FALSE(looksLikeForegroundGame("game", true, true));
}
