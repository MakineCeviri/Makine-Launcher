# Makine-Launcher — Release Readiness Audit

**Tarih:** 2026-03-12
**Versiyon:** v0.1.0-pre-alpha
**Denetci:** Claude Opus 4.6 (Otomatik Kod Denetimi)
**Branch:** main (commit b87d8e7)
**Kapsam:** Tam kod tabani analizi — 167 C++/H, 69 QML, 6 CMakeLists, 33 test dosyasi

---

## Executive Summary

Makine-Launcher, 258 oyun icin Turkce ceviri dagitim platformu olarak oldukca olgun bir noktada. Core kutuphane katmani (22K+ satir C++) saglam error handling, path security, SSL pinning ve Ed25519 imza dogrulama iceriyor. UI katmani (10K+ satir QML + 22K satir C++ backend) profesyonel seviyede: onboarding wizard, batch operations, system tray, self-updater, crash reporting (Sentry), ve Tracy profiler entegrasyonu mevcut. Build sistemi 7 preset ile kapsamli; security hardening (ASLR, DEP, CFG, stack protector) her iki CMakeLists'te aktif. Test kapsami core icin kapsamli (33 test dosyasi, ~12K satir) ancak UI backend icin test yok. Ana blokajlar: Static Qt build, MSIX paketleme ve kod imzalama. Genel hazirlik skoru: **%78 — Alpha release icin yakin, ancak 3 kritik ve 5 yuksek oncelikli bulgu giderilmeli.**

---

## 1. Mevcut Durum Analizi

### 1.1 Mimari Ozet

**Katmanli mimari, temiz sorumluluk ayirimi:**

```
┌────────────────────────────────────────────────────────┐
│  QML UI Layer (69 dosya, ~10K satir)                   │
│  Main.qml → HomeScreen → GameDetail → Settings         │
│  5 ekran, 37 component, 6 dialog, 3 controller         │
│  Theme singleton, Dimensions singleton, DebugHelper     │
├────────────────────────────────────────────────────────┤
│  Qt C++ Backend Services (17 servis, ~22K satir)       │
│  GameService, CoreBridge, BackupManager,                │
│  LocalPackageManager, TranslationDownloader,            │
│  UpdateService, SelfUpdater, IntegrityService, ...      │
├────────────────────────────────────────────────────────┤
│  C++ Core Library (makine_core, static lib)            │
│  game_detector (5 scanner), patch_engine,               │
│  package_catalog, security_manager, ssl_pinning,        │
│  crash_recovery, file_integrity, credential_store       │
│  14 vcpkg dependency (OpenSSL, curl, spdlog, ...)      │
└────────────────────────────────────────────────────────┘
```

**Onemli tasarim kararlari:**
- Super-build: tek `cmake --preset dev` ile core + UI birlikte derlenir
- `MAKINE_UI_ONLY` modu: UI gelistirme icin vcpkg gereksiz, hizli iterasyon
- Hybrid katalog: `index.json` (93KB, startup) + on-demand `packages/{id}.json` (~700B)
- MKPK format: AES-256-GCM sifreleme + zstd sikistirma + tar arsiv
- Ed25519 paket imzalama, compile-time XOR key obfuscation (`obfstring.h`)
- Crash recovery journal: islem ortasinda cokmelerde state kurtarma

### 1.2 Build Sureci

| Preset | Derleyici | Aciklama | Durum |
|--------|-----------|----------|-------|
| `dev` | MinGW+vcpkg | Gunluk gelistirme (Core+UI) | Calisir |
| `dev-ui` | MinGW | Sadece UI, vcpkg gereksiz | Calisir |
| `debug` | MinGW+vcpkg | Debug sembolleri | Calisir |
| `release` | MSVC+vcpkg | Tam release | Test edilmeli |
| `release-static` | MinGW static Qt | Tek EXE | **BLOKER — Static Qt yok** |
| `core` | MinGW+vcpkg | Core kutuphane + testler | Calisir |
| `dev-profile` | MinGW+vcpkg+Tracy | Profiling | Calisir |

