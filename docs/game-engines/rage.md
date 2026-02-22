# RAGE Engine — Translation Extraction Reference

## Overview

RAGE (Rockstar Advanced Game Engine) powers GTA IV, GTA V, RDR2, and GTA VI.
Text localization uses GXT/GXT2 format with Jenkins one-at-a-time hash lookups.

## Text System Architecture

```
RPF Archive → GXT2 File → Hash Table → String Data
                              │
                    rage::fwTextStore
                              │
                    FindTextAndSlot()
                              │
                    Returns: char* (UTF-8)
```

### GXT2 File Format

- **Magic**: `GXT2` (big-endian) or `2TXG` (little-endian)
- **Structure**: Header + array of (hash, offset) pairs + string data block
- **Hash algorithm**: Jenkins one-at-a-time (32-bit)
- **String encoding**: UTF-8 with RAGE formatting tags
- **File location**: Inside RPF archives under `x64/data/lang/`

### RAGE Text Formatting Tags

| Tag | Meaning |
|-----|---------|
| `~z~` | Dialogue text marker (most common prefix) |
| `~s~` | System/default style reset |
| `~n~` | Newline |
| `~o~` | Objective/highlight color |
| `~f~` | NPC name/reference |
| `~sl:X.X:Y.Y~` | Subtitle timing (start:duration in seconds) |
| `~COLOR_RED~` | Color tag |
| `~BLIP_*~` | Map blip reference |
| `~fo\|$util~` | Font override |

### Memory Layout (observed in RDR2)

Translation data in memory follows this pattern:

```
[null terminator of previous string]
[8 bytes: hash(4) + meta(4)]
[null-terminated UTF-8 string starting with ~z~]
```

- **Hash**: 4-byte little-endian Jenkins hash
- **Meta**: 4-byte value, dominant pattern `0x90XXXXXX` (166K of 191K entries)
- **String**: UTF-8, null-terminated, starts with `~z~` for dialogue

### Hash Distribution

Hash values are distributed across `0x00-0xFF` high bytes, with concentration in `0xE0-0xEF` range:

```
0xeaXXXXXX: 12,744 hashes (most common)
0xebXXXXXX: 11,131
0xe8XXXXXX: 10,134
0xe9XXXXXX:  9,848
0xedXXXXXX:  8,277
```

## RDR2-Specific Notes

### Translation ASI Mechanism

The CriminaL/Deftones Turkish translation works via:

1. `dinput8.dll` (ASI Loader) → loads `RDR2-TR-CriminaL-Deftones.asi`
2. ASI hooks `rage::fwTextStore::FindTextAndSlot`
3. Intercepts text lookup by hash, returns Turkish text instead of English
4. Translation data stored in encrypted cache file (AES-256, 3.1 MB)
5. Font replacement via `.font` file for Turkish character support

### Encoding Obfuscation

The ASI uses a custom encoding for anti-piracy/obfuscation:

| Original | Obfuscated | Unicode |
|----------|-----------|---------|
| `i` (dotted lowercase) | `ǔ` | U+01D4 → U+0069 |
| `İ` (dotted uppercase) | `Ǔ` | U+01D3 → U+0130 |
| `ı` (dotless lowercase) | `ı` | unchanged |
| `I` (dotless uppercase) | `I` | unchanged |

Detection method: Statistical analysis — if a non-Turkish character (ǔ) systematically
appears in positions where Turkish grammar requires a specific character (i), it's obfuscation.

### Anti-Tampering

- Full binary integrity verification (hash of entire ASI file)
- Any single byte modification causes the ASI to silently fail
- Split auth tokens: `Deft` + `ones` + `Crimi` + `naL` + `277` (concatenated at runtime)
- Auto-update mechanism phones home to GitHub for version checks

### File Structure

```
Türkçe Yama/
├── RDR2-TR-CriminaL-Deftones.asi    # Main translation hook (6.1 MB)
├── RDR2-TR-CriminaL-Defto.font      # Turkish font file (33 KB)
├── ScriptHookRDR2.dll                # ScriptHook framework
├── dinput8.dll                       # ASI Loader
├── lml/                              # Lenny's Mod Loader
│   ├── TRTextures/                   # Turkish texture replacements
│   │   ├── install.xml               # LML installation manifest
│   │   └── replace/update_1/...      # RPF file replacements (YTD textures)
│   └── mods.xml                      # Mod registry
└── version.dll                       # LML core
```

### LML File Replacement

LML replaces files inside RPF archives without modifying the archives:

```xml
<!-- install.xml -->
<ModInstall>
  <File>
    <Archive>update\update.rpf</Archive>
    <Source>replace\update_1\...</Source>
  </File>
</ModInstall>
```

Replaced files: YTD texture dictionaries containing region-specific Turkish text textures.

## Extraction Results Summary

| Data Type | Count | Description |
|-----------|-------|-------------|
| Dialogue hashes | 140,637 | `~z~` prefixed translation strings |
| General hashes | 51,316 | UI, items, descriptions |
| Total mappings | 191,953 | Complete hash→text database |
| Database size | 47.4 MB | JSON format |
| Scan time | ~29 min | Full 3.8 GB memory scan |

## References

- ScriptHook RDR2: [alexanderblade.com](http://www.dev-c.com/rdr2/scripthookrdr2/)
- LML: Lenny's Mod Loader for RAGE engine
- GXT2 format: Community-documented from GTA V modding
- Jenkins hash: Bob Jenkins' one-at-a-time hash algorithm
