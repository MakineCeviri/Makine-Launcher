# MakineAI

Windows oyunlarını Türkçe'ye çeviren ve çevirileri güncel tutan masaüstü uygulaması.

**Durum:** Alpha — oyun tarama ve yerel paket kurulumu çalışıyor. Sunucu dağıtımı ve adaptasyon motoru geliştirme aşamasında.

## Mimari

Proje iki katmandan oluşur:

- **Makine** — Çeviri dağıtım platformu. Oyun tespiti, paket kurulum/kaldırma, katalog yönetimi.
- **MakineAI** — Adaptasyon motoru. Oyun güncelleme tespiti, çeviri uyarlama, otomatik doğrulama. *(geliştiriliyor)*

UI katmanı saf Qt6 QML + C++ backend olarak çalışır. Core kütüphanesi vcpkg bağımlılıkları ile gelişmiş dosya işleme sağlar.

## Gereksinimler

| Araç | Sürüm | Not |
|------|-------|-----|
| Windows | 10/11 x64 | Tek desteklenen platform |
| Qt | 6.10+ | MinGW 13.1.0 veya MSVC 2022 |
| CMake | 3.28+ | Qt Tools ile gelir |
| Ninja | — | Qt Tools ile gelir |
| vcpkg | Güncel | Sadece `release`/`core` preset için |
| [just](https://github.com/casey/just) | — | Opsiyonel, task runner |

## Kurulum ve Build

```bash
git clone https://github.com/jlceaser/MakineAI.git
cd MakineAI
```

### Hızlı Geliştirme (MinGW, UI-only)

```bash
cmake --preset dev
cmake --build --preset dev
./qml/build/dev/MakineAI.exe
```

Veya `just` kuruluysa:

```bash
just run
```

### Debug Build

```bash
cmake --preset debug
cmake --build --preset debug
```

### Full Release (Core + QML, vcpkg gerekli)

```bash
vcpkg install --triplet x64-windows   # ilk seferde
cmake --preset release
cmake --build --preset release
```

### Core Kütüphanesi (tek başına)

```bash
cmake --preset core
cmake --build --preset core
ctest --preset core-tests              # testleri çalıştır
```

## Build Preset'leri

| Preset | Derleyici | Mod | Çıktı Dizini |
|--------|-----------|-----|---------------|
| `dev` | MinGW | Release, UI-only | `qml/build/dev/` |
| `debug` | MinGW | Debug, UI-only | `qml/build/debug/` |
| `release` | MSVC/vcpkg | Full (core+qml) | `build/release/` |
| `core` | MSVC/vcpkg | Core kütüphane | `core/build/` |

`UI-only` mod (`MAKINEAI_UI_ONLY=ON`): Core kütüphanesine bağımlılık olmadan sadece QML arayüzünü ve Qt servislerini derler. Oyun tarama, paket kurulumu ve UI geliştirme bu modda çalışır.

## Proje Yapısı

```
MakineAI/
├── qml/                        # Ana uygulama (Qt6 QML)
│   ├── src/                    # C++ backend
│   │   ├── main.cpp            # Uygulama giriş noktası
│   │   ├── services/           # Backend servisleri
│   │   │   ├── gameservice     # Oyun tespiti ve yönetimi
│   │   │   ├── corebridge      # Platform tarayıcıları (Steam/Epic/GOG)
│   │   │   ├── localpackagemanager  # .mkpkg paket kurulumu
│   │   │   ├── backupmanager   # Dosya yedekleme/geri yükleme
│   │   │   ├── settingsmanager # Uygulama ayarları
│   │   │   ├── processscanner  # Çalışan oyun tespiti (Win32)
│   │   │   ├── systemtraymanager    # Native Win32 system tray
│   │   │   ├── integrityservice     # Binary bütünlük doğrulama
│   │   │   ├── updatedetectionservice # Oyun güncelleme tespiti
│   │   │   └── batchoperationservice  # Toplu işlemler
│   │   └── models/             # QML veri modelleri
│   ├── qml/                    # QML arayüz dosyaları
│   │   ├── Main.qml            # Ana pencere
│   │   ├── HomeScreen.qml      # Ana ekran
│   │   ├── SettingsScreen.qml  # Ayarlar
│   │   ├── GameDetailScreen.qml # Oyun detay
│   │   ├── theme/              # Theme.qml, Dimensions.qml
│   │   ├── components/         # Yeniden kullanılabilir bileşenler
│   │   └── dialogs/            # Dialog pencereleri
│   └── resources/              # İkon, font, görsel
├── core/                       # C++ core kütüphanesi (vcpkg)
│   ├── include/makineai/       # Public API (.hpp)
│   ├── src/                    # Implementasyon
│   │   ├── asset_parser/       # Dosya formatı ayrıştırıcıları
│   │   ├── game_detector/      # Platform tarayıcıları
│   │   ├── package_manager/    # Paket yönetimi
│   │   ├── patch_engine/       # Yama motoru
│   │   ├── security/           # Güvenlik (sandbox, SSL pinning)
│   │   └── ...
│   └── tests/                  # GTest birim testleri
├── docs/                       # Dokümantasyon
├── recipes/                    # Çeviri şablonları (YAML)
├── scripts/                    # Python yardımcı araçları
├── CMakePresets.json            # Build preset tanımları
├── justfile                    # Task runner komutları
└── vcpkg.json                  # Core bağımlılıkları
```

## Servis Mimarisi

QML singleton olarak kaydedilen C++ servisleri:

| Servis | Sorumluluk |
|--------|------------|
| `GameService` | Oyun kütüphanesi yönetimi, tarama, metadata, çeviri kurulumu |
| `CoreBridge` | Platform tarayıcıları (Steam Registry+VDF+ACF, Epic JSON, GOG Registry), motor tespiti |
| `LocalPackageManager` | `.mkpkg` paketlerini ayrıştırma, dosya eşleştirme, kurulum/kaldırma |
| `BackupManager` | Asenkron dosya yedekleme ve geri yükleme |
| `SettingsManager` | Uygulama ayarları (QSettings), pencere geometrisi, tema |
| `ProcessScanner` | Win32 `CreateToolhelp32Snapshot` ile çalışan oyun tespiti |
| `SystemTrayManager` | Native Win32 `Shell_NotifyIconW` system tray |
| `IntegrityService` | Binary bütünlük doğrulama |
| `UpdateDetectionService` | Oyun dosya hash değişiklik tespiti |
| `BatchOperationService` | Toplu paket kurulum/kaldırma |

## Vcpkg Bağımlılıkları (Core)

Core kütüphanesi (`release`/`core` preset) şu bağımlılıkları kullanır:

boost-filesystem, openssl, curl, nlohmann-json, lz4, zlib, zstd, sqlite3, spdlog, bit7z, libarchive, simdjson, efsw, mio, taskflow, concurrentqueue, simdutf, sqlitecpp, libsodium

`dev`/`debug` preset'lerinde vcpkg **gerekmez** — sadece Qt yeterlidir.

## Just Komutları

```bash
just dev            # MinGW UI-only build (hızlı iterasyon)
just debug          # Debug build
just release        # Full release (vcpkg + core + qml)
just core           # Sadece core kütüphanesi
just run            # Build + çalıştır (dev)
just test           # Core testleri çalıştır
just clean          # Build dizinlerini temizle
just format         # clang-format ile kod biçimlendirme
just stats          # Proje istatistikleri
just deploy         # windeployqt ile dağıtım paketi
```

## Dosya İsimlendirme Kuralları

| Katman | Kural | Uzantı | Örnek |
|--------|-------|--------|-------|
| QML | PascalCase | `.qml` | `GameDetailScreen.qml` |
| QML/UI C++ | camelCase | `.h` / `.cpp` | `gameservice.h` |
| Core C++ | snake_case | `.hpp` / `.cpp` | `game_detector.hpp` |

## Dokümantasyon

Detaylı dokümantasyon `docs/` dizininde:

- [Vizyon ve Yol Haritası](docs/VISION.md) — Proje yönü ve hedefler
- [Mimari](docs/developer-guide/architecture.md) — Servis mimarisi ve veri akışı
- [Geliştirme Ortamı](docs/developer-guide/setup.md) — IDE ve araç kurulumu
- [QML Frontend](docs/developer-guide/qml-frontend.md) — UI yapısı ve kurallar
- [Core Kütüphanesi](docs/developer-guide/core-library.md) — Core API ve modüller
- [Servis API](docs/api-reference/services-api.md) — Backend servis referansı
- [Oyun Motorları](docs/game-engines/) — Motor bazında çeviri rehberleri

## Lisans

Proprietary — MakineAI. Tüm hakları saklıdır.

Detaylar için [LICENSE](LICENSE) dosyasına bakın.
