/**
 * @file translation_pipeline.cpp
 * @brief Translation Pipeline System implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "makineai/translation_pipeline.hpp"
#include "makineai/runtime_manager.hpp"
#include "makineai/patch_engine.hpp"
#include "makineai/logging.hpp"
#include "makineai/metrics.hpp"
#include "makineai/audit.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <fstream>

namespace makineai {

// ============================================================================
// METHOD REGISTRY IMPLEMENTATION
// ============================================================================

MethodRegistry& MethodRegistry::instance() {
    static MethodRegistry instance;
    return instance;
}

MethodRegistry::MethodRegistry() {
    registerBuiltinMethods();
}

void MethodRegistry::registerBuiltinMethods() {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Registering built-in translation methods");

    // === RUNTIME METHODS ===

    // XUnity.AutoTranslator - Best for Unity games
    registerMethod({
        .method = TranslationMethod::RuntimeHook_XUnity,
        .name = "XUnity.AutoTranslator",
        .description = "Runtime hooking via BepInEx + XUnity.AutoTranslator. "
                      "Best for Unity games - captures text at runtime, no file modification.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = true,
            .modifiesOriginalFiles = false,
            .requiresRuntime = true,
            .supportsHotReload = true,
            .riskLevel = 10,
            .stabilityScore = 90,
            .performanceImpact = 15,
            .worksWithAntiCheat = false,
            .worksWithDRM = true,
            .surviveUpdates = true
        },
        .supportedEngines = {GameEngine::Unity_Mono, GameEngine::Unity_IL2CPP},
        .baseScore = 85,
        .safetyBonus = 30,
        .performanceBonus = 10
    });

    // BepInEx custom plugin
    registerMethod({
        .method = TranslationMethod::RuntimeHook_BepInEx,
        .name = "BepInEx Plugin",
        .description = "Custom BepInEx plugin for specialized Unity game translation.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = true,
            .modifiesOriginalFiles = false,
            .requiresRuntime = true,
            .supportsHotReload = true,
            .riskLevel = 15,
            .stabilityScore = 85,
            .performanceImpact = 10,
            .worksWithAntiCheat = false,
            .worksWithDRM = true,
            .surviveUpdates = true
        },
        .supportedEngines = {GameEngine::Unity_Mono, GameEngine::Unity_IL2CPP},
        .baseScore = 75,
        .safetyBonus = 25,
        .performanceBonus = 15
    });

    // === FILE-BASED METHODS ===

    // JSON (RPG Maker MV/MZ)
    registerMethod({
        .method = TranslationMethod::FileBased_JSON,
        .name = "JSON File Patch",
        .description = "Direct JSON file modification. Safe and simple for RPG Maker games.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 20,
            .stabilityScore = 95,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::RPGMaker_MV},
        .baseScore = 90,
        .safetyBonus = 25,
        .performanceBonus = 20
    });

    // Bethesda .strings
    registerMethod({
        .method = TranslationMethod::FileBased_Strings,
        .name = "Bethesda Strings",
        .description = "Bethesda Creation Engine .strings/.dlstrings/.ilstrings file modification.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 25,
            .stabilityScore = 90,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::Bethesda},
        .baseScore = 88,
        .safetyBonus = 20,
        .performanceBonus = 20
    });

    // Unreal .locres
    registerMethod({
        .method = TranslationMethod::FileBased_Locres,
        .name = "Unreal Locres",
        .description = "Unreal Engine .locres localization file modification.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 30,
            .stabilityScore = 85,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::Unreal},
        .baseScore = 82,
        .safetyBonus = 15,
        .performanceBonus = 20
    });

    // Ren'Py script
    registerMethod({
        .method = TranslationMethod::FileBased_Script,
        .name = "Script File Patch",
        .description = "Direct script file modification for Ren'Py games.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 15,
            .stabilityScore = 95,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::RenPy},
        .baseScore = 92,
        .safetyBonus = 30,
        .performanceBonus = 25
    });

    // === BINARY METHODS ===

    // IL2CPP binary patching
    registerMethod({
        .method = TranslationMethod::Binary_IL2CPP,
        .name = "IL2CPP Binary Patch",
        .description = "Direct binary patching of Unity IL2CPP global-metadata.dat. "
                      "Risky but works when runtime hooks fail.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = false,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 70,
            .stabilityScore = 50,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::Unity_IL2CPP},
        .baseScore = 40,
        .safetyBonus = -20,
        .performanceBonus = 25
    });

    // GameMaker data.win
    registerMethod({
        .method = TranslationMethod::Binary_DataWin,
        .name = "GameMaker Binary Patch",
        .description = "GameMaker data.win binary patching. Modifies game data directly.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = false,
            .supportsRollback = true,
            .requiresRestart = false,
            .modifiesOriginalFiles = true,
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 60,
            .stabilityScore = 70,
            .performanceImpact = 0,
            .worksWithAntiCheat = true,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::GameMaker},
        .baseScore = 75,
        .safetyBonus = 0,
        .performanceBonus = 20
    });

    // === ASSET METHODS ===

    // Unreal .pak replacement
    registerMethod({
        .method = TranslationMethod::Asset_Unreal_Pak,
        .name = "Unreal Pak Replacement",
        .description = "Unreal Engine .pak file replacement with localized assets.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = false,
            .supportsRollback = true,
            .requiresRestart = true,
            .modifiesOriginalFiles = false, // Adds new paks
            .requiresRuntime = false,
            .supportsHotReload = false,
            .riskLevel = 35,
            .stabilityScore = 80,
            .performanceImpact = 5,
            .worksWithAntiCheat = false,
            .worksWithDRM = true,
            .surviveUpdates = true // Usually survives if priority is right
        },
        .supportedEngines = {GameEngine::Unreal},
        .baseScore = 78,
        .safetyBonus = 15,
        .performanceBonus = 10
    });

    // === HYBRID METHODS ===

    // Runtime + Binary hybrid
    registerMethod({
        .method = TranslationMethod::Hybrid_RuntimePlusBinary,
        .name = "Hybrid (Runtime+Binary)",
        .description = "Uses runtime hooks for UI text and binary patching for static content. "
                      "Best coverage for complex Unity IL2CPP games.",
        .capabilities = {
            .supportsExtraction = true,
            .supportsApplication = true,
            .supportsIncremental = true,
            .supportsRollback = true,
            .requiresRestart = true,
            .modifiesOriginalFiles = true,
            .requiresRuntime = true,
            .supportsHotReload = false,
            .riskLevel = 55,
            .stabilityScore = 65,
            .performanceImpact = 15,
            .worksWithAntiCheat = false,
            .worksWithDRM = true,
            .surviveUpdates = false
        },
        .supportedEngines = {GameEngine::Unity_IL2CPP},
        .baseScore = 70,
        .safetyBonus = 0,
        .performanceBonus = 5
    });

    MAKINEAI_LOG_INFO(log::PIPELINE, "MethodRegistry: Registered {} translation methods", methods_.size());
    Metrics::instance().gauge("pipeline_registered_methods", static_cast<double>(methods_.size()));
}

void MethodRegistry::registerMethod(const MethodInfo& info) {
    methods_[info.method] = info;

    for (auto engine : info.supportedEngines) {
        engineMethods_[engine].push_back(info.method);
    }
}

const MethodInfo* MethodRegistry::getMethodInfo(TranslationMethod method) const {
    auto it = methods_.find(method);
    return it != methods_.end() ? &it->second : nullptr;
}

std::vector<TranslationMethod> MethodRegistry::getMethodsForEngine(GameEngine engine) const {
    auto it = engineMethods_.find(engine);
    return it != engineMethods_.end() ? it->second : std::vector<TranslationMethod>{};
}

MethodCapability MethodRegistry::getCapabilities(TranslationMethod method) const {
    auto info = getMethodInfo(method);
    return info ? info->capabilities : MethodCapability{};
}

bool MethodRegistry::isMethodAvailable(TranslationMethod method, GameEngine engine) const {
    auto methods = getMethodsForEngine(engine);
    return std::find(methods.begin(), methods.end(), method) != methods.end();
}

std::vector<MethodInfo> MethodRegistry::getAllMethods() const {
    std::vector<MethodInfo> result;
    result.reserve(methods_.size());
    for (const auto& [_, info] : methods_) {
        result.push_back(info);
    }
    return result;
}

// ============================================================================
// DECISION ENGINE IMPLEMENTATION
// ============================================================================

DecisionEngine::DecisionEngine() = default;
DecisionEngine::~DecisionEngine() = default;

Result<MethodDecision> DecisionEngine::selectMethod(
    const GameInfo& game,
    const GameAnalysis& analysis,
    const PipelineOptions& options
) {
    auto decisionTimer = Metrics::instance().timer("pipeline_decision");

    MAKINEAI_LOG_INFO(log::PIPELINE, "DecisionEngine: Selecting method for {} ({})",
                 game.name, engineToString(game.engine));

    // If user forced a method, use it
    if (options.forcedMethod != TranslationMethod::Unknown) {
        auto& registry = MethodRegistry::instance();
        if (registry.isMethodAvailable(options.forcedMethod, game.engine)) {
            MAKINEAI_LOG_INFO(log::PIPELINE, "DecisionEngine: Using forced method: {}",
                        methodToString(options.forcedMethod));

            return MethodDecision{
                .primaryMethod = options.forcedMethod,
                .fallbackMethod = getFallbackMethod(options.forcedMethod, game, analysis),
                .secondaryMethod = std::nullopt,
                .confidence = DecisionConfidence::High,
                .rationale = "User requested specific method",
                .warnings = {},
                .requirements = {},
                .capabilities = registry.getCapabilities(options.forcedMethod),
                .estimatedCoverage = 100,
                .estimatedTime = 30
            };
        } else {
            MAKINEAI_LOG_WARN(log::PIPELINE, "Forced method {} not available for engine {}",
                methodToString(options.forcedMethod), engineToString(game.engine));
            return std::unexpected(Error(ErrorCode::InvalidArgument,
                "Forced method is not available for this game engine"));
        }
    }

    // Score all available methods
    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Scoring {} available methods", analysis.supportedMethods.size());
    std::vector<std::pair<TranslationMethod, int>> scores;
    for (auto method : analysis.supportedMethods) {
        int score = scoreMethod(method, game, analysis, options);
        scores.emplace_back(method, score);
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Decision tree: {} scored {}", methodToString(method), score);
    }

    if (scores.empty()) {
        MAKINEAI_LOG_WARN(log::PIPELINE, "No translation methods available for game: {}", game.name);
        return std::unexpected(Error(ErrorCode::NotSupported,
            "No translation methods available for this game"));
    }

    // Sort by score (descending)
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    auto bestMethod = scores[0].first;
    int bestScore = scores[0].second;

    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Best method after scoring: {} (score: {})",
        methodToString(bestMethod), bestScore);

    // Check if hybrid would be beneficial
    std::optional<TranslationMethod> secondary;
    if (options.allowHybrid && game.engine == GameEngine::Unity_IL2CPP) {
        secondary = selectSecondaryMethod(bestMethod, analysis);
        if (secondary) {
            MAKINEAI_LOG_DEBUG(log::PIPELINE, "Selected secondary method for hybrid: {}",
                methodToString(*secondary));
        }
    }

    // Calculate confidence
    auto confidence = calculateConfidence(bestMethod, bestScore, analysis);

    // Record confidence score in histogram
    Metrics::instance().recordHistogram("pipeline_confidence_scores", static_cast<int>(confidence));

    // Generate rationale
    auto rationale = generateRationale(bestMethod, game, analysis, bestScore);

    // Get fallback
    auto fallback = options.autoFallback ?
        getFallbackMethod(bestMethod, game, analysis) : std::nullopt;

    if (fallback) {
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Fallback method available: {}", methodToString(*fallback));
    }

    // Build warnings
    std::vector<std::string> warnings;
    auto& registry = MethodRegistry::instance();
    auto caps = registry.getCapabilities(bestMethod);

    if (caps.riskLevel > 50) {
        warnings.push_back("This method modifies game files directly. Backup recommended.");
        MAKINEAI_LOG_WARN(log::PIPELINE, "High risk method selected (risk: {})", caps.riskLevel);
    }
    if (caps.requiresRuntime && analysis.hasAntiCheat) {
        warnings.push_back("Anti-cheat detected. Runtime method may not work.");
        MAKINEAI_LOG_WARN(log::PIPELINE, "Anti-cheat detected but runtime method selected");
    }
    if (!caps.surviveUpdates) {
        warnings.push_back("Translation will need to be reapplied after game updates.");
    }

    // Build requirements
    std::vector<std::string> requirements;
    if (caps.requiresRuntime) {
        requirements.push_back("BepInEx runtime installation required");
        if (bestMethod == TranslationMethod::RuntimeHook_XUnity) {
            requirements.push_back("XUnity.AutoTranslator plugin required");
        }
    }

    MethodDecision decision{
        .primaryMethod = bestMethod,
        .fallbackMethod = fallback,
        .secondaryMethod = secondary,
        .confidence = confidence,
        .rationale = rationale,
        .warnings = std::move(warnings),
        .requirements = std::move(requirements),
        .capabilities = caps,
        .estimatedCoverage = 100,
        .estimatedTime = caps.requiresRuntime ? 60 : 30
    };

    MAKINEAI_LOG_INFO(log::PIPELINE, "DecisionEngine: Selected {} (confidence: {}, score: {})",
                 methodToString(bestMethod),
                 static_cast<int>(confidence), bestScore);

    // Audit log the translation decision
    AuditLogger::instance().log({
        .severity = AuditSeverity::Info,
        .category = AuditCategory::PatchOperation,
        .action = "method_decision",
        .target = game.name,
        .details = "method=" + std::string(methodToString(bestMethod)) +
                  " confidence=" + std::to_string(static_cast<int>(confidence)) +
                  " score=" + std::to_string(bestScore),
        .success = true
    });

    return decision;
}

int DecisionEngine::scoreMethod(
    TranslationMethod method,
    const GameInfo& game,
    const GameAnalysis& analysis,
    const PipelineOptions& options
) const {
    auto& registry = MethodRegistry::instance();
    auto info = registry.getMethodInfo(method);
    if (!info) {
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Method {} not found in registry", static_cast<int>(method));
        return 0;
    }

    // Start with base score
    float score = static_cast<float>(info->baseScore);
    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Scoring {}: base={}", methodToString(method), info->baseScore);

    // Apply weighted component scores
    float safetyScore = weights_.safety * scoreSafety(method, analysis);
    float coverageScore = weights_.coverage * scoreCoverage(method, analysis);
    float perfScore = weights_.performance * scorePerformance(method);
    float compatScore = weights_.compatibility * scoreCompatibility(method, analysis);
    float updateScore = weights_.updateSurvival * scoreUpdateSurvival(method);

    score += safetyScore + coverageScore + perfScore + compatScore + updateScore;

    MAKINEAI_LOG_DEBUG(log::PIPELINE,
        "Scoring {}: safety={:.1f} coverage={:.1f} perf={:.1f} compat={:.1f} update={:.1f}",
        methodToString(method), safetyScore, coverageScore, perfScore, compatScore, updateScore);

    // Apply bonuses
    if (options.preferSafeMethods) {
        score += info->safetyBonus * 0.5f;
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Applied safety bonus: {:.1f}", info->safetyBonus * 0.5f);
    } else {
        score += info->performanceBonus * 0.5f;
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Applied performance bonus: {:.1f}", info->performanceBonus * 0.5f);
    }

    // Penalty for anti-cheat incompatibility
    if (analysis.hasAntiCheat && info->capabilities.requiresRuntime) {
        score -= 30;
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Applied anti-cheat penalty: -30");
    }

    // Penalty for existing mods with runtime methods
    if (analysis.hasExistingMods && info->capabilities.requiresRuntime) {
        score -= 10; // Might conflict
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Applied existing mods penalty: -10");
    }

    int finalScore = std::clamp(static_cast<int>(score), 0, 100);
    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Final score for {}: {}", methodToString(method), finalScore);

    return finalScore;
}

int DecisionEngine::scoreSafety(TranslationMethod method, const GameAnalysis& analysis) const {
    auto caps = MethodRegistry::instance().getCapabilities(method);
    int score = 100 - caps.riskLevel;

    if (!caps.modifiesOriginalFiles) score += 20;
    if (caps.supportsRollback) score += 10;

    return std::clamp(score, 0, 100);
}

int DecisionEngine::scoreCoverage(TranslationMethod method, const GameAnalysis& analysis) const {
    // This would ideally use actual string count analysis
    // For now, use heuristics based on method type

    if (isRuntimeMethod(method)) {
        return 95; // Runtime catches most strings
    } else if (isFileBasedMethod(method)) {
        return 90; // File-based covers localization files
    } else if (isBinaryMethod(method)) {
        return 70; // Binary may miss some strings
    } else if (isAssetMethod(method)) {
        return 85; // Asset replacement is fairly complete
    } else if (isHybridMethod(method)) {
        return 98; // Hybrid has best coverage
    }

    return 50;
}

int DecisionEngine::scorePerformance(TranslationMethod method) const {
    auto caps = MethodRegistry::instance().getCapabilities(method);
    return 100 - caps.performanceImpact;
}

int DecisionEngine::scoreCompatibility(TranslationMethod method, const GameAnalysis& analysis) const {
    auto caps = MethodRegistry::instance().getCapabilities(method);
    int score = caps.stabilityScore;

    if (caps.worksWithDRM) score += 10;
    if (analysis.hasAntiCheat && !caps.worksWithAntiCheat) score -= 50;

    return std::clamp(score, 0, 100);
}

int DecisionEngine::scoreUpdateSurvival(TranslationMethod method) const {
    auto caps = MethodRegistry::instance().getCapabilities(method);
    return caps.surviveUpdates ? 100 : 30;
}

std::optional<TranslationMethod> DecisionEngine::getFallbackMethod(
    TranslationMethod primaryMethod,
    const GameInfo& game,
    const GameAnalysis& analysis
) const {
    // Define fallback chains
    static const std::map<TranslationMethod, TranslationMethod> fallbackMap = {
        {TranslationMethod::RuntimeHook_XUnity, TranslationMethod::Binary_IL2CPP},
        {TranslationMethod::RuntimeHook_BepInEx, TranslationMethod::RuntimeHook_XUnity},
        {TranslationMethod::Binary_IL2CPP, TranslationMethod::Asset_Unity_Resources},
        {TranslationMethod::FileBased_Locres, TranslationMethod::Asset_Unreal_Pak},
        {TranslationMethod::Asset_Unreal_Pak, TranslationMethod::FileBased_Locres}
    };

    auto it = fallbackMap.find(primaryMethod);
    if (it != fallbackMap.end()) {
        auto& registry = MethodRegistry::instance();
        if (registry.isMethodAvailable(it->second, game.engine)) {
            return it->second;
        }
    }

    return std::nullopt;
}

std::optional<TranslationMethod> DecisionEngine::selectSecondaryMethod(
    TranslationMethod primary,
    const GameAnalysis& analysis
) const {
    // For Unity IL2CPP, combine runtime with binary for best coverage
    if (isRuntimeMethod(primary)) {
        return TranslationMethod::Binary_IL2CPP;
    }
    return std::nullopt;
}

DecisionConfidence DecisionEngine::calculateConfidence(
    TranslationMethod method,
    int score,
    const GameAnalysis& analysis
) const {
    // High scores with good analysis = high confidence
    if (score >= 85 && analysis.supportedMethods.size() > 0) {
        return DecisionConfidence::VeryHigh;
    } else if (score >= 70) {
        return DecisionConfidence::High;
    } else if (score >= 55) {
        return DecisionConfidence::Medium;
    } else if (score >= 40) {
        return DecisionConfidence::Low;
    }
    return DecisionConfidence::VeryLow;
}

std::string DecisionEngine::generateRationale(
    TranslationMethod method,
    const GameInfo& game,
    const GameAnalysis& analysis,
    int score
) const {
    auto info = MethodRegistry::instance().getMethodInfo(method);
    if (!info) return "Unknown method selected";

    std::string rationale;

    switch (method) {
        case TranslationMethod::RuntimeHook_XUnity:
            rationale = "XUnity.AutoTranslator is the safest and most reliable method for Unity games. "
                       "It hooks text at runtime without modifying original files.";
            break;

        case TranslationMethod::FileBased_JSON:
            rationale = "JSON file patching is the standard method for RPG Maker MV/MZ games. "
                       "Safe, simple, and fully supported.";
            break;

        case TranslationMethod::FileBased_Script:
            rationale = "Ren'Py games use script files (.rpy) that can be safely patched. "
                       "This is the recommended method for visual novels.";
            break;

        case TranslationMethod::Binary_IL2CPP:
            rationale = "IL2CPP binary patching directly modifies game metadata. "
                       "Used when runtime hooks are not viable (e.g., anti-cheat games).";
            break;

        case TranslationMethod::Binary_DataWin:
            rationale = "GameMaker games require data.win binary modification. "
                       "This is the only viable method for this engine.";
            break;

        case TranslationMethod::FileBased_Strings:
            rationale = "Bethesda games use .strings localization files. "
                       "Direct modification is safe and well-tested.";
            break;

        case TranslationMethod::FileBased_Locres:
            rationale = "Unreal Engine .locres files contain all localization data. "
                       "Patching these is the primary method for Unreal games.";
            break;

        case TranslationMethod::Asset_Unreal_Pak:
            rationale = "Unreal .pak file replacement adds translated assets without modifying originals. "
                       "Good for games that verify file integrity.";
            break;

        case TranslationMethod::Hybrid_RuntimePlusBinary:
            rationale = "Hybrid approach uses runtime hooks for dynamic UI text and binary patching "
                       "for static content. Provides maximum coverage for complex games.";
            break;

        default:
            rationale = info->description;
    }

    rationale += " (Score: " + std::to_string(score) + "/100)";

    return rationale;
}

// ============================================================================
// GAME ANALYZER IMPLEMENTATION
// ============================================================================

GameAnalyzer::GameAnalyzer() = default;
GameAnalyzer::~GameAnalyzer() = default;

Result<GameAnalysis> GameAnalyzer::analyze(const GameInfo& game) {
    MAKINEAI_TIMED_SCOPE_INFO(log::PIPELINE, "game_analysis");
    MAKINEAI_LOG_INFO(log::PIPELINE, "GameAnalyzer: Analyzing {} ({})", game.name, engineToString(game.engine));

    switch (game.engine) {
        case GameEngine::Unity_Mono:
        case GameEngine::Unity_IL2CPP:
            return analyzeUnity(game);

        case GameEngine::Unreal:
            return analyzeUnreal(game);

        case GameEngine::RenPy:
            return analyzeRenPy(game);

        case GameEngine::RPGMaker_MV:
        case GameEngine::RPGMaker_VX:
            return analyzeRPGMaker(game);

        case GameEngine::GameMaker:
            return analyzeGameMaker(game);

        case GameEngine::Bethesda:
            return analyzeBethesda(game);

        default:
            MAKINEAI_LOG_WARN(log::PIPELINE, "Unsupported game engine: {}", engineToString(game.engine));
            return std::unexpected(Error(ErrorCode::NotSupported,
                "Unsupported game engine: " + std::string(engineToString(game.engine))));
    }
}

std::vector<TranslationMethod> GameAnalyzer::quickAnalyze(const GameInfo& game) {
    return MethodRegistry::instance().getMethodsForEngine(game.engine);
}

bool GameAnalyzer::isMethodViable(const GameInfo& game, TranslationMethod method) {
    return MethodRegistry::instance().isMethodAvailable(method, game.engine);
}

bool GameAnalyzer::detectExistingMods(const fs::path& gameDir) {
    // Check for common mod indicators
    std::vector<std::string> modIndicators = {
        "BepInEx",
        "doorstop_config.ini",
        "winhttp.dll",
        "version.dll",
        "mods",
        "Mods"
    };

    for (const auto& indicator : modIndicators) {
        if (fs::exists(gameDir / indicator)) {
            MAKINEAI_LOG_DEBUG(log::PIPELINE, "Detected existing mods: {}", indicator);
            return true;
        }
    }

    return false;
}

bool GameAnalyzer::detectAntiCheat(const fs::path& gameDir) {
    // Check for common anti-cheat systems
    std::vector<std::string> antiCheatIndicators = {
        "EasyAntiCheat",
        "EasyAntiCheat_x64.dll",
        "EasyAntiCheat_x86.dll",
        "BattlEye",
        "BEService.exe",
        "vac",
        "PunkBuster",
        "nProtect",
        "Xigncode",
        "mhyprot2.sys"  // Genshin Impact
    };

    for (const auto& indicator : antiCheatIndicators) {
        if (fs::exists(gameDir / indicator)) {
            MAKINEAI_LOG_WARN(log::PIPELINE, "Detected anti-cheat: {}", indicator);
            return true;
        }

        // Also check subdirectories
        for (const auto& entry : fs::recursive_directory_iterator(gameDir,
            fs::directory_options::skip_permission_denied)) {
            if (entry.path().filename().string().find(indicator) != std::string::npos) {
                MAKINEAI_LOG_WARN(log::PIPELINE, "Detected anti-cheat in path: {}", entry.path().string());
                return true;
            }
        }
    }

    return false;
}

Result<GameAnalysis> GameAnalyzer::analyzeUnity(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = game.engine;
    analysis.is64Bit = game.is64Bit;

    // Determine supported methods based on scripting backend
    if (game.engine == GameEngine::Unity_IL2CPP) {
        analysis.supportedMethods = {
            TranslationMethod::RuntimeHook_XUnity,
            TranslationMethod::RuntimeHook_BepInEx,
            TranslationMethod::Binary_IL2CPP,
            TranslationMethod::Hybrid_RuntimePlusBinary
        };
        analysis.recommendedMethods = {TranslationMethod::RuntimeHook_XUnity};
    } else {
        analysis.supportedMethods = {
            TranslationMethod::RuntimeHook_XUnity,
            TranslationMethod::RuntimeHook_BepInEx
        };
        analysis.recommendedMethods = {TranslationMethod::RuntimeHook_XUnity};
    }

    // Detect mods and anti-cheat
    analysis.hasExistingMods = detectExistingMods(game.installPath);
    analysis.hasAntiCheat = detectAntiCheat(game.installPath);

    // Find targets
    discoverBinaryTargets(game.installPath, analysis);

    // Calculate capabilities for each method
    calculateCapabilities(analysis);

    MAKINEAI_LOG_INFO(log::PIPELINE, "GameAnalyzer: Unity analysis complete - {} supported methods",
                 analysis.supportedMethods.size());

    return analysis;
}

Result<GameAnalysis> GameAnalyzer::analyzeUnreal(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = GameEngine::Unreal;
    analysis.is64Bit = game.is64Bit;

    analysis.supportedMethods = {
        TranslationMethod::FileBased_Locres,
        TranslationMethod::Asset_Unreal_Pak
    };
    analysis.recommendedMethods = {TranslationMethod::FileBased_Locres};

    analysis.hasExistingMods = detectExistingMods(game.installPath);
    analysis.hasAntiCheat = detectAntiCheat(game.installPath);

    discoverLocalizationFiles(game.installPath, analysis);
    discoverAssetTargets(game.installPath, analysis);
    calculateCapabilities(analysis);

    return analysis;
}

Result<GameAnalysis> GameAnalyzer::analyzeRenPy(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = GameEngine::RenPy;
    analysis.is64Bit = game.is64Bit;

    analysis.supportedMethods = {TranslationMethod::FileBased_Script};
    analysis.recommendedMethods = {TranslationMethod::FileBased_Script};

    discoverLocalizationFiles(game.installPath, analysis);
    calculateCapabilities(analysis);

    return analysis;
}

Result<GameAnalysis> GameAnalyzer::analyzeRPGMaker(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = game.engine;
    analysis.is64Bit = game.is64Bit;

    analysis.supportedMethods = {TranslationMethod::FileBased_JSON};
    analysis.recommendedMethods = {TranslationMethod::FileBased_JSON};

    discoverLocalizationFiles(game.installPath, analysis);
    calculateCapabilities(analysis);

    return analysis;
}

Result<GameAnalysis> GameAnalyzer::analyzeGameMaker(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = GameEngine::GameMaker;
    analysis.is64Bit = game.is64Bit;

    analysis.supportedMethods = {TranslationMethod::Binary_DataWin};
    analysis.recommendedMethods = {TranslationMethod::Binary_DataWin};

    discoverBinaryTargets(game.installPath, analysis);
    calculateCapabilities(analysis);

    return analysis;
}

Result<GameAnalysis> GameAnalyzer::analyzeBethesda(const GameInfo& game) {
    GameAnalysis analysis;
    analysis.engine = GameEngine::Bethesda;
    analysis.is64Bit = game.is64Bit;

    analysis.supportedMethods = {TranslationMethod::FileBased_Strings};
    analysis.recommendedMethods = {TranslationMethod::FileBased_Strings};

    discoverLocalizationFiles(game.installPath, analysis);
    calculateCapabilities(analysis);

    return analysis;
}

void GameAnalyzer::discoverLocalizationFiles(const fs::path& gameDir, GameAnalysis& analysis) {
    // Common localization file patterns
    std::vector<std::string> patterns = {
        ".json", ".strings", ".dlstrings", ".ilstrings",
        ".locres", ".rpy", ".po", ".mo"
    };

    try {
        for (const auto& entry : fs::recursive_directory_iterator(gameDir,
            fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                for (const auto& pattern : patterns) {
                    if (ext == pattern) {
                        analysis.localizationFiles.push_back(
                            fs::relative(entry.path(), gameDir).string());
                    }
                }
            }
        }
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Found {} localization files", analysis.localizationFiles.size());
    } catch (const std::exception& e) {
        MAKINEAI_LOG_WARN(log::PIPELINE, "Error scanning for localization files: {}", e.what());
    }
}

void GameAnalyzer::discoverBinaryTargets(const fs::path& gameDir, GameAnalysis& analysis) {
    // IL2CPP metadata
    auto metadataPath = gameDir / "GameName_Data" / "il2cpp_data" / "Metadata" / "global-metadata.dat";
    // Try common patterns
    for (const auto& entry : fs::directory_iterator(gameDir, fs::directory_options::skip_permission_denied)) {
        if (entry.is_directory() && entry.path().filename().string().ends_with("_Data")) {
            auto ilPath = entry.path() / "il2cpp_data" / "Metadata" / "global-metadata.dat";
            if (fs::exists(ilPath)) {
                analysis.binaryTargets.push_back(fs::relative(ilPath, gameDir).string());
            }
        }
    }

    // GameMaker data.win
    auto dataWin = gameDir / "data.win";
    if (fs::exists(dataWin)) {
        analysis.binaryTargets.push_back("data.win");
    }
}

void GameAnalyzer::discoverAssetTargets(const fs::path& gameDir, GameAnalysis& analysis) {
    // Unreal .pak files
    try {
        for (const auto& entry : fs::recursive_directory_iterator(gameDir,
            fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && entry.path().extension() == ".pak") {
                analysis.assetTargets.push_back(fs::relative(entry.path(), gameDir).string());
            }
        }
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Found {} asset targets", analysis.assetTargets.size());
    } catch (const std::exception& e) {
        MAKINEAI_LOG_WARN(log::PIPELINE, "Error scanning for asset files: {}", e.what());
    }
}

void GameAnalyzer::calculateCapabilities(GameAnalysis& analysis) {
    for (auto method : analysis.supportedMethods) {
        analysis.methodCapabilities[method] = MethodRegistry::instance().getCapabilities(method);
    }
}

// ============================================================================
// TRANSLATION PIPELINE IMPLEMENTATION
// ============================================================================

TranslationPipeline::TranslationPipeline()
    : analyzer_(std::make_unique<GameAnalyzer>())
    , decisionEngine_(std::make_unique<DecisionEngine>()) {
}

TranslationPipeline::~TranslationPipeline() = default;

Result<PipelineContext> TranslationPipeline::execute(
    const GameInfo& game,
    const std::vector<TranslationEntry>& translations,
    const PipelineOptions& options,
    ProgressCallback progressCallback
) {
    auto batchTimer = Metrics::instance().timer("pipeline_batch");

    MAKINEAI_LOG_INFO(log::PIPELINE, "TranslationPipeline: Starting execution for {}", game.name);
    MAKINEAI_LOG_INFO(log::PIPELINE, "Batch processing start: {} translations for {}",
        translations.size(), game.name);

    // Audit log the start of translation operation
    AuditLogger::logPatchOperation(game.name, true, "start",
        "translations=" + std::to_string(translations.size()));

    PipelineContext ctx;
    ctx.game = game;
    ctx.totalSteps = 5; // 5 phases

    // Phase 1: Analyze
    MAKINEAI_LOG_INFO(log::PIPELINE, "Phase 1/5: Analyzing game");
    emitEvent(PipelinePhase::Analyze, "Analyzing game...", 0);
    auto analysisResult = analyzeGame(game);
    if (!analysisResult) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Analysis failed: {}", analysisResult.error().message());
        AuditLogger::logPatchOperation(game.name, false, "analyze_failed",
            analysisResult.error().message());
        return std::unexpected(analysisResult.error());
    }
    ctx.analysis = *analysisResult;
    ctx.completedSteps++;

    // Phase 2: Decide
    MAKINEAI_LOG_INFO(log::PIPELINE, "Phase 2/5: Selecting translation method");
    emitEvent(PipelinePhase::Decide, "Selecting translation method...", 20);
    auto decisionResult = selectMethod(game, ctx.analysis, options);
    if (!decisionResult) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Method selection failed: {}", decisionResult.error().message());
        AuditLogger::logPatchOperation(game.name, false, "decision_failed",
            decisionResult.error().message());
        return std::unexpected(decisionResult.error());
    }
    ctx.decision = *decisionResult;
    ctx.completedSteps++;

    MAKINEAI_LOG_INFO(log::PIPELINE, "TranslationPipeline: Selected method: {} (confidence: {})",
                 methodToString(ctx.decision.primaryMethod),
                 static_cast<int>(ctx.decision.confidence));

    // Phase 3: Prepare
    MAKINEAI_LOG_INFO(log::PIPELINE, "Phase 3/5: Preparing for translation");
    emitEvent(PipelinePhase::Prepare, "Preparing for translation...", 40);
    auto prepResult = prepare(ctx, progressCallback);
    if (!prepResult) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Preparation failed: {}", prepResult.error().message());
        AuditLogger::logPatchOperation(game.name, false, "prepare_failed",
            prepResult.error().message());
        return std::unexpected(prepResult.error());
    }
    ctx.completedSteps++;

    // Phase 4: Apply
    MAKINEAI_LOG_INFO(log::PIPELINE, "Phase 4/5: Applying translations");
    emitEvent(PipelinePhase::Apply, "Applying translations...", 60);
    auto applyResult = apply(ctx, translations, progressCallback);
    if (!applyResult) {
        if (options.autoFallback && ctx.decision.fallbackMethod) {
            MAKINEAI_LOG_WARN(log::PIPELINE, "Primary method failed, trying fallback: {}",
                methodToString(*ctx.decision.fallbackMethod));
            Metrics::instance().increment("pipeline_fallback_attempts");
            auto fallbackResult = handleFailureWithFallback(ctx, translations, progressCallback);
            if (!fallbackResult) {
                MAKINEAI_LOG_ERROR(log::PIPELINE, "Fallback also failed: {}", fallbackResult.error().message());
                AuditLogger::logPatchOperation(game.name, false, "apply_failed",
                    "primary and fallback both failed");
                return std::unexpected(fallbackResult.error());
            }
        } else {
            MAKINEAI_LOG_ERROR(log::PIPELINE, "Apply failed: {}", applyResult.error().message());
            AuditLogger::logPatchOperation(game.name, false, "apply_failed",
                applyResult.error().message());
            return std::unexpected(applyResult.error());
        }
    }
    ctx.completedSteps++;

    // Phase 5: Verify
    MAKINEAI_LOG_INFO(log::PIPELINE, "Phase 5/5: Verifying translation");
    emitEvent(PipelinePhase::Verify, "Verifying translation...", 90);
    auto verifyResult = verify(ctx);
    if (!verifyResult) {
        ctx.errors.push_back("Verification failed: " + verifyResult.error().message());
        MAKINEAI_LOG_WARN(log::PIPELINE, "Verification failed (non-fatal): {}", verifyResult.error().message());
        // Don't fail entirely on verify - just log
    }
    ctx.completedSteps++;

    ctx.success = true;
    emitEvent(PipelinePhase::Verify, "Translation complete!", 100);

    MAKINEAI_LOG_INFO(log::PIPELINE, "TranslationPipeline: Completed successfully - {} strings applied",
                 ctx.appliedCount);
    MAKINEAI_LOG_INFO(log::PIPELINE, "Batch processing end: {} applied, {} failed",
        ctx.appliedCount, ctx.failedCount);

    // Record final metrics
    Metrics::instance().increment("pipeline_successful_operations");
    Metrics::instance().gauge("pipeline_last_applied_count", static_cast<double>(ctx.appliedCount));

    // Audit log successful completion
    AuditLogger::logPatchOperation(game.name, true, "complete",
        "applied=" + std::to_string(ctx.appliedCount) +
        " failed=" + std::to_string(ctx.failedCount) +
        " method=" + std::string(methodToString(ctx.decision.primaryMethod)));

    return ctx;
}

Result<GameAnalysis> TranslationPipeline::analyzeGame(const GameInfo& game) {
    return analyzer_->analyze(game);
}

Result<MethodDecision> TranslationPipeline::selectMethod(
    const GameInfo& game,
    const GameAnalysis& analysis,
    const PipelineOptions& options
) {
    return decisionEngine_->selectMethod(game, analysis, options);
}

VoidResult TranslationPipeline::prepare(
    PipelineContext& ctx,
    ProgressCallback progressCallback
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Starting preparation phase");
    logPhase(ctx, "Starting preparation phase");

    // Create backup if requested
    if (ctx.decision.capabilities.modifiesOriginalFiles) {
        MAKINEAI_LOG_INFO(log::PIPELINE, "Creating backup (method modifies original files)");
        logPhase(ctx, "Creating backup...");

        // Use handler to create backup
        auto& factory = EngineHandlerFactory::instance();
        auto handler = factory.getHandlerForGame(ctx.game.installPath);
        if (handler) {
            auto timestamp = std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
            auto backupResult = handler->createBackup(ctx.game.installPath, "pre_translation_" + timestamp);
            if (backupResult && backupResult->success) {
                ctx.backupCreated = true;
                ctx.backupId = backupResult->backupId;
                MAKINEAI_LOG_INFO(log::PIPELINE, "Backup created: {}", ctx.backupId);
                logPhase(ctx, "Backup created: " + ctx.backupId);

                AuditLogger::logFileAccess(ctx.game.installPath, "backup", true,
                    "backupId=" + ctx.backupId);
            } else {
                MAKINEAI_LOG_ERROR(log::PIPELINE, "Failed to create backup");
                AuditLogger::logFileAccess(ctx.game.installPath, "backup", false, "backup failed");
                return std::unexpected(Error(ErrorCode::BackupFailed, "Failed to create backup"));
            }
        }
    }

    // Install runtime if needed
    if (ctx.decision.capabilities.requiresRuntime) {
        MAKINEAI_LOG_INFO(log::PIPELINE, "Installing translation runtime (BepInEx/XUnity)");
        logPhase(ctx, "Installing translation runtime...");
        RuntimeManager runtimeMgr;
        auto installResult = runtimeMgr.install(ctx.game, progressCallback);
        if (!installResult) {
            MAKINEAI_LOG_ERROR(log::PIPELINE, "Failed to install translation runtime");
            return std::unexpected(Error(ErrorCode::RuntimeInstallFailed,
                "Failed to install translation runtime"));
        }
        MAKINEAI_LOG_INFO(log::PIPELINE, "Runtime installed successfully");
        logPhase(ctx, "Runtime installed successfully");
    }

    return {};
}

VoidResult TranslationPipeline::apply(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progressCallback
) {
    logPhase(ctx, "Starting apply phase with method: " +
             std::string(methodToString(ctx.decision.primaryMethod)));

    if (isRuntimeMethod(ctx.decision.primaryMethod)) {
        return applyRuntimeMethod(ctx, translations, progressCallback);
    } else if (isFileBasedMethod(ctx.decision.primaryMethod)) {
        return applyFileBasedMethod(ctx, translations, progressCallback);
    } else if (isBinaryMethod(ctx.decision.primaryMethod)) {
        return applyBinaryMethod(ctx, translations, progressCallback);
    } else if (isAssetMethod(ctx.decision.primaryMethod)) {
        return applyAssetMethod(ctx, translations, progressCallback);
    } else if (isHybridMethod(ctx.decision.primaryMethod)) {
        return applyHybridMethod(ctx, translations, progressCallback);
    }

    return std::unexpected(Error(ErrorCode::NotSupported, "Unknown translation method"));
}

VoidResult TranslationPipeline::verify(PipelineContext& ctx) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Starting verification phase");
    logPhase(ctx, "Starting verification phase");

    auto& factory = EngineHandlerFactory::instance();
    auto handler = factory.getHandlerForGame(ctx.game.installPath);

    if (handler) {
        // Use handler's validation
        std::vector<TranslationEntry> dummyEntries; // We'd need actual entries here
        auto validationResult = handler->validatePatch(ctx.game.installPath, dummyEntries);
        if (validationResult && validationResult->isValid) {
            MAKINEAI_LOG_INFO(log::PIPELINE, "Verification passed");
            logPhase(ctx, "Verification passed");
            Metrics::instance().increment("pipeline_verification_passed");
            return {};
        } else {
            MAKINEAI_LOG_WARN(log::PIPELINE, "Verification found issues");
            logPhase(ctx, "Verification found issues");
            Metrics::instance().increment("pipeline_verification_warnings");
            // Don't fail - just log
        }
    } else {
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "No handler available for verification, skipping");
    }

    return {};
}

VoidResult TranslationPipeline::rollback(PipelineContext& ctx) {
    if (!ctx.backupCreated || ctx.backupId.empty()) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "No backup available for rollback");
        return std::unexpected(Error(ErrorCode::InvalidArgument, "No backup available for rollback"));
    }

    MAKINEAI_LOG_INFO(log::PIPELINE, "Rolling back using backup: {}", ctx.backupId);
    logPhase(ctx, "Rolling back using backup: " + ctx.backupId);

    auto& factory = EngineHandlerFactory::instance();
    auto handler = factory.getHandlerForGame(ctx.game.installPath);

    if (handler) {
        auto restoreResult = handler->restoreBackup(ctx.game.installPath, ctx.backupId);
        if (restoreResult && restoreResult->success) {
            MAKINEAI_LOG_INFO(log::PIPELINE, "Rollback successful");
            logPhase(ctx, "Rollback successful");
            AuditLogger::logPatchOperation(ctx.game.name, true, "rollback",
                "backupId=" + ctx.backupId);
            Metrics::instance().increment("pipeline_rollbacks");
            return {};
        }
    }

    MAKINEAI_LOG_ERROR(log::PIPELINE, "Rollback failed for backup: {}", ctx.backupId);
    AuditLogger::logPatchOperation(ctx.game.name, false, "rollback",
        "backupId=" + ctx.backupId + " failed");
    return std::unexpected(Error(ErrorCode::RestoreFailed, "Rollback failed"));
}

VoidResult TranslationPipeline::applyRuntimeMethod(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Applying runtime translation method");
    logPhase(ctx, "Applying runtime translation method");

    // For runtime methods, we write translation files that XUnity will load
    RuntimeManager runtimeMgr;

    // Create translation directory
    auto translationDir = ctx.game.installPath / "BepInEx" / "Translation" / "en" / "Text";
    std::error_code ec;
    fs::create_directories(translationDir, ec);
    if (ec) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Failed to create translation directory: {}", ec.message());
    }

    // Write translations in XUnity format
    fs::path outputFile = translationDir / "_MakineAI_Translations.txt";
    std::ofstream ofs(outputFile);
    if (!ofs) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Cannot create translation file: {}", outputFile.string());
        return std::unexpected(Error(ErrorCode::FileAccessDenied,
            "Cannot create translation file"));
    }

    ofs << "// MakineAI Translations\n";
    ofs << "// Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";

    int tmHits = 0;
    int glossaryHits = 0;

    for (const auto& entry : translations) {
        if (entry.targetText && !entry.targetText->empty()) {
            // XUnity format: original=translated
            ofs << entry.sourceText << "=" << *entry.targetText << "\n";
            ctx.appliedCount++;

            // Track decision source
            if (entry.context && entry.context->find("tm_match") != std::string::npos) {
                tmHits++;
                MAKINEAI_LOG_DEBUG(log::PIPELINE, "TM hit for: {}", entry.sourceText.substr(0, 50));
            } else if (entry.context && entry.context->find("glossary") != std::string::npos) {
                glossaryHits++;
                MAKINEAI_LOG_DEBUG(log::PIPELINE, "Glossary match for: {}", entry.sourceText.substr(0, 50));
            }
        }
    }

    ofs.close();

    // Record metrics
    Metrics::instance().increment("pipeline_tm_hits", tmHits);
    Metrics::instance().increment("pipeline_glossary_hits", glossaryHits);

    MAKINEAI_LOG_INFO(log::PIPELINE, "Written {} translations (TM hits: {}, glossary: {})",
        ctx.appliedCount, tmHits, glossaryHits);
    logPhase(ctx, "Written " + std::to_string(ctx.appliedCount) + " translations");

    AuditLogger::logFileAccess(outputFile, "write", true,
        "count=" + std::to_string(ctx.appliedCount));

    return {};
}

VoidResult TranslationPipeline::applyFileBasedMethod(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Applying file-based translation method");
    logPhase(ctx, "Applying file-based translation method");

    auto& factory = EngineHandlerFactory::instance();
    auto handler = factory.getHandlerForGame(ctx.game.installPath);

    if (!handler) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "No handler available for game at: {}", ctx.game.installPath.string());
        return std::unexpected(Error(ErrorCode::NotSupported,
            "No handler available for this game"));
    }

    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Using handler for {} translations", translations.size());

    PatchOptions patchOpts;
    patchOpts.createBackup = false; // We already created backup in prepare phase

    auto result = handler->applyTranslations(ctx.game.installPath, translations, patchOpts);
    if (!result) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Handler patch failed");
        Metrics::instance().increment("pipeline_api_calls"); // Even failed attempts count
        return std::unexpected(Error(ErrorCode::PatchFailed, "Handler patch failed"));
    }

    ctx.appliedCount = result->appliedCount;
    ctx.failedCount = result->skippedCount;

    // Track API call metric
    Metrics::instance().increment("pipeline_api_calls");

    if (!result->success) {
        for (const auto& error : result->errors) {
            ctx.errors.push_back(error.message);
            MAKINEAI_LOG_WARN(log::PIPELINE, "Patch error: {}", error.message);
        }
        MAKINEAI_LOG_WARN(log::PIPELINE, "Translation application partially failed: {} applied, {} skipped",
            ctx.appliedCount, ctx.failedCount);
        return std::unexpected(Error(ErrorCode::PatchFailed,
            "Translation application partially failed"));
    }

    MAKINEAI_LOG_INFO(log::PIPELINE, "Applied {} translations via file-based method", ctx.appliedCount);
    logPhase(ctx, "Applied " + std::to_string(ctx.appliedCount) + " translations");
    return {};
}

VoidResult TranslationPipeline::applyBinaryMethod(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Applying binary translation method");
    logPhase(ctx, "Applying binary translation method");
    // Same as file-based - handlers handle the specifics
    return applyFileBasedMethod(ctx, translations, progress);
}

VoidResult TranslationPipeline::applyAssetMethod(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Applying asset replacement method");
    logPhase(ctx, "Applying asset replacement method");
    return applyFileBasedMethod(ctx, translations, progress);
}

VoidResult TranslationPipeline::applyHybridMethod(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    MAKINEAI_LOG_INFO(log::PIPELINE, "Applying hybrid translation method");
    logPhase(ctx, "Applying hybrid translation method");

    // First apply runtime method
    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Hybrid step 1: Applying runtime method");
    auto runtimeResult = applyRuntimeMethod(ctx, translations, progress);
    if (!runtimeResult) {
        ctx.errors.push_back("Runtime portion failed: " + runtimeResult.error().message());
        MAKINEAI_LOG_WARN(log::PIPELINE, "Hybrid runtime portion failed: {}", runtimeResult.error().message());
    }

    // Then apply secondary method if specified
    if (ctx.decision.secondaryMethod) {
        MAKINEAI_LOG_DEBUG(log::PIPELINE, "Hybrid step 2: Applying secondary method {}",
            methodToString(*ctx.decision.secondaryMethod));

        auto tempDecision = ctx.decision;
        ctx.decision.primaryMethod = *ctx.decision.secondaryMethod;

        VoidResult secondaryResult;
        if (isBinaryMethod(*ctx.decision.secondaryMethod)) {
            secondaryResult = applyBinaryMethod(ctx, translations, progress);
        } else if (isFileBasedMethod(*ctx.decision.secondaryMethod)) {
            secondaryResult = applyFileBasedMethod(ctx, translations, progress);
        }

        ctx.decision = tempDecision;

        if (!secondaryResult) {
            ctx.errors.push_back("Secondary method failed: " + secondaryResult.error().message());
            MAKINEAI_LOG_WARN(log::PIPELINE, "Hybrid secondary method failed: {}", secondaryResult.error().message());
        }
    }

    MAKINEAI_LOG_INFO(log::PIPELINE, "Hybrid method completed");
    return {};
}

void TranslationPipeline::emitEvent(PipelinePhase phase, const std::string& message, int progress) {
    if (eventCallback_) {
        eventCallback_(phase, message, progress);
    }
}

void TranslationPipeline::logPhase(PipelineContext& ctx, const std::string& message) {
    ctx.log.push_back(message);
    MAKINEAI_LOG_DEBUG(log::PIPELINE, "Pipeline: {}", message);
}

VoidResult TranslationPipeline::handleFailureWithFallback(
    PipelineContext& ctx,
    const std::vector<TranslationEntry>& translations,
    ProgressCallback progress
) {
    if (!ctx.decision.fallbackMethod) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "No fallback method available");
        return std::unexpected(Error(ErrorCode::PatchFailed, "No fallback method available"));
    }

    MAKINEAI_LOG_WARN(log::PIPELINE, "Attempting fallback to: {}",
        methodToString(*ctx.decision.fallbackMethod));
    logPhase(ctx, "Attempting fallback method: " +
             std::string(methodToString(*ctx.decision.fallbackMethod)));

    // Rollback first if we modified files
    if (ctx.backupCreated) {
        MAKINEAI_LOG_INFO(log::PIPELINE, "Rolling back before fallback attempt");
        auto rollbackResult = rollback(ctx);
        if (!rollbackResult) {
            MAKINEAI_LOG_ERROR(log::PIPELINE, "Rollback failed before fallback: {}", rollbackResult.error().message());
            return std::unexpected(Error(ErrorCode::RestoreFailed,
                "Failed to rollback before fallback"));
        }
    }

    // Switch to fallback method
    auto originalMethod = ctx.decision.primaryMethod;
    ctx.decision.primaryMethod = *ctx.decision.fallbackMethod;
    ctx.decision.fallbackMethod = std::nullopt;

    MAKINEAI_LOG_INFO(log::PIPELINE, "Switched from {} to {} (fallback)",
        methodToString(originalMethod), methodToString(ctx.decision.primaryMethod));

    // Re-prepare if needed
    auto prepResult = prepare(ctx, progress);
    if (!prepResult) {
        MAKINEAI_LOG_ERROR(log::PIPELINE, "Fallback preparation failed: {}", prepResult.error().message());
        return std::unexpected(prepResult.error());
    }

    // Try again
    MAKINEAI_LOG_INFO(log::PIPELINE, "Retrying with fallback method");
    return apply(ctx, translations, progress);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

TranslationMethod getDefaultMethod(GameEngine engine) {
    auto methods = MethodRegistry::instance().getMethodsForEngine(engine);
    if (methods.empty()) {
        return TranslationMethod::Unknown;
    }

    // Return first recommended method
    switch (engine) {
        case GameEngine::Unity_Mono:
        case GameEngine::Unity_IL2CPP:
            return TranslationMethod::RuntimeHook_XUnity;

        case GameEngine::Unreal:
            return TranslationMethod::FileBased_Locres;

        case GameEngine::RenPy:
            return TranslationMethod::FileBased_Script;

        case GameEngine::RPGMaker_MV:
        case GameEngine::RPGMaker_VX:
            return TranslationMethod::FileBased_JSON;

        case GameEngine::GameMaker:
            return TranslationMethod::Binary_DataWin;

        case GameEngine::Bethesda:
            return TranslationMethod::FileBased_Strings;

        default:
            return methods[0];
    }
}

} // namespace makineai
