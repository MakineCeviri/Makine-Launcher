# Performance Architecture — Makine Launcher

**Date:** 2026-03-24
**Status:** Approved
**Scope:** Startup flow, FPS governance, lazy loading, GPU efficiency, memory budget

---

## Problem

- Login screen freezes after splash closes (~2-4s unresponsive)
- No FPS cap — renders at max refresh rate even when idle
- 6x-wide gradient rectangle hammers GPU every frame
- 10+ services constructed eagerly before first frame
- Canvas elements use CPU rendering without caching
- Recursive QML bindings on window resize

## Goals

- Splash closes ONLY when UI is ready — zero freeze after splash
- 60 FPS cap during interaction, 1-5 FPS when idle
- Minimal RAM usage — lazy loading, component destruction
- Apple-level responsiveness — instant page transitions

---

## Design

### 1. Splash as Initialization Gate

Current: splash closes on first frame, heavy work continues after.
New: splash closes when ALL critical-path work is done.

```
Phase 1: Qt environment + app construct          (~50ms)
Phase 2: Critical services (Auth, Settings, Theme) (~100ms)
Phase 3: QML engine + root object                 (~200ms)
Phase 4: Auth token check (credential store)       (~50ms)
Phase 5: ManifestSync start (non-blocking fetch)   (~0ms, async)
Phase 6: First QML frame render                    (~100ms)
Phase 7: Splash close → UI interactive

Post-splash (event loop, user never sees):
  - GameService.initialize()
  - BackupManager, ProcessScanner, IntegrityService
  - BatchOperationService, OperationJournal, PluginManager
```

Key change: `AuthService.checkStoredToken()` moves from `Main.qml onCompleted` to C++ Phase 4 (before QML load). By the time QML renders, auth state is already resolved.

### 2. FPS Governor (C++ — `RenderGovernor`)

New lightweight C++ class attached to QQuickWindow:

| State | FPS | Trigger |
|-------|-----|---------|
| Active | 60 (vsync) | User interaction, animation running |
| Idle | on-demand | No animation for 500ms |
| Background | 0 | Window minimized/hidden |

Implementation:
- Keep `QSG_RENDER_LOOP=threaded` (best smoothness)
- Use `QQuickWindow::setRenderTarget()` frame callback for throttle
- Connect to `QQuickWindow::afterAnimating` — if no animations pending, skip render
- Property `window.renderPolicy: "active" | "idle" | "background"`
- Expose to QML so screens can force active during transitions

### 3. GPU-Efficient Background

Replace `OnboardingWizard.qml` 6x-wide gradient:

**Before:** Dynamic 6600px gradient Rectangle rendered every frame
**After:** Pre-rendered gradient image (1920x1 or 1920x1080 PNG, <50KB)

```qml
Image {
    anchors.fill: parent
    source: "qrc:/resources/images/onboarding_bg.png"
    fillMode: Image.Stretch
    asynchronous: false  // Small file, instant load
}
```

For animated gradient shift: use `ShaderEffect` with a single uniform offset instead of a massive Rectangle.

### 4. Deferred Service Construction

Split services into tiers:

**Tier 0 — Pre-QML (during splash):**
- SettingsManager, AuthService, CrashReporter, ManifestSyncService, ImageCacheManager

**Tier 1 — Post first-frame (QTimer::singleShot(0)):**
- GameService.initialize(), CoreBridge catalog load

**Tier 2 — On-demand (first use):**
- BackupManager → created when user opens installed game
- ProcessScanner → created when game launch detected
- IntegrityService → created on first install/update
- BatchOperationService → created when batch panel opened
- OperationJournal → created when first install starts
- PluginManager → created when plugins settings opened

Use factory pattern: `Q_INVOKABLE QObject* getService(QString name)` in a ServiceLocator.

### 5. Canvas → Shapes or Cached Canvas

Replace imperative Canvas with:

**Option A (preferred):** Qt Quick Shapes (GPU-accelerated)
```qml
Shape {
    ShapePath {
        strokeColor: root.accentColor
        strokeWidth: 1.4
        PathLine { ... }
    }
}
```

**Option B:** Canvas with FramebufferObject render target
```qml
Canvas {
    renderStrategy: Canvas.Cooperative
    renderTarget: Canvas.FramebufferObject
}
```

Priority: InstallOptionsDialog icons, header icons, checkbox indicators.

### 6. Window Resize — Native Aspect Lock

Move aspect ratio enforcement from QML bindings to C++ native event filter:

```cpp
case WM_SIZING: {
    auto* rect = reinterpret_cast<RECT*>(lParam);
    enforceAspectRatio(rect, wParam, aspectRatio);
    *result = TRUE;
    return true;
}
```

Remove `onWidthChanged`/`onHeightChanged` recursive bindings from Main.qml.

### 7. Lazy Component Loading

| Component | Current | New |
|-----------|---------|-----|
| OnboardingWizard | Always loaded when `_onboardingActive` | Same (needed at start) |
| HomeScreen | Loader async ✓ | Keep |
| Library | Loader async ✓ | Destroy after 60s hidden |
| Settings | Preloaded behind splash | Loader on-demand, keep alive |
| GameDetail | Loader on-demand ✓ | Destroy after 30s hidden |
| Dialogs | Loader on-demand ✓ | Keep (already lazy) |

### 8. Memory Budget

- Image cache: max 50MB, LRU eviction
- QML component pool: destroy hidden screens after timeout
- `EmptyWorkingSet` on minimize (already present ✓)
- Texture atlas: 512x512 (already present ✓)
- Aggressive JS GC (already present ✓)

---

## Files to Modify

| File | Changes |
|------|---------|
| `main.cpp` | Reorder phases, move token check pre-QML, add RenderGovernor |
| `Main.qml` | Remove async token check, remove aspect ratio bindings |
| `OnboardingWizard.qml` | Replace gradient with image |
| `InstallOptionsDialog.qml` | Canvas → Shape or cached Canvas |
| `translationdownloader.h/cpp` | Already cleaned ✓ |

## New Files

| File | Purpose |
|------|---------|
| `qml/src/services/rendergovernor.h/cpp` | FPS governor |
| `qml/resources/images/onboarding_bg.png` | Pre-rendered gradient |

---

## Success Criteria

- [ ] Splash → UI: zero freeze, instant interactivity
- [ ] Idle GPU: <2% when nothing animates
- [ ] Active FPS: locked 60, no drops below 55
- [ ] RAM: <80MB idle, <150MB active with game library
- [ ] Resize: no stutter, native-feel aspect lock
- [ ] Login flow: button responds within 16ms of click
