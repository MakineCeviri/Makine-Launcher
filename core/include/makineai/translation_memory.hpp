/**
 * @file translation_memory.hpp
 * @brief Translation Memory service with fuzzy matching and n-gram indexing
 * @copyright (c) 2026 MakineAI Team
 *
 * Features:
 * - Exact and fuzzy string matching
 * - N-gram based candidate retrieval
 * - Hybrid similarity scoring (Levenshtein, Jaccard, keyword)
 * - Context-aware matching with game/engine bonuses
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

namespace makineai {

/**
 * @brief N-gram configuration
 */
constexpr int TM_NGRAM_SIZE = 3;
constexpr double TM_MIN_FUZZY_SCORE = 40.0;

/**
 * @brief Translation Memory Service
 *
 * Provides fuzzy matching and n-gram indexing for translation memory.
 * Uses hybrid scoring algorithm combining multiple similarity metrics.
 */
class TranslationMemoryService {
public:
    // ============== TEXT PROCESSING ==============

    /**
     * @brief Normalize text for comparison/hashing
     *
     * Operations:
     * - Convert to lowercase
     * - Remove punctuation
     * - Normalize whitespace
     *
     * @param text Input text
     * @return Normalized text
     */
    static std::string normalizeText(const std::string& text);

    /**
     * @brief Generate MD5 hash of normalized text
     *
     * @param text Input text (will be normalized)
     * @return MD5 hash string
     */
    static std::string hashText(const std::string& text);

    /**
     * @brief Generate n-grams from text
     *
     * @param text Input text
     * @param n N-gram size (default: TM_NGRAM_SIZE)
     * @return List of n-grams
     */
    static std::vector<std::string> generateNgrams(
        const std::string& text,
        int n = TM_NGRAM_SIZE
    );

    // ============== SIMILARITY ALGORITHMS ==============

    /**
     * @brief Calculate Levenshtein edit distance
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance
     */
    static int levenshteinDistance(const std::string& s1, const std::string& s2);

    /**
     * @brief Calculate Levenshtein similarity ratio (0.0 - 1.0)
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Similarity ratio
     */
    static double levenshteinRatio(const std::string& s1, const std::string& s2);

    /**
     * @brief Calculate Jaccard similarity between n-gram sets
     *
     * @param ngrams1 First n-gram set
     * @param ngrams2 Second n-gram set
     * @return Similarity (0.0 - 1.0)
     */
    static double jaccardSimilarity(
        const std::vector<std::string>& ngrams1,
        const std::vector<std::string>& ngrams2
    );

    /**
     * @brief Calculate length similarity
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Length ratio (0.0 - 1.0)
     */
    static double lengthSimilarity(const std::string& s1, const std::string& s2);

    /**
     * @brief Calculate keyword match ratio
     *
     * Compares words (3+ characters) between strings.
     *
     * @param s1 First string
     * @param s2 Second string
     * @return Match ratio (0.0 - 1.0)
     */
    static double keywordMatchRatio(const std::string& s1, const std::string& s2);

    /**
     * @brief Match context for similarity scoring
     */
    struct MatchContext {
        std::optional<std::string> context;
        std::optional<std::string> gameId;
        std::optional<std::string> engineType;
        std::optional<std::string> category;
    };

    /**
     * @brief Calculate hybrid similarity score
     *
     * Combines multiple metrics:
     * - 40% Levenshtein ratio
     * - 30% Jaccard similarity
     * - 20% Keyword match
     * - 10% Length similarity
     *
     * Plus bonuses for matching context:
     * - +10 for same game
     * - +5 for same engine
     * - +5 for same category
     * - -10 for different context
     *
     * @param source Source text
     * @param candidate Candidate text
     * @param sourceCtx Source context (optional)
     * @param candidateCtx Candidate context (optional)
     * @return Similarity score (0.0 - 100.0)
     */
    static double calculateSimilarityScore(
        const std::string& source,
        const std::string& candidate,
        const MatchContext& sourceCtx = {},
        const MatchContext& candidateCtx = {}
    );

    // ============== DATABASE OPERATIONS ==============

    /**
     * @brief Add entry to translation memory
     *
     * @param entry TM entry to add
     * @return ID of inserted entry or error
     */
    static Result<int64_t> addEntry(const TranslationMemoryEntry& entry);

    /**
     * @brief Add n-grams for a TM entry
     *
     * @param tmId TM entry ID
     * @param sourceText Source text to generate n-grams from
     * @return Success or error
     */
    static Result<void> addNgrams(int64_t tmId, const std::string& sourceText);

