/**
 * @file types/tm_types.hpp
 * @brief Translation Memory type definitions
 * @copyright (c) 2026 MakineAI Team
 *
 * Deferred feature — stub types for compilation.
 * Full implementation will be added when TM is prioritized.
 */

#pragma once

#include "makineai/types/common.hpp"

#include <optional>
#include <string>
#include <vector>

namespace makineai {

/**
 * @brief Translation Memory entry
 *
 * Represents a single entry in the translation memory database.
 */
struct TranslationMemoryEntry {
    std::optional<int64_t> id;
    std::string sourceText;
    std::string targetText;
    std::string sourceHash;
    std::string sourceLang = "en";
    std::string targetLang = "tr";
    std::optional<std::string> context;
    std::optional<std::string> gameId;
    std::optional<std::string> engineType;
    std::optional<std::string> category;
    std::optional<std::string> filePath;
    int qualityScore = 100;
    int usageCount = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    std::optional<std::string> createdBy;
    bool verified = false;
};

/**
 * @brief Translation Memory match result
 */
struct TMMatch {
    TranslationMemoryEntry entry;
    double similarity = 0.0;  // 0.0 - 1.0
};

/**
 * @brief Translation Memory statistics
 */
struct TMStats {
    int64_t totalEntries = 0;
    int64_t verifiedEntries = 0;
    double averageQuality = 0.0;
};

} // namespace makineai
