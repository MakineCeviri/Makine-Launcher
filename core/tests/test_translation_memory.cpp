/**
 * @file test_translation_memory.cpp
 * @brief Unit tests for Translation Memory Service
 *
 * Tests: text normalization, hashing, n-gram generation,
 * similarity algorithms (Levenshtein, Jaccard, keyword, length),
 * hybrid scoring, and context bonuses.
 *
 * Note: Database operations are not tested here (require DB setup).
 * These tests focus on pure algorithmic / static methods.
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/translation_memory.hpp>

namespace makineai {
namespace testing {

using TMS = TranslationMemoryService;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::SizeIs;

// =============================================================================
// TEXT NORMALIZATION TESTS
// =============================================================================

class NormalizeTextTest : public ::testing::Test {};

TEST_F(NormalizeTextTest, ConvertsToLowercase) {
    EXPECT_EQ(TMS::normalizeText("Hello World"), "hello world");
    EXPECT_EQ(TMS::normalizeText("ALL CAPS"), "all caps");
    EXPECT_EQ(TMS::normalizeText("MiXeD CaSe"), "mixed case");
}

TEST_F(NormalizeTextTest, RemovesPunctuation) {
    EXPECT_EQ(TMS::normalizeText("Hello, World!"), "hello world");
    EXPECT_EQ(TMS::normalizeText("Score: 100%"), "score 100");
    EXPECT_EQ(TMS::normalizeText("file.txt"), "filetxt");
}

TEST_F(NormalizeTextTest, NormalizesWhitespace) {
    EXPECT_EQ(TMS::normalizeText("hello   world"), "hello world");
    EXPECT_EQ(TMS::normalizeText("  hello  "), "hello");
    EXPECT_EQ(TMS::normalizeText("a\tb\nc"), "a b c");
}

TEST_F(NormalizeTextTest, HandlesEmptyString) {
    EXPECT_EQ(TMS::normalizeText(""), "");
}

TEST_F(NormalizeTextTest, HandlesAllPunctuation) {
    EXPECT_EQ(TMS::normalizeText("...!!!???"), "");
}

TEST_F(NormalizeTextTest, PreservesNumbers) {
    EXPECT_EQ(TMS::normalizeText("Item 42 costs $99"), "item 42 costs 99");
}

TEST_F(NormalizeTextTest, HandlesOnlyWhitespace) {
    EXPECT_EQ(TMS::normalizeText("   \t\n  "), "");
}

// =============================================================================
// HASH TEXT TESTS
// =============================================================================

class HashTextTest : public ::testing::Test {};

TEST_F(HashTextTest, ProducesDeterministicHash) {
    auto hash1 = TMS::hashText("Hello World");
    auto hash2 = TMS::hashText("Hello World");
    EXPECT_EQ(hash1, hash2);
}

TEST_F(HashTextTest, SameHashForNormalizedEquivalents) {
    // Same text with different formatting should hash identically
    auto hash1 = TMS::hashText("Hello World");
    auto hash2 = TMS::hashText("hello   world");
    auto hash3 = TMS::hashText("HELLO WORLD!");
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash2, hash3);
}

TEST_F(HashTextTest, DifferentHashForDifferentText) {
    auto hash1 = TMS::hashText("Hello World");
    auto hash2 = TMS::hashText("Goodbye World");
    EXPECT_NE(hash1, hash2);
}

TEST_F(HashTextTest, HashIsHexString) {
    auto hash = TMS::hashText("test");
    EXPECT_EQ(hash.length(), 32u); // MD5 = 32 hex chars
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST_F(HashTextTest, EmptyStringHashes) {
    auto hash = TMS::hashText("");
    EXPECT_EQ(hash.length(), 32u);
}

// =============================================================================
// N-GRAM GENERATION TESTS
// =============================================================================

class NgramTest : public ::testing::Test {};

TEST_F(NgramTest, GeneratesTrigramsFromText) {
    auto ngrams = TMS::generateNgrams("hello", 3);
    // Normalized: "hello" -> trigrams: "hel", "ell", "llo"
    EXPECT_THAT(ngrams, SizeIs(3));
    EXPECT_THAT(ngrams, Contains("hel"));
    EXPECT_THAT(ngrams, Contains("ell"));
    EXPECT_THAT(ngrams, Contains("llo"));
}

TEST_F(NgramTest, GeneratesBigramsFromText) {
    auto ngrams = TMS::generateNgrams("hello", 2);
    // Normalized: "hello" -> bigrams: "he", "el", "ll", "lo"
    EXPECT_THAT(ngrams, SizeIs(4));
}

TEST_F(NgramTest, ShortTextReturnsSingleNgram) {
    auto ngrams = TMS::generateNgrams("hi", 3);
    // "hi" is shorter than n=3, should return the whole text
    EXPECT_THAT(ngrams, SizeIs(1));
    EXPECT_EQ(ngrams[0], "hi");
}

TEST_F(NgramTest, EmptyTextReturnsEmpty) {
    auto ngrams = TMS::generateNgrams("", 3);
    EXPECT_THAT(ngrams, IsEmpty());
}

TEST_F(NgramTest, NormalizesBeforeGenerating) {
    auto ngrams1 = TMS::generateNgrams("Hello", 3);
    auto ngrams2 = TMS::generateNgrams("hello", 3);
    EXPECT_EQ(ngrams1, ngrams2);
}

TEST_F(NgramTest, HandlesSpacesInNgrams) {
    auto ngrams = TMS::generateNgrams("ab cd", 3);
    // Normalized: "ab cd" -> "ab ", " cd", etc. depends on normalization
    // normalizeText("ab cd") = "ab cd" -> trigrams: "ab ", "b c", " cd"
    EXPECT_THAT(ngrams, SizeIs(3));
}

TEST_F(NgramTest, DefaultNgramSizeIsThree) {
    auto defaultNgrams = TMS::generateNgrams("hello");
    auto triNgrams = TMS::generateNgrams("hello", 3);
    EXPECT_EQ(defaultNgrams, triNgrams);
}

// =============================================================================
// LEVENSHTEIN DISTANCE TESTS
// =============================================================================

class LevenshteinTest : public ::testing::Test {};

TEST_F(LevenshteinTest, IdenticalStringsHaveZeroDistance) {
    EXPECT_EQ(TMS::levenshteinDistance("hello", "hello"), 0);
}

TEST_F(LevenshteinTest, CaseInsensitiveComparison) {
    EXPECT_EQ(TMS::levenshteinDistance("Hello", "hello"), 0);
    EXPECT_EQ(TMS::levenshteinDistance("WORLD", "world"), 0);
}

TEST_F(LevenshteinTest, SingleCharacterDifference) {
    EXPECT_EQ(TMS::levenshteinDistance("cat", "bat"), 1);
    EXPECT_EQ(TMS::levenshteinDistance("cat", "car"), 1);
}

TEST_F(LevenshteinTest, Insertion) {
    EXPECT_EQ(TMS::levenshteinDistance("cat", "cats"), 1);
    EXPECT_EQ(TMS::levenshteinDistance("hello", "helloo"), 1);
}

TEST_F(LevenshteinTest, Deletion) {
    EXPECT_EQ(TMS::levenshteinDistance("cats", "cat"), 1);
}

TEST_F(LevenshteinTest, EmptyStrings) {
    EXPECT_EQ(TMS::levenshteinDistance("", ""), 0);
    EXPECT_EQ(TMS::levenshteinDistance("hello", ""), 5);
    EXPECT_EQ(TMS::levenshteinDistance("", "world"), 5);
}

TEST_F(LevenshteinTest, CompletelyDifferentStrings) {
    // "abc" vs "xyz" = 3 substitutions
    EXPECT_EQ(TMS::levenshteinDistance("abc", "xyz"), 3);
}

TEST_F(LevenshteinTest, SymmetricDistance) {
    int d1 = TMS::levenshteinDistance("kitten", "sitting");
    int d2 = TMS::levenshteinDistance("sitting", "kitten");
    EXPECT_EQ(d1, d2);
}

// =============================================================================
// LEVENSHTEIN RATIO TESTS
// =============================================================================

class LevenshteinRatioTest : public ::testing::Test {};

TEST_F(LevenshteinRatioTest, IdenticalStringsGiveOne) {
    EXPECT_DOUBLE_EQ(TMS::levenshteinRatio("hello", "hello"), 1.0);
}

TEST_F(LevenshteinRatioTest, CompletelyDifferentGivesLowRatio) {
    double ratio = TMS::levenshteinRatio("abc", "xyz");
    EXPECT_LT(ratio, 0.1);
}

TEST_F(LevenshteinRatioTest, BothEmptyGivesOne) {
    EXPECT_DOUBLE_EQ(TMS::levenshteinRatio("", ""), 1.0);
}

TEST_F(LevenshteinRatioTest, SimilarStringsGiveHighRatio) {
    double ratio = TMS::levenshteinRatio("hello world", "hello worl");
    EXPECT_GT(ratio, 0.8);
}

TEST_F(LevenshteinRatioTest, RatioIsBetweenZeroAndOne) {
    double ratio = TMS::levenshteinRatio("abc", "abcdefghij");
    EXPECT_GE(ratio, 0.0);
    EXPECT_LE(ratio, 1.0);
}

// =============================================================================
// JACCARD SIMILARITY TESTS
// =============================================================================

class JaccardTest : public ::testing::Test {};

TEST_F(JaccardTest, IdenticalSetsGiveOne) {
    std::vector<std::string> set1 = {"hel", "ell", "llo"};
    std::vector<std::string> set2 = {"hel", "ell", "llo"};
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(set1, set2), 1.0);
}

TEST_F(JaccardTest, DisjointSetsGiveZero) {
    std::vector<std::string> set1 = {"abc", "bcd", "cde"};
    std::vector<std::string> set2 = {"xyz", "yzw", "zwv"};
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(set1, set2), 0.0);
}

TEST_F(JaccardTest, PartialOverlap) {
    std::vector<std::string> set1 = {"abc", "bcd", "cde"};
    std::vector<std::string> set2 = {"abc", "bcd", "xyz"};
    // Intersection = 2 (abc, bcd), Union = 4 (abc, bcd, cde, xyz)
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(set1, set2), 0.5);
}

TEST_F(JaccardTest, BothEmptyGiveOne) {
    std::vector<std::string> empty;
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(empty, empty), 1.0);
}

TEST_F(JaccardTest, OneEmptyGivesZero) {
    std::vector<std::string> set1 = {"abc"};
    std::vector<std::string> empty;
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(set1, empty), 0.0);
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(empty, set1), 0.0);
}

TEST_F(JaccardTest, HandlesDuplicateNgrams) {
    // Duplicates should be deduplicated (uses unordered_set)
    std::vector<std::string> set1 = {"abc", "abc", "abc"};
    std::vector<std::string> set2 = {"abc"};
    EXPECT_DOUBLE_EQ(TMS::jaccardSimilarity(set1, set2), 1.0);
}

// =============================================================================
// LENGTH SIMILARITY TESTS
// =============================================================================

class LengthSimilarityTest : public ::testing::Test {};

TEST_F(LengthSimilarityTest, SameLengthGivesOne) {
    EXPECT_DOUBLE_EQ(TMS::lengthSimilarity("hello", "world"), 1.0);
}

TEST_F(LengthSimilarityTest, DoubleLengthGivesHalf) {
    EXPECT_DOUBLE_EQ(TMS::lengthSimilarity("hi", "hiii"), 0.5);
}

TEST_F(LengthSimilarityTest, OneEmptyGivesZero) {
    EXPECT_DOUBLE_EQ(TMS::lengthSimilarity("hello", ""), 0.0);
    EXPECT_DOUBLE_EQ(TMS::lengthSimilarity("", "hello"), 0.0);
}

TEST_F(LengthSimilarityTest, BothEmptyGivesOne) {
    EXPECT_DOUBLE_EQ(TMS::lengthSimilarity("", ""), 1.0);
}

TEST_F(LengthSimilarityTest, IsSymmetric) {
    double r1 = TMS::lengthSimilarity("short", "much longer text");
    double r2 = TMS::lengthSimilarity("much longer text", "short");
    EXPECT_DOUBLE_EQ(r1, r2);
}

// =============================================================================
// KEYWORD MATCH RATIO TESTS
// =============================================================================

class KeywordMatchTest : public ::testing::Test {};

TEST_F(KeywordMatchTest, IdenticalKeywordsGiveOne) {
    EXPECT_DOUBLE_EQ(TMS::keywordMatchRatio(
        "the quick brown fox", "the quick brown fox"), 1.0);
}

TEST_F(KeywordMatchTest, NoMatchingKeywordsGiveZero) {
    EXPECT_DOUBLE_EQ(TMS::keywordMatchRatio(
        "hello world test", "abc def ghi"), 0.0);
}

TEST_F(KeywordMatchTest, ShortWordsAreFiltered) {
    // Words < 3 chars are excluded
    EXPECT_DOUBLE_EQ(TMS::keywordMatchRatio("a b c", "a b c"), 1.0);
    // Both have empty keyword sets -> 1.0
}

TEST_F(KeywordMatchTest, PartialOverlap) {
    double ratio = TMS::keywordMatchRatio(
        "the quick brown fox", "the slow brown dog");
    // keywords (>=3 chars): {the, quick, brown, fox} vs {the, slow, brown, dog}
    // intersection: {the, brown} = 2
    // union: {the, quick, brown, fox, slow, dog} = 6
    EXPECT_NEAR(ratio, 2.0 / 6.0, 0.01);
}

TEST_F(KeywordMatchTest, CaseInsensitive) {
    double r1 = TMS::keywordMatchRatio("Hello World", "hello world");
    EXPECT_DOUBLE_EQ(r1, 1.0);
}

TEST_F(KeywordMatchTest, BothEmptyGiveOne) {
    EXPECT_DOUBLE_EQ(TMS::keywordMatchRatio("", ""), 1.0);
}

TEST_F(KeywordMatchTest, OneEmptyGivesZero) {
    EXPECT_DOUBLE_EQ(TMS::keywordMatchRatio("hello world", ""), 0.0);
}

// =============================================================================
// HYBRID SIMILARITY SCORE TESTS
// =============================================================================

class SimilarityScoreTest : public ::testing::Test {};

TEST_F(SimilarityScoreTest, ExactMatchGivesPerfectScore) {
    double score = TMS::calculateSimilarityScore("Hello World", "Hello World");
    EXPECT_DOUBLE_EQ(score, 100.0);
}

TEST_F(SimilarityScoreTest, NormalizedExactMatchGivesPerfectScore) {
    // Same text after normalization
    double score = TMS::calculateSimilarityScore(
        "Hello, World!", "hello world");
    EXPECT_DOUBLE_EQ(score, 100.0);
}

TEST_F(SimilarityScoreTest, NearExactMatchGivesHighScore) {
    double score = TMS::calculateSimilarityScore(
        "Hello World", "Hello Wprld"); // 1 typo
    EXPECT_GE(score, 90.0);
}

TEST_F(SimilarityScoreTest, SimilarStringsGetFuzzyScore) {
    double score = TMS::calculateSimilarityScore(
        "Save your game progress", "Save game progress now");
    EXPECT_GT(score, 50.0);
    EXPECT_LT(score, 100.0);
}

TEST_F(SimilarityScoreTest, CompletelyDifferentStringsGetLowScore) {
    double score = TMS::calculateSimilarityScore(
        "Hello World", "Lorem ipsum dolor sit amet");
    EXPECT_LT(score, 40.0);
}

TEST_F(SimilarityScoreTest, ScoreClampedTo0_100) {
    double score = TMS::calculateSimilarityScore("a", "zzzzzzzzzzzzzzz");
    EXPECT_GE(score, 0.0);
    EXPECT_LE(score, 100.0);
}

// =============================================================================
// CONTEXT BONUS/PENALTY TESTS
// =============================================================================

class ContextBonusTest : public ::testing::Test {};

TEST_F(ContextBonusTest, SameGameGivesBonus) {
    TMS::MatchContext srcCtx;
    srcCtx.gameId = "disco_elysium";

    TMS::MatchContext candCtx;
    candCtx.gameId = "disco_elysium";

    double withCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", srcCtx, candCtx);

    double withoutCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy");

    EXPECT_GT(withCtx, withoutCtx);
}

TEST_F(ContextBonusTest, SameEngineGivesBonus) {
    TMS::MatchContext srcCtx;
    srcCtx.engineType = "Unity";

    TMS::MatchContext candCtx;
    candCtx.engineType = "Unity";

    double withCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", srcCtx, candCtx);

    double withoutCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy");

    EXPECT_GT(withCtx, withoutCtx);
}

TEST_F(ContextBonusTest, SameCategoryGivesBonus) {
    TMS::MatchContext srcCtx;
    srcCtx.category = "dialogue";

    TMS::MatchContext candCtx;
    candCtx.category = "dialogue";

    double withCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", srcCtx, candCtx);

    double withoutCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy");

    EXPECT_GT(withCtx, withoutCtx);
}

TEST_F(ContextBonusTest, DifferentContextGivesPenalty) {
    TMS::MatchContext srcCtx;
    srcCtx.context = "main_menu";

    TMS::MatchContext candCtx;
    candCtx.context = "battle";

    double withPenalty = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", srcCtx, candCtx);

    double withoutCtx = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy");

    EXPECT_LT(withPenalty, withoutCtx);
}

TEST_F(ContextBonusTest, AllBonusesStack) {
    TMS::MatchContext srcCtx;
    srcCtx.gameId = "disco";
    srcCtx.engineType = "Unity";
    srcCtx.category = "dialogue";

    TMS::MatchContext candCtx;
    candCtx.gameId = "disco";
    candCtx.engineType = "Unity";
    candCtx.category = "dialogue";

    // gameId +10, engineType +5, category +5 = +20 total
    TMS::MatchContext singleCtx;
    singleCtx.gameId = "disco";

    TMS::MatchContext singleCandCtx;
    singleCandCtx.gameId = "disco";

    double allBonuses = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", srcCtx, candCtx);

    double singleBonus = TMS::calculateSimilarityScore(
        "Attack the enemy", "Defeat the enemy", singleCtx, singleCandCtx);

    EXPECT_GT(allBonuses, singleBonus);
}

TEST_F(ContextBonusTest, NulloptContextNoEffect) {
    TMS::MatchContext emptyCtx;

    double score1 = TMS::calculateSimilarityScore(
        "Hello World", "Hello Planet");
    double score2 = TMS::calculateSimilarityScore(
        "Hello World", "Hello Planet", emptyCtx, emptyCtx);

    EXPECT_DOUBLE_EQ(score1, score2);
}

// =============================================================================
// EDGE CASES
// =============================================================================

class EdgeCaseTest : public ::testing::Test {};

TEST_F(EdgeCaseTest, SingleCharacterStrings) {
    int dist = TMS::levenshteinDistance("a", "b");
    EXPECT_EQ(dist, 1);

    double ratio = TMS::levenshteinRatio("a", "a");
    EXPECT_DOUBLE_EQ(ratio, 1.0);
}

TEST_F(EdgeCaseTest, VeryLongStrings) {
    std::string long1(1000, 'a');
    std::string long2(1000, 'b');

    // Should not crash or hang
    int dist = TMS::levenshteinDistance(long1, long2);
    EXPECT_EQ(dist, 1000);

    double ratio = TMS::levenshteinRatio(long1, long2);
    EXPECT_DOUBLE_EQ(ratio, 0.0);
}

TEST_F(EdgeCaseTest, UnicodeInNormalization) {
    // UTF-8 Turkish characters should pass through normalizeText
    // (non-ASCII bytes are not alnum in C locale, so they get stripped)
    std::string turkish = "Merhaba d\xC3\xBCnya";
    auto normalized = TMS::normalizeText(turkish);
    // The UTF-8 bytes for 'ü' are not ASCII alnum, so they get removed
    // This is expected behavior for the normalization algorithm
    EXPECT_FALSE(normalized.empty());
}

TEST_F(EdgeCaseTest, NgramsShorterThanN) {
    auto ngrams = TMS::generateNgrams("a", 5);
    EXPECT_THAT(ngrams, SizeIs(1));
    EXPECT_EQ(ngrams[0], "a");
}

TEST_F(EdgeCaseTest, NgramsExactlyN) {
    auto ngrams = TMS::generateNgrams("abc", 3);
    EXPECT_THAT(ngrams, SizeIs(1));
    EXPECT_EQ(ngrams[0], "abc");
}

TEST_F(EdgeCaseTest, LevenshteinWithRepeatedChars) {
    EXPECT_EQ(TMS::levenshteinDistance("aaa", "aaaa"), 1);
    EXPECT_EQ(TMS::levenshteinDistance("aaa", "aab"), 1);
}

TEST_F(EdgeCaseTest, SimilarityScoreIsSymmetric) {
    double s1 = TMS::calculateSimilarityScore("Hello World", "Hello Planet");
    double s2 = TMS::calculateSimilarityScore("Hello Planet", "Hello World");
    // Levenshtein is symmetric, Jaccard is symmetric, length is symmetric
    EXPECT_NEAR(s1, s2, 0.01);
}

// =============================================================================
// GAME TRANSLATION CONTEXT TESTS
// =============================================================================

class GameTranslationTest : public ::testing::Test {};

TEST_F(GameTranslationTest, SimilarGameStringsScore) {
    // Common game translation scenario: similar UI strings
    double score = TMS::calculateSimilarityScore(
        "Press Start to begin", "Press Start to continue");
    EXPECT_GT(score, 60.0);
}

TEST_F(GameTranslationTest, MenuItemsSimilarity) {
    double score = TMS::calculateSimilarityScore(
        "New Game", "New Game+");
    EXPECT_GT(score, 70.0);
}

TEST_F(GameTranslationTest, PlaceholderTextSimilarity) {
    // Strings with different placeholder content
    double score = TMS::calculateSimilarityScore(
        "Player %s has %d points", "Player %s has %d coins");
    EXPECT_GT(score, 60.0);
}

TEST_F(GameTranslationTest, DifferentContextStrings) {
    double score = TMS::calculateSimilarityScore(
        "Are you sure?", "Loading... Please wait.");
    EXPECT_LT(score, 40.0);
}

} // namespace testing
} // namespace makineai
