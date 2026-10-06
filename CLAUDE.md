# Makine-Launcher

> Turkish game translation launcher & adaptation engine.
> Qt 6 / QML + C++23 · MinGW 13.1 / MSVC 2022 · vcpkg · CMake

| | |
|---|---|
| **Repo** | `origin` → MakineCeviri/Makine-Launcher (public, single repo) |
| **Branches** | `dev` (active development; release tags live here) · `main` (default branch, separate history — no common ancestor with `dev`) |
| **Push** | `git push` → origin/dev · release: tag on `dev` (`docs/RELEASING.md`) · scheduled workflows must also exist on `main` (PR) |

## Architecture

Three layers, top to bottom:

- **QML UI** — `qml/qml/` `screens/` · `components/` · `dialogs/` · `controllers/` · `theme/`. PascalCase `.qml`, pure declarative UI, no JS logic.
- **C++ service layer** — `qml/src/services/` bridges Core ↔ UI (`GameService`, `CoreBridge`, `InstallFlow`, `BackupMgr`, `BatchOps`, `PackageCatalog`, `TranslationState`, `TranslationDownloader`, `ManifestSync`, `UpdateService`, `SteamDetails`, `RenderGov`).
- **C++ core library** — `core/include/makine/` + `core/src/`: game detection, patch engine, package catalog, security (crypto, SSL pinning, file integrity, sandbox), VDF parser, database, cache, async/parallel, logging, validation, config.

External: `cdn.makineceviri.org` (Cloudflare R2) and local game files (Steam, GOG, …).

### Package catalog — hybrid model

| Phase | Source | Purpose |
|-------|--------|---------|
| Startup | `index.json` | Lightweight catalog metadata |
| On-demand | `packages/{appId}.json` (~700 B) | Install steps, contributors, variants |

- Entry points: `PackageCatalog::loadFromIndex()` + `enrichPackage()`
- CDN config centralized in `qml/src/services/cdnconfig.h`
- Prefixes: `assets/` (index, packages, images, banners) · `data/` (encrypted `.makine` packages)

Other top-level dirs: `tests/` · `docs/` (ADRs in `docs/adr/`) · `infra/` (Docker, Caddy) · `scripts/` · `build/` (gitignored: `dev/`, `debug/`, `release/`).

## Build

`encryption_key.h` is gitignored. Generate it once after a fresh clone (required for non-`dev-ui` builds):

```bash
python scripts/generate_key_header.py   # reads scripts/.encryption_key → qml/src/services/encryption_key.h
```

```bash
just dev          # MinGW dev build (Core+UI+tests, vcpkg required)
just dev-ui       # UI-only build (no vcpkg, no encryption_key.h needed)
just run          # Run after build
just test         # Build dev, run every suite (core + UI + integration)
just core         # Core library only (MinGW)
just release-zip-dynamic <ver>   # Distribution ZIP (docs/RELEASING.md)
```

PATH (bash):
```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Program Files/Git/usr/bin:$PATH"
export PATH="/c/Qt/6.11.1/mingw_64/bin:$PATH"  # Qt DLLs for runtime
```

| Preset | Compiler | Use case |
|--------|----------|----------|
| `dev` | MinGW + vcpkg | Daily development (Core+UI) |
| `dev-ui` | MinGW | UI-only, no vcpkg (`MAKINE_UI_ONLY=ON`) |
| `debug` | MinGW + vcpkg | Core+UI with debug symbols |
| `release-mingw` | MinGW + vcpkg | Distribution build (ZIP, MSIX) |
| `release` | MSVC + vcpkg | MSVC release (needs `Qt6_DIR` → an MSVC kit) |
| `release-static` | MinGW (static Qt) | Single EXE — needs a Qt static kit built from source |
| `core` | MinGW + vcpkg | Core library + its tests (`build/core`) |

## Coding conventions

| Layer | Extension | File style | Example |
|-------|-----------|------------|---------|
| Core C++ | `.hpp` / `.cpp` | `snake_case` | `game_detector.hpp` |
| UI C++ | `.h` / `.cpp` | `camelCase` | `gameService.h` |
| QML | `.qml` | `PascalCase` | `GameDetailScreen.qml` |

- Classes `PascalCase` · functions & variables `camelCase` · constants `UPPER_SNAKE_CASE` · namespace `makine` (`snake_case`)
- C++23 · `#pragma once` · comments in English · prefer native C++ over Qt for business logic

## Logging

| Layer | System | Usage |
|-------|--------|-------|
| Core | spdlog via `MAKINE_LOG_*` macros | `core/include/makine/logging.hpp` |
| UI | `QLoggingCategory` | `qCDebug(lcXxx)` / `qCWarning(lcXxx)` |

UI categories: `makine.app` · `.game` · `.bridge` · `.package` · `.download` · `.batch` · `.backup` · `.process` · `.integrity` · `.manifest` · `.journal` · `.steam` · `.update` · `.updater` · `.security` · `.render`. Toggle with `QT_LOGGING_RULES="makine.*=true"` / `QT_LOGGING_RULES="makine.game=false"`.

## Known gotchas

| Area | Issue | Rule |
|------|-------|------|
| MinGW 13.1 | `<regex>` is broken | Use `find()`, `starts_with()`, `ends_with()` |
| MinGW 13.1 | `<set>` / `<map>` not implicit | Always `#include` explicitly |
| MinGW 13.1 | spdlog ADL collision | Fully qualified `spdlog::info()` |
| MinGW 13.1 | Forward decls in `#ifdef` | Place at file top level (AUTOMOC) |
| QML | `Theme.background` | Does not exist — use `Theme.bgPrimary` |
| QML | `ApplicationWindow.visible` | Defaults to `false` — keep `visible: true` |
| QML | `Behavior on` a readonly property | Runtime crash — use a non-readonly property |
| QML | `component X:` | Don't shadow shared component names |
| QML | `clip: true` in scrollables | Required for Flickable, ListView, ScrollView |
| QML | `Connections.target` changed from its own handler | Use-after-free in delegates (Qt 6.11, NATIVE-74) — keep `target` fixed, gate with `enabled` |
| vcpkg | Manifest mode | Classic mode only (`VCPKG_MANIFEST_MODE=OFF`), triplet `x64-mingw-dynamic` |

## Deferred features

Intentionally deferred — stub headers removed:
- Translation Memory, Glossary Service, QA Service, Translation Pipeline
- Engine Handlers (only the `IEngineHandler` interface in `engine_handler.hpp`)
- BepInEx/XUnity runtime (fully removed — `RuntimeManager` is a stub)
- Integration tests disabled until handlers are implemented

## Rules

**Do NOT**
- Touch UI animations, MultiEffect, or gradient designs
- Output build artifacts to the Desktop
- Commit secrets (`.env`, `.key`, `.pfx`, `.pem`, `encryption_key.h`, `scripts/certs/**`) or build artifacts (`.exe`, `.dll`, `.obj`, `.lib`, `build/`)
- Commit files > 5 MB — use the CDN instead
- Hardcode absolute paths in source

**Commits:** [Conventional Commits](https://www.conventionalcommits.org/) `type(scope): description` · types `feat` `fix` `refactor` `build` `ci` `docs` `test` `chore` · scopes `core` `ui` `build` `ci` `docs` (lowercase). Enforced by the local `commit-msg` hook; `pre-commit` / `pre-push` hooks run the build and integrity checks. Hooks resolve the repo via `git rev-parse --show-toplevel` (worktree-safe).
