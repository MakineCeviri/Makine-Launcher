# Makine-Launcher

> Turkish game translation launcher & adaptation engine.
> Qt 6 / QML + C++23 · MinGW 13.1 / MSVC 2022 · vcpkg · CMake

| | |
|---|---|
| **Repo** | `origin` → MakineCeviri/Makine-Launcher (public, single repo) |
| **Branches** | `main` (stable, release-ready) · `dev` (active development) |
| **Push** | `git push` → origin/dev · release: merge dev → main |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        QML UI Layer                         │
│  screens/  ·  components/  ·  dialogs/  ·  controllers/     │
│  theme/                                                     │
│  (PascalCase.qml — pure declarative UI, no JS logic)        │
├─────────────────────────────────────────────────────────────┤
│                    C++ Service Layer                         │
│  qml/src/services/ — bridges Core ↔ UI                      │
│                                                             │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐ │
│  │ GameService   │ │ CoreBridge   │ │ PackageCatalog       │ │
│  │               │ │ InstallFlow  │ │ TranslationState     │ │
│  │ UpdateService │ │ BackupMgr    │ │ TranslationDownloader│ │
│  │ SteamDetails  │ │ BatchOps     │ │ ManifestSync         │ │
│  │ RenderGov    │ │              │ │ TranslationDownloader│ │
│  └──────────────┘ └──────────────┘ └──────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                      C++ Core Library                       │
│  core/include/makine/ + core/src/                           │
│                                                             │
│  game_detector · patch_engine · package_catalog · security  │
│  crypto_utils  · ssl_pinning · file_integrity  · sandbox    │
│  vdf_parser    · database    · cache · async · parallel     │
│  logging (spdlog) · validation · config · error handling    │
└─────────────────────────────────────────────────────────────┘
         │                              │
         ▼                              ▼
   cdn.makineceviri.org          Local Game Files
   (Cloudflare R2)               (Steam, GOG, etc.)
```

### Package Catalog — Hybrid Model

| Phase | Source | Purpose |
|-------|--------|---------|
| Startup | `index.json` (93 KB, 273 games) | Lightweight catalog metadata |
| On-demand | `packages/{appId}.json` (~700 B) | Install steps, contributors, variants |

- Entry points: `PackageCatalog::loadFromIndex()` + `enrichPackage()`
- CDN config centralized in `qml/src/services/cdnconfig.h`
- Asset prefix: `assets/` (index, packages, images, banners)
- Data prefix: `data/` (encrypted `.makine` packages)

---

## Project Structure

```
Makine-Launcher/
├── core/                    C++ core library
│   ├── include/makine/      Public headers (.hpp, snake_case)
│   └── src/                 Implementation files
├── qml/                     Qt/QML application
│   ├── src/services/        C++ backend services (.h/.cpp, camelCase)
│   └── qml/                 QML frontend
│       ├── screens/         Top-level screens (HomePage, Library)
│       ├── components/      Reusable UI components (36 files)
│       ├── dialogs/         Modal dialogs
│       ├── controllers/     QML logic controllers
│       └── theme/           Theme definitions
├── tests/                   Test suites
│   └── plugins/             Plugin tests
├── docs/                    Documentation
│   ├── adr/                 Architecture Decision Records
│   ├── api-reference/       API docs
│   ├── developer-guide/     Developer guides
│   └── security/            Security documentation
├── infra/                   Infrastructure (Docker, Caddy)
├── scripts/                 Build & utility scripts
└── build/                   Build output (gitignored)
    ├── dev/                 MinGW dev build
    ├── debug/               Debug build
    └── release/             MSVC release build
```

---

## Build

### First-time Setup

`encryption_key.h` is gitignored. Generate it once after fresh clone (required for non-`dev-ui` builds):

```bash
python scripts/generate_key_header.py   # reads scripts/.encryption_key → qml/src/services/encryption_key.h
```

### Commands

```bash
just dev          # MinGW dev build (Core+UI, vcpkg required)
just dev-ui       # UI-only build (no vcpkg needed, encryption_key.h not required)
just run          # Run after build
just test         # Run tests
just core         # Core library only (MSVC)
just release      # MSVC release build
```

### PATH (bash)

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Program Files/Git/usr/bin:$PATH"
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"  # Qt DLLs for runtime
```

### Presets

