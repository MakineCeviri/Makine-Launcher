/**
 * @file types/translation_types.hpp
 * @brief Translation entry and glossary type definitions
 * @copyright (c) 2026 MakineAI Team
 *
 * This file contains types for translation entries, glossary terms,
 * QA issues, and translation projects.
 */

#pragma once

#include "makineai/types/common.hpp"

#include <optional>
#include <vector>

namespace makineai {

// ============================================================================
// Translation Entry Status
// ============================================================================

/**
 * @brief Translation entry status
 *
 * Indicates the translation state of a single entry.
 */
enum class EntryStatus {
    Untranslated,   ///< Not yet translated
    Translated,     ///< Has translation
    Fuzzy,          ///< Fuzzy match used (needs review)
    Verified,       ///< Human verified
    Rejected        ///< Rejected translation
};

/**
 * @brief Translation entry category
 *
 * Categorizes the type of translatable content.
 */
enum class EntryCategory {
    UI,             ///< Interface (menu, button)
    Dialog,         ///< Dialog
    Narration,      ///< Narration
    Item,           ///< Item name/description
    Skill,          ///< Skill name/description
    Quest,          ///< Quest
    System,         ///< System message
    Tutorial,       ///< Tutorial
    Credits,        ///< Credits
    Lore,           ///< World lore, books
    Other           ///< Other
};

// ============================================================================
// Placeholder Types
// ============================================================================

/**
 * @brief Placeholder type in text
 *
 * Identifies the format of variable placeholders in translatable strings.
 */
enum class PlaceholderType {
    Printf,         ///< %s, %d, %f
    Named,          ///< {name}, ${var}
    Indexed,        ///< {0}, {1}, $1
    Ruby,           ///< #{var}
    RenPy,          ///< [var]
    Unity,          ///< <color=#fff>
    BBCode,         ///< [b], [i]
    Html,           ///< <br>, &nbsp;
    Escape,         ///< \n, \t
    Unknown         ///< Unknown type
};

/**
 * @brief Placeholder information in text
 *
 * Describes a single placeholder occurrence in a string.
 */
struct PlaceholderInfo {
    PlaceholderType type;
    std::string original;         ///< Original placeholder text
    size_t startIndex = 0;        ///< Start position in string
    size_t endIndex = 0;          ///< End position in string
    std::optional<std::string> variableName;  ///< Variable name if applicable
};

// ============================================================================
// QA Types
// ============================================================================

/**
 * @brief QA issue severity
 *
 * Indicates how critical a QA issue is.
 */
enum class QASeverity {
    Info = 1,       ///< Information only
    Warning = 2,    ///< Warning
    Major = 3,      ///< Major issue
    Critical = 4    ///< Critical issue
};

/**
 * @brief QA issue structure
 *
 * Describes a quality issue found in a translation.
 */
struct QAIssue {
    std::string code;             ///< Issue code (e.g., "PLACEHOLDER_MISMATCH")
    std::string message;          ///< Human-readable description
    QASeverity severity = QASeverity::Warning;
    int penaltyPoints = 0;        ///< Points deducted from QA score
};

// ============================================================================
// Translation Entry
// ============================================================================

/**
 * @brief Translation entry (single translatable string)
 *
 * Represents a single string that can be translated, including
 * source text, target text, context, and quality information.
 */
struct TranslationEntry {
    std::optional<int64_t> id;            ///< Database ID
    std::string projectId;                ///< Project this belongs to
    std::string filePath;                 ///< Source file path
    std::optional<std::string> entryKey;  ///< Unique key within file
    std::string sourceText;               ///< Original text
    std::optional<std::string> targetText;///< Translated text
    std::optional<std::string> context;   ///< Context hint
    std::optional<EntryCategory> category;
    EntryStatus status = EntryStatus::Untranslated;
    int qaScore = 100;                    ///< QA score (0-100)
    std::vector<QAIssue> qaIssues;
    std::vector<PlaceholderInfo> placeholders;
    std::optional<int> lineNumber;        ///< Line number in source file
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    std::optional<std::string> translatedBy;

