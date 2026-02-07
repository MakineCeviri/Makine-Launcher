/**
 * @file test_glossary_service.cpp
 * @brief Unit tests for GlossaryService and DefaultGlossary
 * @copyright (c) 2026 MakineAI Team
 *
 * Tests term management, text matching, forbidden term detection,
 * glossary application, default terms, and caching.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <makineai/glossary_service.hpp>
#include <makineai/database.hpp>
#include <makineai/types.hpp>
#include <filesystem>

namespace fs = std::filesystem;
using namespace makineai;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::Not;
using ::testing::Gt;

// ============================================================================
// Test Fixture — initializes DB + GlossaryService for each test
// ============================================================================

class GlossaryServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = fs::temp_directory_path() / "makineai_test_glossary";
        fs::create_directories(tempDir_);
        dbPath_ = tempDir_ / "glossary_test.db";
        fs::remove(dbPath_);

        auto dbResult = Database::instance().initialize(dbPath_);
        ASSERT_TRUE(dbResult.has_value()) << "DB init failed";

        GlossaryService::instance().clearCache();
    }

    void TearDown() override {
        GlossaryService::instance().clearCache();
        Database::instance().close();
        fs::remove_all(tempDir_);
    }

    // Helper: add a term and return its ID
    int64_t addTerm(
        const std::string& source,
        const std::string& target,
        TermDomain domain = TermDomain::General,
        int priority = 50,
        bool doNotTranslate = false
    ) {
        GlossaryTerm term;
        term.termSource = source;
        term.termTarget = target;
        term.domain = domain;
        term.priority = priority;
        term.doNotTranslate = doNotTranslate;
        auto result = GlossaryService::instance().addTerm(term);
        EXPECT_TRUE(result.has_value());
        return result.has_value() ? result.value() : -1;
    }

    fs::path tempDir_;
    fs::path dbPath_;
};

// ============================================================================
// DefaultGlossary
// ============================================================================

class DefaultGlossaryTest : public ::testing::Test {};

TEST_F(DefaultGlossaryTest, AllTermsNotEmpty) {
    auto terms = DefaultGlossary::getAllTerms();
    EXPECT_THAT(terms, Not(IsEmpty()));
}

TEST_F(DefaultGlossaryTest, UITermsHaveSourceAndTarget) {
    auto terms = DefaultGlossary::getUITerms();
    EXPECT_THAT(terms, Not(IsEmpty()));
    for (const auto& t : terms) {
        EXPECT_FALSE(t.termSource.empty()) << "UI term missing source";
        EXPECT_FALSE(t.termTarget.empty()) << "UI term missing target";
    }
}

TEST_F(DefaultGlossaryTest, RPGTermsHaveSourceAndTarget) {
    auto terms = DefaultGlossary::getRPGTerms();
    EXPECT_THAT(terms, Not(IsEmpty()));
    for (const auto& t : terms) {
        EXPECT_FALSE(t.termSource.empty());
        EXPECT_FALSE(t.termTarget.empty());
    }
}

TEST_F(DefaultGlossaryTest, FPSTermsExist) {
    auto terms = DefaultGlossary::getFPSTerms();
    EXPECT_THAT(terms, Not(IsEmpty()));
}

TEST_F(DefaultGlossaryTest, ActionTermsExist) {
    auto terms = DefaultGlossary::getActionTerms();
    EXPECT_THAT(terms, Not(IsEmpty()));
}

// ============================================================================
// Term CRUD
// ============================================================================

TEST_F(GlossaryServiceTest, AddTermAndGetAll) {
    addTerm("Health", "Can", TermDomain::RPG);
    addTerm("Mana", "Buyu Gucu", TermDomain::RPG);

    auto result = GlossaryService::instance().getAllTerms(true);
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(2));
}

TEST_F(GlossaryServiceTest, UpdateTerm) {
    auto id = addTerm("Shield", "Kalkan", TermDomain::RPG);
    ASSERT_GT(id, 0);

    GlossaryTerm updated;
    updated.id = id;
    updated.termSource = "Shield";
    updated.termTarget = "Siper";
    updated.domain = TermDomain::RPG;
    updated.priority = 75;

    auto updateResult = GlossaryService::instance().updateTerm(updated);
    EXPECT_TRUE(updateResult.has_value());

    auto all = GlossaryService::instance().getAllTerms(true);
    ASSERT_TRUE(all.has_value());
    EXPECT_EQ(all.value().front().termTarget, "Siper");
}

TEST_F(GlossaryServiceTest, DeleteTerm) {
    auto id = addTerm("Weapon", "Silah");
    ASSERT_GT(id, 0);

    auto delResult = GlossaryService::instance().deleteTerm(id);
    EXPECT_TRUE(delResult.has_value());

    auto all = GlossaryService::instance().getAllTerms(true);
    ASSERT_TRUE(all.has_value());
    EXPECT_THAT(all.value(), IsEmpty());
}

// ============================================================================
// Alternatives and Forbidden
// ============================================================================

TEST_F(GlossaryServiceTest, AddAlternativeTranslation) {
    auto id = addTerm("Skill", "Yetenek", TermDomain::RPG);
    ASSERT_GT(id, 0);

    auto result = GlossaryService::instance().addAlternative(
        id, "Beceri", "passive skills context");
    EXPECT_TRUE(result.has_value());
}

TEST_F(GlossaryServiceTest, AddForbiddenTranslation) {
    auto id = addTerm("Experience", "Deneyim", TermDomain::RPG);
    ASSERT_GT(id, 0);

    auto result = GlossaryService::instance().addForbidden(
        id, "Tecrube", "Informal, prefer Deneyim");
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Querying
// ============================================================================

TEST_F(GlossaryServiceTest, GetTermsByDomain) {
    addTerm("Health", "Can", TermDomain::RPG);
    addTerm("Reload", "Yeniden Yukle", TermDomain::FPS);
    addTerm("Settings", "Ayarlar", TermDomain::General);

    auto rpgResult = GlossaryService::instance().getTermsByDomain(TermDomain::RPG);
    ASSERT_TRUE(rpgResult.has_value());
    // Should include RPG + General terms
    EXPECT_GE(rpgResult.value().size(), 2u);

    auto fpsResult = GlossaryService::instance().getTermsByDomain(TermDomain::FPS);
    ASSERT_TRUE(fpsResult.has_value());
    EXPECT_GE(fpsResult.value().size(), 2u);
}

TEST_F(GlossaryServiceTest, GetTermsForGame) {
    GlossaryTerm gameSpecific;
    gameSpecific.termSource = "Soul";
    gameSpecific.termTarget = "Ruh";
    gameSpecific.domain = TermDomain::RPG;
    gameSpecific.gameSpecific = "hollow_knight";

    GlossaryService::instance().addTerm(gameSpecific);
    addTerm("Health", "Can");

    auto result = GlossaryService::instance().getTermsForGame("hollow_knight");
    ASSERT_TRUE(result.has_value());
    // Should include game-specific + general (non-game-specific) terms
    EXPECT_GE(result.value().size(), 2u);
}

TEST_F(GlossaryServiceTest, SearchTerms) {
    addTerm("Save Game", "Oyunu Kaydet");
    addTerm("Load Game", "Oyunu Yukle");
    addTerm("Health", "Can");

    auto result = GlossaryService::instance().searchTerms("Game");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(2));
}

// ============================================================================
// Text Matching
// ============================================================================

TEST_F(GlossaryServiceTest, FindTermsInText) {
    addTerm("health", "can", TermDomain::RPG);
    addTerm("mana", "buyu gucu", TermDomain::RPG);

    auto result = GlossaryService::instance().findTermsInText(
        "Your health and mana are restored.");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(2));
}

TEST_F(GlossaryServiceTest, FindTermsNoMatch) {
    addTerm("shield", "kalkan");

    auto result = GlossaryService::instance().findTermsInText(
        "The sword is powerful.");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), IsEmpty());
}

TEST_F(GlossaryServiceTest, CheckForbiddenTermsDetectsViolation) {
    auto id = addTerm("Experience", "Deneyim", TermDomain::RPG);
    GlossaryService::instance().addForbidden(id, "Tecrube");

    auto result = GlossaryService::instance().checkForbiddenTerms(
        "You gained experience.", "Tecrube kazandiniz.");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), SizeIs(Gt(0u)));
}

TEST_F(GlossaryServiceTest, CheckForbiddenTermsNoViolation) {
    auto id = addTerm("Experience", "Deneyim", TermDomain::RPG);
    GlossaryService::instance().addForbidden(id, "Tecrube");

    auto result = GlossaryService::instance().checkForbiddenTerms(
        "You gained experience.", "Deneyim kazandiniz.");
    ASSERT_TRUE(result.has_value());
    EXPECT_THAT(result.value(), IsEmpty());
}

// ============================================================================
// Apply Glossary
// ============================================================================

TEST_F(GlossaryServiceTest, ApplyGlossaryReplacesTerms) {
    addTerm("Save", "Kaydet");
    addTerm("Load", "Yukle");

    auto result = GlossaryService::instance().applyGlossary(
        "Save and Load your progress.");
    ASSERT_TRUE(result.has_value());

    auto& translated = result.value();
    EXPECT_NE(translated.find("Kaydet"), std::string::npos);
    EXPECT_NE(translated.find("Yukle"), std::string::npos);
}

TEST_F(GlossaryServiceTest, ApplyGlossaryRespectsDoNotTranslate) {
    GlossaryTerm dnt;
    dnt.termSource = "PlayStation";
    dnt.termTarget = "PlayStation";
    dnt.doNotTranslate = true;
    GlossaryService::instance().addTerm(dnt);

    auto result = GlossaryService::instance().applyGlossary(
        "Available on PlayStation.");
    ASSERT_TRUE(result.has_value());
    // PlayStation should remain unchanged
    EXPECT_NE(result.value().find("PlayStation"), std::string::npos);
}

// ============================================================================
// Default Terms Loading
// ============================================================================

TEST_F(GlossaryServiceTest, LoadDefaultTerms) {
    auto result = GlossaryService::instance().loadDefaultTerms();
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);

    auto all = GlossaryService::instance().getAllTerms(true);
    ASSERT_TRUE(all.has_value());
    EXPECT_THAT(all.value(), Not(IsEmpty()));
}

TEST_F(GlossaryServiceTest, EnsureDefaultTermsLoadedIdempotent) {
    auto first = GlossaryService::instance().ensureDefaultTermsLoaded();
    EXPECT_TRUE(first.has_value());

    auto countAfterFirst = GlossaryService::instance().getAllTerms(true).value().size();

    auto second = GlossaryService::instance().ensureDefaultTermsLoaded();
    EXPECT_TRUE(second.has_value());

    auto countAfterSecond = GlossaryService::instance().getAllTerms(true).value().size();
    EXPECT_EQ(countAfterFirst, countAfterSecond);
}

// ============================================================================
// Statistics
// ============================================================================

TEST_F(GlossaryServiceTest, GetStats) {
    addTerm("Health", "Can", TermDomain::RPG);
    addTerm("Attack", "Saldiri", TermDomain::RPG);

    GlossaryTerm dnt;
    dnt.termSource = "Xbox";
    dnt.termTarget = "Xbox";
    dnt.doNotTranslate = true;
    GlossaryService::instance().addTerm(dnt);

    auto stats = GlossaryService::instance().getStats();
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(stats.value().totalTerms, 3);
    EXPECT_EQ(stats.value().doNotTranslate, 1);
}

// ============================================================================
// Cache Behavior
// ============================================================================

TEST_F(GlossaryServiceTest, CacheWorksAfterClear) {
    addTerm("Test", "Deneme");

    // First call fills cache
    auto first = GlossaryService::instance().getAllTerms();
    ASSERT_TRUE(first.has_value());
    EXPECT_THAT(first.value(), SizeIs(1));

    // Second call uses cache
    auto second = GlossaryService::instance().getAllTerms();
    ASSERT_TRUE(second.has_value());
    EXPECT_THAT(second.value(), SizeIs(1));

    // Clear and add new term
    GlossaryService::instance().clearCache();
    addTerm("New", "Yeni");

    // Third call should see both terms
    auto third = GlossaryService::instance().getAllTerms(true);
    ASSERT_TRUE(third.has_value());
    EXPECT_THAT(third.value(), SizeIs(2));
}
