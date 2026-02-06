/**
 * @file translation_memory.cpp
 * @brief Translation Memory service implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/translation_memory.hpp"
#include "makineai/database.hpp"
#include "makineai/core.hpp"
#include "makineai/features.hpp"

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

// Optional: digestpp for fast hashing (alternative to OpenSSL MD5)
#ifdef MAKINEAI_HAS_DIGESTPP
// TODO: [DIGESTPP] Replace OpenSSL MD5 with digestpp for header-only hash
// Example:
//   #include <digestpp.hpp>
//   std::string hash = digestpp::md5().absorb(text).hexdigest();
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

    // Create entry with hash
    TranslationMemoryEntry entryWithHash = entry;
    entryWithHash.sourceHash = hashText(entry.sourceText);

    auto result = db.addToTranslationMemory(entryWithHash);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Add n-grams
    auto ngramResult = addNgrams(*result, entry.sourceText);
    if (!ngramResult) {
        logger()->warn("Failed to add n-grams for TM entry {}: {}",
            *result, ngramResult.error().message());
    }

    logger()->debug("TM entry added: ID={}", *result);
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
    std::string hash = hashText(sourceText);

    auto result = Database::instance().findExactMatch(hash);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (result->has_value()) {
        auto& entry = **result;

        // Filter by target language
        if (entry.targetLang != targetLang) {
            return std::nullopt;
        }

        // Filter by context if specified
        if (context.has_value() && entry.context.has_value() &&
            *context != *entry.context) {
            return std::nullopt;
        }

        // Increment usage count
        if (entry.id.has_value()) {
            Database::instance().incrementTMUsage(*entry.id);
        }

        return entry;
    }

    return std::nullopt;
}

Result<std::vector<TMMatch>> TranslationMemoryService::findFuzzyMatches(
    const std::string& sourceText,
    const MatchContext& ctx,
    const std::string& targetLang,
    size_t limit,
    double minScore
) {
    // First try exact match
    auto exactResult = findExactMatch(sourceText, ctx.context, targetLang);
    if (exactResult && exactResult->has_value()) {
        TMMatch match;
        match.entry = **exactResult;
        match.similarity = 100.0;
        match.matchType = MatchType::Exact;
        return std::vector<TMMatch>{match};
    }

    // Generate n-grams for candidate retrieval
    auto sourceNgrams = generateNgrams(sourceText);
    if (sourceNgrams.empty()) {
        return std::vector<TMMatch>{};
    }

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

    // Filter candidates with minimum n-gram matches
    std::vector<int64_t> candidateIds;
    for (const auto& [tmId, count] : candidateCounts) {
        if (count >= minNgramMatches) {
            candidateIds.push_back(tmId);
        }
    }

    if (candidateIds.empty()) {
        return std::vector<TMMatch>{};
    }

    // Get TM entries for candidates
    std::vector<TMMatch> scoredMatches;

    for (int64_t tmId : candidateIds) {
        // Get entry by searching for it
        auto entriesResult = db.findByHash("");  // We need to get by ID instead
        // For now, we'll use the hash lookup approach
        // TODO: Add getEntryById to Database

        // Alternative: get entries for game and filter
        // This is a simplified approach - in production, add proper ID lookup
    }

    // Simplified approach: get all entries and score them
    // (In production, this should use proper indexed lookup)
    auto allEntries = db.getEntriesForGame(
        ctx.gameId.value_or(""), 1000);

    if (!allEntries) {
        return std::vector<TMMatch>{};
    }

    // Score candidates
    MatchContext candidateCtx;
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

    return scoredMatches;
}

Result<std::optional<TMMatch>> TranslationMemoryService::findBestMatch(
    const std::string& sourceText,
    const MatchContext& ctx,
    const std::string& targetLang,
    double minScore
) {
    auto matches = findFuzzyMatches(sourceText, ctx, targetLang, 1, minScore);
    if (!matches) {
        return std::unexpected(matches.error());
    }

    if (matches->empty()) {
        return std::nullopt;
    }

    return matches->front();
}

Result<std::vector<std::pair<std::string, std::optional<TMMatch>>>>
TranslationMemoryService::findBatchMatches(
    const std::vector<std::string>& sourceTexts,
    const MatchContext& ctx,
    const std::string& targetLang,
    double minScore
) {
    std::vector<std::pair<std::string, std::optional<TMMatch>>> results;
    results.reserve(sourceTexts.size());

#ifdef MAKINEAI_HAS_TASKFLOW
    // TODO: [TASKFLOW] Parallel batch matching for large translation files
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
    logger()->debug("Taskflow available - parallel batch matching enabled");
#endif

#ifdef MAKINEAI_HAS_CONCURRENTQUEUE
    // TODO: [CONCURRENTQUEUE] Producer-consumer pattern for streaming translations
    // Use lock-free queue for real-time translation processing
    logger()->debug("concurrentqueue available for streaming translations");
#endif

    for (const auto& text : sourceTexts) {
        auto match = findBestMatch(text, ctx, targetLang, minScore);
        if (match) {
            results.emplace_back(text, *match);
        } else {
            results.emplace_back(text, std::nullopt);
        }
    }

    return results;
}

Result<void> TranslationMemoryService::updateEntry(
    int64_t tmId,
    const std::string& targetText,
    std::optional<int> qualityScore,
    std::optional<bool> verified
) {
    // Build update through database
    // For now, we need to add an update method to Database
    // This is a simplified implementation
    logger()->debug("Updating TM entry {}", tmId);

    // TODO: Implement proper update in Database class
    return {};
}

Result<void> TranslationMemoryService::deleteEntry(int64_t tmId) {
    // TODO: Add delete method to Database class
    logger()->debug("Deleting TM entry {}", tmId);
    return {};
}

Result<void> TranslationMemoryService::clearAll() {
    logger()->info("Clearing all TM data");

    auto& db = Database::instance();

    // Execute raw SQL to clear tables
    auto result1 = db.executeRaw("DELETE FROM tm_ngrams");
    auto result2 = db.executeRaw("DELETE FROM tm_variants");
    auto result3 = db.executeRaw("DELETE FROM translation_memory");

    if (!result1 || !result2 || !result3) {
        return std::unexpected(Error(ErrorCode::DatabaseError,
            "Failed to clear TM data"));
    }

    logger()->info("All TM data cleared");
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
    auto statsResult = Database::instance().getTranslationMemoryStats();
    if (!statsResult) {
        return std::unexpected(statsResult.error());
    }

    TMStats stats;
    stats.totalEntries = statsResult->first;
    stats.verifiedEntries = statsResult->second;
    stats.averageQuality = 0.0;  // TODO: Calculate from database

    return stats;
}

Result<int> TranslationMemoryService::importEntries(
    const std::vector<TranslationMemoryEntry>& entries
) {
    int imported = 0;

    for (const auto& entry : entries) {
        auto result = addEntry(entry);
        if (result) {
            ++imported;
        } else {
            logger()->warn("TM import error: {}", result.error().message());
        }
    }

    logger()->info("Imported {} TM entries", imported);
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
