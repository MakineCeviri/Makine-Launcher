# Changelog

Tüm önemli değişiklikler bu dosyada belgelenecektir.

Format [Keep a Changelog](https://keepachangelog.com/tr/1.0.0/) standardına,
sürümleme [Semantic Versioning](https://semver.org/lang/tr/) standardına uygundur.

## [Unreleased]

### Eklenen (2026-03-12)
- **DPI-aware UI scaling** — 3 seçenekli ölçek ayarı (Kompakt/Otomatik/Büyük), Win32 native DPI algılama, küçük ekran adaptasyonu (#112 kısmi)
- **Discord butonu** — Duyuru kartında hover animasyonlu Discord bağlantısı
- **Release pipeline** — GitHub Actions 2 aşamalı CI/CD (build+sign → deploy), PFX sertifika desteği
- **justfile release recipe'leri** — `just publish <version>`, `just publish-final`, `just publish-status`

### Düzeltilen (2026-03-12)
- **Oyun taşıma yedek koruması** — 4 katmanlı koruma: backup skip guard, installPath koruma, path migration, scan güncelleme
- **Restore hedef doğrulama** — Yedek geri yükleme öncesi hedef dizin kontrolü
- **saveCachedGames exception safety** — Background thread try/catch sarmalama
- **Release readiness** — Exception safety iyileştirmeleri, hızlı düzeltmeler

### İyileştirilen (2026-03-12)
- **Kategorize logging** — 186 qDebug/qWarning/qCritical → qCDebug/qCWarning/qCCritical (15 dosya, 15 kategori)
- **fmt::format migration** — Tüm std::to_string + string concat → fmt::format (sıfır kaldı)
- **RAII wrappers** — OpenSSL (EVP_PKEY, BIO, EVP_MD_CTX) + SQLite errMsg için unique_ptr sarmalayıcılar
- **ScopedMetrics pattern** — Database + PatchEngine'de metrics boilerplate eliminasyonu
- **main.cpp decompose** — 1377 satırlık init kodu organize helper fonksiyonlara ayrıldı
- **Code duplication** — LocalPackageManager (-299), GameService (-80), PackageCatalog (-53) satır azaltma
- **ProcessScanner** — toLower→CaseInsensitive, sistem process filtresi konsolidasyonu
- **TranslationDownloader** — fail() lambda ile 7 duplicated error-bailout bloğu birleştirildi

### Kaldırılan (2026-03-12)
- **Ed25519 imza doğrulama** — Paket indirmede .sig dosya sistemi tamamen kaldırıldı, AES-256-GCM auth tag yeterli
- **sign_packages.py** — Ed25519 imzalama scripti silindi (543 satır)
- **R2 .sig dosyaları** — CDN'den 260 .sig dosyası temizlendi
- **BepInEx/XUnity** — Tüm runtime manager kodu (1132→18 satır), tüm referanslar, test suite
- **Deferred features** — Translation Memory, Glossary, QA Service, Translation Pipeline stub'ları
- **Dead code** — pak/ legacy, findExtractedSubdir, ManifestSyncService bağımlılığı (TranslationDownloader)
- **11 stale worktree branch** temizlendi

### İyileştirilen (2026-02-18)
- **Güvenlik denetimi** — 14 bulgu düzeltildi (path traversal, input validation, SSL pinning)
- **Modülerlik** — GameService ↔ UpdateDetectionService decoupling (nullptr güvenliği)
- **Header organizasyonu** — İç header'lar `detail/` dizinine taşındı (string_pool, debug, mio_utils)
- **Kod minimizasyonu** — 18 gereksiz yorum kaldırıldı (main.cpp, gameservice.cpp)
- **Qt Quick performans** — Image sourceSize optimizasyonu, gereksiz clip kaldırma
- **Kullanılmayan import** — HomePage.qml'den `QtQuick.Effects` kaldırıldı
- **Erişilebilirlik** — ProjectShowcaseCard boş key handler'lar düzeltildi
- **Q_UNUSED düzeltmesi** — installRuntime/uninstallRuntime lambda capture uyumsuzluğu

### Kaldırılan (2026-02-14 Temizlik)
- GameListModel (cpp+h), makinaeiMetatypes.h, Badge.qml, InfoRow.qml, 12 SVG ikon
- GameService: 9 dead Q_INVOKABLE, 6 dead Q_PROPERTY, 3 dead signal
- CoreBridge: 5 dead signal, 2 unused include
- Dimensions.qml: 76 dead property (302→~180 satır)
- DebugHelper.qml: 7 dead fonksiyon (196→30 satır)
- Dead import: QtQuick.Effects (DropZoneOverlay, SkeletonLoader)
- Dead signal: steamStoreClicked, restoreClicked (GameDetailScreen)

### Kaldirilan (2026-02-12 Temizlik)
- **~32K satir dead code** kaldirildi
- Engine handler'lar: UnityHandler, UnrealHandler, RPGMakerHandler, RenPyHandler, GameMakerHandler (~6800 satir)
- TranslationMemory, GlossaryService, QAService, TranslationPipeline servisleri
- StringClassifier, FontManager, RecipeLoader
- 13 olu QML bileseni: SplashScreen, SplashWindow, TitleBar, DevButton, WindowButton, GradientText, TranslationProgressBar, BackupListItem, EmptyStateMessage, MakineLogo, GameDetectorDialog, ManualFolderDialog, CedraInteractiveCard
- Eski ayar sayfalari: GeneralSettings, TranslationSettings, ProjectsSettings, PerformanceSettings, AboutSettings, DeveloperSettings
- TranslationModel ve TranslationService QML siniflari
- Handler testleri, fuzz testleri, handler ornekleri
- Gecersiz dokumanlar (ADR-0003, ADR-0005, core-api, qml-types, 4 kullanici rehberi)
- Gereksiz recipe dosyalari (gamemaker-generic.yaml, renpy-generic.yaml, unity-il2cpp-generic.yaml)

### Duzeltilen (2026-02-12)
- D2R fallback mapping hatasi: 990080 (Hogwarts Legacy) → 1293830 (Diablo II: Resurrected)
- Vulkan probe UB: `PFN_vkCreateInstance` donus tipi `void*` → `int` (VkResult)
- Islevsiz NVIDIA overlay env vars kaldirildi
- Kullanilmayan `qtMessageHandler` fonksiyonu kaldirildi
- Kullanilmayan `createBackupAsync()` metodu kaldirildi
- Sahte `RecentFirst` sort order kaldirildi (aslinda NameAsc yapiyordu)
- Build: gereksiz `Version` lib, `qtquick3d` modulu, silinen QML dosya referanslari temizlendi

### Eklenen
- **Gercek oyun tarama** — Steam (Registry+VDF+ACF), Epic (Manifest JSON), GOG (Registry)
- **Motor tespiti** — Dosya imzalariyla Unity, Unreal, Bethesda, Ren'Py, RPGMaker, GameMaker, Godot, Source
- **Anti-cheat tespiti** — EAC, BattlEye, Vanguard dosya/dizin kontrolu
- **LocalPackageManager** — Yerel ceviri paketi tarama, kurulum, kaldirma
- **VDF Parser** — Header-only Steam config parser (UI_ONLY build icin)
- **Async yedekleme** — QtConcurrent ile arka plan yedekleme + progress
- **Sistem tepsisi servis modu** — Minimize'da azaltilmis kaynak kullanimi
- **Lazy QML yukleme** — Settings sayfasi talep uzerine yukleniyor
- **Visibility-aware timers** — Pencere minimize'da ProcessScanner intervali 3s→30s
- **Ceviri veri yolu ayari** — SettingsManager'da translationDataPath property'si
- `.editorconfig` - Editörler arası tutarlı kodlama standartları
- `vcpkg-configuration.json` - Tekrarlanabilir bağımlılık yapıları (baseline pinleme)
- `.clang-tidy` - Statik analiz yapılandırması (C++23, proje-uyumlu kontroller)
- `.github/workflows/auto-assign.yml` - Yeni issue'lar otomatik atanıyor
- `.github/FUNDING.yml` - GitHub sponsor butonu
- GitHub Issues ile kapsamlı roadmap takibi (#12-#28)
- Integration test build target (`makine_integration_tests`)
- `justfile` yeni recipe'ler: `format`, `check-format`, `lint`, `check`, `stats`, `info`, `docs`
- 20+ yeni GitHub label (type/scope/status/engine kategorileri)
- Genişletilmiş PR auto-labeling kuralları

### Değiştirilen
- **Proje yonu:** "String extraction + handler pipeline" → "Ceviri dagitim + adaptasyon motoru"
- **VISION.md:** Gaming Companion AI → Oyun guncelleme adaptasyonu
- **ROADMAP.md:** Core entegrasyonu odagi → Makine (urun) + MakineAI (motor) ayrimi
- **README.md:** Guncellenmis proje tanimi ve ozellik listesi
- **architecture.md:** Iki katmanli yapi (Makine + MakineAI)
- **docs/index.md:** Yeni yapi ve vizyon yansitildi
- CoreBridge UI_ONLY: Sahte veriler → gercek Steam/Epic/GOG tarama
- `docs/ROADMAP.md` güncel durum ve issue bağlantılarıyla yenilendi
- CI workflow: `permissions`, `concurrency`, paralel static analysis
- CI workflow: cppcheck `--std=c++23`, `-j $(nproc)`, `--inline-suppr`
- CodeQL workflow: concurrency group eklendi
- Release workflow: `permissions: contents: write`, cache key v2
- `README.md`: C++20 badge → C++23, vcpkg bağımlılık sayısı
- `CONTRIBUTING.md`: C++20 standardı → C++23
- `justfile`: Windows/PowerShell uyumlu komutlar (Unix `find/xargs` kaldırıldı)
- Test CMakeLists: 7 eksik test dosyası build'e dahil edildi
- Test yapısı: Unit ve integration testler ayrı executable'lara bölündü

### Düzeltilen
- `main.cpp`: Hardcoded log yolu (`C:/cedra/...`) → `QStandardPaths` (portabilite) (#27)
- Flutter referans kalıntıları QML dosyalarından temizlendi
- Gereksiz yorum ve dead code kaldırıldı
- Core modüllerinde tüm TODO öğeleri implemente edildi
- CI/CD pipeline'da vcpkg entegrasyonu iyileştirildi
- CodeQL workflow vcpkg cache sorunu çözüldü
- Release workflow `lukka/run-vcpkg` yerine built-in vcpkg kullanılıyor

---

## [0.1.0-alpha] - 2026-02-03

### Eklenen
- **Native Qt6/QML UI** - Flutter'dan geçiş tamamlandı
- **F12 Screenshot** - Oyun anı yakalama (Gaming Companion AI altyapısı)
- **VISION.md** - Uygulamanın nihai hedefi belgelendi
- **ROADMAP.md** - Geliştirme yol haritası
- **DevOps Automation** - CI/CD, issue templates, code quality
- **C++ Core Library** - Tam entegre çeviri motoru
- **Oyun Motorları Desteği:**
  - Unity (IL2CPP + Mono)
  - Unreal Engine 4/5
  - RPG Maker MV/MZ
  - Ren'Py
  - GameMaker Studio 2
- **Oyun Tarayıcıları:**
  - Steam
  - Epic Games
  - GOG Galaxy
  - Manuel ekleme
- **Translation Pipeline** - Akıllı çeviri karar motoru
- **GPU Optimizasyonları** - Görünmeyen animasyonlar durduruldu
- **Performance Monitor** - F3 ile FPS overlay
- **30+ UI Bileşeni** - Native Qt tasarım sistemi
- **vcpkg Entegrasyonu** - 18 C++ bağımlılık
- **CMake Presets** - Modern build konfigürasyonu

### Değiştirilen
- Mimari: Flutter + FFI → Qt6/QML + Native C++
- Build sistemi: Manuel → CMake Presets + just
- UI framework: FluentUI → Native Qt Components

### Kaldırılan
- Flutter codebase (archived)
- FFI bridge layer
- FluentUI dependency

### Güvenlik
- Path traversal koruması eklendi
- Input validation güçlendirildi
- libsodium entegrasyonu (BLAKE2b, secure random)

---

## [0.0.8] - 2026-01-15 [Archived]

### Not
Bu sürüm Flutter-based eski mimariye aittir ve artık desteklenmemektedir.
Yeni sürümler için v0.1.0+ kullanın.

---

## Sürüm Karşılaştırması

| Özellik | v0.0.x (Flutter) | v0.1.x (Qt Native) |
|---------|------------------|---------------------|
| UI Framework | Flutter | Qt6/QML |
| Core Binding | FFI | Direct C++ |
| Build Size | ~50MB | ~15MB |
| Startup Time | ~3s | <1s |
| Memory Usage | ~300MB | ~150MB |
| Windows Support | ✅ | ✅ |
| Linux Support | ❌ | Planned |
| macOS Support | ❌ | Planned |

---

[Unreleased]: https://github.com/MakineCeviri/Makine-Launcher/compare/v0.1.0-alpha...HEAD
[0.1.0-alpha]: https://github.com/MakineCeviri/Makine-Launcher/releases/tag/v0.1.0-alpha
[0.0.8]: https://github.com/MakineCeviri/Makine-Launcher/releases/tag/v0.0.8
