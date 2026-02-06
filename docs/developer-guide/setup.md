# Gelistirme Ortami Kurulumu

Bu kilavuz MakineAI gelistirme ortaminin kurulumunu anlatmaktadir.

---

## Gereksinimler

### Zorunlu Yazilimlar

| Yazilim | Surum | Indirme |
|---------|-------|---------|
| Visual Studio 2022 | 17.8+ | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| Qt | 6.8.1+ | [qt.io](https://www.qt.io/download) |
| CMake | 3.25+ | [cmake.org](https://cmake.org/download/) |
| vcpkg | Latest | [github.com/microsoft/vcpkg](https://github.com/microsoft/vcpkg) |
| Git | 2.40+ | [git-scm.com](https://git-scm.com/) |

### Visual Studio Workload'lari

Visual Studio Installer'dan kurun:
- Desktop development with C++
- C++ CMake tools for Windows

### Opsiyonel Araclar

| Arac | Amac |
|------|------|
| just | Task runner (onerilen) |
| Qt Creator | Alternatif IDE |
| Ninja | Daha hizli build |
| ccache | Build onbellegi |
| clang-format | Kod formatlama |

---

## Kurulum Adimlari

### 1. Repository Klonlama

```bash
git clone <repo-url>
cd MakineAI
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

1. Qt Online Installer'i indir
2. Qt 6.8.1+ kur:
   - MSVC 2022 64-bit
   - Qt Quick
   - Qt Quick Controls
   - Ek moduller: Network, Svg, Concurrent

3. Ortam degiskenini ayarla:
```bash
setx Qt6_DIR "C:\Qt\6.8.1\msvc2022_64"
```

### 4. Bagimliliklari Kur

```bash
cd C:\cedra\MakineAI

# just ile (onerilen)
just setup

# veya manuel
vcpkg install --triplet x64-windows
```

### 5. Pre-commit Hook'lari (Opsiyonel)

```bash
pip install pre-commit
pre-commit install
```

---

## IDE Ayarlari

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

1. CMakeLists.txt'i ac
2. Kit olarak "Qt 6.8.1 MSVC" sec
3. Build & Run

### Visual Studio 2022

1. File > Open > CMake
2. CMakePresets.json otomatik alinir
3. Preset sec ve build et

---

## Ilk Build

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
./build/dev/MakineAI.exe
```

---

## Build Presetleri

| Preset | Aciklama |
|--------|----------|
| `dev` | Gunluk gelistirme (MinGW) |
| `debug` | Debug build with symbols |
| `release` | Release build with optimizations |
| `core` | Sadece core library |

---

## Dogrulama

Build basarili ise:
```bash
./build/dev/MakineAI.exe
```

Uygulama acilmali ve ana ekrani gostermelidir.

---

## Sonraki Adimlar

- [Mimari](architecture.md)
- [Core Kutuphane](core-library.md)
- [Build Sistemi](build-system.md)
