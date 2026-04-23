# Geliştirme Ortamı Kurulumu

Bu kılavuz Makine-Launcher geliştirme ortamının kurulumunu anlatmaktadır.

---

## Gereksinimler

### Zorunlu Yazılımlar

| Yazılım | Sürüm | İndirme |
|---------|-------|---------|
| Visual Studio 2022 | 17.8+ | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| Qt | 6.10+ | [qt.io](https://www.qt.io/download) |
| CMake | 3.25+ | [cmake.org](https://cmake.org/download/) |
| vcpkg | Latest | [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) |
| Git | 2.40+ | [git-scm.com](https://git-scm.com/) |

### Visual Studio Workload'ları

Visual Studio Installer'dan kurun:
- Desktop development with C++
- C++ CMake tools for Windows

### Opsiyonel Araçlar

| Araç | Amaç |
|------|------|
| just | Task runner (önerilen) |
| Qt Creator | Alternatif IDE |
| Ninja | Daha hızlı build |
| ccache | Build önbelleği |
| clang-format | Kod formatlama |

---

## Kurulum Adımları

### 1. Repository Klonlama

```bash
git clone <repo-url>
cd Makine-Launcher
```

### 2. vcpkg Kurulumu

```bash
# vcpkg'yi klonla
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Ortam degiskenini ayarla
setx VCPKG_ROOT "C:\vcpkg"
```

### 3. Qt Kurulumu

1. Qt Online Installer'ı indir
2. Qt 6.10+ kur:
   - MSVC 2022 64-bit
   - Qt Quick
   - Qt Quick Controls
   - Ek modüller: Network, Svg, Concurrent

3. Ortam değişkenini ayarla:
```bash
setx Qt6_DIR "C:\Qt\6.10.1\msvc2022_64"
```

### 4. Bağımlılıkları Kur

```bash
cd C:\Workspace\Makine\Makine-Launcher

# just ile (önerilen)
just setup

# veya manuel
vcpkg install --triplet x64-windows
```

### 5. Pre-commit Hook'ları (Opsiyonel)

```bash
pip install pre-commit
pre-commit install
```

---

## IDE Ayarları

### Visual Studio Code

1. C/C++ extension kur
2. CMake Tools extension kur
3. Qt extension kur

`.vscode/settings.json`:
```json
{
    "cmake.configureSettings": {
        "CMAKE_TOOLCHAIN_FILE": "${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    }
}
```

### Qt Creator

1. CMakeLists.txt'i aç
2. Kit olarak "Qt 6.10 MSVC" seç
3. Build & Run

### Visual Studio 2022

1. File > Open > CMake
2. CMakePresets.json otomatik alınır
3. Preset seç ve build et

---

## İlk Build

### just ile

```bash
# Herseyi derle
just all

# veya ayri ayri
just core    # Core library
just qml     # QML uygulama
```

### CMake ile

```bash
# Configure
cmake --preset dev

# Build
cmake --build build/dev

# Run
./build/dev/Makine-Launcher.exe
```

---

## Build Presetleri

| Preset | Açıklama |
|--------|----------|
| `dev` | Günlük geliştirme — Core+UI (MinGW+vcpkg) |
| `dev-ui` | UI-only, Core yok (MinGW, vcpkg gereksiz) |
| `debug` | Debug build — Core+UI (MinGW+vcpkg) |
| `release` | Release — Core+UI (MSVC+vcpkg) |
| `release-static` | Tek EXE — UI-only (static Qt) |
| `core` | Sadece core library (MSVC+vcpkg) |

---

## Doğrulama

Build başarılı ise:
```bash
./build/dev/Makine-Launcher.exe
```

Uygulama açılmalı ve ana ekranı göstermelidir.

---

## Sonraki Adımlar

- [Mimari](architecture.md)
- [Core Kütüphane](core-library.md)
- [Build Sistemi](build-system.md)
