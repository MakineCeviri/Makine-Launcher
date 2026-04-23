# Memory Translation Extractor — Developer Guide

## What Is This?

A process memory scanner that extracts translation data from running games.
Works by reading the decrypted/unpacked translation strings directly from game memory,
bypassing encryption, integrity checks, and obfuscation.

**Core insight**: No matter how a game protects its translation files on disk,
the text must eventually exist as readable strings in process memory.

## Architecture

```
Game Process (read-only)
    │
    ▼
┌──────────────────┐
│  Process Scanner  │  ← Find game by name/PID
└────────┬─────────┘
         │
┌────────▼─────────┐
│  Memory Reader    │  ← VirtualQueryEx + ReadProcessMemory
│  Region Enum.     │     Enumerate committed readable regions
└────────┬─────────┘
         │
┌────────▼─────────┐
│  String Extractor │  ← Multi-encoding (UTF-8, UTF-16LE)
│                   │     Turkish char fingerprinting
└────────┬─────────┘
         │
    ┌────▼────┐
    │ Engine  │  ← Pluggable format detection
    │ Module  │     RAGE, UE4, Unity, Generic...
    └────┬────┘
         │
┌────────▼─────────┐
│  Quality Filter   │  ← Remove false positives
│  Encoding Fixer   │     Decode obfuscation
└────────┬─────────┘
         │
┌────────▼─────────┐
│  Translation DB   │  ← hash→text JSON database
└──────────────────┘
```

## Core API Design

```cpp
namespace makine {

// Process attachment
struct ProcessInfo {
    uint32_t pid;
    std::string name;
    std::string path;
    uint64_t base_address;
};

// Memory region
struct MemoryRegion {
    uint64_t base;
    size_t size;
    uint32_t protection;
};

// Extracted translation entry
struct TranslationEntry {
    uint32_t hash;              // Engine-specific text hash
    std::string text;           // Decoded translation text
    std::string raw_text;       // Original text (with engine tags)
    std::string encoding;       // "utf-8", "utf-16-le"
    uint64_t address;           // Memory address where found
    std::string category;       // "dialogue", "ui", "item", etc.
};

// Extraction result
struct ExtractionResult {
    std::string game_name;
    std::string engine;
    size_t total_regions;
    size_t total_bytes_scanned;
    size_t scan_duration_ms;
    std::vector<TranslationEntry> entries;
    std::unordered_map<std::string, std::string> encoding_fixes;
};

// Main extractor interface
class MemoryExtractor {
public:
    // Attach to a running game process
    static std::optional<ProcessInfo> findProcess(std::string_view name);

    // Scan and extract translations
    ExtractionResult extract(const ProcessInfo& process,
                            const ExtractionConfig& config);

    // Register engine-specific modules
    void registerModule(std::unique_ptr<IEngineModule> module);
};

// Engine module interface
class IEngineModule {
public:
    virtual ~IEngineModule() = default;
    virtual std::string name() const = 0;
    virtual bool detectEngine(std::span<const uint8_t> sample) const = 0;
    virtual std::vector<TranslationEntry> parseRegion(
        std::span<const uint8_t> data,
        uint64_t base_addr) const = 0;
};

} // namespace makine
```

## Engine Module: RAGE (GTA/RDR)

Detection: Search for `GXT2` or `2TXG` magic bytes.

Parsing strategy:
1. Find `~z~` byte sequence (0x7E 0x7A 0x7E)
2. Read 8 bytes backwards: hash(4 LE) + meta(4 LE)
3. Read forward until null terminator for the text
4. Validate meta pattern (dominant: 0x90XXXXXX)
5. Decode ǔ→i obfuscation if detected

Key signatures:
```
~z~     = 7E 7A 7E        (dialogue marker)
~s~     = 7E 73 7E        (system style)
~sl:    = 7E 73 6C 3A     (subtitle timing)
GXT2    = 47 58 54 32     (format magic)
2TXG    = 32 54 58 47     (LE format magic)
```

## Engine Module: Unreal Engine 4/5

Detection: Search for `.locres` header magic or FText serialization patterns.

Parsing strategy:
1. Look for FText serialized format: [length(4)] [utf16-string] [null(2)]
2. Or LocRes header: magic bytes + version + namespace tables
3. Extract FNV hash from preceding structure
4. Decode UTF-16LE to UTF-8

Key signatures:
```
LocRes magic: varies by version (typically in first 16 bytes)
FText:        [int32 len] [char16[] data] [0x0000]
```

## Engine Module: Unity

Detection: Search for I2 Localization CSV patterns or TextMeshPro markers.

Parsing strategy:
1. Look for I2 CSV format: `key,English,Turkish,...`
2. Or serialized MonoBehaviour text fields
3. Or TextMeshPro `<TMP>` tagged strings
4. Extract by parsing CSV or Unity serialization format

## Engine Module: Generic

For unknown engines, fallback to pure string scanning:
1. Scan for Turkish character fingerprints (İıŞşÇçĞğÖöÜü)
2. Validate string quality (printable ratio > 80%)
3. Categorize by length and content patterns
4. No hash extraction (just text collection)

## Turkish Character Fingerprints

