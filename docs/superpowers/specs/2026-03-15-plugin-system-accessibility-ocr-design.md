# MakineAI-Launcher — Plugin System, OCR & Accessibility Design

> **Date:** 2026-03-15
> **Status:** Reviewed & Revised (v2)
> **Scope:** Plugin architecture, Live translation, TextHook, Accessibility
> **Review:** All 4 CRITICAL, 9 IMPORTANT issues addressed. See review doc for originals.

---

## 1. Overview

MakineAI-Launcher evolves from a package-based translation installer into a **modular game translation platform**. A lightweight plugin system enables official plugins and community extensions.

**Design goals:**
- Simple plugin loading (DLL + manifest.json)
- Single EXE launcher remains the core
- **C ABI boundary** for cross-compiler plugin compatibility
- Plugins are open source, distributed via CDN
- **Core accessibility built-in** — not gated behind a plugin
- No over-engineering — minimum viable plugin API

**Architecture change from v1:** Accessibility features (screen reader, reduced motion, font scaling, high contrast) are now **built into the launcher core**, not a plugin. Only optional extras (TTS voice packs, advanced color vision simulation overlays) remain as plugin territory.

---

## 2. Plugin System Architecture

### 2.1 Directory Layout

```
AppData/Local/MakineAI/
├── plugins/
│   ├── live/                    ← MakineAI Live (OCR + Translate + Overlay)
│   │   ├── manifest.json
│   │   ├── makineai-live.dll
│   │   └── models/              ← OCR models (lazy-downloaded from CDN)
│   ├── texthook/                ← MakineAI TextHook
│   │   ├── manifest.json
│   │   ├── makineai-texthook.dll
│   │   └── engines/             ← Engine-specific handlers
│   │       ├── unity.dll
│   │       ├── unreal.dll
│   │       └── rpgmaker.dll
│   └── {community-plugin}/      ← 3rd party plugins
│       ├── manifest.json
│       └── plugin.dll
├── plugin-data/                 ← Plugin runtime data (persists across updates)
│   ├── live/
│   │   ├── cache/               ← Translation cache
│   │   └── regions/             ← Saved capture regions per game
│   └── texthook/
│       └── hooks.json           ← Saved hook configs per game
└── ... (existing: logs, cache, data, backups, packages)
```

### 2.2 Plugin Manifest (manifest.json)

```json
{
  "id": "com.makineceviri.live",
  "name": "MakineAI Live",
  "version": "1.0.0",
  "apiVersion": 1,
  "category": "translation",
  "description": "Real-time game screen OCR and translation overlay",
  "author": "MakineAI Team",
  "license": "GPL-3.0",
  "homepage": "https://makineceviri.net/plugins/live",
  "entry": "makineai-live.dll",
  "dependencies": [],
  "capabilities": ["network", "screen_capture", "overlay"],
  "platforms": ["win64"],
  "minLauncherVersion": "0.2.0",
  "maxLauncherVersion": null,
  "settings": [
    {
      "key": "ocrEngine",
      "type": "select",
      "label": "OCR Engine",
      "options": ["rapidocr", "tesseract"],
      "default": "rapidocr"
    },
    {
      "key": "captureMethod",
      "type": "select",
      "label": "Capture Method",
      "options": ["dxgi", "gdi", "auto"],
      "default": "auto"
    },
    {
      "key": "overlayOpacity",
      "type": "slider",
      "label": "Overlay Opacity",
      "min": 0.1,
      "max": 1.0,
      "default": 0.9
    }
  ]
}
```

**Manifest fields:**
- `capabilities` — Declared capabilities for audit/review (NOT runtime-enforced — DLLs run in-process). Marketplace reviewers verify plugins only use declared capabilities.
- `maxLauncherVersion` — null means no upper limit. Set when API v2 drops v1 support.
- `dependencies` — Plugin IDs. Manager loads dependencies first. Missing dependency = plugin disabled with user notification.

### 2.3 Plugin API (C ABI)

All plugin interfaces use **flat C functions with POD structs** for cross-compiler ABI safety. A C++ convenience wrapper header is provided on top.

