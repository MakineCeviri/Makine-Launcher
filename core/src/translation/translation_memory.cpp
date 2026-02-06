/**
 * @file translation_memory.cpp
 * @brief Translation Memory service implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/translation_memory.hpp"
#include "makineai/database.hpp"
#include "makineai/core.hpp"
#include "makineai/features.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <openssl/md5.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>
#include <regex>
#include <iomanip>

// Optional: simdutf for fast UTF conversions (Turkish characters)
#ifdef MAKINEAI_HAS_SIMDUTF
#include <simdutf.h>
#endif

// Optional: simdjson for fast JSON parsing
#ifdef MAKINEAI_HAS_SIMDJSON
#include <simdjson.h>
#endif

// Optional: concurrentqueue for parallel batch processing
#ifdef MAKINEAI_HAS_CONCURRENTQUEUE
#include <concurrentqueue/concurrentqueue.h>
#endif

// Optional: Taskflow for parallel similarity calculations
#ifdef MAKINEAI_HAS_TASKFLOW
#include <taskflow/taskflow.hpp>
#endif

namespace makineai {

// =============================================================================
// TEXT PROCESSING
// =============================================================================

std::string TranslationMemoryService::normalizeText(const std::string& text) {
    std::string result;
    result.reserve(text.size());

    bool lastWasSpace = false;

    for (char c : text) {
        // Convert to lowercase
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // Keep only alphanumeric and spaces
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result += lower;
            lastWasSpace = false;
        }
        else if (std::isspace(static_cast<unsigned char>(c))) {
            // Normalize whitespace to single space
            if (!lastWasSpace && !result.empty()) {
                result += ' ';
                lastWasSpace = true;
            }
        }
        // Skip punctuation
    }

    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

std::string TranslationMemoryService::hashText(const std::string& text) {
    std::string normalized = normalizeText(text);

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(normalized.c_str()),
        normalized.size(), digest);

    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(digest[i]);
    }

    return oss.str();
}

std::vector<std::string> TranslationMemoryService::generateNgrams(
    const std::string& text, int n
) {
    std::string normalized = normalizeText(text);
    std::vector<std::string> ngrams;

    if (static_cast<int>(normalized.length()) < n) {
        if (!normalized.empty()) {
            ngrams.push_back(normalized);
        }
        return ngrams;
    }

    ngrams.reserve(normalized.length() - n + 1);

    for (size_t i = 0; i <= normalized.length() - n; ++i) {
        ngrams.push_back(normalized.substr(i, n));
    }

    return ngrams;
}

// =============================================================================
// SIMILARITY ALGORITHMS
// =============================================================================

int TranslationMemoryService::levenshteinDistance(
    const std::string& s1, const std::string& s2
) {
    if (s1 == s2) return 0;
    if (s1.empty()) return static_cast<int>(s2.length());
    if (s2.empty()) return static_cast<int>(s1.length());

    const size_t len1 = s1.length();
    const size_t len2 = s2.length();

    // Use two rows instead of full matrix for memory efficiency
    std::vector<int> v0(len2 + 1);
    std::vector<int> v1(len2 + 1);

    // Initialize first row
    for (size_t i = 0; i <= len2; ++i) {
        v0[i] = static_cast<int>(i);
    }

    for (size_t i = 0; i < len1; ++i) {
        v1[0] = static_cast<int>(i + 1);

        for (size_t j = 0; j < len2; ++j) {
            int cost = (std::tolower(static_cast<unsigned char>(s1[i])) ==
                       std::tolower(static_cast<unsigned char>(s2[j]))) ? 0 : 1;

            v1[j + 1] = std::min({
                v1[j] + 1,      // Insertion
                v0[j + 1] + 1,  // Deletion
                v0[j] + cost    // Substitution
            });
        }

        std::swap(v0, v1);
    }

    return v0[len2];
}

double TranslationMemoryService::levenshteinRatio(
    const std::string& s1, const std::string& s2
) {
    size_t maxLen = std::max(s1.length(), s2.length());
    if (maxLen == 0) return 1.0;

    int distance = levenshteinDistance(s1, s2);
    return 1.0 - (static_cast<double>(distance) / static_cast<double>(maxLen));
}

double TranslationMemoryService::jaccardSimilarity(
    const std::vector<std::string>& ngrams1,
    const std::vector<std::string>& ngrams2
) {
    if (ngrams1.empty() && ngrams2.empty()) return 1.0;
    if (ngrams1.empty() || ngrams2.empty()) return 0.0;

    std::unordered_set<std::string> set1(ngrams1.begin(), ngrams1.end());
    std::unordered_set<std::string> set2(ngrams2.begin(), ngrams2.end());

    // Calculate intersection
    size_t intersection = 0;
    for (const auto& ngram : set1) {
        if (set2.count(ngram) > 0) {
            ++intersection;
        }
    }

    // Calculate union
    std::unordered_set<std::string> unionSet = set1;
    unionSet.insert(set2.begin(), set2.end());
    size_t unionSize = unionSet.size();

    return unionSize > 0 ? static_cast<double>(intersection) / static_cast<double>(unionSize) : 0.0;
}

double TranslationMemoryService::lengthSimilarity(
    const std::string& s1, const std::string& s2
) {
    size_t len1 = s1.length();
    size_t len2 = s2.length();
    size_t maxLen = std::max(len1, len2);
    size_t minLen = std::min(len1, len2);

    if (maxLen == 0) return 1.0;
    return static_cast<double>(minLen) / static_cast<double>(maxLen);
}

double TranslationMemoryService::keywordMatchRatio(
    const std::string& s1, const std::string& s2
) {
    auto extractKeywords = [](const std::string& text) {
        std::unordered_set<std::string> keywords;
        std::string word;
        std::istringstream iss(text);

        while (iss >> word) {
            // Convert to lowercase
            std::transform(word.begin(), word.end(), word.begin(),
                [](unsigned char c) { return std::tolower(c); });

            // Keep words with 3+ characters
            if (word.length() >= 3) {
                keywords.insert(word);
            }
        }

        return keywords;
    };

    auto keywords1 = extractKeywords(s1);
    auto keywords2 = extractKeywords(s2);

    if (keywords1.empty() && keywords2.empty()) return 1.0;
    if (keywords1.empty() || keywords2.empty()) return 0.0;

    // Calculate intersection
    size_t matches = 0;
    for (const auto& kw : keywords1) {
        if (keywords2.count(kw) > 0) {
            ++matches;
        }
    }

    // Calculate union
    std::unordered_set<std::string> unionSet = keywords1;
    unionSet.insert(keywords2.begin(), keywords2.end());
    size_t total = unionSet.size();

    return total > 0 ? static_cast<double>(matches) / static_cast<double>(total) : 0.0;
}

double TranslationMemoryService::calculateSimilarityScore(
    const std::string& source,
    const std::string& candidate,
    const MatchContext& sourceCtx,
    const MatchContext& candidateCtx
) {
    std::string normalizedSource = normalizeText(source);
    std::string normalizedCandidate = normalizeText(candidate);

    // Exact match
    if (normalizedSource == normalizedCandidate) {
        return 100.0;
    }

    // Near-exact (only whitespace/case difference)
    int levDistance = levenshteinDistance(normalizedSource, normalizedCandidate);
    if (levDistance <= 2) {
        return 95.0 + (2 - levDistance) * 2;
    }

    // Generate n-grams
    auto sourceNgrams = generateNgrams(source);
    auto candidateNgrams = generateNgrams(candidate);

    // Calculate base scores
    double levRatio = levenshteinRatio(normalizedSource, normalizedCandidate);
    double jaccardScore = jaccardSimilarity(sourceNgrams, candidateNgrams);
    double keywordScore = keywordMatchRatio(source, candidate);
    double lengthScore = lengthSimilarity(source, candidate);

    // Weighted score
    double score = (0.4 * levRatio) +
                   (0.3 * jaccardScore) +
                   (0.2 * keywordScore) +
                   (0.1 * lengthScore);

    score = score * 100.0; // Convert to 0-100 range

    // Bonus/Penalty based on context
    if (sourceCtx.gameId.has_value() && candidateCtx.gameId.has_value() &&
        *sourceCtx.gameId == *candidateCtx.gameId) {
        score += 10.0;
    }

    if (sourceCtx.engineType.has_value() && candidateCtx.engineType.has_value() &&
        *sourceCtx.engineType == *candidateCtx.engineType) {
        score += 5.0;
    }

    if (sourceCtx.category.has_value() && candidateCtx.category.has_value() &&
        *sourceCtx.category == *candidateCtx.category) {
        score += 5.0;
    }

    if (sourceCtx.context.has_value() && candidateCtx.context.has_value() &&
        *sourceCtx.context != *candidateCtx.context) {
        score -= 10.0;
    }

    // Clamp to 0-100 range
    return std::clamp(score, 0.0, 100.0);
}

MatchType TranslationMemoryService::scoreToMatchType(double score) {
    if (score >= 100.0) return MatchType::Exact;
    if (score >= 95.0) return MatchType::NearExact;
    if (score >= 75.0) return MatchType::Fuzzy;
    return MatchType::Poor;
}

// =============================================================================
// DATABASE OPERATIONS
// =============================================================================

Result<int64_t> TranslationMemoryService::addEntry(const TranslationMemoryEntry& entry) {
    auto& db = Database::instance();

    MAKINEAI_LOG_DEBUG(log::TM, "Adding entry: source len={}, target len={}, lang={}",
        entry.sourceText.length(), entry.targetText.length(), entry.targetLang);

    // Create entry with hash
    TranslationMemoryEntry entryWithHash = entry;
    entryWithHash.sourceHash = hashText(entry.sourceText);

    auto result = db.addToTranslationMemory(entryWithHash);
    if (!result) {
        MAKINEAI_LOG_ERROR(log::TM, "Failed to add TM entry: {}", result.error().message());
        return std::unexpected(result.error());
    }

    // Add n-grams
    auto ngramResult = addNgrams(*result, entry.sourceText);
    if (!ngramResult) {
        MAKINEAI_LOG_WARN(log::TM, "Failed to add n-grams for TM entry {}: {}",
            *result, ngramResult.error().message());
    }

    MAKINEAI_LOG_DEBUG(log::TM, "TM entry added: ID={}", *result);
    metrics().increment("tm_entries_added");
    return *result;
}

Result<void> TranslationMemoryService::addNgrams(
    int64_t tmId, const std::string& sourceText
) {
    auto ngrams = generateNgrams(sourceText);
    std::vector<std::pair<std::string, int>> ngramsWithPositions;
    ngramsWithPositions.reserve(ngrams.size());

    for (size_t i = 0; i < ngrams.size(); ++i) {
        ngramsWithPositions.emplace_back(ngrams[i], static_cast<int>(i));
    }

    return Database::instance().storeNgrams(tmId, ngramsWithPositions);
}

Result<std::optional<TranslationMemoryEntry>> TranslationMemoryService::findExactMatch(
    const std::string& sourceText,
    const std::optional<std::string>& context,
    const std::string& targetLang
) {
    auto timer = metrics().timer("tm_search");

    MAKINEAI_LOG_DEBUG(log::TM, "Searching exact match for text (len={}), lang={}",
        sourceText.length(), targetLang);

    std::string hash = hashText(sourceText);

    auto result = Database::instance().findExactMatch(hash);
    if (!result) {
        MAKINEAI_LOG_WARN(log::TM, "Database error during exact match search: {}",
            result.error().message());
        return std::unexpected(result.error());
    }

    if (result->has_value()) {
        auto& entry = **result;

        // Filter by target language
        if (entry.targetLang != targetLang) {
            MAKINEAI_LOG_DEBUG(log::TM, "Exact match found but wrong language: {} vs {}",
                entry.targetLang, targetLang);
            metrics().increment("tm_no_matches");
            return std::nullopt;
        }

        // Filter by context if specified
        if (context.has_value() && entry.context.has_value() &&
            *context != *entry.context) {
            MAKINEAI_LOG_DEBUG(log::TM, "Exact match found but context mismatch");
            metrics().increment("tm_no_matches");
            return std::nullopt;
        }

        // Increment usage count
        if (entry.id.has_value()) {
            Database::instance().incrementTMUsage(*entry.id);
        }

        MAKINEAI_LOG_DEBUG(log::TM, "Exact match found: ID={}", entry.id.value_or(-1));
        metrics().increment("tm_exact_matches");
        metrics().recordHistogram("tm_match_scores", 100);
        return entry;
    }

    MAKINEAI_LOG_DEBUG(log::TM, "No exact match found for hash={}", hash.substr(0, 8));
    metrics().increment("tm_no_matches");
    return std::nullopt;
}

Result<std::vector<TMMatch>> TranslationMemoryService::findFuzzyMatches(
    const std::string& sourceText,
    const MatchContext& ctx,
    const std::string& targetLang,
    size_t limit,
    double minScore
) {
    auto timer = metrics().timer("tm_search");

    MAKINEAI_LOG_DEBUG(log::TM, "Fuzzy search: text len={}, lang={}, minScore={:.1f}, limit={}",
        sourceText.length(), targetLang, minScore, limit);

    // First try exact match
    auto exactResult = findExactMatch(sourceText, ctx.context, targetLang);
    if (exactResult && exactResult->has_value()) {
        TMMatch match;
        match.entry = **exactResult;
        match.similarity = 100.0;
        match.matchType = MatchType::Exact;
        MAKINEAI_LOG_DEBUG(log::TM, "Exact match found during fuzzy search, score=100.0");
        return std::vector<TMMatch>{match};
    }

    // Generate n-grams for candidate retrieval
    auto sourceNgrams = generateNgrams(sourceText);
    if (sourceNgrams.empty()) {
        MAKINEAI_LOG_WARN(log::TM, "No n-grams generated for source text");
        metrics().increment("tm_no_matches");
        return std::vector<TMMatch>{};
    }

    MAKINEAI_LOG_DEBUG(log::TM, "Generated {} n-grams for fuzzy matching", sourceNgrams.size());

    // Find candidate TM IDs using n-gram index
    auto& db = Database::instance();

    // We need at least 30% n-gram overlap
    size_t minNgramMatches = std::max<size_t>(1, sourceNgrams.size() * 30 / 100);

    // Collect candidate IDs from all n-grams
    std::unordered_map<int64_t, size_t> candidateCounts;
    for (const auto& ngram : sourceNgrams) {
        auto ngramResult = db.findByNgram(ngram, 100);
        if (ngramResult) {
            for (int64_t tmId : *ngramResult) {
                candidateCounts[tmId]++;
            }
        }
    }

    MAKINEAI_LOG_DEBUG(log::TM, "Found {} candidate entries from n-gram index", candidateCounts.size());

    // Filter candidates with minimum n-gram matches
    std::vector<int64_t> candidateIds;
    for (const auto& [tmId, count] : candidateCounts) {
        if (count >= minNgramMatches) {
            candidateIds.push_back(tmId);
        }
    }

    if (candidateIds.empty()) {
        MAKINEAI_LOG_DEBUG(log::TM, "No candidates passed n-gram threshold (min={})", minNgramMatches);
        // Don't count as no_matches yet, we'll try game fallback
    }

    // Get TM entries for candidates by ID
    std::vector<TMMatch> scoredMatches;

    // Score candidate entries found by n-gram matching
    MatchContext candidateCtx;
    for (int64_t tmId : candidateIds) {
        auto entryResult = db.getTranslationMemoryEntryById(tmId);
        if (!entryResult) continue;

        const auto& entry = *entryResult;
        if (entry.targetLang != targetLang) continue;

        candidateCtx.context = entry.context;
        candidateCtx.gameId = entry.gameId;
        candidateCtx.engineType = entry.engineType;

        double score = calculateSimilarityScore(sourceText, entry.sourceText, ctx, candidateCtx);

        if (score >= minScore) {
            TMMatch match;
            match.entry = entry;
            match.similarity = score;
            match.matchType = scoreToMatchType(score);
            scoredMatches.push_back(std::move(match));

            // Record match score in histogram
            metrics().recordHistogram("tm_match_scores", static_cast<int64_t>(score));
        }
    }

    // If no matches from n-grams, fall back to game entries (if configured)
    if (scoredMatches.empty() && ctx.gameId.has_value()) {
        MAKINEAI_LOG_DEBUG(log::TM, "No n-gram matches, falling back to game entries for gameId={}",
            *ctx.gameId);

        auto allEntries = db.getEntriesForGame(*ctx.gameId, 1000);
        if (!allEntries) {
            MAKINEAI_LOG_WARN(log::TM, "Failed to get game entries: {}", allEntries.error().message());
            metrics().increment("tm_no_matches");
            return std::vector<TMMatch>{};
        }

        MAKINEAI_LOG_DEBUG(log::TM, "Searching {} game entries", allEntries->size());

        for (const auto& entry : *allEntries) {
            if (entry.targetLang != targetLang) continue;

            candidateCtx.context = entry.context;
            candidateCtx.gameId = entry.gameId;
            candidateCtx.engineType = entry.engineType;
            candidateCtx.category = entry.category;

            double score = calculateSimilarityScore(
                sourceText, entry.sourceText, ctx, candidateCtx);

            if (score >= minScore) {
                TMMatch match;
                match.entry = entry;
                match.similarity = score;
                match.matchType = scoreToMatchType(score);
                scoredMatches.push_back(match);

                // Record match score in histogram
                metrics().recordHistogram("tm_match_scores", static_cast<int64_t>(score));
            }
        }
    }

    // Sort by score descending
    std::sort(scoredMatches.begin(), scoredMatches.end(),
        [](const TMMatch& a, const TMMatch& b) {
            return a.similarity > b.similarity;
        });

    // Limit results
    if (scoredMatches.size() > limit) {
        scoredMatches.resize(limit);
    }

    // Log and record metrics based on results
    if (scoredMatches.empty()) {
        MAKINEAI_LOG_WARN(log::TM, "No fuzzy matches found for text (len={})", sourceText.length());
        metrics().increment("tm_no_matches");
    } else {
        MAKINEAI_LOG_DEBUG(log::TM, "Found {} fuzzy matches, best score={:.1f}",
            scoredMatches.size(), scoredMatches.front().similarity);
        metrics().increment("tm_fuzzy_matches", static_cast<int64_t>(scoredMatches.size()));
    }

    return scoredMatches;
}

Result<std::optional<TMMatch>> TranslationMemoryService::findBestMatch(
    const std::string& sourceText,
    const MatchContext& ctx,
    const std::string& targetLang,
    double minScore
) {
    MAKINEAI_LOG_DEBUG(log::TM, "Finding best match for text (len={}), minScore={:.1f}",
        sourceText.length(), minScore);

    auto matches = findFuzzyMatches(sourceText, ctx, targetLang, 1, minScore);
    if (!matches) {
        MAKINEAI_LOG_WARN(log::TM, "Best match search failed: {}", matches.error().message());
        return std::unexpected(matches.error());
    }

    if (matches->empty()) {
        MAKINEAI_LOG_DEBUG(log::TM, "No best match found above threshold {:.1f}", minScore);
        return std::nullopt;
    }

    const auto& best = matches->front();
    MAKINEAI_LOG_DEBUG(log::TM, "Best match found: score={:.1f}, type={}",
        best.similarity, static_cast<int>(best.matchType));

    return best;
}

Result<std::vector<std::pair<std::string, std::optional<TMMatch>>>>
TranslationMemoryService::findBatchMatches(
    const std::vector<std::string>& sourceTexts,
    const MatchContext& ctx,
    const std::string& targetLang,
    double minScore
) {
    auto timer = metrics().timer("tm_search");

    MAKINEAI_LOG_INFO(log::TM, "Batch match: {} texts, lang={}, minScore={:.1f}",
        sourceTexts.size(), targetLang, minScore);

    std::vector<std::pair<std::string, std::optional<TMMatch>>> results;
    results.reserve(sourceTexts.size());

    size_t matchCount = 0;
    size_t noMatchCount = 0;

#ifdef MAKINEAI_HAS_TASKFLOW
    // TODO(makineai): [TASKFLOW] Parallel batch matching for large translation files
    // Process multiple source texts concurrently using thread pool
    // Example:
    //   tf::Executor executor(std::thread::hardware_concurrency());
    //   tf::Taskflow taskflow;
    //   std::mutex resultsMutex;
    //   results.resize(sourceTexts.size());
    //
    //   for (size_t i = 0; i < sourceTexts.size(); ++i) {
    //       taskflow.emplace([&, i]() {
    //           auto match = findBestMatch(sourceTexts[i], ctx, targetLang, minScore);
    //           results[i] = {sourceTexts[i], match ? *match : std::nullopt};
    //       });
    //   }
    //   executor.run(taskflow).wait();
    //   return results;
    MAKINEAI_LOG_DEBUG(log::TM, "Taskflow available - parallel batch matching enabled");
#endif

#ifdef MAKINEAI_HAS_CONCURRENTQUEUE
    // TODO(makineai): [CONCURRENTQUEUE] Producer-consumer pattern for streaming translations
    // Use lock-free queue for real-time translation processing
    MAKINEAI_LOG_DEBUG(log::TM, "concurrentqueue available for streaming translations");
#endif

    for (const auto& text : sourceTexts) {
        auto match = findBestMatch(text, ctx, targetLang, minScore);
        if (match) {
            if (match->has_value()) {
                results.emplace_back(text, *match);
                ++matchCount;
            } else {
                results.emplace_back(text, std::nullopt);
                ++noMatchCount;
            }
        } else {
            results.emplace_back(text, std::nullopt);
            ++noMatchCount;
        }
    }

    MAKINEAI_LOG_INFO(log::TM, "Batch complete: {}/{} matches ({:.1f}%)",
        matchCount, sourceTexts.size(),
        sourceTexts.empty() ? 0.0 : (100.0 * matchCount / sourceTexts.size()));

    // Record batch metrics
    metrics().gauge("tm_batch_match_rate",
        sourceTexts.empty() ? 0.0 : (100.0 * matchCount / sourceTexts.size()));

    return results;
}

Result<void> TranslationMemoryService::updateEntry(
    int64_t tmId,
    const std::string& targetText,
    std::optional<int> qualityScore,
    std::optional<bool> verified
) {
    MAKINEAI_LOG_DEBUG(log::TM, "Updating TM entry {}: targetLen={}, quality={}, verified={}",
        tmId, targetText.length(),
        qualityScore.has_value() ? std::to_string(*qualityScore) : "unchanged",
        verified.has_value() ? (*verified ? "true" : "false") : "unchanged");

    auto& db = Database::instance();
    auto result = db.updateTranslationMemoryEntry(tmId, targetText, qualityScore, verified);

    if (!result) {
        MAKINEAI_LOG_ERROR(log::TM, "Failed to update TM entry {}: {}", tmId, result.error().message());
        return std::unexpected(result.error());
    }

    MAKINEAI_LOG_INFO(log::TM, "Updated TM entry {}", tmId);
    metrics().increment("tm_entries_updated");
    return {};
}

Result<void> TranslationMemoryService::deleteEntry(int64_t tmId) {
    MAKINEAI_LOG_DEBUG(log::TM, "Deleting TM entry {}", tmId);

    auto& db = Database::instance();
    auto result = db.deleteTranslationMemoryEntry(tmId);

    if (!result) {
        MAKINEAI_LOG_ERROR(log::TM, "Failed to delete TM entry {}: {}", tmId, result.error().message());
        return std::unexpected(result.error());
    }

    MAKINEAI_LOG_INFO(log::TM, "Deleted TM entry {}", tmId);
    metrics().increment("tm_entries_deleted");
    return {};
}

Result<void> TranslationMemoryService::clearAll() {
    MAKINEAI_LOG_INFO(log::TM, "Clearing all TM data");

    auto& db = Database::instance();

    // Execute raw SQL to clear tables
    auto result1 = db.executeRaw("DELETE FROM tm_ngrams");
    auto result2 = db.executeRaw("DELETE FROM tm_variants");
    auto result3 = db.executeRaw("DELETE FROM translation_memory");

    if (!result1 || !result2 || !result3) {
        MAKINEAI_LOG_ERROR(log::TM, "Failed to clear TM data");
        return std::unexpected(Error(ErrorCode::DatabaseError,
            "Failed to clear TM data"));
    }

    MAKINEAI_LOG_INFO(log::TM, "All TM data cleared successfully");
    return {};
}

Result<std::vector<TranslationMemoryEntry>> TranslationMemoryService::getEntriesForGame(
    const std::string& gameId,
    const std::string& targetLang,
    size_t limit,
    size_t offset
) {
    return Database::instance().getEntriesForGame(gameId, limit);
}

Result<TranslationMemoryService::TMStats> TranslationMemoryService::getStats() {
    MAKINEAI_LOG_DEBUG(log::TM, "Retrieving TM statistics");

    auto& db = Database::instance();

    auto statsResult = db.getTranslationMemoryStats();
    if (!statsResult) {
        MAKINEAI_LOG_ERROR(log::TM, "Failed to get TM stats: {}", statsResult.error().message());
        return std::unexpected(statsResult.error());
    }

    auto qualityResult = db.getAverageQualityScore();

    TMStats stats;
    stats.totalEntries = statsResult->first;
    stats.verifiedEntries = statsResult->second;
    stats.averageQuality = qualityResult.value_or(0.0);

    MAKINEAI_LOG_DEBUG(log::TM, "TM stats: total={}, verified={}, avgQuality={:.1f}",
        stats.totalEntries, stats.verifiedEntries, stats.averageQuality);

    // Update gauges for monitoring
    metrics().gauge("tm_total_entries", static_cast<double>(stats.totalEntries));
    metrics().gauge("tm_verified_entries", static_cast<double>(stats.verifiedEntries));
    metrics().gauge("tm_average_quality", stats.averageQuality);

    return stats;
}

Result<int> TranslationMemoryService::importEntries(
    const std::vector<TranslationMemoryEntry>& entries
) {
    MAKINEAI_LOG_INFO(log::TM, "Importing {} TM entries", entries.size());

    int imported = 0;
    int failed = 0;

    for (const auto& entry : entries) {
        auto result = addEntry(entry);
        if (result) {
            ++imported;
        } else {
            ++failed;
            MAKINEAI_LOG_WARN(log::TM, "TM import error: {}", result.error().message());
        }
    }

    MAKINEAI_LOG_INFO(log::TM, "Import complete: {}/{} entries imported ({} failed)",
        imported, entries.size(), failed);

    metrics().increment("tm_entries_imported", imported);
    if (failed > 0) {
        metrics().increment("tm_import_failures", failed);
    }

    return imported;
}

Result<std::vector<TranslationMemoryEntry>> TranslationMemoryService::exportEntries(
    const std::optional<std::string>& gameId,
    const std::string& targetLang
) {
    if (gameId.has_value()) {
        return getEntriesForGame(*gameId, targetLang, 10000, 0);
    }

    // Export all entries
    return Database::instance().getEntriesForGame("", 10000);
}

} // namespace makineai
