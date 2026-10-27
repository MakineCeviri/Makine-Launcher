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

TEST(StepRules, InPlaceActionsAreTheOnesThatEditGameFiles)
{
    // An update of a recipe with one of these restores the originals first.
    for (const char* a : {"patchBytes", "forgeReimport", "xdelta", "vpatch"})
        EXPECT_TRUE(isInPlaceAction(QString::fromLatin1(a))) << a;
    for (const char* a : {"copy", "copyFile", "copyDir", "delete", "installFont", "run",
                          "copyToDesktop", "rename", "setSteamLanguage", ""})
        EXPECT_FALSE(isInPlaceAction(QString::fromLatin1(a))) << a;
}

TEST(StepRules, BackupListsEveryGameFileAStepChangesOrDeletes)
{
    // AC Unity (289650), verbatim: the last step deletes a game archive. It
    // has to be in the backup or uninstall cannot bring it back.
    EXPECT_EQ(backupTarget("delete", {}, "DataPC_patch_01.forge"), "DataPC_patch_01.forge");
    EXPECT_EQ(backupTarget("vpatch", "patches/BigFile_PC_d01.pat", "BigFile_PC.d01"),
              "BigFile_PC.d01");
    EXPECT_EQ(backupTarget("forgeReimport", "DataPC_35_dlc", "dlc_35/DataPC_35_dlc.forge"),
              "dlc_35/DataPC_35_dlc.forge");
    EXPECT_EQ(backupTarget("xdelta", "aco156.aco", "DataPC_patch_01.forge"),
              "DataPC_patch_01.forge");
    // Elden Ring writes a new exe from the original; in place when dest is empty.
    EXPECT_EQ(backupTarget("patchBytes", "Game/eldenring.exe", "Game/start_protected_game.exe"),
              "Game/start_protected_game.exe");
    EXPECT_EQ(backupTarget("patchBytes", "Game/eldenring.exe", {}), "Game/eldenring.exe");
    // Copies are listed from the package itself; the rest touch no game file.
    for (const char* a : {"copy", "copyFile", "copyDir", "installFont", "run",
                          "copyToDesktop", "setSteamLanguage", ""})
        EXPECT_TRUE(backupTarget(QString::fromLatin1(a), "x", "y").isEmpty()) << a;
}

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
    // 911400, as the recipe stood on 2026-09-20: "run" steps carrying "cmd"
    // and never parsed into exe, a "backup" action with no executor, and a
    // "rename" carrying "pattern"/"to". All six steps were dead, yet every
    // action name but "backup" is known — the case an action-name-only check
    // lets through. (That package now applies its delta directly; the shapes
    // below are still what this function has to catch.)
    EXPECT_EQ(missingField("run", {}, {}, /*exe=*/{}, {}), QStringLiteral("exe"));
    EXPECT_FALSE(isKnownAction("backup"));
    EXPECT_EQ(missingField("rename", /*src=*/{}, /*dest=*/{}, {}, {}),
              QStringLiteral("src"));
}

