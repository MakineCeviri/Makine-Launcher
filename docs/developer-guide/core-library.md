# Core Kutuphane

MakineAI C++ Core kutuphanesinin detayli aciklamasi.

---

## Genel Bakis

Core kutuphane, MakineAI'nin tum is mantigi iceren C++ kutuphanesidir.

**Ozellikler:**
- Modern C++20/23
- Header-only optional libraries
- Result-based error handling
- Asenkron operasyon destegi

---

## Modul Yapisi

```
core/
├── include/makineai/
│   ├── core.hpp           # Ana header
│   ├── types.hpp          # Tip tanimlari
│   ├── error.hpp          # Hata tipleri
│   ├── features.hpp       # Ozellik tanimlari
│   │
│   ├── logging.hpp        # spdlog wrapper
│   ├── config.hpp         # Konfigürasyon
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
│   ├── runtime_manager.hpp # BepInEx/XUnity
│   ├── security.hpp       # Imza dogrulama
│   │
│   └── handlers/          # Motor handler'lari
│       ├── unity_handler.hpp
│       ├── unreal_handler.hpp
│       └── ...
│
└── src/
    ├── game_detector/     # Implementasyon
    ├── asset_parser/
    ├── patch_engine/
    └── handlers/
```

---

## Temel Siniflar

### Core (Singleton)

Ana giris noktasi:

```cpp
#include <makineai/core.hpp>

auto& core = makineai::Core::instance();
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

Ceviri paketi yonetimi:

```cpp
auto& pkgMgr = core.packageManager();

// Paket listele
auto packages = pkgMgr.listAvailable(game.id);

// Paket indir
auto result = pkgMgr.download(packageId, progress);

// Paket dogrula
auto valid = pkgMgr.verify(localPath);
```

---

## Utility Siniflari

### Lazy<T>

Thread-safe lazy initialization:

```cpp
#include <makineai/utils/lazy.hpp>

Lazy<ExpensiveObject> obj([]{
    return ExpensiveObject();
});

// Ilk erisimde olusturulur
auto& instance = obj.get();
```

### BatchProcessor

Paralel batch isleme:

```cpp
#include <makineai/utils/batch.hpp>

BatchProcessor<FileInfo> processor;
processor.setWorkerCount(4);
processor.setBatchSize(100);

processor.process(files, [](const FileInfo& f) {
    return processFile(f);
});
```

### AuditLogger

Guvenlik loglama:

```cpp
auto& audit = AuditLogger::instance();

audit.log(AuditEvent::PackageInstall, {
    {"package_id", pkgId},
    {"game_id", gameId}
});
```

### Metrics

Performans metrikleri:

```cpp
auto& metrics = Metrics::instance();

metrics.counter("games_scanned").increment();
metrics.histogram("scan_duration").record(duration);

auto text = metrics.toText();
```

### HealthChecker

Sistem sagligi:

```cpp
auto& health = HealthChecker::instance();

health.addCheck("database", [] {
    return db.isConnected();
});

auto status = health.check(); // HealthStatus
```

---

## Hata Yonetimi

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

### Error Kodlari

```cpp
enum class ErrorCode {
    Success = 0,
    NotFound,
    AccessDenied,
    InvalidFormat,
    NetworkError,
    DatabaseError,
    // ...
};
```

---

## Asenkron API

### AsyncOperation

```cpp
AsyncOperation<std::vector<GameInfo>> scanGamesAsync(
    ProgressCallback progress = nullptr
);

// Kullanim
auto op = core.scanGamesAsync();

op.then([](auto games) {
    // Basari
}).onError([](auto error) {
    // Hata
}).onProgress([](float p) {
    // Ilerleme
});

// Veya bekle
auto games = op.get();
```

### AsyncQueue

```cpp
auto& queue = core.taskQueue();

queue.enqueue([]{
    // Arka plan gorevi
});
```

---

## Konfigürasyon

### CoreConfig

```cpp
CoreConfig config;
config.dataDir = "C:/MakineAI/data";
config.cacheDir = "C:/MakineAI/cache";
config.logLevel = LogLevel::Info;
config.maxCacheSize = 500 * 1024 * 1024; // 500 MB

core.initialize(config);
```

### Varsayilan Degerler

```cpp
auto defaults = CoreConfig::getDefaults();
// dataDir = %APPDATA%/MakineAI
// cacheDir = %LOCALAPPDATA%/MakineAI/cache
// logLevel = Info
```

---

## Test Yazma

```cpp
#include <gtest/gtest.h>
#include <makineai/core.hpp>

TEST(GameDetectorTest, DetectUnity) {
    auto& detector = makineai::Core::instance().gameDetector();

    auto result = detector.detect("testdata/unity_game");

    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.value().engine, EngineType::Unity);
}
```

---

## Sonraki Adimlar

- [QML Arayuz](qml-frontend.md)
- [Build Sistemi](build-system.md)
- [Test Yazma](testing.md)
