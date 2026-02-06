/**
 * @file glossary_service.hpp
 * @brief Glossary service for consistent term translation
 * @copyright (c) 2026 MakineAI Team
 *
 * Features:
 * - Term management (add, update, delete)
 * - Alternative and forbidden translations
 * - Text matching with position tracking
 * - Priority-based term application
 * - Domain and game-specific filtering
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

namespace makineai {

/**
 * @brief Match position in text
 */
struct MatchPosition {
    size_t start;
    size_t end;

    [[nodiscard]] size_t length() const noexcept { return end - start; }
};

/**
 * @brief Glossary term match result
 */
struct GlossaryMatch {
    GlossaryTerm term;
    std::vector<MatchPosition> positions;
};

/**
 * @brief Forbidden term violation
 */
struct ForbiddenViolation {
    GlossaryTerm term;
    ForbiddenTranslation forbidden;
    std::string foundText;
};

/**
 * @brief Glossary statistics
 */
struct GlossaryStats {
    int64_t totalTerms = 0;
    int64_t doNotTranslate = 0;
    int64_t totalAlternatives = 0;
    int64_t totalForbidden = 0;
    std::map<std::string, int64_t> domainDistribution;
};

/**
 * @brief Glossary Service
 *
 * Manages game translation glossary for consistent terminology.
 * Supports caching, priority-based matching, and domain filtering.
 */
class GlossaryService {
public:
    /**
     * @brief Get singleton instance
     */
    static GlossaryService& instance();

    /**
     * @brief Clear the term cache
     */
    void clearCache();

    // ============== BASIC OPERATIONS ==============

    /**
     * @brief Add a new glossary term
     *
     * @param term Term to add (id will be assigned)
     * @return ID of inserted term or error
     */
    Result<int64_t> addTerm(const GlossaryTerm& term);

    /**
     * @brief Update existing term
     *
     * @param term Term with updated values (id required)
     * @return Success or error
     */
    Result<void> updateTerm(const GlossaryTerm& term);

    /**
     * @brief Delete term and related data
     *
     * @param termId Term ID to delete
     * @return Success or error
     */
    Result<void> deleteTerm(int64_t termId);

    /**
     * @brief Add alternative translation for a term
     *
     * @param termId Term ID
     * @param alternative Alternative translation
     * @param context Optional usage context
     * @return Success or error
     */
    Result<void> addAlternative(
        int64_t termId,
        const std::string& alternative,
        const std::optional<std::string>& context = std::nullopt
    );

    /**
     * @brief Add forbidden translation for a term
     *
     * @param termId Term ID
     * @param forbidden Forbidden translation
     * @param reason Optional reason
     * @return Success or error
     */
    Result<void> addForbidden(
        int64_t termId,
        const std::string& forbidden,
        const std::optional<std::string>& reason = std::nullopt
    );

    // ============== QUERYING ==============

    /**
     * @brief Get all glossary terms (cached)
     *
     * @param forceRefresh Bypass cache
     * @return List of all terms
     */
    Result<std::vector<GlossaryTerm>> getAllTerms(bool forceRefresh = false);

    /**
     * @brief Get terms by domain
     *
     * Also includes general domain terms.
     *
     * @param domain Domain filter
     * @return Filtered terms
     */
    Result<std::vector<GlossaryTerm>> getTermsByDomain(TermDomain domain);

    /**
     * @brief Get terms for a specific game
     *
     * Returns terms that are not game-specific or match the game ID.
     *
     * @param gameId Game ID
     * @return Filtered terms
     */
    Result<std::vector<GlossaryTerm>> getTermsForGame(const std::string& gameId);

    /**
     * @brief Search terms by source or target text
     *
     * @param query Search query
     * @return Matching terms
     */
    Result<std::vector<GlossaryTerm>> searchTerms(const std::string& query);

    // ============== TEXT MATCHING ==============

    /**
     * @brief Find glossary terms in text
     *
     * Returns terms with their match positions, sorted by priority.
     *
     * @param text Text to search
     * @param domain Optional domain filter
     * @param gameId Optional game filter
     * @return List of matches with positions
     */
    Result<std::vector<GlossaryMatch>> findTermsInText(
        const std::string& text,
        const std::optional<TermDomain>& domain = std::nullopt,
        const std::optional<std::string>& gameId = std::nullopt
    );

    /**
     * @brief Check translation for forbidden terms
     *
     * @param sourceText Original text
     * @param targetText Translation to check
     * @param gameId Optional game filter
     * @return List of violations found
     */
    Result<std::vector<ForbiddenViolation>> checkForbiddenTerms(
        const std::string& sourceText,
        const std::string& targetText,
        const std::optional<std::string>& gameId = std::nullopt
    );

    /**
     * @brief Apply glossary translations to text
     *
     * Replaces matched terms with their translations.
     * Respects priority and doNotTranslate flags.
     *
     * @param text Text to translate
     * @param domain Optional domain filter
     * @param gameId Optional game filter
     * @return Translated text
     */
    Result<std::string> applyGlossary(
        const std::string& text,
        const std::optional<TermDomain>& domain = std::nullopt,
        const std::optional<std::string>& gameId = std::nullopt
    );

    // ============== DEFAULT TERMS ==============

    /**
     * @brief Load default glossary terms
     *
     * Loads built-in common game terms.
     *
     * @return Number of terms loaded
     */
    Result<int> loadDefaultTerms();

    /**
     * @brief Ensure default terms are loaded
     *
     * Only loads if glossary is empty.
     *
     * @return Success or error
     */
    Result<void> ensureDefaultTermsLoaded();

    // ============== STATISTICS ==============

    /**
     * @brief Get glossary statistics
     *
     * @return Statistics or error
     */
    Result<GlossaryStats> getStats();

public:
    // Destructor must be public for unique_ptr usage in Core
    ~GlossaryService() = default;

private:
    GlossaryService() = default;
    GlossaryService(const GlossaryService&) = delete;
    GlossaryService& operator=(const GlossaryService&) = delete;

    // Find match positions for a term in text
    std::vector<MatchPosition> findMatchPositions(
        const std::string& text,
        const GlossaryTerm& term
    ) const;

    // Check if cache is valid
    bool isCacheValid() const;

    // Cache
    std::vector<GlossaryTerm> cachedTerms_;
    std::chrono::steady_clock::time_point lastCacheUpdate_;
    mutable std::mutex cacheMutex_;
    static constexpr std::chrono::minutes CACHE_DURATION{5};
};

/**
 * @brief Default glossary terms for common game terminology
 *
 * English -> Turkish mappings for frequently used terms.
 */
class DefaultGlossary {
public:
    /**
     * @brief Get all default terms
     */
    static std::vector<GlossaryTerm> getAllTerms();

    /**
     * @brief Get UI terms
     */
    static std::vector<GlossaryTerm> getUITerms();

    /**
     * @brief Get RPG terms
     */
    static std::vector<GlossaryTerm> getRPGTerms();

    /**
     * @brief Get FPS terms
     */
    static std::vector<GlossaryTerm> getFPSTerms();

    /**
     * @brief Get common action terms
     */
    static std::vector<GlossaryTerm> getActionTerms();
};

} // namespace makineai
