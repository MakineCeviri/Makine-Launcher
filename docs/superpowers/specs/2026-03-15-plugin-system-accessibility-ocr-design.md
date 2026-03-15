# MakineAI-Launcher — Plugin System, OCR & Accessibility Design

> **Date:** 2026-03-15
> **Status:** Draft
> **Scope:** Plugin architecture, Live translation, TextHook, Accessibility

---

## 1. Overview

MakineAI-Launcher evolves from a package-based translation installer into a **modular game translation platform**. A lightweight plugin system enables three official plugins and community extensions.

**Design goals:**
- Simple plugin loading (DLL + manifest.json)
- Single EXE launcher remains the core
- Plugins are open source, distributed via CDN
- No over-engineering — minimum viable plugin API

---

## 2. Plugin System Architecture

### 2.1 Directory Layout

```
AppData/Local/MakineAI/
├── plugins/
│   ├── live/                    ← MakineAI Live (OCR + Translate + Overlay)
│   │   ├── manifest.json
│   │   ├── makineai-live.dll
│   │   └── models/              ← OCR model files (ONNX)
│   │       ├── det.onnx         ← Text detection (~3MB)
│   │       ├── rec.onnx         ← Text recognition (~5MB)
│   │       ├── cls.onnx         ← Direction classifier (~1MB)
│   │       └── keys.txt         ← Character dictionary
│   ├── accessibility/           ← MakineAI Accessibility
│   │   ├── manifest.json
│   │   └── makineai-a11y.dll
│   └── texthook/                ← MakineAI TextHook
│       ├── manifest.json
│       ├── makineai-texthook.dll
│       └── engines/             ← Engine-specific handlers
│           ├── unity.dll
│           ├── unreal.dll
│           └── rpgmaker.dll
├── plugin-data/                 ← Plugin runtime data
│   ├── live/
│   │   ├── cache/               ← Translation cache
│   │   └── regions/             ← Saved capture regions per game
│   ├── accessibility/
│   │   └── preferences.json     ← User a11y preferences
│   └── texthook/
│       └── hooks.json           ← Saved hook configurations per game
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
  "platforms": ["win64"],
  "minLauncherVersion": "0.2.0",
  "settings": [
    {
      "key": "ocrEngine",
      "type": "select",
      "label": "OCR Engine",
      "options": ["rapidocr", "tesseract", "windows"],
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

### 2.3 Plugin API (C++ Interfaces)

```
core/include/makineai/plugin/
├── iplugin.hpp          ← Base interface (all plugins)
├── iplugin_live.hpp     ← Live translation extensions
├── iplugin_a11y.hpp     ← Accessibility extensions
├── iplugin_hook.hpp     ← Text hooking extensions
├── plugin_manager.hpp   ← Plugin lifecycle management
└── plugin_api.hpp       ← Convenience header
```

#### IPlugin — Base Interface

```cpp
#pragma once
#include <string>
#include <cstdint>

namespace makineai::plugin {

constexpr uint32_t API_VERSION = 1;

struct PluginInfo {
    const char* id;
    const char* name;
    const char* version;
    uint32_t apiVersion;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual bool initialize(const char* dataPath) = 0;
    virtual void shutdown() = 0;
    virtual bool isReady() const = 0;
};

// DLL export macros
using CreatePluginFn = IPlugin* (*)();
using DestroyPluginFn = void (*)(IPlugin*);

} // namespace makineai::plugin

// Each plugin DLL exports:
// extern "C" __declspec(dllexport) makineai::plugin::IPlugin* makineai_create_plugin();
// extern "C" __declspec(dllexport) void makineai_destroy_plugin(makineai::plugin::IPlugin*);
```

#### IPluginLive — Live Translation Interface

```cpp
#pragma once
#include "iplugin.hpp"
#include <functional>
#include <vector>
#include <cstdint>

