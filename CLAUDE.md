# MakineAI - Claude Context

## Project Overview

**MakineAI** is a Turkish game translation launcher for Windows. Built with Qt6/QML UI and native C++ core for high performance.

---

## Current Version: 0.1.0-alpha

```
Architecture: Native (Qt6 UI + C++ Core)
Core: C++20/23, CMake, vcpkg
UI: Qt6/QML
Target: Windows x64
Status: Active Development
```

---

## Project Health (2026-01-23)

```
+===============================================+
|  Component           Score    Status          |
+===============================================+
|  Architecture:       8/10     Solid           |
|  UI/UX:              9/10     Production-ready|
|  Core Logic:         7/10     Good            |
|  Build System:       10/10    Professional    |
|  Test Coverage:      2/10     Ready to enable |
|  DevOps/CI:          7/10     Workflows ready |
|  Documentation:      5/10     Improved        |
+===============================================+
|  OVERALL:            ~69%     Alpha Stage     |
|  TARGET (MVP):       75%                      |
|  PREVIOUS:           ~65%     (+4%)           |
+===============================================+
```

### Recent Improvements
- GPU optimization (animation visibility binding)
- PerformanceMonitor component (F3 toggle)
- 22 vcpkg dependencies (from 10)
- Optional library infrastructure with feature detection
- Comprehensive library integration plan
- **AssetParser factory** - 4 built-in parsers now active
- **Handler factory** - 5 engine handlers registered
- **CMakePresets.json** - Modern CMake configuration
- **justfile** - Task automation (just setup, just all, just test)
- **Professional .gitignore** - 240 lines, well-organized
- **Deleted legacy scripts** - .bat/.ps1 files removed

---

## Directory Structure

```
C:\cedra\MakineAI\
+-- CMakePresets.json           # Modern CMake configuration
+-- justfile                    # Task automation (just <recipe>)
+-- vcpkg.json                  # Dependency manifest (22 libs)
+-- .gitignore                  # Comprehensive ignore rules
+-- core/                       # C++ Core Library
|   +-- CMakeLists.txt
|   +-- TODO_LIBRARY_INTEGRATION.md  # Library integration plan
|   +-- include/makineai/       # Public headers (17 files)
|   |   +-- core.hpp            # Main singleton
|   |   +-- types.hpp           # Common types
|   |   +-- error.hpp           # Error handling
|   |   +-- features.hpp        # Compile-time feature detection (NEW)
|   |   +-- database.hpp        # SQLite wrapper
|   |   +-- game_detector.hpp   # Game scanning
|   |   +-- asset_parser.hpp    # Asset parsing
|   |   +-- patch_engine.hpp    # Patching logic
|   |   +-- package_manager.hpp # Package downloads
|   |   +-- runtime_manager.hpp # BepInEx/XUnity
|   |   +-- security.hpp        # Crypto/signing
|   |   +-- version_tracker.hpp # Update detection
|   |   +-- translation_memory.hpp
|   |   +-- glossary_service.hpp
|   |   +-- qa_service.hpp
|   |   +-- handlers/           # Engine-specific handlers
|   +-- src/                    # Implementation (13 modules)
|   |   +-- core.cpp
|   |   +-- database/
|   |   +-- game_detector/      # Steam, Epic, GOG scanners
|   |   +-- asset_parser/       # Unity, Unreal, Bethesda, GameMaker
|   |   +-- patch_engine/
|   |   +-- package_manager/
|   |   +-- runtime/
|   |   +-- security/
|   |   +-- version_tracker/
|   |   +-- translation/
|   |   +-- handlers/           # Engine handlers
|   |   +-- platform/windows/   # Windows-specific
|   +-- tests/                  # Unit tests (6 files, disabled)
|
+-- qml/                        # Qt QML Application
|   +-- CMakeLists.txt
|   +-- src/
|   |   +-- main.cpp            # Entry point
|   |   +-- services/           # Backend services (6)
|   |   +-- models/             # Qt models (2)
|   |   +-- makineai_metatypes.h
|   +-- qml/                    # QML files
|   |   +-- Main.qml            # Main window + F3 perf monitor
|   |   +-- HomeScreen.qml      # Home page
|   |   +-- SettingsScreen.qml  # Settings
|   |   +-- SplashScreen.qml    # Splash
|   |   +-- SplashWindow.qml    # Splash window
|   |   +-- GameDetailScreen.qml
|   |   +-- TranslationWorkflowScreen.qml
|   |   +-- theme/              # Theme singletons
|   |   +-- components/         # 22+ reusable components
|   |   |   +-- PerformanceMonitor.qml  # FPS overlay (NEW)
|   |   +-- dialogs/            # Dialog components
|   +-- resources/
|       +-- icons/
|
+-- ui/                         # Qt C++ UI Layer (legacy)
+-- assets/                     # Shared assets
+-- docs/                       # Documentation
+-- CLAUDE.md                   # This file
```

