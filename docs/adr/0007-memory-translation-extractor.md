# ADR-0007: Memory Translation Extractor

## Status

Accepted

## Date

2026-02-19

## Context

ADR-0006 established the adaptation engine direction: detect game updates → diff analysis → re-adapt translations. However, one critical missing piece remained: **how to capture the complete translation data from a working installation**.

Many community translation packages use protected mechanisms:
- **Encrypted caches** (AES-256 encrypted translation databases)
- **Integrity-checked ASI plugins** (any byte modification breaks the binary)
- **Obfuscated encodings** (custom character substitutions like ǔ→i)
- **Auto-update mechanisms** (phone home to GitHub for new versions)

These protections make it impossible to directly modify or redistribute the translation files. We need an alternative approach.

### RDR2 Case Study (Proof of Concept)

Red Dead Redemption 2's Turkish translation uses a ScriptHook ASI plugin that:
1. Downloads an AES-encrypted cache from GitHub (3.1 MB)
2. Decrypts it at runtime using embedded auth tokens
3. Hooks `rage::fwTextStore::FindTextAndSlot` to intercept text lookups
4. Replaces English text with Turkish text using a hash-based lookup table
5. Has full binary integrity verification (any byte change = crash)

We proved that **process memory scanning** can extract the decrypted translation data:
- 3,869 MB of game memory scanned across 16,161 regions
- 191,953 unique hash→text mappings extracted
- Encoding obfuscation (ǔ→i, Ǔ→İ) decoded automatically
- GXT2-like data structures identified and parsed
- All data captured without modifying the game binary

## Decision

Build a **Memory Translation Extractor** as a core component of the adaptation engine. This tool:

1. Attaches to a running game process (read-only, no injection)
2. Scans process memory for translation strings
3. Detects engine-specific text format structures
4. Extracts hash→text mappings
5. Decodes obfuscated encodings
6. Produces a clean, structured translation database

This approach is **engine-agnostic at its core** but supports **pluggable engine modules** for format-specific extraction.

### Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Memory Translation Extractor          │
├─────────────────────────────────────────────────────┤
│                                                       │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Process   │  │ Memory       │  │ String        │  │
│  │ Scanner   │→ │ Reader       │→ │ Extractor     │  │
│  └──────────┘  └──────────────┘  └───────┬───────┘  │
│                                           │          │
│  ┌──────────────────────────────────────┐ │          │
│  │         Analysis Pipeline            │ │          │
│  │  ┌────────────┐  ┌────────────────┐  │ │          │
│  │  │ Encoding   │  │ Format         │  │◄┘          │
│  │  │ Detector   │  │ Detector       │  │            │
│  │  └────────────┘  └────────────────┘  │            │
│  │  ┌────────────┐  ┌────────────────┐  │            │
│  │  │ Hash Table │  │ Quality        │  │            │
│  │  │ Scanner    │  │ Filter         │  │            │
│  │  └────────────┘  └────────────────┘  │            │
│  └──────────────────────────────────────┘            │
│                       │                              │
│  ┌────────────────────▼─────────────────────────┐    │
│  │          Engine Modules (pluggable)            │   │
│  │  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ │   │
│  │  │ RAGE   │ │ UE4/5  │ │ Unity  │ │Generic │ │   │
│  │  │ GXT2   │ │ LocRes │ │ I2/TMPro│ │        │ │   │
│  │  └────────┘ └────────┘ └────────┘ └────────┘ │   │
│  └──────────────────────────────────────────────┘    │
│                       │                              │
│  ┌────────────────────▼─────────────────────────┐    │
│  │           Translation Database                │    │
│  │   hash → text mappings, metadata, stats       │    │
│  └──────────────────────────────────────────────┘    │
│                                                       │
└─────────────────────────────────────────────────────┘
```

### Integration with Adaptation Engine

```
Pre-Update State                Post-Update State
     │                               │
     ▼                               ▼
[Memory Extract]              [File Diff / Re-extract]
     │                               │
     ▼                               ▼
[Translation DB v1]           [Changed Strings]
     │                               │
     └───────────┬───────────────────┘
                 ▼
        [Adaptation Engine]
         fuzzy match, merge
                 │
                 ▼
        [Updated Translation]
