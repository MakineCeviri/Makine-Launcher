// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// What the launcher tells a user to do after a patch installs.
//
// The bug that produced this suite: Far Cry 6's catalogue note said "oyun
// içerisinden dil ayarını Türkçe olarak değiştirin" and Far Cry 6 has no
// Turkish entry — the pack overwrites an existing slot through a Dunia patch
// archive. The user followed the instruction, found nothing, and reported the
// patch broken. The instruction was prose; nothing could check it.
//
// Every path list below was read out of the live catalogue on 2026-09-19 by
// decrypting the published .makine and listing its tar members. Synthetic
// paths would have agreed with any rule; these are the ones that disagree.

#include <gtest/gtest.h>
#include "postinstallrules.h"

using namespace makine::postinstall;

namespace {

// Thief ships 200 Unreal ".int" files under Localization/INT plus 12 configs.
QStringList thiefPackage()
{
    QStringList files;
    for (int i = 0; i < 200; ++i)
        files << QStringLiteral("Localization/INT/Swt_Alerters_Barks_%1.int").arg(i);
    for (int i = 0; i < 12; ++i)
        files << QStringLiteral("Config/D3D11/Cfg%1.ini").arg(i);
    return files;
}

// The Witcher: 901 of 4712 files are named it_*.uti — Aurora's "item" prefix.
QStringList witcherPackage()
{
    QStringList files;
    for (int i = 0; i < 901; ++i)
        files << QStringLiteral("Data/_thewitcherTR/it_amm_%1.uti").arg(i);
    for (int i = 0; i < 3811; ++i)
        files << QStringLiteral("Data/_thewitcherTR/04_q%1slmd.utc").arg(i);
    return files;
}

} // namespace

// ===== The regression: a language the package never touches ================

TEST(PostInstallRules, DuniaPatchArchiveNamesNoLanguageSoNothingIsClaimed)
{
    // data_win32/patch.fat is keyed by hash. There is no language here to read,
    // and inventing one is exactly how the Far Cry 6 note went wrong.
    const QStringList farCry6 = {QStringLiteral("data_win32/patch.fat"),
                                 QStringLiteral("data_win32/patch.dat")};
    EXPECT_EQ(languageSlotFromPaths(farCry6), QString());
}

TEST(PostInstallRules, AnItemPrefixIsNotTheItalianLanguage)
{
    // Counted among tagged files this reads as 100% Italian. Counted among all
    // 4712 it is 19%, and a slot that covers a fifth of a package is not a slot.
    EXPECT_EQ(languageSlotFromPaths(witcherPackage()), QString());
}

// ===== Slots the packages really do write into =============================

TEST(PostInstallRules, UnrealInternationalFilesMeanEnglish)
{
    // Thief's note is right and now provable: "dili İngilizce olarak değiştirin".
    EXPECT_EQ(languageSlotFromPaths(thiefPackage()), QStringLiteral("İngilizce"));
}

TEST(PostInstallRules, AMinorityFileInAnotherLanguageDoesNotBlockTheDecision)
{
    // Curse of the Dead Gods: seven .FR against one .EN. The translation is in
    // the French slot — and the catalogue note tells the user to pick Turkish.
    const QStringList cotdg = {
        QStringLiteral("TempleResources/_Cooking/Text/Codex~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Common~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Credits~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/CursesAndBlessings~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Help~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Relics~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Weapons~GAM.xls.LocalText.gen.FR"),
        QStringLiteral("TempleResources/_Cooking/Text/Common~GAM.xls.LocalText.gen.EN"),
    };
    EXPECT_EQ(languageSlotFromPaths(cotdg), QStringLiteral("Fransızca"));
}

TEST(PostInstallRules, ATwoLetterCodeInsideAFilenameStillCounts)
{
    // Alan Wake 2 writes base-en-000.rmdblob: Turkish text into the English
    // slot, and its note never says so.
    const QStringList alanWake2 = {
        QStringLiteral("data_pack2/pc/base-en-000.rmdblob"),
        QStringLiteral("data_pack2/pc/base-en.rmdtoc"),
    };
    EXPECT_EQ(languageSlotFromPaths(alanWake2), QStringLiteral("İngilizce"));
}

TEST(PostInstallRules, EngineSpecificSuffixesResolve)
{
    // Mad Max marks the Polish slot with _pol; its note already says so.
    const QStringList madMax = {
        QStringLiteral("dropzone/global/global_pol.stringlookup"),
        QStringLiteral("dropzone/global/sound_global_pol.stringlookup"),
        QStringLiteral("dropzone/gui/fonts/mad_max_polish.font"),
    };
    EXPECT_EQ(languageSlotFromPaths(madMax), QStringLiteral("Lehçe"));

    // Larian names the file after the slot outright.
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("DefEd/Data/Localization/English.pak")}),
              QStringLiteral("İngilizce"));
}