```
core/include/makineai/plugin/
├── plugin_api.h         ← C ABI: exported functions, POD structs, error codes
├── plugin_api.hpp       ← C++ wrapper (header-only, optional convenience)
├── plugin_manager.hpp   ← Plugin lifecycle management (internal)
└── plugin_types.h       ← Shared type definitions
```

#### C ABI Interface (plugin_api.h)

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error Handling ── */

typedef enum {
    MAKINEAI_OK = 0,
    MAKINEAI_ERR_INIT_FAILED,
    MAKINEAI_ERR_NOT_READY,
    MAKINEAI_ERR_INVALID_PARAM,
    MAKINEAI_ERR_NOT_FOUND,
    MAKINEAI_ERR_ACCESS_DENIED,
    MAKINEAI_ERR_UNSUPPORTED,
    MAKINEAI_ERR_TIMEOUT,
    MAKINEAI_ERR_ENGINE_ERROR,
    MAKINEAI_ERR_CAPTURE_FAILED,
    MAKINEAI_ERR_OCR_FAILED,
    MAKINEAI_ERR_TRANSLATE_FAILED,
    MAKINEAI_ERR_HOOK_FAILED,
    MAKINEAI_ERR_ANTICHEAT_DETECTED,
} MakineAiError;

/* ── Plugin Info ── */

typedef struct {
    const char* id;
    const char* name;
    const char* version;
    uint32_t apiVersion;
} MakineAiPluginInfo;

/* ── Base Plugin Functions (every plugin exports these) ── */

typedef MakineAiPluginInfo (*MakineAiFn_GetInfo)(void);
typedef MakineAiError      (*MakineAiFn_Initialize)(const char* dataPath);
typedef void               (*MakineAiFn_Shutdown)(void);
typedef bool               (*MakineAiFn_IsReady)(void);
typedef const char*        (*MakineAiFn_GetLastError)(void);

/* ── OCR Result ── */

typedef struct {
    const char* text;
    float confidence;
    int32_t x, y, width, height;
} MakineAiOcrResult;

typedef struct {
    MakineAiOcrResult* items;
    uint32_t count;
} MakineAiOcrResultList;

/* ── Translation Result ── */

typedef struct {
    const char* source;
    const char* translated;
    const char* engine;
} MakineAiTranslation;

/* ── Capture Region ── */

typedef struct {
    int32_t x, y, width, height;
} MakineAiRegion;

/* ── Callbacks (C function pointers + user data) ── */

typedef void (*MakineAiFn_OnText)(const char* text, void* userData);
typedef void (*MakineAiFn_OnTranslation)(const MakineAiTranslation* result, void* userData);

/* ── Live Plugin Functions ── */

typedef MakineAiError      (*MakineAiFn_StartCapture)(void* windowHandle, MakineAiRegion region);
typedef void               (*MakineAiFn_StopCapture)(void);
typedef MakineAiOcrResultList (*MakineAiFn_RecognizeScreen)(void);
typedef MakineAiTranslation (*MakineAiFn_Translate)(const char* text, const char* from, const char* to);
typedef void               (*MakineAiFn_ShowOverlay)(const MakineAiTranslation* result, MakineAiRegion pos);
typedef void               (*MakineAiFn_HideOverlay)(void);
typedef MakineAiError      (*MakineAiFn_StartPipeline)(void);
typedef void               (*MakineAiFn_StopPipeline)(void);
typedef bool               (*MakineAiFn_IsPipelineRunning)(void);
typedef void               (*MakineAiFn_SetOnText)(MakineAiFn_OnText cb, void* userData);
typedef void               (*MakineAiFn_SetOnTranslation)(MakineAiFn_OnTranslation cb, void* userData);
typedef void               (*MakineAiFn_FreeOcrResults)(MakineAiOcrResultList* results);

/* ── Hook Plugin Functions ── */

typedef struct {
    uint64_t address;
    const char* moduleName;
    const char* functionName;
    const char* engineName;
} MakineAiHookTarget;

