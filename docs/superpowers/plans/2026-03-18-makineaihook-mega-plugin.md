# MakineHook Mega Plugin — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Consolidate TextHook + Archive engine modules into a single community plugin called MakineHook — the ultimate game translation toolkit.

**Architecture:** Single plugin with two DLLs (plugin DLL loaded by Launcher + hook DLL injected into games). Plugin DLL exposes all functionality through C ABI exports: hooking, asset parsing, memory scanning. Archive code is adapted to remove `makine_common` dependency and use lightweight local types.

**Tech Stack:** C++23, CMake 3.25+, vcpkg (zlib, LZ4), nlohmann/json (header-only), Windows API

**Source Material:**
- `MakineAI-Plugin-TextHook` (existing repo, will be renamed)
- `Makine-Archive/engine/` (private archive, code ported via `gh api`)

---

## File Structure

```
MakineAI-Plugin-MakineHook/
├── manifest.json                          — Plugin metadata + settings
├── CMakeLists.txt                         — Top-level build (both DLLs)
├── vcpkg.json                             — Dependencies (zlib, lz4)
├── README.md                              — Documentation
├── makine-pack.py                         — Packaging tool (from template)
├── .gitignore
│
├── include/makine/
│   ├── plugin/
│   │   ├── plugin_api.h                   — Plugin SDK (from Launcher)
│   │   └── plugin_types.h                 — Error codes, structs
│   ├── types.hpp                          — Common types (GameEngine, StringEntry, Result<T>)
│   ├── error.hpp                          — Error/ErrorCode definitions
│   ├── logging.hpp                        — Lightweight logging macros
│   ├── metrics.hpp                        — No-op metrics stub
│   ├── asset_parser.hpp                   — IAssetFormatParser interface + AssetParser
│   ├── parsers_factory.hpp                — Factory functions
│   ├── memory_extractor.hpp               — MemoryExtractor class
│   └── handlers/
│       └── engine_handler.hpp             — IEngineHandler interface
│
├── src/                                   — Plugin DLL sources
│   ├── plugin.cpp                         — C ABI entry point (all exports)
│   ├── settings.h                         — Key-value persistence
│   │
│   ├── hooking/                           — [From TextHook]
│   │   ├── hook_manager.h
│   │   └── hook_manager.cpp
│   │
│   ├── parsers/                           — [From Archive]
│   │   ├── unity_bundle_parser.cpp
│   │   ├── unreal_pak_parser.cpp
│   │   ├── bethesda_ba2_parser.cpp
│   │   ├── gamemaker_data_parser.cpp
│   │   └── formats/                       — Binary format support
│   │       ├── unity_bundle.hpp
│   │       ├── unreal_pak.hpp
│   │       ├── bethesda_ba2.hpp
│   │       ├── gamemaker_data.hpp
│   │       ├── renpy_rpa.hpp
│   │       ├── rpa_archive.cpp
│   │       ├── pickle_reader.hpp
│   │       └── pickle_reader.cpp
│   │
│   └── memory/                            — [From Archive]
│       └── memory_extractor.cpp
│
├── hook/                                  — Hook DLL (injected into game)
│   ├── dllmain.cpp
│   ├── text_hooks.h
│   ├── text_hooks.cpp
│   ├── pipe_client.h
│   └── pipe_client.cpp
│
└── tests/
    ├── CMakeLists.txt
    ├── test_types.cpp
    ├── test_parsers.cpp
    └── test_memory.cpp
```

---

## Phase 1: Repo Setup and Rename

### Task 1.1: Rename GitHub repo + clone fresh

**Files:**
- GitHub: `MakineAI-Plugin-TextHook` -> `MakineAI-Plugin-MakineHook`

- [ ] **Step 1: Rename repo on GitHub**

```bash
gh repo rename MakineAI-Plugin-MakineHook --repo MakineCeviri/MakineAI-Plugin-TextHook --yes
```

- [ ] **Step 2: Clone fresh with new name**

```bash
cd /c/Workspace/Makine
rm -rf MakineAI-Plugin-TextHook
git clone https://github.com/MakineCeviri/MakineAI-Plugin-MakineHook.git
cd MakineAI-Plugin-MakineHook
```

