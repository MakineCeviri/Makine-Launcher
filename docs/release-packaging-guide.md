# Makine-Launcher — Release Packaging Guide

> Versiyon: 0.1.0-pre-alpha | Qt 6.10.1, MinGW 13.1.0, C++23
> Bu rehber, dağıtım için hazır tek-EXE paket üretmenin adım adım sürecini belgeler.

---

## 1. Genel Bakış: Dağıtım Yolları

Projenin iki farklı dağıtım yolu var:

| Yol | Preset | Çıktı | Durum |
|-----|--------|-------|-------|
| **Static Qt** | `release-static` | `build/release-static/Makine-Launcher.exe` — tek EXE, DLL yok | Tercih edilen, kurulum gerektirir |
| **Shared Qt + windeployqt** | `dev` + `just deploy` | `dist/` klasörü — EXE + DLL'ler | Geçici çözüm (şu an kullanılan) |

Static build hedef; windeployqt yolu ise static Qt build kurulana kadar geçerli ara çözümdür.

---

## 2. Ön Koşullar

### Her iki yol için
```bash
# PATH kurulumu (her build session'ında gerekli)
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Program Files/Git/usr/bin:$PATH"

# vcpkg bağımlılıklarını kur (ilk sefer veya yeni paket eklenince)
just setup
```

### Static build için (ek: bir kerelik)
Static Qt build ~1-2 saat sürer. Qt Online Installer'dan `Qt 6.10.1 > Sources` bileşeni kurulu olmalı.

```bash
just setup-static-qt
```

Çıktı: `C:\Qt\6.10.1\mingw_64_static\`

---

## 3. Release Build Adımları

### Yol A — Static EXE (Tek Dosya, Tercih Edilen)

```bash
# 1. Build
just release-static
# → build/release-static/Makine-Launcher.exe

# 2. Paketle (zip)
just package-static
# → dist-static/Makine-Launcher.exe
# → Makine-Launcher-static.zip
```

**Preset detayları** (`release-static` — `CMakePresets.json`):
- Compiler: MinGW GCC 13.1.0
- Qt: `C:/Qt/6.10.1/mingw_64_static` (statik Qt)
- Linker flags: `-static -static-libgcc -static-libstdc++ -Wl,--gc-sections`
- LTO: ON (`CMAKE_INTERPROCEDURAL_OPTIMIZATION`)
- vcpkg triplet: `x64-mingw-static`
- Çıktı: `build/release-static/`

Post-build otomatik çalışanlar:
- `cmake --strip` — debug sembollerini temizler
- `sha256sum` → `Makine-Launcher.exe.sha256` üretir
- (Opsiyonel) UPX sıkıştırma: `cmake -DMAKINE_UPX=ON` ile etkinleştirilir

### Yol B — Shared Qt + windeployqt (Geçici Çözüm)

```bash
# 1. Build + deploy (tek komut)
just deploy
# → dist/Makine-Launcher.exe
# → dist/ altında Qt DLL'leri, QML plugin'leri, platform plugin'leri

# 2. Arşivle
just package
# → Makine-Launcher-release.zip
```

`just deploy` komutunun içeriği:
```powershell
cmake --preset release && cmake --build --preset release
Copy-Item build/release/Makine-Launcher.exe dist/
windeployqt --qmldir qml/qml --release dist/Makine-Launcher.exe
```

**`release` preset** (`CMakePresets.json`):
- Compiler: MSVC (vcpkg-base inherit)
- Qt path: `$env{Qt6_DIR}` ortam değişkeninden
- `MAKINE_RELEASE_VERIFIED=ON` — SSL pin doğrulama ve integrity service aktif
- Çıktı: `build/release/`

---

## 4. Runtime Bağımlılıkları

### Qt Modülleri (qml/CMakeLists.txt'den)
```
Qt6::Core          Qt6::Gui           Qt6::Quick
Qt6::QuickControls2  Qt6::QuickEffects  Qt6::Network
Qt6::Svg           Qt6::Concurrent
```

### windeployqt ile Kopyalananlar (Shared Build)
windeployqt `--qmldir qml/qml` argümanıyla aşağıdakileri otomatik toplar:
- Qt DLL'leri: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Quick.dll`, `Qt6Network.dll`, vb.
- Platform plugin: `platforms/qwindows.dll`
- QML plugins: `QtQuick/`, `QtQuick/Controls/`, `QtQuick/Effects/`, `Qt5Compat/`
- Styles: `QtQuick/Controls/Basic/`

