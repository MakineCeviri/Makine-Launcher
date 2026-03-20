# Decima Workshop (ShadelessFox/decima) - DS2 Support Research

**Date:** 2026-03-19
**Researcher:** Claude Code (automated)
**Purpose:** Evaluate Decima Workshop for DS2 Turkish translation pipeline

---

## 1. DS2 Support Status: NOT SUPPORTED

### Issue #88: "DEATH STRANDING 2 Support Request"
- **Opened:** Mar 18, 2026 by user `sonying`
- **Status:** Closed as "Not planned" (same day, within hours of DS2 PC release)
- **No response from ShadelessFox** explaining why or when
- **No labels assigned** (unlike HFW which got `feature`, `xg: hfw`, `xp: high` labels)

### Pull Requests
- **0 PRs** related to DS2, DSOTB, or Death Stranding 2 (open or closed)

### Branches
- Only **1 branch**: `master` (last commit Nov 23, 2025)
- **No DS2-related branches** exist

### Recent Commits (Last Activity)
- **Nov 23, 2025:** Model Viewer: Handle zeroed primitive's start and end indexes
- **Aug 23, 2025:** Core Editor: Allow changing orientation of the value panel
- **Jul 29, 2025:** Find Files: Add column sorter
- **No commits since Nov 2025** - project appears to have slowed development significantly
- **Zero DS2-related commits** exist

### GitHub Search
- **0 repositories** matching `decima "death stranding 2"` on all of GitHub
- **0 relevant forks** with DS2 work (21 forks total, most are mirrors)
- Notable fork: `4ogg/DecimaDS` - name suggests DS focus but no DS2 work visible

---

## 2. Currently Supported Games

### GameType Enum (source of truth)
```java
// decima-model/src/main/java/com/shade/decima/model/base/GameType.java
public enum GameType {
    DS("Death Stranding"),
    DSDC("Death Stranding (Director's Cut)"),
    HZD("Horizon Zero Dawn");
}
```

### Per-Game Metadata Files
Each game type has dedicated type metadata and file path databases:
- `ds_types.json.gz` / `ds_paths.txt.gz` - Death Stranding
- `dsdc_types.json.gz` / `dsdc_paths.txt.gz` - Death Stranding: Director's Cut
- `hzd_types.json.gz` / `hzd_paths.txt.gz` - Horizon Zero Dawn

### Wiki-Documented Support (Archives page)
| Game | Archive Format |
|------|---------------|
| Horizon Zero Dawn | PackFile Archive (plain) |
| Death Stranding / DS:DC | PackFile Archive (encrypted) |
| Horizon Forbidden West (PC) | DirectStorage Archive (DSAR) |
| Horizon Zero Dawn Remastered (PC) | DirectStorage Archive (DSAR) |

### HFW Support Status
- **Issue #67**: "HorizonForbiddenWest Support Request" - **still OPEN**
- Labels: `feature`, `xg: hfw`, `xp: high` (high priority)
- Archive format is documented but **NOT yet functional in the tool**
- HFW/HZDR use DSAR format (magic `"DSAR"`, LZ4 compression) which is fundamentally different from PackFile

---

## 3. Archive Format Details

### PackFile Archive (HZD - Plain)
- Magic: `0x20304050`
- Structure: Header -> FileEntries -> ChunkEntries
- Files referenced by MurmurHash3 of normalized path
- Chunks are Oodle-compressed blocks (max 256KB each)

### PackFile Archive (DS/DS:DC - Encrypted)
- Magic: `0x21304050`
- Same structure as plain but with MurmurHash3-based encryption
- **Decryption Keys:**
  - HEADER_KEY: `43 94 3A FA 62 AB 1C F4 1C 81 76 F3 3E 9E A8 D2`
  - DATA_KEY: `37 4A 08 6C 95 9D 15 7E E8 F7 5A 3D 3F 7D AA 18`
- Header decrypted with: `data XOR murmurhash3(key_with_file_key, seed=42)`
- Data chunks decrypted similarly with DATA_KEY

### DirectStorage Archive (HFW/HZDR - NOT YET IN TOOL)
- Magic: `"DSAR"`
- Version: 3.1
- LZ4 compression (not Oodle)
- Requires external metadata to understand contents:
  - HFW: `LocalCacheWinGame/package/streaming_graph.core`
  - HZDR: `LocalCacheDX12/package/PackFileLocators.bin`

### DS2 Archive Format: UNKNOWN
- DS2 uses an updated Decima Engine
- Likely candidates:
  1. Enhanced encrypted PackFile (same as DS:DC but with new keys)
  2. DirectStorage Archive (DSAR) like HFW (PS5-era engine)
  3. Entirely new format
- **This needs empirical investigation** by examining DS2 game files

---

## 4. RTTI / Type Registry System

### Architecture
```
ProjectContainer.getTypeMetadata() -> loads game-specific JSON type definitions
    -> RTTITypeRegistry (via ServiceLoader pattern)
        -> RTTITypeProvider.initialize() populates registry
        -> Types cached by: name, hash (CRC64), Java class
```

### Core Binary Format (RTTIBinaryVersion 2)
Used by HZD, DS, and DS:DC:
```c
// .core file structure
while (!EOF) {
    uint64 type_id;    // RTTI type hash
    uint32 size;       // data size in bytes
    ubyte  data[size]; // serialized object
}
```

