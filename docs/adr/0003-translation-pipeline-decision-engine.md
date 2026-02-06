# ADR-0003: Translation Pipeline Decision Engine

## Status

Accepted

## Date

2026-01-23

## Context

Games can be translated using multiple methods:
- **Runtime hooks** (BepInEx + XUnity.AutoTranslator) - Intercept text at runtime
- **File patching** - Modify data files directly (JSON, CSV, binary)
- **Asset modification** - Edit Unity bundles, Unreal paks
- **Binary patching** - Modify hardcoded strings in executables

Each method has trade-offs:
- Runtime hooks work for most Unity games but require maintained runtime
- File patching is stable but may break on game updates
- Binary patching is permanent but risky and game-specific

Previously, method selection was manual. Users had to:
1. Understand their game's engine
2. Know which methods are supported
3. Make the right choice (often incorrectly)

## Decision

Implement an algorithmic decision engine that automatically selects the best
translation method based on:
- Game engine detection
- Available file formats
- Anti-cheat presence
- User preferences (stability vs features)

The pipeline has 5 phases:
```
Analyze → Decide → Prepare → Apply → Verify
```

Decision factors (weighted scoring):
- **Compatibility** (30%): Does method work with this engine?
- **Stability** (25%): How resilient to game updates?
- **Quality** (20%): Translation accuracy and completeness
- **Safety** (15%): Risk of game corruption
- **Performance** (10%): Impact on game performance

Method registry defines capabilities:
```cpp
MethodRegistry::instance().register(TranslationMethod::BepInExAutoTranslator, {
    .supportedEngines = {GameEngine::Unity, GameEngine::UnityIL2CPP},
    .capabilities = MethodCapability::Runtime | MethodCapability::TextHook,
    .stabilityRating = 0.7f,
    .safetyRating = 0.9f
});
```

## Consequences

### Positive

- Users don't need to understand technical details
- Consistent, optimal method selection
- Fallback chains handle partial failures
- Hybrid methods (Runtime + Binary) for best coverage
- Scoring is transparent and explainable

### Negative

- Complex logic to maintain
- May make suboptimal choices for edge cases
- Requires keeping engine detection up to date
- Some users want manual control

### Neutral

- Method registry needs updates as new methods emerge
- Testing requires many game samples

## Alternatives Considered

### Alternative 1: Manual Method Selection Only

Let users choose the translation method themselves.

**Rejected because:**
- Users often choose wrong method
- Requires technical knowledge
- Support burden for "why doesn't it work"

### Alternative 2: Simple Engine→Method Mapping

Static mapping: Unity→BepInEx, Unreal→FilePatch, etc.

**Rejected because:**
- Doesn't account for game-specific factors
- No fallback on failure
- Can't handle hybrid approaches
- Ignores user preferences

### Alternative 3: Machine Learning Classification

Train a model on game characteristics to predict best method.

**Rejected because:**
- Overkill for this problem
- Needs large training dataset
- Black box decision making
- Maintenance complexity

## Related

- [ADR-0005](0005-handler-based-engine-support.md) - Engine handlers execute chosen method

## Notes

The decision engine produces a `MethodDecision`:

```cpp
struct MethodDecision {
    TranslationMethod primaryMethod;
    TranslationMethod fallbackMethod;
    DecisionConfidence confidence;
    std::string reasoning;
    std::map<std::string, float> scores;
};
```

Confidence levels guide user communication:
- `VeryHigh` (>90%): Auto-proceed
- `High` (70-90%): Proceed with info
- `Medium` (50-70%): Ask for confirmation
- `Low` (<50%): Warn and suggest alternatives