| Preset | Compiler | Use Case |
|--------|----------|----------|
| `dev` | MinGW + vcpkg | Daily development (Core+UI) |
| `dev-ui` | MinGW | UI-only, no vcpkg (`MAKINE_UI_ONLY=ON`) |
| `debug` | MinGW + vcpkg | Core+UI with debug symbols |
| `release` | MSVC + vcpkg | Production release |
| `release-static` | MinGW (static Qt) | Single EXE distribution |
| `core` | MSVC + vcpkg | Core library only |

---

## Coding Conventions

### File Naming

| Layer | Extension | Style | Example |
|-------|-----------|-------|---------|
| Core C++ | `.hpp` / `.cpp` | `snake_case` | `game_detector.hpp` |
| UI C++ | `.h` / `.cpp` | `camelCase` | `gameService.h` |
| QML | `.qml` | `PascalCase` | `GameDetailScreen.qml` |

### Identifiers

| Element | Style | Example |
|---------|-------|---------|
| Classes | `PascalCase` | `GameService` |
| Functions & variables | `camelCase` | `loadFromIndex()` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_RETRY_COUNT` |
| Namespace | `snake_case` | `makine` |

### General Rules

- **Standard:** C++23 · **Namespace:** `makine`
- **Headers:** `#pragma once`
- **Comments:** English
- **Preference:** Native C++ over Qt for business logic

---

## Logging

| Layer | System | Usage |
|-------|--------|-------|
| Core | spdlog via `MAKINE_LOG_*` macros | `core/include/makine/logging.hpp` |
| UI | `QLoggingCategory` | `qCDebug(lcXxx)` / `qCWarning(lcXxx)` |

**UI categories:** `makine.app` · `makine.game` · `makine.bridge` · `makine.package` · `makine.download` · `makine.batch` · `makine.backup` · `makine.process` · `makine.integrity` · `makine.manifest` · `makine.journal` · `makine.steam` · `makine.update` · `makine.updater` · `makine.security` · `makine.render`

```bash
QT_LOGGING_RULES="makine.*=true"          # Enable all
QT_LOGGING_RULES="makine.game=false"      # Disable specific
```

---

## Known Gotchas

> Full list with examples: `~/.claude/rules/compatibility-rules.md`

### MinGW GCC 13.1

| Issue | Workaround |
|-------|------------|
| `<regex>` is broken | Use `find()`, `starts_with()`, `ends_with()` |
| `<set>` / `<map>` not implicit | Always `#include` explicitly |
| spdlog ADL collision | Use fully qualified `spdlog::info()` |
| Forward decls in `#ifdef` | Place at file top level (AUTOMOC) |

### QML

| Issue | Rule |
|-------|------|
| `Theme.background` | Does not exist — use `Theme.bgPrimary` |
| `ApplicationWindow.visible` | Defaults to `false` — keep `visible: true` |
| `Behavior on readonly` | Runtime crash — use non-readonly property |
| `component X:` shadows | Don't shadow shared component names |
| `clip: true` in scrollables | Required for Flickable, ListView, ScrollView |

### vcpkg

- Classic mode only — set `VCPKG_MANIFEST_MODE=OFF`
- Triplet: `x64-mingw-dynamic`

---

## Deferred Features

These modules are intentionally deferred — stub headers removed:

- Translation Memory, Glossary Service, QA Service, Translation Pipeline
- Engine Handlers (only `IEngineHandler` interface in `engine_handler.hpp`)
- BepInEx/XUnity runtime (fully removed — `RuntimeManager` is a stub)
- Integration tests disabled until handlers are implemented

---

## Rules

### Do NOT

- Touch UI animations, MultiEffect, or gradient designs
- Output build artifacts to Desktop
- Commit secrets (`.env`, `.key`, `.pfx`, `.pem`, `encryption_key.h`)
- Commit build artifacts (`.exe`, `.dll`, `.obj`, `.lib`, `build/`)
- Commit files > 5 MB — use CDN instead

### Commits

[Conventional Commits](https://www.conventionalcommits.org/): `type(scope): description`

**Types:** `feat` · `fix` · `refactor` · `build` · `ci` · `docs` · `test` · `chore`
**Scopes:** `core` · `ui` · `build` · `ci` · `docs`

### Defense Layers

```
hookify (PreToolUse) → post-edit (PostToolUse) → pre-commit → pre-push
```

Hookify rules: `.claude/hookify.*.local.md` — blocks anti-patterns, Desktop output, hardcoded paths.
Hooks use dynamic git root (`git rev-parse --show-toplevel`) — worktree-compatible.