namespace makineai::plugin {

struct OcrResult {
    std::string text;
    float confidence;
    int32_t x, y, width, height;  // Bounding box
};

struct TranslationResult {
    std::string source;
    std::string translated;
    std::string engine;
};

struct CaptureRegion {
    int32_t x, y, width, height;
};

class IPluginLive : public IPlugin {
public:
    // Screen capture
    virtual bool startCapture(void* windowHandle, CaptureRegion region) = 0;
    virtual void stopCapture() = 0;

    // OCR
    virtual std::vector<OcrResult> recognizeScreen() = 0;
    virtual std::vector<OcrResult> recognizeImage(const uint8_t* data, int w, int h) = 0;

    // Translation
    virtual TranslationResult translate(const std::string& text,
                                         const std::string& from,
                                         const std::string& to) = 0;

    // Overlay
    virtual void showOverlay(const TranslationResult& result, CaptureRegion pos) = 0;
    virtual void hideOverlay() = 0;

    // Pipeline control
    virtual void startPipeline() = 0;  // Capture → OCR → Translate → Display loop
    virtual void stopPipeline() = 0;
    virtual bool isPipelineRunning() const = 0;

    // Callbacks
    using TextCallback = std::function<void(const std::string& text)>;
    virtual void onTextDetected(TextCallback cb) = 0;
    virtual void onTranslated(std::function<void(const TranslationResult&)> cb) = 0;
};

} // namespace makineai::plugin
```

#### IPluginAccessibility — Accessibility Interface

```cpp
#pragma once
#include "iplugin.hpp"

namespace makineai::plugin {

enum class ColorVisionMode {
    Normal,
    Protanopia,
    Deuteranopia,
    Tritanopia,
    HighContrast
};

class IPluginAccessibility : public IPlugin {
public:
    // Color vision
    virtual void setColorVisionMode(ColorVisionMode mode) = 0;
    virtual ColorVisionMode colorVisionMode() const = 0;

    // Screen reader
    virtual void announce(const char* text) = 0;  // Push text to screen reader
    virtual bool isScreenReaderActive() const = 0;

    // TTS
    virtual void speak(const char* text, const char* locale = "tr-TR") = 0;
    virtual void stopSpeaking() = 0;

    // Reduced motion
    virtual bool isReducedMotionEnabled() const = 0;

    // Font scaling
    virtual float fontScale() const = 0;
    virtual void setFontScale(float scale) = 0;
};

} // namespace makineai::plugin
```

#### IPluginHook — Text Hooking Interface

```cpp
#pragma once
#include "iplugin.hpp"
#include <functional>
#include <vector>
#include <cstdint>

namespace makineai::plugin {

struct HookTarget {
    uint64_t address;
    const char* moduleName;
    const char* functionName;
    const char* engineName;  // "Unity", "Unreal", "RPGMaker", etc.
};

struct HookedText {
    std::string text;
    std::string context;    // Function/module where text was captured
    uint64_t address;
    uint64_t timestamp;
};

class IPluginHook : public IPlugin {
public:
    // Process management
    virtual bool attach(uint32_t processId) = 0;
    virtual void detach() = 0;
    virtual bool isAttached() const = 0;

    // Hook discovery
    virtual std::vector<HookTarget> detectHooks() = 0;
    virtual bool activateHook(const HookTarget& target) = 0;
    virtual void deactivateHook(const HookTarget& target) = 0;

    // Text callback
    using TextCallback = std::function<void(const HookedText&)>;
    virtual void onTextReceived(TextCallback cb) = 0;

