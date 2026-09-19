// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The guard that decides whether a detected game really is the catalog entry
// claiming it. It has two ways to be wrong, and both are here:
//
//   too strict — a supported game is hidden and the user is told it was not
//                detected, with nothing to do about it
//   too loose  — a repack that reuses a real appid gets the wrong game's
//                translation written over its files
//
// Names below are the ones the stores and the catalog actually write. Steam's
// appmanifest for 243470 says "Watch_Dogs"; cdn.makineceviri.org/assets/
// packages/243470.json says "Watch Dogs" (both read 2026-09-19).

#include <gtest/gtest.h>
#include "gamenamerules.h"

using namespace makine::namerules;

// ===== The regression: separators differ between the store and the catalog ==

TEST(GameNameRules, SteamUnderscoreNameMatchesSpacedCatalogName) {
    // Dropping the underscore gave "watchdogs" against "watch dogs" — no
    // shared token, so the match was rejected and Watch Dogs showed up as
    // unsupported on a machine that had it installed.
    EXPECT_TRUE(gameNamesLikelyMatch("Watch_Dogs", "Watch Dogs"));
    EXPECT_TRUE(gameNamesLikelyMatch("Watch_Dogs™", "Watch Dogs"));
    EXPECT_TRUE(gameNamesLikelyMatch("Watch_Dogs_2", "Watch Dogs 2"));
}

TEST(GameNameRules, DottedAcronymStillMatchesItsUndottedCatalogName) {
    // The other reading: here the dots have to vanish rather than become word
    // breaks, or "stalker" turns into seven one-letter tokens.
    EXPECT_TRUE(gameNamesLikelyMatch("S.T.A.L.K.E.R. 2", "STALKER 2"));
    EXPECT_TRUE(gameNamesLikelyMatch("S.T.A.L.K.E.R.: Shadow of Chernobyl",
                                     "STALKER Shadow of Chernobyl"));
}

// ===== The rejections the guard exists for =====

TEST(GameNameRules, RepackReusingAnotherGamesAppIdIsStillRejected) {
    // Little Nightmares' appid shipped in a repack of a different game.
    EXPECT_FALSE(gameNamesLikelyMatch("The Genesis Order", "Little Nightmares"));
}

TEST(GameNameRules, AFolderThatIsNotAGameIsStillRejected) {
    // Seen in the field: a directory literally named "steamapps" fuzzy-matched
    // to Stellaris.
    EXPECT_FALSE(gameNamesLikelyMatch("steamapps", "Stellaris"));
}

TEST(GameNameRules, UnrelatedNamesAreRejectedUnderBothReadings) {
    EXPECT_FALSE(gameNamesLikelyMatch("Portal 2", "Hollow Knight"));
    EXPECT_FALSE(gameNamesLikelyMatch("Elden Ring", "Cuphead"));
}

// ===== Ordinary names must be unaffected =====

TEST(GameNameRules, PunctuationAndTrademarksDoNotBreakAMatch) {
    EXPECT_TRUE(gameNamesLikelyMatch("Assassin's Creed® Odyssey",
                                     "Assassin's Creed Odyssey"));
    EXPECT_TRUE(gameNamesLikelyMatch("Marvel's Spider-Man 2", "Marvel's Spider-Man 2"));
    EXPECT_TRUE(gameNamesLikelyMatch("Half-Life", "Half-Life"));
    EXPECT_TRUE(gameNamesLikelyMatch("RESIDENT EVIL 7 biohazard",
                                     "Resident Evil 7 Biohazard"));
}

TEST(GameNameRules, SubtitleOnOneSideOnlyStillMatches) {
    EXPECT_TRUE(gameNamesLikelyMatch("Watch Dogs: Legion", "Watch Dogs Legion"));
    EXPECT_TRUE(gameNamesLikelyMatch("DOOM", "DOOM (2016)"));
}

TEST(GameNameRules, AnUndecidableSideTrustsTheCatalog) {
    // Nothing alphanumeric to compare: refusing here would hide a game over a
    // name we could not read.
    EXPECT_TRUE(gameNamesLikelyMatch("", "Watch Dogs"));
    EXPECT_TRUE(gameNamesLikelyMatch("???", "Watch Dogs"));
}

// ===== Every case the field actually produced =====
//
// Sampled from the "Game name mismatch" breadcrumb on 2026-09-19. Each of these
// is a real decision the guard made on a real machine: the first three were
// wrong, the rest were right and must stay that way.

TEST(GameNameRules, FieldCasesThatWereWronglyRejected) {
    EXPECT_TRUE(gameNamesLikelyMatch("Watch_Dogs", "Watch Dogs"));                  // 6x
    EXPECT_TRUE(gameNamesLikelyMatch("Mass_Effect_Legendary_Edition_TRyama",
                                     "Mass Effect Legendary Edition"));             // 14x
    EXPECT_TRUE(gameNamesLikelyMatch("ItTakesTwo", "It Takes Two"));                // 6x
}

TEST(GameNameRules, FieldCasesThatWereRightlyRejected) {
    // A library root mistaken for a game, and repacks reusing a real appid.
    EXPECT_FALSE(gameNamesLikelyMatch("steamapps", "Assassin's Creed Syndicate"));  // 21x
    EXPECT_FALSE(gameNamesLikelyMatch("steamapps", "Elden Ring"));                  // 12x
    EXPECT_FALSE(gameNamesLikelyMatch("steamapps", "Half-Life"));                   // 2x
    EXPECT_FALSE(gameNamesLikelyMatch("Docked", "Little Nightmares"));              // 12x
    EXPECT_FALSE(gameNamesLikelyMatch("The Outlast Trials", "Elden Ring"));         // 9x
    EXPECT_FALSE(gameNamesLikelyMatch("Counter-Strike WaRzOnE", "Half-Life"));      // 4x
}

// ===== Normalization itself =====

TEST(GameNameRules, DropReadingRemovesSeparatorsEntirely) {
    EXPECT_EQ(normalizeGameName("Watch_Dogs™", Separators::Drop), "watchdogs");
    EXPECT_EQ(normalizeGameName("S.T.A.L.K.E.R.", Separators::Drop), "stalker");
}

TEST(GameNameRules, CompactReadingDropsWordBreaksToo) {
    EXPECT_EQ(normalizeGameName("It Takes Two", Separators::Compact), "ittakestwo");
    EXPECT_EQ(normalizeGameName("ItTakesTwo", Separators::Compact), "ittakestwo");
}

TEST(GameNameRules, SpaceReadingTurnsSeparatorsIntoOneWordBreak) {
    EXPECT_EQ(normalizeGameName("Watch_Dogs™", Separators::AsSpace), "watch dogs");
    EXPECT_EQ(normalizeGameName("Marvel's  Spider-Man  2", Separators::AsSpace),
              "marvel s spider man 2");
}
