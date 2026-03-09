/**
 * @file translation_types.hpp
 * @brief Translation system type definitions (glossary, projects, entries, pipeline)
 * @copyright (c) 2026 MakineAI Team
 *
 * These types support the database layer for translation management.
 * The higher-level translation pipeline features (QA, TM, glossary service)
 * are intentionally deferred - only the data model types are defined here.
 */

#pragma once

#include "makineai/types/game_types.hpp"
#include "makineai/types/patch_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace makineai {

// ============================================================================
// Glossary Types
// ============================================================================

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
    Horror,
};

struct GlossaryTerm {
    std::optional<int64_t>     id;
    std::string                termSource;
    std::string                termTarget;
    std::optional<std::string> termType;
    std::optional<TermDomain>  domain;
    bool                       caseSensitive  = false;
    bool                       exactMatch     = false;
    int                        priority       = 0;
    std::optional<std::string> notes;
    std::vector<std::string>   examples;
    bool                       doNotTranslate = false;
    int64_t                    createdAt      = 0;
    std::optional<std::string> gameSpecific;
};

// ============================================================================
// Translation Project Types
// ============================================================================

enum class ProjectStatus {
    Active,
    Completed,
    Archived,
    Paused,
};

struct TranslationProject {
    std::string                id;
    std::optional<std::string> gameId;
    std::string                name;
    std::string                sourceLang;
    std::string                targetLang;
    int64_t                    createdAt = 0;
    int64_t                    updatedAt = 0;
    ProjectStatus              status    = ProjectStatus::Active;
    double                     progress  = 0.0;
    std::optional<std::string> settings;
};

// ============================================================================
// Translation Entry Types
// ============================================================================

enum class EntryCategory {
    UI,
    Dialogue,
    Item,
    Skill,
    Location,
    Tutorial,
    Credits,
    Other,
};

struct TranslationEntry {
    std::optional<int64_t>       id;
    std::string                  projectId;
    std::string                  filePath;
    std::optional<std::string>   entryKey;
    std::string                  sourceText;
    std::optional<std::string>   targetText;
    std::optional<std::string>   context;
    std::optional<EntryCategory> category;
    EntryStatus                  status    = EntryStatus::Untranslated;
    int                          qaScore   = 0;
    std::optional<int>           lineNumber;
    int64_t                      createdAt = 0;
    int64_t                      updatedAt = 0;
    std::optional<std::string>   translatedBy;
};


// ============================================================================
// Translation Memory Types
// ============================================================================

struct TranslationMemoryEntry {
    int64_t                    id          = 0;
    std::string                sourceText;
    std::string                targetText;
    std::string                sourceHash;
    std::string                sourceLang;
    std::string                targetLang;
    std::optional<std::string> context;
    std::optional<std::string> gameId;
    std::optional<std::string> engineType;
    std::optional<std::string> filePath;
    std::optional<std::string> category;
    int                        qualityScore = 0;
    int                        usageCount   = 0;
    int64_t                    createdAt    = 0;
    int64_t                    updatedAt    = 0;
    std::optional<std::string> createdBy;
    bool                       verified     = false;
};

// ============================================================================
// Pipeline Context (consumed by DebugDumper::dumpPipelineContext)
// ============================================================================

struct PipelineDecision {
    int         primaryMethod = 0;
    int         confidence    = 0;
    std::string rationale;
};

struct PipelineContext {
    GameInfo         game;
    int              currentPhase   = 0;
    PipelineDecision decision;
    int              totalSteps     = 0;
    int              completedSteps = 0;
    bool             backupCreated  = false;
    std::string      backupId;
    bool             success        = false;
    int              appliedCount   = 0;
    int              failedCount    = 0;
    std::vector<std::string> errors;
    std::vector<std::string> log;

    [[nodiscard]] double progressPercent() const noexcept {
        if (totalSteps <= 0) return 0.0;
        return static_cast<double>(completedSteps) / totalSteps * 100.0;
    }
};

} // namespace makineai
