# Core API Referansi

MakineAI C++ Core kutuphanesi API referansi.

---

## Namespace

Tum API `makineai` namespace'i altindadir:

```cpp
#include <makineai/core.hpp>

using namespace makineai;
```

---

## Core Sinifi

Ana giris noktasi (Singleton):

```cpp
class Core {
public:
    static Core& instance();

    // Yasam dongusu
    Result<InitResult> initialize(
        const CoreConfig& config = CoreConfig::getDefaults(),
        const InitOptions& options = {}
    );
    void shutdown();
    bool isInitialized() const noexcept;

    // Moduller
    GameDetector& gameDetector();
    AssetParser& assetParser();
    PatchEngine& patchEngine();
    PackageManager& packageManager();
    RuntimeManager& runtimeManager();
    SecurityManager& securityManager();

    // Altyapi
    ConfigManager& configManager() noexcept;
    Metrics& metrics() noexcept;
    HealthChecker& healthChecker() noexcept;
    AuditLogger& auditLogger() noexcept;
    CacheManager& caches() noexcept;

    // Ceviri servisleri
    TranslationMemory& translationMemory();
    GlossaryService& glossaryService();
    QAService& qaService();
    TranslationPipeline& translationPipeline();

    // Asenkron
    AsyncOperation<std::vector<GameInfo>> scanGamesAsync(ProgressCallback progress = nullptr);
    AsyncOperation<PatchResult> applyTranslationAsync(
        const GameInfo& game,
        const std::string& packageId,
        ProgressCallback progress = nullptr
    );

    // Utility
    static constexpr std::string_view version() noexcept;
};
```

---

## Tipler

### GameInfo

```cpp
struct GameInfo {
    std::string id;          // Benzersiz ID
    std::string name;        // Oyun adi
    std::string path;        // Kurulum yolu
    EngineType engine;       // Motor tipi
    Platform platform;       // Steam, Epic, GOG
    std::string version;     // Oyun versiyonu
    bool hasTranslation;     // Ceviri mevcut mu
    std::string coverImage;  // Kapak resmi URL
};
```

### EngineType

```cpp
enum class EngineType {
    Unknown = 0,
    Unity,
    Unreal,
    RpgMaker,
    RenPy,
    GameMaker,
    Bethesda,
    Godot,
    Source,
    Custom
};
```

### Platform

```cpp
enum class Platform {
    Unknown = 0,
    Steam,
    Epic,
    GOG,
    Manual
};
```

### PatchResult

```cpp
struct PatchResult {
    bool success;
    std::string message;
    std::string backupPath;
    std::vector<std::string> modifiedFiles;
    std::chrono::milliseconds duration;
};
```

---

## GameDetector

```cpp
class GameDetector {
public:
    // Tum platformlari tara
    std::vector<GameInfo> scanAll();

    // Spesifik platform
    std::vector<GameInfo> scanSteam();
    std::vector<GameInfo> scanEpic();
    std::vector<GameInfo> scanGOG();

    // Tek oyun tespit
    Result<GameInfo> detect(const std::string& path);

    // Calisanan oyun tespiti
    std::optional<GameInfo> detectRunningGame();
};
```

---

## AssetParser

```cpp
class AssetParser {
public:
    // Motor tespiti
    Result<EngineType> detectEngine(const std::string& gamePath);

    // Asset analizi
    Result<std::vector<AssetInfo>> parseAssets(
        const std::string& gamePath,
        EngineType engine
    );

    // Handler al
    IEngineHandler* getHandler(EngineType engine);
};
```

---

## PatchEngine

```cpp
class PatchEngine {
public:
    // Patch uygula
    Result<PatchResult> apply(
        const GameInfo& game,
        const TranslationPackage& package,
        const PatchOptions& options = {}
    );

    // Geri al
    Result<void> rollback(const GameInfo& game);

    // Yedek yonetimi
    std::vector<BackupInfo> listBackups(const std::string& gameId);
    Result<void> deleteBackup(const std::string& backupPath);
};

struct PatchOptions {
    bool createBackup = true;
    bool verifyAfterPatch = true;
    bool preserveTimestamps = false;
};
```