### UTF-8 Byte Sequences

| Char | UTF-8 Bytes | Notes |
|------|-------------|-------|
| İ | C4 B0 | Uppercase dotted I |
| ı | C4 B1 | Lowercase dotless i |
| Ş | C5 9E | |
| ş | C5 9F | |
| Ç | C3 87 | |
| ç | C3 A7 | |
| Ğ | C4 9E | |
| ğ | C4 9F | |
| Ö | C3 96 | |
| ö | C3 B6 | |
| Ü | C3 9C | |
| ü | C3 BC | |

### UTF-16LE Code Points

| Char | UTF-16LE | Risk of false positive |
|------|----------|----------------------|
| İ | 30 01 | HIGH — 0x0130 common in binary |
| ı | 31 01 | HIGH — 0x0131 common in binary |
| Ş | 5E 01 | MEDIUM |
| ş | 5F 01 | MEDIUM |
| Ç | C7 00 | LOW |
| ç | E7 00 | LOW |
| Ğ | 1E 01 | MEDIUM |
| ğ | 1F 01 | MEDIUM |
| Ö | D6 00 | LOW |
| ö | F6 00 | LOW |
| Ü | DC 00 | LOW |
| ü | FC 00 | LOW |

**Important**: UTF-16LE scanning produces ~90% false positives for İ and ı because
their code points (0x0130, 0x0131) commonly appear in binary data as small integers.
Always validate with printable ratio and word structure checks.

## Encoding Obfuscation Detection

Some translators use character substitution to protect their work:

### Detection Algorithm

```python
def detect_obfuscation(strings):
    """Detect if a non-Turkish char systematically replaces a Turkish char"""
    # Count character frequencies
    # If a rare Unicode char (e.g., ǔ U+01D4) appears with high frequency
    # AND the expected Turkish char (i) has unusually low frequency
    # → likely obfuscation

    # Pattern: ǔ appears in positions where Turkish grammar requires 'i'
    # Example: "bǔlǔnǔr" should be "bilinir" (not "bılınır")
    # Key test: check surrounding chars — if ı (dotless) also appears normally,
    # then ǔ is replacing dotted i specifically
```

### Known Obfuscation Mappings

| Translator | Method | Mapping |
|-----------|--------|---------|
| CriminaL/Deftones (RDR2) | Char substitution | ǔ(U+01D4)→i, Ǔ(U+01D3)→İ |

## Quality Filtering

### False Positive Indicators

1. **Printable ratio < 80%**: String contains too many control/non-printable chars
2. **Letter count < 3**: Not enough alphabetic characters to be real text
3. **Non-Latin extended chars > 30%**: Characters in U+024F-U+0370 range (rare in Turkish)
4. **Assembly patterns**: Strings like `A^_^[]`, `A\Üǔer` (register/instruction fragments)
5. **Path strings**: Contains `\\`, `.exe`, `.dll` (filesystem paths, not game text)
6. **UTF-16LE garbage**: High-byte values that accidentally match Turkish code points

### Quality Scoring

```
score = (printable_ratio * 0.4) +
        (turkish_char_presence * 0.3) +
        (word_structure * 0.2) +
        (length_appropriateness * 0.1)

// Accept if score > 0.7
```

## Integration with Makine-Launcher

### Adaptation Engine Workflow

```
1. User installs community translation (overlay/ASI/mod)
2. User runs Makine-Launcher → "Extract Translation Data"
3. Makine-Launcher attaches to running game → scans memory
4. Produces translation_db.json (hash→text mappings)
5. Stores DB as baseline for this game+version

--- Game updates ---

6. Makine-Launcher detects game update (file hash change)
7. User runs updated game (with/without old translation)
8. Option A: Re-extract from memory → diff with baseline
   Option B: File-level diff on game assets
9. Adaptation engine: merge changes into translation
10. Makine-Launcher packages adapted translation for installation
```

### CLI Usage (planned)

```bash
# Extract from running game
makine extract --process RDR2.exe --engine rage --output rdr2_tr.json

# Extract with auto-detect
makine extract --process "Elden Ring.exe" --output er_tr.json

# Compare two extraction databases
makine diff --base v1.0_tr.json --updated v1.1_tr.json --output changes.json

# Adapt translation to new version
makine adapt --translation v1.0_tr.json --changes changes.json --output v1.1_tr.json
```

## Performance Characteristics

Based on RDR2 extraction (3.8 GB memory footprint):

| Phase | Time | Notes |
|-------|------|-------|
| Region enumeration | <1s | 16K regions |
| Full memory scan | ~25 min | Sequential, single-threaded |
| String extraction | Included | During scan |
| Hash table scan | Included | During scan |
| Post-processing | ~5s | Dedup, filter, encode fix |
| Database write | ~3s | JSON serialization |

### Optimization Opportunities

1. **Parallel scanning**: Split regions across threads (4-8x speedup)
2. **Hot zone targeting**: Scan high-density regions first (>80% of data in first 400 MB)
3. **Incremental scanning**: Skip regions that haven't changed since last scan
4. **SIMD string matching**: Use SSE4.2 PCMPESTRI for Turkish char fingerprint search
