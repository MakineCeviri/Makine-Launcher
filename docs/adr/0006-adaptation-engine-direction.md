# ADR-0006: Adaptation Engine Direction

## Status

Accepted

## Date

2026-02-10

## Context

The original Makine-Launcher architecture (ADR-0003, ADR-0005) was designed around a string extraction pipeline with engine-specific handlers (UnityHandler, UnrealHandler, etc.) and a translation decision engine with weighted scoring. This approach had several fundamental problems:

1. **Redundant work**: Game modding communities already have mature tools for each engine (BepInEx for Unity, UE4SS for Unreal, RPG Maker modding tools, etc.). Reimplementing string extraction per engine is duplicating effort.

2. **Wrong problem**: The real pain point for Turkish game translators is not extraction — it's that game updates break existing translations. A translator spends weeks translating a game, then a patch drops and the translation is partially or fully broken.

3. **Unsustainable scope**: Supporting 7+ engine handlers with their own binary formats, file structures, and edge cases would require enormous ongoing maintenance.

## Decision

Pivot from string extraction pipeline to an **adaptation engine** model:

```
Game Update Detected → Diff Analysis → Automatic Re-adaptation → Verification
```

Specifically:

- **Makine** (product layer): Game detection, translation package distribution, install/uninstall with backup. Uses simple file overlay — no engine-specific handling.
- **MakineAI** (engine layer): Hash-based update detection, structural diff analysis, fuzzy string matching, automatic re-adaptation of existing community translation patches.

The handler pipeline, translation memory, glossary service, QA service, and string classifier have been removed from the codebase (~32K lines). Engine detection remains (file signature matching) to inform compatibility checks.

## Consequences

### Positive

- Focused scope: solve the real problem (broken translations after updates)
- Leverages community work: install pre-made packages instead of extracting strings
- Reduced maintenance: no per-engine handler code to maintain
- Clear value proposition: "your translation survives game updates"

### Negative

- Cannot create translations from scratch (depends on community packages)
- Adaptation engine is complex (fuzzy matching, binary diff, format-specific merge)
- No general-purpose translation tool capability

## Supersedes

- **ADR-0003** (Translation Pipeline Decision Engine) — Decision engine with weighted scoring removed
- **ADR-0005** (Handler-Based Engine Support) — All engine handlers removed (~6,800 lines)
