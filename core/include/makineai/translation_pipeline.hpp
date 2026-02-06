/**
 * @file translation_pipeline.hpp
 * @brief Translation Pipeline System - Algorithmic method selection and execution
 * @copyright (c) 2026 MakineAI Team
 *
 * The Translation Pipeline is the core decision engine that:
 * 1. Analyzes a game's structure and capabilities
 * 2. Selects the best translation method(s)
 * 3. Executes the translation in phases with fallback support
 * 4. Verifies results and handles errors
 *
 * Key Features:
 * - Multi-method support (runtime, file-based, binary, hybrid)
 * - Automatic fallback on failure
 * - Risk-based method selection
 * - Confidence scoring for decisions
 */

#pragma once

#include "types.hpp"
#include "error.hpp"
#include "handlers/engine_handler.hpp"

#include <functional>
#include <memory>
#include <map>

namespace makineai {

// Forward declarations
class RuntimeManager;
class IEngineHandler;

// ============================================================================
// METHOD REGISTRY
// ============================================================================

/**
 * @brief Method information for registry
 */
struct MethodInfo {
    TranslationMethod method;
    std::string name;
    std::string description;
    MethodCapability capabilities;
    std::vector<GameEngine> supportedEngines;

    // Scoring weights for decision engine
    int baseScore = 50;                 // Base priority score
    int safetyBonus = 0;                // Bonus for being safe
    int performanceBonus = 0;           // Bonus for being fast
};

/**
 * @brief Registry of all available translation methods
 *
 * Singleton that maintains metadata about all translation methods
 * and their capabilities for each game engine.
 */
class MethodRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static MethodRegistry& instance();

    /**
     * @brief Register a method with its capabilities
     */
    void registerMethod(const MethodInfo& info);

    /**
     * @brief Get method info
     */
    [[nodiscard]] const MethodInfo* getMethodInfo(TranslationMethod method) const;

    /**
     * @brief Get all methods for an engine
     */
    [[nodiscard]] std::vector<TranslationMethod> getMethodsForEngine(GameEngine engine) const;

    /**
     * @brief Get method capabilities
     */
    [[nodiscard]] MethodCapability getCapabilities(TranslationMethod method) const;

    /**
     * @brief Check if method is available for engine
     */
    [[nodiscard]] bool isMethodAvailable(TranslationMethod method, GameEngine engine) const;

    /**
     * @brief Get all registered methods
     */
    [[nodiscard]] std::vector<MethodInfo> getAllMethods() const;

private:
    MethodRegistry();
    void registerBuiltinMethods();

    std::map<TranslationMethod, MethodInfo> methods_;
    std::map<GameEngine, std::vector<TranslationMethod>> engineMethods_;
};

// ============================================================================
// DECISION ENGINE
// ============================================================================

/**
 * @brief Weights for method scoring
 */
struct ScoringWeights {
    float safety = 0.35f;           // Weight for safety/stability
    float coverage = 0.25f;         // Weight for string coverage
    float performance = 0.15f;      // Weight for speed
    float compatibility = 0.15f;    // Weight for mod/DRM compatibility
    float updateSurvival = 0.10f;   // Weight for surviving game updates
};

/**
 * @brief Decision engine that selects the best translation method
 *
 * Uses a scoring algorithm to select the optimal method based on:
 * - Game engine and version
 * - Available methods and their capabilities
 * - User preferences (safe vs fast)
 * - Game-specific factors (mods, anti-cheat, etc.)
 */
class DecisionEngine {
public:
    DecisionEngine();
    ~DecisionEngine();

    /**
     * @brief Analyze game and select best method
     *
     * @param game Game information
     * @param analysis Game analysis results
     * @param options Pipeline options
     * @return Method decision with rationale
     */
    [[nodiscard]] Result<MethodDecision> selectMethod(
        const GameInfo& game,
        const GameAnalysis& analysis,
        const PipelineOptions& options = {}
    );

    /**
     * @brief Score a specific method for a game
     *
     * @param method Method to score
     * @param game Game information
     * @param analysis Game analysis
     * @param options Options affecting scoring
     * @return Score 0-100 (higher is better)
     */
    [[nodiscard]] int scoreMethod(
        TranslationMethod method,
        const GameInfo& game,
        const GameAnalysis& analysis,
        const PipelineOptions& options = {}
    ) const;

    /**
     * @brief Get fallback method if primary fails
     *
     * @param primaryMethod The failed method
     * @param game Game information
     * @param analysis Game analysis
     * @return Fallback method or nullopt
     */
    [[nodiscard]] std::optional<TranslationMethod> getFallbackMethod(
        TranslationMethod primaryMethod,
        const GameInfo& game,
        const GameAnalysis& analysis
    ) const;

    /**
     * @brief Set scoring weights
     */
    void setWeights(const ScoringWeights& weights) { weights_ = weights; }

