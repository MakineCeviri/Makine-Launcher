# Makine-Launcher - Claude Code Context

## Project Overview

Turkish game translation launcher + adaptation engine.

- **Makine-Launcher** (this repo) — Launcher: game detection, package install/remove, patching, catalog UI

## Build Commands

```bash
# Quick dev build (MinGW, Core+UI, vcpkg required)
just dev        # or: cmake --preset dev && cmake --build --preset dev

# UI-only build (no vcpkg needed)
just dev-ui     # or: cmake --preset dev-ui && cmake --build --preset dev-ui

# Run after build
just run        # or: ./build/dev/Makine-Launcher.exe

# Core library only (MSVC)
just core

# Tests
just test

# Release (MSVC + vcpkg)
just release
```

## PATH Setup (bash)

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Program Files/Git/usr/bin:$PATH"
# For running (Qt DLLs):
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"
```

## Project Structure

```
qml/src/services/     — C++ backend services (GameService, CoreBridge, etc.)
qml/qml/              — QML UI files (Main.qml, screens/, components/, dialogs/)
qml/qml/controllers/  — QML logic controllers (InstallFlowController)
core/src/              — C++ core library (asset_parser, game_detector, security, etc.)
core/include/makine/  — Public headers (.hpp, snake_case)
```

### Package Catalog Architecture

Hybrid index + on-demand detail system:
- **Startup:** `index.json` (93 KB) → lightweight catalog metadata
- **On-demand:** `packages/{appId}.json` (~700 B) → install steps, contributors, variants
- **Core:** `PackageCatalog::loadFromIndex()` + `enrichPackage()`
- **Assets CDN:** `cdn.makineceviri.org` (Cloudflare R2 custom domain)
  - Config: `qml/src/services/cdnconfig.h` — all CDN URLs centralized here
  - Assets: `assets/` prefix (index.json + packages/ + images/ + banners/)
  - Data: `data/` prefix (.mkpkg encrypted packages)

## Coding Conventions

- **C++23** standard, namespace `makine`
- **QML/UI C++**: camelCase filenames, `.h`/`.cpp` — e.g. `gameservice.h`
- **Core C++**: snake_case filenames, `.hpp`/`.cpp` — e.g. `game_detector.hpp`
- **QML**: PascalCase filenames — e.g. `GameDetailScreen.qml`
- Code comments in English
- Prefer native C++ over Qt for business logic
- Use `#pragma once` in headers
- Classes: `PascalCase`, functions/variables: `camelCase`, constants: `UPPER_SNAKE_CASE`

## Build Presets

| Preset | Compiler | Description |
|--------|----------|-------------|
| `dev` | MinGW+vcpkg | Core+UI, daily development |
| `dev-ui` | MinGW | UI-only (`MAKINE_UI_ONLY=ON`), no vcpkg |
| `debug` | MinGW+vcpkg | Core+UI with debug symbols |
| `release` | MSVC+vcpkg | Full release |
| `release-static` | MinGW (static Qt) | Single EXE, UI-only |
| `core` | MSVC+vcpkg | Core library only |

## Known Gotchas

Full list with examples: `~/.claude/rules/compatibility-rules.md`

- **MinGW GCC 13.1 `<regex>`**: Broken — files using it are excluded from build
- **spdlog ADL**: `spdlog::info` may resolve to `makine::info` — use fully qualified calls
- **`#include <set>`**: Must be explicit (implicit on MSVC, not on MinGW)
- **vcpkg classic mode**: Use `--classic` flag and `-DVCPKG_MANIFEST_MODE=OFF`
- **QML Theme**: Use `Theme.bgPrimary` (not `Theme.background` — doesn't exist)
- **Forward declarations**: At file top level, not inside `#ifdef` blocks (AUTOMOC issues)
- **QML `component X:`**: Local component definitions shadow shared components — avoid naming collisions
- **QML `Behavior on readonly`**: Crashes at runtime — use non-readonly property
- **QML `ApplicationWindow.visible`**: Defaults to `false` — do NOT remove `visible: true` from Main.qml
- **QML `clip: true`**: Required in scroll containers (Flickable, ListView, ScrollView)

## Important Rules

- Do NOT touch UI animations, MultiEffect, or gradient designs — user explicitly preserves original look
- Do NOT output build artifacts to Desktop
- Commit convention: Conventional Commits (`feat(scope): message`)
- Scopes: `core`, `ui`, `build`, `ci`, `docs`
- Hookify rules active at `.claude/hookify.*.local.md` — block known anti-patterns, Desktop output, CEDRA/ writes
- Hooks use dynamic git root (`git rev-parse --show-toplevel`) — worktree-compatible
- Defense layers: hookify (PreToolUse blocks) → post-edit hook (PostToolUse warns) → pre-commit (commit blocks) → pre-push (push blocks)

## Deferred Features

These modules are intentionally deferred — stub headers removed:
- Translation Memory, Glossary Service, QA Service, Translation Pipeline
- Engine Handlers (only interface `IEngineHandler` in `engine_handler.hpp`)
- BepInEx/XUnity runtime system (fully removed — RuntimeManager is a stub)
- Integration tests disabled until handlers are implemented

## Logging

- **Core (C++)**: `MAKINE_LOG_*` macros via spdlog (see `core/include/makine/logging.hpp`)
- **UI (Qt)**: `qCDebug(lcXxx)` / `qCWarning(lcXxx)` — categorized logging with `QLoggingCategory`
  - Categories: `makine.app`, `makine.game`, `makine.bridge`, `makine.package`, `makine.download`, `makine.batch`, `makine.backup`, `makine.process`, `makine.integrity`, `makine.manifest`, `makine.journal`, `makine.steam`, `makine.update`, `makine.updater`, `makine.security`
  - Filter at runtime: `QT_LOGGING_RULES="makine.*=true"` or `"makine.game=false"`