typedef struct {
    const char* text;
    const char* context;
    uint64_t address;
    uint64_t timestamp;
} MakineAiHookedText;

typedef void (*MakineAiFn_OnHookedText)(const MakineAiHookedText* text, void* userData);

typedef MakineAiError      (*MakineAiFn_Attach)(uint32_t processId);
typedef void               (*MakineAiFn_Detach)(void);
typedef bool               (*MakineAiFn_IsAttached)(void);
typedef MakineAiHookTarget* (*MakineAiFn_DetectHooks)(uint32_t* outCount);
typedef MakineAiError      (*MakineAiFn_ActivateHook)(const MakineAiHookTarget* target);
typedef void               (*MakineAiFn_DeactivateHook)(const MakineAiHookTarget* target);
typedef void               (*MakineAiFn_SetOnHookedText)(MakineAiFn_OnHookedText cb, void* userData);
typedef bool               (*MakineAiFn_CanEmbed)(void);
typedef MakineAiError      (*MakineAiFn_EmbedTranslation)(const char* original, const char* translated);
typedef void               (*MakineAiFn_FreeHookTargets)(MakineAiHookTarget* targets);

#ifdef __cplusplus
}
#endif

/*
 * Each plugin DLL exports these symbols:
 *
 * Required (all plugins):
 *   makineai_get_info        → MakineAiFn_GetInfo
 *   makineai_initialize      → MakineAiFn_Initialize
 *   makineai_shutdown        → MakineAiFn_Shutdown
 *   makineai_is_ready        → MakineAiFn_IsReady
 *   makineai_get_last_error  → MakineAiFn_GetLastError
 *
 * Live plugin additionally:
 *   makineai_start_capture, makineai_stop_capture,
 *   makineai_recognize_screen, makineai_free_ocr_results,
 *   makineai_translate, makineai_show_overlay, makineai_hide_overlay,
 *   makineai_start_pipeline, makineai_stop_pipeline,
 *   makineai_is_pipeline_running,
 *   makineai_set_on_text, makineai_set_on_translation
 *
 * Hook plugin additionally:
 *   makineai_attach, makineai_detach, makineai_is_attached,
 *   makineai_detect_hooks, makineai_free_hook_targets,
 *   makineai_activate_hook, makineai_deactivate_hook,
 *   makineai_set_on_hooked_text,
 *   makineai_can_embed, makineai_embed_translation
 */
```

**Key design decisions:**
- All strings are `const char*` (UTF-8) — no std::string across DLL boundaries
- All callbacks are C function pointers with `void* userData` — no std::function
- Plugin owns memory it returns; host calls `makineai_free_*()` to release
- Every fallible function returns `MakineAiError`; details via `makineai_get_last_error()`

### 2.4 API Versioning Policy

- **apiVersion 1** — Initial stable API. Frozen once released.
- **New versions are additive** — v2 adds new exported functions, never removes v1 functions.
- **Manager loads plugins if `plugin.apiVersion <= launcher.apiVersion`**
- **Deprecation:** Functions marked deprecated in v(N), removed earliest in v(N+2). Minimum 2 version deprecation window.
- **maxLauncherVersion:** If a launcher release drops an old API version, plugins declaring that version must set `maxLauncherVersion` to the last compatible launcher.

### 2.5 Plugin Manager

```cpp
// Lifecycle:
// 1. Scan plugins/ directory for manifest.json files
// 2. Validate apiVersion, minLauncherVersion, maxLauncherVersion
// 3. Resolve dependencies (topological sort), disable if missing
// 4. If enabled in settings → LoadLibrary(entry DLL)
// 5. Resolve required symbols via GetProcAddress
// 6. Call makineai_initialize(dataPath)
// 7. On shutdown → makineai_shutdown() → FreeLibrary()
// Note: Enable/disable requires launcher restart (DLL unloading is unsafe with running threads)
```

Key responsibilities:
- **Discovery:** Scan `plugins/` for `manifest.json`
- **Validation:** API version, launcher version range, DLL code signature
- **Dependency resolution:** Topological load order, graceful disable on missing deps
- **Lifecycle:** Load → Init → Ready → Shutdown → Unload (restart required for changes)
- **Settings bridge:** Expose plugin settings to QML via PluginManager Q_PROPERTYs
- **Install/Update:** Download from CDN, verify SHA-256 checksum, extract to plugins/
- **Uninstall:** Remove plugin dir. Prompt user: "Also remove plugin data?" for plugin-data/ cleanup.
- **Error reporting:** Aggregate plugin errors, expose to QML for user notification

### 2.6 Threading Contract

- **All callbacks are invoked on the Qt main thread** via `QMetaObject::invokeMethod(Qt::QueuedConnection)`
- Plugin internal threads are the plugin's responsibility
- Plugin must not call Qt/QML APIs directly — only communicate via registered callbacks
- Host serializes all calls to a single plugin (no concurrent calls to the same plugin)

### 2.7 QML Integration

```cpp
class PluginManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool liveAvailable READ liveAvailable NOTIFY pluginsChanged)
    Q_PROPERTY(bool hookAvailable READ hookAvailable NOTIFY pluginsChanged)