    /**
     * @brief Get current weights
     */
    [[nodiscard]] const ScoringWeights& weights() const { return weights_; }

private:
    ScoringWeights weights_;

    // Method-specific scoring
    int scoreSafety(TranslationMethod method, const GameAnalysis& analysis) const;
    int scoreCoverage(TranslationMethod method, const GameAnalysis& analysis) const;
    int scorePerformance(TranslationMethod method) const;
    int scoreCompatibility(TranslationMethod method, const GameAnalysis& analysis) const;
    int scoreUpdateSurvival(TranslationMethod method) const;

    // Hybrid method selection
    std::optional<TranslationMethod> selectSecondaryMethod(
        TranslationMethod primary,
        const GameAnalysis& analysis
    ) const;

    // Confidence calculation
    DecisionConfidence calculateConfidence(
        TranslationMethod method,
        int score,
        const GameAnalysis& analysis
    ) const;

    // Rationale generation
    std::string generateRationale(
        TranslationMethod method,
        const GameInfo& game,
        const GameAnalysis& analysis,
        int score
    ) const;
};

// ============================================================================
// GAME ANALYZER
// ============================================================================

/**
 * @brief Analyzes games to determine translation options
 */
class GameAnalyzer {
public:
    GameAnalyzer();
    ~GameAnalyzer();

    /**
     * @brief Perform full game analysis
     *
     * @param game Game information
     * @return Analysis result with supported methods
     */
    [[nodiscard]] Result<GameAnalysis> analyze(const GameInfo& game);

    /**
     * @brief Quick analysis for method compatibility
     *
     * @param game Game information
     * @return List of potentially supported methods
     */
    [[nodiscard]] std::vector<TranslationMethod> quickAnalyze(const GameInfo& game);

    /**
     * @brief Check if a specific method is viable for the game
     *
     * @param game Game information
     * @param method Method to check
     * @return true if method can be used
     */
    [[nodiscard]] bool isMethodViable(const GameInfo& game, TranslationMethod method);

    /**
     * @brief Detect existing modifications
     *
     * @param gameDir Game directory
     * @return true if mods detected
     */
    [[nodiscard]] bool detectExistingMods(const fs::path& gameDir);

    /**
     * @brief Detect anti-cheat presence
     *
     * @param gameDir Game directory
     * @return true if anti-cheat detected
     */
    [[nodiscard]] bool detectAntiCheat(const fs::path& gameDir);

private:
    // Engine-specific analysis
    [[nodiscard]] Result<GameAnalysis> analyzeUnity(const GameInfo& game);
    [[nodiscard]] Result<GameAnalysis> analyzeUnreal(const GameInfo& game);
    [[nodiscard]] Result<GameAnalysis> analyzeRenPy(const GameInfo& game);
    [[nodiscard]] Result<GameAnalysis> analyzeRPGMaker(const GameInfo& game);
    [[nodiscard]] Result<GameAnalysis> analyzeGameMaker(const GameInfo& game);
    [[nodiscard]] Result<GameAnalysis> analyzeBethesda(const GameInfo& game);

    // File discovery
    void discoverLocalizationFiles(const fs::path& gameDir, GameAnalysis& analysis);
    void discoverBinaryTargets(const fs::path& gameDir, GameAnalysis& analysis);
    void discoverAssetTargets(const fs::path& gameDir, GameAnalysis& analysis);

    // Capability calculation
    void calculateCapabilities(GameAnalysis& analysis);
};

// ============================================================================
// TRANSLATION PIPELINE
// ============================================================================

/**
 * @brief Phase execution result
 */
struct PhaseResult {
    PipelinePhase phase;
    bool success = true;
    std::string message;
    std::chrono::milliseconds duration{0};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/**
 * @brief Pipeline event callback
 */
using PipelineEventCallback = std::function<void(
    PipelinePhase phase,
    const std::string& message,
    int progressPercent
)>;

/**
 * @brief The Translation Pipeline - orchestrates the entire translation process
 *
 * This is the main class that ties together:
 * - Game analysis
 * - Method selection
 * - Translation application
 * - Verification and rollback
 */
class TranslationPipeline {
public:
    TranslationPipeline();
    ~TranslationPipeline();

