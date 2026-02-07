/**
 * @file qa_service.hpp
 * @brief Quality Assurance service for translation validation
 * @copyright (c) 2026 MakineAI Team
 *
 * QA Checks:
 * 1. Placeholder validation (missing/extra/order)
 * 2. Escape sequence validation
 * 3. Tag balance (HTML/BBCode/Unity)
 * 4. Length ratio (50-150%)
 * 5. Character validation (suspicious, null)
 * 6. Whitespace validation
 * 7. Punctuation validation
 * 8. Case validation
 * 9. Glossary compliance
 */

#pragma once

#include "types.hpp"
#include "error.hpp"

#include <regex>
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace makineai {

/**
 * @brief Minimum acceptable QA score
 */
constexpr int QA_MIN_ACCEPT_SCORE = 70;

/**
 * @brief Maximum length ratio (target/source * 100)
 */
constexpr double QA_MAX_LENGTH_RATIO = 150.0;

/**
 * @brief Minimum length ratio (target/source * 100)
 */
constexpr double QA_MIN_LENGTH_RATIO = 50.0;

/**
 * @brief Placeholder validation result
 */
struct PlaceholderValidation {
    bool isValid = true;
    std::vector<std::string> issues;
    std::vector<PlaceholderInfo> sourcePlaceholders;
    std::vector<PlaceholderInfo> targetPlaceholders;
    std::vector<std::string> missing;
    std::vector<std::string> extra;

    [[nodiscard]] bool hasCriticalError() const {
        return !missing.empty() || !extra.empty();
    }
};

/**
 * @brief QA check result
 */
struct QAResult {
    int score = 100;
    std::vector<QAIssue> issues;
    bool passed = true;
    bool hasCriticalIssues = false;

    [[nodiscard]] std::vector<QAIssue> getCriticalIssues() const {
        std::vector<QAIssue> result;
        for (const auto& issue : issues) {
            if (static_cast<int>(issue.severity) >= static_cast<int>(QASeverity::Critical)) {
                result.push_back(issue);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<QAIssue> getMajorIssues() const {
        std::vector<QAIssue> result;
        for (const auto& issue : issues) {
            if (issue.severity == QASeverity::Major) {
                result.push_back(issue);
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<QAIssue> getWarnings() const {
        std::vector<QAIssue> result;
        for (const auto& issue : issues) {
            if (issue.severity == QASeverity::Warning) {
                result.push_back(issue);
            }
        }
        return result;
    }
};

/**
 * @brief QA summary for batch processing
 */
struct QASummary {
    int totalEntries = 0;
    int passedEntries = 0;
    int failedEntries = 0;
    int criticalEntries = 0;
    double averageScore = 0.0;
    std::map<std::string, int> issueDistribution;
    int totalIssues = 0;

    [[nodiscard]] double getPassRate() const {
        return totalEntries > 0 ? (static_cast<double>(passedEntries) / totalEntries) * 100 : 0;
    }

    [[nodiscard]] double getCriticalRate() const {
        return totalEntries > 0 ? (static_cast<double>(criticalEntries) / totalEntries) * 100 : 0;
    }
};

/**
 * @brief Placeholder Handler
 *
 * Detects and validates various placeholder formats.
 */
class PlaceholderHandler {
public:
    /**
     * @brief Detect all placeholders in text
     */
    static std::vector<PlaceholderInfo> detectPlaceholders(const std::string& text);

    /**
     * @brief Validate placeholder consistency between source and target
     */
    static PlaceholderValidation validatePlaceholders(
        const std::string& sourceText,
        const std::string& targetText
    );

    /**
     * @brief Check printf placeholder order consistency
     */
    static bool checkPrintfOrder(
        const std::string& sourceText,
        const std::string& targetText
    );

    /**
     * @brief Validate escape sequences
     */
    static bool validateEscapeSequences(
        const std::string& sourceText,
        const std::string& targetText
    );

    /**
     * @brief Validate tag balance (HTML/BBCode)
     */
    static bool validateTagBalance(const std::string& text);

    /**
     * @brief Detect placeholder type
     */
    static std::optional<PlaceholderType> detectType(const std::string& placeholder);

private:
    /**
     * @brief Extract variable name from placeholder
     */
    static std::optional<std::string> extractVariableName(
        const std::string& placeholder,
        PlaceholderType type
    );
};

/**
 * @brief QA (Quality Assurance) Service
 *
 * Performs comprehensive quality checks on translations.
 */
class QAService {
public:
    /**
     * @brief Perform full QA check (synchronous)
     *
     * @param sourceText Original text
     * @param targetText Translated text
     * @param gameId Optional game ID for glossary
     * @param domain Optional term domain for glossary
     * @param checkGlossary Whether to check glossary compliance
     * @return QA result with score and issues
     */
    static QAResult performFullQA(
        const std::string& sourceText,
        const std::string& targetText,
        const std::optional<std::string>& gameId = std::nullopt,
        const std::optional<TermDomain>& domain = std::nullopt,
        bool checkGlossary = false
    );

    /**
     * @brief Batch QA check
     *
     * @param entries Map of ID -> (source, target) pairs
     * @param gameId Optional game ID
     * @param domain Optional domain
     * @param checkGlossary Whether to check glossary
     * @return Map of ID -> QA result
     */
    static std::map<int64_t, QAResult> batchQA(
        const std::map<int64_t, std::pair<std::string, std::string>>& entries,
        const std::optional<std::string>& gameId = std::nullopt,
        const std::optional<TermDomain>& domain = std::nullopt,
        bool checkGlossary = false
    );

    /**
     * @brief Create summary from QA results
     */
    static QASummary createSummary(const std::vector<QAResult>& results);

private:
    // Individual check functions
    struct CheckResult {
        std::vector<QAIssue> issues;
        int penalty = 0;
    };

    static CheckResult checkPlaceholders(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkEscapeSequences(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkTagBalance(const std::string& target);

    static CheckResult checkLength(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkCharacters(const std::string& target);

    static CheckResult checkWhitespace(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkPunctuation(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkCase(
        const std::string& source,
        const std::string& target
    );

    static CheckResult checkGlossary(
        const std::string& source,
        const std::string& target,
        const std::optional<std::string>& gameId,
        const std::optional<TermDomain>& domain
    );

    static CheckResult checkTurkishCharacters(const std::string& target);
};

} // namespace makineai
