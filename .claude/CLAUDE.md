# Makine-Launcher — Claude Context

> **Qt6/QML + C++23 oyun çeviri launcher**
> **Repo:** `origin` → Makine-Launcher-Dev (private) · `public` → Makine-Launcher (public)
> **Durum:** v0.1.0-alpha ~%92 | Blockers: Static Qt build, MSIX signing
>
> Build, conventions, gotchas → **`CLAUDE.md`** (proje kökü)

---

## Agents

| Agent | Domain | Model |
|-------|--------|-------|
| `core-dev` | C++ core, CMake, vcpkg | opus |
| `ui-dev` | QML, frontend services, theme | sonnet |
| `qa` | Pre-release: build → test → lint → audit | — |
| `ops` | CMake, justfile, build system | — |
| `refactor` | Code cleanup, restructuring | — |
| `perf` | Profiling, optimization | — |

## Skills

| Category | Commands |
|----------|----------|
| **Build & Run** | `/build` `/test` `/verify` `/format` `/lint` |
| **Analysis** | `/inspect` `/ui-check` `/security-scan` `/deps` `/doctor` |
| **Project** | `/git-overview` `/quick-status` `/changelog` `/manifest-check` `/scan-refs` |

## Hookify Rules (8 active)

| Rule | Action | Trigger |
|------|--------|---------|
| `protect-personal-workspace` | block | Writes to `C:\Workspace\Cedra\` |
| `protect-secrets` | block | `.env`, `encryption_key.h`, `*.pem`, `*.pfx`, `*.key` |
| `block-regex-header` | block | `#include <regex>` (broken on MinGW 13.1) |
| `block-theme-background` | block | `Theme.background` → use `Theme.bgPrimary` |
| `block-desktop-output` | block | Build artifacts to Desktop |
| `protect-ui-design` | warn | Animations, MultiEffect, gradients |
| `block-hardcoded-paths` | block | Absolute paths in source code |
| `block-large-files` | warn | `git add` without size check (>5 MB → CDN) |

## Permissions

### Allowed (auto-approve)
`cmake`, `just`, `ctest`, `git status/diff/log/branch/stash`, `ls`, `clang-format`, context-mode MCP tools

### Denied (hard block)
Read/Write/Edit: `.env*`, `encryption_key.h`, `scripts/certs/**`, `*.pem`, `*.pfx`, `*.key`, `*.p12`, `credentials.json`

## Plugins

| Plugin | Status | Purpose |
|--------|--------|---------|
| `clangd-lsp` | enabled | C++ intellisense, diagnostics |
| `pyright-lsp` | disabled | Not a Python project |
| `rust-analyzer-lsp` | disabled | Not a Rust project |
| `typescript-lsp` | disabled | Not a TypeScript project |
| `frontend-design` | disabled | Not using web frontend |

## Secrets — NEVER commit

`qml/src/services/encryption_key.h` · `.env` · `scripts/certs/**` · `*.pem` · `*.pfx` · `*.key`

## Defense Layers

```
hookify (PreToolUse) → post-edit (PostToolUse) → pre-commit → pre-push
```