    /**
     * @brief Find exact match in translation memory
     *
     * @param sourceText Source text to match
     * @param context Optional context filter
     * @param targetLang Target language (default: "tr")
     * @return TM entry with 100% match score, or nullopt
     */
    static Result<std::optional<TranslationMemoryEntry>> findExactMatch(
        const std::string& sourceText,
        const std::optional<std::string>& context = std::nullopt,
        const std::string& targetLang = "tr"
    );

    /**
     * @brief Find fuzzy matches in translation memory
     *
     * Uses n-gram indexing for efficient candidate retrieval,
     * then calculates hybrid similarity scores.
     *
     * @param sourceText Source text to match
     * @param ctx Match context (game, engine, category)
     * @param targetLang Target language (default: "tr")
     * @param limit Maximum results (default: 5)
     * @param minScore Minimum similarity score (default: TM_MIN_FUZZY_SCORE)
     * @return List of TM entries with match scores
     */
    static Result<std::vector<TMMatch>> findFuzzyMatches(
        const std::string& sourceText,
        const MatchContext& ctx = {},
        const std::string& targetLang = "tr",
        size_t limit = 5,
        double minScore = TM_MIN_FUZZY_SCORE
    );

    /**
     * @brief Find best matching entry
     *
     * @param sourceText Source text to match
     * @param ctx Match context
     * @param targetLang Target language
     * @param minScore Minimum score threshold
     * @return Best TM match or nullopt
     */
    static Result<std::optional<TMMatch>> findBestMatch(
        const std::string& sourceText,
        const MatchContext& ctx = {},
        const std::string& targetLang = "tr",
        double minScore = TM_MIN_FUZZY_SCORE
    );

    /**
     * @brief Find matches for multiple source texts
     *
     * @param sourceTexts List of source texts
     * @param ctx Match context
     * @param targetLang Target language
     * @param minScore Minimum score threshold
     * @return Map of source text to best match
     */
    static Result<std::vector<std::pair<std::string, std::optional<TMMatch>>>> findBatchMatches(
        const std::vector<std::string>& sourceTexts,
        const MatchContext& ctx = {},
        const std::string& targetLang = "tr",
        double minScore = TM_MIN_FUZZY_SCORE
    );

    /**
     * @brief Update TM entry
     *
     * @param tmId Entry ID
     * @param targetText New translation
     * @param qualityScore Optional new quality score
     * @param verified Optional verification status
     * @return Success or error
     */
    static Result<void> updateEntry(
        int64_t tmId,
        const std::string& targetText,
        std::optional<int> qualityScore = std::nullopt,
        std::optional<bool> verified = std::nullopt
    );

    /**
     * @brief Delete TM entry
     *
     * @param tmId Entry ID to delete
     * @return Success or error
     */
    static Result<void> deleteEntry(int64_t tmId);

    /**
     * @brief Clear all TM data
     *
     * @return Success or error
     */
    static Result<void> clearAll();

    /**
     * @brief Get TM entries for a specific game
     *
     * @param gameId Game ID
     * @param targetLang Target language
     * @param limit Maximum results
     * @param offset Skip first N results
     * @return List of TM entries
     */
    static Result<std::vector<TranslationMemoryEntry>> getEntriesForGame(
        const std::string& gameId,
        const std::string& targetLang = "tr",
        size_t limit = 1000,
        size_t offset = 0
    );

    /**
     * @brief TM statistics
     */
    struct TMStats {
        int64_t totalEntries = 0;
        int64_t verifiedEntries = 0;
        double averageQuality = 0.0;
    };

    /**
     * @brief Get TM statistics
     *
     * @return TMStats or error
     */
    static Result<TMStats> getStats();

    /**
     * @brief Import TM entries from list
     *
     * @param entries List of entries to import
     * @return Number of successfully imported entries
     */
    static Result<int> importEntries(const std::vector<TranslationMemoryEntry>& entries);

    /**
     * @brief Export TM entries
     *
     * @param gameId Optional game filter
     * @param targetLang Target language
     * @return List of exportable entry maps
     */
    static Result<std::vector<TranslationMemoryEntry>> exportEntries(
        const std::optional<std::string>& gameId = std::nullopt,
        const std::string& targetLang = "tr"
    );

private:
    /**
     * @brief Determine match type from score
     */
    static MatchType scoreToMatchType(double score);
};

} // namespace makineai
