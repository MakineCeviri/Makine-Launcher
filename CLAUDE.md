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

- **QML UI** — `qml/qml/` `screens/` · `components/` · `dialogs/` · `controllers/` · `theme/`. PascalCase `.qml`, pure declarative UI, no JS logic. Apple structure in 0.1.5's tone (sidebar shell, lit near-black page `AppBackground`, every section on a `Card` container with a lit top edge, `EdgeFade`/`TopDissolve` instead of hard cuts, `Theme`/`Dimensions`/`Icons` tokens, `AppButton`/`ListSection`/`ListRow`/`SegmentedControl`…): [`docs/design.md`](docs/design.md) — build new UI from it.
- **C++ service layer** — `qml/src/services/` bridges Core ↔ UI (`GameService`, `CoreBridge`, `InstallFlow`, `BackupMgr`, `BatchOps`, `PackageCatalog`, `TranslationState`, `TranslationDownloader`, `ManifestSync`, `UpdateService`, `SteamDetails`, `RenderGov`, `CompanionService`).
- **Makine, the companion** — closed source, in the private repo `MakineCeviri/Makine-Companion` (proprietary, not AGPL). Clone it next to this repo: CMake finds `../Makine-Companion`, links its `makine_companion` library and defines `MAKINE_HAS_COMPANION`; without it the launcher builds without him (`Companion.available` false, his settings hidden) and a `MAKINE_RELEASE_VERIFIED` build stops. This repo holds only his side of the launcher: `CompanionService` (talks to his one header, `makine/companion.h`), `screens/home/MakineCorner.qml`, `Sidebar.qml` `companionDock`. Never add his engine, paint or window code here; his design and pitfalls are in that repo's `docs/companion.md`.
- **C++ core library** — `core/include/makine/` + `core/src/`: package catalog, crash-recovery journal, file integrity, path checks, error types, logging macros, plugin ABI (`plugin/abi.h` + header-only `sdk.hpp`). Game detection, VDF parsing and patching live in the service layer.
- **Plugins** (ADR-0009, guide `docs/plugins/README.md`) — each enabled plugin runs out of process in `makine-plugin-host.exe` (`tools/plugin-host/`, Qt-free, static); launcher side `PluginManager` + `plugin{process,manifest,registry,installer}`; signed CDN registry; `scripts/plugin_registry.py` (pack/sign) and `plugin_dev.py` (run a plugin without the launcher). Still behind `MAKINE_DEV_TOOLS`.

External: `cdn.makineceviri.org` (Cloudflare R2) and local game files (Steam, GOG, …).

### Package catalog — hybrid model

| Phase | Source | Purpose |
|-------|--------|---------|
| Startup | R2 `assets/index.json` | Catalog metadata, package size + SHA-256 (download verification) |
| On-demand | `makineceviri.org/api/v2/games/{appId}` (Makine-WEB Worker → D1 `makine-catalog`) | Install steps, notes, contributors, variants |

