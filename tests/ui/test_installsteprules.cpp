// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// Rules that decide whether we touch a user's game directory.
//
// Every fixture below is a VERBATIM recipe from the live CDN
// (cdn.makineceviri.org/assets/packages/<appId>.json, read 2026-07-28), because
// the bug being guarded against is precisely a disagreement between what the
// catalog ships and what the executor can run. Synthetic fixtures would have
// hidden it.
//
// The negative cases matter as much as the positive ones: a guard that refuses
// working packages would break 56 recipes that install fine today.

#include <gtest/gtest.h>
#include "installsteprules.h"

using namespace makine::steprules;

namespace {

// Convenience: a step is executable when its action is known AND no required
// field is empty. Mirrors unexecutableSteps() in localpackagemanager.cpp.
bool executable(const QString& action, const QString& src = {},
                const QString& dest = {}, const QString& exe = {},
                const QString& language = {})
{
    return isKnownAction(action)
        && missingField(action, src, dest, exe, language).isEmpty();
}

} // namespace

// ── Recipes that MUST still run (regression guard) ───────────────────────────

TEST(StepRules, EldenRingOptionRecipeStaysExecutable)
{
    // 1245620, option "patch" — the launcher's most-used scripted install.
    EXPECT_TRUE(executable("copyDir", "engus", "Game/msg/engus"));
    EXPECT_TRUE(executable("copyDir", "fn", "Game/fn"));
    EXPECT_TRUE(executable("run", {}, {}, "ERING_TR.exe"));
    EXPECT_TRUE(executable("copyToDesktop", "ERING_TR.exe", "Elden Ring.exe"));
    // combinedSteps "dubbing+patch"
    EXPECT_TRUE(executable("rename", "eldenring.tr.exe", "eldenring.exe"));
}

TEST(StepRules, HollowKnightAndAlanWakeRecipesStayExecutable)
{
    // 367520
    EXPECT_TRUE(executable("copy", "hollow_knight_Data/resources.assets",
                                   "hollow_knight_Data/resources.assets"));
    // 3611110
    EXPECT_TRUE(executable("copyFile", "data_pack2/pc/base-en-000.rmdblob",
                                       "data_pack2/pc/base-en-000.rmdblob"));
    EXPECT_TRUE(executable("copyFile", "data_pack2/pc/base-en.rmdtoc",
                                       "data_pack2/pc/base-en.rmdtoc"));
}

TEST(StepRules, SingleFieldActionsStayExecutable)
{
    EXPECT_TRUE(executable("installFont", "fonts"));
    EXPECT_TRUE(executable("delete", {}, "readme.txt"));
    EXPECT_TRUE(executable("setSteamLanguage", {}, {}, {}, "turkish"));
}

// ── Recipes that MUST be refused before the game is touched ──────────────────

TEST(StepRules, AcThreeRemasteredRecipeIsRefused)
{
    // 911400 — "run" steps carry "cmd", never parsed into exe; "backup" has no
    // executor at all; "rename" carries "pattern"/"to", never parsed into
    // src/dest. All six steps are dead, yet every action name but "backup" is
    // known — this is the case an action-name-only check lets through.
    EXPECT_EQ(missingField("run", {}, {}, /*exe=*/{}, {}), QStringLiteral("exe"));
    EXPECT_FALSE(isKnownAction("backup"));
    EXPECT_EQ(missingField("rename", /*src=*/{}, /*dest=*/{}, {}, {}),
              QStringLiteral("src"));
}

TEST(StepRules, FahrenheitVpatchIsRefused)
{
    // 312840 — action "vpatch" (keys "patch"/"target") has no executor.
    EXPECT_FALSE(isKnownAction("vpatch"));
}

TEST(StepRules, ShadowOfWarRunWithoutExeIsRefused)
{
    // 356190 — "run" carrying only "cmd".
    EXPECT_TRUE(isKnownAction("run"));
    EXPECT_EQ(missingField("run", {}, {}, {}, {}), QStringLiteral("exe"));
}

TEST(StepRules, EachActionReportsItsOwnMissingField)
{
    EXPECT_EQ(missingField("copy", {}, "d"), QStringLiteral("src"));
    EXPECT_EQ(missingField("copy", "s", {}), QStringLiteral("dest"));
    EXPECT_EQ(missingField("copyDir", {}, "d"), QStringLiteral("src"));
    EXPECT_EQ(missingField("delete", "s", {}), QStringLiteral("dest"));
    EXPECT_EQ(missingField("installFont", {}, "d"), QStringLiteral("src"));
    EXPECT_EQ(missingField("setSteamLanguage", "s", "d", "e", {}),
              QStringLiteral("language"));
    // Unknown actions are not this function's job — it must not invent a field.
    EXPECT_TRUE(missingField("vpatch", {}, {}, {}, {}).isEmpty());
}

// ── Variant folder matching ──────────────────────────────────────────────────

TEST(StepRules, HollowKnightVariantFoldersResolve)
{
    // Declared variants ["1.5.78","1.5.80"]; shipped folders
    // "v1.5.78.11833" and "1.5.80". Exact matching resolved only one of them,
    // so the recipe fell back to the package root, found the same relative path
    // under BOTH folders and refused as ambiguous — 48 events / 6 users.
    EXPECT_TRUE(variantFolderMatches("1.5.78", "v1.5.78.11833"));
    EXPECT_TRUE(variantFolderMatches("1.5.80", "1.5.80"));

    // Cross-matching would copy the wrong game version's assets.
    EXPECT_FALSE(variantFolderMatches("1.5.78", "1.5.80"));
    EXPECT_FALSE(variantFolderMatches("1.5.80", "v1.5.78.11833"));
}

TEST(StepRules, VariantMatchStopsAtComponentBoundary)
{
    // "1.5.8" must not swallow "1.5.80" — a prefix may only extend at a dot.
    EXPECT_FALSE(variantFolderMatches("1.5.8", "1.5.80"));
    EXPECT_TRUE(variantFolderMatches("1.5.8", "1.5.8.4321"));
    EXPECT_TRUE(variantFolderMatches("1.5.8", "v1.5.8"));
}

TEST(StepRules, VariantMatchRejectsEmptyInput)
{
    EXPECT_FALSE(variantFolderMatches({}, "1.5.80"));
    EXPECT_FALSE(variantFolderMatches("1.5.80", {}));
}