- [ ] **Step 3: Create dev branch**

```bash
git checkout -b feat/mega-plugin
```

- [ ] **Step 4: Initial commit**

```bash
git commit --allow-empty -m "chore: begin mega-plugin restructuring"
```

---

### Task 1.2: Restructure directory layout

**Files:**
- Move: `src/hook_manager.h` -> `src/hooking/hook_manager.h`
- Move: `src/hook_manager.cpp` -> `src/hooking/hook_manager.cpp`

- [ ] **Step 1: Create new directories**

```bash
mkdir -p src/hooking src/parsers/formats src/memory tests
```

- [ ] **Step 2: Move hooking code**

```bash
git mv src/hook_manager.h src/hooking/hook_manager.h
git mv src/hook_manager.cpp src/hooking/hook_manager.cpp
```

- [ ] **Step 3: Update include paths in plugin.cpp**

Change `#include "hook_manager.h"` to `#include "hooking/hook_manager.h"`

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "refactor: restructure into hooking/ subdirectory"
```

---

## Phase 2: Foundation Types (from Archive)

### Task 2.1: Create common types

These types are used by all archive modules. Extracted from `makine_common` into self-contained headers.

**Files:**
- Create: `include/makine/error.hpp`
- Create: `include/makine/types.hpp`
- Create: `include/makine/logging.hpp`
- Create: `include/makine/metrics.hpp`

- [ ] **Step 1: Create error.hpp** — ErrorCode enum + Result<T> alias using std::expected
- [ ] **Step 2: Create types.hpp** — GameEngine enum, TranslationEntry, StringList, fs alias
- [ ] **Step 3: Create logging.hpp** — Lightweight fprintf macros replacing spdlog (MAKINE_LOG_DEBUG/INFO/WARN/ERROR)
- [ ] **Step 4: Create metrics.hpp** — No-op Metrics stub (timer/increment do nothing)
- [ ] **Step 5: Commit**

```bash
git add include/makine/{error,types,logging,metrics}.hpp
git commit -m "feat: add foundation types (error, types, logging, metrics stubs)"
```

---

## Phase 3: Asset Parsers (from Archive)

### Task 3.1: Port IAssetFormatParser interface

**Files:**
- Create: `include/makine/asset_parser.hpp` (from `Makine-Archive/engine/include/makine/asset_parser.hpp`)
- Create: `include/makine/parsers_factory.hpp` (from archive)

- [ ] **Step 1: Fetch asset_parser.hpp from archive via `gh api`**
- [ ] **Step 2: Fetch parsers_factory.hpp from archive**
- [ ] **Step 3: Adapt includes to use local types.hpp/error.hpp**
- [ ] **Step 4: Commit**

```bash
git add include/makine/asset_parser.hpp include/makine/parsers_factory.hpp
git commit -m "feat: add asset parser interface and factory"
```

---

### Task 3.2: Port format headers

Binary struct definitions for each engine's file format.

**Files:**
- Create: `src/parsers/formats/unity_bundle.hpp`
- Create: `src/parsers/formats/unreal_pak.hpp`
- Create: `src/parsers/formats/bethesda_ba2.hpp`
- Create: `src/parsers/formats/gamemaker_data.hpp`
- Create: `src/parsers/formats/renpy_rpa.hpp`
- Create: `src/parsers/formats/pickle_reader.hpp`

- [ ] **Step 1: Fetch format headers from archive `engine/src/formats/`**
- [ ] **Step 2: Adapt include paths to local types.hpp**
- [ ] **Step 3: Commit**

```bash
git add src/parsers/formats/
git commit -m "feat: add binary format headers (Unity, Unreal, Bethesda, GameMaker, RenPy)"
```

---

### Task 3.3: Port Unity AssetBundle parser

**Files:**
- Create: `src/parsers/unity_bundle_parser.cpp` (from `Makine-Archive/engine/src/asset_parser/unity_bundle_parser.cpp`)

- [ ] **Step 1: Fetch from archive**
- [ ] **Step 2: Adapt logging (spdlog fmt -> fprintf), remove metrics calls**
- [ ] **Step 3: Verify no makine_common references remain**
- [ ] **Step 4: Commit**

```bash
git add src/parsers/unity_bundle_parser.cpp
git commit -m "feat(parsers): port Unity AssetBundle parser from archive"
```

---

### Task 3.4: Port Unreal PAK parser

**Files:**
- Create: `src/parsers/unreal_pak_parser.cpp` (from archive)

Full .locres read+write support. UE4/UE5 PAK v8-v11.

- [ ] **Step 1: Fetch and adapt**
- [ ] **Step 2: Adapt spdlog format strings**
- [ ] **Step 3: Commit**

```bash
git add src/parsers/unreal_pak_parser.cpp
git commit -m "feat(parsers): port Unreal PAK/locres parser from archive"
```

---

### Task 3.5: Port Bethesda BA2 parser

**Files:**
- Create: `src/parsers/bethesda_ba2_parser.cpp` (from archive)

Supports .ba2 + .strings/.dlstrings/.ilstrings (read AND write). Uses zlib + LZ4.

- [ ] **Step 1: Fetch and adapt**
- [ ] **Step 2: Commit**

```bash
git add src/parsers/bethesda_ba2_parser.cpp
git commit -m "feat(parsers): port Bethesda BA2/strings parser from archive"
```

---

### Task 3.6: Port GameMaker data.win parser

**Files:**
- Create: `src/parsers/gamemaker_data_parser.cpp` (from archive)

IFF/FORM chunk parser (STRG + LANG). No external deps.

- [ ] **Step 1: Fetch and adapt**
- [ ] **Step 2: Commit**

```bash
git add src/parsers/gamemaker_data_parser.cpp
git commit -m "feat(parsers): port GameMaker data.win parser from archive"
```

---

### Task 3.7: Port RPA archive + format reader

**Files:**
- Create: `src/parsers/formats/rpa_archive.cpp` (from archive)
- Create: `src/parsers/formats/pickle_reader.cpp` (from archive)

RPA needs zlib. Reader is standalone. Used for Ren'Py game support.

> **Security note:** The reader only decodes Ren'Py archive indexes from trusted game files, not arbitrary untrusted data.

- [ ] **Step 1: Fetch both from archive**
- [ ] **Step 2: Adapt includes**
- [ ] **Step 3: Commit**

```bash
git add src/parsers/formats/rpa_archive.cpp src/parsers/formats/pickle_reader.cpp
git commit -m "feat(formats): port RPA archive reader from archive"
```

---

## Phase 4: Memory Extractor (from Archive)

### Task 4.1: Port MemoryExtractor

**Files:**
- Create: `include/makine/memory_extractor.hpp` (from archive)
- Create: `src/memory/memory_extractor.cpp`

- [ ] **Step 1: Fetch header from archive**
- [ ] **Step 2: Check for .cpp source, fetch if exists**
- [ ] **Step 3: Adapt imports (replace makine_common with local types)**
- [ ] **Step 4: Verify Turkish fingerprint tables are preserved (characters used for scanning)**
- [ ] **Step 5: Commit**

```bash
git add include/makine/memory_extractor.hpp src/memory/memory_extractor.cpp
git commit -m "feat(memory): port MemoryExtractor with Turkish fingerprinting"
```

---

## Phase 5: Engine Handler Interface (from Archive)

### Task 5.1: Port IEngineHandler

**Files:**
- Create: `include/makine/handlers/engine_handler.hpp` (from archive)

- [ ] **Step 1: Fetch from archive (already a stub interface)**
- [ ] **Step 2: Adapt to use local types.hpp**
- [ ] **Step 3: Commit**

```bash
git add include/makine/handlers/engine_handler.hpp
git commit -m "feat: port IEngineHandler interface from archive"
```

---

## Phase 6: C ABI Integration

### Task 6.1: Expand plugin.cpp with new exports

**Files:**
- Modify: `src/plugin.cpp`

- [ ] **Step 1: Add parser exports** — `makine_detect_engine`, `makine_parse_assets`, `makine_get_string_count`, `makine_get_string_at`
- [ ] **Step 2: Add memory scanner exports** — `makine_scan_memory`, `makine_get_scanned_text`
- [ ] **Step 3: Update `makine_get_info()` with new id/name/version**
- [ ] **Step 4: Commit**

```bash
git add src/plugin.cpp
git commit -m "feat: expand C ABI with parser + memory scanner exports"
```

---

### Task 6.2: Update manifest.json

**Files:**
- Modify: `manifest.json`

- [ ] **Step 1: Update id, name, version, capabilities, features, settings**

New id: `com.makineceviri.makinehook`
New capabilities: `["process", "hook", "parser", "memory"]`
Add `memoryScanning` toggle to settings.

- [ ] **Step 2: Commit**

```bash
git add manifest.json
git commit -m "feat: update manifest for MakineHook identity"
```

---

## Phase 7: Build System

### Task 7.1: CMakeLists.txt overhaul

**Files:**
- Modify: `CMakeLists.txt`
- Create: `vcpkg.json`
- Create: `tests/CMakeLists.txt`

- [ ] **Step 1: Create vcpkg.json** with zlib, lz4, nlohmann-json
- [ ] **Step 2: Rewrite CMakeLists.txt** — two DLL targets, all source files, vcpkg deps
- [ ] **Step 3: Create tests/CMakeLists.txt** — basic test structure
- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt vcpkg.json tests/CMakeLists.txt
git commit -m "build: CMake overhaul with vcpkg deps for mega plugin"
```

