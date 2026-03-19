# Fresh Clone Runbook

Clone'dan çalışan exe'ye adım adım rehber.

---

## 1. Clone

```bash
git clone https://github.com/MakineCeviri/Makine-Launcher.git
cd Makine-Launcher
```

## 2. Ortam Değişkenleri (.env)

```bash
cp .env.example .env
# .env dosyasını aç ve gerçek token'larını yaz
```

Gerekli token'lar:
- `CLOUDFLARE_API_TOKEN` + `CLOUDFLARE_ACCOUNT_ID` — CDN erişimi
- `MAKINE_SENTRY_DSN` — crash reporting (opsiyonel, sadece release build)

Diğer token'lar (Discord, Railway, Gemini) sadece belirli servisler için gerekli.

## 3. encryption_key.h

Bu dosya repo'da yoktur. Paket şifreleme/çözme için gereklidir.

```
core/include/makine/encryption_key.h
```

Dosyayı proje sahibinden al veya kendi anahtarını üret. Olmadan build eder ama şifreli paketler çözülemez.

## 4. Araçları Kur

### Zorunlu

| Araç | Not |
|------|-----|
| Qt 6.10+ | MinGW 13.1 ve/veya MSVC 2022 kit'i |
| CMake 3.28+ | Qt Tools ile gelir |
| Ninja | Qt Tools ile gelir |
| Git | Submodule desteği |

### vcpkg (Core build için zorunlu)

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg && bootstrap-vcpkg.bat
setx VCPKG_ROOT "C:\vcpkg"
```

### Opsiyonel

| Araç | Komut |
|------|-------|
| just | `winget install Casey.Just` |
| clang-format | Qt Tools ile gelir |
| Doxygen | `winget install doxygen` |

## 5. PATH Ayarla (bash/MSYS2)

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:$PATH"
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"  # runtime DLL'ler
```

## 6. vcpkg Bağımlılıkları (Core build için)

```bash
just setup
# veya manuel:
vcpkg install boost-filesystem:x64-mingw-dynamic openssl:x64-mingw-dynamic \
  curl:x64-mingw-dynamic nlohmann-json:x64-mingw-dynamic lz4:x64-mingw-dynamic \
  zlib:x64-mingw-dynamic zstd:x64-mingw-dynamic sqlite3:x64-mingw-dynamic \
  spdlog:x64-mingw-dynamic simdjson:x64-mingw-dynamic mio:x64-mingw-dynamic \
  taskflow:x64-mingw-dynamic concurrentqueue:x64-mingw-dynamic \
  simdutf:x64-mingw-dynamic sqlitecpp:x64-mingw-dynamic \
  libsodium:x64-mingw-dynamic libarchive:x64-mingw-dynamic \
  bit7z:x64-mingw-dynamic efsw:x64-mingw-dynamic
```

> UI-only build (`dev-ui`) vcpkg gerektirmez.

## 7. Build & Run

```bash
# Hızlı yol (just)
just run

# Veya manuel
cmake --preset dev
cmake --build --preset dev
./build/dev/Makine-Launcher.exe
```

### Build Alternatifleri

| Komut | Ne yapar |
|-------|----------|
| `just dev` | Core+UI (MinGW+vcpkg) |
| `just dev-ui` | UI-only (vcpkg gereksiz) |
| `just debug` | Debug build |
| `just release` | Release (MSVC+vcpkg) |
| `just core` | Sadece core kütüphanesi |

## 8. Doğrulama

Build başarılıysa uygulama açılmalı ve ana ekranı göstermelidir.

```bash
# Test (opsiyonel)
just test

# Dokümantasyon (opsiyonel, Doxygen gerekir)
just docs
```

---

## Sorun Giderme

| Sorun | Çözüm |
|-------|-------|
| `vcpkg not found` | `VCPKG_ROOT` env var'ı ayarla |
| `Qt6 not found` | PATH'e Qt CMake dizinini ekle |
| `encryption_key.h not found` | Adım 3'e bak |
| `submodule is empty` | `git submodule update --init` |
| MinGW `<regex>` hatası | Bilinen sorun — regex kullanan dosyalar build'den hariç |

---

Detaylı kurulum: [setup.md](setup.md) | Mimari: [architecture.md](architecture.md)