    /**
     * @brief Execute full translation pipeline
     *
     * @param game Target game
     * @param translations Translations to apply
     * @param options Pipeline options
     * @param progressCallback Progress callback
     * @return Pipeline context with results
     */
    [[nodiscard]] Result<PipelineContext> execute(
        const GameInfo& game,
        const std::vector<TranslationEntry>& translations,
        const PipelineOptions& options = {},
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Execute specific phase
     *
     * @param ctx Pipeline context
     * @param phase Phase to execute
     * @param translations Translations (needed for Apply phase)
     * @return Phase result
     */
    [[nodiscard]] Result<PhaseResult> executePhase(
        PipelineContext& ctx,
        PipelinePhase phase,
        const std::vector<TranslationEntry>& translations = {}
    );

    /**
     * @brief Analyze game (Phase 1)
     *
     * @param game Game to analyze
     * @return Analysis result
     */
    [[nodiscard]] Result<GameAnalysis> analyzeGame(const GameInfo& game);

    /**
     * @brief Select best method (Phase 2)
     *
     * @param game Game info
     * @param analysis Analysis result
     * @param options Options
     * @return Method decision
     */
    [[nodiscard]] Result<MethodDecision> selectMethod(
        const GameInfo& game,
        const GameAnalysis& analysis,
        const PipelineOptions& options = {}
    );

    /**
     * @brief Prepare for translation (Phase 3)
     *
     * Creates backups, installs runtime if needed, etc.
     *
     * @param ctx Pipeline context
     * @param progressCallback Progress callback
     * @return Success/failure
     */
    [[nodiscard]] VoidResult prepare(
        PipelineContext& ctx,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Apply translations (Phase 4)
     *
     * @param ctx Pipeline context
     * @param translations Translations to apply
     * @param progressCallback Progress callback
     * @return Success/failure
     */
    [[nodiscard]] VoidResult apply(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progressCallback = nullptr
    );

    /**
     * @brief Verify translation success (Phase 5)
     *
     * @param ctx Pipeline context
     * @return Success/failure
     */
    [[nodiscard]] VoidResult verify(PipelineContext& ctx);

    /**
     * @brief Rollback translation
     *
     * @param ctx Pipeline context with backup info
     * @return Success/failure
     */
    [[nodiscard]] VoidResult rollback(PipelineContext& ctx);

    /**
     * @brief Set event callback
     */
    void setEventCallback(PipelineEventCallback callback) {
        eventCallback_ = std::move(callback);
    }

    /**
     * @brief Get analyzer instance
     */
    [[nodiscard]] GameAnalyzer& analyzer() { return *analyzer_; }

    /**
     * @brief Get decision engine
     */
    [[nodiscard]] DecisionEngine& decisionEngine() { return *decisionEngine_; }

private:
    std::unique_ptr<GameAnalyzer> analyzer_;
    std::unique_ptr<DecisionEngine> decisionEngine_;
    PipelineEventCallback eventCallback_;

    // Method executors
    [[nodiscard]] VoidResult applyRuntimeMethod(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );

    [[nodiscard]] VoidResult applyFileBasedMethod(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );

    [[nodiscard]] VoidResult applyBinaryMethod(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );

    [[nodiscard]] VoidResult applyAssetMethod(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );

    [[nodiscard]] VoidResult applyHybridMethod(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );

    // Helper methods
    void emitEvent(PipelinePhase phase, const std::string& message, int progress);
    void logPhase(PipelineContext& ctx, const std::string& message);

    // Fallback handling
    [[nodiscard]] VoidResult handleFailureWithFallback(
        PipelineContext& ctx,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress
    );
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Get default method for an engine
 */
[[nodiscard]] TranslationMethod getDefaultMethod(GameEngine engine);

/**
 * @brief Check if method is runtime-based
 */
[[nodiscard]] inline bool isRuntimeMethod(TranslationMethod method) {
    return method == TranslationMethod::RuntimeHook_XUnity ||
           method == TranslationMethod::RuntimeHook_BepInEx ||
           method == TranslationMethod::RuntimeHook_Custom;
}

/**
 * @brief Check if method is file-based
 */
[[nodiscard]] inline bool isFileBasedMethod(TranslationMethod method) {
    return method == TranslationMethod::FileBased_JSON ||
           method == TranslationMethod::FileBased_Strings ||
           method == TranslationMethod::FileBased_Locres ||
           method == TranslationMethod::FileBased_Script ||
           method == TranslationMethod::FileBased_INI ||
           method == TranslationMethod::FileBased_PO;
}

/**
 * @brief Check if method is binary-based
 */
[[nodiscard]] inline bool isBinaryMethod(TranslationMethod method) {
    return method == TranslationMethod::Binary_IL2CPP ||
           method == TranslationMethod::Binary_DataWin ||
           method == TranslationMethod::Binary_Generic;
}

/**
 * @brief Check if method is asset-based
 */
[[nodiscard]] inline bool isAssetMethod(TranslationMethod method) {
    return method == TranslationMethod::Asset_Unreal_Pak ||
           method == TranslationMethod::Asset_Unity_Resources ||
           method == TranslationMethod::Asset_Generic;
}

/**
 * @brief Check if method is hybrid
 */
[[nodiscard]] inline bool isHybridMethod(TranslationMethod method) {
    return method == TranslationMethod::Hybrid_RuntimePlusBinary ||
           method == TranslationMethod::Hybrid_RuntimePlusFile;
}

} // namespace makineai
