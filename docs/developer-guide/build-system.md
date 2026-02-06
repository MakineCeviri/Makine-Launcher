# Build Sistemi

MakineAI CMake ve vcpkg tabanli build sisteminin aciklamasi.

---

## Genel Bakis

MakineAI su araclari kullanir:
- **CMake** - Build konfigürasyonu
- **vcpkg** - C++ paket yonetimi
- **Ninja** - Build sistemi (opsiyonel)
- **just** - Task runner (opsiyonel)

---

## CMake Presetleri

`CMakePresets.json` dosyasinda tanimli presetler:

| Preset | Aciklama | Kullanim |
|--------|----------|----------|
| `dev` | Gunluk gelistirme | `cmake --preset dev` |
| `debug` | Debug sembollu | `cmake --preset debug` |
| `release` | Optimizasyonlu | `cmake --preset release` |
| `core` | Sadece core lib | `cmake --preset core` |

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
  "name": "makineai",
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
MakineAI/
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

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MakineAI VERSION 0.1.0)

# Core library
add_subdirectory(core)

# QML application
add_subdirectory(qml)
```

### Core CMakeLists.txt

```cmake
# Static library
add_library(makineai_core STATIC
    src/game_detector/steam_scanner.cpp
    src/game_detector/epic_scanner.cpp
    # ...
)

target_include_directories(makineai_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(makineai_core PUBLIC
    Boost::filesystem
    spdlog::spdlog
    # ...
)
```

### QML CMakeLists.txt

```cmake
qt_add_executable(MakineAI
    src/main.cpp
    src/services/gameservice.cpp
    # ...
)

qt_add_qml_module(MakineAI
    URI MakineAI
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/HomeScreen.qml
        # ...
)

target_link_libraries(MakineAI PRIVATE
    makineai_core
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
qt_add_qml_module(MakineAI
    URI MakineAI
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
windeployqt.exe MakineAI.exe --qmldir qml/qml

# veya just ile
just deploy
```

---

## CI/CD

### GitHub Actions

```yaml
# .github/workflows/ci.yml
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11

      - name: Configure
        run: cmake --preset release

      - name: Build
        run: cmake --build build/release

      - name: Test
        run: ctest --test-dir build/release
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
# Qt6_DIR ayarla
setx Qt6_DIR "C:\Qt\6.8.1\msvc2022_64"
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
