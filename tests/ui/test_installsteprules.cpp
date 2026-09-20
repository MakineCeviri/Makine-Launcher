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

// ===== An empty recipe under a copy-shaped type =============================
//
// The catalogue API fills every recipe-less package in with
// {"type":"script","steps":[]}; the published CDN entry for the same package
// has no installMethod at all. 67 of 237 packages differ that way (measured
// 2026-09-19) and every one of them was refused at install with "Bu yama
// otomatik kurulamiyor (kurulum yontemi: script)".

TEST(InstallStepRules, ScriptWithNoStepsIsJustAnOverlay)
{
    // The regression: Watch Dogs, Thief, DOOM (2016), Skyrim Special Edition,
    // Control, Cuphead, A Plague Tale — all of them arrive as bare "script".
    EXPECT_TRUE(makine::steprules::emptyRecipeIsPlainOverlay(QStringLiteral("script")));
}

TEST(InstallStepRules, OtherCopyShapedTypesAgree)
{
    for (const auto& t : {"copy", "copyFile", "copyDir", "overlay", "direct", "file-replace"})
        EXPECT_TRUE(makine::steprules::emptyRecipeIsPlainOverlay(QString::fromLatin1(t))) << t;
}

TEST(InstallStepRules, TypesThatNameAProcessStillRefuse)
{
    // These have no recipe either, and that is exactly the point: something
    // outside the launcher has to happen. Overlay-copying their payload and
    // reporting success is the "kuruldu ama calismiyor" lie the gate exists for.
    for (const auto& t : {"external", "installer", "workshop", "paradox-mod",
                          "d2r_mod", "forge_inject", "modengine", "unityPatch",
                          "userPath", "vpatch"})
        EXPECT_FALSE(makine::steprules::emptyRecipeIsPlainOverlay(QString::fromLatin1(t))) << t;
}

TEST(InstallStepRules, AnUnknownFutureTypeRefuses)
{
    // A method nobody taught the launcher must not be guessed into an overlay.
    EXPECT_FALSE(makine::steprules::emptyRecipeIsPlainOverlay(QStringLiteral("quantum-inject")));
    EXPECT_FALSE(makine::steprules::emptyRecipeIsPlainOverlay(QStringLiteral("")));
}

// ===== Variant folders vs. overlay content =================================
//
// api/v2/games/<id> drops variantType/variants for all 15 variant packages in
// the catalogue (verified 2026-09-20), so pkg.variants arrives empty and
// nothing asks the user to choose. Without this guard the empty-recipe overlay
// above copies EVERY variant tree into the game root at once — Tormented Souls
// would land as <game>/v0.88.0/BepInEx/…, reporting success and loading
// nothing. Names below are the catalogue's real variant strings and the real
// top-level directories of the packages they are contrasted with.

TEST(InstallStepRules, EveryVariantStringInTheCatalogueIsRecognised) {
    using makine::steprules::looksLikeVariantFolderName;
    for (const auto* v : {"1.5.78", "1.5.80",              // Hollow Knight
                          "1.00", "1.01", "1.04", "1.05",  // AC Syndicate
                          "1.5.6",                          // AC Odyssey
                          "v0.88.0", "v0.94.0",             // Tormented Souls
                          "1.202.0.0", "1.312.0.0",         // Spider-Man 2
                          "1.0", "1.3", "1.3.3", "1.4.1",   // Indiana Jones
                          "1.0.6", "1.0.8", "1.0.8 Steam",  // AC Mirage
                          "Steam", "Gamepass"})             // platform packages
        EXPECT_TRUE(looksLikeVariantFolderName(QString::fromLatin1(v))) << v;
}

TEST(InstallStepRules, OverlayPackageRootsAreNotVariants) {
    using makine::steprules::looksLikeVariantFolderName;
    // Real top-level directories of packages that must keep overlaying.
    for (const auto* d : {"base", "Mods", "font", "menu", "msg", "mods",
                          "data_win32", "dropzone", "Config", "Localization",
                          "Text", "TempleResources", "DefEd", "Data",
                          "PenDriverPro", "engus", "fn", "eldenring-mods"})
        EXPECT_FALSE(looksLikeVariantFolderName(QString::fromLatin1(d))) << d;
}

TEST(InstallStepRules, ABareNumberIsNotAVersion) {
    using makine::steprules::looksLikeVariantFolderName;
    // A year or a numbered chapter directory must not read as a variant.
    EXPECT_FALSE(looksLikeVariantFolderName(QStringLiteral("2015")));
    EXPECT_FALSE(looksLikeVariantFolderName(QStringLiteral("01")));
    EXPECT_FALSE(looksLikeVariantFolderName(QString()));
}

