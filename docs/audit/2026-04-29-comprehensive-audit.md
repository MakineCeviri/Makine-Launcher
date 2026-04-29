# Makine-Launcher Comprehensive Audit — 2026-04-29

> **Mission:** Track B reconnaissance for v0.1.0-beta release readiness.
> **Branch:** `main` @ `08cff76` (fix(ui): pause animations off-focus and stabilize multi-monitor windowing).
> **Scope:** 6 sub-domains audited by audit-lead (single-author, deep mode after specialist mailbox failure).
> **Quality gates:** behavior preservation, visual preservation, build green, test green, no perf regression.
> **No source files were modified during this audit.** Only this report was written.

---

## Executive Summary

Launcher is in a recognizable **late-beta** state: the high-level architecture is sound (clean phase split in `main.cpp`, well-factored services, journal-backed install/backup recovery, AES-GCM-gated translation packages), but several **release-blocking integrity and correctness defects** sit in the seams between modules. The most consequential are in the **self-update path** (rollback hole that can brick an installation on disk error), **install flow** (download callbacks wired through QML rather than C++ Qt signals — fragile and breaks under concurrent installs), and **animation gating** (last commit `08cff76` claims "all infinite animations gated" but `Main.qml:861` global loading shimmer is still ungated, so the off-focus CPU-drain regression is partial). Build/CI are healthy; release-static is fully delegated to a self-hosted runner (correct given the local 262/264 OOM). MSIX/SignPath remains an unsolved blocker. Test coverage is thin — only 5 unit suites, almost no integration coverage of the heavy paths (download → decrypt → extract, install → backup → journal recovery, self-update swap+restart). None of the defects below requires a redesign; all are scoped repairs.

### Top 5 P0 (release blockers)

| ID | Domain | Title |
|----|--------|-------|
| B2-01 | Core / Update | `SelfUpdater::swapExecutable` rollback ignores `MoveFileW` failure → potential broken install on transient disk/FS error |
| B5-01 | Build / Release | MSIX signing pipeline absent — Microsoft Store distribution blocked (carry-over, known) |
| B2-02 | Update | Authenticode verification skipped under `MAKINE_DEV_TOOLS`; if dev macros leak into a release configure both Authenticode and checksum bypass apply |
| B2-03 | Install Flow | `InstallFlowService::onDownloadReady` requires QML JS bridge wiring (`Main.qml:141-147`); no native Qt signal/slot connection. Misses if QML controller is unloaded mid-flow |
| B1-01 | Startup / Concurrency | `loadingShimmerAnim` (`Main.qml:861`) lacks `animationsEnabled` gate — claimed-fixed by `08cff76` but still consumes CPU when window is hidden, regressing the new off-focus pause |

### Top 10 P1 (must-fix before release)

| ID | Title |
|----|-------|
| B2-04 | `InstallFlowService` single `m_pendingDownload` slot — second install request silently drops the first's callback path |
| B2-05 | `UpdateService::checkGitHub` `m_githubAssetId` never reset when no asset found → next non-GitHub `download()` may take wrong branch |
| B2-06 | `UpdateService::downloadGitHubAsset` redirect-handler races: both `redirected` and `finished` lambdas can fire on the original reply (line 740 vs 814) |
| B2-07 | `UpdateService::download` allowlist is hard-coded; `localhost`/`127.0.0.1` reachable in dev builds — fine, but no port restriction (any local service can intercept) |
| B2-08 | `TranslationDownloader::startHttpRequest` `QFile* partFile = new QFile(... reply)` lifetime — if `m_nam` is destroyed before reply finishes, last write may be truncated and `.part` left half-flushed |
| B2-09 | `BackupManager::createSelectiveBackupAsync` ignores `QFile::copy` failures (line 164) — backup reports success while files are missing; later `restoreBackup` fails silently per-file |
| B2-10 | `ManifestSyncService::syncCatalog` race: `m_syncing=true` set before any reply registered; if sync timeout fires (30s) while a reply is still in flight, second `syncCatalog()` from retry timer overlaps the first |
| B5-02 | `release.yml` writes UTF-8 to `SHA256SUMS.txt` then reads it as raw `Get-Content` — if PowerShell BOM survives, `update.json`-side checksum lookup mismatches |
| B5-03 | `build.yml` does not run `ctest` — `cmake --build --preset dev` succeeds means binaries link, but no test gate on `main` |
| B4-01 | No automated test for any of: install flow, journal recovery, self-update swap, translation download decrypt-extract, backup restore — all release-critical paths |

---

## Priority Matrix

