# Build Sistemi

Makine-Launcher CMake ve vcpkg tabanli build sisteminin aciklamasi.

---

## Genel Bakis

Makine-Launcher su araclari kullanir:
- **CMake** - Build konfigürasyonu
- **vcpkg** - C++ paket yonetimi
- **Ninja** - Build sistemi (opsiyonel)
- **just** - Task runner (opsiyonel)

---

## CMake Presetleri

`CMakePresets.json` dosyasinda tanimli presetler:

| Preset | Aciklama | Derleyici | Kullanim |
|--------|----------|-----------|----------|
| `dev` | Gunluk gelistirme (Core+UI) | MinGW+vcpkg | `cmake --preset dev` |
| `dev-ui` | UI-only (Core yok) | MinGW | `cmake --preset dev-ui` |
| `debug` | Debug sembollu (Core+UI) | MinGW+vcpkg | `cmake --preset debug` |
| `release` | Release (Core+UI) | MSVC+vcpkg | `cmake --preset release` |
| `release-mingw` | Dagitim build'i (ZIP, MSIX) | MinGW+vcpkg | `cmake --preset release-mingw` |
| `release-static` | Tek EXE, Core+UI | MinGW (static Qt)+vcpkg | `cmake --preset release-static` |
| `core` | Sadece core lib + testleri | MinGW+vcpkg | `cmake --preset core` |

### Preset Kullanimi

```bash
# Configure
cmake --preset dev

# Build
cmake --build build/dev

# veya tek komut
cmake --build --preset dev
```

---

## vcpkg Bagimliliklari

`vcpkg.json` manifest dosyasi:

```json
{
  "name": "makine-launcher",
  "version": "0.1.0",
  "dependencies": [
    "openssl",
    "nlohmann-json",
    "zstd",
    "spdlog"
  ],
  "features": {
    "tests": {
      "description": "Build unit tests",
      "dependencies": ["gtest"]
    }
  }
}
```

### Bagimlilik Kurulumu

vcpkg classic mode ile kullanilir (`VCPKG_MANIFEST_MODE=OFF`, triplet `x64-mingw-dynamic`);
configure sirasinda paket kurulmaz.

```bash
just setup        # openssl, nlohmann-json, zstd, spdlog
just setup-tests  # + gtest

# Manuel
vcpkg install openssl:x64-mingw-dynamic nlohmann-json:x64-mingw-dynamic \
  zstd:x64-mingw-dynamic spdlog:x64-mingw-dynamic gtest:x64-mingw-dynamic
```

---

## Proje Yapisi

```
Makine-Launcher/
├── CMakeLists.txt          # Root CMake
├── CMakePresets.json       # Presetler
├── vcpkg.json              # Bagimliliklar
│
├── core/
│   └── CMakeLists.txt      # Core library
│
└── qml/
    └── CMakeLists.txt      # QML app
```

### Build Presets

Core tek basina (`core` preset) ya da kok CMakeLists.txt uzerinden QML ile birlikte
(super-build) derlenir (CMakePresets.json):

```bash
# Core library + testleri (MinGW + vcpkg) → build/core
cmake --preset core
cmake --build --preset core

# Core + QML application (MinGW + Qt + vcpkg) → build/dev
cmake --preset dev
cmake --build --preset dev
```

### Core CMakeLists.txt

```cmake
# Static library
add_library(makine_core STATIC
    src/package_catalog/package_catalog.cpp
    src/crash_recovery/crash_recovery.cpp
    src/file_integrity/file_integrity.cpp
)

target_include_directories(makine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(makine_core PUBLIC
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
```

### QML CMakeLists.txt

```cmake
qt_add_executable(MakineLauncher
    src/main.cpp
    src/services/gameservice.cpp
    # ...
)

qt_add_qml_module(MakineLauncher
    URI MakineLauncher
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/HomeScreen.qml
        # ...
)

target_link_libraries(MakineLauncher PRIVATE
    makine_core
    Qt6::Quick
    Qt6::QuickControls2
)
```

---

## Build Tipleri

### Debug Build

- Optimizasyon yok
- Debug sembolleri
- Assert'ler aktif
- Yavas ama debug kolay

```bash
cmake --preset debug
cmake --build build/debug
```

### Release Build

- Full optimizasyon
- Debug sembolleri yok
- Assert'ler pasif
- Hizli

```bash
cmake --preset release
cmake --build build/release
```

---

## Qt Integration

### Qt Bulma

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Network
    Svg
    Concurrent
)
```

### QML Modul

```cmake
qt_add_qml_module(MakineLauncher
    URI MakineLauncher
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    RESOURCES
        resources/icons/logo.svg
)
```

### Qt Deploy

```bash
# Windows
windeployqt.exe Makine-Launcher.exe --qmldir qml/qml

# veya just ile
just deploy
```

---

## CI/CD

`.github/workflows/ci.yml` (Build & test): `dev`'e her push'ta (yalnizca `docs/` / `*.md`
degisen push'lar haric) ve `dev`'e acilan PR'larda `windows-latest` uzerinde:

1. MinGW 13.1 + Qt 6.11.1 kurar
2. vcpkg paketlerini classic mode ile kurar (`openssl nlohmann-json zstd spdlog gtest`, `x64-mingw-dynamic`)
3. Gecici bir paket anahtari uretir (`scripts/generate_key_header.py`)
4. `dev` preset'ini derler (crash reporting kapali) ve `ctest --preset dev-tests` calistirir

Diger is akislari: `deploy-manifests.yml`, `telemetry-watchdog.yml`.

---

## Troubleshooting

### vcpkg Bulunamadi

```bash
# VCPKG_ROOT ayarla
setx VCPKG_ROOT "C:\vcpkg"

# Yeniden ac terminal
```

### Qt Bulunamadi

```bash
# Qt6_DIR ayarla (MSVC)
setx Qt6_DIR "C:\Qt\6.10.1\msvc2022_64"
```

### Ninja Bulunamadi

```bash
# CMake varsayilan generator kullanir
# veya Ninja kur:
winget install Ninja-build.Ninja
```

---

## Sonraki Adimlar

- [Test Yazma](testing.md)
- [Gelistirme Ortami](setup.md)