TEST(InstallStepRules, TormentedSoulsRootIsVariantFoldered) {
    using makine::steprules::rootLooksVariantFoldered;
    // Measured by decrypting the published package: 69 files under two roots.
    EXPECT_TRUE(rootLooksVariantFoldered({QStringLiteral("v0.88.0"),
                                          QStringLiteral("v0.94.0")}));
    // Indiana Jones ships six.
    EXPECT_TRUE(rootLooksVariantFoldered({QStringLiteral("1.0"), QStringLiteral("1.3"),
                                          QStringLiteral("1.3.3"), QStringLiteral("1.4"),
                                          QStringLiteral("1.4.1"), QStringLiteral("1.5")}));
    EXPECT_TRUE(rootLooksVariantFoldered({QStringLiteral("Steam"),
                                          QStringLiteral("Gamepass")}));
}

TEST(InstallStepRules, ComplementaryRootsStillOverlay) {
    using makine::steprules::rootLooksVariantFoldered;
    // DOOM (2016) and Dark Souls: Remastered, also measured from the packages.
    EXPECT_FALSE(rootLooksVariantFoldered({QStringLiteral("base"), QStringLiteral("Mods")}));
    EXPECT_FALSE(rootLooksVariantFoldered({QStringLiteral("font"), QStringLiteral("menu"),
                                           QStringLiteral("msg")}));
    // One folder is never a choice between alternatives.
    EXPECT_FALSE(rootLooksVariantFoldered({QStringLiteral("1.00")}));
    EXPECT_FALSE(rootLooksVariantFoldered({}));
}

TEST(InstallStepRules, OneStrayVersionFolderDoesNotDisableTheOverlay) {
    using makine::steprules::rootLooksVariantFoldered;
    // Two variant-shaped entries are required, so a lone "1.0" beside real
    // content keeps the package installable.
    EXPECT_FALSE(rootLooksVariantFoldered({QStringLiteral("base"), QStringLiteral("1.0")}));
}

// ===== A tool the recipe hands to the user ==================================
//
// Elden Ring's recipe copies engus/ and fn/ into the game, runs ERING_TR.exe,
// then copies that same ERING_TR.exe to the desktop as "Elden Ring.exe". The
// binary is a compiled AutoIt script: subsystem GUI, requireAdministrator, and
// a GUIGetMsg event loop — it opens a window and waits for a person. Run
// unattended it cannot finish, and the three ways it fails all reached Sentry
// as one "1 adımda hata oluştu": 915 events, 160 users, 14 days.

TEST(InstallStepRules, TheEldenRingShape) {
    using makine::steprules::runStepIsUserLaunchedTool;
    const QStringList desktop = { QStringLiteral("ERING_TR.exe") };
    EXPECT_TRUE(runStepIsUserLaunchedTool(QStringLiteral("ERING_TR.exe"), desktop));
    // Case and directory prefixes differ between the two steps in practice.
    EXPECT_TRUE(runStepIsUserLaunchedTool(QStringLiteral("ering_tr.exe"), desktop));
    EXPECT_TRUE(runStepIsUserLaunchedTool(QStringLiteral("tools/ERING_TR.exe"), desktop));
    EXPECT_TRUE(runStepIsUserLaunchedTool(QStringLiteral("tools\\ERING_TR.exe"), desktop));
    EXPECT_TRUE(runStepIsUserLaunchedTool(QStringLiteral(" ERING_TR.exe "), desktop));
}

TEST(InstallStepRules, RealCommandLinePatchersKeepRunning) {
    using makine::steprules::runStepIsUserLaunchedTool;
    // The catalogue's other four run steps, measured 2026-09-20. None of these
    // executables is installed for the user, so none may be skipped.
    const QStringList none;
    EXPECT_FALSE(runStepIsUserLaunchedTool(QStringLiteral("tools/moesow.exe"), none));
    EXPECT_FALSE(runStepIsUserLaunchedTool(QStringLiteral("patch.exe"), none));
    // A desktop copy of something else must not disarm an unrelated run step.
    const QStringList other = { QStringLiteral("Baslat.lnk"), QStringLiteral("readme.txt") };
    EXPECT_FALSE(runStepIsUserLaunchedTool(QStringLiteral("patch.exe"), other));
    EXPECT_FALSE(runStepIsUserLaunchedTool(QString(), other));
}