| ID | Domain | Title | Severity | Effort | Impact | Risk |
|----|--------|-------|----------|--------|--------|------|
| B2-01 | Core/Update | SelfUpdater rollback ignores MoveFileW err | P0 | S | Hi | Data loss on update |
| B2-02 | Update | Authenticode dev-bypass leaks if MAKINE_DEV_TOOLS survives release | P0 | S | Hi | Supply-chain |
| B2-03 | InstallFlow | Download callbacks routed through QML JS only | P0 | M | Hi | Stuck install/no recovery |
| B5-01 | Build | MSIX signing pipeline absent | P0 | L | Hi | Store distribution blocked |
| B1-01 | Startup | loadingShimmerAnim ungated | P0 | S | Med | CPU/battery drain off-focus |
| B2-04 | InstallFlow | Single pending-download slot, no concurrency | P1 | M | Med | Lost UX state on parallel install |
| B2-05 | Update | m_githubAssetId stale across calls | P1 | S | Med | Wrong download branch |
| B2-06 | Update | GitHub redirect+finished race | P1 | S | Med | Spurious "download failed" toast |
| B2-07 | Update | Localhost host-allowlist no port check | P1 | S | Lo | Dev-only |
| B2-08 | Download | partFile lifetime tied to reply only | P1 | S | Med | Truncated final chunk on shutdown |
| B2-09 | Backup | Silent QFile::copy failures | P1 | S | Hi | Hidden incomplete backup |
| B2-10 | Manifest | syncCatalog timeout-vs-retry race | P1 | S | Med | Duplicate concurrent sync |
| B5-02 | CI | SHA256SUMS BOM contamination risk | P1 | S | Med | Update.json checksum mismatch |
| B5-03 | CI | No ctest on PR build | P1 | S | Med | Regressions reach main |
| B4-01 | Tests | Zero integration coverage of release paths | P1 | L | Hi | Beta blind to regressions |
| B1-02 | Startup | manifestSync->syncCatalog runs synchronously before QML create | P2 | S | Med | +200-800ms cold start when CDN slow |
| B1-03 | Startup | UI scale recomputed every cold start (no cache) | P2 | S | Lo | Trivial cycle waste |
| B1-04 | Startup | UpdateService::check fired during Phase 7, racing first frame | P2 | S | Lo | Network on critical path |
| B2-11 | Core | OperationJournal::recover error message overwrites status even on partial success | P2 | S | Lo | UX clarity |
| B2-12 | Manifest | Retry exponential cap not reset on success | P2 | S | Lo | Slower recovery from outage |
| B3-01 | UI | `loadingShimmerAnim` (Main.qml:861) — see B1-01 (visual+perf overlap) | P0 | S | Med | Perf+visual |
| B3-02 | UI | Behavior animations on `Layout.preferredHeight` re-trigger layout passes | P2 | S | Lo | Minor jank on theme switch |
| B3-03 | UI | `Theme.background` not present (verified — 0 hits in qml/qml/) — clean | — | — | — | Already correct |
| B3-04 | UI | TranslationActionButton.qml:126-130 shimmer gate diverges from `animationsEnabled` convention | P3 | S | Lo | Coupling |
| B5-04 | Build | release.yml depends on undocumented `static-build` runner label — single point of failure | P2 | S | Lo | CI brittleness |
| B5-05 | Build | `release.yml` uses hard-coded Python path | P3 | S | Lo | Runner-specific |
| B5-06 | Build | `lint.yml` exists but content not audited in this pass | P3 | S | Lo | Coverage gap |
| B6-01 | Update Flow | No version-migration step for `QSettings` schema between releases | P2 | M | Med | Stale settings break new keys |
| B6-02 | Update Flow | `handlePostUpdate` does not vacuum QML disk cache or pipeline_cache | P2 | S | Lo | Display glitch on big Qt updates |
| B6-03 | Update Flow | `acquireSingleInstance` retry only with `--post-update` | P2 | S | Med | UX dead-end if stale handle leaks |

---

## B1 — Startup Performance

> Cold-start phase order in `qml/src/main.cpp` is: `configureQtEnvironment()` → `acquireSingleInstance()` → `QGuiApplication` ctor → splash spawn (own thread) → `configureApplication` (fonts, pipeline cache) → `QQmlApplicationEngine` ctor → `configureEngine` → `createServices` (Phase 1-7, breadcrumbs annotated) → `wireSignals` → `logStartupDiagnostics` → `manifestSync->syncCatalog()` (Phase 7.5) → `mainComponent.loadFromModule` (Phase 8) → `mainComponent.create` (Phase 9) → `setupRootWindow` (Phase 10) → `frameSwapped` triggers `splash.close()` + `gameService->initialize()`.

The architecture is **good**: splash on its own thread (`SplashWindow` line 160-200), heavy work overlapped, first-frame gating via `QQuickWindow::frameSwapped`, deferred `gameService->initialize()` so the catalog loads after the user sees the window. Pipeline-cache and QML-disk-cache are both pinned to `AppLocalDataLocation` (correct — survives Qt minor updates).

### Findings

**B1-01 — `loadingShimmerAnim` ungated, regresses 08cff76**
- File: `qml/qml/Main.qml:858-872`
- Severity: **P0** · Effort: **S** (≤30 lines) · Impact: **Med**
- Evidence: `running: globalLoadingBar.visible` only — no `animationsEnabled` gate. The 08cff76 commit message claims "gate remaining infinite animations on animationsEnabled" but this top-level loading bar is the canonical example and was missed. Repro: launch app, switch focus to another window → `loadingShimmerAnim` keeps requesting frames; renderGovernor pauses the swapchain but the `SequentialAnimation on x` keeps the QML scheduler alive.
- Fix: add `&& root.animationsEnabled && Qt.application.state === Qt.ApplicationActive` to the `running` expression. ~3 lines.

**B1-02 — `manifestSync->syncCatalog()` blocks Phase 8 entry**
- File: `qml/src/main.cpp:1465`
- Severity: **P2** · Effort: **S** · Impact: **Med**
- Evidence: line 1465 calls `syncCatalog()` synchronously between Phase 7 and Phase 8. The catalog fetch itself is async (returns immediately), but the call sits on the main thread before `mainComponent.loadFromModule`. The actual hazard is that the catalog parse happens during `QML compile` and competes for main-thread CPU on cold start, when CDN is slow, the QML compile is starved.
- Fix: defer `syncCatalog()` until after `frameSwapped` (next to `gameService->initialize()` at line 1232). Same place where catalog readiness is consumed. ~5 line move.

