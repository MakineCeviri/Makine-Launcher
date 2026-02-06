# MakineAI C++ Native Core

Modern C++20/23 native core for the MakineAI Turkish game translation platform.

## Overview

The MakineAI C++ core provides high-performance, secure game detection, asset parsing, and translation patch management. It integrates with Flutter UI via FFI (Foreign Function Interface).

## Architecture

```
core/
├── include/
│   ├── makineai/           # Main headers
│   │   ├── core.hpp        # Main entry point
│   │   ├── types.hpp       # Type definitions
│   │   ├── error.hpp       # Error handling
│   │   ├── asset_parser.hpp
│   │   ├── patch_engine.hpp
│   │   ├── game_detector.hpp
│   │   ├── package_manager.hpp
│   │   ├── runtime_manager.hpp
│   │   ├── security.hpp
│   │   └── version_tracker.hpp
│   └── formats/            # Game format definitions
│       ├── unity_bundle.hpp
│       ├── unreal_pak.hpp
│       ├── bethesda_ba2.hpp
│       └── gamemaker_data.hpp
├── src/
│   ├── core.cpp
│   ├── asset_parser/       # Asset parsing implementations
│   ├── patch_engine/       # Patch operations
│   ├── game_detector/      # Game store scanners
│   ├── package_manager/    # Translation package handling
│   ├── runtime/            # BepInEx/XUnity integration
│   └── platform/
│       └── windows/        # Windows-specific code
└── tests/                  # Google Test unit tests
```

## Building

### Prerequisites

- Visual Studio 2022 (MSVC v143 toolset)
- CMake 3.28+
- vcpkg package manager

### Setup vcpkg

```bash
# Clone vcpkg if not installed
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable
set VCPKG_ROOT=C:\vcpkg
```

### Build Steps

```bash
cd C:\cedra\MakineAI\core

# Configure with vcpkg
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

# Build Release
cmake --build build --config Release

# Build Debug
cmake --build build --config Debug
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Boost.Filesystem | 1.84+ | Cross-platform filesystem |
| OpenSSL | 3.2+ | Cryptography, signatures |
| libcurl | 8.5+ | HTTP downloads |
| nlohmann/json | 3.11+ | JSON parsing |
| LZ4 | 1.9+ | Unity bundle decompression |
| zlib | 1.3+ | General compression |
| zstd | 1.5+ | Unreal pak compression |
| SQLite3 | 3.44+ | Local database |
| spdlog | 1.13+ | Logging |
| Google Test | 1.14+ | Unit testing |

## Modules

### Core

Central singleton providing access to all modules:

```cpp
#include <makineai/core.hpp>

auto& core = makineai::Core::instance();
core.initialize();

auto games = core.gameDetector().scanAll();
```

### AssetParser

Parses game assets to extract translatable strings:

```cpp
auto& parser = core.assetParser();
auto engine = parser.detectEngine(gameDir);
auto result = parser.parseFile(assetPath);
```

Supported formats:
- Unity AssetBundle (UnityFS + LZ4)
- Unreal Engine PAK
- Bethesda BA2
- GameMaker data.win

### GameDetector

Scans game stores for installed games:

```cpp
auto& detector = core.gameDetector();
auto games = detector.scanAll();  // Steam, Epic, GOG

auto game = detector.detectGame("D:/Games/Starfield");
auto verified = detector.verify(game);
```

### PatchEngine

Applies translation patches with backup support:

```cpp
auto& patcher = core.patchEngine();
auto result = patcher.apply(operations, game, version);
// ... later ...
auto restored = patcher.restore(gameDir, backupId);
```

### PackageManager

Downloads and manages translation packages:

```cpp
auto& packages = core.packageManager();
packages.fetchManifest();

if (packages.hasTranslation(game)) {
    auto result = packages.install(package, game);
}
```

### RuntimeManager

Handles Unity runtime translation (BepInEx + XUnity):

```cpp
auto& runtime = core.runtimeManager();
if (runtime.needsRuntime(game)) {
    runtime.install(game);
    runtime.addTranslations(game, translationDir);
}
```

## FFI Integration

The core exposes a C API for Flutter integration:

```c
// C API (makineai_ffi.h)
MakineAI_Result makineai_init(const char* data_dir);
MakineAI_Result makineai_scan_games(...);
MakineAI_Result makineai_install_translation(...);
```

```dart
// Dart bindings (makineai_ffi.dart)
final bindings = MakineAIBindings.instance;
bindings.initialize();
final games = await bindings.scanGames();
```

## Error Handling

Uses `std::expected<T, Error>` for type-safe error handling:

```cpp
auto result = detector.detectGame(path);
if (!result) {
    logger()->error("Error: {}", result.error().message());
    return;
}
auto& game = *result;
```

## Logging

Uses spdlog for logging:

```cpp
makineai::logger()->info("Found {} games", count);
makineai::logger()->error("Failed to parse: {}", path);
```

Logs are written to `%LOCALAPPDATA%/MakineAI/logs/makineai.log`

## Thread Safety

- `Core::instance()` is thread-safe (singleton)
- Individual module methods should be called from a single thread
- Progress callbacks may be called from worker threads

## Security

- Package signature verification (RSA-2048)
- File checksum validation (SHA-256)
- Windows Authenticode signature checking
- No direct code execution from downloaded content

## License

Copyright (c) 2026 MakineAI Team. All rights reserved.
