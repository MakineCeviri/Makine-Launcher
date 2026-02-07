/**
 * @file test_qa_service.cpp
 * @brief Unit tests for QA Service
 *
 * Tests: placeholder validation, escape sequences, tag balance,
 * length checks, character checks, Turkish character validation,
 * and full QA pipeline.
 *
 * Copyright (c) 2026 MakineAI Team
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/qa_service.hpp>

namespace makineai {
namespace testing {

using ::testing::Contains;
using ::testing::AnyOf;

// =============================================================================
// Helper: find QA issue by code
// =============================================================================

static bool hasIssueCode(const QAResult& result, const std::string& code) {
    for (const auto& issue : result.issues) {
        if (issue.code == code) return true;
    }
    return false;
}

// =============================================================================
// PLACEHOLDER HANDLER TESTS
// =============================================================================

class PlaceholderTest : public ::testing::Test {};

TEST_F(PlaceholderTest, DetectsPrintfPlaceholders) {
    auto placeholders = PlaceholderHandler::detectPlaceholders("Hello %s, you have %d items");
    EXPECT_EQ(placeholders.size(), 2u);
    EXPECT_EQ(placeholders[0].type, PlaceholderType::Printf);
    EXPECT_EQ(placeholders[0].original, "%s");
    EXPECT_EQ(placeholders[1].original, "%d");
}

TEST_F(PlaceholderTest, DetectsNamedPlaceholders) {
    auto placeholders = PlaceholderHandler::detectPlaceholders("Hello {name}, welcome to {place}");
    EXPECT_EQ(placeholders.size(), 2u);
    EXPECT_EQ(placeholders[0].type, PlaceholderType::Named);
    EXPECT_EQ(placeholders[0].original, "{name}");
}

TEST_F(PlaceholderTest, DetectsIndexedPlaceholders) {
    auto placeholders = PlaceholderHandler::detectPlaceholders("Player {0} scored {1} points");
    EXPECT_EQ(placeholders.size(), 2u);
    EXPECT_EQ(placeholders[0].type, PlaceholderType::Indexed);
}

TEST_F(PlaceholderTest, DetectsUnityRichText) {
    auto placeholders = PlaceholderHandler::detectPlaceholders("<color=#ff0000>Warning</color>");
    EXPECT_GE(placeholders.size(), 2u);
}

TEST_F(PlaceholderTest, DetectsEscapeSequences) {
    auto placeholders = PlaceholderHandler::detectPlaceholders("Line1\\nLine2\\tTabbed");
    EXPECT_EQ(placeholders.size(), 2u);
    EXPECT_EQ(placeholders[0].type, PlaceholderType::Escape);
}

TEST_F(PlaceholderTest, ValidatesMatchingPlaceholders) {
    auto result = PlaceholderHandler::validatePlaceholders(
        "Hello %s, score: %d", "Merhaba %s, skor: %d");
    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.missing.empty());
    EXPECT_TRUE(result.extra.empty());
}

TEST_F(PlaceholderTest, DetectsMissingPlaceholders) {
    auto result = PlaceholderHandler::validatePlaceholders(
        "Hello %s, score: %d", "Merhaba, skor: %d");
    EXPECT_FALSE(result.isValid);
    EXPECT_EQ(result.missing.size(), 1u);
    EXPECT_EQ(result.missing[0], "%s");
}

TEST_F(PlaceholderTest, DetectsExtraPlaceholders) {
    auto result = PlaceholderHandler::validatePlaceholders(
        "Hello %s", "Merhaba %s %d");
    EXPECT_FALSE(result.isValid);
    EXPECT_EQ(result.extra.size(), 1u);
}

TEST_F(PlaceholderTest, ChecksPrintfOrder) {
    EXPECT_TRUE(PlaceholderHandler::checkPrintfOrder(
        "Name: %s, Age: %d", "Ad: %s, Yas: %d"));
    EXPECT_FALSE(PlaceholderHandler::checkPrintfOrder(
        "Name: %s, Age: %d", "Yas: %d, Ad: %s"));
}

TEST_F(PlaceholderTest, ValidatesEscapeSequences) {
    EXPECT_TRUE(PlaceholderHandler::validateEscapeSequences(
        "Line1\\nLine2", "Satir1\\nSatir2"));
    EXPECT_FALSE(PlaceholderHandler::validateEscapeSequences(
        "Line1\\nLine2", "Satir1 Satir2"));
}

TEST_F(PlaceholderTest, ValidatesTagBalance) {
    EXPECT_TRUE(PlaceholderHandler::validateTagBalance(
        "<b>Bold</b> and <i>italic</i>"));
    EXPECT_FALSE(PlaceholderHandler::validateTagBalance(
        "<b>Bold and <i>italic</i>"));
}

// =============================================================================
// FULL QA SERVICE TESTS
// =============================================================================

class QAServiceTest : public ::testing::Test {};

TEST_F(QAServiceTest, PerfectTranslationGetsFullScore) {
    auto result = QAService::performFullQA("Hello", "Merhaba");
    EXPECT_GE(result.score, 90);
    EXPECT_TRUE(result.passed);
}

TEST_F(QAServiceTest, MissingPlaceholderIsCritical) {
    auto result = QAService::performFullQA(
        "Hello %s, you have %d items",
        "Merhaba, %d eşya var");
    EXPECT_TRUE(hasIssueCode(result, "PH_MISSING"));
    EXPECT_TRUE(result.hasCriticalIssues);
    EXPECT_LT(result.score, 80);
}

TEST_F(QAServiceTest, ExtraPlaceholderIsCritical) {
    auto result = QAService::performFullQA(
        "Hello player",
        "Merhaba %s oyuncu");
    EXPECT_TRUE(hasIssueCode(result, "PH_EXTRA"));
    EXPECT_TRUE(result.hasCriticalIssues);
}

TEST_F(QAServiceTest, EscapeMismatchIsCritical) {
    auto result = QAService::performFullQA(
        "Line1\\nLine2\\nLine3",
        "Satir1\\nSatir2");
    EXPECT_TRUE(hasIssueCode(result, "ESC_MISMATCH"));
}

TEST_F(QAServiceTest, TranslationTooLong) {
    auto result = QAService::performFullQA(
        "Hi",
        "Bu cok uzun bir ceviri metnidir ve orijinalin yuzde yuz elli katindan fazla");
    EXPECT_TRUE(hasIssueCode(result, "LEN_LONG"));
}

TEST_F(QAServiceTest, NullCharacterIsCritical) {
    std::string target = "Merhaba";
    target += '\x00';
    target += "dunya";
    auto result = QAService::performFullQA("Hello world", target);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_NULL"));
    EXPECT_TRUE(result.hasCriticalIssues);
}

TEST_F(QAServiceTest, WhitespaceDifferenceDetected) {
    auto result = QAService::performFullQA(
        "  Hello  ", "Merhaba");
    EXPECT_TRUE(hasIssueCode(result, "WS_DIFF"));
}

TEST_F(QAServiceTest, MissingFinalPunctuation) {
    auto result = QAService::performFullQA(
        "Hello world.", "Merhaba dunya");
    EXPECT_TRUE(hasIssueCode(result, "PUNCT_MISSING"));
}

TEST_F(QAServiceTest, FirstLetterCaseCheck) {
    auto result = QAService::performFullQA(
        "Hello world", "merhaba dunya");
    EXPECT_TRUE(hasIssueCode(result, "CASE_FIRST"));
}

// =============================================================================
// TURKISH CHARACTER TESTS
// =============================================================================

class TurkishCharTest : public ::testing::Test {};

TEST_F(TurkishCharTest, DetectsMojibake_LowercaseC) {
    // Double-encoded ç: C3 A7 → C3 83 C2 A7
    std::string mojibake = "Merhaba d\xC3\x83\xC2\xA7nya";
    auto result = QAService::performFullQA("Hello world", mojibake);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_MOJIBAKE"));
    EXPECT_TRUE(result.hasCriticalIssues);
}

TEST_F(TurkishCharTest, DetectsMojibake_LowercaseO) {
    // Double-encoded ö: C3 B6 → C3 83 C2 B6
    std::string mojibake = "G\xC3\x83\xC2\xB6r\xC3\x83\xC2\xBCnt\xC3\x83\xC2\xBC";
    auto result = QAService::performFullQA("Display", mojibake);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_MOJIBAKE"));
}

TEST_F(TurkishCharTest, DetectsMojibake_UppercaseI) {
    // Double-encoded İ: C4 B0 → C3 84 C2 B0
    std::string mojibake = "\xC3\x84\xC2\xB0stanbul'a ho\xC5\x9F geldiniz";
    auto result = QAService::performFullQA("Welcome to Istanbul", mojibake);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_MOJIBAKE"));
}

TEST_F(TurkishCharTest, DetectsReplacementCharacter) {
    // U+FFFD = EF BF BD
    std::string broken = "Merhaba d\xEF\xBF\xBDnya";
    auto result = QAService::performFullQA("Hello world", broken);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_REPLACEMENT"));
    EXPECT_TRUE(result.hasCriticalIssues);
}

TEST_F(TurkishCharTest, FontRiskWarningForNonCP1252Chars) {
    // İ (C4 B0), ı (C4 B1), ğ (C4 9F) are outside CP1252
    std::string turkish = "\xC4\xB0stanbul'\xC4\xB1n g\xC4\x9Fzel g\xC3\xBCnleri";
    auto result = QAService::performFullQA("Istanbul's beautiful days", turkish);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_FONT_RISK"));
}

TEST_F(TurkishCharTest, NoFontRiskForCP1252SafeChars) {
    // ç, ö, ü are in CP1252 — no font risk warning expected
    std::string safe = "\xC3\xA7ok g\xC3\xBCzel \xC3\xB6\xC4\x9Frenci";
    auto result = QAService::performFullQA("Very nice student", safe);
    // Should still have font risk because ğ is NOT in CP1252
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_FONT_RISK"));
}

TEST_F(TurkishCharTest, NoFontRiskForOnlyCP1252Chars) {
    // Only ç, ö, ü — all in CP1252
    std::string cp1252safe = "\xC3\xA7ok g\xC3\xBCzel bir \xC3\xB6rnek";
    auto result = QAService::performFullQA("A very nice example", cp1252safe);
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_FONT_RISK"));
}

TEST_F(TurkishCharTest, MissingTurkishCharsWarning) {
    // Long translation with no Turkish-specific characters
    auto result = QAService::performFullQA(
        "This is a long English sentence that should be translated",
        "Bu bir uzun Ingilizce cumle ve cevrisi yapilmali");
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_MISSING"));
}

TEST_F(TurkishCharTest, NoMissingWarningForShortText) {
    // Short text — no warning expected
    auto result = QAService::performFullQA("OK", "Tamam");
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_MISSING"));
}

TEST_F(TurkishCharTest, NoMissingWarningWhenTurkishCharsPresent) {
    // Text with Turkish chars — no missing warning
    std::string withTurkish = "Bu \xC3\xA7ok g\xC3\xBCzel bir \xC3\xB6rnek c\xC3\xBCmledir";
    auto result = QAService::performFullQA(
        "This is a very nice example sentence", withTurkish);
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_MISSING"));
}

TEST_F(TurkishCharTest, DottedIConfusionDetected) {
    // "bIlmek" — ASCII 'I' surrounded by lowercase letters
    // With Turkish chars present to confirm Turkish context
    std::string text = "bIlmek \xC3\xA7ok \xC3\xB6nemli";
    auto result = QAService::performFullQA("Knowing is important", text);
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_DOTTED_I"));
}

TEST_F(TurkishCharTest, NoDottedIFalsePositiveForEnglishWords) {
    // English-style text with no Turkish chars — should NOT trigger dotted-I check
    auto result = QAService::performFullQA("Hello", "Hello World");
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_DOTTED_I"));
}

TEST_F(TurkishCharTest, StrongerWarningForEnglishSourceNoTurkish) {
    // ASCII source + long target with no Turkish = stronger warning
    auto result = QAService::performFullQA(
        "This is definitely an English sentence for translation",
        "Bu kesinlikle bir Ingilizce cumle cevirisi icin kullanilmali");
    EXPECT_TRUE(hasIssueCode(result, "CHAR_TR_MISSING"));
    // Find the issue and check severity
    for (const auto& issue : result.issues) {
        if (issue.code == "CHAR_TR_MISSING") {
            EXPECT_EQ(issue.severity, QASeverity::Warning);
            break;
        }
    }
}

TEST_F(TurkishCharTest, CleanTurkishTranslationPasses) {
    // Well-formed Turkish translation should get high score
    std::string source = "Welcome to the game!";
    std::string target = "Oyuna ho\xC5\x9F geldiniz!";
    auto result = QAService::performFullQA(source, target);
    EXPECT_GE(result.score, 80);
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_MOJIBAKE"));
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_REPLACEMENT"));
    EXPECT_FALSE(hasIssueCode(result, "CHAR_TR_MISSING"));
}

// =============================================================================
// BATCH QA TESTS
// =============================================================================

class BatchQATest : public ::testing::Test {};

TEST_F(BatchQATest, ProcessesMultipleEntries) {
    std::map<int64_t, std::pair<std::string, std::string>> entries;
    entries[1] = {"Hello", "Merhaba"};
    entries[2] = {"World", "D\xC3\xBCnya"};
    entries[3] = {"Score: %d", "Skor: %d"};

    auto results = QAService::batchQA(entries);

    EXPECT_EQ(results.size(), 3u);
    EXPECT_TRUE(results[1].passed);
    EXPECT_TRUE(results[2].passed);
    EXPECT_TRUE(results[3].passed);
}

TEST_F(BatchQATest, CreatesSummary) {
    std::vector<QAResult> results;

    QAResult good;
    good.score = 95;
    good.passed = true;
    results.push_back(good);

    QAResult bad;
    bad.score = 40;
    bad.passed = false;
    bad.hasCriticalIssues = true;
    bad.issues.push_back(QAIssue{"TEST", "test issue", QASeverity::Critical, 20});
    results.push_back(bad);

    auto summary = QAService::createSummary(results);

    EXPECT_EQ(summary.totalEntries, 2);
    EXPECT_EQ(summary.passedEntries, 1);
    EXPECT_EQ(summary.failedEntries, 1);
    EXPECT_EQ(summary.criticalEntries, 1);
    EXPECT_DOUBLE_EQ(summary.averageScore, 67.5);
    EXPECT_GT(summary.totalIssues, 0);
}

} // namespace testing
} // namespace makineai
