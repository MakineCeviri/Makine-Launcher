# Makine-Launcher — Claude Context

> **Qt6/QML + C++23 oyun çeviri launcher**
> **Repo:** `origin` → MakineCeviri/Makine-Launcher (public, tek repo) · branches: `main` (stable, release-ready) + `dev` (WIP, daily driver)
> **Eski:** Makine-Launcher-Dev archived (read-only yedek, 2026-05-20)
> **Durum:** v0.1.4-beta | Vulkan default → D3D11 fix (5870b22, dev) + cherry-pick main (439ab92)
> **Blockers:** MSIX submit (sonraki sürüm)
> **Telemetri:** çalışıyor — `0.1.2-beta` 36 issue / 536 olay. Sentry API erişimi sorunsuz
> (token `.env`'de, `SENTRY_AUTH_TOKEN`). Açık fatal: 3. Denetim: `just telemetry-check`
>
> Build, conventions, gotchas → **`CLAUDE.md`** (proje kökü)
> Beta hazırlık akışı, key üretim, repo sync detayları → memory dosyaları (`~/.claude/projects/.../memory/`)

---

## Ekip Hafızası (`memory` MCP)

Bu projeyi ilgilendiren **20 hazır not** var — araştırmaya başlamadan önce `memory_search`:

| `project` | Not | İçerik |
|-----------|-----|--------|
| `makine-launcher-dev` | 9 | `.makine` paketleme + R2/D1/CDN yayın süreci · MSIX Store kimlik değerleri · CDN/R2 erişim yolları · core testlerini çalıştırma · build hız tercihi · çalışma tarzı geri bildirimleri |
| `makine` | 7 | encryption key **değişmezliği** · pwsh 7.6.3 bozuk build · Sentry sessiz hata modları · MSIX sürümleme kuralı |
| `makineai` | 4 | çeviri paketi analizi · güvenlik/code signing · referans temizlik kuralları · dağıtım mimarisi |

- Yeni bir şey öğrenince: `memory_upsert(project: "makine-launcher-dev")`.
- **Qt/QML/MinGW tuzakları projeler arası geçerli** — aynı toolchain'i kullanan `scframework`
  notlarına da bak (48 not; ör. `qt-automoc-stale-cache`, `qt-extra-column-proxy-pattern`).
- Detaylı kurallar → global `CLAUDE.md` › *Ekip Hafızası*.

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