### vcpkg DLL'leri (Shared Build — x64-mingw-dynamic)
`dist/` klasörüne manuel kopyalanması gerekenler:
```
openssl        → libssl-3-x64.dll, libcrypto-3-x64.dll
curl           → libcurl.dll
boost-filesystem → libboost_filesystem*.dll
lz4            → lz4.dll
zstd           → zstd.dll
sqlite3        → sqlite3.dll
spdlog         → spdlog.dll
libsodium      → libsodium.dll
libarchive     → archive.dll
```

> Static build'de bu adım gerekmez; tüm kütüphaneler EXE'ye gömülü gelir.

### Windows Sistem Kütüphaneleri (otomatik)
`psapi`, `Shlwapi`, `Advapi32`, `User32`, `Crypt32`, `Wintrust`, `dwmapi` — her Windows kurulumunda mevcut.

---

## 5. Gizli Bilgiler ve Konfigürasyon

### Derleme Zamanı Gömülen (Compile-Time Embedded)
Çalışma zamanında dosya gerekmez — tüm hassas değerler build sırasında binary'ye gömülür.

| Değer | Kaynak | Nasıl |
|-------|--------|-------|
| Şifreleme anahtarı | `qml/src/services/encryption_key.h` | Header doğrudan include edilir |
| Sentry DSN | `$MAKINE_SENTRY_DSN` ortam değişkeni | CMake build sırasında okunur |
| CDN URL'leri | `qml/src/services/cdnconfig.h` | `constexpr` sabitler |
| Uygulama versiyonu | `CMakeLists.txt` → `MAKINE_VERSION_FULL` | Makro olarak tanımlanır |

### Sentry DSN (Crash Reporting)
`dev` preset'te `MAKINE_CRASH_REPORTING=ON`; release build'de Sentry aktifse:
```bash
export MAKINE_SENTRY_DSN="https://xxxxx@sentry.io/xxxxx"
just release
```
DSN ayarlanmazsa CMake warning üretir, Sentry devre dışı kalır (build başarısız olmaz).

### Gitignore Güvenceleri
Aşağıdakiler `.gitignore` ile repodan dışlanmıştır:
```
qml/src/services/encryption_key.h
scripts/.encryption_key
scripts/r2_config.json
scripts/certs/
*.pem  *.pfx  *.key
.env   .env.*
```

**Bu dosyalara asla dokunma. Değiştirme gerekiyorsa lead'e bildir.**

---

## 6. Versiyonlama

### Mevcut Versiyon
- `vcpkg.json`: `"version": "0.1.0"`
- `CMakeLists.txt`: `project(MakineLauncher VERSION 0.1.0)` + suffix `"pre-alpha"`
- Tam versiyon string'i: `0.1.0-pre-alpha`
- Sentry release tag: `makine-launcher@0.1.0-pre-alpha`

### Versiyon Nasıl Güncellenir
İki yerde değiştirilmeli — ikisi de tutarlı olmalı:

**1. `CMakeLists.txt` (satır 30 ve 36):**
```cmake
project(MakineLauncher VERSION 0.2.0 ...)
set(MAKINE_VERSION_SUFFIX "alpha")   # Stabil release için boş bırak
```

**2. `vcpkg.json` (satır 3):**
```json
"version": "0.2.0"
```

### Semantic Versioning Kuralı
```
MAJOR.MINOR.PATCH[-SUFFIX]

0.x.x    → Pre-release (breaking changes serbest)
1.0.0    → İlk kararlı release
Suffix   → pre-alpha / alpha / beta / rc.1 / (stabil için boş)
```

---

## 7. Kod İmzalama

### Gereklilik
Antivirüs false-positive engelleme için imzalama şiddetle önerilir. Windows Authenticode imzası.

### Sertifika Kurulumu (Bir Kerelik)
```bash
# Admin olarak çalıştır
just setup-cert
# → scripts/certs/thumbprint.txt oluşturulur
# → Windows sertifika deposuna (My store) eklenir
```

### İmzalama
```bash
# Static EXE için (tercih edilen release pipeline)
just release-signed
# Eşdeğer: just release-static && scripts/sign_exe.ps1 -Path build/release-static/Makine-Launcher.exe

# Belirli bir dosya için
just sign-file build/release-static/Makine-Launcher.exe

# Tüm build çıktılarını otomatik tara ve imzala
just sign
```