- **A recipe fix is a D1 change**, not an R2 one: neither the launcher nor `catalog_validate.py` reads R2 `assets/packages/*.json` — an old mirror that has drifted from D1.
- **A recipe reaches every launcher at once.** A new step action makes the recipe unrunnable for older builds, so: bump `steprules::kRecipeLevel` (`installsteprules.h`) with the action — the detail request sends it as `X-Makine-Recipe` — and put the recipe older launchers can run in D1 `install_methods_compat` (`level` = the new level; Makine-WEB `migrations/0009`). 0.1.5 sends nothing (level 1).
- **A patch is checked against the copy it was made on** — OyunBul's fingerprint (`gameprint.h`: the package's `game.bin`, or the catalogue's `fingerprint.exe`/`altExes`/object `keyFiles`), byte-compatible with OyunBul.exe (`test_gameprint`). Another version or another folder holds the install for the player's "Yine de kur"; `${exeDir}`/`${gameRoot}` in run args are recipe level 4 (`docs/patch-programs.md`; what our patch tools still need, and the plan: `docs/yama-araclari.md`).
- **The launcher's own news has one source** — `qml/resources/whatsnew.json` (release notes + plan) is built in and shown on "Yenilikler" (`WhatsNewScreen.qml`, `releasenotes.h`). The same file plus `latest`/`store` goes to the CDN as `assets/launcher.json` (`just launcher-json`, Assets push), which is how 0.2.3+ hears of a new version: ZIP compares `latest` and opens the release page, the MSIX compares `store` and opens the Store. Bumping the version without a `whatsnew.json` entry fails `test_releasenotes`; raise `store` only after Partner Center has published. `assets/update.json` is 0.2.2-and-older only — leave it.
- **A patch's own program (`run` step) runs under a contract** — [`docs/patch-programs.md`](docs/patch-programs.md): the step's `requires` files are checked in the game folder first, the program starts from a `%TEMP%` copy of the package whose scripts set the game folder the launcher found (`runstage.h`: `set OYUN=D:\…` → `%MAKINE_OYUN%`), `<game>/.makine` notes the folder, what it changed is recorded as `_program:` (`changescan.h`; nothing changed = not installed), and its `uninstall` program is waited on. Recipe level 3.
- **Launchers fetch `index.json` again only when the catalog version moves** (`MAX(catalog_versions.version)` from `/api/v2/catalog/meta`; `/catalog/delta` always answers 410 so they take the full index). A rewritten `index.json` alone (`scripts/deploy.py`, an Assets push) reaches nobody who has synced before — the admin panel (makineceviri.org/admin/paketler) raises the version on every publish and has "Launcher'lara duyur" for changes made outside it. The launcher asks at start, on Ctrl+R / library refresh, every 30 min and when the window regains focus (≥10 min apart; quiet when unchanged). A new index (`version`/`generatedAt`) drops every cached detail. `Makine-Launcher-Assets` `main` must equal R2 — a push there syncs `assets/` to R2.
- `packages/<dirName>/` is reused for an install/update only when `data/package_stamps.json` says it was extracted from the package the catalogue lists now (`packagestamp.h`). Cached covers are re-asked every 3 days with `If-None-Match: "<md5>"` (R2's ETag) — `cache/image_checks.json`.

- Entry points: `PackageCatalog::loadFromIndex()` + `enrichPackage()`
- CDN config centralized in `qml/src/services/cdnconfig.h`
- Prefixes: `assets/` (index, packages, images, banners) · `data/` (encrypted `.makine` packages)

Other top-level dirs: `tests/` · `docs/` (ADRs in `docs/adr/`) · `infra/` (Docker, Caddy) · `scripts/` · `third_party/` (vendored xdelta3 decoder, Apache-2.0 — provenance in its README) · `build/` (gitignored: `dev/`, `debug/`, `release/`).

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
| Core C++ | `.hpp` / `.cpp` | `snake_case` | `package_catalog.hpp` |
| UI C++ | `.h` / `.cpp` | `lowercase` | `gameservice.h` |
| QML | `.qml` | `PascalCase` | `GameDetailScreen.qml` |

- Classes `PascalCase` · functions & variables `camelCase` · constants `UPPER_SNAKE_CASE` · namespace `makine` (`snake_case`)
- C++23 · `#pragma once` · comments in English · prefer native C++ over Qt for business logic

## Logging

| Layer | System | Usage |
|-------|--------|-------|
| Core | spdlog via `MAKINE_LOG_*` macros | `core/include/makine/logging.hpp` |
| UI | `QLoggingCategory` | `qCDebug(lcXxx)` / `qCWarning(lcXxx)` |

UI categories: `makine.app` · `.game` · `.bridge` · `.package` · `.download` · `.batch` · `.backup` · `.process` · `.integrity` · `.manifest` · `.journal` · `.steam` · `.update` · `.updater` · `.security` · `.render` · `.plugin` · `.companion` (debug: frame stats every 10 s). Toggle with `QT_LOGGING_RULES="makine.*=true"` / `QT_LOGGING_RULES="makine.game=false"`.

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
| QML | A `Shape` on `Shape.CurveRenderer` (Qt 6.11) | Draws nothing after the window is minimized or hidden and shown again, until its path changes — `main.cpp` `reviveCurveShapes` flips every such Shape's renderer on restore; keep new curve Shapes under the window's content item |
| QML | A parent's `visible` bound to its children's `visible` | Reading `visible` gives the effective value: once the parent hides, the children read hidden and it never shows again — derive it from the data (`HomePage.qml` `heroRow`) |
| QML | `QV4_MM_AGGRESSIVE_GC` is a V4 debug switch (a full GC on every JS allocation) | Never set it: up to 0.1.5 `main.cpp` did, and every JS-heavy binding ran 100–300× slower (2 s freezes) |
| vcpkg | Manifest mode | Classic mode only (`VCPKG_MANIFEST_MODE=OFF`), triplet `x64-mingw-dynamic` |
| Qt | Every `*.lnk` is read as a shell shortcut | A game data file named `.lnk` (Nioh) "does not exist", has size 0, `QDir::Files` skips it — use `rawfile.h` for game/package files |
| MinGW libstdc++ | Paths past MAX_PATH | File I/O needs the `\\?\` form (long paths are off by default; `mkpkformat.h` `io_path`); `fs::remove_all` never returns on such a tree, and directory iterators list entries that are not there below a `\\?\` root — delete with Qt, or walk with `FindFirstFileW` (`crash_recovery.cpp`) |
| Qt | `emit` is a macro | A C struct field or function named `emit` in a header Qt code includes breaks the build (`abi.h` uses `emit_event`) |
| msvcrt | `stderr` on a pipe is fully buffered | A helper process that can be `TerminateProcess`ed loses its last lines — `setvbuf(stderr, nullptr, _IONBF, 0)` (plugin host) |
| Plugin host | DLL search is plugin folder → host folder → System32 | Plugins link the C++ runtime statically or ship it; `PATH` is never searched. The host manifest makes the code page UTF-8 and the process per-monitor DPI aware (coordinates are physical pixels) |
| Packaging | Python `tarfile` defaults to PAX | ustar name holds an ASCII stand-in that 0.1.5 reads — `package_pipeline.py` writes USTAR (GNU fallback) |
| Sentry | Reads DWARF, not the COFF symbol table | A release `.sym` without `-g` symbolicates every frame of ours to `qml_register_types_MakineLauncher` (0.1.5); `qml/CMakeLists.txt` adds `-g1` in Release — check with `addr2line -f -e Makine-Launcher.exe.sym <addr>` |
| C++ | `std::filesystem::relative` / `weakly_canonical` in a loop | Each call canonicalises its paths on disk: 5.7 s for a 5,460-file package (Elden Ring), 13 s+ on players' PCs, on the UI thread — use `lexically_relative` on paths an iterator gave you, canonicalise a root once |
| Win32 | `HeapCompact` over `GetProcessHeaps()` | Never: other modules own those heaps and can destroy one mid-call (0.1.5 startup fatals 3C/1V) |
| CI | Git Bash puts Git's `ucrt64\bin` before MinGW 13.1 on `PATH` | A test run from a bash step loads Git's `libstdc++-6.dll`; the spdlog-linked suites fail to bind an import and hang silently on a loader dialog — run `ctest` from `pwsh` (`ci.yml`), or put `mingw1310_64/bin` first |
| Win32 | A job object assigned after `QProcess::start` | Misses what a `.bat` `start`s in its first milliseconds — create the child inside the job (`PROC_THREAD_ATTRIBUTE_JOB_LIST` via `setCreateProcessArgumentsModifier`, `runProcess`). Read `JOBOBJECT_BASIC_PROCESS_ID_LIST` from a flat buffer: indexing its `ProcessIdList[1]` past 0 is UB and GCC -O2 drops the loop |

## Deferred features

Intentionally deferred — stub headers removed:
- Translation Memory, Glossary Service, QA Service, Translation Pipeline
- Engine Handlers and the BepInEx/XUnity runtime — fully removed in `b5a003b`, no stub left
  (`IEngineHandler` / `RuntimeManager` no longer exist)
- Integration tests disabled until handlers are implemented

## Rules

**Do NOT**
- Invent colours, font sizes, button styles or Segoe MDL2 glyphs — use the tokens and components in `docs/design.md`; motion stays short and calm (no endless pulses)
- Output build artifacts to the Desktop
- Commit secrets (`.env`, `.key`, `.pfx`, `.pem`, `encryption_key.h`, `scripts/certs/**`) or build artifacts (`.exe`, `.dll`, `.obj`, `.lib`, `build/`)
- Commit files > 5 MB — use the CDN instead
- Hardcode absolute paths in source

**Commits:** [Conventional Commits](https://www.conventionalcommits.org/) `type(scope): description` · types `feat` `fix` `refactor` `build` `ci` `docs` `test` `chore` · scopes `core` `ui` `build` `ci` `docs` (lowercase). Enforced by the local `commit-msg` hook; `pre-commit` / `pre-push` hooks run the build and integrity checks. Hooks resolve the repo via `git rev-parse --show-toplevel` (worktree-safe).