TEST(PostInstallRules, TurkishSlotsAreDetectedTheSameWay)
{
    // Gone Home adds real Turkish files — here "select Turkish" is correct.
    EXPECT_EQ(languageSlotFromPaths(
                  {QStringLiteral("Text/Localized/Journals/01 - New House_Türkçe.txt"),
                   QStringLiteral("Text/Localized/Journals/02 - First Day_Türkçe.txt")}),
              QStringLiteral("Türkçe"));

    // A bare two-letter stem, and a region tag.
    EXPECT_EQ(languageSlotFromPaths(
                  {QStringLiteral("DD2_Data/StreamingAssets/Localization/Poedit/tr.po")}),
              QStringLiteral("Türkçe"));
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("mods/Yama/locale/tr_TR/main.mo")}),
              QStringLiteral("Türkçe"));
}

// ===== Refusing to decide ==================================================

TEST(PostInstallRules, AMinorityOfTaggedFilesDecidesNothing)
{
    // Kenshi: 2 of 10 files sit under locale/tr_TR, the rest are fonts and
    // mod metadata. Two files are not a language slot.
    const QStringList kenshi = {
        QStringLiteral("mods/Yama/Yama.mod"),          QStringLiteral("mods/Yama/_Yama.img"),
        QStringLiteral("mods/Yama/_Yama.info"),        QStringLiteral("mods/Yama/gui/fonts/a.ttf"),
        QStringLiteral("mods/Yama/gui/fonts/b.ttf"),   QStringLiteral("mods/Yama/gui/fonts/c.ttf"),
        QStringLiteral("mods/Yama/gui/fonts/d.xml"),   QStringLiteral("mods/Yama/readme.txt"),
        QStringLiteral("mods/Yama/locale/tr_TR/LC_MESSAGES/main.mo"),
        QStringLiteral("mods/Yama/locale/tr_TR/LC_MESSAGES/gui.mo"),
    };
    EXPECT_EQ(languageSlotFromPaths(kenshi), QString());
}

TEST(PostInstallRules, AnExactTieDecidesNothing)
{
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("loc/english.lang"),
                                     QStringLiteral("loc/french.lang")}),
              QString());
}

TEST(PostInstallRules, EmptyInputIsNotAnAnswer)
{
    EXPECT_EQ(languageSlotFromPaths({}), QString());
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("")}), QString());
}

TEST(PostInstallRules, ALanguageWordInsideALongerWordIsNotAMatch)
{
    // Substring matching would turn these into language slots.
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("maps/Englishtown.bsp")}), QString());
    EXPECT_EQ(languageSlotFromPaths({QStringLiteral("data/internal/cache.bin")}), QString());
}

// ===== Instruction or description? =========================================
//
// Every note below is a live catalogue entry. The launcher interrupts for the
// first group and stays quiet for the second.

TEST(PostInstallRules, NotesThatAskTheUserToDoSomething)
{
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Oyun icerisinden dil ayarini Turkce olarak degistirmeyi unutmayin.")));
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Yamayı kurduktan sonra oyun içi ayarlardan dili İngilizce olarak "
                       "değiştirin.")));
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Oyun ayarlarından alt yazıları açmayı unutmayın.")));
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Yamayı kurduktan sonra modları etkinleştirin.")));
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Oyun icerisinden Ayarlar > Dil > Turkce secilmelidir.")));
    EXPECT_TRUE(noteRequiresUserAction(
        QStringLiteral("Dublaj için ME3 mod motoru gereklidir.")));
}

TEST(PostInstallRules, NotesThatOnlyDescribeThePatch)
{
    // The suffix carries the whole difference: "değiştirir" reports what the
    // patch does, "değiştirin" asks the user to act. A rule matching bare
    // "değiştir" cannot separate them, and would interrupt for all 61 notes.
    EXPECT_FALSE(noteRequiresUserAction(
        QStringLiteral("İngilizce metin dosyasını Türkçe ile değiştirir.")));
    EXPECT_FALSE(noteRequiresUserAction(
        QStringLiteral("Unity resources.assets dosyasini degistirir. 2 surum mevcut.")));
    EXPECT_FALSE(noteRequiresUserAction(
        QStringLiteral("core, font, novel, quest, subtitle, txtmess, ui gibi 16 alt dizin "
                       "kopyalanır.")));
    EXPECT_FALSE(noteRequiresUserAction(
        QStringLiteral("ModEngine tabanlı yama. Oyun otomatik olarak Türkçe yüklenecektir.")));
    EXPECT_FALSE(noteRequiresUserAction(
        QStringLiteral("Ana oyun + 37 DLC çeviri dosyalarını içerir.")));
    EXPECT_FALSE(noteRequiresUserAction(QStringLiteral("")));
}

TEST(PostInstallRules, AsciiSpellingsAreAcceptedToo)
{
    // Half the catalogue's notes are written without Turkish diacritics.
    EXPECT_TRUE(noteRequiresUserAction(QStringLiteral("Altyazilari acmayi unutmayin.")));
    EXPECT_TRUE(noteRequiresUserAction(QStringLiteral("Oyun dilini Ingilizce yapin.")));
    EXPECT_TRUE(noteRequiresUserAction(QStringLiteral("Modlari etkinlestirin.")));
}
