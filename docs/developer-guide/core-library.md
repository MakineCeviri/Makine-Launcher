# Core Kütüphane

Makine-Launcher C++ Core kütüphanesinin (`core/`, `makine_core` statik kütüphanesi) açıklaması.

> **Not:** Core, vcpkg'li preset'lerde (`dev`, `debug`, `release-mingw`, `release`)
> EXE'ye statik olarak linklenir. `dev-ui` build'de Core linklenmez; yalnızca başlıkları
> (eklenti API'si, paket kataloğu tipleri) kullanılır, `OperationJournal` ve
> `IntegrityService` kendi Qt yedek kodlarına düşer.
>
> 0.2.0'da launcher'ın hiç çağırmadığı modüller (oyun algılayıcı, yama motoru, SQLite
> veritabanı, runtime/güvenlik yöneticisi, SSL pinning, `Core` singleton'ı...) silindi.
> Oyun tarama, VDF ayrıştırma ve kurulum servis katmanındadır (`qml/src/services/`).

---

## Genel Bakış

Core, Qt'ye bağımlı olmayan saf C++23 iş mantığıdır: üç modül ve bunların ortak başlıkları.

- Result-based error handling (ADR-0002) — `Result<T> = std::expected<T, Error>`
- Bağımlılıklar: OpenSSL (`Crypto`), nlohmann-json, spdlog
- Singleton/`initialize()` yok; servisler sınıfları doğrudan oluşturur

---

## Modül Yapısı

```
core/
├── include/makine/
│   ├── package_catalog.hpp   # Çeviri paketi kataloğu
│   ├── crash_recovery.hpp    # Kurulum/kaldırma çökme kurtarma günlüğü
│   ├── file_integrity.hpp    # SHA-256 dosya doğrulama
│   ├── path_utils.hpp        # Yol normalizasyonu, traversal kontrolü (header-only)
│   ├── error.hpp             # ErrorCode, Error, Result<T>
│   ├── logging.hpp           # spdlog üstünde MAKINE_LOG_* makroları
│   ├── types/common.hpp      # fs alias, ByteBuffer, ProgressCallback, CancellationToken, Version
│   └── plugin/
│       ├── plugin_api.h      # Eklenti DLL fonksiyon imzaları (C ABI)
│       └── plugin_types.h    # MakineError, MakinePluginInfo
│
├── src/
│   ├── package_catalog/package_catalog.cpp
│   ├── crash_recovery/crash_recovery.cpp
│   └── file_integrity/file_integrity.cpp
│
└── tests/                    # GTest → makine_tests (231 test)
```

---

## Modüller

### PackageCatalog (`makine::packages`)

Çeviri paketi kataloğu: `index.json`'u yükler, `packages/{appId}.json` detayını sonradan
birleştirir, kurulu paket durumunu (`installed_packages.json`) tutar.

```cpp
#include <makine/package_catalog.hpp>

makine::packages::PackageCatalog catalog;
catalog.loadFromIndex(indexPath, packageCacheRoot);   // başlangıç: hafif katalog
catalog.enrichPackage("1245620", detailJson);         // talep üzerine: kurulum adımları vb.

auto appId = catalog.resolveGameId("epic_abc123");    // Epic/GOG kimliği → Steam AppID
auto pkg   = catalog.getPackage(appId);               // std::optional<PackageCatalogEntry>
```

Ayrıca: klasör adı eşleştirme (`findMatchingAppId`), parmak izi eşleştirme
(`findMatchingGames`), varyantlar, paket dosya listesi, exe → AppID haritası
(`getAllExeMap`). Tüm public metotlar `shared_mutex` ile korunur.

**Çağıran:** `LocalPackageManager` (`m_catalog` üyesi); `CoreBridge`, `ProcessScanner`,
`GameService` ona `LocalPackageManager`/`CoreBridge` üzerinden ulaşır.

### CrashRecoveryJournal (`makine::recovery`)

İşlem başlamadan önce diske JSON günlüğü (`pending_operation.json`) yazar. Uygulama
işlem ortasında çökerse sonraki açılışta günlük bulunur ve `recover()` işlem türüne
göre yarım kalan durumu toparlar (ör. yarım kurulumu yedekten geri alır). Başarısız kurtarmada günlük
`pending_operation.failed.json` olarak kenara alınır.

```cpp
#include <makine/crash_recovery.hpp>

makine::recovery::CrashRecoveryJournal journal(dataDir);
journal.beginOperation({.type = makine::recovery::OperationType::Install,
                        .gameId = "123", .gamePath = "/game"});
journal.recordFileModified("data/localization.pak");
journal.commitOperation();   // günlüğü siler
```

**Çağıran:** `OperationJournal` (`qml/src/services/operationjournal.cpp`).

### File Integrity (`makine::integrity`)

OpenSSL ile parça parça SHA-256 hesaplar ve `<dosya>.sha256` yan dosyasıyla sabit
zamanlı karşılaştırır: `computeFileHash`, `readHashFile`, `verifyFile`,
`secureCompareHex`, `isValidSha256Hex`.

**Çağıran:** `IntegrityService` — EXE'yi doğrular; yalnızca `MAKINE_RELEASE_VERIFIED`
build'lerde çalışır, `.sha256` dosyası yoksa kontrol atlanır.

### Ortak Başlıklar

| Başlık | İçerik | Kullanan |
|--------|--------|----------|
| `path_utils.hpp` | `normalize`, `isContainedIn`, `containsTraversalPattern`, `safeJoin`, Windows ad/uzunluk kontrolleri | `CatalogStore`, `DetailFetcher` (appId doğrulama) |
| `error.hpp` | `ErrorCode`, `Error`, `Result<T>`, `VoidResult`, `MAKINE_TRY` | Üç modül |
| `logging.hpp` | `MAKINE_LOG_*`, `MAKINE_TIMED_SCOPE` | Üç modül |
| `types/common.hpp` | Ortak tipler | `crash_recovery`, `file_integrity` |
| `plugin/plugin_api.h` | Eklenti DLL'lerinin dışa aktardığı fonksiyon tipleri | `PluginManager` |

---

## Hata Yönetimi

### Result<T>

```cpp
auto result = makine::integrity::verifyFile(exePath);
if (!result) {
    if (result.error().code() == makine::ErrorCode::FileNotFound) {
        // .sha256 yok — dev build
    }
    return;
}
bool matches = *result;
```

---

## Loglama

Core `MAKINE_LOG_*` makrolarıyla spdlog'un varsayılan logger'ına yazar. Bu logger
`qml/src/services/corebridge.cpp` içindeki `startCoreLogging()` ile ilk taramada bir kez
kurulur ve `logs/makine.log` dosyasına (ve stdout'a) yazar.

---

## Testler

`core/tests/` — `test_error`, `test_path_utils`, `test_file_integrity`,
`test_crash_recovery`, `test_package_catalog` (+ `test_main`) → `makine_tests`.

```bash
just test-core   # core preset + ctest --preset core-tests
```

---

## Sonraki Adımlar

- [QML Arayüz](qml-frontend.md)
- [Build Sistemi](build-system.md)
- [Test Yazma](testing.md)
