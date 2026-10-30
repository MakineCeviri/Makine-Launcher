# Telemetri — Sentry ile Uzaktan Teşhis

> **Amaç:** Kullanıcı geri bildirimi beklemeden sorunları görmek.
> **Proje:** `makineceviri / makine-launcher` (0.1.5+; 0.1.4 ve öncesi eski `makine-ceviri / native`'e gönderir) · AB bölgesi, API `https://de.sentry.io/api/0` (entegrasyon listesi kontrol silosunda: `sentry.io`) · **Durum:** 2026-10-22 — iki kanal: sayım (kendi ucumuz) + teşhis (Sentry), körlük bekçisi; QML hatası ve UI donması da raporlanıyor, sunucu tarafı gruplama ve yol temizliği `sentry_setup.py`'de

## Komutlar

| Komut | Ne yapar |
|---|---|
| `just telemetry-check` | Tümü: kapsam denetimi + canlı öz-test + ölü alarm kontrolü |
| `just telemetry-coverage` | Hangi hata sinyali Sentry'ye ulaşmıyor (kapsam varsa çıkış kodu ≠ 0) |
| `just telemetry-selftest` | Gerçek yoldan hata + QML betik hatası + UI donması (`--qml --hang`) gönderir, üçünün Sentry'de varlığını **ve** olayın hiçbir yerinde (modül listesi dahil) kullanıcı adı kalmadığını doğrular |
| `just triage` | Sıralı iş listesi: production, son 14 gün (`--period`), kullanıcı sayısına göre; açık fatal, regresyon (`substatus=regressed`), ölü alarm |
| `just sentry-setup` | İş akışı alarmlarını, gruplama kurallarını, sunucu tarafı yol temizliğini ve crash-free dedektör eşiklerini kurar/onarır, sunucudan geri okuyup doğrular |
| `just telemetry-watch` | Şu an kör müyüz: Sentry kota yüzünden olay düşürüyor mu, sayım kanalı sustu mu (CI'da her gün) |
| `python scripts/set_sentry_token.py` | Token iptal/yenilendiğinde: panodaki token'ı doğrular, `.env` + GitHub secret'ı değiştirir (sahibi çalıştırır) |

Telemetriye dokunan her değişiklikten sonra `just telemetry-check`. Derleme başarısı kanıt değildir.

---

## İki kanal (2026-09-23)

Ölçüm: Sentry'nin aylık 5.000 olaylık kotası her sıfırlanmadan 6-9 gün sonra doluyordu.
30 günde 11.617 olayın 6.592'si düşürüldü, son 61 günün 37'sinde **hiç veri yoktu**.
Kabul edilenlerin %69'u launcher'ın bilerek yaptığı retlerdi (`unsupported`), gerisi
tekrardı (Elden Ring: 122 kullanıcı, 744 olay). Aynı sırada kendi ucumuz
(`/api/v2/telemetry`) Mart'tan beri hiçbir şey yazmıyordu: `/api/v2/*` Hono uygulamasına
gidiyor, oradaki işleyici launcher'ın göndermediği alanları okuyup yazmayı beklemiyordu.
İkisi de haftalarca kimse fark etmeden kördü.

| Kanal | Ne taşır | Nerede |
|---|---|---|
| **Sayım** | Her işlemin sonucu: `session` `sync` `scan` `download` `install` `uninstall` … → `ok` / `fail` (+ `side`, `reason`) / `cancel`. Başarılar dahil — başarı oranı artık hesaplanabiliyor. Kota yok. | Launcher `TelemetryService` → `POST makineceviri.org/api/v2/telemetry` (şema v2, toplu) → D1 `makine-catalog.telemetry_events` |
| **Teşhis** | Yalnız çökme ve beklenmeyen hata, bağlamıyla (breadcrumb, etiketler). | Sentry, **kapıdan** geçerek |

**Sentry kapısı** (`telemetryrules.h`, `CrashReporter::reportFailure`):
- `unsupported` retler Sentry'ye hiç gitmez — sayım kanalında `side=unsupported` + `reason` ile sayılır
  (hangi handler'ı önce yazmalı sorusunun cevabı artık oradan).
- Aynı `(operation, reason, subject)` kurulum başına **24 saatte bir** gider; `qCritical` için aynı kural.
- Oturum başına en fazla 30 olay. Çökmeler (crashpad) ve `selftest` kapıdan geçmez.
- Her hata, kapının kararından **önce** sayım kanalına gider (`CrashReporter::setFailureSink`) — sayılar eksiksiz.
- QML betik hataları: anahtar (`qml|dosya:satır|tür`) kurulum başına **7 günde bir**, oturumda en fazla 3;
  UI donması oturumda bir, kurulum başına günde bir. İkisi de sayım kanalına gitmez.
- Bütçe (2026-10-22): Ekim'de 1.242 kabul edilen hata olayı (50K'nın %2,5'i), kota yüzünden düşen 0.
  Kapı kör bırakmıyor (her farklı hata ilk gün gelir, sayılar sayım kanalında tam); yeni sinyallerin
  en kötü hali (her ekranda bozuk bir QML satırı) ayda ~3K olay.

**Sayım kanalı ayrıntıları:** anonim kurulum kimliği Sentry'nin kullanıcı kimliğiyle aynı
(makine kimliğinin SHA-256'sı, ilk 16 hane) — iki kanal kurulum bazında birleştirilebilir.
Yol, kullanıcı adı, oyun listesi gönderilmez. Kayıtlar 30 sn'de bir, en fazla 50'lik
gruplar halinde gider; gönderilemeyenler kapanışta `telemetry-queue.json`'a yazılır, sonraki
açılışta gider (en fazla 500). 400/413 dönen grup atılır, diğer her hata yeniden denenir.
Worker 0.1.4'ün eski tek-olay gövdesini de kabul eder (`schema=1`, `out=attempt`).
Sağlık ucu: `GET /api/v2/telemetry/health` (yalnız toplamlar).

**Plan:** yeni org Sponsored Team (öğrenci) — ayda 50.000 hata olayı (eskisi 5.000).
Aşağıdaki kısıtlar eski org'un ücretsiz planı içindi.

**Sunucu tarafında yapılamayanlar (eski org, ücretsiz plan):** istemci anahtarına hız sınırı
(`rateLimit`) — API 200 döner ama **sessizce yok sayar** (geri okumayla görüldü);
mesaj filtresi (`filters:error_messages`) — "You do not have that feature enabled".
Yani 0.1.4'ün gürültüsü sahadan çekilene kadar kotayı yer; 0.1.5+ kapıyla korunur.

**Bekçi:** `scripts/telemetry_watchdog.py` üç sayıya bakar ve körlükte sıfır dışı çıkar:
Sentry'nin son 24 saatteki `rate_limited` sayısı, sayım kanalının son 24 saatteki kayıt
sayısı (dev kanalı hariç) ve sürüm başına **çökmesiz oturum oranı** (Sentry release health,
7 gün, ≥100 oturum, eşik %99,5 — 2026-10-22'ye kadar %98'di, taban %99,9 iken bir sürüm
20 kat sık çökebiliyordu). Oturumlar hata kotasından düşmez: 2026-09-23'te hata olaylarının
tamamı düşürülürken 0.1.4'ün oranı yine okunabiliyordu (%98,69, 2.972 oturum). `.github/workflows/telemetry-watchdog.yml` her gün 09:00'da (TR) çalıştırır —
zamanlanmış iş yalnız varsayılan daldan (`main`) koşar. Sürümler `dev`'den çıkıyor ve
`main` ile ortak geçmişi yok; bu yüzden dosya `main`'de de duruyor (PR #7) ve `dev`'i
checkout ediyor. İki kopya elle aynı tutulur. `telemetry-check`'e konmadı: o kodu
doğrular, bekçi sahayı.

---

## Neden

Launcher, indirme/çıkarma/kurulum/yedek/senkron akışlarında **~71 farklı hata mesajı**
üretiyor. Bu oturumdan önce bunların **yalnızca 1'i** Sentry'ye ulaşıyordu.

Sebep ince bir ayrıntıydı: hata yolları `qCWarning` kullanıyor, `crashreporter.cpp` ise
Qt uyarılarını yalnızca **breadcrumb**'a çeviriyordu. Breadcrumb'lar bir çökmeye iliştirilir,
tek başlarına gönderilmez. Yani uygulama çökmediği sürece — ki kurulum hataları çökme
değildir — hiçbir şey görünmüyordu.

Sonuç: her "yama kurulamadı" raporu, kullanıcının gördüğü mesaj bilinmeden geliyordu.
Cuphead/Brothers vakası tam olarak bu yüzden teşhis edilemedi.

---

## Ne Toplanıyor

Yakalama **sinyal seviyesinde** yapılıyor — 71 emit noktasına tek tek kod eklemek yerine
her servis kendi hata sinyaline bağlanıyor. Yeni bir hata yolu eklendiğinde otomatik
kapsanır, güncelliğini yitiremez.

| Operasyon | Kaynak sinyal | Kapsadığı |
|---|---|---|
| `download` | `TranslationDownloader::downloadError` | İndirme + çıkarma + şifre çözme (9 nokta) |
| `install` | `CoreBridge::packageInstallCompleted(false)`, `packageInstallError` | Kurulum, **handler eksikliği (fail-loud)** dahil |
| `uninstall` | `GameService` — 3 ret yolu + `finalizeUninstall` | Kaldırma reddi ve başarısızlığı |
| `restore` | `BackupManager::backupRestoreFailed` | Geri yükleme |
| `backup` | `BackupManager::backupError` (seçmeli yedeğin her başarısızlığı nedenini buradan söyler; `selectiveBackupCompleted(false)` 2026-10-30'dan beri ayrıca bildirilmez) | Yedekleme |
| `sync` | `ManifestSyncService::syncError` | Katalog senkronu |
| `scan` | `CoreBridge::scanError` + tarama özeti | Kütüphane taraması; katalog boş veya hiç oyun bulunamadı |
| `integrity` | `GameService::checkPatchIntegrity` | **Kurulu yamanın dosyaları kaybolmuş** — mağaza doğrulaması, oyun güncellemesi veya antivirüs karantinası |
| `startup` | `main.cpp` — QML kökü derlenemez/oluşturulamaz (`qml_load`) | İlk kareden önce kapanan launcher; önceden yalnız yerel günlükteydi |
| `qml` | Qt mesaj işleyicisi — gönderilen QML'de `TypeError`/`ReferenceError`/… (`qml_script`) | Yarım çalışan ekran. Anahtar dosya:satır + tür, kurulum başına **haftada bir**, oturumda en fazla 3; sayım kanalına gitmez (her karede tekrarlanır). Görsel/ağ uyarıları ve binding loop breadcrumb'da kalır |
| `hang` | Bekçi iş parçacığı — GUI nabzı ≥ 5 sn eskidi (`ui_hang`) | Donan pencere; donma sürerken, oturumda bir kez gider. Başlık son kendi breadcrumb'ını adlandırır (`after package: installPackage`); uyku/hata ayıklayıcı donma sayılmaz. **2026-10-27'den beri donan GUI iş parçacığının yığınıyla** (`hang.stack:yes`, "GUI" iş parçacığı): bekçi onu bir anlığına durdurur, x64 çözme bilgisiyle (`RtlVirtualUnwind`) dönüş adreslerini toplar, bırakır — arada bellek ayırmaz; çözücü bir kilitte takılırsa 250 ms sonra iş parçacığını kendisi bırakır. Kendi karelerimiz yüklenen sembollerle adlanır, Qt'ninkiler `Qt6Core.dll + 0x…` |

**Özellikle değerli:** `install` yolu, dürüstlük kapısına takılan paketleri de yakalar.
Yani *"hangi oyunun handler'ı ne sıklıkla isteniyor"* sorusu artık veriyle cevaplanabilir —
`forge_inject`, `external`, `paradox-mod` handler'larından hangisini önce yazmak
gerektiğini varsayımla değil talep sayısıyla belirleriz.

---

## Gözden geçirme — 2026-10-27 (0.2.3 Store'da)

`just triage` (14 gün, production): 100 açık issue, 1.094 olay; `system` %89. 0.2.3'ün kendi sorunları:

| Bulgu | Kullanıcı | Kök neden | Ne yapıldı |
|---|---|---|---|
| UI donması, `after download: packageReady` | 39 | Kurulumdan önce paketin dosya listesi GUI iş parçacığında çıkarılıyordu; `fs::relative` her dosyada iki yolu da kanonikleştiriyor: 5.460 dosyalık Elden Ring paketi bu makinede 5,7 sn, oyuncularda 13 sn+ | Liste ön kontrole (iş parçacığı) taşındı; `lexically_relative` (0,42 sn). Paket açarken de her girdi iki kez kanonikleştiriliyordu — artık yalnız adıyla denetleniyor (`test_mkpktar`) |
| Elden Ring `target_missing` / `patchBytes` | 16 | Elle seçilen `ELDEN RING/Game` klasörü: yanında `unins000.exe` olan bir repack'te FromSoftware kuralı üst klasöre çıkamıyor, yama `Game/Game/eldenring.exe`'yi arıyor | Ön kontrol, seçilen adımların yerinde düzenlediği dosyaları da sorar: burada yok ama çevredeki bir klasörde hepsi varsa oyun oraya bağlanır, kurulum yeniden başlar (`inPlaceTargetsFor`, `test_gamefolderrules`) |
| Başlık "1 adımda hata oluştu" | ~30 | Neden etiketlerde vardı (`fail.reason`, `fail.step`), başlıkta yoktu; triyaj listesi bunları "teşhis edilemez" sayıyordu | İlk başarısız adım başlığa katlanıyor (`foldFirstStep`); `just triage` eski issue'ların nedenini etiketten okuyup yazıyor |
| `run KUR.bat` → `tool_exit_code` (AC Shadows, RDR2) | 4 | Sentry programın çıktısının ilk 500 karakterini, yani afişini tutuyordu; neden sondaydı: oyunun başka sürümü (`FARKLI SURUM`), Windows uygulama denetimi (Device Guard) programı engelledi | Çıktının başı ve sonu günlüğe; bilinen iki neden adımın ayrıntısında ve kendi kodunda: `tool_version_mismatch`, `tool_blocked_by_policy` (taraf `user`) |
| Diğer UI donmaları (ManifestSync, oyun sayfası, RDR2 kurulumu) | ~20 | Belirsiz — breadcrumb yalnız öncesini söylüyor | Donma olayı artık GUI yığınını taşıyor: bir sonraki sürümde nerede takıldığı görünecek |

Veri tarafı (launcher değil, paketler): AC Odyssey'nin `xdelta` yamaları bazı oyuncuların sürümüyle eşleşmiyor
(`vpatch_nomatch`), AC Shadows'un aracı bazı `.forge` dosyalarını "FARKLI SURUM" buluyor — yamaların oyunun
güncel sürümüne göre yenilenmesi gerekiyor. 0.1.5'in (eski Store) en büyük issue'su RDR2 `integrity` (79 kullanıcı)
0.2.3'te 3 kullanıcıya indi.

## Gözden geçirme — 2026-10-30 (0.2.4 GitHub'da, Store 0.2.3)

`just triage` (14 gün, production): 100 açık issue; `system` %89. 0.2.4'ün GUI yığınlı donmaları (`hang.stack:yes`,
26 olay) ilk kez nerede takıldığını söyledi. Dal `fix/sentry-2026-10-30`:

| Issue (kullanıcı) | Kök neden | Ne yapıldı |
|---|---|---|
| Donma `after download: packageReady` (59) | 57'si 0.2.3: paketin dosya listesi GUI'de (0.2.4'te düzeldi, `ece5fa1`). 0.2.4'ün 2'si AC Valhalla, yığınsız; breadcrumb'da çıkış (`QQmlEngine::quit`) | Aşağıdaki çıkış beklemesi (`5e7c7ea`) — yığın olmadığı için çıkarım |
| Donma `after navigation: game 1174180` (16), `game-install 2208920` (10), `game 2208920` (7) | Yığınlı olayların en büyük grubu (8): GUI `QThreadPool::waitForDone`'da (`main.cpp` aboutToQuit). Yedek / kurulum / RDR2 programı sürerken çıkış: işçiler 30 sn'ye kadar bekleniyor, pencere ekranda donuk, bekçinin kendi aboutToQuit'i bu bekleyişten sonra çalışıyor (`closingDown()` henüz false) | Pencereler önce gizleniyor, bekçi `CrashReporter::stopHangWatchdog` ile duruyor (`5e7c7ea`) |
| Donma `after package: uninstallPackage` (6) | `uninstallPackage` GUI'de, her dosyada `isPathContained` iki yolu diske soruyordu (`resolveThroughExisting`, `DeleteFileW` yığınları; başarısız Elden Ring kurulumunun geri alınması) | `security::TreeContainment`: klasör başına bir kez. 5.453 dosyada 4,9-5,1 sn → 0,84-1,0 sn (`9fd6fd7`). Kalan süre silmenin kendisi |
| Donma `after manifest: ManifestSync` (6) ve yığınlı diğerleri | Yığınlar: GUI render iş parçacığını bekliyor (`WindowControls` → Qt6Quick `WaitForSingleObject`; açılışta `Phase 10`), tepsi menüsü (`TrackPopupMenu` yazı tipi ölçümü), sürükle-bırak COM çağrısı, klasör penceresi (`SHCreateItemFromParsingName`) ya da örnek alındığında boşta (`PeekMessage`) | Kod değişmedi: sürücü / Windows kabuğu. Render beklemesi büyürse ayrı bakılmalı |
| Elden Ring `Adım 3: patchBytes … bulunamadı` (44) | Ön kontrol patchBytes için **yazdığı** dosyayı (`Game/start_protected_game.exe`) soruyordu; SteamRIP / DODI / P2P repack'lerde o dosya yok, elle seçilen `…/Game` hiç yukarı taşınmadı, yama `Game/Game/eldenring.exe`'yi aradı (%85 `manual`) | `steprules::inPlaceSource`: patchBytes için okuduğu `src` (`e89af2c`). Doğru kökte `Game/eldenring.exe` okunamayanlar (SteamRIP, bir Steam kopyası) ortam: exe yok ya da antivirüs |
| `integrity [1245620]: 5453/5453` (36) | 90 olayın 90'ı `0.1.5.0-beta` (eski Store). 0.2.x'te aynı kayıt için olay yok (0.2.x başlığı `(e.g. …)` taşır); 0.2.4'teki Elden Ring bütünlük issue'su 1/12 (`start_protected_game.exe` — mağaza doğrulaması) | Kod değişmedi; 0.1.5 kullanıcıları Store'dan güncellendikçe sönüyor |
| RDR2 kaldırma `changed nothing (first press)` (24), `KALDIR.bat: durduruldu` (6) | Tarif kopyalanan hook yamasından (overlay, `dinput8.dll` …) KUR.bat / KALDIR.bat'a geçti; eski kaydı (işaretsiz, notsuz) "takipten önceki program kurulumu" sanıp güncellemede yeni KALDIR.bat çalıştırılıyordu | Launcher'ın kopyaladığı dosyaları listeleyen takipsiz kayıt programa verilmiyor, geri yükleme + silme kaldırıyor (`changescan::recordLeftToProgram`, `bf51e6e`) |
| `sync failed: Katalog indirilemedi: Operation timed out` (14) | 12'si 0.1.5; 0.2.4'teki 4'ü ilk açılışta (önbellek yok), `user` tarafı, 15 sn'den 5 dk'ya geri çekilerek yeniden deneniyor. 123 KB'lık index bizden 0,25 sn'de iniyor | Kod değişmedi: oyuncunun ağı. Kendi kesintimizi uptime izleyicisi yakalar |
| `backup failed [2208920]: selective backup failed; install aborted` (6) | AC Valhalla'nın forge yedeği 25-27 GB istiyor; dolu disk `backupError` (user) ile bildirilip `selectiveBackupCompleted(false)` ile bir kez de `system` olarak gidiyordu | İkinci bildirim kaldırıldı, kurulum hatası gerekli / mevcut alanı söylüyor (`007f514`) |

Veri tarafı (panel / paket):
- **RDR2 (1174180) tarifi:** `run` adımına `"args": ["--oyun", "${gamePath}"]` ve `"uninstallArgs": ["--oyun", "${gamePath}"]`
  (seviye 3, `install_methods_compat` gerekmez). Argümansız KUR.bat / KALDIR.bat `LAUNCHER`'ı tanımlamıyor: sonda
  `pause` sayfanın açık stdin'inde oyuncu Enter'a basana kadar bekliyor (`tool_timeout` 2 saat, "durduruldu"
  74 sn-3 dk sonra) ve RDR2Loc oyunu kendi arıyor (`RDR2_DIR` boş) — başka bir RDR2 kopyasına gidebilir.
- **AC Valhalla (2208920):** kurulum notuna yedek için ~26 GB boş alan gerektiği yazılmalı (tarifte `requiredFreeSpace`
  henüz yok — `yama-araclari.md` F).

Açık kalan: Bazı 0.2.4 donmaları yığınsız geldi (`guiStack` 0 döndü: çözücü 250 ms'de bitmedi ya da iş parçacığı
durdurulamadı).

Ardından (dal `fix/quit-and-uninstall`):
- **Ctrl+Q:** `Qt.quit()` → `QQmlEngine::quit` hiçbir yere bağlı değildi ("no receivers"), Ctrl+Q hiçbir şey
  yapmıyordu. `main.cpp` sinyali tepsinin yoluna, `QCoreApplication::quit`'e bağlıyor (kuyruklu); Main.qml'deki her
  çıkış `quitApp()` ile önce `forceQuit` koyuyor (yoksa "tepsiye küçült" açıkken çıkış yalnız pencereyi gizliyor).
  Derlenmiş uygulamada betikle Ctrl+Q ve tepsinin "Tamamen Kapat" komutu, tepsiye küçült açık / kapalı: dördü de
  ~0,3 sn'de çıkış kodu 0, aboutToQuit çalıştı (öncesinde Ctrl+Q 15 sn'de çıkmadı).
- **Kaldırmanın silme döngüsü iş parçacığı havuzunda:** `GameService::removePackageFiles` (`QFutureWatcher` +
  `QtConcurrent::run`, `runUndoPrograms` gibi) — `finalizeUninstall` ve başarısız kurulumun geri alınması
  (`performInstallRollback`, iki yol) sonucu GUI'de alıp eskisi gibi sürdürüyor (günlük ve kayıt
  `uninstallPackage`'ta, hata bildirimi ve mesajlar sonuçta; değişmedi). Silme sürerken kurulum reddediliyor
  (`m_removals`, kaldırma sürerken verilen mesaj), iki kaldırma `LocalPackageManager`'da sırayla (günlük ve durum
  dosyası ortak). 5.453 dosyalık sahte Elden Ring kaydı, 3'er ölçüm: GUI'nin en uzun durması 938-986 ms → 22-23 ms
  (toplam süre aynı, 985-1.058 ms); kilitli dosyada "Yama tamamen kaldırılamadı", kayıtta o dosya kalıyor.

## Sınıflandırma — gürültüyü ayırmak

Her hata eşit değil. Hepsini `error` göndermek projeyi çöp kutusuna çevirir ve gerçek
arızalar görünmez olur. `CrashReporter::isUserActionable()` mesaj içeriğine bakarak ayırır:

| Sınıf | Seviye | Örnek | Aksiyon |
|---|---|---|---|
| **user** | `warning` | Disk dolu, oyun açık, izin reddedildi, bağlantı yok, kullanıcı iptali | Ortam sorunu — üründe iş yok |
| **system** | `error` | Handler yok, paket bozuk, CDN 404, beklenmeyen yanıt | **Bizim düzeltmemiz gereken** |

Sınıflandırma çağrı yerine değil **mesaja** bakar: aynı arıza birden çok kod yolundan
yüzeye çıkabiliyor, ve "diskiniz dolu" ile "paketimiz bozuk" ayrımını yapan şey metnin
kendisi.

Sentry'de filtre: `failure.side:system` → yalnızca bizim sorunlarımız.

Qt'nin ağ hata metinleri (`Operation timed out`, `Host … not found`) 2026-10-22'den beri
kullanıcı tarafı: 0.1.5'te katalog zaman aşımı iki haftada 11 kullanıcıya `system` olarak
düştü. `HTTP/2 protocol error` bizim (0.1.6'da HTTP/1.1) olduğu için `system` kalıyor; kendi
CDN/Worker kesintimizi Sentry uptime izleyicisi (`catalog/meta`) yakalıyor.

### Çözüm metni sınıflandırmayı bozuyordu (2026-07-23 düzeltildi)

Kurulum hataları mesajın sonuna bir öneri paragrafı ekliyor:

> *Çözüm: oyunu ve Steam'i kapatın, Makine Launcher'ı **yönetici** olarak çalıştırın,
> antivirüste oyun klasörünü **izinli** yapın…*

`isUserActionable()` mesajın tamamına baktığı için bu paragraftaki "kapatın",
"yönetici", "izin" kelimeleri kullanıcı-taraflı desenlere çarpıyordu. Sonuç: **öneri
paragrafı taşıyan her kurulum hatası, gerçekte ne kırılmış olursa olsun
`failure.side:user` etiketleniyordu.**

Bu, "Widespread Failure" alarmının filtrelediği etiket. Yani kendi kodumuzdaki bir
kusur birçok kullanıcıya ulaşsa bile alarm çalmıyordu. ELDEN RING vakası tam olarak
böyle kaçtı: kök neden bizim yol kodumuzdaki Türkçe karakter bozulması, etiket `user`.

Düzeltme: sınıflandırma yalnızca `\n\nÇözüm:` **öncesindeki** metne bakıyor. Öneri
paragrafı olmayan mesajlar önceki gibi sınıflanıyor.

---

## Etiketler

Her olay şu etiketlerle gelir:

| Etiket | İçerik |
|---|---|
| `operation` | `download` · `install` · `uninstall` · `backup` · `restore` · `sync` · `scan` · `integrity` · `package` · `startup` · `qml` · `hang` |
| `subject` | appId / gameId (kurulum-kaldırmada oyun adı da eklenir) |
| `failure.side` | `user` · `system` |
| `fail.reason`, `fail.step` | Neden kodu (`failurereasons.h`), başarısız tarif adımı |
| `game.id` | Konudaki çıplak kimlik — sunucu tarafı gruplama buna bakar (klasör adı değil) |
| `game.name`, `game.store`, `install.method` | Kurulum/kaldırma: oyun adı, kaynak (`steam`/`epic`/`gog`/`manual`/`filesystem`/`registry`), kurulum yöntemi |
| `hang.context` | Donmadan önceki son kendi breadcrumb'ı |
| `os.name`, `os.version`, `os.build`, `arch`, `app.packaged` | Ortam, oturum boyunca (`msix` / `plain`) |
| `release`, `environment` | `makine-launcher@<sürüm>`; dağıtım `production`, `MAKINE_DEV_TOOLS` yapıları `development` |

**Hata etiketleri olaya özel (2026-10-22).** 0.1.6'ya kadar `operation`/`subject`/
`failure.side` ve `game.id`/`game.name` oturum kapsamına yazılıyor, sonraki her olayda
kalıyordu: bir kaldırma reddi `operation=integrity` ile geldi, başarısız kurulumdan
dakikalar sonraki bir çökme `operation=install, failure.side=system` ve oyunla geldi
(Widespread Failure alarmını da besliyordu). Artık `telemetryrules::failureTags` ile
yalnız kendi olayında; oturum kapsamında yalnız ortam etiketleri var.

Böylece iki soru doğrudan sorulabilir:
- **Hangi operasyon en çok başarısız?** → `operation` bazlı gruplama
- **Hangi oyun en çok başarısız?** → `subject` / `game.name` bazlı gruplama

---

## Gizlilik

Hata mesajları paket klasör yolunu içerir (`"... çıkarıldı: C:\Users\<ad>\..."`), yani
telemetriyi açmak Windows kullanıcı adını sızdırma riski taşıyordu.

| Önlem | Durum |
|---|---|
| Yığın çerçevelerinde yol temizliği (`beforeSend`) | Önceden vardı |
| Mesaj gövdesinde yol temizliği (`captureMessage`) | 2026-07-21 |
| `sanitizePath` tüm eşleşmeleri değiştiriyor | 2026-07-21 |
| **Breadcrumb'larda yol temizliği** (`addBreadcrumb`) | **2026-07-23 — sızıntı canlı veride görüldü** |
| **Mesaj sonunda biten yol** (`C:\Users\Ad`, ayraçsız) | **2026-07-23** |
| **Karışık ayraçlı yol** (`C:\Users\Ad/AppData`) | **2026-07-23** |
| Anonim kullanıcı kimliği (makine ID'sinin SHA-256'sı, ilk 16 hane) | Önceden vardı |
| Debug/info mesajları gönderilmiyor | Önceden vardı |
| **Modül listesinde yol temizliği** (`debug_meta.images` code_file/debug_file) | **2026-10-22 — sızıntı canlı veride görüldü** (0.1.6 ZIP Masaüstü'nden, MSIX'te OneDrive DLL'i) |
| **Sunucu tarafı yedek** (`relayPiiConfig`, `Users\<ad>` → `[scrubbed]`) | **2026-10-22** — istemcinin göremediği minidump modül listesi ve eski sürümler için |
| IP saklanmıyor (org + proje `scrubIPAddresses`) | 2026-10-22 doğrulandı |

**Modül listesi (2026-10-22).** SDK her olaya yüklü modüllerin yollarını ekliyor;
`beforeSend` yalnız yığın çerçevelerine bakıyordu. Launcher ya da enjekte bir DLL profil
altındaysa (`C:\Users\<ad>\Desktop\…\Makine-Launcher.exe`) kullanıcı adı her olayla
gidiyordu. SDK listeyi dondurulmuş veriyor (`set_by_key` sessizce başarısız), bu yüzden
liste kopyadan yeniden kuruluyor. Öz-test artık olayın tamamında maskesiz
`Users\<ad>` arıyor; profil altına kopyalanan exe ile kanıtlandı.

**Sunucu tarafı yedek.** `sentry_setup.py` bir Relay kuralı kuruyor: `$string` ve
`debug_meta.images.*.code_file|debug_file` üzerinde `Users\` sonrasındaki adı
`[scrubbed]` yapıyor (`[` ile başlayan, yani zaten maskeli ad dokunulmaz). İşaret
istemcininkinden farklı: istemci maskelemeyi bırakırsa öz-test `[redacted]`'ı bulamaz ve
düşer, sunucunun sessizce örtmesi gerilemeyi gizlemez. Kanıt (geliştirme ortamı,
sentetik olaylar): mesaj, breadcrumb, modül yolu ve gerçek bir minidump'ın 16 modül yolu
temizlendi (minidump'ta Relay ikiliyi yerinde, uzunluğu koruyarak temizliyor:
`[scrubbed]***`). Minidump'lar işlendikten sonra saklanmıyor (`storeCrashReports=0`,
ek kotasındaki "filtered: crash-report-limit" bu).

Kullanıcı adı, e-posta, oyun kütüphanesi listesi veya dosya içeriği **gönderilmez**.

### Breadcrumb sızıntısı — nasıl kaçtı

`beforeSend` yalnızca **exception yığın çerçevelerini** dolaşır; breadcrumb'lara hiç
dokunmaz. `addBreadcrumb` ise temizlik uygulamıyordu. `qCWarning` satırları breadcrumb'a
dönüştüğü ve breadcrumb'lar her olaya iliştirildiği için, mesaj gövdesi düzgün
maskelenirken hemen yanındaki breadcrumb ham yolu taşıyordu:

```
[makine.package] Process failed to start: "C:/Users/Win_11/AppData/Local/…"
[default] Could not open pipeline cache 'C:/Users/gadaroksa/AppData/Local/…'
```

Beta kullanıcılarının Windows hesap adları bu yoldan Sentry'ye ulaştı. Artık
`addBreadcrumb` de `sanitizePath`'ten geçiyor ve `just telemetry-selftest` bunu her
çalıştırmada uçtan uca doğruluyor — öz-test bilinen bir kullanıcı adı ekiyor, olayın
içinde o adın **bulunmadığını** kanıtlıyor.

---

## Triyaj Akışı

1. **Filtre:** `failure.side:system` — kullanıcı ortamı kaynaklı gürültüyü ele
2. **Sırala:** olay sayısına göre — en çok kullanıcıyı etkileyen önce
3. **Grupla:** `operation` etiketiyle — sorun indirmede mi, kurulumda mı, yedekte mi
4. **Eşle:** `subject` etiketindeki appId ile `assets/packages/{id}.json` — kurulum
   yöntemi ve paket yapısı oradan görülür
5. **Karar:** kod hatası mı, paket verisi mi, yoksa ürün kararı mı

---

## Doğrulama — "derleniyor" kanıt değildir

Telemetri, **mükemmel derlenip yine de tamamen ölü olabilir.** Bu tam olarak yaşandı:

DSN, yapılandırma anında `MAKINE_SENTRY_DSN` ortam değişkeninden okunuyordu. `just` bunu
`.env`'den yüklüyor, ama düz `cmake --preset dev` yüklemiyor. Sonuç:

```
SENTRY_DSN=\"\"
MAKINE_SENTRY_DSN not set — Sentry crash reporting will be disabled at runtime
```

`MAKINE_HAS_SENTRY` tanımlıydı, kod derlendi, uygulama açıldı, `sentry_init` başarılı
döndü — **ve her rapor hiçbir yere gitmedi.** Bir teşhis özelliği için mümkün olan en kötü
başarısızlık biçimi: sessizce çalışmıyor ama her şey sağlıklı görünüyor.

### Kalıcı önlem

1. **`.env` yedeği:** `qml/CMakeLists.txt` ortam değişkeni yoksa `.env` dosyasını kendisi
   okur. DSN artık build'in nasıl başlatıldığından bağımsız.
2. **Gürültülü uyarı:** DSN yine de bulunamazsa yapılandırma çerçeveli, açık bir uyarı
   basar — sessiz `message(WARNING)` gözden kaçtığı için.
3. **Öz-test:** dev derlemelerinde
   ```
   Makine-Launcher.exe --selftest-telemetry
   ```
   Gerçek yolu kullanır — aynı `initialize()`, aynı `reportFailure()`, aynı transport —
   tek olay gönderir, kuyruğu boşaltır ve çıkar.

### Kanıtlanan zincir (2026-07-21)

| Adım | Kanıt |
|---|---|
| DSN gömülü | `build.ninja` içinde `SENTRY_DSN="https://…ingest.de.sentry.io/…"` |
| `initialize()` çalışıyor | `AppData/…/Makine-Launcher/sentry-db/` dizini oluştu |
| Oturum gönderiliyor | Sentry `sessions=1`, zaman damgası çalıştırma saatiyle eşleşti |
| Ağ yokken kuyruğa alınıyor | `sentry-db/<uuid>.run/session.json` diskte belirdi |
| **Olay ulaşıyor** | Sentry'de `selftest failed [telemetry]: …` issue'su, **`ev=2`** (iki çalıştırma, sayaç arttı) |
| Seviye doğru seçiliyor | Olay `error` olarak geldi (mesaj kullanıcı kaynaklı değil) |

Ortam değişkeni **tanımsızken** temiz yapılandırma da doğrulandı:
`-- MakineLauncher: Sentry DSN configured (telemetry active)`

> **Kural:** Telemetriyi etkileyen her değişiklikten sonra `--selftest-telemetry`
> çalıştırılıp Sentry'de olayın göründüğü teyit edilmeli. Derleme başarısı yeterli değildir.

---

## Sentry Proje Yapılandırması

| Öğe | Durum |
|---|---|
| GitHub entegrasyonu | ⏳ yeni org'da (`makineceviri`) kurulu değil — tarayıcıdan OAuth ister; eski org'da aktifti |
| Release takibi (`deploy.py` → `sentry-cli releases`) | ✅ mevcut |
| Debug sembolleri yükleme | ✅ `just release-zip-dynamic` / `msix-dynamic` içinde (`upload_symbols.py`: Debug ID eşleşmesi + geri okuma). Önceden yalnız `deploy.py`'deydi, yayın akışı çalıştırmıyordu. |
| Ortam | ✅ dağıtım yapıları `production`, `MAKINE_DEV_TOOLS`'lu yapılar (dev preset'leri, selftest) `development`. Dev preset'i Release (NDEBUG) derlendiği için önceden her dev çalıştırması `production`'a düşüyordu. |
| Uyarı kuralları | ✅ iş akışı motorunda 6 iş akışı + crash-free dedektörü, hepsi e-posta, hepsi `production` (2026-10-22, aşağıda) |
| Gruplama kuralları | ✅ kurulum/kaldırma: `operation + fail.reason + game.id` (2026-10-22, aşağıda) |
| Sunucu tarafı yol temizliği, IP | ✅ `relayPiiConfig`, `scrubIPAddresses` (2026-10-22, Gizlilik) |
| Spike koruması | ✅ açık (`quotas:spike-protection-disabled=false`, `sentry-setup` doğruluyor) |
| Gelen filtreler | Yerel (native) proje: tarayıcı filtreleri anlamsız, mesaj filtresi gerekmedi — üçüncü taraf gürültüsü görülmedi |

### İş akışları ve gruplama (2026-10-22)

`sentry_setup.py` hâlâ eski `/projects/.../rules/` ucuna yazıyordu; o uç 404 verdiği için
`just sentry-setup` 2026-09'dan beri hiçbir şey kurmuyordu (kurallar arayüzden/elden
taşınmıştı). Artık `/organizations/makineceviri/workflows/` ve `/detectors/` üzerinden
kuruyor, onarıyor, geri okuyor:

| İş akışı | Tetik | Süzgeç | Sıklık |
|---|---|---|---|
| New crash (fatal) → Email | yeni issue | seviye ≥ fatal | 30 dk |
| Regression Detected | çözülmüş issue olay alır | — | 30 dk |
| Widespread Failure | 1 saatte 3+ kullanıcı | `failure.side:system` | 60 dk |
| Spreading Failure (24h, 10+ users) | 1 günde 10+ kullanıcı | `failure.side:system` | günde 1 |
| Crash Spreading (1h, 3+ users) | 1 saatte 3+ kullanıcı | seviye ≥ fatal | 60 dk |
| Send a notification for high priority issues | Sentry'nin kendi önceliği (yeni + tırmanan) | — | — (yönetilmiyor, yalnız bildirim gönderebildiği doğrulanıyor) |
| Crash-free oturum oranı (24 sa) | metrik dedektör, production | < %99,5 orta, < %99 yüksek | "Monitors" iş akışı → takım e-postası |

- "New issue (error+)" fatal'a indirildi: Sentry'nin high priority iş akışı her yeni hata
  issue'sunu zaten bildiriyordu, her yeni hata iki e-posta oluyordu (günde ~4 yeni issue).
- "Spreading Failure" saatlik eşiğin kaçırdığını yakalar: Valhalla'nın yedek reddi günde
  ~30 kullanıcıya (11 günde 326) — saatte bir — ulaşıyordu.
- API metrik dedektör **oluşturamıyor** (POST 405); crash-free dedektörü yoksa script
  tarayıcı adımını yazdırır, varsa eşiklerini onarır.

**Gruplama.** Hata olaylarının varsayılan gruplaması mesaj (sayılar parametreleşir),
mesaj da kullanıcının klasör adını taşıyor: tek Diablo II kusuru 30 günde 8 issue
("Diablo II: Resurrected", "… [FitGirl Repack]", "Diablo"…) ve hiçbiri kullanıcı
eşiklerine ulaşmıyordu. Kurallar (`FINGERPRINT_RULES`):

```
tags.fail.reason:other -> {{ default }}
tags.operation:install tags.fail.reason:* tags.game.id:* -> launcher-failure, install, {{ tags.fail.reason }}, {{ tags.game.id }}
tags.operation:uninstall … -> launcher-failure, uninstall, …
!release:makine-launcher@0.1.5* !release:makine-launcher@0.1.6* tags.fail.reason:* tags.game.id:* -> launcher-failure, {{ tags.operation }}, …
```

0.1.5/0.1.6'da `game.id` yalnız kurulum/kaldırmada güvenilir (raporun hemen öncesinde
yazılıyordu); diğer işlemler 0.1.7'den itibaren katılıyor. Kanıt (sentetik geliştirme
olayları): iki farklı oyun adı tek gruba düştü; 0.1.5 sürümlü `restore` olayları iki
gruba (varsayılan), 0.1.7 sürümlüler tek gruba. Kural değişince açık kurulum/kaldırma
issue'ları yeni olay almaz, yeni gruplar açılır (bir kerelik).

### Alarm kuralları sessizce ölüydü (2026-07-23)

Dört kural da "aktif" görünüyordu ama üçünün `actions` listesi **boştu** — koşullar
değerlendiriliyor, hiçbir bildirim gitmiyordu.

Sebep: `sentry_setup.py` aksiyon olarak `NotifyEventAction` ("legacy entegrasyonlar
üzerinden bildir") kullanıyordu. Sentry bu id'yi kabul edip kuralı kaydediyor, ama
kurulu legacy entegrasyon olmadığı için aksiyonu **sessizce düşürüyor**. API 200
döndüğü için script başarı raporluyordu.

Bu, telemetride ikinci kez görülen aynı arıza sınıfı: her şey sağlıklı görünürken
hiçbir şey çalışmıyor. Alınan önlemler:

| Önlem | Nasıl |
|---|---|
| Çalışan aksiyon | `NotifyEmailAction` — entegrasyon gerektirmez |
| Onarım, sadece oluşturma değil | Kural varsa ve bozuksa `PUT` ile düzeltilir; "ismi varsa atla" yetmiyordu |
| Sunucudan geri okuma | `verify_alert_rules()` kuralları tekrar çeker, aksiyonsuz kalan varsa çıkış kodu ≠ 0 |
| Sürekli denetim | `just telemetry-check` ölü kural bulursa hata verir |

O günkü kurallar (güncel hali: yukarıda "İş akışları ve gruplama"):

| Kural | Koşul | Eşik |
|---|---|---|
| New Crash → GitHub Issue | ilk görülen issue, seviye ≥ error | günde 1 |
| Regression Detected | çözülmüş issue yeniden olay alırsa | 30 dk |
| Widespread Failure | `failure.side:system` + 1 saatte **3+** kullanıcı | 60 dk |
| High priority issues | Sentry'nin kendi önceliklendirmesi | 30 dk |

> "High priority issues" Sentry'nin varsayılan kuralı ve yeni iş akışı motorunda duruyor:
> eski `/projects/.../rules/<id>/` ucu `PUT`'a 404 veriyor. Değişiklik
> `/organizations/makineceviri/workflows/<id>/` üzerinden yapılır (GET → yalnız
> `environment`'ı değiştir → PUT → geri oku). 2026-10-22'den beri diğerlerinin hepsi
> `sentry_setup.py` ile bu uçtan yönetiliyor (PUT kimliksiz koşul/aksiyon kabul ediyor).

> Eşik 10'dan **3'e** indirildi: projedeki en yaygın kusur (`.forge` enjeksiyonu) en
> yüksek noktada 8 kullanıcıya ulaştı — 10'luk kapı en büyük sorunumuzda hiç açılmazdı.

---

## Bilinen Durum (2026-07-23)

`0.1.2-beta` dağıtıldı ve telemetri beklendiği gibi çalıştı: **36 issue · 536 olay**.
Genişletmeden önce toplam 3 issue vardı — yani kapsam sorunu değil, görünürlük sorunuydu.

Operasyon dağılımı: `install` 425 · `backup` 30 · `uninstall` 18 · `scan` 8.

### En büyük sorun bir hata değil, eksik bir özellik

Olayların çoğunluğu "kurulamıyor, çünkü bu kurulum yöntemi desteklenmiyor" diyor.
Telemetrinin kurulma amacı buydu — hangi handler'ı önce yazacağımızı varsayımla değil
talep sayısıyla belirlemek:

| Eksik handler | Olay | Oyun | Örnekler |
|---|---|---|---|
| `.forge` enjeksiyonu | 243 | 2 | AC Odyssey (201), AC Valhalla (42) |
| `script` kurulumu | 123 | 12 | RE7, Mafia DE, Brothers, Road 96, Wolf Among Us, AC Mirage… |
| `paradox` modu | 2 | 1 | Stellaris |

`.forge` iki oyunda yoğunlaşıyor ama olay sayısı en yüksek; `script` daha az olay
üretiyor, buna karşılık **12 ayrı oyuna** yayılıyor. Kapsam mı, yoğunluk mu önce —
bu artık bir ürün kararı, tahmin değil.

### ELDEN RING — "1 adımda hata oluştu" (kök neden bulundu, düzeltildi)

48 olay / 8 kullanıcı, tek satırlık bilgisiz bir başlık. Gövde ve breadcrumb'lar kök
nedeni veriyordu:

```
Türkçe Yama — Adım 3: run ERING_TR.exe
[makine.package] Option subDir not found, falling back to package root: ".../Elden Ring/Türkçe Yama"
[makine.package] Process failed to start:  ".../Elden Ring/T_rk_e Yama/ERING_TR.exe"
```

Zincir:

1. Paketleme aracı, tar'a yazarken ASCII olmayan karakterleri `?` yapıyor —
   `Türkçe Yama` arşivde `T?rk?e Yama` oluyor.
2. `mkpkformat.h` çıkarırken `?` karakterini `_` ile değiştiriyor, çünkü Windows onu
   joker olarak reddeder. Diskte **`T_rk_e Yama`** oluşuyor. Bu bilinçli bir davranış.
3. Reçete hâlâ orijinal adı taşıyor (`"subDir": "Türkçe Yama"`), ve `installWithOptions`
   birebir eşleştirme yapıyordu → **bulunamıyor**.
4. Kod paket köküne düşüyordu. `optionDir`, `executeStep`'e `packageDir` olarak geçtiği
   için bu aynı zamanda **çalışma dizinini** de kaydırıyordu: `ERING_TR.exe` yanlış
   yerden başlatılıyordu.
5. Kullanıcı "1 adımda hata oluştu" ve **antivirüsünü kontrol etmesini** söyleyen bir
   öneri görüyordu.

**Düzeltme (launcher tarafı):** karşılaştırmanın her iki tarafı da çıkarıcının kendi
dönüşümünden geçiriliyor (`extractorMangledName`). Launcher zaten `?`→`_` dönüşümünü
yapıyordu ama arama tarafında bunu unutuyordu — asimetri buradaydı. Paketleri yeniden
üretmeye gerek yok; doğru paketlenmiş arşivler de aynı yoldan eşleşmeye devam ediyor
(8/8 birim testi).

> **Kalıcı çözüm paketleme tarafında:** tar'ı UTF-8 adlarla üretmek gerekir. Launcher
> düzeltmesi mevcut bozuk paketleri kurtarır, bozulmayı ortadan kaldırmaz.

**Ayrıca:** `runProcess` başlatma hatasının sebebini kaydetmiyordu. "Process failed to
start" satırı eksik dosyayı, engellenen ikiliyi ve reddedilen izni aynı şekilde
gösteriyordu. Artık `QProcess::error()`, `errorString()`, dosya varlığı/boyutu ve
çalışma dizini raporlanıyor.

### Katalog boş açılıyor — yanlış pozitifti

5 olay / 5 ayrı kullanıcı, her birinde bir kez. Sebep: `scanAllLibraries` katalog
boyutunu ölçerken `index.json` henüz indirilmemiş oluyordu. Kod bunu zaten biliyordu
("No cached index yet, waiting for sync…") ama 200 satır aşağıda aynı durum `error`
olarak raporlanıyordu.

Gerçek arıza değil: `catalogReady` gelince `refreshPackageManifest()` kataloğu yüklüyor
ve oyunlar yeniden değerlendiriliyor. Yani her yeni kullanıcının ilk açılışı bir hata
üretiyordu — ve gerçek bir senkron arızası bundan ayırt edilemiyordu.

Artık ayrılıyor: **indeks dosyası diskte var ama katalog boş** → gerçek arıza, raporlanır.
**İndeks henüz yok** → breadcrumb, rapor yok.

### Diğer açık başlıklar

| Bulgu | Olay / kullanıcı | Not |
|---|---|---|
| `scan failed [catalog]: Çeviri kataloğu boş` | 5 olay / **5 ayrı kullanıcı** | `catalog=0` ile açılıyor; `syncError` hiç tetiklenmemiş — senkron sessizce başarısız |
| `No backup available — refusing uninstall` | 10 olay / 3 kullanıcı | Kullanıcı yamayı kaldıramıyor |
| `selective backup failed; install aborted` | 15 olay / 1 kullanıcı | 277/1269 dosya kopyalandı, Epic klasöründe kilit |
| `RtlpHpSegReAlloc` (fatal) | 43 olay / 2 kullanıcı | **resolved işaretli ama 2 ay olay almış** — regresyon alarmı o dönemde ölüydü |
| `WideCharToMultiByte` (fatal) | 1 olay | 2026-07-23, yeni; heap ailesinin devamı olabilir |

---

## Açık İşler

**Launcher — çözüldü (2026-07-23):**
- [x] ELDEN RING: bozuk klasör adı eşleştirmesi (`extractorMangledName`)
- [x] `run` adımı başlatma hatasının sebebi kaydediliyor
- [x] Katalog boş açılma yanlış pozitifi ayrıldı
- [x] Yedeksiz kaldırmada kayıt sayısı raporlanıyor (hangi tür "yedek yok" olduğu)

**Ürün — telemetrinin işaret ettiği (launcher dışı):**
- [ ] `.forge` enjeksiyon handler'ı (243 olay, en yoğun)
- [ ] `script` kurulum handler'ı (123 olay, 12 oyuna yayılmış)
- [ ] Paketleme: tar adlarını UTF-8 üret — `?` bozulmasının kaynağı burası
- [ ] Yedeksiz kaldırma çıkmazı: sonraki sürümün verisine göre karar (kayıt hiç yok mu,
      geçersiz mi)

**Telemetri altyapısı:**
- [x] Kurulum **başarı** oranı ölçümü — sayım kanalı (2026-09-23)
- [x] Körlük bekçisi — `telemetry_watchdog.py` + günlük CI (2026-09-23)
- [ ] Sayım kanalı ucu kimliksiz: IP bazlı Cloudflare Rate Limiting kuralı
- [ ] `RtlpHpSegReAlloc` kapatma kararını gözden geçir (regresyon alarmı artık çalışıyor)
- [ ] `just telemetry-check`'i pre-push kancasına bağla