---

## Build Commands

### Using justfile (Recommended)
```bash
cd C:\cedra\MakineAI

# Install dependencies
just setup

# Build everything (release)
just all

# Build core only
just core

# Build QML app only
just qml

# Run tests
just test

# Run the app
just run

# Deploy with Qt dependencies
just deploy
```

### Using CMake Presets
```bash
cd C:\cedra\MakineAI

# Core library
cmake --preset core-release
cmake --build --preset core-release

# QML application
cmake --preset qml-release
cmake --build --preset qml-release

# Debug builds
cmake --preset core-debug
cmake --build --preset core-debug
```

### Visual Studio
```bash
# Generate VS2022 solution
cmake --preset vs2022-qml
start build/vs2022-qml/MakineAI.sln
```

### Environment Variables Required
```powershell
setx VCPKG_ROOT "C:\vcpkg"
setx Qt6_DIR "C:\Qt\6.8.1\msvc2022_64"
```

---

## Core Modules Status

| Module | Header | Impl | Status | Priority |
|--------|--------|------|--------|----------|
| Core | core.hpp | core.cpp | Framework | - |
| Types | types.hpp | - | Done | - |
| Error | error.hpp | - | Done | - |
| Features | features.hpp | - | **NEW** | - |
| Database | database.hpp | database.cpp | Needs verify | Medium |
| GameDetector | game_detector.hpp | 4 files | **Taskflow ready** | **HIGH** |
| AssetParser | asset_parser.hpp | 4 files | **Parsers commented** | **CRITICAL** |
| PatchEngine | patch_engine.hpp | 1 file | **Incomplete** | **HIGH** |
| PackageManager | package_manager.hpp | 1 file | Needs verify | Medium |
| RuntimeManager | runtime_manager.hpp | 1 file | Needs verify | Medium |
| Security | security.hpp | 1 file | **libsodium ready** | Low |
| VersionTracker | version_tracker.hpp | 1 file | Needs verify | Low |
| TranslationMemory | translation_memory.hpp | 1 file | **simdjson ready** | Medium |
| GlossaryService | glossary_service.hpp | 1 file | Needs verify | Medium |
| QAService | qa_service.hpp | 1 file | Needs verify | Low |

### Engine Handlers Status

| Handler | File | Status | Market Share |
|---------|------|--------|--------------|
| Unity | unity_handler.cpp | **TODO: Verify** | 45-50% |
| Unreal | unreal_handler.cpp | **TODO: Verify** | 15-20% |
| RPG Maker | rpgmaker_handler.cpp | **TODO: Verify** | 15-20% |
| Ren'Py | renpy_handler.cpp | Works (from 0.0.1-alpha) | 3-5% |
| GameMaker | gamemaker_handler.cpp | **TODO: Verify** | 2-3% |

---

## QML UI Status

### Screens (7)
| Screen | File | Status |
|--------|------|--------|
| Main | Main.qml | Done + PerfMon |
| Home | HomeScreen.qml | Done + GPU opt |
| Settings | SettingsScreen.qml | Done |
| Splash | SplashScreen.qml | Done |
| SplashWindow | SplashWindow.qml | Done |
| GameDetail | GameDetailScreen.qml | Done |
| TranslationWorkflow | TranslationWorkflowScreen.qml | In Progress |

### Components (22+)
```
GlassCard, AnimatedButton, GradientButton, MakineLogo,
RotatingHourglass, TranslationPhaseBadge, WaitingForGameCard,
GameDetectedCard, AnnouncementCard, DiscordButton, CedraCard,
BackgroundBlur, MinimalTitleBar, NavBar, NavBarItem,
CedraInteractiveCard, StopButton, GameCard, ViewAllCard,
DonateButton, HoverBuilder, WindowButton, PerformanceMonitor (NEW),
SkeletonLoader, GradientText
```