public:
    Q_INVOKABLE bool installPlugin(const QString& pluginId);
    Q_INVOKABLE bool uninstallPlugin(const QString& pluginId, bool removeData = false);
    Q_INVOKABLE bool enablePlugin(const QString& pluginId);   // takes effect after restart
    Q_INVOKABLE bool disablePlugin(const QString& pluginId);  // takes effect after restart
    Q_INVOKABLE QVariantMap pluginSettings(const QString& pluginId);
    Q_INVOKABLE void setPluginSetting(const QString& pluginId,
                                       const QString& key,
                                       const QVariant& value);
    Q_INVOKABLE QString pluginError(const QString& pluginId);
};
```

---

## 3. Official Plugin: MakineAI Live

Real-time screen OCR + translation + overlay. All-in-one pipeline.

### 3.1 OCR Engine Stack

| Priority | Engine | Use Case | MinGW | Model Size |
|----------|--------|----------|-------|------------|
| Primary | **RapidOcrOnnx** | Game text (scene text) | YES* | ~15MB |
| Fallback | **Tesseract 5.x** | Clean dialog text | YES | ~20MB |

*RapidOcrOnnx uses ONNX Runtime via **C API** (`onnxruntime_c_api.h`). Prebuilt MSVC binaries of ONNX Runtime are shipped as DLLs alongside the plugin — the C API works across compiler boundaries (no C++ ABI dependency). DirectML GPU acceleration available through the prebuilt DLL.

**OCR model lazy download:**
- Models are NOT bundled in the plugin ZIP (keeps install small)
- On first use, models downloaded from `cdn.makineceviri.net/assets/models/rapidocr/`
- Progress shown in launcher UI
- Models cached in `plugins/live/models/`
- Model updates independent of plugin updates

### 3.2 Screen Capture

| Method | Speed | Compatibility | Priority |
|--------|-------|--------------|----------|
| **DXGI Desktop Duplication** | <5ms | Win8+, same GPU | Primary |
| **GDI BitBlt** | ~20ms | Universal | Fallback |

Auto-detection: DXGI first → GDI fallback. Windows.Graphics.Capture dropped (requires WinRT/MSVC).

### 3.3 Translation Engines (plugin-internal adapters)

| Engine | Type | Notes |
|--------|------|-------|
| **DeepL** | API (key required) | Best quality for European languages |
| **Google Cloud Translation** | API (key required) | Official API, not web scraping |
| **ChatGPT / Claude** | API (key required) | Context-aware, best for narrative text |
| **LibreTranslate** | Self-hosted / free | Offline-capable, open source |
| **MakineAI** | Future | Own Turkish-optimized model |

Note: No web scraping of free translation services — all engines use official APIs to avoid ToS violations and reliability issues.

### 3.4 Overlay

- Transparent Qt Quick window (`Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint`)
- `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` to prevent OCR feedback loop (Win10 2004+)
- Configurable: position, opacity, font size, background style
- Two modes: subtitle bar (bottom) or floating box (near text region)

### 3.5 Pipeline Flow

```
Game Window
    ↓ [Screen Capture: DXGI/GDI]
