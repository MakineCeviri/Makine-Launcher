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

## [0.2.2-beta] - 2026-10-23

### Düzeltilen
- **Dolu rafın üstünde sayfa kaymıyordu** (0.2.0): rafın `WheelHandler`'ı tekerlek olayını yok saysa da arkasındaki sayfayı engelliyordu. Artık yalnız Shift + tekerlek ve yatay tekerlek rafın (iki ayrı işleyici, `acceptedModifiers` / `orientation`), düz tekerlek sayfanın
- **Kendi programıyla kurulan yamada yedek istenmiyor**: launcher yedek listesi çıkarmıyor (yedeği ve geri almayı program yapar: Heavy Rain `.hryedek`, ACS `restore`). Kaldırma komutu tanımsız tarif yine kurulmuyor, ama mesaj yedekten değil eksik komuttan söz ediyor; açıklama kartı da aynı şeyi söylüyor
- **Programın günlüğü saklanıyor**: programın geçici kopyada kendi yanına yazdığı metin dosyaları (`HR_yama_gunluk.txt`) kopya silinmeden `logs/programs/<oyun>/<zaman>/`'a alınıyor, yolu oyunun sayfasında

## [0.2.1-beta] - 2026-10-23

### Düzeltilen
- **İlk açılışta donma** (0.2.0-beta, her yeni kurulum): katalog henüz eşitlenmemişken `GameService::supportedGames()` boş listeyle modeli her çağrıda sıfırlıyordu; ana sayfanın, öne çıkanlar şeridinin ve karşılama adımının `modelReset` işleyicileri listeyi yeniden isteyince sıfırlama yeniden tetikleniyor, JS yığını taşıyordu (Sentry: `RangeError: Maximum call stack size exceeded`). Boş üstüne boş artık sıfırlama sayılmıyor; `supportedGames()` kendi sıfırlamasının içinden ikinci kez sıfırlamıyor

## [0.2.0-beta] - 2026-10-23

### Eklenen
- **Yamanın kendi programıyla kurulum, sözleşmeyle** (`docs/patch-programs.md`, tarif seviyesi 3) — program çalışmadan önce oyun klasörü tarifin `requires` dosyalarıyla doğrulanıyor (yanlış klasörde hiçbir şey çalışmıyor; bir üst/alt klasör uyuyorsa oyun oraya bağlanıyor); program paketin geçici kopyasından çalışıyor ve kopyadaki betiklerde çevirmenin sabit oyun klasörü (`set OYUN=D:\…`, `game.path`) launcher'ın bulduğu klasörle değiştiriliyor; klasör ayrıca `MAKINE_OYUN` ortam değişkeni ve oyun klasöründeki `.makine` notuyla veriliyor; program bittiğinde oyunda ne değiştiğine bakılıyor — hiçbir şey değişmediyse "kuruldu" denmiyor; kaldırma programı saklanan kopyadan çalışıp bekleniyor, çıktısı sayfada
- **Ubisoft Connect kütüphanesi** taranıyor (kayıt defterindeki kurulumlar; başka diskteki kütüphaneler dahil)

