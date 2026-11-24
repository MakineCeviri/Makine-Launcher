// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file test_patchcompat.cpp
 * @brief Patch alternatives and game-version compatibility: the decisions the
 *        game page shows (patchcompat.h) and assets/game-builds.json parsing
 */

#include <gtest/gtest.h>
#include "patchcompat.h"

using namespace makine::patchcompat;

// ---- The warning --------------------------------------------------------------

TEST(PatchCompat, InstalledGameNewerThanThePatchWarns)
{
    const Compat c = decide(16412345, 16550001, 0);
    EXPECT_TRUE(c.warn);
    EXPECT_EQ(c.source, BuildSource::Installed);
    EXPECT_EQ(c.currentBuild, 16550001);
}

TEST(PatchCompat, InstalledBuildWinsOverSteamsLatest)
{
    // The player's copy is the patch's build; Steam has moved on, but the
    // player has not updated: no warning, the installed copy is what counts.
    const Compat c = decide(16412345, 16412345, 16550001);
    EXPECT_FALSE(c.warn);
    EXPECT_EQ(c.source, BuildSource::Installed);
    EXPECT_EQ(c.currentBuild, 16412345);
}

TEST(PatchCompat, LatestBuildIsUsedWhenTheInstalledOneIsUnknown)
{
    const Compat c = decide(16412345, 0, 16550001);
    EXPECT_TRUE(c.warn);
    EXPECT_EQ(c.source, BuildSource::Latest);
    EXPECT_EQ(sourceName(c.source), QStringLiteral("latest"));
}

TEST(PatchCompat, UnknownPatchBuildNeverWarns)
{
    EXPECT_FALSE(decide(0, 99999999, 99999999).warn);
    EXPECT_FALSE(decide(-1, 99999999, 0).warn);
}

TEST(PatchCompat, NothingKnownAboutTheGameNeverWarns)
{
    const Compat c = decide(16412345, 0, 0);
    EXPECT_FALSE(c.warn);
    EXPECT_EQ(c.source, BuildSource::None);
    EXPECT_EQ(c.currentBuild, 0);
    EXPECT_TRUE(sourceName(c.source).isEmpty());
}

TEST(PatchCompat, OlderGameThanThePatchDoesNotWarn)
{
    // A game rolled back to a beta branch, or a patch made on a newer build
    EXPECT_FALSE(decide(16550001, 16412345, 0).warn);
}

// ---- What a patch option offers (installed record's pkg) ----------------------

TEST(PatchAction, NothingInstalledOffersInstall)
{
    EXPECT_EQ(actionFor(QString(), false, QString(), false), Action::Install);
    EXPECT_EQ(actionFor("dublaj", false, QString(), false), Action::Install);
}

TEST(PatchAction, OldRecordWithoutPkgIsTheDefault)
{
    // installed_packages.json from before alternatives: pkg read as ""
    EXPECT_EQ(actionFor(QString(), true, QString(), false), Action::Installed);
    EXPECT_EQ(actionFor(QString(), true, QString(), true), Action::Update);
    EXPECT_EQ(actionFor("dublaj", true, QString(), false), Action::Switch);
}

TEST(PatchAction, InstalledAlternative)
{
    EXPECT_EQ(actionFor("dublaj", true, "dublaj", false), Action::Installed);
    EXPECT_EQ(actionFor("dublaj", true, "dublaj", true), Action::Update);
    EXPECT_EQ(actionFor(QString(), true, "dublaj", false), Action::Switch);
    EXPECT_EQ(actionFor("eski", true, "dublaj", true), Action::Switch);
    EXPECT_EQ(actionName(Action::Switch), QStringLiteral("switch"));
}

TEST(PatchAction, DetailKeyNamesTheCacheFile)
{
    EXPECT_EQ(detailKey("1245620", QString()), QStringLiteral("1245620"));
    EXPECT_EQ(detailKey("1245620", "dublaj"), QStringLiteral("1245620--dublaj"));
}

// ---- assets/game-builds.json ------------------------------------------------

TEST(GameBuilds, ParsesTheContractShape)
{
    const auto b = parseGameBuilds(R"({"generatedAt":"2026-11-24T18:00:00Z",
        "builds":{"1245620":{"steamBuild":16550001,"date":"2026-11-20"},
                  "292030":{"steamBuild":123}}})");
    ASSERT_EQ(b.size(), 2);
    EXPECT_EQ(b.value("1245620").steamBuild, 16550001);
    EXPECT_EQ(b.value("1245620").date, QStringLiteral("2026-11-20"));
    EXPECT_EQ(b.value("292030").steamBuild, 123);
    EXPECT_TRUE(b.value("292030").date.isEmpty());
}

TEST(GameBuilds, MalformedPartsAreLeftOut)
{
    const auto b = parseGameBuilds(R"({"builds":{
        "1":{"steamBuild":"16550001"}, "2":42, "3":{"date":"x"},
        "4":{"steamBuild":-5}, "5":{"steamBuild":0}, "6":{"steamBuild":7,"date":5}}})");
    ASSERT_EQ(b.size(), 1);
    EXPECT_EQ(b.value("6").steamBuild, 7);
    EXPECT_TRUE(b.value("6").date.isEmpty());
}

TEST(GameBuilds, BrokenFilesAreEmpty)
{
    EXPECT_TRUE(parseGameBuilds("").isEmpty());
    EXPECT_TRUE(parseGameBuilds("<html>404</html>").isEmpty());
    EXPECT_TRUE(parseGameBuilds("[]").isEmpty());
    EXPECT_TRUE(parseGameBuilds(R"({"builds":[]})").isEmpty());
    EXPECT_TRUE(parseGameBuilds(R"({"generatedAt":"x"})").isEmpty());
}
