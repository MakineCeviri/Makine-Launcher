# Core Kütüphane

Makine-Launcher C++ Core kütüphanesinin detaylı açıklaması.

> **Not:** Core kütüphane `dev` (MinGW+vcpkg) ve `release` (MSVC+vcpkg) build'lerde
> UI'ya entegre çalışır. `dev-ui` build'de Core devre dışıdır, saf Qt servisleri kullanılır.

---

## Genel Bakış

Core kütüphane, Makine-Launcher'ın ileri iş mantığı içeren C++ kütüphanesidir.

**Özellikler:**
- Modern C++20/23
- Header-only optional libraries
- Result-based error handling (ADR-0002)
- Asenkron operasyon desteği

---

## Modül Yapısı

```
core/
├── include/makine/
│   ├── core.hpp           # Ana header
│   ├── types.hpp          # Tip tanimlari
│   ├── error.hpp          # Hata tipleri
│   ├── features.hpp       # Ozellik tanimlari
│   │
│   ├── logging.hpp        # spdlog wrapper
│   ├── config.hpp         # Konfigurasyon
│   ├── metrics.hpp        # Performans metrikleri
│   ├── health.hpp         # Sistem sagligi
│   ├── audit.hpp          # Guvenlik loglama
│   ├── debug.hpp          # Debug/crash dump
│   ├── cache.hpp          # LRU/TTL cache
│   ├── async.hpp          # Asenkron islemler
│   │
│   ├── database.hpp       # SQLite veritabani
│   ├── asset_parser.hpp   # Dosya analizi
│   ├── patch_engine.hpp   # Patch uygulama
│   ├── game_detector.hpp  # Oyun tespiti
│   ├── package_manager.hpp # Paket yonetimi
│   ├── package_catalog.hpp # Ceviri paketi katalogu
│   ├── version_tracker.hpp # Versiyon takibi
│   ├── security.hpp       # Imza dogrulama
│   ├── ssl_pinning.hpp    # SSL pinning
│   └── sandbox.hpp        # Sandbox islemleri (build disi)
│
└── src/
    ├── game_detector/     # Steam, Epic, GOG tarama
    ├── asset_parser/      # Dosya format analizi
    ├── patch_engine/      # Patch uygulama/rollback
    ├── package_manager/   # Paket yonetimi
    ├── version_tracker/   # Oyun versiyon takibi
    ├── security/          # Imza, credential, sandbox
    ├── database/          # SQLite islemleri
    └── platform/          # Platform-specific kod
```

---

## Temel Sınıflar

### Core (Singleton)

Ana giriş noktası:

```cpp
#include <makine/core.hpp>

auto& core = makine::Core::instance();
auto result = core.initialize();

if (result) {
    auto games = core.gameDetector().scanAll();
}

core.shutdown();
```

### GameDetector

Oyun tespiti:

```cpp
auto& detector = core.gameDetector();

// Tum platformlari tara
auto games = detector.scanAll();

// Spesifik platform
auto steamGames = detector.scanSteam();
auto epicGames = detector.scanEpic();

// Tek oyun tespit
auto result = detector.detect("/path/to/game");
```

### AssetParser

Dosya analizi ve motor tespiti:

```cpp
auto& parser = core.assetParser();

// Motor tespit
auto engine = parser.detectEngine(gamePath);
// EngineType::Unity, EngineType::Unreal, etc.

// Dosya analizi
auto assets = parser.parseAssets(gamePath, engine);
```

### PatchEngine

Patch uygulama:

```cpp
auto& patcher = core.patchEngine();

// Patch uygula
PatchOptions options;
options.createBackup = true;
auto result = patcher.apply(game, translationPkg, options);

// Geri al
patcher.rollback(game);
```

### PackageManager

Çeviri paketi yönetimi:

```cpp
auto& pkgMgr = core.packageManager();

// Paket listele
auto packages = pkgMgr.listAvailable(game.id);

// Paket dogrula
auto valid = pkgMgr.verify(localPath);
```

---

## Hata Yönetimi

### Result<T>

```cpp
Result<GameInfo> detect(const std::string& path) {
    if (!fs::exists(path)) {
        return Error{ErrorCode::NotFound, "Path not found"};
    }
    return GameInfo{...};
}

// Kullanim
auto result = detect(path);
if (!result) {
    logger()->error("{}", result.error().message());
    return;
}
auto game = *result;
```

---

## Konfigürasyon

```cpp
CoreConfig config;
config.dataDir = "C:/MakineLauncher/data";
config.cacheDir = "C:/MakineLauncher/cache";
config.logLevel = LogLevel::Info;

core.initialize(config);
```

---

## Sonraki Adımlar

- [QML Arayüz](qml-frontend.md)
- [Build Sistemi](build-system.md)
- [Test Yazma](testing.md)
