# Telemetri — Sentry ile Uzaktan Teşhis

> **Amaç:** Kullanıcı geri bildirimi beklemeden sorunları görmek.
> **Proje:** `makine-ceviri / native` · **Durum:** 2026-07-23 — beta verisi geldi, denetim otomatikleşti

## Komutlar

| Komut | Ne yapar |
|---|---|
| `just telemetry-check` | Tümü: kapsam denetimi + canlı öz-test + ölü alarm kontrolü |
| `just telemetry-coverage` | Hangi hata sinyali Sentry'ye ulaşmıyor (kapsam varsa çıkış kodu ≠ 0) |
| `just telemetry-selftest` | Gerçek yoldan bir olay gönderir, Sentry'de varlığını **ve** yol temizliğini doğrular |
| `just triage` | Sıralı iş listesi: en çok başarısız operasyon, en çok istenen handler, regresyon |
| `just sentry-setup` | Alarm kurallarını kurar/onarır, sonucu sunucudan geri okuyup doğrular |

Telemetriye dokunan her değişiklikten sonra `just telemetry-check`. Derleme başarısı kanıt değildir.

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
| `backup` | `BackupManager::backupError`, `selectiveBackupCompleted(false)` | Yedekleme |
| `sync` | `ManifestSyncService::syncError` | Katalog senkronu |
| `scan` | `CoreBridge::scanError` + tarama özeti | Kütüphane taraması; katalog boş veya hiç oyun bulunamadı |
| `integrity` | `GameService::checkPatchIntegrity` | **Kurulu yamanın dosyaları kaybolmuş** — mağaza doğrulaması, oyun güncellemesi veya antivirüs karantinası |

**Özellikle değerli:** `install` yolu, dürüstlük kapısına takılan paketleri de yakalar.
Yani *"hangi oyunun handler'ı ne sıklıkla isteniyor"* sorusu artık veriyle cevaplanabilir —
`forge_inject`, `external`, `paradox-mod` handler'larından hangisini önce yazmak
gerektiğini varsayımla değil talep sayısıyla belirleriz.

---

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
| `operation` | `download` · `install` · `uninstall` · `backup` · `restore` · `sync` · `scan` |
| `subject` | appId / gameId (kurulum-kaldırmada oyun adı da eklenir) |
| `failure.side` | `user` · `system` |
| `game.id`, `game.name` | Oyun bağlamı (kurulum/kaldırma yollarında) |
| `os.name`, `os.version`, `arch` | Ortam (başlangıçta ayarlanır) |
| `release` | `makine-launcher@<sürüm>` |

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
| GitHub entegrasyonu (`MakineCeviri`) | ✅ aktif |
| Release takibi (`deploy.py` → `sentry-cli releases`) | ✅ mevcut |
| Debug sembolleri yükleme (`deploy.py`) | ✅ mevcut |
| Uyarı kuralları | ✅ 4 kural, **dördü de bildirim gönderiyor** (2026-07-23 onarıldı) |

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

Aktif kurallar:

| Kural | Koşul | Eşik |
|---|---|---|
| New Crash → GitHub Issue | ilk görülen issue, seviye ≥ error | günde 1 |
| Regression Detected | çözülmüş issue yeniden olay alırsa | 30 dk |
| Widespread Failure | `failure.side:system` + 1 saatte **3+** kullanıcı | 60 dk |
| High priority issues | Sentry'nin kendi önceliklendirmesi | 30 dk |

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
- [ ] Kurulum **başarı** oranı ölçümü — şu an yalnızca hatalar toplanıyor, oran hesaplanamıyor
- [ ] `RtlpHpSegReAlloc` kapatma kararını gözden geçir (regresyon alarmı artık çalışıyor)
- [ ] `just telemetry-check`'i pre-push kancasına bağla