Captured Frame
    ↓ [Region Crop — user-defined area]
Text Region
    ↓ [Change Detection — pixel hash comparison]
Changed? ──No──→ Skip (reuse cached translation)
    │Yes
    ↓ [OCR: RapidOcrOnnx → Tesseract fallback]
OcrResult { text, confidence, bbox }
    ↓ [Text Dedup & Clean]
Clean Text
    ↓ [Cache Lookup — skip translation if cached]
    ↓ [Translation: selected engine]
TranslationResult { source, translated }
    ↓ [Overlay Render]
Transparent Window
```

**Threading model:**
- Capture thread: polls at configurable rate (default 2 FPS, max 30 FPS)
- OCR thread: processes when change detected
- Translation thread: async API calls
- Main thread: overlay rendering + all plugin callbacks
- Total pipeline latency: ~300-800ms (acceptable for dialog text)

### 3.6 TextHook Integration

When both Live and TextHook plugins are active:
- TextHook provides extracted text → Live translates and displays overlay
- If TextHook has text, OCR is bypassed (hook text is more accurate)
- If hook fails for a game → Live falls back to OCR automatically
- Communication via Plugin Manager event bridge: TextHook calls its text callback → Manager routes to Live's translate function

---

## 4. Core Accessibility (Built-in, NOT a Plugin)

These features are built into the launcher core because they are fundamental to usability and must work without any plugin installation.

### 4.1 Screen Reader Support

Retrofit `Accessible.*` properties on all interactive QML elements.

**Existing coverage:** 22 QML files already have `Accessible.*` annotations (71 occurrences). Focus on gaps:

**Priority gaps to fill:**
1. GameCard.qml — catalog card (most used interactive element)
2. TranslationActionButton.qml — primary CTA
3. All dialogs — install confirmation, error, progress
4. GameDetailScreen.qml sections — hero, info tiles, contributors
5. CatalogSection.qml — search, category filters

**Implementation pattern:**
```qml
// Every interactive element gets at minimum:
Accessible.role: Accessible.Button
Accessible.name: qsTr("Install Turkish Translation")
Accessible.focusable: true
activeFocusOnTab: true
Keys.onReturnPressed: clicked()
Keys.onSpacePressed: clicked()
```

**Qt 6.10 features to leverage:**
- `Accessible.labelFor` / `Accessible.labelledBy` for form relationships
- `QAccessibilityHints::contrastPreference` for high contrast detection

### 4.2 Color Vision Modes

Integrated into Theme.qml via a new `colorVisionMode` property:

```qml
// Theme.qml additions
property string colorVisionMode: SettingsManager.colorVisionMode  // "normal", "protanopia", "deuteranopia", "tritanopia"

// Colors switch based on mode — accessible palette for CVD modes
property color statusSuccess: colorVisionMode === "normal" ? "#22c55e" : "#FFB000"
property color statusError:   colorVisionMode === "normal" ? "#ef4444" : "#DC267F"
property color statusWarning: colorVisionMode === "normal" ? "#f59e0b" : "#FE6100"
```

**CVD-safe universal palette (IBM Design):**
- Primary: `#648FFF` (Blue)
- Secondary: `#785EF0` (Purple)
- Error: `#DC267F` (Magenta)
- Warning: `#FE6100` (Orange)
- Success: `#FFB000` (Amber)

**Beyond color:** All status indicators use shape + icon + text in addition to color. Never rely on color alone.

### 4.3 Reduced Motion

New C++ helper using Win32 API:

```cpp
// SettingsManager additions
Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY reduceMotionChanged)

bool reduceMotion() const {
    if (m_reduceMotionOverride.has_value()) return *m_reduceMotionOverride;
    // Auto-detect from Windows settings
    BOOL enabled = TRUE;
    SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    return !enabled;
}
```

**QML usage:**
```qml
Behavior on opacity {
    enabled: !SettingsManager.reduceMotion
    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
}
```

User override in Settings: Auto (follow OS) / Always On / Always Off.