```

## Technical Details

### Core Components (engine-agnostic)

| Component | Responsibility |
|-----------|---------------|
| ProcessScanner | Find game process by name/PID, verify accessibility |
| MemoryReader | Read-only process memory via Win32 API (VirtualQueryEx, ReadProcessMemory) |
| RegionEnumerator | Enumerate committed, readable memory regions |
| StringExtractor | Multi-encoding string extraction (UTF-8, UTF-16LE, custom) |
| EncodingDetector | Detect and decode obfuscated encodings (pattern analysis) |
| QualityFilter | Remove false positives (binary garbage, non-text data) |
| HashTableScanner | Find structured hash→text tables in memory |
| TranslationDB | Store and query extracted translations |

### Engine Modules (pluggable)

| Module | Engine | Key Patterns |
|--------|--------|-------------|
| rage_gxt2 | RAGE (GTA, RDR) | `~z~` prefix, Jenkins hash, GXT2/2TXG magic, `0x90XXXXXX` meta |
| ue4_ftext | Unreal Engine 4/5 | FText serialization, LocRes header, FNV hash |
| unity_text | Unity | I2 Localization tables, TextMeshPro strings, MonoBehaviour fields |
| renpy_tl | Ren'Py | Python string literals, `renpy.translation` structures |
| generic | Any | Pure Turkish character scanning, no format assumptions |

### Proven Techniques (from RDR2 extraction)

1. **Turkish character fingerprinting**: İıŞşÇçĞğÖöÜü in UTF-8 have unique multi-byte sequences that reliably identify Turkish text in binary data
2. **Encoding obfuscation detection**: When a replacement character (ǔ) consistently appears where a native character (i) should be, statistical analysis detects the pattern
3. **Structure-adjacent scanning**: Hash entries can be found by looking at the 8 bytes preceding known text strings
4. **Region concentration analysis**: Translation data clusters in specific memory regions — early scanning reveals the hot zones
5. **Meta value patterns**: Structured entries have consistent meta byte patterns (e.g., 0x90XXXXXX) that distinguish real entries from coincidental byte matches
6. **Deduplication by hash**: Memory may contain multiple copies of the same text table — deduplicate by hash, keeping the longest text

### Extraction Statistics (RDR2 baseline)

| Metric | Value |
|--------|-------|
| Memory scanned | 3,869 MB |
| Regions enumerated | 16,161 |
| Scan time | ~29 minutes |
| Raw dialogue hits | 140,715 |
| Unique dialogue hashes | 140,637 |
| Unique general hashes | 51,316 |
| Total unique mappings | 191,953 |
| Database size | 47.4 MB |
| Encoding fixes applied | 59,328 strings |
| False positive rate (UTF-16LE) | ~90% (filtered out) |

## Consequences

### Positive

- **Engine-agnostic core**: Same memory scanning works for any game
- **Non-invasive**: Read-only access, no game modification required
- **Bypasses protections**: Encrypted caches, integrity checks, obfuscation — all irrelevant because we read the decrypted runtime state
- **Complete data**: Captures everything the game has loaded, including dynamically generated text
- **Foundation for adaptation**: Combined with file-level diff, enables automatic translation re-adaptation after game updates

### Negative

- **Requires running game**: Game must be running with the translation active
- **One-time capture**: Need to scan while the specific text is loaded in memory (story-dependent text may require multiple scans at different game states)
- **Windows-only**: Uses Win32 API (VirtualQueryEx, ReadProcessMemory)
- **Admin rights may be needed**: Some games run elevated or use anti-cheat that blocks memory reading
- **Anti-cheat interference**: EAC, BattlEye, Vanguard may block process attachment

### Mitigations

- For story-dependent text: Instruct user to load save at key story points and scan multiple times
- For anti-cheat: Most translations disable anti-cheat anyway (offline play)
- For Windows-only: Target platform is Windows (primary gaming OS)
- For admin rights: Makine-Launcher already requests elevation for game file operations

## Related

- **ADR-0006**: Adaptation Engine Direction (this builds on it)
- **ADR-0001**: Native C++ Architecture (implementation will use native Win32 + standard C++)
