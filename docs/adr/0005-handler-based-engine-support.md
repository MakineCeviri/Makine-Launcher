# ADR-0005: Handler-Based Engine Support

## Status

Accepted

## Date

2026-01-18

## Context

Different game engines store translatable text in completely different formats:
- **Unity**: Asset bundles, IL2CPP metadata, Mono assemblies
- **Unreal**: PAK files, localization databases
- **RPG Maker**: JSON data files (Map, System, Actors)
- **Ren'Py**: Python script files with translation blocks
- **GameMaker**: data.win binary format

Each engine requires:
- Different file parsing logic
- Different string extraction methods
- Different patching approaches
- Different runtime integration (if any)

We need a way to support multiple engines without:
- Monolithic switch statements
- Tight coupling between detection and translation
- Difficulty adding new engine support

## Decision

Implement a **Handler Pattern** where each game engine has a dedicated handler class
implementing a common interface.

Interface:
```cpp
class IEngineHandler {
public:
    virtual ~IEngineHandler() = default;
    virtual std::string name() const = 0;
    virtual GameEngine engineType() const = 0;
    virtual bool canHandle(const GameInfo& game) const = 0;
    virtual int confidence(const GameInfo& game) const = 0;

    virtual Result<std::vector<StringEntry>> extractStrings(
        const GameInfo& game,
        ProgressCallback progress = nullptr
    ) const = 0;

    virtual Result<PatchResult> applyTranslations(
        const GameInfo& game,
        const std::vector<TranslationEntry>& translations,
        ProgressCallback progress = nullptr
    ) const = 0;

    virtual Result<BackupResult> createBackup(const GameInfo& game) const = 0;
    virtual Result<RestoreResult> restoreBackup(
        const GameInfo& game,
        const std::string& backupId
    ) const = 0;
};
```

Base class provides common functionality:
```cpp
class EngineHandlerBase : public IEngineHandler {
protected:
    Result<void> writeFileAtomic(const fs::path& path, WriteFunc func);
    bool validateFileReadable(const fs::path& path);
    fs::path getBackupPath(const GameInfo& game);
    // ... more utilities
};
```

Handler registration:
```cpp
// In UnityHandler.cpp
static bool registered = HandlerRegistry::instance().registerHandler(
    std::make_unique<UnityHandler>()
);
```

Handler selection:
```cpp
auto handler = HandlerRegistry::instance().findBest(game);
// Returns handler with highest confidence() score
```

## Consequences

### Positive

- Clean separation of engine-specific logic
- Easy to add new engine support
- Handlers can be tested independently
- Common utilities reduce code duplication
- Confidence scoring enables graceful degradation

### Negative

- Interface must be general enough for all engines
- Some engines may need handler-specific APIs
- Handler registration uses static initialization (order issues)

### Neutral

- Each new engine requires implementing ~10 methods
- Testing requires mock games or fixtures

## Alternatives Considered

### Alternative 1: Strategy Pattern with Lambdas

Use function objects instead of classes.

```cpp
struct EngineStrategy {
    std::function<Result<...>(...)> extractStrings;
    std::function<Result<...>(...)> applyTranslations;
    // ...
};
```

**Rejected because:**
- Loses inheritance benefits (shared utilities)
- Harder to maintain state between operations
- Less discoverable in IDE

### Alternative 2: Visitor Pattern

Game objects accept visitors that perform operations.

**Rejected because:**
- Inverts control flow confusingly
- Doesn't fit well with external game files
- Overcomplicated for this use case

### Alternative 3: Plugin System

Load engine support as DLLs.

**Rejected because:**
- Deployment complexity
- ABI stability concerns
- Debugging difficulty
- Most engines are known in advance

## Related

- [ADR-0003](0003-translation-pipeline-decision-engine.md) - Pipeline uses handlers
- [ADR-0001](0001-native-cpp-architecture.md) - C++ architecture

## Notes

Current handlers:

| Handler | Engine | Status | Lines |
|---------|--------|--------|-------|
| UnityHandler | Unity/IL2CPP | Implemented | 1866 |
| UnrealHandler | Unreal 4/5 | Implemented | 1607 |
| RPGMakerHandler | RPG Maker MV/MZ | Implemented | 1549 |
| RenPyHandler | Ren'Py | Implemented | 668 |
| GameMakerHandler | GameMaker Studio | Implemented | 1124 |

Each handler provides:
- `canHandle()`: Check engine markers (files, signatures)
- `confidence()`: Return 0-100 confidence score
- `extractStrings()`: Parse game files, return StringEntry list
- `applyTranslations()`: Modify files with translations
- `createBackup()` / `restoreBackup()`: Safe backup/restore

Handler base class utilities:
- `writeFileAtomic()`: Write with temp file + rename
- `validateFileReadable()`: Check file access
- `isFileLocked()`: Detect if game is running
- `getBackupPath()`: Generate backup directory
- Progress reporting helpers