`scripts/sign_exe.ps1` otomatik olarak:
- `signtool.exe` lokasyonunu Windows SDK'dan bulur
- `scripts/certs/thumbprint.txt` den sertifika parmak izini okur
- SHA-256 digest + DigiCert timestamp kullanır
- İmzayı doğrular

**`scripts/certs/` repoya commit edilmez.**

---

## 8. SHA-256 Hash Üretimi

Build sırasında otomatik olarak üretilir (post-build command):
```
build/release-static/Makine-Launcher.exe.sha256
```

Manuel üretim:
```bash
# PowerShell
Get-FileHash dist-static\Makine-Launcher.exe -Algorithm SHA256 | Select-Object Hash

# bash
sha256sum dist-static/Makine-Launcher.exe
```

Release notlarına ve GitHub release'e hash eklenmeli.

---

## 9. Release Notes Şablonu (CHANGELOG)

CHANGELOG.md için giriş formatı:

```markdown
## [0.x.0] — YYYY-MM-DD

### Yeni Özellikler
- feat(ui): ...
- feat(core): ...

### Düzeltmeler
- fix(ui): ...
- fix(core): ...

### Altyapı
- build: ...
- ci: ...

### SHA-256
Makine-Launcher-0.x.0.exe: `<hash>`
```

Conventional commit tipleri: `feat` `fix` `refactor` `build` `ci` `docs` `test` `chore`
Scopelar: `core` `ui` `build` `ci` `docs`

---

## 10. Release Öncesi Kontrol Listesi

```
[ ] 1. Versiyon güncellendi
      CMakeLists.txt → VERSION x.y.z, SUFFIX değeri
      vcpkg.json → "version": "x.y.z"

[ ] 2. CHANGELOG güncellendi
      Yeni versiyon başlığı ve tüm değişiklikler eklendi

[ ] 3. Build geçiyor
      just release-static    (static path)
      just release           (shared path)

[ ] 4. Testler geçiyor
      just test

[ ] 5. Gizli dosyalar repoda yok
      git status → encryption_key.h, .env, *.key, scripts/certs/ görünmemeli
      git diff --cached → aynı kontrol

[ ] 6. İmzalama yapıldı
      just release-signed
      signtool verify /pa /v build/release-static/Makine-Launcher.exe

[ ] 7. SHA-256 hash üretildi
      build/release-static/Makine-Launcher.exe.sha256 mevcut
      Hash release notlarına kopyalandı

[ ] 8. windeployqt çıktısı kontrol edildi (shared build ise)
      dist/ altındaki DLL'ler eksiksiz
      QML plugin'leri kopyalandı
      Temiz bir Windows makinesinde test edildi

[ ] 9. Paket arşivi oluşturuldu
      just package-static    → Makine-Launcher-static.zip
      just package           → Makine-Launcher-release.zip (shared)

[ ] 10. GitHub release oluşturuldu
       Tag: vX.Y.Z
       EXE ve zip eklendi
       SHA-256 hash release body'de
```

---

## 11. Hızlı Başvuru: Komutlar

| Amaç | Komut |
|------|-------|
| vcpkg bağımlılıkları kur | `just setup` |
| Static Qt kur (bir kerelik) | `just setup-static-qt` |
| Static EXE build | `just release-static` |
| Static EXE + imzala | `just release-signed` |
| Static paketi arşivle | `just package-static` |
| Shared build + deploy | `just deploy` |
| Shared build arşivle | `just package` |
| Tüm EXE'leri imzala | `just sign` |
| Sertifika oluştur | `just setup-cert` |
| Build dizinlerini temizle | `just clean` |
| Testleri çalıştır | `just test` |

---

## 12. Bilinen Limitasyonlar (Alpha)

- `release-static` preset için `x64-mingw-static` vcpkg triplet'i kurulmuş olmalı: `vcpkg install <pkg>:x64-mingw-static`
- `release` preset MSVC gerektirir (`Qt6_DIR` ortam değişkeni ayarlanmalı)
- Sentry crash reporting MinGW'de `breakpad` backend kullanır (crashpad yok)
- MSIX/NSIS installer desteği yok — dağıtım zip arşivi veya naked EXE ile
- UPX sıkıştırma opsiyonel: `cmake -DMAKINE_UPX=ON --preset release-static`

---

*Son güncelleme: 2026-03-12*
