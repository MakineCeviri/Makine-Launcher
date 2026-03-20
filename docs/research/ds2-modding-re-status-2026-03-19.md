# Death Stranding 2: On The Beach — Modding & Reverse Engineering Status Report

**Date:** 2026-03-19 (PC launch day)
**Author:** Research agent
**Purpose:** Assess feasibility of DS2 Turkish localization mod via file-level text replacement

---

## 1. FearLess Cheat Engine Forum

### Active Threads
- **SunBeam's table** (FRF admin, rep 4936): [viewtopic.php?f=4&t=38616](https://fearlessrevolution.com/viewtopic.php?f=4&t=38616)
  - Posted: Wed Mar 18, 2026 2:13 PM — game version `1.0.45.0`, process `ds.exe`
  - Currently a placeholder — will restore features from his DS:DC table over time
  - Engine tagged as `[Engine:Decima]`
  - SunBeam has deep Decima memory RE experience from DS1/DS:DC tables

- **NoByte's table**: [viewtopic.php?f=4&t=38611](https://fearlessrevolution.com/viewtopic.php?f=4&t=38611)
  - Posted: Wed Mar 18, 2026 4:42 AM
  - Process: `DEATH STRANDING 2 ON THE BEACH.exe`
  - Features: Infinite Health, Infinite Stamina, Game Speed, Speed (+5)
  - Works on Steam + Epic + Xbox GamePass
  - 98 downloads already

### Key Observations
- Neither table-maker has published file format research. Both work via **dynamic memory injection** (runtime pointers, AOB scanning) — not file-level reverse engineering.
- SunBeam is the most likely person to eventually publish Decima binary format findings for DS2, given his history with DS:DC.
- No one on FRF is working on text/localization modifications.

---

## 2. Reddit