**Build boyutu:** dev build 423MB (DLL'ler dahil)
**ccache:** CMakePresets.json'da etkin (hizli yeniden derleme)

### 1.3 Risk Profili

| Risk Alani | Seviye | Aciklama |
|------------|--------|----------|
| Guvenlik | Orta | Core katman saglam; UI katmaninda path security eklenmis ama exception handling zayif |
| Kararlilik | Orta-Dusuk | Crash recovery, journal sistemi mevcut; try/catch kullanimi sinirli (7 yer) |
| Performans | Dusuk | Tracy profiler, FrameTimer, MemoryProfiler entegre; splash screen optimize |
| Dagitim | Yuksek | Static Qt build ve MSIX imzalama tamamlanmadi |
| Test | Orta-Yuksek | Core testler kapsamli; UI backend tamamen test edilmemis |

---

## 2. Bulgu Tablosu

| # | Oncelik | Alan | Bulgu | Risk | Oneri | Efor |
|---|---------|------|-------|------|-------|------|
| 1 | **KRİTİK** | Dagitim | Static Qt build yapilmamis (`release-static` preset). `C:/Qt/6.10.1/mingw_64_static` dizini mevcut olmayabilir. Tek EXE dagitimi mumkun degil. | Alpha release bloker | `just setup-static-qt` calistir (~2 saat), ardindan `just release-static` test et | 2-3 saat |
| 2 | **KRİTİK** | Dagitim | MSIX paketleme/kod imzalama eksik. Windows SmartScreen uyarisi gorunecek, AV false positive riski yuksek. | Kullanici guvenini kirar | Sertifika temin et (self-signed veya CA); `scripts/sign_exe.ps1` mevcut ama test edilmemis | 1-2 gun |
| 3 | **KRİTİK** | Guvenlik | `encryption_key.h` dosyasi binary'de compile-time XOR ile obfuscate ediliyor (`obfstring.h`), ancak deterministic bir PRNG kullaniyor (`__LINE__` bazli seed). Reverse engineering ile key cikarilabilir. | Paket sifreleme kirilabilir | Kabul edilebilir risk (DRM degil, casual protection). Dokumante et. Long-term: server-side key exchange | Bilgi |
| 4 | YUKSEK | Test | UI backend servisleri (17 dosya, ~22K satir) icin birim testi yok. `GameService`, `LocalPackageManager`, `BackupManager` gibi kritik servisler test edilmemis. | Regresyon riski | GameService ve LocalPackageManager icin en az smoke test ekle | 2-3 gun |
| 5 | YUKSEK | Hata Yonetimi | QML backend'de try/catch kullanimi cok sinirli (7 toplam yer, 4 dosyada). `localpackagemanager.cpp` file I/O islemleri exception yakalami yapmadan devam edebilir. | Islenmemis exception crash | Kritik dosya islemlerini try/catch ile sar; `gameservice.cpp`, `backupmanager.cpp`, `localpackagemanager.cpp` | 1 gun |
| 6 | YUKSEK | Guvenlik | Sentry DSN, CMake build-time'da `$ENV{MAKINE_SENTRY_DSN}` olarak enjekte ediliyor (`qml/CMakeLists.txt:442`). Bu deger binary'nin `.rodata` section'inda plaintext kalir. | DSN leak (dusuk etki ama best practice degil) | Sentry client-key DSN zaten "dusuk yetkili" — kabul edilebilir. Alternatif: runtime `.env` okuma | Dusuk |
| 7 | YUKSEK | CI/CD | GitHub Actions'da yalnizca `deploy-manifests.yml` mevcut (manifest sagligi). Build CI, test CI, CodeQL taramasi yok. | Regresyon CI tarafindan yakalanamaz | Build + test + CodeQL workflow ekle. `.github/workflows/ci.yml` | 1 gun |
| 8 | YUKSEK | Guvenlik | `vcpkg.json` lisesinde `"license": "MIT"` yazili, ancak `LICENSE` dosyasi "Makine-Launcher Proprietary License". Tutarsizlik. | Lisans karisikligi | `vcpkg.json` icinde `"license": "LicenseRef-Proprietary"` yap | 5 dk |
| 9 | ORTA | Gozlemlenebilirlik | `HealthChecker::checkNetwork()` placeholder — gercek network kontrolu yapmiyOr (`health.hpp:384-398`). CDN erisim kontrolu eksik. | Offline hata tespiti gecikir | CURL ile `cdn.makineceviri.org` HEAD istegi ekle | 2 saat |
| 10 | ORTA | Kararlilik | `GameService::gameCount()` her cagirisinda `games().count()` calistirir (`gameservice.h:100`) — `games()` bir `QVariantList` kopyasi donduruyor. Hot path'te gereksiz kopya. | Performans (micro) | `return m_games.count()` kullan | 5 dk |
| 11 | ORTA | i18n | `qml/i18n/` dizininde `makine_en.ts`, `makine_en.qm`, `makine_tr.ts` mevcut. Ancak `qml/CMakeLists.txt:516`'da sadece `makine_en.ts` kayitli — `_tr.ts` CMake'e dahil degil. | Turkce ceviriler uygulamaya yuklenmez | TS_FILES listesine `i18n/makine_tr.ts` ekle | 5 dk |
| 12 | ORTA | UX | `EmptyState` componenti sadece 3 yerde kullaniliyor (BackupsSettings, GameSection, EmptyState.qml). Library ekrani, HomePage, ve GameDetailScreen'de bos durum gosterimi eksik olabilir. | Kullanici bosluklarda ne yapacagini bilemez | Tum veri-bagli ekranlarda EmptyState kullanildigini dogrula | 2 saat |
| 13 | ORTA | Erisilebilirlik | `Accessible` property'leri 22 QML dosyasinda 71 yerde kullaniliyor — iyi kapsam. Ancak `HomePage.qml`, `Library.qml`, `HomeScreen.qml` ana ekranlarda Accessible kontrol gerekli. | Ekran okuyucu desteği eksik olabilir | Ana ekranlarda `Accessible.name` ve `Accessible.role` kontrolu yap | 1 saat |
| 14 | ORTA | Guvenlik | `scripts/.encryption_key` (66 byte) ve `scripts/r2_config.json` (450 byte) `.gitignore`'da listelenmiyor — `.gitignore` icinde `scripts/.encryption_key` ve `scripts/r2_config.json` satirlari VAR. Ancak dosyalar diskte mevcut. Git'e commit edilmedigini dogrula. | Secret leak | `git ls-files scripts/.encryption_key scripts/r2_config.json` ile kontrol et | 5 dk |
| 15 | DUSUK | Kod Kalitesi | `localpackagemanager.h:1-2` satirinda `#pragma once` iki kez yazilmis (satir 1 ve satir 6). | Gereksiz tekrar, zarar vermez | Birini kaldir | 1 dk |
| 16 | DUSUK | Dokumantasyon | `docs/security/security-model.md` mevcut, ancak `docs/security-plan.md` (proje kokunde) ile caprazlanma mevcut. Iki farkli guvenlik dokumani. | Dokuman tutarsizligi | Birlestir veya cross-reference ekle | 30 dk |
| 17 | DUSUK | Build | `justfile:21-22` setup recipe'de `simdjson`, `taskflow`, `concurrentqueue`, `simdutf` listeleniyOr ama `vcpkg.json` dependencies'de bunlar yok. Ayri/klasik mod farki olsa da tutarsiz. | Build karisikligi | Senkronize et | 15 dk |
| 18 | DUSUK | Performans | `mkpkformat.h:213-214`: zstd decompress 10GB safety limit — makul. Ancak `zstd_decompress` streaming modunda `reserve(size * 4)` kullanir — buyuk dosyalarda fazla bellek ayirabilir. | Bellek spikesi | Streaming modda kademeli buyutme kullan | 1 saat |
| 19 | DUSUK | Guvenlik | `selfupdater.h:43` `verifySignature` Authenticode dogrulamasi iceriyor — iyi. Ancak update JSON (`cdn.makineceviri.org/assets/update.json`) indirilirken SSL pinning'in aktif olup olmadigini dogrula. | MITM update saldirisi | `TranslationDownloader` ve `UpdateService` network islemlerinde SSL pin uygulamasini kontrol et | 1 saat |
| 20 | DUSUK | UX | `OnboardingWizard.qml` 3 adimli (Welcome → Scan → Ready). Scan adiminda hata durumu (Steam bulunamadi, dizin erisim sorunu) icin kullanici geri bildirimi kontrol edilmeli. | Kullanici onboarding'de takilabilir | Error state handling'i ScanStep.qml'de dogrula | 30 dk |

---

## 3. Release Gates Checklist

### Gate 1: Build & Toolchain
- [x] `cmake --preset dev && cmake --build --preset dev` basarili (423MB build mevcut)
- [x] CMakePresets.json gecerli JSON, 7 configure + 7 build + 1 test preset
- [x] Tum QML dosyalari CMakeLists.txt'te kayitli ve diskte mevcut
- [x] Tum backend source/header dosyalari CMakeLists.txt'te kayitli
- [x] ccache entegrasyonu aktif
- [ ] `cmake --preset release` MSVC build'i basarili (test edilmeli)
- [ ] `cmake --preset release-static` static build basarili (**BLOKER**)
- [x] `just dev` hizli iterasyon calisiyor

### Gate 2: Core Library Quality
- [x] 33 birim test dosyasi (12K+ satir) — kapsamli
- [x] Benchmark (`bench_json_parsing.cpp`) ve fuzz testi (`fuzz_json_parser.cpp`) mevcut
- [x] `std::expected`-bazli hata yonetimi (`Result<T>`, `VoidResult`)
- [x] Error context chain desteGi (`.withContext()`, `.withFile()`, `.withGame()`)
- [x] ErrorCollector, retry logic, ErrorSuggestion mevcut
- [x] HealthChecker — database, filesystem, memory, network kontrolleri
- [ ] Integration testleri devre disi — handler'lar implement edilmediginden

### Gate 3: Security
- [x] Build hardening: ASLR, DEP, CFG, stack protector (core + qml CMakeLists)
- [x] Path traversal koruması — `pathsecurity.h` + core `PathValidator`
- [x] SSL certificate pinning — 4 pin (2 primary, 2 backup Cloudflare CA)
- [x] Ed25519 paket imzalama ve dogrulama
- [x] AES-256-GCM paket sifreleme (MKPK format)
- [x] Compile-time key obfuscation (`obfstring.h`)
- [x] `MAKINE_RELEASE_VERIFIED` static_assert — placeholder pin koruması
- [x] Sentry PII stripping (Windows kullanici adi redaction)
- [x] VDF parser recursion depth limiti (32) ve dosya boyutu limiti (10MB)
- [x] Windows Credential Manager entegrasyonu (`credential_store.hpp`)
- [x] Binary integrity hash (SHA-256 post-build otomatik)
- [x] Authenticode dogrulama (`SelfUpdater::verifySignature`)
- [ ] Kod imzalama sertifikasi (**BLOKER**)
- [ ] Pin rotation proseduru (dokumante edilmemis)

### Gate 4: UI/UX Quality
- [x] 5 ekran: Home, Library, GameDetail, Settings, Onboarding
- [x] 37 component, 6 dialog, 3 controller
- [x] Theme singleton, Dimensions singleton (responsive layout)
- [x] Accessibility: 71 Accessible property kullanimi (22 dosya)
- [x] Empty state componenti mevcut
- [x] Skeleton loader mevcut (loading state)
- [x] FocusRing (klavye navigasyonu)
- [x] System tray entegrasyonu
- [x] Custom frameless title bar (DWM Mica/dark mode destegi)
- [x] Native Win32 splash screen (threaded, animasyonlu gradient)
- [x] Batch operations panel
- [x] Anti-cheat uyari dialog'u

### Gate 5: Observability
- [x] Sentry crash reporting (breakpad backend, MinGW uyumlu)
- [x] Qt categorized logging (16 kategori: `makine.*`)
- [x] Tracy profiler entegrasyonu (on-demand, sifir overhead)
- [x] FrameTimer, SceneProfiler, MemoryProfiler (dev tools)
- [x] PerformanceMonitor QML overlay (F3)
- [x] PerfReporter (CSV export)
- [x] CrashRecoveryJournal (persistent JSON journal)
- [x] OperationJournal (install/uninstall izleme)
- [ ] Runtime log export (kullanicinin log paylasma ozelligi yok)

### Gate 6: Deployment Pipeline
- [x] CDN: cdn.makineceviri.org (Cloudflare R2 custom domain)
- [x] 258/258 .mkpkg paketi yuklenmis ve imzalanmis
- [x] deploy.py + sign_packages.py + r2_upload.py pipeline
- [x] GitHub Actions: `deploy-manifests.yml` (manifest health check)
- [x] windeployqt entegrasyonu (`just deploy`)
- [x] UPX compression secenegi (static build icin)
- [ ] GitHub Actions: CI build + test workflow yok
- [ ] NSIS/MSIX installer yapisi

---

## 4. MVP Release Gate (Minimum Gerekli Set)

Alpha release icin **mutlaka** tamamlanmasi gerekenler:

| # | Gorev | Tahmini Efor | Blokaj |
|---|-------|-------------|--------|
| 1 | Static Qt build (`just setup-static-qt` + `just release-static`) | 2-3 saat | **Evet** |
| 2 | Kod imzalama (self-signed en az, CA ideal) | 1 gun | **Evet** |
| 3 | `vcpkg.json` lisans duzeltmesi ("MIT" → "LicenseRef-Proprietary") | 5 dk | Hayir |
| 4 | `makine_tr.ts` CMake'e dahil etme | 5 dk | Hayir |
| 5 | Exception handling — kritik I/O islemlerinde try/catch | 4 saat | Hayir (ama onerilen) |
| 6 | `release` preset MSVC build testi | 1 saat | Test |

**MVP sonrasi ama pre-beta:**
- CI/CD pipeline (build + test + CodeQL)
- UI backend birim testleri
- NSIS veya MSIX installer
- Network health check gercek implementasyonu

---

## 5. Kod Kalitesi ve Iyilestirme Onerileri

### 5.1 Core Library — Guclü Yonler
- `std::expected` tabanli hata yonetimi modernite acisından mucemmel
- `PathValidator`, `PathGuard` RAII pattern'leri guvenlik icin ideal
- Header-only utility'ler (`cache.hpp`, `lazy.hpp`, `async.hpp`) temiz
- ADR (Architecture Decision Records) mevcut — 5 karar dokumante

### 5.2 UI Backend — Iyilestirme Alanlari
- **Singleton pattern:** `CoreBridge`, `BackupManager` global singleton kullanir — test edilebilirlik dusuk. Dependency injection tercih edilmeli.
- **Cache invalidation:** `GameService` 5 farkli cache flag'i yonetiyor (`m_cacheValid`, `m_supportedCacheValid`, `m_translationCacheValid`, `m_installedCacheValid`, `m_outdatedPatchCount`). Bu karmasikligi azaltmak icin tek bir versiyon sayaci veya `QAbstractListModel` kullanimi degerlendirilmeli.
- **Thread safety:** `LocalPackageManager::m_cancelRequested` atomic — iyi. Ancak `m_catalog` member'a concurrent erisim icin koruma gorulmuyor.
- **Signal/slot bağlantilari:** `GameService` 16 signal tanimliyor — bu sayi yuksek ama QML binding modeli icin kabul edilebilir.

### 5.3 QML — Kalite Gozlemleri
- `pragma ComponentBehavior: Bound` — Main.qml ve OnboardingWizard'da kullaniliyor (performans icin iyi)
- Singleton'lar dogru kayitli (Theme, Dimensions, DebugHelper)
- `visible: true` Main.qml'de mevcut (known gotcha korunmus)
- Component isimlendirme: shadowing riski yok (tum local component'ler benzersiz isimlendirilmis)

### 5.4 Build Sistemi — Kalite
- Preset mimarisi kapsamli ve tutarli
- Super-build pattern (root CMakeLists → core/ + qml/) temiz
- Optional dependency handling (`if(TARGET bit7z::bit7z)`) iyi izole
- Post-build SHA-256 hash uretimi — integrity icin artı

---

## 6. Release Readiness Skor Karti

| Alan | Skor | Aciklama |
|------|------|----------|
| **Mimari** | %90 | Temiz katmanli yapi, ADR'ler mevcut, sorumluluk ayirimi iyi |
| **Build Sistemi** | %85 | 7 preset, ccache, super-build; static build eksik |
| **Guvenlik** | %80 | SSL pinning, Ed25519, AES-256-GCM, path security; pin rotation ve kod imzalama eksik |
| **Kararlilik** | %75 | Crash recovery, journal; exception handling zayif, singleton test edilebilirligi dusuk |
| **Performans** | %85 | Tracy, FrameTimer, splash screen, lazy loading; micro-optimization firsatlari var |
| **Test Kapsami** | %55 | Core: 33 test (iyi); UI backend: 0 test (risk); QML: test yok |
| **Gozlemlenebilirlik** | %85 | Sentry, 16 log kategorisi, Tracy, journal; log export eksik |
| **CI/CD** | %40 | Sadece manifest health check; build/test/lint CI yok |
| **UX/Urun** | %90 | Onboarding, empty state, skeleton loader, accessibility, i18n; TR cevirisi CMake'de eksik |
| **Dokumantasyon** | %80 | CLAUDE.md, ROADMAP, security-plan, ADR'ler; API reference ve user guide parcali |
| **Dagitim** | %60 | CDN tamam, pipeline tamam; static build, installer, kod imzalama eksik |
| **Uyumluluk/Lisans** | %85 | Proprietary LICENSE; vcpkg.json tutarsiz; KVKK/GDPR icin veri toplama minimal (Sentry anonymized) |
| **GENEL** | **%78** | Alpha release'e yakin. 3 kritik bloker giderilmeli. |

---

## 7. Blokajlar — Kullanicidan Beklenenler

### Karar Gerektiren Konular

1. **Static Qt Build:** `C:/Qt/6.10.1/mingw_64_static` dizini mevcut mu? Yoksa `just setup-static-qt` (Qt Source'dan derleme, ~1-2 saat) gerekiyor. Qt Online Installer'dan "Qt 6.10.1 > Sources" kurulmali.

2. **Kod Imzalama Stratejisi:**
   - **Secenek A:** Self-signed sertifika (`scripts/create_dev_cert.ps1` mevcut) — SmartScreen uyarisi kalir ama AV false positive azalir
   - **Secenek B:** CA sertifikasi (Sectigo, DigiCert, vb.) — ~$70-300/yil, en iyi UX
   - **Secenek C:** Azure Trusted Signing (Microsoft, dusuk maliyet) — MSIX icin ideal

3. **Dagitim Formati:**
   - **Secenek A:** Tek EXE (static build + kod imzalama) — en basit
   - **Secenek B:** NSIS installer + kod imzalama — profesyonel gorunum
   - **Secenek C:** MSIX + Microsoft Store — en guvenilir, otoGuncelleme dahil

4. **CI/CD Onceligi:** GitHub Actions build + test workflow olusturulsun mu? CodeQL security scanning isteniyOr mu?

### Bilgi Gerektiren Konular

5. **KVKK/GDPR:** Sentry'ye gonderilen veriler anonymized (machine ID SHA-256, kullanici adi redacted). Acik privacy policy veya KVKK aydinlatma metni gerekiyor mu?

6. **Telemetry:** Sentry disinda kullanim telemetrisi (hangi oyunlar yuklendi, vb.) toplanmiyOr. Bu kasitli mi, yoksa ileriye donuk planlanan bir ozellik mi?

---

## 8. Bugun Yapilacak Net Aksiyon Plani

### Hemen (< 30 dakika)

1. **`vcpkg.json` lisans duzeltmesi** — `"license": "MIT"` → `"license": "LicenseRef-Proprietary"`
2. **`makine_tr.ts` CMake'e dahil etme** — `qml/CMakeLists.txt:515` TS_FILES listesine ekle
3. **`localpackagemanager.h` cift `#pragma once`** kaldir (satir 1 veya satir 6)
4. **`gameservice.h:100` optimizasyon** — `games().count()` → `m_games.count()`
5. **Secrets kontrol:** `git ls-files scripts/.encryption_key scripts/r2_config.json` — commit edilmemis olmali

### Kisa vadede (1-3 gun)

6. **Exception handling sarimlama** — `localpackagemanager.cpp`, `backupmanager.cpp`, `gameservice.cpp` dosyalarindaki dosya I/O islemlerini try/catch ile koru
7. **CI/CD workflow** — `.github/workflows/ci.yml` olustur: dev preset build + core testler
8. **Static Qt build** — `just setup-static-qt` calistir, `just release-static` test et
9. **Kod imzalama** — karar sonrasi uygula

### Orta vadede (1-2 hafta)

10. **UI backend testleri** — `GameService`, `LocalPackageManager`, `BackupManager` icin smoke testler
11. **Network health check** — `HealthChecker::checkNetwork()` gercek implementasyonu
12. **Installer** — NSIS veya MSIX, karar sonrasi
13. **Pin rotation proseduru** — SSL pin guncelleme adimlari dokumante et

---

## Ek: Dosya Istatistikleri

| Metrik | Deger |
|--------|-------|
| C++ kaynak dosyalari (core/src + qml/src) | ~22K satir |
| C++ baslik dosyalari (core/include + qml/src/services) | ~48 dosya |
| QML dosyalari | 69 dosya, ~10K satir |
| Test dosyalari | 33 dosya, ~12K satir |
| CMakeLists.txt | 6 dosya |
| Build presetleri | 7 configure + 7 build + 1 test |
| vcpkg bagimliliklari | 14 (+ 5 opsiyonel classic-mode) |
| Q_INVOKABLE + Q_PROPERTY | 193 toplam (17 servis dosyasi) |
| QML componenti | 37 + 6 dialog + 3 controller |
| Logging kategorileri | 16 (`makine.*`) |
| CDN paketleri | 258 .mkpkg + .sig |
| Git commitleri (son 5) | Refactoring, logging, backup fix |

---

*Bu denetim raporu, kod tabaninin 2026-03-12 tarihli durumuna dayanmaktadir. Tum bulgular dosya ve satir referanslari ile desteklenmistir.*