---

## PackageManager

```cpp
class PackageManager {
public:
    // Paket listele
    std::vector<PackageInfo> listAvailable(const std::string& gameId);

    // Paket indir
    AsyncOperation<std::string> download(
        const std::string& packageId,
        ProgressCallback progress = nullptr
    );

    // Paket dogrula
    Result<bool> verify(const std::string& packagePath);

    // Yerel paketler
    std::vector<PackageInfo> listLocal();
};

struct PackageInfo {
    std::string id;
    std::string gameId;
    std::string version;
    std::string author;
    QualityLevel quality;  // Draft, Beta, Verified, Official
    size_t size;
    std::string checksum;
};
```

---

## Result<T>

Hata yonetimi icin:

```cpp
template<typename T>
class Result {
public:
    // Kontrol
    bool success() const;
    explicit operator bool() const;

    // Deger erisimi
    T& value();
    const T& value() const;
    T& operator*();

    // Hata erisimi
    Error& error();
    const Error& error() const;

    // Fonksiyonel
    template<typename F>
    auto map(F&& f) -> Result<std::invoke_result_t<F, T>>;

    template<typename F>
    Result<T>& onSuccess(F&& f);

    template<typename F>
    Result<T>& onError(F&& f);
};
```

---

## Error

```cpp
struct Error {
    ErrorCode code;
    std::string message;
    std::string details;
    std::source_location location;
};

enum class ErrorCode {
    Success = 0,
    NotFound,
    AccessDenied,
    InvalidFormat,
    InvalidSignature,
    NetworkError,
    DatabaseError,
    Cancelled,
    Timeout,
    Unknown = 999
};
```

---

## AsyncOperation<T>

```cpp
template<typename T>
class AsyncOperation {
public:
    // Durum
    bool isComplete() const;
    bool isCancelled() const;
    float progress() const;

    // Bekle
    T get();  // Bloklayan
    std::future<T> future();

    // Callback'ler
    AsyncOperation& then(std::function<void(T)> onSuccess);
    AsyncOperation& onError(std::function<void(Error)> onError);
    AsyncOperation& onProgress(std::function<void(float)> onProgress);

    // Iptal
    void cancel();
};
```

---

## Callback Tipleri

```cpp
using ProgressCallback = std::function<void(float progress, const std::string& message)>;
using CompletionCallback = std::function<void(bool success)>;
```

---

## Konfigürasyon

### CoreConfig

```cpp
struct CoreConfig {
    std::string dataDir;         // Veri dizini
    std::string cacheDir;        // Onbellek dizini
    std::string logDir;          // Log dizini
    LogLevel logLevel;           // Log seviyesi
    size_t maxCacheSize;         // Max onbellek boyutu
    bool enableMetrics;          // Metrik toplama
    bool enableAuditLog;         // Guvenlik logu

    static CoreConfig getDefaults();
};
```

### InitOptions

```cpp
struct InitOptions {
    bool skipHealthCheck = false;
    bool skipDatabaseInit = false;
    bool enableMetrics = true;
    bool enableAuditLog = true;
    bool enableDebugDumps = false;
    bool verboseLogging = false;
};
```

---

## Ornek Kullanim

```cpp
#include <makineai/core.hpp>
#include <iostream>

int main() {
    auto& core = makineai::Core::instance();

    // Initialize
    auto initResult = core.initialize();
    if (!initResult) {
        std::cerr << "Init failed: " << initResult.error().message() << std::endl;
        return 1;
    }

    // Scan games
    auto games = core.gameDetector().scanAll();
    std::cout << "Found " << games.size() << " games" << std::endl;

    for (const auto& game : games) {
        std::cout << "- " << game.name << " (" << game.path << ")" << std::endl;
    }

    // Cleanup
    core.shutdown();
    return 0;
}
```
