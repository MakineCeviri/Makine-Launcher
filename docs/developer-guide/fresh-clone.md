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

Bu dosya repo'da yoktur (gitignored). Paket şifreleme/çözme için gereklidir.

```
qml/src/services/encryption_key.h
```

`scripts/.encryption_key` dosyasından üretilir (anahtarı proje sahibinden al):

```bash
python scripts/generate_key_header.py
```

`dev-ui` dışındaki build'ler bu dosya olmadan derlenmez.

## 4. Araçları Kur

### Zorunlu

| Araç | Not |
|------|-----|
| Qt 6.11.1 | MinGW 13.1 kit'i (`C:\Qt\6.11.1\mingw_64`, preset'lerde sabit); MSVC 2022 kit'i yalnız `release` için |
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
export PATH="/c/Qt/6.11.1/mingw_64/bin:$PATH"  # runtime DLL'ler
```

## 6. vcpkg Bağımlılıkları (Core build için)

```bash
just setup        # openssl, nlohmann-json, zstd, spdlog
just setup-tests  # + gtest (just test için)
# veya manuel:
vcpkg install openssl:x64-mingw-dynamic nlohmann-json:x64-mingw-dynamic \
  zstd:x64-mingw-dynamic spdlog:x64-mingw-dynamic gtest:x64-mingw-dynamic
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
| MinGW `<regex>` hatası | Bilinen sorun — `<regex>` kullanma; `find()`, `starts_with()`, `ends_with()` kullan |

---

Detaylı kurulum: [setup.md](setup.md) | Mimari: [architecture.md](architecture.md)