### 4.4 Font Scaling

New computed properties in Dimensions.qml:

```qml
// Dimensions.qml additions
property real fontScale: SettingsManager.fontScale  // 0.8 — 2.0, default 1.0

// Scaled variants (use these instead of raw constants)
readonly property int fontBodyScaled: Math.round(fontBody * fontScale)
readonly property int fontSmallScaled: Math.round(fontSmall * fontScale)
readonly property int fontH1Scaled: Math.round(fontH1 * fontScale)
// ... etc for all font sizes
```

Backward compatible: existing `fontBody` stays as raw constant, new `fontBodyScaled` added alongside. Migration is incremental — update QML files to use scaled variants over time.

**Minimum touch target:** 24x24px at any scale (WCAG 2.2 Level AA).

### 4.5 Keyboard Navigation

- All interactive elements: `activeFocusOnTab: true`
- FocusRing.qml (already exists) — 2px accent outline, 3:1+ contrast
- Arrow keys for grid navigation (GameCard grid in catalog)
- Escape = go back, Enter = activate, Space = toggle
- Tab order follows visual layout (managed via QML item order in layouts)

### 4.6 High Contrast Mode

Leverage Qt 6.10 built-in detection:
```qml
property bool highContrast: Application.styleHints.contrastPreference !== 0

Rectangle {
    border.width: highContrast ? 2 : 1
    border.color: highContrast ? Theme.textPrimary : Theme.border
}
```

### 4.7 Settings UI for Accessibility

New section in Settings (always available, no plugin required):
- Color vision mode dropdown (Normal / Protanopia / Deuteranopia / Tritanopia)
- Font scale slider (0.8x — 2.0x)
- Reduced motion toggle (Auto / On / Off)
- High contrast (Auto-detect / Force)
- Screen reader hints toggle

---

## 5. Official Plugin: MakineAI TextHook

### 5.1 Hooking Methods

| Method | Use Case | Mechanism |
|--------|----------|-----------|
| **MinHook** (inline) | Primary, most games | Trampoline-based function hooking |
| **VEH** (breakpoint) | 64-bit Unity/UE games | INT3 + single-step exception handler |
| **Memory Read** | Simple engines, emulators | Polling known memory addresses |

### 5.2 Engine Handlers

Separate DLLs per engine family, loaded on demand:
- `unity.dll` — Unity (Mono/.NET) text functions
- `unreal.dll` — Unreal Engine FText/FString
- `rpgmaker.dll` — RPG Maker MV/MZ (JavaScript bridge)
- `renpy.dll` — Ren'Py (Python string intercept)
- `generic.dll` — Common Win32 text APIs (TextOutW, DrawTextW)

### 5.3 Embedded Translation

For supported engines:
- Replace game text strings in memory at display time
- Font API hooking for encoding compatibility (Turkish characters)
- Synchronize via named events + shared memory

### 5.4 Security & Anti-Cheat Considerations

**Antivirus:**
- Plugin DLLs are code-signed to reduce false positives
- User documentation: how to add Windows Defender exclusion
- Known false positive patterns documented in FAQ

**Anti-cheat compatibility:**
- TextHook will NOT work with games using: EAC (Easy Anti-Cheat), BattlEye, Vanguard, nProtect GameGuard
- Launcher shows clear warning before attaching to any game
- Known safe/unsafe game list maintained in plugin data
- User explicitly confirms "I understand this may trigger anti-cheat" before first attach

**UAC / Elevation:**
- Cross-process hooking requires Administrator privileges
- Launcher prompts for elevation only when TextHook attach is requested
- No admin required for normal launcher operation or Live plugin (OCR is screen-level, not process-level)

### 5.5 RuntimeManager Deprecation

`core/include/makineai/runtime_manager.hpp` (current stub) will be removed. The TextHook plugin replaces the planned runtime system entirely. RuntimeManager stub deleted when TextHook reaches feature parity.

---

## 6. Settings UI Changes

### 6.1 New Settings Sections