    // IL2CPP metadata
    int64_t offset = 0;                   ///< Binary offset
    int64_t length = 0;                   ///< Original length

    /// @brief Check if entry has a translation
    [[nodiscard]] bool isTranslated() const noexcept {
        return targetText.has_value() && !targetText->empty() &&
               status != EntryStatus::Untranslated;
    }

    /// @brief Check if entry passed QA
    [[nodiscard]] bool passedQA() const noexcept {
        return qaScore >= 70;
    }

    /// @brief Check if entry has critical QA issues
    [[nodiscard]] bool hasCriticalIssues() const noexcept {
        for (const auto& issue : qaIssues) {
            if (issue.severity == QASeverity::Critical) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Glossary Types
// ============================================================================

/**
 * @brief Glossary term type
 *
 * Categorizes the linguistic type of a glossary term.
 */
enum class TermType {
    Noun,
    Verb,
    Adjective,
    UI,
    Item,
    Skill,
    Stat,
    Action,
    Currency,
    Place,
    Character,
    Other
};

/**
 * @brief Glossary domain (game genre)
 *
 * Specifies the domain a glossary term applies to.
 */
enum class TermDomain {
    General,
    RPG,
    FPS,
    VisualNovel,
    Strategy,
    Simulation,
    Adventure,
    Puzzle,
    Action,
    Horror
};

/**
 * @brief Forbidden translation entry
 *
 * Represents a translation that should NOT be used for a term.
 */
struct ForbiddenTranslation {
    std::optional<int64_t> id;
    int64_t glossaryId = 0;
    std::string forbiddenTranslation;
    std::optional<std::string> reason;
};

/**
 * @brief Glossary term
 *
 * Represents a terminology entry with its approved translation
 * and usage rules.
 */
struct GlossaryTerm {
    std::optional<int64_t> id;
    std::string termSource;               ///< Source language term
    std::string termTarget;               ///< Target language translation
    std::optional<TermType> termType;
    std::optional<TermDomain> domain;
    bool caseSensitive = false;           ///< Case-sensitive matching
    bool exactMatch = false;              ///< Require exact word match
    int priority = 50;                    ///< Priority (0-100, higher wins)
    std::optional<std::string> notes;     ///< Usage notes
    std::vector<std::string> examples;    ///< Usage examples
    bool doNotTranslate = false;          ///< Keep original (proper nouns)
    int64_t createdAt = 0;
    std::optional<std::string> gameSpecific;  ///< Game ID if game-specific
    std::vector<std::string> alternatives;    ///< Acceptable alternatives
    std::vector<ForbiddenTranslation> forbidden;  ///< Do NOT use these

    /// @brief Check if text matches this term
    [[nodiscard]] bool matches(std::string_view text) const;
};

// ============================================================================
// Project Types
// ============================================================================

/**
 * @brief Translation project status
 */
enum class ProjectStatus {
    Active,
    Completed,
    Archived,
    Paused
};

/**
 * @brief Translation project
 *
 * Represents a translation project for a specific game.
 */
struct TranslationProject {
    std::string id;                       ///< Unique project ID
    std::optional<std::string> gameId;    ///< Associated game ID
    std::string name;                     ///< Project name
    std::string sourceLang = "en";        ///< Source language code
    std::string targetLang = "tr";        ///< Target language code
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    ProjectStatus status = ProjectStatus::Active;
    double progress = 0.0;                ///< Progress percentage (0-100)
    std::optional<std::string> settings;  ///< JSON settings string

    /// @brief Check if project is active
    [[nodiscard]] bool isActive() const noexcept {
        return status == ProjectStatus::Active;
    }

    /// @brief Check if project is complete
    [[nodiscard]] bool isComplete() const noexcept {
        return progress >= 100.0 || status == ProjectStatus::Completed;
    }
};

} // namespace makineai