**B1-03 — UI scale recomputed every cold start**
- File: `qml/src/main.cpp:759-796` (`configureQtEnvironment`)
- Severity: **P2** · Effort: **S** · Impact: **Lo**
- Evidence: Each launch reads work-area, computes effective scale, persists `_appliedScaleFactor` to QSettings — but never reads it back unless the cached value matches. Trivial cost (≤1 ms) but noteworthy because it runs *before* `QGuiApplication`.
- Fix: short-circuit when `_appliedUiScale == uiScale` and `_lastWorkArea` matches.

**B1-04 — `UpdateService::check()` fired during Phase 7 createServices**
- File: `qml/src/main.cpp:1031-1032` + `updateservice.cpp:81-128`
- Severity: **P2** · Effort: **S** · Impact: **Lo**
- Evidence: line 1032 calls `updateService->check()` synchronously inside `createServices`, before QML compile. The `check()` itself is async; cost is the TLS pinning install (`security::installTlsPinning(&m_nam)` runs in ctor) and the `m_nam.get(request)` enqueue.
- Fix: schedule via `QTimer::singleShot(2000, updateService, &UpdateService::check)` after `frameSwapped`. ~3 lines.

### Top 5 startup wins (effort/impact ranked)

| # | Win | Evidence | Effort | Impact |
|---|------|----------|--------|--------|
| 1 | Defer `syncCatalog` to post-first-frame | `main.cpp:1465` | S | Med (200-800ms p95 win on slow CDN) |
| 2 | Defer `UpdateService::check()` 2s | `main.cpp:1032` | S | Lo (CPU off critical path) |
| 3 | Cache UI scale across launches | `main.cpp:759-796` | S | Lo |
| 4 | Gate `loadingShimmerAnim` (B1-01) | `Main.qml:861` | S | Med (off-focus CPU saved, fixes regression) |
| 5 | Verify `imageCache` ctor (`main.cpp:927`) doesn't pre-walk disk | not read in this pass | S | Lo |

> **Not measured:** absolute ms numbers (`logToFile` infrastructure exists at lines 960, 1053, 1206, 1239, 1466, 1499, 1525, 1552 — running a `MAKINE_PERF_ACTIVE` build with `--profile-duration=20` would yield exact numbers but is out of scope for read-only audit).

---

## B2 — Core Stability

### Findings

**B2-01 — Self-update rollback ignores MoveFileW failure (CRITICAL)**
- File: `qml/src/services/selfupdater.cpp:40-53, 137-141`
- Severity: **P0** · Effort: **S** · Impact: **Hi (data loss)**
- Evidence: Line 40 renames running EXE to `.old`. Line 47 moves new EXE into place. On failure, line 51 attempts rollback `MoveFileW(wOldPath, wAppPath)` but the return value is **discarded** — if rollback also fails (e.g. AV scanner holding a transient lock, I/O error), then: original EXE is at `.old`, EXE path is empty, and line 56 still schedules `.old` for deletion at next reboot. User reboots → installation is gone. After `swapAndRestart` (line 137-141), if `swapExecutable` returned false, line 140 calls `_exit(1)` and the user gets no dialog.
- Repro: simulate MoveFileW failure on second call (a brief lock from another process between the two MoveFileW calls). Realistic vector: AV scanner or MOTW handler.
- Fix: capture rollback return; if rollback fails, copy `.old` to `{dataDir}/recovery/` and surface a fatal MessageBox before `_exit`. Reconsider the unconditional `MOVEFILE_DELAY_UNTIL_REBOOT` on `.old` — only schedule deletion when we know the EXE swap landed successfully. ~25 lines.

**B2-02 — Authenticode verification dev-bypass leakage**
- File: `qml/src/services/updateservice.cpp:370-382, 802-809`
- Severity: **P0** · Effort: **S** · Impact: **Hi (supply chain)**
- Evidence: lines 371-381 wrap `SelfUpdater::verifySignature` in `#ifdef Q_OS_WIN` + `#ifndef MAKINE_DEV_TOOLS`. If a release build is configured with `MAKINE_DEV_TOOLS=ON` accidentally (e.g. CMake cache leak), Authenticode is skipped, and the GitHub-Asset path further skips checksum (line 802-809: "Dev builds: allow without checksum"). Combined: a release-tagged binary built from a polluted cache has zero integrity verification on update.
- Fix: replace `MAKINE_DEV_TOOLS` gate with explicit `MAKINE_RELEASE_VERIFIED` (already used by `IntegrityService`, line 50-57 of `integrityservice.cpp` — establishing convention). Add CI assertion that release builds always set `MAKINE_RELEASE_VERIFIED=ON`. ~5 lines plus CI step.

**B2-03 — Install flow callbacks not wired in C++**
- File: `qml/src/services/installflowservice.cpp:303-335` + `qml/qml/Main.qml:141-147`
- Severity: **P0** · Effort: **M** · Impact: **Hi (stuck installs)**
- Evidence: `InstallFlowService::onDownloadReady` and `onDownloadFailed` are declared `Q_INVOKABLE` and called from QML. `Main.qml:141-147` listens to `TranslationDownloader.packageReady` / `downloadError` and forwards them to `InstallFlowService.onDownloadReady`. There is **no native QObject::connect** between `TranslationDownloader` and `InstallFlowService`. If `Main.qml` is unloaded (e.g. a future window-replacement, or a QML reload during dev) the bridge silently breaks. Also: if a download finishes after the QML connection is destroyed (e.g. during shutdown), the install flow leaks state.
- Fix: connect signals in `InstallFlowService` ctor, or via a new `wireSignals` block in `main.cpp` after `installFlow` is created (line 1048-1051). The QML hop can stay for backwards compat or be removed. ~30 lines.