### GPU Optimizations Applied
- CedraInteractiveCard: Animation stops when not visible
- SkeletonLoader: Shimmer + pulse stop when not visible
- GradientText: Color animation stops when not visible
- ViewAllCard: Animation stops when not visible

### Dialogs (2)
- GameDetectorDialog.qml
- AllGamesDialog.qml

---

## Dependencies (vcpkg.json)

### Core Dependencies (Always Required)
```
boost-filesystem    # Cross-platform filesystem
openssl            # TLS/crypto
curl               # HTTP client
nlohmann-json      # JSON parsing
lz4                # Fast compression
zlib               # Deflate compression
zstd               # Modern compression
sqlite3            # Local database
spdlog             # Logging
```

### Optional Performance Libraries (NEW)
```
taskflow           # Parallel task execution
mio                # Memory-mapped I/O
simdjson           # SIMD-accelerated JSON
simdutf            # SIMD-accelerated UTF conversion
concurrentqueue    # Lock-free queue
```

### Optional Feature Libraries (NEW)
```
bit7z              # 7-zip archive support
libarchive         # Multi-format archives
minizip-ng         # ZIP with encryption
efsw               # Filesystem watcher
sqlitecpp          # Modern SQLite wrapper
libsodium          # Modern cryptography
digestpp           # Hash algorithms
```

### Qt/QML
```
Qt6::Core
Qt6::Gui
Qt6::Quick
Qt6::QuickControls2
Qt6::QuickEffects
Qt6::Network
Qt6::Svg
Qt6::Concurrent
```

---

## Library Integration Plan

See `core/TODO_LIBRARY_INTEGRATION.md` for detailed integration plan.

### Priority Order
1. **HIGH** (Immediate)
   - TASKFLOW - Parallel game scanning and asset parsing
   - MIO - Large file memory-mapping
   - SIMDJSON - Fast JSON parsing for RPG Maker

2. **MEDIUM** (MVP)
   - BIT7Z/LIBARCHIVE - Archive extraction
   - SIMDUTF - UTF conversion
   - SQLITECPP - Database improvements

3. **LOW** (Post-MVP)
   - EFSW - Filesystem watching
   - LIBSODIUM - Advanced crypto
   - CONCURRENTQUEUE - Streaming

### Feature Detection
Use compile-time macros in code:
```cpp
#ifdef MAKINEAI_HAS_TASKFLOW
    // Use parallel execution
#else
    // Fallback to sequential
#endif
```

Runtime check via `makineai::Features` struct in `features.hpp`.

---

## Known Issues & TODOs

### CRITICAL (Blocks Release)
- [x] ~~No vcpkg.json~~ - Created with 22 dependencies
- [x] ~~AssetParser registration commented out~~ - Fixed with factory pattern
- [x] ~~Handler registration~~ - All 5 handlers now registered
- [x] ~~Build system~~ - CMakePresets.json + justfile
- [x] ~~Legacy scripts~~ - Deleted .bat/.ps1 files
- [ ] **Tests disabled** - BUILD_TESTING=OFF, 6 test files ready
- [ ] **Handler verification** - Need to test with real games
- [ ] **GitHub repo** - Need to push to jlceaser/MakineAI

### HIGH Priority
- [x] ~~Create vcpkg.json manifest~~ - Done
- [x] ~~GPU optimization~~ - Animation visibility binding
- [x] ~~AssetParser factory~~ - 4 parsers active via factory functions
- [x] ~~Handler factory~~ - 5 handlers registered (Unity, RPGMaker, Unreal, RenPy, GameMaker)
- [ ] Enable and run unit tests (BUILD_TESTING=ON)
- [ ] Verify GameDetector (Steam/Epic/GOG)
- [ ] Test handlers with real games
- [ ] Implement Taskflow parallel scanning

### MEDIUM Priority
- [ ] Recipe system implementation
- [ ] Translation package format
- [ ] Error handling improvements
- [ ] Implement MIO for large files
- [ ] Implement simdjson for JSON parsing

