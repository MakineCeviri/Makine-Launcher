/**
 * @file types/tm_types.hpp
 * @brief Translation memory type definitions
 * @copyright (c) 2026 MakineAI Team
 *
 * This file contains types for translation memory entries and match results.
 */

#pragma once

#include "makineai/types/common.hpp"

#include <optional>

namespace makineai {

// ============================================================================
// Match Types
// ============================================================================

/**
 * @brief Match type for translation memory
 *
 * Categorizes how closely a source text matches a TM entry.
 */
enum class MatchType {
    Exact,          ///< 100% match
    NearExact,      ///< 95-99% match
    Fuzzy,          ///< 75-94% match
    Poor            ///< <75% match
};

/**
 * @brief Convert match type to minimum similarity threshold
 * @param type The match type
 * @return Minimum similarity percentage for this match type
 */
[[nodiscard]] constexpr double matchTypeThreshold(MatchType type) noexcept {
    switch (type) {
        case MatchType::Exact:     return 100.0;
        case MatchType::NearExact: return 95.0;
        case MatchType::Fuzzy:     return 75.0;
        case MatchType::Poor:      return 0.0;
    }
    return 0.0;
}

/**
 * @brief Convert similarity score to match type
 * @param similarity Similarity percentage (0-100)
 * @return Corresponding match type
 */
[[nodiscard]] constexpr MatchType similarityToMatchType(double similarity) noexcept {
    if (similarity >= 100.0) return MatchType::Exact;
    if (similarity >= 95.0)  return MatchType::NearExact;
    if (similarity >= 75.0)  return MatchType::Fuzzy;
    return MatchType::Poor;
}

// ============================================================================
// Translation Memory Entry
// ============================================================================

/**
 * @brief Translation memory entry
 *
 * Represents a source-target text pair stored in translation memory.
 * Entries include metadata for context-aware matching.
 */
struct TranslationMemoryEntry {
    std::optional<int64_t> id;            ///< Database ID
    std::string sourceText;               ///< Source language text
    std::string targetText;               ///< Target language text
    std::string sourceHash;               ///< Hash of normalized source
    std::string sourceLang = "en";        ///< Source language code
    std::string targetLang = "tr";        ///< Target language code
    std::optional<std::string> context;   ///< Context hint
    std::optional<std::string> gameId;    ///< Game-specific entry
    std::optional<std::string> engineType;///< Engine-specific entry
    std::optional<std::string> filePath;  ///< Original file path
    std::optional<std::string> category;  ///< Content category
    int qualityScore = 100;               ///< Quality score (0-100)
    int usageCount = 1;                   ///< Times this entry was used
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    std::optional<std::string> createdBy; ///< Author identifier
    bool verified = false;                ///< Human verified

    /// @brief Check if entry is high quality
    [[nodiscard]] bool isHighQuality() const noexcept {
        return qualityScore >= 80 && verified;
    }

    /// @brief Check if entry is game-specific
    [[nodiscard]] bool isGameSpecific() const noexcept {
        return gameId.has_value();
    }

    /// @brief Check if entry has been used multiple times
    [[nodiscard]] bool isFrequentlyUsed() const noexcept {
        return usageCount >= 3;
    }
};

// ============================================================================
// Translation Memory Match
// ============================================================================

/**
 * @brief Translation memory match result
 *
 * Represents a match found in translation memory during lookup.
 */
struct TMMatch {
    TranslationMemoryEntry entry;
    double similarity = 0.0;              ///< Similarity percentage (0-100)
    MatchType matchType = MatchType::Poor;

    /// @brief Check if this is an exact match
    [[nodiscard]] bool isExact() const noexcept {
        return matchType == MatchType::Exact;
    }

    /// @brief Check if this is a usable match (>= fuzzy threshold)
    [[nodiscard]] bool isUsable() const noexcept {
        return matchType != MatchType::Poor;
    }

    /// @brief Comparison operator for sorting (higher similarity first)
    [[nodiscard]] bool operator<(const TMMatch& other) const noexcept {
        return similarity > other.similarity;  // Descending order
    }
};

} // namespace makineai