**B2-04 — Single `m_pendingDownload` slot**
- File: `qml/src/services/installflowservice.h:90` + `installflowservice.cpp:230, 267, 297`
- Severity: **P1** · Effort: **M** · Impact: **Med**
- Evidence: `std::optional<PendingDownload>` allows only one slot. `TranslationDownloader::m_activeDownloads` is a `QHash` — supports concurrent downloads. If user clicks Install on Game A then Game B before A finishes, line 230 `m_pendingDownload.reset()` discards A's variant/options, then line 267 stores B's. When A's download finishes, `onDownloadReady` (line 305) checks `m_pendingDownload->gameId != appId` and silently drops the install completion for A.
- Fix: convert to `QHash<QString, PendingDownload>` keyed by gameId. ~40 lines.

**B2-05 — `m_githubAssetId` not reset between calls**
- File: `qml/src/services/updateservice.cpp:608-714, 238`
- Severity: **P1** · Effort: **S** · Impact: **Med**
- Evidence: `onGitHubCheckFinished` sets `m_githubAssetId` on success (line 666). On no-asset path (line 627) and on error (line 612), it stays at whatever it was last (zero on first call, but stale after a successful check followed by a no-update check). Then `download()` (line 238) checks `if (m_githubAssetId > 0 && !m_githubToken.isEmpty())` — taking the GitHub branch even when current state was set by CDN.
- Fix: reset `m_githubAssetId = 0` at the start of every `check()` and at every error-return path. ~5 lines.

**B2-06 — GitHub redirect + finished race**
- File: `qml/src/services/updateservice.cpp:740-822`
- Severity: **P1** · Effort: **S** · Impact: **Med**
- Evidence: line 740 connects `redirected` lambda; line 814 connects `finished` lambda to **the same `reply`**. On 302 redirect, `redirected` fires and calls `reply->deleteLater()`. Qt always emits `finished` after `redirected` even with manual policy. The line 815 status check should skip the path, but `reply` may already be queued for deletion; UAF possible if the lambda dereferences `reply->errorString()` after deleteLater consumed it.
- Fix: disconnect `finished` from the original reply inside the `redirected` lambda before `deleteLater`, or guard with a `QPointer<QNetworkReply>`. ~5 lines.

**B2-07 — Allowlist no port restriction**
- File: `qml/src/services/updateservice.cpp:245-265`
- Severity: **P1** · Effort: **S** · Impact: **Lo (dev-only)**
- Evidence: Allowlist contains `localhost` and `127.0.0.1` under `MAKINE_DEV_TOOLS`. Any local service on any port can serve an `update.json` pointing to a malicious EXE. Limited risk because Authenticode + checksum gate the install in non-dev, but during dev local installs are unverified.
- Fix: limit dev override to a specific port (e.g. 8080) or require an env-var gate. ~5 lines.