### LOW Priority
- [ ] Code documentation
- [ ] User documentation
- [ ] EFSW filesystem watching
- [ ] Installer/deployment

### COMPLETED (2026-01-23)
- [x] GPU optimization (animations stop when not visible)
- [x] PerformanceMonitor component with F3 toggle
- [x] vcpkg.json with 22 dependencies
- [x] features.hpp for compile-time feature detection
- [x] Optional library CMake infrastructure
- [x] TODO_LIBRARY_INTEGRATION.md plan
- [x] Library includes in source files with TODOs
- [x] **AssetParser factory pattern** - parsers_factory.hpp + 4 parsers active
- [x] **EngineHandler factory** - All 5 handlers registered (Unity, Unreal, RenPy, RPGMaker, GameMaker)
- [x] **CMakePresets.json** - Modern build configuration with 6 presets
- [x] **justfile** - Task automation (setup, build, test, deploy, format)
- [x] **Professional .gitignore** - 240 lines, organized by category
- [x] **Deleted legacy scripts** - Removed .bat/.ps1 files
- [x] **Project cleanup** - Removed build artifacts, duplicates

---

## Development Shortcuts

### Performance Testing
- Press **F3** in app to toggle PerformanceMonitor
- Shows: FPS, frame time, min/max, dropped frames
- Color coded: Green (60+), Yellow (30-59), Red (<30)

### Quick Reference
| What | Where |
|------|-------|
| Build config | `CMakePresets.json` |
| Task runner | `justfile` (run: `just <recipe>`) |
| Dependencies | `vcpkg.json` |
| QML UI files | `qml/qml/*.qml` |
| QML Components | `qml/qml/components/` |
| Backend Services | `qml/src/services/` |
| Core Headers | `core/include/makineai/` |
| Core Impl | `core/src/` |
| Engine Handlers | `core/src/handlers/` |
| Tests | `core/tests/` |
| Feature Flags | `core/include/makineai/features.hpp` |
| Library Plan | `core/TODO_LIBRARY_INTEGRATION.md` |

### Common just Commands
| Command | Description |
|---------|-------------|
| `just` | Show all recipes |
| `just setup` | Install vcpkg deps |
| `just all` | Build everything |
| `just core` | Build core library |
| `just qml` | Build QML app |
| `just test` | Run tests |
| `just run` | Run app (debug) |
| `just deploy` | Deploy with Qt deps |
| `just clean` | Clean build dirs |

### Key Files to Modify
| Task | File |
|------|------|
| Add new QML screen | `qml/CMakeLists.txt` (QML_FILES) |
| Add backend service | `qml/src/services/` + CMakeLists |
| Add core module | `core/CMakeLists.txt` (SOURCES) |
| Add handler | `core/src/handlers/` + header |
| Add dependency | `vcpkg.json` + `core/CMakeLists.txt` |

### Claude Tools
| Need | Tool |
|------|------|
| Qt/QML docs | `context7` -> query-docs |
| Code review | `/review-pr` |
| Commit | `/commit` |
| PR | `/commit-push-pr` |

---

## Active Development Focus

### Phase 1: GitHub Ready (Current - 69%)
1. [x] vcpkg.json manifest
2. [x] GPU optimizations
3. [x] Performance monitoring
4. [x] AssetParser factory pattern
5. [x] Handler registration (5 handlers)
6. [x] CMakePresets.json + justfile
7. [x] Professional .gitignore
8. [ ] Push to GitHub
9. [ ] Enable tests
10. [ ] Verify 3 handlers

### Phase 2: MVP (Target: 75%)
1. [ ] 3 working handlers
2. [ ] Parallel scanning (Taskflow)
3. [ ] 10+ game recipes
4. [ ] Basic game detection
5. [ ] Alpha release

### Phase 3: Beta
1. [ ] Full handler support
2. [ ] 50+ recipes
3. [ ] Auto-update system
4. [ ] Public beta

---

## Version History

| Version | Date | Architecture | Status |
|---------|------|--------------|--------|
| 0.0.1-alpha | 2026-01 | Flutter + C++ FFI | Archived |
| 0.1.0-alpha | 2026-01 | Qt QML + C++ | **Active** |

---

*Last updated: 2026-01-23*
