/**
 * @file types/pipeline_types.hpp
 * @brief Translation pipeline type definitions
 * @copyright (c) 2026 MakineAI Team
 *
 * This file contains types for the translation pipeline system,
 * including method selection, capabilities, and execution context.
 */

#pragma once

#include "makineai/types/common.hpp"
#include "makineai/types/game_types.hpp"

#include <map>
#include <optional>
#include <vector>

namespace makineai {

// ============================================================================
// Translation Methods
// ============================================================================

/**
 * @brief Translation method types
 *
 * Different approaches for applying translations to games.
 * The pipeline system will choose the best method(s) based on
 * game engine, stability requirements, and available options.
 */
enum class TranslationMethod {
    // --- Runtime Methods (Hook-based) ---
    RuntimeHook_XUnity,     ///< XUnity.AutoTranslator (Unity Mono/IL2CPP)
    RuntimeHook_BepInEx,    ///< BepInEx plugin (Unity)
    RuntimeHook_Custom,     ///< Custom runtime hook

    // --- File-Based Methods ---
    FileBased_JSON,         ///< RPG Maker MV/MZ JSON
    FileBased_Strings,      ///< Bethesda .strings files
    FileBased_Locres,       ///< Unreal .locres files
    FileBased_Script,       ///< Ren'Py .rpy scripts
    FileBased_INI,          ///< Generic INI/config files
    FileBased_PO,           ///< GNU gettext .po files

    // --- Binary Patching Methods ---
    Binary_IL2CPP,          ///< Unity IL2CPP global-metadata.dat
    Binary_DataWin,         ///< GameMaker data.win
    Binary_Generic,         ///< Generic binary string replacement

    // --- Asset Replacement Methods ---
    Asset_Unreal_Pak,       ///< Unreal .pak file replacement
    Asset_Unity_Resources,  ///< Unity .assets file replacement
    Asset_Generic,          ///< Generic asset file replacement

    // --- Hybrid Methods ---
    Hybrid_RuntimePlusBinary,   ///< Runtime for UI + Binary for static
    Hybrid_RuntimePlusFile,     ///< Runtime for dynamic + File for static