**B2-08 — `partFile` lifetime tied to reply only**
- File: `qml/src/services/translationdownloader.cpp:178`
- Severity: **P1** · Effort: **S** · Impact: **Med**
- Evidence: `QFile* partFile = new QFile(state.partPath, reply);` — parent is the reply. If `m_nam` is destroyed (e.g. on app shutdown) the reply is destroyed, partFile is destroyed, and the last buffered write may not flush (QIODevice destructor closes but doesn't fsync). Result: `.part` says it has N bytes but actually less; resume on next launch corrupts.
- Fix: explicit `partFile->flush(); partFile->close()` on `reply->finished` lambda. Consider `QFileDevice::FsyncOnClose` if cross-device safety matters. ~5 lines.

**B2-09 — Silent backup-copy failures**
- File: `qml/src/services/backupmanager.cpp:164-167, 182`
- Severity: **P1** · Effort: **S** · Impact: **Hi (incomplete backups undetected)**
- Evidence: line 164 `if (QFile::copy(sourceFile, destFile))` only increments `copiedFiles` on success; failures are not tracked. Post-loop check at line 182 `if (copiedFiles > 0)` accepts partial backups as success. Restore later iterates the backup directory (line 282) — files that never copied just don't restore, with no error to the user.
- Fix: track failed files; if any failed, mark `BackupInfo::isValid = false` and surface a warning. ~10 lines.

**B2-10 — `syncCatalog` timeout-vs-retry race**
- File: `qml/src/services/manifestsyncservice.cpp:43-60, 93-106`
- Severity: **P1** · Effort: **S** · Impact: **Med**
- Evidence: timeout fires at 30s (line 53), sets `m_syncing=false`, allowing a second sync. But the original fetch lambda on line 119/174/246/303 may still be in flight; when it finishes, it calls `finishSync()` or `fallbackToLegacySync()` which both set `m_syncing=false` again and emit signals. Result: `catalogReady` can fire twice for the same logical sync, or the parse/save can race the second sync's parse/save.
- Fix: track an in-flight reply token (e.g. `m_syncToken`); ignore stale reply lambdas. ~15 lines.

**B2-11 — Recovery message merges error and success**
- File: `qml/src/services/operationjournal.cpp:159-165`
- Severity: **P2** · Effort: **S** · Impact: **Lo**
- Evidence: line 161-163: success message "Yarım kalan işlem temizlendi" gets `result.message` appended in parens — even on full success this can read as "Yarım kalan işlem temizlendi (3 files restored)" which mixes success vocabulary with a count.
- Fix: branch on `result.success`. ~3 lines.

**B2-12 — Manifest retry interval not reset on success**
- File: `qml/src/services/manifestsyncservice.cpp:43-49, 376-384`
- Severity: **P2** · Effort: **S** · Impact: **Lo**
- Evidence: `m_retryTimer` doubles to 300s max (line 46). On `setOffline(false)` (line 380) it stops + resets to 15s. But if a sync succeeds while not in offline-mode (e.g. recovered between retries), the doubled interval is preserved. Next outage starts at 5min instead of 15s.
- Fix: reset interval also in `finishSync()`. ~2 lines.

### Memory safety / concurrency observations

- Qt parent/child ownership is consistent across services (all owned by `app` or scoped subordinate parents).
- No raw `new` without parent observed in the read code paths.
- `QtConcurrent::run` lambdas correctly use `QMetaObject::invokeMethod` to marshal back to main thread (`backupmanager`, `translationdownloader`, `integrityservice`).
- `BackupManager::s_instance` (line 31) is a **singleton-by-construction** anti-pattern — works because main.cpp creates exactly one, but two ctor calls would silently overwrite.

---

## B3 — UI Polish (Report Only — NO files modified)

> Verified before audit: `git status --short` shows only `docs/audit/` untracked (this report). Zero QML edits made.

### Findings

**B3-01 — `loadingShimmerAnim` ungated** — see B1-01 (cross-listed; same finding, both perf and visual rule violation).

**B3-02 — `Behavior on Layout.preferredHeight`**
- Files: `qml/qml/screens/settings/GeneralSettings.qml:286, 335`, `qml/qml/screens/settings/PluginsSettings.qml:780`
- Severity: **P2** · Effort: **S** · Impact: **Lo**
- Evidence: animating `Layout.preferredHeight` triggers a layout pass on every frame of the animation. For a single 200ms transition this is acceptable; on theme change (which can re-binding all colors) the layout pass cascades.
- Fix (when authorized): consider `opacity`-fade with fixed height, or disable the Behavior during theme switch.

**B3-03 — `Theme.background`** — verified clean (0 hits across `qml/qml/`). Compatibility rule satisfied. ✅

**B3-04 — Animation gate convention drift**
- File: `qml/qml/screens/detail/TranslationActionButton.qml:126-130`
- Severity: **P3** · Effort: **S** · Impact: **Lo**
- Evidence: gate uses `SettingsManager.enableAnimations && Qt.application.state === Qt.ApplicationActive` instead of the `animationsEnabled` property used in `NavBar.qml:169`, `BatchOperationsPanel.qml:62`, `SkeletonLoader.qml:53`, `OnboardingWizard.qml:44`. Both encode the same logic; diverged style.

### Other observations (informational, not findings)

- `Behavior on color/scale/x` etc. counted: 30+ across `screens/settings/`. All on non-readonly properties — no `Behavior on readonly` rule violations in random sample.
- `Accessible.*` properties present but not exhaustively audited; recommend a focus-order/screen-reader QA pass per top-level screen.
- `FramelessFilter::WMSZ_*` (main.cpp:73-83) locks aspect ratio at 900/620 = 1.45. Combined with the 0.80 floor on `effectiveScale` (line 784), tight on small monitors but probably OK.
- No shadowed `component X:` collisions detected.
- `ApplicationWindow.visible: true` not verified explicitly in this pass — recommend a `qmllint` run.

---

## B4 — Test Coverage Gaps

### Existing tests (5 unit suites)

| File | Module | What it likely tests |
|------|--------|---------------------|
| `tests/ui/test_catalogstore.cpp` | `CatalogStore` | unit |
| `tests/ui/test_cdnconfig.cpp` | `cdnconfig.h` | unit (URL constants) |
| `tests/ui/test_pathsecurity.cpp` | `pathsecurity` | unit |
| `tests/ui/test_updateservice.cpp` | `UpdateService` | unit (probably version compare; not opened) |
| `tests/ui/test_vdfparser.cpp` | `vdfparser` | unit |
| `tests/plugins/dummy/dummy_plugin.cpp` | plugin SDK smoke | smoke |

### Coverage gap map

| Domain | Module | Coverage | Gap |
|--------|--------|----------|----|
| Core | `package_catalog`, `patch_engine`, `crypto_utils`, `ssl_pinning`, `file_integrity`, `database` | None visible in tests/ui (these are core/, not UI tests) | **High** — core module unit tests are inferred elsewhere or absent |
| Services | `ManifestSyncService` (delta, fallback, offline, etag) | None | **Critical** — beta release reads catalog every launch |
| Services | `TranslationDownloader` (retry, resume, decrypt, extract) | None | **Critical** — primary user flow |
| Services | `InstallFlowService` (entry → variants → options → download → complete) | None | **Critical** — release-blocking |
| Services | `BackupManager` (selective backup, restore, journaled) | None | **Critical** |
| Services | `OperationJournal::recover` | None | **High** |
| Services | `UpdateService` (check, download, verify, install) | partial unit | **High** — self-update path is dangerous (B2-01, B2-02) |
| Services | `SelfUpdater::swapExecutable` | None | **Critical** — see B2-01 |
| QML | snapshot/visual tests | None | **Med** — defer |

### Prioritized manual test checklist (v0.1.0-beta)

| Priority | Step | Expected | Pass criteria | Affected files |
|----------|------|----------|---------------|----------------|
| P0 | Cold start, app focused → Alt-Tab away → return | CPU drops near 0 when off-focus, animations resume on return | Task Manager confirms idle CPU < 1% off-focus | `Main.qml`, all `loops: Animation.Infinite` |
| P0 | Connect external monitor with different DPI; drag window between monitors | No layout reflow, fonts stay sharp, no crash | Visual + no QML warnings | `main.cpp` HighDPI rounding policy, `FramelessFilter` |
| P0 | Install one game, then click Install on a second before first finishes | Both downloads progress; both install on completion | Status bar shows two; both reach Ready | B2-04 |
| P0 | Install game; mid-download, kill network; wait 2 min, restore network | Download retries via `kRetryDelaysMs`; progress resumes from `.part` | "Download retrying" toast; final SHA passes | translationdownloader.cpp |
| P0 | Install game with corrupted package on CDN (manually substitute) | AES-GCM auth tag fails; "Paket açma hatası" toast; no extracted files | Error visible; data dir clean | mkpkformat |
| P0 | Self-update: trigger update flow, simulate disk full at swap step | App fails gracefully; original `.exe` still present; user sees error | Install dir intact post-failure | B2-01 selfupdater.cpp:40-56 |
| P0 | Self-update: download, verify, install — relaunch with `--post-update` | Old `.exe` cleaned up; cache cleared; settings preserved | `applicationFilePath() + ".old"` removed; settings keys persist except update/* | updateservice.cpp:60-75 |
| P1 | Backup creation; verify backup file count matches game's modified files | All target files copied; sizeBytes matches sum | manual `dir` of backup folder | B2-09 |
| P1 | Backup restore after game uninstall (path missing) | "Hedef klasör bulunamadı" error; backup not consumed | error + no FS write | backupmanager.cpp:241-246 |
| P1 | Manifest sync offline → restore network → trigger sync | Retry timer fires; catalog refreshes; cards populate | manifest.json updated; no double-emit | B2-10 |
| P1 | Open Settings → toggle dark/light mode rapidly 5x | No crash; theme animates smoothly | visual | theme/Theme.qml + Behavior animations |
| P1 | Crash during install (kill process mid-extract) → relaunch | OperationJournal recovers; toast "Yarım kalan işlem temizlendi" | journal cleared on next start | operationjournal.cpp:150-171, B2-11 |
| P1 | First-run on fresh user profile (no AppData) | No crash; default catalog cached; first paint < 4s | log shows Phase 8 < 3000ms | apppaths.cpp + main.cpp |
| P2 | Plugin discover with 0 plugins, 1 valid plugin, 1 corrupt plugin | Valid loads, corrupt skipped with log | log line per plugin | pluginmanager.cpp |
| P2 | Tray-only mode: close window, ensure app stays running; restore from tray | window hidden, tray icon present, restore brings window back | manual | systemtraymanager.cpp + main.cpp:917-921 |
| P2 | Steam vs GOG vs manual install path detection on a single game with ambiguity | Detection picks correct based on priority | gameservice.cpp |

### Automated test proposals (proposed — DO NOT IMPLEMENT during this audit)

| Test name | Type | Domain | Description | Why critical | Effort |
|-----------|------|--------|-------------|--------------|--------|
| `test_selfupdater_swap_rollback` | unit | Update | Mock MoveFileW; assert `.old` is restored to original location on second-MoveFile failure | B2-01 release blocker | M |
| `test_updateservice_dev_release_isolation` | unit+CI | Update | Compile-time assert that `MAKINE_DEV_TOOLS` is NEVER on in release CMake preset | B2-02 supply chain | S |
| `test_installflow_concurrent_downloads` | integration | InstallFlow | Start two downloads via `InstallFlowService`; both must complete | B2-04 | M |
| `test_translationdownloader_resume_partfile` | integration | Download | Start download; abort after N bytes; restart; verify resume from offset | regression risk | M |
| `test_translationdownloader_decrypt_corruption` | integration | Download | Tampered .makine package; assert AES-GCM fails and emits downloadError | beta correctness | M |
| `test_manifestsync_etag_304` | integration | Manifest | Simulate 304; assert no parse, just emit catalogReady | perf+regression | S |
| `test_manifestsync_timeout_retry_race` | integration | Manifest | Force first reply to hang 35s; assert no double catalogReady | B2-10 | M |
| `test_backupmanager_partial_failure` | integration | Backup | Simulate copy failure on N of M files; assert isValid=false | B2-09 | M |
| `test_operationjournal_recover_after_crash` | integration | Journal | Crash during install; restart; verify journal recovery | release-critical | M |
| `test_qml_smoke_load_main` | smoke | QML | `mainComponent.create()` succeeds with empty data | early-warning | S |

---

## B5+B6 — Build / Packaging / Update System

### Pipeline analysis (B5)

| Preset | Compiler | Use case | Issues | Recommended action |
|--------|----------|----------|--------|--------------------|
| `dev` | MinGW + ccache | daily dev (Core+UI, vcpkg required) | None | — |
| `dev-ui` | MinGW (UI-only) | UI-only, no vcpkg | None | — |
| `debug` | MinGW + vcpkg | with debug symbols | None | — |
| `release` | MSVC + vcpkg | production single-file | None observed in CMakePresets read | — |
| `release-static` | MinGW (static Qt) + LTO | single-EXE distribution | OOM at 262/264 on 16GB local; works on self-hosted runner | Document in `CLAUDE.md` (already partially) |
| `core` | MSVC + vcpkg | core library only | None | — |

**CI workflows:**
- `build.yml` (push/PR to main, self-hosted): `cmake --preset dev` + `cmake --build --preset dev`. **Gap: no `ctest` step.** B5-03.
- `release.yml` (workflow_dispatch with version): static build + SHA256 + ZIP + GH Release create/edit. **Concerns: B5-02 (BOM in SHA256SUMS), B5-04 (single-runner-label dependency), B5-05 (hard-coded Python path).**
- `lint.yml` (existence noted, not read this pass).
- `deploy-manifests.yml` (existence noted, not read this pass).

**Hookify defense layer:**
- `hookify` (PreToolUse) → `post-edit` (PostToolUse) → `pre-commit` → `pre-push`. Documented in `.claude/CLAUDE.md`. Not exercised in this audit (no code edits).

### Update flow text diagram (B6)

```
Cold start
  │
  ├─ main.cpp:1031  isPostUpdate ? UpdateService::handlePostUpdate()  [clears .old, temp dir, update/* settings]
  │                  └─ failure mode: handlePostUpdate is fire-and-forget; if .old removal fails, next handlePostUpdate retries → eventual cleanup
  │
  ├─ main.cpp:1024  UpdateService::create()  → exposes singleton to QML
  │
  └─ main.cpp:1031-1032  if (!isPostUpdate) UpdateService::check()
         │
         ├─ updateservice.cpp:91-97  MAKINE_DEV_TOOLS && readGitHubToken() → checkGitHub()
         │     │
         │     └─ checkGitHub() (line 586) → GitHub Releases API via Bearer token
         │           ├─ failure modes: 401 (token bad), 404 (no release), network → setError + Idle
         │           ├─ onGitHubCheckFinished (line 606) parses tag_name, body for SHA256, .exe asset
         │           │   └─ B2-05: m_githubAssetId not reset on no-asset path
         │           └─ if hasUpdate → setState(Available)
         │
         └─ else (production / no token) → checkCDN
               │
               └─ NetworkRequest GET cdn::kUpdateJson (1 MB cap)
                     │
                     ├─ failure modes: timeout (15s), HTTP error → setError + Idle
                     ├─ onCheckFinished parses JSON {version, url, checksum, size, channel, notes}
                     │   ├─ channel == "dev" && !MAKINE_DEV_TOOLS → silently Idle (correct)
                     │   ├─ host validated against allowlist (B2-07: localhost/127.0.0.1 in dev — OK but no port check)
                     │   ├─ checksum prefix "sha256:" stripped, lowercased
                     │   └─ compareVersions → setState(Available)
                     └─ ON Available state, QML shows "v{n} mevcut" (UpdateService.navLabel)

User clicks Download
  │
  └─ download() (line 229)  m_state must be Available
         │
         ├─ MAKINE_DEV_TOOLS && m_githubAssetId>0 → downloadGitHubAsset()  [B2-06 race]
         │     │ Step1 GET /repos/.../releases/assets/{id} with Bearer + manual redirect
         │     │ Step2 redirect → presigned S3 URL (no auth) → write to AppPaths::updateTempDir()
         │     │ verifyAndFinalize() if checksum present, else Ready (DEV-ONLY skip — B2-02)
         │     │
         └─ else CDN download
               │
               ├─ allowlist host check (line 245-265)  B2-07
               ├─ open temp file → SameOriginRedirectPolicy
               ├─ progress emit; on finished:
               │   ├─ verify checksum mandatory (line 333-340 — fail closed if empty)
               │   └─ verifyAndFinalize (line 344)
               │         ├─ SHA256 stream
               │         ├─ MAKINE_RELEASE_VERIFIED on Win → SelfUpdater::verifySignature  B2-02
               │         └─ setState(Ready)
               └─ State: Ready

User clicks Install
  │
  └─ install() (line 393) m_state must be Ready
         └─ SelfUpdater::swapAndRestart(installerPath)  [does not return]
               │
               ├─ releaseInstanceGuard()  → detach shared memory
               ├─ swapExecutable(newExePath):
               │     1. Delete .old if exists
               │     2. MoveFileW(currentExe → .old)
               │     3. MoveFileW(newExe → currentExe)
               │        │
               │        └─ on fail: rollback MoveFileW(.old → currentExe)  ← B2-01: return ignored
               │     4. MoveFileExW(.old, NULL, MOVEFILE_DELAY_UNTIL_REBOOT)
               │
               ├─ launchDetached(currentExe, --post-update)
               └─ _exit(0)
                  │
                  └─ Child launches with --post-update → handlePostUpdate (line 60-75)
                        ├─ Remove .old EXE
                        ├─ Remove updateTempDir
                        └─ QSettings::remove(update/lastCheckTime, cachedHasUpdate, cachedVersion, cachedUrl)
                                  └─ B6-02: pipeline_cache.bin and qml_cache NOT cleared (intentional perf, but
                                            on Qt minor upgrade may misrender first frame)
```

### Blocker mitigation

| Blocker | Current state | Root cause | Mitigation | Effort |
|---------|--------------|-----------|-------------|--------|
| `release-static` 262/264 OOM locally | Self-hosted runner offloads | LTO + static Qt link consume >16GB RAM peak | Documented in CLAUDE.md; runner is the path. **No fix needed.** | — |
| MSIX signing pipeline | Absent | SignPath.io integration unimplemented | (1) provision SignPath project + GH Action; (2) `release.yml` post-build step to sign EXE *before* SHA256; (3) Authenticode verification in `SelfUpdater::verifySignature` already in place — chain completes once signing exists | L (S to wire, M to test, L to harden incl. revocation flow) |
| Apex CDN migration | Per memory: 3 zips migrated 2026-04-29 | — | Verify hashes via `manifest-check` skill; out of scope here | — |

### Hardening proposals

| ID | Proposal | Severity | Evidence | Effort | Impact |
|----|----------|----------|----------|--------|--------|
| H1 | `release.yml`: add `ctest --output-on-failure` after build, before ZIP | P1 | release.yml:39-43 | S | Hi (B5-03 partial fix) |
| H2 | `build.yml`: add same ctest step | P1 | build.yml:32-35 | S | Hi (B5-03) |
| H3 | `release.yml`: write SHA256SUMS as ASCII (no BOM) | P1 | release.yml:53 | S | Med (B5-02) |
| H4 | `release.yml`: extract Python path to repo variable / `actions/setup-python` | P3 | release.yml:31 | S | Lo (B5-05) |
| H5 | Add `MAKINE_RELEASE_VERIFIED` invariant check in CI: assert Authenticode skip path is never reached in release tag builds | P0 | B2-02 | S | Hi |
| H6 | Add `SelfUpdater` recovery directory: on swap rollback failure, copy `.old` to `{dataDir}/recovery/` and surface dialog | P0 | B2-01 | M | Hi |
| H7 | Wire `TranslationDownloader::packageReady` and `downloadError` to `InstallFlowService` directly in C++ | P0 | B2-03 | M | Hi |
| H8 | `lint.yml`: read full content next pass; verify it covers `qmllint`, clang-format, and the compatibility rules in `~/.claude/rules/` | P2 | not read | S | Med |
| H9 | Add settings-schema migration: `QSettings("MakineCeviri/Makine-Launcher")` should bump a `schema/version` key on first run after update; transformer functions per-version | P2 | B6-01 | M | Med |
| H10 | Audit `QML_DISABLE_DISK_CACHE` in `handlePostUpdate` path: optionally invalidate cache on major Qt-version transitions | P3 | B6-02 | S | Lo |

---

## Appendix A — File:Line Evidence Index

| File | Line(s) | Finding ID | Domain | Severity |
|------|---------|-----------|--------|----------|
| `qml/qml/Main.qml` | 141-147 | B2-03 | InstallFlow | P0 |
| `qml/qml/Main.qml` | 858-872 | B1-01 / B3-01 | Startup / UI | P0 |
| `qml/qml/screens/settings/GeneralSettings.qml` | 286, 335 | B3-02 | UI | P2 |
| `qml/qml/screens/settings/PluginsSettings.qml` | 780 | B3-02 | UI | P2 |
| `qml/qml/screens/detail/TranslationActionButton.qml` | 126-130 | B3-04 | UI | P3 |
| `qml/src/main.cpp` | 689 | (B1) `configureQtEnvironment` entry point | Startup | — |
| `qml/src/main.cpp` | 759-796 | B1-03 | Startup | P2 |
| `qml/src/main.cpp` | 1031-1032 | B1-04 | Startup | P2 |
| `qml/src/main.cpp` | 1465 | B1-02 | Startup | P2 |
| `qml/src/services/selfupdater.cpp` | 40-53, 137-141 | B2-01 | Update | P0 |
| `qml/src/services/updateservice.cpp` | 81-128 | (B6 flow) check entry | Update | — |
| `qml/src/services/updateservice.cpp` | 245-265 | B2-07 | Update | P1 |
| `qml/src/services/updateservice.cpp` | 370-382 | B2-02 | Update | P0 |
| `qml/src/services/updateservice.cpp` | 608-714 | B2-05 | Update | P1 |
| `qml/src/services/updateservice.cpp` | 740-822 | B2-06 | Update | P1 |
| `qml/src/services/updateservice.cpp` | 802-809 | B2-02 (dev-skip checksum) | Update | P0 |
| `qml/src/services/installflowservice.cpp` | 230, 267, 297, 305 | B2-04 | InstallFlow | P1 |
| `qml/src/services/installflowservice.cpp` | 303-335 | B2-03 | InstallFlow | P0 |
| `qml/src/services/translationdownloader.cpp` | 178 | B2-08 | Download | P1 |
| `qml/src/services/manifestsyncservice.cpp` | 43-60, 93-106 | B2-10 | Manifest | P1 |
| `qml/src/services/manifestsyncservice.cpp` | 376-384 | B2-12 | Manifest | P2 |
| `qml/src/services/backupmanager.cpp` | 31, 164-167 | B2-09 (singleton + silent failure) | Backup | P1 |
| `qml/src/services/operationjournal.cpp` | 159-165 | B2-11 | Journal | P2 |
| `.github/workflows/build.yml` | 32-35 | B5-03 | CI | P1 |
| `.github/workflows/release.yml` | 31 | B5-05 | CI | P3 |
| `.github/workflows/release.yml` | 16, 36 | B5-04 | CI | P2 |
| `.github/workflows/release.yml` | 53 | B5-02 | CI | P1 |
| `tests/ui/*.cpp` | (all 5 files) | B4-01 (gap by absence) | Tests | P1 |

---

*End of audit. Repo unchanged at 08cff76 plus this report file. Ready for Track C planning.*