- **r/DeathStranding** and **r/DeathStranding2**: No specific modding/RE threads found in search results. Discussion is gameplay-focused on launch day.
- A Steam Community discussion exists requesting Turkish localization: [DEATH STRANDING 2: ON THE BEACH TURKISH LOCALIZATION](https://steamcommunity.com/app/1850570/discussions/0/597403312579449785/)
- The Steam DS:DC forum has a thread asking "Is there a reason why there is not more mods for this game?" — community consensus is that Decima is extremely locked down.

---

## 3. NexusMods

**Page:** [nexusmods.com/deathstranding2onthebeach](https://www.nexusmods.com/games/deathstranding2onthebeach)

### Current Mods (as of launch day)
| # | Mod | Type | Notes |
|---|-----|------|-------|
| 2 | (Removed by staff) | Unknown | Taken down |
| 3 | DS2 Intro Skip | Utility | External EXE, uses pixel detection to skip logos. NOT a file mod — closes itself after main menu. Uploaded Mar 18 by gattucio4509. Flagged "some suspicious files". |

**Total mods: 2** (1 removed). Zero localization/translation mods. Zero file-replacement mods. The modding scene is essentially non-existent on NexusMods at launch.

---

## 4. XMODhub

**Page:** [xmodhub.com/en/xmod-cheats/death-stranding-2-on-the-beach-trainers](https://www.xmodhub.com/en/xmod-cheats/death-stranding-2-on-the-beach-trainers)

### Real-Time Auto-Translation Feature
XMODhub has a **built-in Real-Time Auto-Translation** overlay:
- Uses a lightweight overlay to **detect and translate cutscene subtitles** into any native language
- Runs independently from the game process — no file modification, no frame drops
- Select feature from in-game overlay → choose language → done
- **This is NOT file-level translation** — it is an OCR/screen-capture based overlay
- Targets players whose languages are not officially supported (Turkish, Arabic, Thai mentioned specifically)

### Assessment
- XMODhub's approach is a runtime overlay, not a real localization mod
- It cannot translate UI elements, menus, item names, or in-game text that isn't displayed as subtitles
- Quality depends on OCR accuracy and translation API — likely machine translation
- **Not a competitor to a proper file-level Turkish localization patch**

---

## 5. Wunkolo/DecimaTools (GitHub)

**Repo:** [github.com/Wunkolo/DecimaTools](https://github.com/Wunkolo/DecimaTools)

- Focus: Death Stranding PC (DS1) iteration of Decima only
- 26 commits total, last major activity unknown (no recent releases)
- Uses Murmur3 hashing and MD5 for archive encryption
- **No DS2 support. No HFW support. No DirectStorage support.**
- References XenTax forum threads for original RE work (Ekey + Jayveer)

---

## 6. ShadelessFox/Decima Workshop (GitHub)

**Repo:** [github.com/ShadelessFox/decima](https://github.com/ShadelessFox/decima) (219 stars, 1117 commits)

### Supported Games (as of v0.1.27, Apr 26 2025)
| Game | Platform | Status |
|------|----------|--------|
| Death Stranding | PC | Supported |
| Death Stranding: Director's Cut | PC | Supported |
| Horizon Zero Dawn | PC | Supported |
| Horizon Forbidden West | PC | **Open issue #67** (feature, high priority) — NOT yet supported |
| Death Stranding 2 | PC | **Issue #88 — Closed as "Not planned (skipped)"** on Mar 18, 2026 |

### Critical Finding: DS2 Support Rejected
- Issue [#88](https://github.com/ShadelessFox/decima/issues/88) "DEATH STRANDING 2 Support Request" opened by user `sonying` on Mar 18, 2026
- **Closed same day as "Not planned (skipped)"** by ShadelessFox
- No explanation given in the visible issue body
- This means **the primary Decima modding tool will NOT support DS2** in the near term
- HFW support (#67) is still open but has been pending since March 2024 (2 years!)

### Implication
Decima Workshop cannot open, read, or edit DS2 `.core` files. Since HFW (which also uses DirectStorage archives) is still unsupported after 2 years, DS2 support is unlikely to come from this tool.

---

## 7. Other Tools

### Jayveer/Decima-Explorer
- [github.com/Jayveer/Decima-Explorer](https://github.com/Jayveer/Decima-Explorer) (207 stars)
- Can unpack/pack DS1 and HZD `.bin` archives
- Uses Oodle DLL for decompression
- **No DS2 support** — only handles PackFile archives, not DirectStorage (DSAR)

### YouKnow-sys/decima-loc
- [github.com/YouKnow-sys/decima-loc](https://github.com/YouKnow-sys/decima-loc) (9 stars)
- Rust tool for managing Decima engine localization
- Export/import text as JSON, YAML, TXT
- **Supported: HZD (PC), DS1 (PC + PS4) only**
- **No DS2, no DS:DC, no HFW support**

### Bearborg/pydecima
- [github.com/Bearborg/pydecima](https://github.com/Bearborg/pydecima)
- Python `.core` file reader
- Supports "HZDPC", "HZDPS4", "DSPC" — **"very little support for Death Stranding resource formats"**
- **No DS2 support**

### Stealch/DSDCModInstaller
- One-click archive repacking tool for Decima Workshop
- Only works with DS:DC

---

## 8. Chinese/Japanese Modding Communities

- **Bilibili**: Only crack/piracy-related content found, no technical modding discussions
- **No evidence** of Chinese or Japanese communities cracking DS2's file format
- DS2 PC was leaked 2 days before release (no Denuvo DRM), but this only resulted in piracy, not RE work
- The game uses Steam DRM only, which was trivially bypassed

---

## 9. DS2 Archive Format: DirectStorage (DSAR)

### Key Technical Finding
Based on the Decima Wiki (ShadelessFox) and previous research findings:

**DS2 uses DirectStorage archives (DSAR), NOT the classic PackFile format.**

| Game | Archive Format |
|------|---------------|
| Horizon Zero Dawn | PackFile Archive (magic `0x20304050`) |
| Death Stranding (DS1) | PackFile Archive **Encrypted** (magic `0x21304050`) |
| Horizon Forbidden West (PC) | **DirectStorage Archive** (magic `DSAR`) |
| HZD Remastered (PC) | **DirectStorage Archive** (magic `DSAR`) |
| **Death Stranding 2 (PC)** | **DirectStorage Archive** (magic `DSAR`, version `0x00010003`) |

### DSAR Format Structure (from Decima Wiki + previous research)
```c
typedef struct {
    char magic[4];       // "DSAR"
    uint16 version_major; // 3
    uint16 version_minor; // 1
    uint32 chunk_count;
    uint32 first_chunk_offset;
    uint64 total_size;
    char   padding[8];
} Header;  // 32 bytes

typedef struct {
    uint64 offset;
    uint64 compressed_offset;
    uint32 size;
    uint32 compressed_size;
    ubyte  type;         // 3 = LZ4
    ubyte  padding[7];
} Chunk;   // 32 bytes per entry
```

### Critical Difference from PackFile
> "Unlike PackFiles, DirectStorage archives contain **raw data split into compressed chunks**. The contents **can't be extracted without external description or metadata**."

- In HFW: contents are described by `LocalCacheWinGame\package\streaming_graph.core` (a regular RTTIBinaryVersion 2 corefile)
- In HZD:R: contents are described by `LocalCacheDX12\package\PackFileLocators.bin`
- **In DS2**: The equivalent metadata file location is **unknown** — this is one of the key unknowns to solve

### Implications for Text Modding
1. You cannot simply unpack a DSAR archive like a PackFile — you need the resource catalog/index file
2. The resource catalog tells you where each `.core` file's data lives within the compressed chunks
3. Without this catalog, you're blindly searching compressed LZ4 chunks
4. **No existing tool can open DS2's DSAR archives**

---

## 10. Corefile Format (RTTIBinaryVersion 2)

DS1 and HZD use RTTIBinaryVersion 2 for `.core` files. DS2 **likely** uses the same or an evolved version.

### Classic Format (HZD/DS1)
Each `.core` file is a sequence of RTTI objects:
```
[type_hash: uint64] [size: uint32] [object_data: byte[size]]
[type_hash: uint64] [size: uint32] [object_data: byte[size]]
...
```

For localization resources, the type hash for `LocalizedTextResource` is `457431352450be31`.

### DS2 String Format (from previous research session)
- **Same as DS1**: 2-byte little-endian length prefix + UTF-8 string + 3-null separator
- Container format changed (DSAR vs PackFile), but the string encoding within `.core` files appears identical
- Language index for Turkish: slot 18 (out of 25 languages)

---

## 11. Answers to Key Questions

### Has ANYONE successfully modified DS2 text with variable-length strings?
**NO.** As of launch day (March 19, 2026), nobody has demonstrated file-level text modification in DS2. The community is doing runtime memory injection only. No tool can currently extract or repack DS2 `.core` files.

### Is there a resource catalog/index file that tracks resource sizes?
**YES, but its location in DS2 is unknown.** In HFW it's `streaming_graph.core`, in HZD:R it's `PackFileLocators.bin`. DS2 almost certainly has an equivalent file — finding it is the first step to extracting archives.

### What is DS2's actual binary serialization format?
**Almost certainly RTTIBinaryVersion 2** (same as DS1/HZD) for `.core` file internals. The archive container changed from encrypted PackFile to DSAR (DirectStorage Archive) with LZ4 compression. The string format within localization cores is confirmed identical to DS1.

### Are there any tools that can properly open/edit DS2 .core files?
**NO.** Zero tools support DS2:
- Decima Workshop: DS2 support request closed as "not planned"
- Decima Explorer: PackFile only, no DSAR
- decima-loc: DS1 and HZD only
- pydecima: DS1 only (limited)
- DecimaTools: DS1 only

---

## 12. Strategic Assessment for MakineAI Turkish Localization

### Path Forward
1. **Find the streaming graph / resource catalog** — Scan DS2's game directory for a `streaming_graph.core` or equivalent metadata file. This is the Rosetta Stone.
2. **DSAR extraction** — Implement LZ4 decompression of DSAR chunks, then use the catalog to reconstruct `.core` files
3. **Validate corefile format** — Confirm that extracted `.core` files match RTTIBinaryVersion 2 and that localization strings use the known `457431352450be31` type hash with 2-byte prefix + UTF-8 encoding
4. **Build custom tooling** — No existing tool will work. Custom extraction/repacking is required.
5. **Monitor SunBeam (FRF)** — He is the most likely community member to publish deep Decima RE findings for DS2
6. **Monitor ShadelessFox** — If HFW support eventually lands in Decima Workshop, the DSAR handling code could be adapted for DS2

### Risk: Variable-Length String Replacement
In PackFile archives (DS1), replacing a string with a longer one shifts all subsequent offsets. The archive header tracks file sizes and chunk offsets. In DSAR, the chunk structure has explicit `size` and `compressed_size` fields, so recompression after modification is mandatory. The streaming graph maps resources to chunks, so any size change requires updating the graph.

This is **solvable** but requires complete understanding of the DSAR repacking process.

---

## Sources

### FearLess Cheat Engine
- [SunBeam DS2 Table Thread](https://fearlessrevolution.com/viewtopic.php?f=4&t=38616)
- [NoByte DS2 Table Thread](https://fearlessrevolution.com/viewtopic.php?f=4&t=38611)
- [DS2 Table Request Thread](https://fearlessrevolution.com/viewtopic.php?t=38605)

### GitHub Repositories
- [ShadelessFox/decima (Decima Workshop)](https://github.com/ShadelessFox/decima)
- [ShadelessFox/decima Issue #88 — DS2 Rejected](https://github.com/ShadelessFox/decima/issues/88)
- [ShadelessFox/decima Issue #67 — HFW Pending](https://github.com/ShadelessFox/decima/issues/67)
- [ShadelessFox/decima Wiki — Archives](https://github.com/ShadelessFox/decima/wiki/Archives)
- [ShadelessFox/decima Wiki — Corefiles](https://github.com/ShadelessFox/decima/wiki/Corefiles)
- [Wunkolo/DecimaTools](https://github.com/Wunkolo/DecimaTools)
- [Jayveer/Decima-Explorer](https://github.com/Jayveer/Decima-Explorer)
- [YouKnow-sys/decima-loc](https://github.com/YouKnow-sys/decima-loc)
- [Bearborg/pydecima](https://github.com/Bearborg/pydecima)
- [Stealch/DSDCModInstaller](https://github.com/Stealch/DSDCModInstaller)

### NexusMods
- [DS2 NexusMods Page](https://www.nexusmods.com/games/deathstranding2onthebeach)
- [DS2 Intro Skip Mod](https://www.nexusmods.com/deathstranding2onthebeach/mods/3)

### XMODhub
- [DS2 Language & Translation Guide](https://www.xmodhub.com/info/xmod-blog/how-to-change-language-subtitles-death-stranding-2-pc/)
- [DS2 Cheats & Trainer Guide](https://www.xmodhub.com/info/xmod-blog/death-stranding-2-cheats-trainer-console-commands/)
- [Top 10 DS2 Mods](https://www.xmodhub.com/info/xmod-blog/best-death-stranding-2-mods/)

### Other
- [DS2 PCGamingWiki](https://www.pcgamingwiki.com/wiki/Death_Stranding)
- [Decima Engine Wikipedia](https://en.wikipedia.org/wiki/Decima_(game_engine))
- [DS2 PC System Requirements](https://www.pcgamer.com/games/action/death-stranding-2-pc-system-requirements/)
- [KojiPro DS2 PC Announcement](https://www.kojimaproductions.jp/en/deathstranding2_feb_announce)
- [DS2 Leak Coverage](https://www.dsogaming.com/news/death-stranding-2-pc-has-been-leaked-two-days-before-release/)
