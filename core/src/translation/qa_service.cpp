/**
 * @file qa_service.cpp
 * @brief QA service implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/qa_service.hpp"
#include "makineai/glossary_service.hpp"
#include "makineai/core.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <stack>

namespace makineai {

// =============================================================================
// PLACEHOLDER HANDLER
// =============================================================================

namespace {
    // Placeholder patterns organized by type
    struct PatternEntry {
        PlaceholderType type;
        std::regex pattern;
    };

    std::vector<PatternEntry> getPatterns() {
        return {
            // Printf-style: %s, %d, %02d, %.2f, etc.
            {PlaceholderType::Printf, std::regex(R"(%[-+0 #]*\d*\.?\d*[hlL]?[diouxXeEfFgGaAcspn%])")},

            // Named placeholders: {name}, ${var}
            {PlaceholderType::Named, std::regex(R"(\{[a-zA-Z_][a-zA-Z0-9_]*\})")},
            {PlaceholderType::Named, std::regex(R"(\$\{[a-zA-Z_][a-zA-Z0-9_]*\})")},

            // Indexed placeholders: {0}, {1}, $1
            {PlaceholderType::Indexed, std::regex(R"(\{\d+\})")},
            {PlaceholderType::Indexed, std::regex(R"(\$\d+)")},

            // Ruby style: #{var}
            {PlaceholderType::Ruby, std::regex(R"(#\{[^}]+\})")},

            // Ren'Py style: [var], [var!t]
            {PlaceholderType::RenPy, std::regex(R"(\[[a-zA-Z_][a-zA-Z0-9_.]*(?:![tqTQisu])?\])")},

            // Unity rich text
            {PlaceholderType::Unity, std::regex(R"(<color=[^>]+>)")},
            {PlaceholderType::Unity, std::regex(R"(</color>)")},
            {PlaceholderType::Unity, std::regex(R"(<size=[^>]+>)")},
            {PlaceholderType::Unity, std::regex(R"(</size>)")},
            {PlaceholderType::Unity, std::regex(R"(<[bius]>)")},
            {PlaceholderType::Unity, std::regex(R"(</[bius]>)")},

            // BBCode
            {PlaceholderType::BBCode, std::regex(R"(\[[bius]\])")},
            {PlaceholderType::BBCode, std::regex(R"(\[/[bius]\])")},
            {PlaceholderType::BBCode, std::regex(R"(\[color=[^\]]+\])")},
            {PlaceholderType::BBCode, std::regex(R"(\[/color\])")},

            // HTML
            {PlaceholderType::Html, std::regex(R"(<br\s*/?>)")},
            {PlaceholderType::Html, std::regex(R"(&[a-zA-Z]+;)")},
            {PlaceholderType::Html, std::regex(R"(&#\d+;)")},

            // Escape sequences
            {PlaceholderType::Escape, std::regex(R"(\\[nrtfvab])")},
            {PlaceholderType::Escape, std::regex(R"(\\\\)")},
            {PlaceholderType::Escape, std::regex(R"(\\["'])")},
        };
    }

    bool hasOverlap(const std::vector<PlaceholderInfo>& existing, size_t start, size_t end) {
        for (const auto& ph : existing) {
            if ((start >= ph.startIndex && start < ph.endIndex) ||
                (end > ph.startIndex && end <= ph.endIndex)) {
                return true;
            }
        }
        return false;
    }
}

std::vector<PlaceholderInfo> PlaceholderHandler::detectPlaceholders(const std::string& text) {
    std::vector<PlaceholderInfo> results;
    auto patterns = getPatterns();

    for (const auto& entry : patterns) {
        std::sregex_iterator begin(text.begin(), text.end(), entry.pattern);
        std::sregex_iterator end;

        for (auto it = begin; it != end; ++it) {
            size_t start = static_cast<size_t>(it->position());
            size_t endPos = start + it->length();

            // Skip overlapping matches
            if (hasOverlap(results, start, endPos)) {
                continue;
            }

            PlaceholderInfo info;
            info.type = entry.type;
            info.original = it->str();
            info.startIndex = start;
            info.endIndex = endPos;
            info.variableName = extractVariableName(it->str(), entry.type);

            results.push_back(info);
        }
    }

    // Sort by position
    std::sort(results.begin(), results.end(),
        [](const PlaceholderInfo& a, const PlaceholderInfo& b) {
            return a.startIndex < b.startIndex;
        });

    return results;
}

