# Integration Test Coverage — B4-01

> Status as of 2026-05-02. Tracks the audit's "zero integration coverage"
> finding piecemeal as the qml services get a testable boundary.

## Covered ✅

| Suite | File | What it exercises |
|-------|------|-------------------|
| `test_operationjournal_recover_integration` | `tests/integration/test_operationjournal_recover.cpp` | begin / record / commit / abort, simulated mid-operation crash with `kFlushInterval` rollover, recovery against missing target paths |

## Not Yet Covered 🟡

| Service | Blocker | Notes |
|---------|---------|-------|
| `BackupManager` | header pulls `<QQmlEngine>` (and `appprotection.cpp` pulls `<QGuiApplication>`) — a headless test binary would have to drag in `Qt6::Gui` and the QML runtime | Move BackupManager + AppProtection into a `qml_services_object` CMake OBJECT library, then the test binary can link only the bits it needs. ~2 h. |
| `TranslationDownloader` | network path — needs a localhost mock server (or a `QNetworkAccessManager` swap) for download / resume / extract round trip | Start with `QNetworkAccessManager` injection so tests can hand in a fake; live CDN is too flaky for CI. ~3 h. |
| `SelfUpdater` | rollback path runs Win32 `MoveFileW`; needs file-system fault injection to exercise the brick-recovery branch | Refactor `swapAndRestart` to take a filesystem-op functor we can override under test. ~2 h. |
| `InstallFlowService` | pulls `GameService` + `TranslationDownloader` + `CoreBridge` + `ManifestSyncService` — each itself unmockable today | Comes for free once the three above are unmockable. |

## Strategy

The single biggest unblocker is converting the qml services CMake target from
"sources compiled directly into the launcher executable" into a separate
`OBJECT` (or `STATIC`) library that:

1. Has a clean public include surface (no `<QGuiApplication>` /
   `<QQmlEngine>` leakage from header files unless strictly needed).
2. Can be linked from both `MakineLauncher.exe` and the integration tests
   without recompiling the `.cpp` files.
3. Lets each test pull in only the services it needs, without pulling in
   `appprotection`, the QML runtime, or other unrelated machinery.

That refactor is the natural follow-up to v0.1.0-beta — it doesn't change
runtime behaviour and keeps the existing build green, but it unlocks the
remaining audit coverage in one move.

## Out of Scope for Beta

- End-to-end install / uninstall against a real game directory (covered by
  manual user testing per release).
- Self-update against a real GitHub release (gated on the MSIX signing
  pipeline landing — B5-01).