    // Embedded translation
    virtual bool canEmbed() const = 0;
    virtual bool embedTranslation(const std::string& original,
                                   const std::string& translated) = 0;
};

} // namespace makineai::plugin
```

### 2.4 Plugin Manager

```cpp
// Simplified lifecycle:
// 1. Scan plugins/ directory
// 2. Read each manifest.json, validate apiVersion
// 3. If plugin enabled in settings → LoadLibrary(entry)
// 4. Call makineai_create_plugin() → IPlugin*
// 5. Call plugin->initialize(dataPath)
// 6. On shutdown → plugin->shutdown() → makineai_destroy_plugin() → FreeLibrary
```

Key responsibilities:
- **Discovery:** Scan `plugins/` subdirectories for `manifest.json`
- **Validation:** Check `apiVersion` compatibility, verify DLL signature
- **Lifecycle:** Load → Initialize → Ready → Shutdown → Unload
- **Settings bridge:** Expose plugin settings to QML Settings UI
- **Install/Update:** Download from CDN, verify checksum, extract to plugins/
- **Enable/Disable:** Per-plugin toggle in settings (persisted via QSettings)

### 2.5 QML Integration

Plugin Manager exposes to QML via Q_PROPERTY:

```cpp
class PluginManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool liveAvailable READ liveAvailable NOTIFY pluginsChanged)
    Q_PROPERTY(bool a11yAvailable READ a11yAvailable NOTIFY pluginsChanged)
    Q_PROPERTY(bool hookAvailable READ hookAvailable NOTIFY pluginsChanged)

public:
    Q_INVOKABLE bool installPlugin(const QString& pluginId);
    Q_INVOKABLE bool uninstallPlugin(const QString& pluginId);
    Q_INVOKABLE bool enablePlugin(const QString& pluginId);
    Q_INVOKABLE bool disablePlugin(const QString& pluginId);
    Q_INVOKABLE QVariantMap pluginSettings(const QString& pluginId);
    Q_INVOKABLE void setPluginSetting(const QString& pluginId,
                                       const QString& key,
                                       const QVariant& value);
};
```

---

## 3. Official Plugin: MakineAI Live

Real-time screen OCR + translation + overlay. All-in-one pipeline.

### 3.1 OCR Engine Stack

| Priority | Engine | Use Case | MinGW | Model Size |
|----------|--------|----------|-------|------------|
| Primary | **RapidOcrOnnx** | Game text (scene text) | YES | ~15MB |
| Fallback | **Tesseract 5.x** | Clean dialog text | YES | ~20MB |
| Optional | **Windows OCR** | If MSVC build available | NO | 0 (built-in) |

**RapidOcrOnnx integration:**
- PaddleOCR PP-OCRv3/v4 models converted to ONNX
- ONNX Runtime with DirectML backend (GPU) or CPU fallback
- 3-stage pipeline: Detection → Classification → Recognition
- Turkish character support via custom dictionary (keys.txt)

### 3.2 Screen Capture

| Method | Speed | Compatibility | Priority |
|--------|-------|--------------|----------|
| **DXGI Desktop Duplication** | <5ms | Win8+, same GPU | Primary |
| **Windows.Graphics.Capture** | ~10ms | Win10 1903+ | Secondary |
| **GDI BitBlt** | ~20ms | Universal | Fallback |

Auto-detection: Try DXGI first → WGC → GDI fallback.

### 3.3 Translation Engines (plugin-internal adapters)

Initial set:
- **Google Translate** (free tier, web scraping)
- **DeepL** (API key)
- **ChatGPT/Claude** (API key, context-aware translation)
- **MakineAI** (future: own Turkish-optimized model)
- **Offline** (Argos Translate or CTranslate2)

### 3.4 Overlay

- Transparent Qt Quick window (`Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint`)
- `WDA_EXCLUDEFROMCAPTURE` to prevent OCR feedback loop
- Configurable: position, opacity, font size, background blur
- Subtitle bar mode (bottom of screen) or floating box mode

### 3.5 Pipeline Flow

```
Game Window
    ↓ [Screen Capture: DXGI/GDI]
Captured Frame (GPU texture or bitmap)
    ↓ [Region Crop]
Text Region
    ↓ [Change Detection: pixel hash comparison]