std::optional<std::string> PlaceholderHandler::extractVariableName(
    const std::string& placeholder, PlaceholderType type
) {
    switch (type) {
        case PlaceholderType::Named: {
            std::regex pattern(R"([\{\$\(]+([a-zA-Z_][a-zA-Z0-9_]*)[\}\)]+)");
            std::smatch match;
            if (std::regex_search(placeholder, match, pattern)) {
                return match[1].str();
            }
            break;
        }
        case PlaceholderType::Indexed: {
            std::regex pattern(R"((\d+))");
            std::smatch match;
            if (std::regex_search(placeholder, match, pattern)) {
                return match[1].str();
            }
            break;
        }
        case PlaceholderType::Ruby: {
            std::regex pattern(R"(#\{([^}]+)\})");
            std::smatch match;
            if (std::regex_search(placeholder, match, pattern)) {
                return match[1].str();
            }
            break;
        }
        case PlaceholderType::RenPy: {
            std::regex pattern(R"(\[([a-zA-Z_][a-zA-Z0-9_.]*)[!\]])");
            std::smatch match;
            if (std::regex_search(placeholder, match, pattern)) {
                return match[1].str();
            }
            break;
        }
        default:
            break;
    }
    return std::nullopt;
}

PlaceholderValidation PlaceholderHandler::validatePlaceholders(
    const std::string& sourceText,
    const std::string& targetText
) {
    PlaceholderValidation result;
    result.sourcePlaceholders = detectPlaceholders(sourceText);
    result.targetPlaceholders = detectPlaceholders(targetText);

    // Collect originals
    std::unordered_set<std::string> sourceOriginals;
    std::unordered_set<std::string> targetOriginals;

    for (const auto& ph : result.sourcePlaceholders) {
        sourceOriginals.insert(ph.original);
    }
    for (const auto& ph : result.targetPlaceholders) {
        targetOriginals.insert(ph.original);
    }

    // Find missing (in source but not in target)
    for (const auto& orig : sourceOriginals) {
        if (targetOriginals.count(orig) == 0) {
            result.missing.push_back(orig);
            result.issues.push_back("Missing placeholder: " + orig);
            result.isValid = false;
        }
    }

    // Find extra (in target but not in source)
    for (const auto& orig : targetOriginals) {
        if (sourceOriginals.count(orig) == 0) {
            result.extra.push_back(orig);
            result.issues.push_back("Extra placeholder: " + orig);
            result.isValid = false;
        }
    }

    // Check counts for matching placeholders
    for (const auto& orig : sourceOriginals) {
        if (targetOriginals.count(orig) == 0) continue;

        size_t sourceCount = std::count_if(result.sourcePlaceholders.begin(),
            result.sourcePlaceholders.end(),
            [&orig](const PlaceholderInfo& p) { return p.original == orig; });

        size_t targetCount = std::count_if(result.targetPlaceholders.begin(),
            result.targetPlaceholders.end(),
            [&orig](const PlaceholderInfo& p) { return p.original == orig; });

        if (sourceCount != targetCount) {
            result.issues.push_back("Placeholder count mismatch: " + orig +
                " (source: " + std::to_string(sourceCount) +
                ", target: " + std::to_string(targetCount) + ")");
            result.isValid = false;
        }
    }

    return result;
}

bool PlaceholderHandler::checkPrintfOrder(
    const std::string& sourceText,
    const std::string& targetText
) {
    std::regex pattern(R"(%[-+0 #]*\d*\.?\d*[hlL]?[diouxXeEfFgGaAcspn])");

    std::vector<std::string> sourceMatches;
    std::vector<std::string> targetMatches;

    auto extractTypes = [&pattern](const std::string& text) {
        std::vector<std::string> types;
        std::sregex_iterator begin(text.begin(), text.end(), pattern);
        std::sregex_iterator end;

        for (auto it = begin; it != end; ++it) {
            std::string match = it->str();
            // Extract just the type character
            char type = match.back();
            types.push_back(std::string(1, type));
        }
        return types;
    };

    auto sourceTypes = extractTypes(sourceText);
    auto targetTypes = extractTypes(targetText);

    if (sourceTypes.size() != targetTypes.size()) return false;

    for (size_t i = 0; i < sourceTypes.size(); ++i) {
        if (sourceTypes[i] != targetTypes[i]) return false;
    }

    return true;
}

