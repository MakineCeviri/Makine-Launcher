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
| `release-static` | Tek EXE, UI-only | MinGW (static Qt) | `cmake --preset release-static` |
| `full-static` | Tek EXE, Core+UI | MinGW+vcpkg (static) | `cmake --preset full-static` |
| `core` | Sadece core lib | MSVC+vcpkg | `cmake --preset core` |

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
    "boost-filesystem",
    "openssl",
    "curl",
    "nlohmann-json",
    "lz4",
    "zlib",
    "zstd",
    "sqlite3",
    "spdlog",
    "bit7z",
    "libarchive",
    "simdjson",
    "efsw",
    "mio",
    "taskflow",
    "concurrentqueue",
    "simdutf",
    "sqlitecpp",
    "libsodium"
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

```bash
# Otomatik (manifest mode)
cmake --preset dev  # vcpkg otomatik calisir

# Manuel
vcpkg install --triplet x64-windows
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

Core ve QML ayri CMake projeleri olarak build edilir (CMakePresets.json):

```bash
# Core library (MSVC + vcpkg)
cmake --preset core-debug     # Configure
cmake --build core/build      # Build

# QML application (MinGW + Qt)
cmake --preset dev             # Configure
cmake --build qml/build/dev    # Build
```

### Core CMakeLists.txt

```cmake
# Static library
add_library(makine_core STATIC
    src/game_detector/steam_scanner.cpp
    src/game_detector/epic_scanner.cpp
    # ...
)

target_include_directories(makine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(makine_core PUBLIC
    Boost::filesystem
    spdlog::spdlog
    # ...
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

### GitHub Actions

```yaml
# .github/workflows/ci.yml (simplified)
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ilammy/msvc-dev-cmd@v1

      - name: Cache vcpkg
        uses: actions/cache@v5
        with:
          path: C:/vcpkg/installed
          key: vcpkg-classic-${{ hashFiles('vcpkg.json') }}

      - name: Install dependencies
        run: |
          $deps = (Get-Content vcpkg.json | ConvertFrom-Json).dependencies
          foreach ($dep in $deps) { C:/vcpkg/vcpkg install "${dep}:x64-windows" --classic --recurse }

      - name: Configure & Build Core
        run: |
          cmake -B core/build -S core -G Ninja -DCMAKE_BUILD_TYPE=Release -DVCPKG_MANIFEST_MODE=OFF -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
          cmake --build core/build

      - name: Test
        run: ctest --test-dir core/build
```

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