### Key Source Files
- `GameType.java` - Enum defining supported games
- `ProjectContainer.java` - Loads game-specific metadata, maps GameType -> resources
- `RTTITypeRegistry.java` - Central type registry, resolves types by name/hash/class
- `RTTIType.java` - Base class for all RTTI types (read/write/instantiate/copyOf)
- `RTTIClass.java` - RTTI class definition with fields
- `Packfile.java` - Archive reader/writer with encryption support
- `CoreBinary.java` - .core file reader (reads type_id + size + data entries)

### Adding DS2 Would Require
1. New `GameType` enum value (e.g., `DS2`)
2. New metadata files: `ds2_types.json.gz` + `ds2_paths.txt.gz`
3. Potentially new archive reader (if format changed)
4. Potentially new encryption keys (if encrypted PackFile)
5. New RTTI type definitions extracted from DS2 executable

---

## 5. Localization System

### Built-in CLI (Decima Workshop)
```shell
# Export localization
decima-cli.exe localization export -p "path/to/game" -o output/ -l English

# Import localization
decima-cli.exe localization import -p "path/to/game" -s source/

# Supports: simpletext.core, sentences.core files
```

### What Localization Files Contain
- `simpletext.core` - Simple key-value text strings
- `sentences.core` - Dialogue/narrative sentences with metadata
- Export formats: JSON (structured), potentially others
- Languages identified by enum in the localization resource types

### Release Note (v0.1.25)
> "CLI: Export/import localization strings using `localization export` and `localization import`.
> This covers almost all localization in DS/HZD, allowing translation of the entire game to foreign languages."

### Alternative Tool: decima-loc (YouKnow-sys/decima-loc)
- Rust-based localization manager
- Supports HZD and DS (not DS:DC explicitly)
- Export/import: TXT, JSON, YAML formats
- CLI and library interfaces
- Batch export/import capability

---

## 6. Contributing & License

### License
- **GPL-3.0** (GNU General Public License v3.0)
- This means any derivative work must also be GPL-3.0
- Important: if we fork and modify, our changes must be open-sourced

### Contributing Status
- No CONTRIBUTING.md file found
- Project has 21 forks, 219 stars
- 1,117 commits total
- Single developer project (ShadelessFox only committer)
- Has a Discord server: discord.gg/Gt4gkMwadB
- Accepting donations via Ko-fi: ko-fi.com/shadelessfox
- Build requires: Java 24, Git, Maven wrapper included

### Project Activity
- Latest release: **v0.1.27** (Apr 26, 2025) - technical release
- Last commit: **Nov 23, 2025** (~4 months ago)
- DS2 support request closed immediately as "not planned"
- HFW support request still open (from Mar 2024, 2 years ago)
- **Project appears to be in maintenance mode, not actively adding new game support**

---

## 7. Key Findings & Implications for DS2 Translation Project

### Critical Blockers
1. **DS2 is explicitly "not planned"** in Decima Workshop
2. **No existing tools support DS2** anywhere on GitHub
3. **DS2's archive format is unknown** - must be reverse-engineered first
4. **RTTI types for DS2 are not extracted** - need to dump from executable
5. **Encryption keys (if applicable) are unknown** for DS2

### Positive Signs
1. DS:DC support exists and works well (closest relative to DS2)
2. The localization CLI is mature and battle-tested for DS/DS:DC
3. The codebase is well-structured with clear extension points (GameType enum, metadata files)
4. GPL-3.0 license allows forking and modification
5. Wiki documents all binary formats in detail (010 Editor templates)
6. CoreBinary format (RTTIBinaryVersion 2) may be reused in DS2

### Recommended Strategy
1. **Investigate DS2 game files empirically:**
   - Check if .bin archives use PackFile magic (`0x20304050`/`0x21304050`) or DSAR magic
   - Determine compression: Oodle or LZ4
   - Check encryption status
2. **Fork Decima Workshop** if PackFile format is compatible:
   - Add `DS2` to GameType enum
   - Extract RTTI types from DS2 executable
   - Generate `ds2_types.json.gz` and `ds2_paths.txt.gz`
   - Test with localization CLI
3. **Alternative: Build custom tooling** if format is incompatible:
   - Use Decima Workshop wiki as format reference
   - Implement minimal .core reader in C++ (for MakineAI-Launcher integration)
4. **Contact ShadelessFox via Discord** to understand why DS2 was rejected
   - May be temporary (waiting for PC version analysis)
   - May be engine version too different
   - Discord: discord.gg/Gt4gkMwadB

### Related External Resources
- **Decima Workshop Discord:** discord.gg/Gt4gkMwadB (likely has community knowledge)
- **decima-loc:** github.com/YouKnow-sys/decima-loc (Rust localization tool, DS/HZD)
- **Decima Explorer:** Another Decima tool (C#, older)
- **Wunkolo/DecimaTools:** C++ Decima tools (another implementation)

---

## Source URLs
- Main repo: https://github.com/ShadelessFox/decima
- DS2 Issue: https://github.com/ShadelessFox/decima/issues/88
- HFW Issue: https://github.com/ShadelessFox/decima/issues/67
- Wiki - Archives: https://github.com/ShadelessFox/decima/wiki/Archives
- Wiki - Corefiles: https://github.com/ShadelessFox/decima/wiki/Corefiles
- Wiki - CLI: https://github.com/ShadelessFox/decima/wiki/CLI
- Releases: https://github.com/ShadelessFox/decima/releases
- Forks: https://github.com/ShadelessFox/decima/network/members