TEST(StepRules, FahrenheitVpatchIsNowExecutable)
{
    // 312840 — action "vpatch" (keys "patch"/"target"). It was refused here
    // for as long as the executor had no such action; vpatchapply.h added one,
    // ported from the NSIS reference applier and verified against that
    // project's own test vectors, so the package installs instead of stopping
    // at the gate.
    EXPECT_TRUE(isKnownAction("vpatch"));
    EXPECT_EQ(missingField("vpatch", "patches/BigFile_PC_d01.pat",
                           "BigFile_PC_d01.big"), QString());
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
    EXPECT_EQ(missingField("vpatch", {}, "BigFile_PC_d01.big"), QStringLiteral("src"));
    EXPECT_EQ(missingField("vpatch", "patches/x.pat", {}), QStringLiteral("dest"));
    // Unknown actions are not this function's job — it must not invent a field.
    EXPECT_TRUE(missingField("nosuchaction", {}, {}, {}, {}).isEmpty());
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

// ===== vpatch ===============================================================
//
// Two catalogue packages apply a VPatch binary delta to a file the game
// already owns. Both used to fail at the pre-flight gate with "'vpatch'
// çalıştırıcıda yok", because the executor had no such action; the recipes
// also name their paths "patch"/"target" rather than src/dest, which the
// catalog parser now maps. A vpatch step missing either path would rewrite
// the wrong file or nothing at all, so both are required.

TEST(InstallStepRules, VpatchIsAnActionTheExecutorDispatchesOn) {
    using makine::steprules::isKnownAction;
    EXPECT_TRUE(isKnownAction(QStringLiteral("vpatch")));
}

TEST(InstallStepRules, VpatchNeedsBothThePatchAndTheTarget) {
    using makine::steprules::missingField;
    // Fahrenheit (312840), as published.
    EXPECT_EQ(missingField(QStringLiteral("vpatch"),
                           QStringLiteral("patches/BigFile_PC_d01.pat"),
                           QStringLiteral("BigFile_PC_d01.big")),
              QString());
    // Assassin's Creed III (911400): one delta file, four targets.
    EXPECT_EQ(missingField(QStringLiteral("vpatch"), QStringLiteral("tr"),
                           QStringLiteral("DataPC_DX11.forge")),
              QString());
    EXPECT_EQ(missingField(QStringLiteral("vpatch"), QString(),
                           QStringLiteral("BigFile_PC_d01.big")),
              QStringLiteral("src"));
    EXPECT_EQ(missingField(QStringLiteral("vpatch"),
                           QStringLiteral("patches/BigFile_PC_d01.pat"), QString()),
              QStringLiteral("dest"));
}

// ===== patchBytes ===========================================================
//
// Elden Ring's exe edit (see bytepatch.h). The pre-flight gate must refuse a
// malformed pair before any earlier step has copied files into the game.

TEST(InstallStepRules, PatchBytesIsAnActionTheExecutorDispatchesOn) {
    EXPECT_TRUE(isKnownAction(QStringLiteral("patchBytes")));
}

TEST(InstallStepRules, PatchBytesNeedsASourceAndUsablePairs) {
    const QList<QPair<QString, QString>> eldenRing = {
        {QStringLiteral("utf16:data0:/font") + QChar(0), QStringLiteral("utf16:dvdroot:/fn") + QChar(0)},
        {QStringLiteral("utf16:data0:/msg") + QChar(0),
         QStringLiteral("utf16:dvdroot:/") + QChar(0) + QChar(0)},
    };
    EXPECT_EQ(missingField(QStringLiteral("patchBytes"), QStringLiteral("Game/eldenring.exe"),
                           QStringLiteral("Game/start_protected_game.exe"), {}, {}, eldenRing),
              QString());
    // In place: dest is optional.
    EXPECT_EQ(missingField(QStringLiteral("patchBytes"), QStringLiteral("Game/eldenring.exe"),
                           {}, {}, {}, eldenRing),
              QString());
    EXPECT_EQ(missingField(QStringLiteral("patchBytes"), {}, {}, {}, {}, eldenRing),
              QStringLiteral("src"));
    EXPECT_EQ(missingField(QStringLiteral("patchBytes"), QStringLiteral("Game/eldenring.exe")),
              QStringLiteral("patches"));
    // A replacement one character short would shift every byte after it.
    EXPECT_EQ(missingField(QStringLiteral("patchBytes"), QStringLiteral("Game/eldenring.exe"),
                           {}, {}, {},
                           {{QStringLiteral("utf16:data0:/msg"), QStringLiteral("utf16:dvdroot:/")}}),
              QStringLiteral("patches"));
}

// ===== forgeReimport / xdelta ==============================================
//
// The forge games (Valhalla, Mirage, Odyssey). Both rewrite an archive the
// game owns, so a step without its target must never reach the executor.

TEST(InstallStepRules, ForgeActionsNeedTheirSourceAndTheirArchive) {
    for (const QString& action : {QStringLiteral("forgeReimport"), QStringLiteral("xdelta")}) {
        EXPECT_TRUE(isKnownAction(action));
        EXPECT_EQ(missingField(action, QStringLiteral("DataPC_35_dlc"),
                               QStringLiteral("dlc_35/DataPC_35_dlc.forge")),
                  QString());
        EXPECT_EQ(missingField(action, {}, QStringLiteral("DataPC_patch_01.forge")),
                  QStringLiteral("src"));
        EXPECT_EQ(missingField(action, QStringLiteral("aco156.aco")), QStringLiteral("dest"));
    }
}

// ===== run step placeholders ================================================
//
// The admin panel's "run the patch's own program" recipe hands the game
// folder to a patch .bat: {"action":"run","exe":"HeavyRain_Turkce_Yama.exe",
// "args":["${gamePath}"],"workDir":"package"}. Qt holds paths with '/', and
// xcopy/copy inside the .bat read "/SteamLibrary" as a switch.

TEST(InstallStepRules, RunArgsGetTheGameAndPackagePathsWithNativeSeparators) {
    const QString game = QStringLiteral("D:/SteamLibrary/steamapps/common/Heavy Rain");
    const QString pkg  = QStringLiteral("C:/Users/x/AppData/Local/MakineCeviri/Makine-Launcher/packages/heavy-rain");
    const QStringList args = resolveRunArgs(
        {QStringLiteral("${gamePath}"), QStringLiteral("/S"),
         QStringLiteral("--from=${packageDir}"), QStringLiteral(R"(${gamePath}\data)")},
        game, pkg);
#ifdef Q_OS_WIN
    EXPECT_EQ(args, QStringList({
        QStringLiteral(R"(D:\SteamLibrary\steamapps\common\Heavy Rain)"),
        QStringLiteral("/S"),   // the recipe's own text is left as written
        QStringLiteral(R"(--from=C:\Users\x\AppData\Local\MakineCeviri\Makine-Launcher\packages\heavy-rain)"),
        QStringLiteral(R"(D:\SteamLibrary\steamapps\common\Heavy Rain\data)")}));
#else
    EXPECT_EQ(args.at(0), game);
#endif
}

// Elden Ring's ER.exe wants the folder Data0.bhd sits in (<root>/Game), the
// forge tools the root. ${gameRoot} is the root under its other name; with
// no exe folder known, ${exeDir} is the root too.
TEST(InstallStepRules, RunArgsGetTheRootAndTheExeFolder) {
    const QStringList args = {QStringLiteral("pack"), QStringLiteral("--game"), QStringLiteral("${exeDir}"),
                              QStringLiteral("--root=${gameRoot}")};
    const QStringList er = resolveRunArgs(args, QStringLiteral("D:/Oyunlar/ELDEN RING"), QStringLiteral("C:/pkg"),
                                          QStringLiteral("D:/Oyunlar/ELDEN RING/Game"));
    EXPECT_EQ(er.at(2), QDir::toNativeSeparators(QStringLiteral("D:/Oyunlar/ELDEN RING/Game")));
    EXPECT_EQ(er.at(3), QStringLiteral("--root=") + QDir::toNativeSeparators(QStringLiteral("D:/Oyunlar/ELDEN RING")));
    const QStringList flat = resolveRunArgs(args, QStringLiteral("D:/Games/ACS"), QStringLiteral("C:/pkg"));
    EXPECT_EQ(flat.at(2), QDir::toNativeSeparators(QStringLiteral("D:/Games/ACS")));
}

TEST(InstallStepRules, RunArgsWithoutPlaceholdersAreUnchanged) {
    const QStringList args = {QStringLiteral("kapat"), QStringLiteral("hotchunk.arch06")};
    EXPECT_EQ(resolveRunArgs(args, QStringLiteral("D:/Game"), QStringLiteral("C:/pkg")), args);
    EXPECT_TRUE(resolveRunArgs({}, QStringLiteral("D:/Game"), QStringLiteral("C:/pkg")).isEmpty());
}

// ===== what an update records as the game's own =============================

TEST(InstallStepRules, UpdateCountsAFileItOverwritesForTheFirstTimeAsTheGames) {
    // v1 replaced data/a.pak; v2 also ships data/b.pak, an original v1 never
    // touched. Recorded as added, uninstall deleted the game's b.pak.
    EXPECT_TRUE(updateOverwritesOriginal(/*existed*/ true, /*replaced*/ false, /*added*/ false));
    // Still the game's: replaced at the first install.
    EXPECT_TRUE(updateOverwritesOriginal(true, true, false));
    // Ours from the first install, or new now: added, and uninstall removes it.
    EXPECT_FALSE(updateOverwritesOriginal(true, false, true));
    EXPECT_FALSE(updateOverwritesOriginal(false, false, false));
}

TEST(InstallStepRules, ReinstallOverThePatchKeepsItsFilesOurs) {
    // An add-only package has no backup, so its update runs as a re-install:
    // every file of the first install is on disk and was counted as the game's.
    QStringList added{"new_in_v2.pak"};
    QStringList replaced{"Content/Paks/tr_P.pak", "Binaries/dinput8.dll"};
    keepEarlierAdded(added, replaced, {"Content/Paks/tr_P.pak"});
    EXPECT_EQ(added, QStringList({"new_in_v2.pak", "Content/Paks/tr_P.pak"}));
    EXPECT_EQ(replaced, QStringList({"Binaries/dinput8.dll"}));   // really the game's

    // No earlier record (a first install): nothing moves.
    QStringList a2{"x"}, r2{"y"};
    keepEarlierAdded(a2, r2, {});
    EXPECT_EQ(a2, QStringList({"x"}));
    EXPECT_EQ(r2, QStringList({"y"}));
}

TEST(InstallStepRules, ReinstallKeepsTheEarlierInPlaceMarkers) {
    // AC Unity (289650): copy, then delete the game's DataPC_patch_01.forge.
    // Run again, the delete finds nothing and writes no marker.
    QStringList installed{"DataPC_patch_01_tr.forge"};
    keepEarlierInPlace(installed, {"DataPC_patch_01_tr.forge", "_vpatch:DataPC_patch_01.forge",
                                   "_steamlang:289650:english"});
    EXPECT_EQ(installed, QStringList({"DataPC_patch_01_tr.forge",
                                      "_vpatch:DataPC_patch_01.forge"}));

    // The step wrote its marker again: not doubled.
    QStringList again{"_vpatch:DataPC_patch_01.forge"};
    keepEarlierInPlace(again, {"_vpatch:DataPC_patch_01.forge"});
    EXPECT_EQ(again.size(), 1);
}

TEST(InstallStepRules, ProgramChangesStayOnRecordOverAReinstall) {
    // ACS.exe run again for an update rewrites the same forge: nothing new to
    // list, and its undo is still owed the files the first run changed.
    QStringList installed{".makine"};
    keepEarlierInPlace(installed, {".makine", "_program:DataPC_boot_patch_02.forge",
                                   "_program:acs_text_yedek/DataPC_boot_patch_02.forge.tablo"});
    EXPECT_EQ(installed, QStringList({".makine", "_program:DataPC_boot_patch_02.forge",
                                      "_program:acs_text_yedek/DataPC_boot_patch_02.forge.tablo"}));
}

// ===== the record a failed install leaves ===================================
//
// The rollback after a failed install restores the backup, then removes what
// the install added — read from the install record. Without one, a half-copied
// overlay stayed in the game and the next attempt backed it up as "original".

TEST(InstallStepRules, FailedFirstInstallRecordsWhatItWrote) {
    // An overlay stopped by a full disk after two files.
    const FileRecord r = mergeWrittenFiles({}, {"dinput8.dll", "lml/tr/strings.rpf"},
                                           {"lml/tr/strings.rpf"});
    EXPECT_EQ(r.installed, QStringList({"dinput8.dll", "lml/tr/strings.rpf"}));
    EXPECT_EQ(r.added, QStringList({"dinput8.dll"}));           // removed by the rollback
    EXPECT_EQ(r.replaced, QStringList({"lml/tr/strings.rpf"})); // left to the restore
}

TEST(InstallStepRules, FailedRecipeKeepsMarkersOutOfTheFileLists) {
    // Elden Ring's option: copyDir, then patchBytes, then the Steam language.
    const FileRecord r = mergeWrittenFiles(
        {}, {"Game/engus/item.msgbnd.dcx", "_vpatch:Game/start_protected_game.exe",
             "_steamlang:1245620:turkish"}, {});
    EXPECT_EQ(r.installed.size(), 3);
    EXPECT_EQ(r.added, QStringList({"Game/engus/item.msgbnd.dcx"}));
    EXPECT_TRUE(r.replaced.isEmpty());
}

TEST(InstallStepRules, FailedReinstallKeepsTheEarlierClassification) {
    // A re-install finds the first install's files on disk and would call them
    // originals; the earlier record knows they are ours.
    const FileRecord old{{"a.pak", "data/b.pak"}, {"a.pak"}, {"data/b.pak"}};
    const FileRecord r = mergeWrittenFiles(old, {"a.pak", "data/b.pak", "c.pak"},
                                           {"a.pak", "data/b.pak"});
    EXPECT_EQ(r.installed, QStringList({"a.pak", "data/b.pak", "c.pak"}));
    EXPECT_EQ(r.added, QStringList({"a.pak", "c.pak"}));
    EXPECT_EQ(r.replaced, QStringList({"data/b.pak"}));
}

TEST(InstallStepRules, FailedInstallThatWroteNothingChangesNoRecord) {
    const FileRecord old{{"a.pak"}, {"a.pak"}, {}};
    const FileRecord r = mergeWrittenFiles(old, {}, {});
    EXPECT_EQ(r.installed, old.installed);
    EXPECT_EQ(r.added, old.added);
    EXPECT_TRUE(r.replaced.isEmpty());
}

TEST(InstallStepRules, FailedUpdateWithoutBackupRemovesOnlyItsOwnFiles) {
    // v1 of an add-only patch (no backup) on record; its update wrote one file
    // of v1 again and one new file before the disk filled.
    const QStringList v1{"Content/Paks/tr_P.pak", "Binaries/dinput8.dll"};
    const FileRecord failed = mergeWrittenFiles({v1, v1, {}},
                                                {"Content/Paks/tr_P.pak", "new_in_v2.pak"}, {});
    EXPECT_EQ(notOnRecord(failed.added, v1), QStringList({"new_in_v2.pak"}));

    // The update never started ("Yama paketi bulunamadı"): nothing of its own.
    EXPECT_TRUE(notOnRecord(v1, v1).isEmpty());
}

TEST(InstallStepRules, UpdateTopsUpTheBackupWithItsFirstOverwritesOnly) {
    // v1 added tr.pak and replaced ui.pak (in the backup); v2 also ships
    // data/b.pak, which v1 left alone. Only that original is missing from the
    // backup — tr.pak on disk is ours, ui.pak on disk is already patched.
    const QStringList v2Writes{"tr.pak", "ui.pak", "data/b.pak"};
    const QStringList record{"tr.pak", "ui.pak", /*added*/ "tr.pak", /*replaced*/ "ui.pak"};
    EXPECT_EQ(notOnRecord(v2Writes, record), QStringList({"data/b.pak"}));
}

TEST(InstallStepRules, AnInPlaceMarkerListsTheFileItEdited) {
    EXPECT_EQ(notOnRecord({"DataPC_patch_01.forge", "data/b.pak"},
                          {"_vpatch:DataPC_patch_01.forge", "a.pak"}),
              QStringList({"data/b.pak"}));
}

TEST(StepRules, APatchProgramsOutputNamesTheCauseItsExitCodeHides)
{
    // AC Shadows' KUR.bat (0.2.3): the files of another game version, listed before it gives up
    EXPECT_TRUE(toolOutputCause(QStringLiteral(
        "Assassin's Creed Shadows Turkce yama kurulumu\r\n  DataPC_boot.forge: orijinal\r\n"
        "  DataPC_boot_patch_02.forge: FARKLI SURUM (kayit tablolari farkli)\r\nKurulum yapilamadi"))
        .contains(QStringLiteral("sürümünü tanımadı")));
    // Windows' application control stopping the tool itself
    EXPECT_TRUE(toolOutputCause(QStringLiteral(
        "'C:/Users/x/AppData/Local/Temp/ACS.exe' was blocked by your organization's Device Guard policy."))
        .contains(QStringLiteral("uygulama denetimi")));
    // The tool's own help text alone ("yazilamiyor diyorsa …") names nothing
    EXPECT_TRUE(toolOutputCause(QStringLiteral(
        "Kurulum yapilamadi; yukaridaki mesaja bakin.\r\n  - \"yazilamiyor\" diyorsa KUR.bat'a sag tiklayin")).isEmpty());
}