```
Settings
├── General (existing)
├── Performance (existing)
├── Translation (existing)
├── Accessibility              ← NEW (always available, core feature)
│   ├── Color vision mode
│   ├── Font scale slider
│   ├── Reduced motion toggle
│   ├── High contrast (auto/force)
│   └── Screen reader hints toggle
├── Plugins                    ← NEW
│   ├── Installed plugins list
│   ├── Enable/Disable toggles (restart required)
│   ├── Per-plugin settings (from manifest "settings")
│   ├── Install from CDN button
│   ├── Plugin updates
│   └── Uninstall (with "remove data" option)
├── Live Translation           ← NEW (visible when Live plugin active)
│   ├── OCR Engine selection
│   ├── Capture method
│   ├── Translation engine + API keys
│   ├── Overlay appearance (opacity, position, font)
│   └── Saved regions per game
└── About (existing)
```

### 6.2 Plugin Store (in-app)

Simple list showing available plugins from CDN:
- Plugin name, description, version, download size
- Install / Update / Uninstall buttons
- "Official" badge for MakineAI plugins
- Community plugins: open source link, review status

---

## 7. Website Integration (makineceviri.net)

### 7.1 Plugin Pages

- `/plugins` — Plugin listing (official + community)
- `/plugins/{id}` — Plugin detail page (description, screenshots, changelog)
- `/docs/plugin-api` — C ABI reference documentation
- `/docs/plugin-tutorial` — How to build a community plugin

### 7.2 CDN Distribution

```
cdn.makineceviri.net/
├── assets/
│   ├── plugins/
│   │   ├── index.json            ← Plugin catalog (id, version, size, sha256)
│   │   ├── live/
│   │   │   └── makineai-live-1.0.0-win64.zip
│   │   └── texthook/
│   │       └── makineai-texthook-1.0.0-win64.zip
│   └── models/                   ← Shared OCR models (lazy download)
│       ├── rapidocr/
│       │   ├── det.onnx
│       │   ├── rec.onnx
│       │   └── cls.onnx
│       └── tesseract/
│           └── tur.traineddata
└── data/                         (existing: .makine packages)
```

---

## 8. Security Model

**Honest security posture:** Plugins are DLLs loaded in-process. They have full access to the launcher's memory and OS APIs. There is NO runtime sandboxing.

**Mitigation layers:**
1. **Code signing** — All official plugin DLLs are signed. Community plugins recommended to sign.
2. **CDN integrity** — SHA-256 checksums in `index.json`, verified on download.
3. **Open source** — Official plugins are GPL-3.0, source auditable on GitHub.
4. **Marketplace review** — Community plugins require manual review before listing.
5. **Declared capabilities** — Manifest `capabilities` field is an audit tool for reviewers, not runtime enforcement.
6. **User consent** — Each plugin install shows what capabilities the plugin declares.

---

## 9. New Dependencies

| Library | Purpose | Integration | MinGW |
|---------|---------|-------------|-------|
| ONNX Runtime | OCR inference | Prebuilt MSVC DLL, C API | YES (C API) |
| OpenCV (minimal) | Image preprocessing | vcpkg | YES |
| Tesseract 5.x | Fallback OCR | vcpkg | YES |
| MinHook | Inline function hooking | Vendored source | YES |
| DXGI headers | Screen capture | Windows SDK | YES |

---

## 10. Phasing

| Phase | Deliverable | Scope |
|-------|------------|-------|
| **Phase 1** | Plugin System Core | PluginManager, C ABI, manifest loading, settings UI, CDN install |
| **Phase 2** | Core Accessibility | Screen reader attrs, reduced motion, font scaling, color vision, keyboard nav |
| **Phase 3** | MakineAI Live v1 | Screen capture + RapidOcrOnnx + single translator + overlay |
| **Phase 4** | MakineAI TextHook v1 | MinHook, generic handler, Unity handler, Live integration |
| **Phase 5** | Polish & Expand | Multiple translators, plugin store UI, website marketplace |
| **Phase 6** | Advanced | Embedded translation, emulator hooks, community plugin ecosystem |

Note: Phase 2 (Accessibility) moved up — it's core functionality, not plugin-dependent.