    Unknown
};

/**
 * @brief Convert translation method to string
 * @param method The translation method
 * @return Human-readable method name
 */
[[nodiscard]] constexpr std::string_view methodToString(TranslationMethod method) noexcept {
    switch (method) {
        case TranslationMethod::RuntimeHook_XUnity:       return "XUnity.AutoTranslator";
        case TranslationMethod::RuntimeHook_BepInEx:      return "BepInEx Plugin";
        case TranslationMethod::RuntimeHook_Custom:       return "Custom Runtime Hook";
        case TranslationMethod::FileBased_JSON:           return "JSON File Patch";
        case TranslationMethod::FileBased_Strings:        return "Bethesda Strings";
        case TranslationMethod::FileBased_Locres:         return "Unreal Locres";
        case TranslationMethod::FileBased_Script:         return "Script File Patch";
        case TranslationMethod::FileBased_INI:            return "INI File Patch";
        case TranslationMethod::FileBased_PO:             return "PO File Patch";
        case TranslationMethod::Binary_IL2CPP:            return "IL2CPP Binary Patch";
        case TranslationMethod::Binary_DataWin:           return "GameMaker Binary Patch";
        case TranslationMethod::Binary_Generic:           return "Generic Binary Patch";
        case TranslationMethod::Asset_Unreal_Pak:         return "Unreal Pak Replacement";
        case TranslationMethod::Asset_Unity_Resources:    return "Unity Asset Replacement";
        case TranslationMethod::Asset_Generic:            return "Asset File Replacement";
        case TranslationMethod::Hybrid_RuntimePlusBinary: return "Hybrid (Runtime+Binary)";
        case TranslationMethod::Hybrid_RuntimePlusFile:   return "Hybrid (Runtime+File)";
        default:                                          return "Unknown";
    }
}

/**
 * @brief Check if method requires runtime injection
 * @param method The translation method
 * @return true if runtime (BepInEx/XUnity) is needed
 */
[[nodiscard]] constexpr bool methodRequiresRuntime(TranslationMethod method) noexcept {
    switch (method) {
        case TranslationMethod::RuntimeHook_XUnity:
        case TranslationMethod::RuntimeHook_BepInEx:
        case TranslationMethod::RuntimeHook_Custom:
        case TranslationMethod::Hybrid_RuntimePlusBinary:
        case TranslationMethod::Hybrid_RuntimePlusFile:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if method modifies original game files
 * @param method The translation method
 * @return true if original files are changed
 */
[[nodiscard]] constexpr bool methodModifiesFiles(TranslationMethod method) noexcept {
    switch (method) {
        case TranslationMethod::RuntimeHook_XUnity:
        case TranslationMethod::RuntimeHook_BepInEx:
        case TranslationMethod::RuntimeHook_Custom:
            return false;  // Only add files, don't modify
        default:
            return true;
    }
}

// ============================================================================
// Method Capabilities
// ============================================================================

/**
 * @brief Method capability flags
 *
 * Describes what a translation method can do and its characteristics.
 */
struct MethodCapability {
    bool supportsExtraction = false;      ///< Can extract strings
    bool supportsApplication = false;     ///< Can apply translations
    bool supportsIncremental = false;     ///< Can apply partial updates
    bool supportsRollback = true;         ///< Can be rolled back
    bool requiresRestart = true;          ///< Game restart needed
    bool modifiesOriginalFiles = false;   ///< Changes original game files
    bool requiresRuntime = false;         ///< Needs BepInEx/XUnity
    bool supportsHotReload = false;       ///< Can reload without restart

    // Risk and stability
    int riskLevel = 50;                   ///< 0=safest, 100=riskiest
    int stabilityScore = 50;              ///< 0=unstable, 100=rock solid
    int performanceImpact = 10;           ///< 0=no impact, 100=heavy

    // Compatibility
    bool worksWithAntiCheat = false;      ///< Works with anti-cheat games
    bool worksWithDRM = true;             ///< Works with DRM-protected games
    bool surviveUpdates = false;          ///< Translation survives game updates

    /// @brief Calculate overall safety score
    [[nodiscard]] int safetyScore() const noexcept {
        return 100 - riskLevel;
    }

    /// @brief Check if method is low risk
    [[nodiscard]] bool isLowRisk() const noexcept {
        return riskLevel <= 30;
    }

    /// @brief Check if method is production-ready
    [[nodiscard]] bool isProductionReady() const noexcept {
        return stabilityScore >= 70 && supportsRollback;
    }
};

// ============================================================================
// Pipeline Phases
// ============================================================================

/**
 * @brief Pipeline execution phase
 *
 * Represents the current stage of translation pipeline execution.
 */
enum class PipelinePhase {
    Analyze,        ///< Analyze game structure
    Decide,         ///< Choose best method(s)
    Prepare,        ///< Prepare resources (backup, download)
    Apply,          ///< Apply translations
    Verify          ///< Verify success
};

/**
 * @brief Convert pipeline phase to string
 */
[[nodiscard]] constexpr std::string_view phaseToString(PipelinePhase phase) noexcept {
    switch (phase) {
        case PipelinePhase::Analyze: return "Analyze";
        case PipelinePhase::Decide:  return "Decide";
        case PipelinePhase::Prepare: return "Prepare";
        case PipelinePhase::Apply:   return "Apply";
        case PipelinePhase::Verify:  return "Verify";
    }
    return "Unknown";
}

/**
 * @brief Decision confidence level
 *
 * Indicates how confident the system is about its method selection.
 */
enum class DecisionConfidence {
    VeryHigh = 100,     ///< Exact match, tested combination
    High = 80,          ///< Strong match, common scenario
    Medium = 60,        ///< Reasonable match, may need adjustment
    Low = 40,           ///< Weak match, likely issues
    VeryLow = 20        ///< Guess, probably won't work
};

// ============================================================================
// Method Decision
// ============================================================================

/**
 * @brief Method selection decision
 *
 * Contains the decision engine's choice of translation method(s)
 * along with rationale and warnings.
 */
struct MethodDecision {
    TranslationMethod primaryMethod = TranslationMethod::Unknown;
    std::optional<TranslationMethod> fallbackMethod;
    std::optional<TranslationMethod> secondaryMethod; ///< For hybrid

    DecisionConfidence confidence = DecisionConfidence::VeryLow;
    std::string rationale;              ///< Why this method was chosen
    std::vector<std::string> warnings;  ///< Potential issues
    std::vector<std::string> requirements; ///< What needs to be installed

    MethodCapability capabilities;

    // Coverage estimation
    int estimatedCoverage = 100;        ///< % of strings this method covers
    int estimatedTime = 0;              ///< Estimated time in seconds

    /// @brief Check if this is a hybrid decision
    [[nodiscard]] bool isHybrid() const noexcept {
        return primaryMethod == TranslationMethod::Hybrid_RuntimePlusBinary ||
               primaryMethod == TranslationMethod::Hybrid_RuntimePlusFile ||
               secondaryMethod.has_value();
    }

    /// @brief Check if decision has high confidence
    [[nodiscard]] bool isHighConfidence() const noexcept {
        return confidence >= DecisionConfidence::High;
    }

    /// @brief Check if fallback is available
    [[nodiscard]] bool hasFallback() const noexcept {
        return fallbackMethod.has_value();
    }
};

// ============================================================================
// Game Analysis
// ============================================================================

/**
 * @brief Game analysis for method selection
 *
 * Contains detailed analysis of a game's structure for
 * determining the best translation approach.
 */
struct GameAnalysis {
    GameEngine engine = GameEngine::Unknown;
    std::string engineVersion;
    std::vector<TranslationMethod> supportedMethods;
    std::vector<TranslationMethod> recommendedMethods;
    std::map<TranslationMethod, MethodCapability> methodCapabilities;

    // Game-specific info
    bool hasExistingMods = false;
    bool hasAntiCheat = false;
    bool is64Bit = true;
    uint64_t estimatedStringCount = 0;

    // File discovery
    std::vector<std::string> localizationFiles;
    std::vector<std::string> binaryTargets;
    std::vector<std::string> assetTargets;

    /// @brief Check if any methods are supported
    [[nodiscard]] bool hasSupportedMethods() const noexcept {
        return !supportedMethods.empty();
    }

    /// @brief Get the primary recommended method
    [[nodiscard]] std::optional<TranslationMethod> primaryRecommendation() const {
        if (recommendedMethods.empty()) return std::nullopt;
        return recommendedMethods.front();
    }
};

// ============================================================================
// Pipeline Context
// ============================================================================

/**
 * @brief Pipeline execution context
 *
 * Contains all state for a translation pipeline execution.
 */
struct PipelineContext {
    GameInfo game;
    GameAnalysis analysis;
    MethodDecision decision;

    // Execution state
    PipelinePhase currentPhase = PipelinePhase::Analyze;
    bool backupCreated = false;
    std::string backupId;

    // Progress tracking
    uint32_t totalSteps = 0;
    uint32_t completedSteps = 0;
    std::vector<std::string> log;

    // Results
    bool success = false;
    int appliedCount = 0;
    int failedCount = 0;
    std::vector<std::string> errors;

    /// @brief Calculate progress percentage
    [[nodiscard]] double progressPercent() const noexcept {
        if (totalSteps == 0) return 0.0;
        return (static_cast<double>(completedSteps) / totalSteps) * 100.0;
    }

    /// @brief Add log entry
    void addLog(std::string_view message) {
        log.emplace_back(message);
    }

    /// @brief Add error
    void addError(std::string_view error) {
        errors.emplace_back(error);
    }
};

/**
 * @brief Pipeline execution options
 *
 * Configuration options for pipeline execution.
 */
struct PipelineOptions {
    bool preferSafeMethods = true;      ///< Prefer non-destructive methods
    bool allowHybrid = true;            ///< Allow combining methods
    bool autoFallback = true;           ///< Auto-fallback on failure
    bool createBackup = true;           ///< Always create backup
    bool dryRun = false;                ///< Simulate without applying
    int maxRetries = 2;                 ///< Retries on failure
    TranslationMethod forcedMethod = TranslationMethod::Unknown; ///< Force specific method

    /// @brief Check if a specific method is forced
    [[nodiscard]] bool hasForceMethod() const noexcept {
        return forcedMethod != TranslationMethod::Unknown;
    }
};

} // namespace makineai
