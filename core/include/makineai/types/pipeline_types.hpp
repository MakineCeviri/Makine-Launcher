/**
 * @file types/pipeline_types.hpp
 * @brief Translation pipeline type definitions
 * @copyright (c) 2026 MakineAI Team
 *
 * Deferred feature — stub types for compilation.
 * Full implementation will be added when pipeline is prioritized.
 */

#pragma once

#include "makineai/types/common.hpp"
#include "makineai/types/game_types.hpp"

#include <string>
#include <vector>

namespace makineai {

/// @brief Translation method
enum class TranslationMethod : int {
    Unknown = 0,
    Direct,
    AssetParsing,
    BinaryPatching,
    ScriptHooking
};

/// @brief Method capability flags
enum class MethodCapability : int {
    None = 0,
    Extract = 1,
    Replace = 2,
    Validate = 4
};

/// @brief Pipeline execution phase
enum class PipelinePhase : int {
    Idle = 0,
    Analysis,
    Decision,
    Preparation,
    Execution,
    Validation,
    Complete,
    Failed
};

/// @brief Decision confidence level
enum class DecisionConfidence : int {
    None = 0,
    Low,
    Medium,
    High,
    Certain
};

/// @brief Method decision result
struct MethodDecision {
    TranslationMethod primaryMethod = TranslationMethod::Unknown;
    DecisionConfidence confidence = DecisionConfidence::None;
    std::string rationale;
};

/// @brief Game analysis result
struct GameAnalysis {
    GameInfo game;
    std::vector<TranslationMethod> supportedMethods;
};

/// @brief Pipeline execution context
struct PipelineContext {
    GameInfo game;
    PipelinePhase currentPhase = PipelinePhase::Idle;
    MethodDecision decision;

    int totalSteps = 0;
    int completedSteps = 0;

    bool backupCreated = false;
    std::string backupId;

    bool success = false;
    int appliedCount = 0;
    int failedCount = 0;
    std::vector<std::string> errors;
    std::vector<std::string> log;

    [[nodiscard]] double progressPercent() const {
        if (totalSteps <= 0) return 0.0;
        return (static_cast<double>(completedSteps) / totalSteps) * 100.0;
    }
};

/// @brief Pipeline options
struct PipelineOptions {
    bool createBackup = true;
    bool validateResults = true;
    bool dryRun = false;
};

/// @brief Pipeline execution result
struct PipelineResult {
    bool success = false;
    int appliedCount = 0;
    int failedCount = 0;
    std::vector<std::string> errors;
};

} // namespace makineai