### Değişen
- Kendi programıyla kurulan yamalarda sayfa, kaldırma onayı ve açıklamalar yedekten değil programdan söz ediyor; yanlış klasör hatasında "Oyun dosyalarını doğrula" yerine "Klasörü değiştir"
- **Görünüm: Apple yapısı, 0.1.5'in tonu** (`docs/design.md`) — neredeyse siyah zemin vurgunun iki yumuşak ışığıyla (`AppBackground`); her bölüm bir kapta: yarı saydam, ince çizgili, üst kenarı ışık alan (`Card`); kayan içerik kenarda kesilmiyor, kayboluyor (`EdgeFade`, `TopDissolve`); uçları kaybolan ayraç (`FadeLine`); 0.1.5'in vurgu paleti, varsayılan mor (0.1.7'nin mavisi bir kez mora döner); sıcak kırık beyaz yazı
- Sayfa başlığının açıklaması tam genişlikte; filtreler kabın üst çubuğunda (Tüm Oyunlar, Kütüphanem)

### Düzeltilen
- Raflar fare tekerleğini yutmuyordu: imleç raftayken sayfa kaymıyordu
- Başarısız kurulumdan sonra oyunda değişiklik kalmadıysa "Oyun dosyalarını doğrula" gösterilmiyor; yanlış klasörde "Klasörü değiştir"
- Yama programı sözleşmesi sağlamlaştırıldı (yayın öncesi denetim):
  - `start` ile asıl aracı açıp hemen biten betik: araç bitene kadar bekleniyor (iş nesnesi; paketin kendi programları ve betik yorumlayıcıları), "Durdur" bütün ağacı kapatıyor — önceden araç çalışırken kopyası siliniyor ve kurulum "değişiklik yok" diye başarısız sayılıyordu
  - Kaldırma programı soru sorarsa yanıt sayfadan veriliyor, "Durdur" ile durduruluyor; aynı anda tek kaldırma programı
  - Kaldırma programı iki kez başarısız olursa (bulunamadı, yeni tarifte adı değişti, hata kodu) üçüncü "Kaldır" yamayı programsız kaldırıp mağaza onarımını başlatıyor; önceden kayıt sonsuza dek kalıyordu
  - Bir dosyası silinemeyen kaldırmayı yeniden denemek programı ikinci kez çalıştırmıyor
  - Güncelleme eski sürümü önce kendi programıyla kaldırıyor (araç yamalı dosyaları "orijinal" diye yedeklemesin)
  - `requires` yalnız seçilen sürümün/seçeneklerin adımlarından; kurulu yamanın klasöründe eksikse "Klasörü değiştir" yerine güncelle/doğrula söyleniyor
  - Yedeği olan başarısız kurulumda programın değişiklikleri kayıttan düşmüyor; geri alma programı olmayan tarifte "Kaldır" programın değiştirdiklerini mağaza onarımına bırakıyor (sayfa ve onay penceresi bunu söylüyor)
  - Betik düzeltmesi: tırnaklı değer tırnağıyla kalıyor (`set OYUN="%MAKINE_OYUN%"`); mağazanın oyun olmayan klasörleri (`Steamworks Shared`, `Epic Games\Launcher`) değiştirilmiyor
  - Sentry: her sonuç kendi nedeniyle (`folder_mismatch`, `program_no_change`, `tool_missing`, `uninstall_tool_skipped` …); 0.1.5'in "oyun dosyası bulunamadı" adımları artık `target_missing`
- Raf okları kenar solmasının altında soluk görünüyordu; kaydırma çubuğu sayfanın üst solmasıyla birlikte kayboluyordu; öne çıkanlar şeridi kaydırılıp görünmezken de slayt değiştiriyordu

## [0.1.7-beta] - 2026-10-22

### Değişen
- **Apple tarzı arayüz** — kenar çubuklu macOS düzeni, Apple sistem renkleri (koyu ve açık görünüm), Inter Display başlıklar, yeni simgeler; Ana Sayfa App Store rafları ve öne çıkan oyunlarla, oyun sayfası Apple TV tarzı sanat kartıyla, Ayarlar macOS ayar penceresi gibi. Ayrıntı: `docs/design.md`.
- **Kurulu yama** — "Türkçe yama kurulu" durumu ve "Kaldır" artık ayrı; üzerine gelince silme düğmesine dönüşen durum kaldırıldı. Yama notları kurulumdan önce de görünüyor.
- **Açık görünüm** artık seçilebiliyor; vurgu rengi varsayılanı mavi.

### Eklenen
- **Oyun klasörü seçimi** — bulunamayan oyunun klasörü, `.exe`'nin klasörü ya da içindeki herhangi bir klasör seçilebiliyor ya da pencereye sürüklenebiliyor; kök klasör yamanın dosyalarına göre bulunuyor, yanlış klasöre kurulmuş yama için "Yamayı taşı"
- **Yamanın kendi programı oyun sayfasında** — `run` adımının çıktısı görünüyor, sorduklarına yanıt verilebiliyor
- **Nasıl çalışır?** (F1) ve **Tüm Oyunlar** ekranları; Kütüphanem eylem listesi
- **Sentry gezinme izleri** — sayfa ve açılan oyun (uygulama kimliği) olaylara iz olarak düşüyor; açılıştaki kurtarma yarım kalan işlemi tam geri alamazsa sayfanın üstünde söyleniyor

### Düzeltilen
- **Güncelleme** — yedeksiz güncelleme yarıda kalınca çalışan önceki yama silinmiyor; güncellemenin ilk kez değiştirdiği oyun dosyası önce yedeğe ekleniyor; tarifi yeniden çalıştıran güncelleme yerinde düzenleme işaretini kayıttan düşürmüyor; geri alma kayıt bırakınca sayfa "Kaldır"ı gösteriyor; onarım başka bir oyunun kaldırma sonucunu kendi sonucu sanmıyor
- **Parmak izi** — exe adları birleştiriliyor, yollu ad dosya adıyla eşleşiyor
- **Kaldırma ve kurtarma** — yarım kalan kaldırmada geri gelen orijinaller silinmiyor; kurulum günlüğü her kaydı anında yazıyor; uzun yollu ve Türkçe karakterli klasörlerde kurtarma takılmıyor; kaldırma kayıtları sondan başa geri alınıyor; kurulum ile kaldırma aynı anda çalışmıyor; eski yedek sonraki kaldırmada geri yüklenmiyor.
- **Kurulum** — silme adımının sildiği dosya yedekleniyor; yarıda kalan kurulumun yazdıkları kayda geçip geri alınıyor; güncelleme oyunun orijinalini "eklenen" saymıyor; sıradaki indirme başlıyor; yeniden denemede iptal çalışıyor; yarım açılan paket tam sanılmıyor; seçenekli yamaların güncellemesi ve onarımı yamayı silip reddetmiyor.
- **Tespit ve ağ** — güvenilmeyen TLS sertifikası hiçbir koşulda kabul edilmiyor; birkaç oyunun paylaştığı klasör adı rastgele oyuna bağlanmıyor; tarama detay bilgilerini silmiyor; açık kalan launcher eski tarifi kurmuyor; "Windows ile başlat" açıkken açılış ekranı takılmıyor.
- **Sentry** — Windows kullanıcı adı modül yollarından temizleniyor; hata etiketleri sonraki olaylara sızmıyor; QML hataları ve 5 sn'lik donmalar raporlanıyor; gruplama ve uyarılar yeniden kuruldu.

## [0.1.6-beta] - 2026-10-20

### Eklenen
- **Hata raporu ayarı** — Ayarlar → Genel → "Çökme ve Hata Raporlarını Gönder" (varsayılan açık). Kapalıyken Sentry hiç başlatılmıyor; çalışırken kapatılınca o andan sonra hiçbir rapor, çökme ya da oturum bilgisi gönderilmiyor; yeniden açılınca yeniden başlatma gerekmeden sürüyor. Sayım kanalı (`TelemetryService`) bu ayarın dışında
- **Kendi programıyla kurulan yamayı kaldırma** — `run` adımına isteğe bağlı `uninstall` + `uninstallArgs` (aynı `${gamePath}`/`${packageDir}`); "Kaldır" o programı beklemeden başlatıp normal kaldırmaya devam ediyor. Önceden yalnız izlenen dosyalar siliniyor, yama oyunda kalırken "kaldırıldı" deniyordu. Eski launcher'lar bu anahtarları yok sayar
- **Yeni kurulum adımları (tarif seviyesi 2)** — `patchBytes` (oyun dosyasında bayt değiştirme), `forgeReimport` (.forge arşivine aktarma), `xdelta` (VCDIFF delta, `third_party/xdelta3`): Elden Ring ve Assassin's Creed Valhalla / Odyssey / Mirage yamaları launcher'ın içinde kuruluyor. Launcher bildiği adım kümesini `X-Makine-Recipe` ile söylüyor; eski sürümlere çalıştırabilecekleri tarif gidiyor
- **Katalog açıkken de yenileniyor** — 30 dakikada bir ve pencereye dönünce (en az 10 dakika arayla); değişmediyse sessiz
- **Değişen kapaklar geliyor** — önbellekteki kapak 3 günde bir CDN'e soruluyor (`If-None-Match`)

### Düzeltilen
- `run` adımında `${gamePath}` / `${packageDir}` Windows ayırıcısıyla (`D:\SteamLibrary\…`) veriliyor; `D:/…` biçimi yamanın kendi .bat'ındaki xcopy/copy komutlarını bozuyordu
- `run` adımının süre sınırı 30 dakikadan 2 saate çıktı: etkileşimli yama kurucuları (Heavy Rain'inki 10–20 dk + kullanıcının tıklamaları) yarıda kesiliyordu; iptal her an çalışıyor
- **Arayüz donmaları** — `main.cpp` Qt'nin hata ayıklama anahtarı `QV4_MM_AGGRESSIVE_GC`'yi açıyordu; JS ağırlıklı her bağlama 100–300 kat yavaş çalışıyor, eklenti sayfası 2 saniye donuyordu
- **Kaldırma doğruluğu** — tarifli kaldırma geri yüklenen orijinalleri silmiyor; seçenekli paketlerde (Elden Ring, DS2, GTA…) yedek listesi seçenek adımlarını kapsıyor; yedeksiz kaldırma eklenen dosyaları silip değişenler için mağaza onarımı başlatıyor; salt okunur dosyalar kaldırılabiliyor; silinmiş ya da taşınmış oyun klasöründe kayıt düşüyor; yedek ikinci kez geri yüklenemezse mağaza onarımına geçiliyor
- Yerinde yamalayan paketin güncellemesi önce orijinalleri geri yüklüyor; güncelleme eski açılmış paketi yeniden kurmuyor (paket damgası)
- Paket açıcı PAX/GNU uzun adları okuyor ("Türkçe Yama" artık "T_rk_e Yama" değil, 100 bayttan uzun adlar kesilmiyor), MAX_PATH'i aşan yolları yazabiliyor, `.lnk` uzantılı oyun dosyalarını kısayol sanmıyor
- İndirme öncesi boş alan denetimi paketin gerçek boyutunu kullanıyor (önceden sabit 500 MB)
- Kopyalama hatasında antivirüs engeli ve Windows hata kodu görünüyor
- Oyunun aslı remaster paketine, remaster aslının paketine eşlenmiyor (AC III ↔ Remastered, Hades ↔ Hades II)
- `userPath` hedefindeki `Documents/` gerçek Belgeler klasörüne çözülüyor (OneDrive klasör taşıması açıkken boş `Documents`'a kuruyordu)
- Elle kurulan yamada (external / installer / paradox-mod) çıkarılan klasör kendiliğinden açılıyor
- Açılıştaki eski klasör temizliği yalnız boş klasörü siliyor (önceden yeni kökteki `backups/` dahil siliyordu)
- 0.1.6'ya geçince 0.1.5'in indirdiği eski tarifler atılıyor

### Geliştirme
- Release yapısında `-g1`: Sentry kendi çerçevelerimizi çözümlüyor; MinGW/vcpkg DLL'lerine Debug ID damgalanıp sembolleri yükleniyor; The Qt Company imzalı Qt DLL'lerine dokunulmuyor
- `--selftest-telemetry` hata raporu ayarının çalışırken kapanıp açılmasını da doğruluyor (`MAKINE_SELFTEST_TOGGLE`)
- Eklenti sistemi v2 (süreç dışı host, imzalı kayıt defteri; ADR-0009) hâlâ `MAKINE_DEV_TOOLS` arkasında, yayına girmiyor

## [0.1.5-beta] - 2026-10-06

### Eklenen
- **Doğrudan kurulum paketi** — Qt çalışma zamanını içinde taşıyan `Makine-Launcher-v0.1.5-beta-win64.zip`, GitHub Releases'te yayımlanıyor (`just release-zip-dynamic`, `scripts/make_release_zip.ps1`)
- **VPatch ikili delta uygulaması** — Delta biçimi launcher'ın içinde çözülüyor, paketin kendi uygulayıcısına gerek yok
- **İndirme bütünlüğü** — İndirilen paket katalogdaki SHA-256 ile doğrulanıyor
- **Hata telemetrisine neden etiketleri** — Olaylara hatanın nedenini taşıyan tag'ler eklendi
- **Katalog/R2 tutarlılık kapısı** — Yayın öncesi denetim CI adımına bağlandı
- **release-mingw preset** — Statik Qt kiti olmadan dağıtım yapısı (MSIX + ZIP)
- **İşlem sonucu sayımı** — Her işlemin sonucu (başarı dahil) `TelemetryService` ile kotasız kendi ucumuza (`/api/v2/telemetry`, şema v2) gidiyor; çevrimdışıyken diske yazılıp sonra gönderiliyor
- **Sentry kapısı** — Desteklenmeyen kurulum retleri Sentry'ye gitmiyor, aynı hata kurulum başına günde bir kez, oturum başına en fazla 30 olay (`telemetryrules.h`)
- **Telemetri bekçisi** — `scripts/telemetry_watchdog.py` + günlük CI: Sentry kota düşüşü ya da sayım kanalının susması hata verir
- **CI** — Her `dev` push'unda Windows'ta derleme ve 19 test takımı (`.github/workflows/ci.yml`)
- **Elle eklemede klasör denetimi** — Seçilen klasör oyunun kök klasörüne bağlanıyor; sürücü kökü, korumalı klasör, exe'siz ya da oyun koleksiyonu klasörü nedeni söylenerek reddediliyor

### Düzeltilen
- `GameCard` görsel bağlantısı kendi işleyicisinden hedef değiştirdiği için Qt6Qml.dll'de oluşan çökme (0.1.4 saha çökmelerinin 24/27'si)
- `PackageCatalog` tarama worker'ı ile ana thread arasında korumasız okunup yazılıyordu (NATIVE-7N, 8A)
- Açılışta kütüphane iki kez taranıyordu
- Etkileşimli GUI isteyen kurulum araçları kurulum sırasında çalıştırılmıyor
- Varyant klasörlü paketler körlemesine overlay'lenmiyor; varyant her hedef yola taşınıyor
- Boş paket klasörü ve boş reçeteli kopyalama tipi geçerli kurulum kaynağı sayılmıyor
- Yedek listesi belirlenemiyorsa yama kurulmuyor; liste kurulumun kendi çözücüsünden geliyor
- Tek satırlık `cmd` reçeteleri çalıştırılabilir hale getirildi
- Kurulum sonrası talimat gerçekten kurulan dosyalardan türetiliyor ve kurulum bitince gösteriliyor
- Yol sınırı kontrolleri dizin sınırını bilen ortak yardımcıya taşındı; var olmayan hedef kaçış sayılmıyor
- Yazma izni ön-kontrolü yükseltilmiş yolu baştan kesmiyor; yükseltilmiş adım konteyner dışı gerçek yolla başlıyor
- Eksik `userPath` hedefi "desteklenmiyor" olarak gösterilmiyor
- HTTP/2 protokol hatasında indirme HTTP/1.1 ile yeniden deneniyor
- Boş çıkan pakette sonsuz yeniden-indirme döngüsü durduruldu
- Aralık tarif etmeyen delta yanıtı uygulanmıyor
- Steam kütüphanesi kayıt defteri yanıt vermediğinde de bulunuyor
- Watch Dogs gibi oyunlar "tespit edilemedi" sayılmıyor; mağaza istemcileri oyun olarak listelenmiyor
- Çözülemeyen veritabanı uygulamayı kalıcı olarak kırmıyor
- Yedek alınamadığında gerçek engel raporlanıyor; kaldırma hatası ve kurtarma düğmesi 6 saniyede kaybolmuyor
- Güncelleme denetiminde üst çubuk sessiz; "Güncel" rozeti yalnızca ayarlar sayfasında
- Doğrulayıcı, yönlendirme girdilerini paket sanmıyor
- Çıkarıcının ASCII dışı adları bozduğu iddiası düzeltildi
- Yarım kalan kurulumun çökme kurtarması yedeği olan orijinalleri siliyordu; artık yedekten geri koyuyor, yedek yoksa dosyalara dokunmuyor
- Kaldırma: dosya silinemeyince "kaldırıldı" denmiyor; tetiklenmeyen geri yükleme bağlantısı sonraki geri yüklemede eski geri çağrıyı çalıştırmıyor
- İptal ve zaman aşımı, işçi bitmeden kurulum yuvasını boşaltmıyor; sayfa kapalıyken biten kurulum bildirim veriyor, sayfa yeniden açılınca süren kurulum görünüyor
- Escape ile kapanan pencere iptali bildiriyor; kapanışta arka plan işleri bekleniyor
- Tarama ve açılış iş parçacıkları ana iş parçacığının üyelerini okumuyor
- Paylaşılan exe adları (`game.exe`, `launcher.exe`…) ve iki pakete ait exe'ler oyun tanımlamıyor
- Paket yalnız CDN sunucusundan indiriliyor (adres alan adıyla denetleniyor)
- Kısaltılmış açıklama biçimlendirme olarak yorumlanmıyor
- Boşluklu klasörden eklenti kurulabiliyor
- İlk açılıştaki renk seçenekleri Ayarlar'dakiyle aynı
- Tepsi menüsündeki "Ayarlar" ayarlar sayfasını açıyor
- Discord bağlantısı kalıcı sunucu davetine çevrildi

### İyileştirilen
- Reçete önbelleğine yaş sınırı, paket detay önbelleği katalog değişince tazeleniyor
- Katalog sürüm karşılaştırması tek otoriteye bağlandı
- Çökme raporları sembolleşebilsin diye semboller korunuyor ve doğru yerden yükleniyor
- Telemetriye giden yoldan kullanıcı adını silen kural testle sabitlendi
- Dil tablosundan kanıtsız kodlar çıkarıldı
- Yayın düzeni belgelendi (`docs/RELEASING.md`), eski sürümler arşivlendi
- `just test` bütün test takımlarını çalıştırıyor; yayın ZIP'i testler geçmeden üretilmiyor
- Kendi kodumuz `-Wall -Wextra` ile uyarısız derleniyor

### Kaldırılan
- Çekirdeğin çağrılmayan modülleri (oyun algılayıcı, yama motoru, SQLite veritabanı, runtime/güvenlik yöneticisi, SSL pinning — 106 dosya, ~34.700 satır); vcpkg bağımlılıkları 4'e indi, açılışta şifresi çözülen kullanılmayan `makine.db` artık yüklenmiyor
- Hiçbir yerde kullanılmayan 12 QML bileşeni ve 24 MB vitrin görseli

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