bool PlaceholderHandler::validateEscapeSequences(
    const std::string& sourceText,
    const std::string& targetText
) {
    std::regex escapePattern(R"(\\[nrtfvab]|\\\\|\\["'])");

    auto countEscapes = [&escapePattern](const std::string& text, const std::string& escape) {
        size_t count = 0;
        std::sregex_iterator begin(text.begin(), text.end(), escapePattern);
        std::sregex_iterator end;

        for (auto it = begin; it != end; ++it) {
            if (it->str() == escape) count++;
        }
        return count;
    };

    // Critical escapes that must be preserved
    std::vector<std::string> criticalEscapes = {"\\n", "\\t", "\\r"};

    for (const auto& escape : criticalEscapes) {
        if (countEscapes(sourceText, escape) != countEscapes(targetText, escape)) {
            return false;
        }
    }

    return true;
}

bool PlaceholderHandler::validateTagBalance(const std::string& text) {
    // HTML tags
    std::regex htmlOpen(R"(<(\w+)[^>]*(?<!/)>)");
    std::regex htmlClose(R"(</(\w+)>)");

    std::stack<std::string> htmlStack;

    std::sregex_iterator openBegin(text.begin(), text.end(), htmlOpen);
    std::sregex_iterator openEnd;
    for (auto it = openBegin; it != openEnd; ++it) {
        std::string tag = (*it)[1].str();
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        htmlStack.push(tag);
    }

    std::sregex_iterator closeBegin(text.begin(), text.end(), htmlClose);
    std::sregex_iterator closeEnd;
    for (auto it = closeBegin; it != closeEnd; ++it) {
        std::string tag = (*it)[1].str();
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

        if (htmlStack.empty() || htmlStack.top() != tag) {
            return false;
        }
        htmlStack.pop();
    }

    if (!htmlStack.empty()) return false;

    // BBCode tags
    std::regex bbOpen(R"(\[(\w+)(?:=[^\]]+)?\])");
    std::regex bbClose(R"(\[/(\w+)\])");

    std::stack<std::string> bbStack;

    std::sregex_iterator bbOpenBegin(text.begin(), text.end(), bbOpen);
    std::sregex_iterator bbOpenEnd;
    for (auto it = bbOpenBegin; it != bbOpenEnd; ++it) {
        std::string tag = (*it)[1].str();
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
        bbStack.push(tag);
    }

    std::sregex_iterator bbCloseBegin(text.begin(), text.end(), bbClose);
    std::sregex_iterator bbCloseEnd;
    for (auto it = bbCloseBegin; it != bbCloseEnd; ++it) {
        std::string tag = (*it)[1].str();
        std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

        if (bbStack.empty() || bbStack.top() != tag) {
            return false;
        }
        bbStack.pop();
    }

    return bbStack.empty();
}

std::optional<PlaceholderType> PlaceholderHandler::detectType(const std::string& placeholder) {
    auto patterns = getPatterns();
    for (const auto& entry : patterns) {
        if (std::regex_match(placeholder, entry.pattern)) {
            return entry.type;
        }
    }
    return std::nullopt;
}

// =============================================================================
// QA SERVICE
// =============================================================================

QAResult QAService::performFullQA(
    const std::string& sourceText,
    const std::string& targetText,
    const std::optional<std::string>& gameId,
    const std::optional<TermDomain>& domain,
    bool checkGlossaryFlag
) {
    MAKINEAI_LOG_INFO(log::QA, "Starting QA check (source length: {}, target length: {})",
        sourceText.length(), targetText.length());

    auto timer = metrics().timer("qa_check_duration");
    QAResult result;
    result.score = 100;

    // 1. Placeholder checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running placeholder check...");
    auto phResult = checkPlaceholders(sourceText, targetText);
    result.issues.insert(result.issues.end(), phResult.issues.begin(), phResult.issues.end());
    result.score -= phResult.penalty;

    // 2. Escape sequence checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running escape sequence check...");
    auto escResult = checkEscapeSequences(sourceText, targetText);
    result.issues.insert(result.issues.end(), escResult.issues.begin(), escResult.issues.end());
    result.score -= escResult.penalty;

    // 3. Tag balance checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running tag balance check...");
    auto tagResult = checkTagBalance(targetText);
    result.issues.insert(result.issues.end(), tagResult.issues.begin(), tagResult.issues.end());
    result.score -= tagResult.penalty;

    // 4. Length checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running length check...");
    auto lenResult = checkLength(sourceText, targetText);
    result.issues.insert(result.issues.end(), lenResult.issues.begin(), lenResult.issues.end());
    result.score -= lenResult.penalty;

    // 5. Character checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running character check...");
    auto charResult = checkCharacters(targetText);
    result.issues.insert(result.issues.end(), charResult.issues.begin(), charResult.issues.end());
    result.score -= charResult.penalty;

    // 6. Whitespace checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running whitespace check...");
    auto wsResult = checkWhitespace(sourceText, targetText);
    result.issues.insert(result.issues.end(), wsResult.issues.begin(), wsResult.issues.end());
    result.score -= wsResult.penalty;

    // 7. Punctuation checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running punctuation check...");
    auto punctResult = checkPunctuation(sourceText, targetText);
    result.issues.insert(result.issues.end(), punctResult.issues.begin(), punctResult.issues.end());
    result.score -= punctResult.penalty;

    // 8. Case checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running case check...");
    auto caseResult = checkCase(sourceText, targetText);
    result.issues.insert(result.issues.end(), caseResult.issues.begin(), caseResult.issues.end());
    result.score -= caseResult.penalty;

    // 9. Turkish character checks
    MAKINEAI_LOG_DEBUG(log::QA, "Running Turkish character check...");
    auto trResult = checkTurkishCharacters(targetText);
    result.issues.insert(result.issues.end(), trResult.issues.begin(), trResult.issues.end());
    result.score -= trResult.penalty;

    // 10. Glossary checks (optional)
    if (checkGlossaryFlag) {
        MAKINEAI_LOG_DEBUG(log::QA, "Running glossary check...");
        auto glossResult = checkGlossary(sourceText, targetText, gameId, domain);
        result.issues.insert(result.issues.end(), glossResult.issues.begin(), glossResult.issues.end());
        result.score -= glossResult.penalty;
    }

    // Clamp score
    result.score = std::clamp(result.score, 0, 100);
    result.passed = result.score >= QA_MIN_ACCEPT_SCORE;
    result.hasCriticalIssues = std::any_of(result.issues.begin(), result.issues.end(),
        [](const QAIssue& issue) { return static_cast<int>(issue.severity) >= static_cast<int>(QASeverity::Critical); });

    // Update metrics
    if (result.passed) {
        metrics().increment("qa_checks_passed");
    } else {
        metrics().increment("qa_checks_failed");
    }
    metrics().recordHistogram("qa_issues_per_check", static_cast<int64_t>(result.issues.size()));

    // Log warnings for issues found
    if (!result.issues.empty()) {
        MAKINEAI_LOG_WARN(log::QA, "QA check found {} issues (score: {})",
            result.issues.size(), result.score);
        for (const auto& issue : result.issues) {
            MAKINEAI_LOG_DEBUG(log::QA, "  Issue [{}]: {}", issue.code, issue.message);
        }
    }

    MAKINEAI_LOG_INFO(log::QA, "QA check complete (score: {}, passed: {}, issues: {})",
        result.score, result.passed, result.issues.size());

    return result;
}

QAService::CheckResult QAService::checkPlaceholders(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    auto validation = PlaceholderHandler::validatePlaceholders(source, target);

    // Missing placeholders (critical)
    for (const auto& missing : validation.missing) {
        result.issues.push_back(QAIssue{
            "PH_MISSING",
            "Missing placeholder: " + missing,
            QASeverity::Critical,
            20
        });
        result.penalty += 20;
    }

    // Extra placeholders (critical)
    for (const auto& extra : validation.extra) {
        result.issues.push_back(QAIssue{
            "PH_EXTRA",
            "Extra placeholder: " + extra,
            QASeverity::Critical,
            20
        });
        result.penalty += 20;
    }

    // Printf order check
    bool hasPrintf = std::any_of(validation.sourcePlaceholders.begin(),
        validation.sourcePlaceholders.end(),
        [](const PlaceholderInfo& p) { return p.type == PlaceholderType::Printf; });

    if (hasPrintf && !PlaceholderHandler::checkPrintfOrder(source, target)) {
        result.issues.push_back(QAIssue{
            "PH_ORDER",
            "Printf placeholder order mismatch",
            QASeverity::Major,
            10
        });
        result.penalty += 10;
    }

    return result;
}

QAService::CheckResult QAService::checkEscapeSequences(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    if (!PlaceholderHandler::validateEscapeSequences(source, target)) {
        result.issues.push_back(QAIssue{
            "ESC_MISMATCH",
            "Escape sequence mismatch",
            QASeverity::Critical,
            20
        });
        result.penalty += 20;
    }

    // Check for broken escape sequences
    std::regex brokenEscape(R"(\\[^nrtfvab\\"'xuU0-9])");
    if (std::regex_search(target, brokenEscape)) {
        result.issues.push_back(QAIssue{
            "ESC_BROKEN",
            "Broken escape sequence",
            QASeverity::Critical,
            20
        });
        result.penalty += 20;
    }

    return result;
}

QAService::CheckResult QAService::checkTagBalance(const std::string& target) {
    CheckResult result;

    if (!PlaceholderHandler::validateTagBalance(target)) {
        result.issues.push_back(QAIssue{
            "TAG_UNCLOSED",
            "Unclosed tag",
            QASeverity::Major,
            10
        });
        result.penalty += 10;
    }

    return result;
}

QAService::CheckResult QAService::checkLength(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    if (source.empty()) return result;

    double ratio = (static_cast<double>(target.length()) / source.length()) * 100.0;

    if (ratio > QA_MAX_LENGTH_RATIO) {
        result.issues.push_back(QAIssue{
            "LEN_LONG",
            "Translation too long (" + std::to_string(static_cast<int>(ratio)) + "%)",
            QASeverity::Major,
            10
        });
        result.penalty += 10;
    }
    else if (ratio < QA_MIN_LENGTH_RATIO && !target.empty()) {
        result.issues.push_back(QAIssue{
            "LEN_SHORT",
            "Translation too short (" + std::to_string(static_cast<int>(ratio)) + "%)",
            QASeverity::Warning,
            5
        });
        result.penalty += 5;
    }

    return result;
}

QAService::CheckResult QAService::checkCharacters(const std::string& target) {
    CheckResult result;

    // Suspicious characters (control chars except common whitespace)
    std::regex suspicious(R"([\x00-\x08\x0B\x0C\x0E-\x1F\x7F])");
    if (std::regex_search(target, suspicious)) {
        result.issues.push_back(QAIssue{
            "CHAR_SUSPICIOUS",
            "Suspicious character detected",
            QASeverity::Warning,
            5
        });
        result.penalty += 5;
    }

    // Null character
    if (target.find('\x00') != std::string::npos) {
        result.issues.push_back(QAIssue{
            "CHAR_NULL",
            "Null character detected",
            QASeverity::Critical,
            20
        });
        result.penalty += 20;
    }

    // Double spaces
    if (target.find("  ") != std::string::npos) {
        result.issues.push_back(QAIssue{
            "CHAR_DOUBLE_SPACE",
            "Double space detected",
            QASeverity::Info,
            1
        });
        result.penalty += 1;
    }

    return result;
}

QAService::CheckResult QAService::checkWhitespace(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    // Leading/trailing whitespace difference
    auto hasLeading = [](const std::string& s) {
        return !s.empty() && (s[0] == ' ' || s[0] == '\t');
    };
    auto hasTrailing = [](const std::string& s) {
        return !s.empty() && (s.back() == ' ' || s.back() == '\t');
    };

    if (hasLeading(source) != hasLeading(target) ||
        hasTrailing(source) != hasTrailing(target)) {
        result.issues.push_back(QAIssue{
            "WS_DIFF",
            "Whitespace difference (leading/trailing)",
            QASeverity::Info,
            1
        });
        result.penalty += 1;
    }

    // Newline count difference
    auto countNewlines = [](const std::string& s) {
        return std::count(s.begin(), s.end(), '\n');
    };

    if (countNewlines(source) != countNewlines(target)) {
        result.issues.push_back(QAIssue{
            "WS_NEWLINE",
            "Newline count difference",
            QASeverity::Warning,
            5
        });
        result.penalty += 5;
    }

    return result;
}

QAService::CheckResult QAService::checkPunctuation(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    if (source.empty() || target.empty()) return result;

    // Trim strings
    auto trim = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return std::string();
        return s.substr(start, end - start + 1);
    };

    std::string trimmedSource = trim(source);
    std::string trimmedTarget = trim(target);

    if (trimmedSource.empty() || trimmedTarget.empty()) return result;

    // Final punctuation check
    char sourceLast = trimmedSource.back();
    char targetLast = trimmedTarget.back();

    bool sourcePunct = std::string(".!?:;").find(sourceLast) != std::string::npos;
    bool targetPunct = std::string(".!?:;").find(targetLast) != std::string::npos;

    if (sourcePunct && !targetPunct) {
        result.issues.push_back(QAIssue{
            "PUNCT_MISSING",
            "Missing final punctuation",
            QASeverity::Info,
            1
        });
        result.penalty += 1;
    }

    // Parenthesis balance
    auto countChar = [](const std::string& s, char c) {
        return std::count(s.begin(), s.end(), c);
    };

    long sourceParenBalance = countChar(source, '(') - countChar(source, ')');
    long targetParenBalance = countChar(target, '(') - countChar(target, ')');

    if (sourceParenBalance != targetParenBalance) {
        result.issues.push_back(QAIssue{
            "PUNCT_PAREN",
            "Parenthesis balance mismatch",
            QASeverity::Warning,
            5
        });
        result.penalty += 5;
    }

    // Quote balance
    long sourceQuotes = countChar(source, '"');
    long targetQuotes = countChar(target, '"');

    if (sourceQuotes % 2 != targetQuotes % 2) {
        result.issues.push_back(QAIssue{
            "PUNCT_QUOTE",
            "Quote balance mismatch",
            QASeverity::Warning,
            5
        });
        result.penalty += 5;
    }

    return result;
}

QAService::CheckResult QAService::checkCase(
    const std::string& source,
    const std::string& target
) {
    CheckResult result;

    if (source.empty() || target.empty()) return result;

    // First character uppercase check
    bool sourceFirstUpper = std::isupper(static_cast<unsigned char>(source[0]));
    bool targetFirstUpper = std::isupper(static_cast<unsigned char>(target[0]));

    if (sourceFirstUpper && !targetFirstUpper) {
        result.issues.push_back(QAIssue{
            "CASE_FIRST",
            "First letter should be uppercase",
            QASeverity::Info,
            1
        });
        result.penalty += 1;
    }

    // All uppercase check
    auto isAllUpper = [](const std::string& s) {
        bool hasLetter = false;
        for (char c : s) {
            if (std::isalpha(static_cast<unsigned char>(c))) {
                hasLetter = true;
                if (!std::isupper(static_cast<unsigned char>(c))) {
                    return false;
                }
            }
        }
        return hasLetter;
    };

    if (isAllUpper(source) && !isAllUpper(target)) {
        result.issues.push_back(QAIssue{
            "CASE_ALL_UPPER",
            "Should be all uppercase",
            QASeverity::Info,
            1
        });
        result.penalty += 1;
    }

    return result;
}

QAService::CheckResult QAService::checkGlossary(
    const std::string& source,
    const std::string& target,
    const std::optional<std::string>& gameId,
    const std::optional<TermDomain>& domain
) {
    CheckResult result;

    // Find glossary terms in source
    auto& glossary = GlossaryService::instance();
    auto matchesResult = glossary.findTermsInText(source, domain, gameId);

    if (!matchesResult) {
        return result;  // Skip on error
    }

    std::string lowerTarget = target;
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& match : *matchesResult) {
        const auto& term = match.term;

        // Do not translate term
        if (term.doNotTranslate) {
            if (target.find(term.termSource) == std::string::npos) {
                result.issues.push_back(QAIssue{
                    "GLOSS_DNT",
                    "\"" + term.termSource + "\" should not be translated",
                    QASeverity::Major,
                    10
                });
                result.penalty += 10;
            }
            continue;
        }

        // Check correct translation used
        std::string lowerTermTarget = term.termTarget;
        std::transform(lowerTermTarget.begin(), lowerTermTarget.end(),
            lowerTermTarget.begin(), [](unsigned char c) { return std::tolower(c); });

        bool hasCorrectTranslation = lowerTarget.find(lowerTermTarget) != std::string::npos;

        // Also check alternatives
        if (!hasCorrectTranslation) {
            for (const auto& alt : term.alternatives) {
                std::string lowerAlt = alt;
                std::transform(lowerAlt.begin(), lowerAlt.end(), lowerAlt.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                if (lowerTarget.find(lowerAlt) != std::string::npos) {
                    hasCorrectTranslation = true;
                    break;
                }
            }
        }

        if (!hasCorrectTranslation) {
            result.issues.push_back(QAIssue{
                "GLOSS_MISSING",
                "\"" + term.termSource + "\" missing correct translation (expected: " + term.termTarget + ")",
                QASeverity::Warning,
                5
            });
            result.penalty += 5;
        }
    }

    // Check forbidden translations
    auto forbiddenResult = glossary.checkForbiddenTerms(source, target, gameId);
    if (forbiddenResult) {
        for (const auto& violation : *forbiddenResult) {
            std::string message = "\"" + violation.forbidden.forbiddenTranslation + "\" should not be used";
            if (violation.forbidden.reason.has_value()) {
                message += " (" + *violation.forbidden.reason + ")";
            }
            result.issues.push_back(QAIssue{
                "GLOSS_FORBIDDEN",
                message,
                QASeverity::Major,
                10
            });
            result.penalty += 10;
        }
    }

    return result;
}

QAService::CheckResult QAService::checkTurkishCharacters(const std::string& target) {
    CheckResult result;

    // Turkish-specific characters (UTF-8 byte sequences)
    static const std::string_view turkishChars[] = {
        "\xC3\xA7",  // ç
        "\xC3\xB6",  // ö
        "\xC3\xBC",  // ü
        "\xC4\x9F",  // ğ
        "\xC4\xB1",  // ı
        "\xC5\x9F",  // ş
        "\xC3\x87",  // Ç
        "\xC3\x96",  // Ö
        "\xC3\x9C",  // Ü
        "\xC4\x9E",  // Ğ
        "\xC4\xB0",  // İ
        "\xC5\x9E",  // Ş
    };

    // Check for UTF-8 mojibake (double-encoded Turkish characters)
    // Pattern: UTF-8 bytes read as Latin-1 then re-encoded to UTF-8
    // e.g. ç (C3 A7) becomes Ã§ (C3 83 C2 A7)
    static const std::string_view mojibakePatterns[] = {
        "\xC3\x83\xC2\xA7",  // ç → Ã§
        "\xC3\x83\xC2\xB6",  // ö → Ã¶
        "\xC3\x83\xC2\xBC",  // ü → Ã¼
    };

    for (const auto& pattern : mojibakePatterns) {
        if (target.find(pattern) != std::string::npos) {
            result.issues.push_back(QAIssue{
                "CHAR_TR_MOJIBAKE",
                "Turkish character encoding error (mojibake) detected",
                QASeverity::Major,
                10
            });
            result.penalty += 10;
            break;
        }
    }

    // For texts longer than 30 bytes, warn if no Turkish-specific characters found
    if (target.length() > 30) {
        bool hasTurkishChar = false;
        for (const auto& ch : turkishChars) {
            if (target.find(ch) != std::string::npos) {
                hasTurkishChar = true;
                break;
            }
        }

        if (!hasTurkishChar) {
            result.issues.push_back(QAIssue{
                "CHAR_TR_MISSING",
                "No Turkish-specific characters found in translation",
                QASeverity::Info,
                2
            });
            result.penalty += 2;
        }
    }

    return result;
}

std::map<int64_t, QAResult> QAService::batchQA(
    const std::map<int64_t, std::pair<std::string, std::string>>& entries,
    const std::optional<std::string>& gameId,
    const std::optional<TermDomain>& domain,
    bool checkGlossaryFlag
) {
    MAKINEAI_LOG_INFO(log::QA, "Starting batch QA check for {} entries", entries.size());

    std::map<int64_t, QAResult> results;

    for (const auto& [id, texts] : entries) {
        results[id] = performFullQA(texts.first, texts.second,
            gameId, domain, checkGlossaryFlag);
    }

    MAKINEAI_LOG_INFO(log::QA, "Batch QA check complete for {} entries", entries.size());
    return results;
}

QASummary QAService::createSummary(const std::vector<QAResult>& results) {
    QASummary summary;
    summary.totalEntries = static_cast<int>(results.size());

    int totalScore = 0;

    for (const auto& result : results) {
        totalScore += result.score;

        if (result.passed) {
            summary.passedEntries++;
        } else {
            summary.failedEntries++;
        }

        if (result.hasCriticalIssues) {
            summary.criticalEntries++;
        }

        for (const auto& issue : result.issues) {
            summary.issueDistribution[issue.code]++;
            summary.totalIssues++;
        }
    }

    summary.averageScore = results.empty() ? 0.0 :
        static_cast<double>(totalScore) / results.size();

    return summary;
}

} // namespace makineai