---

## Phase 8: README + Documentation

### Task 8.1: Comprehensive README

**Files:**
- Modify: `README.md`

Sections: About, Features, Architecture diagram, Supported engines, Build, C ABI reference, Settings, Roadmap, Contributing, License.

- [ ] **Step 1: Write full README**
- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: comprehensive README for MakineHook"
```

---

## Phase 9: Package and Release

### Task 9.1: First release

- [ ] **Step 1: Copy makine-pack.py from template**
- [ ] **Step 2: Build release**
- [ ] **Step 3: Package as .makine**
- [ ] **Step 4: Create GitHub release v0.2.0**
- [ ] **Step 5: Add makine-plugin topic**
- [ ] **Step 6: Merge feat/mega-plugin into main, push**

---

## Dependency Map

```
makinehook.dll
  +-- zlib          (BA2 decompression, RPA decompression)
  +-- LZ4           (Unity AssetBundle decompression)
  +-- nlohmann/json (JSON output for C ABI results)
  +-- psapi         (Process enumeration, Windows)
  +-- kernel32      (CreateRemoteThread, VirtualAllocEx, Named Pipes)

makine-hook.dll
  +-- gdi32         (GDI function hooking targets)
  +-- user32        (DrawText function hooking targets)
```

No curl, no spdlog, no simdjson — kept lightweight for plugin distribution.

## Modules Sourced from Archive

| Module | Archive Path | Adaptations |
|--------|-------------|-------------|
| Unity parser | `engine/src/asset_parser/unity_bundle_parser.cpp` | Replace logging, remove metrics |
| Unreal parser | `engine/src/asset_parser/unreal_pak_parser.cpp` | Replace logging, remove metrics |
| Bethesda parser | `engine/src/asset_parser/bethesda_ba2_parser.cpp` | Replace logging, remove metrics |
| GameMaker parser | `engine/src/asset_parser/gamemaker_data_parser.cpp` | Replace logging, remove metrics |
| RPA archive | `engine/src/formats/rpa_archive.cpp` | Adapt includes |
| Format reader | `engine/src/formats/pickle_reader.cpp` | Standalone, minimal |
| IAssetFormatParser | `engine/include/makine/asset_parser.hpp` | Remove makine_common dep |
| ParsersFactory | `engine/include/makine/parsers_factory.hpp` | Minimal changes |
| MemoryExtractor | `engine/include/makine/memory_extractor.hpp` | Remove makine_common dep |
| IEngineHandler | `engine/include/makine/handlers/engine_handler.hpp` | Already a stub |

## Risk Notes

- **Trampoline safety:** 14-byte inline hook assumes safe instruction boundary. Works for GDI but not guaranteed for custom hooks. Future: add LDE.
- **Archive logging:** spdlog format syntax needs conversion to fprintf per file.
- **Optional deps skipped:** simdjson, simdutf, Taskflow, concurrentqueue are ifdef'd in archive. We skip them entirely.
- **Translation API:** NOT included (Launcher handles translation). Can be added later.