Changed? ──No──→ Skip (reuse cached translation)
    │Yes
    ↓ [OCR: RapidOcrOnnx]
OcrResult { text, confidence, bbox }
    ↓ [Text Dedup & Clean]
Clean Text
    ↓ [Translation: selected engine]
TranslationResult { source, translated }
    ↓ [Overlay Render]
Transparent Window with translated text
```

**Threading model:**
- Capture thread: 30 FPS max, checks for changes
- OCR thread: processes when new frame detected
- Translation thread: async API calls with caching
- Main thread: overlay rendering only

---

## 4. Official Plugin: MakineAI Accessibility

### 4.1 Color Vision Modes

| Mode | Algorithm | Implementation |
|------|-----------|----------------|
| Normal | None | Default theme |
| Protanopia | Viénot 1999 | Adjusted palette |
| Deuteranopia | Viénot 1999 | Adjusted palette |
| Tritanopia | Brettel 1997 | Adjusted palette |
| High Contrast | System detect | Qt 6.10 `contrastPreference` |

**Approach:** Separate Theme palettes per mode + shape-based differentiation.

The plugin sets `Theme.colorVisionMode` property → Theme.qml switches color values.

**Accessible palette (all CVD-safe):**
```
Primary:   #648FFF (Blue)
Secondary: #785EF0 (Purple)
Error:     #DC267F (Magenta)
Warning:   #FE6100 (Orange)
Success:   #FFB000 (Amber)
```

### 4.2 Screen Reader Support

- Add `Accessible.name`, `Accessible.role`, `Accessible.description` to ALL interactive QML elements
- Leverage Qt 6.10 `labelFor`/`labelledBy` for form elements
- Custom `announce()` function for dynamic content changes
- Live region equivalent: focus management on new content

**Priority QML files for a11y retrofit:**
1. NavBar.qml, NavItem.qml (navigation)
2. GameCard.qml (catalog browsing)
3. TranslationActionButton.qml (primary action)
4. SettingsScreen.qml (all settings)
5. GameDetailScreen.qml (game info)
6. All dialogs

### 4.3 TTS (Text-to-Speech)

- Engine: SAPI (MinGW compatible)
- Turkish voice: "Microsoft Tolga" (Win11 built-in)
- Usage: Read game descriptions, installation status, error messages
- QML: `TextToSpeech { engine: "sapi" }` with `locale: Qt.locale("tr-TR")`

### 4.4 Reduced Motion

- Detect via Win32 `SystemParametersInfo(SPI_GETCLIENTAREAANIMATION)`
- User override in plugin settings (force on/off/auto)
- QML: Global `reduceMotion` property → all `Behavior on` blocks check this
- When active: instant property changes, no easing, no parallax

### 4.5 Keyboard Navigation

- All interactive elements: `activeFocusOnTab: true`
- Focus ring: 2px accent outline, 3:1 contrast (FocusRing.qml already exists)
- Arrow keys for grid navigation (GameCard grid)
- Escape to go back, Enter to activate
- Skip-to-content shortcut (Ctrl+M for main content)

### 4.6 Font Scaling

- User-configurable font scale: 0.8x — 2.0x
- Applied via `Dimensions.fontScale` multiplier
- All `font.pixelSize` values use `Dimensions.fontBody * fontScale`
- Min touch target: 24x24px at any scale

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
- `kirikiri.dll` — KiriKiri/KAG engine
- `generic.dll` — Common Win32 text APIs (TextOutW, DrawTextW)

### 5.3 Embedded Translation

For supported engines:
- Replace game text strings in memory at display time
- Font API hooking for encoding compatibility (Turkish characters)
- Synchronize via named events + shared memory

### 5.4 Integration with Live Plugin

TextHook and Live can work together:
- TextHook provides text → Live translates and displays overlay
- If TextHook can embed → direct in-game translation (no overlay needed)
- Fallback: if hook fails → Live switches to OCR mode automatically

---

## 6. Settings UI

### 6.1 New Settings Sections

```
Settings
├── General (existing)
├── Performance (existing)
├── Translation (existing)
├── Plugins                    ← NEW
│   ├── Installed plugins list
│   ├── Enable/Disable toggles
│   ├── Per-plugin settings (from manifest.json "settings")
│   ├── Install from CDN button
│   └── Plugin updates
├── Live Translation           ← NEW (visible when Live plugin active)
│   ├── OCR Engine selection
│   ├── Capture method
│   ├── Translation engine + API keys
│   ├── Overlay appearance
│   └── Saved regions per game
├── Accessibility              ← NEW (visible when A11y plugin active)
│   ├── Color vision mode
│   ├── Font scale slider
│   ├── Reduced motion toggle
│   ├── TTS on/off + voice selection
│   ├── Screen reader hints toggle
│   └── Keyboard shortcuts
└── About (existing)
```

### 6.2 Plugin Store (in-app)

Simple list view showing available plugins from CDN:
- Plugin name, description, version, size
- Install/Update/Uninstall buttons
- "Official" badge for MakineAI plugins
- Future: community plugins with star ratings

---

## 7. Website Integration (makineceviri.net)

### 7.1 Plugin Marketplace Page

New page on makineceviri.net:
- `/plugins` — Plugin listing (official + community)
- `/plugins/{id}` — Plugin detail page
- `/docs/plugin-api` — Developer documentation
- `/docs/plugin-tutorial` — How to build a plugin

### 7.2 CDN Distribution

```
cdn.makineceviri.net/
├── assets/                   (existing)
│   ├── plugins/
│   │   ├── index.json        ← Plugin catalog (id, version, size, checksum)
│   │   ├── live/
│   │   │   ├── manifest.json
│   │   │   └── makineai-live-1.0.0-win64.zip
│   │   ├── accessibility/
│   │   │   └── makineai-a11y-1.0.0-win64.zip
│   │   └── texthook/
│   │       └── makineai-texthook-1.0.0-win64.zip
│   └── models/               ← Shared OCR models
│       ├── rapidocr/
│       │   ├── det.onnx
│       │   ├── rec.onnx
│       │   └── cls.onnx
│       └── tesseract/
│           └── tur.traineddata
└── data/                     (existing)
```

---

## 8. Security

- All plugin DLLs must be signed (code signing certificate)
- CDN downloads verified via SHA-256 checksum in index.json
- Plugin API is sandboxed: plugins cannot access launcher internals beyond API
- Official plugins are open source (GPL-3.0) — auditable
- Community plugins require review before listing on marketplace
- Plugin permissions declared in manifest (e.g., "network", "process", "filesystem")

---

## 9. Dependencies (New)

| Library | Purpose | vcpkg | MinGW |
|---------|---------|-------|-------|
| ONNX Runtime | OCR inference (RapidOcrOnnx) | onnxruntime | YES |
| OpenCV (minimal) | Image preprocessing | opencv4 | YES |
| Tesseract 5.x | Fallback OCR | tesseract | YES |
| MinHook | Inline function hooking | — (vendored) | YES |
| DXGI (system) | Screen capture | — (Win SDK) | YES (headers) |

---

## 10. Phasing

| Phase | Deliverable | Dependencies |
|-------|------------|-------------|
| **Phase 1** | Plugin system core (Manager, API, manifest, settings UI) | None |
| **Phase 2** | MakineAI Live (capture + OCR + overlay, single translate engine) | Phase 1 |
| **Phase 3** | MakineAI Accessibility (color vision, screen reader, TTS, reduced motion) | Phase 1 |
| **Phase 4** | MakineAI TextHook (MinHook, generic handler, Unity handler) | Phase 1 |
| **Phase 5** | Multiple translation engines, plugin store, website marketplace | Phase 2 |
| **Phase 6** | Embedded translation, emulator support, community plugins | Phase 4 |
