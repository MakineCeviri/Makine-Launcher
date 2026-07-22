# Telemetri — Sentry ile Uzaktan Teşhis

> **Amaç:** Kullanıcı geri bildirimi beklemeden sorunları görmek.
> **Proje:** `makine-ceviri / native` · **Durum:** 2026-07-21 itibarıyla kapsam genişletildi

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
| `scan` | `CoreBridge::scanError` | Kütüphane taraması |

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
| **Mesaj gövdesinde yol temizliği** (`captureMessage`) | **Bu oturumda eklendi** |
| `sanitizePath` tüm eşleşmeleri değiştiriyor | **Düzeltildi** (önceden ilkinde duruyordu) |
| Anonim kullanıcı kimliği (makine ID'sinin SHA-256'sı, ilk 16 hane) | Önceden vardı |
| Debug/info mesajları gönderilmiyor | Önceden vardı |

Kullanıcı adı, e-posta, oyun kütüphanesi listesi veya dosya içeriği **gönderilmez**.

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
| Uyarı kuralı | ⚠️ **yalnızca 1** — "high priority issues" → e-posta (30 dk) |

### Önerilen ek uyarı kuralları

Mevcut tek kural yalnızca yüksek öncelikli olayları yakalıyor; yeni telemetriyle gelen
`failure.side:system` olayları büyük ihtimalle bu eşiğin altında kalır.

1. **Yeni sistem hatası** — `failure.side:system` etiketli ilk görülen issue → bildirim
2. **Yaygınlaşan hata** — bir issue 1 saatte 10+ kullanıcıya ulaşırsa → bildirim
3. **Regresyon** — çözülmüş bir issue yeniden olay alırsa → bildirim + GitHub issue

`scripts/sentry_setup.py` bu kuralları API üzerinden kurmak için hazır; çalıştırmadan
önce e-posta trafiği açısından gözden geçirilmeli.

---

## Bilinen Durum (2026-07-21)

Projede 3 issue var, üçü de Windows heap bozulması imzası taşıyor:

| Issue | Olay | Kullanıcı | Son görülme | Durum |
|---|---|---|---|---|
| `RtlpHpSegReAlloc` | 43 | 2 | 2026-07-13 | **"resolved" ama olay almaya devam etmiş** |
| `memset$thunk$…` | 1 | 1 | 2026-07-16 | unresolved |
| `RtlCompactHeap` | 1 | 1 | 2026-07-05 | unresolved |

Üçü de aynı sınıf (heap bozulması) — muhtemelen **tek bir kök nedenin** farklı yerlerde
patlaması. `RtlpHpSegReAlloc` kapatılmış olmasına rağmen aylarca olay almış; kapatma
kararı gözden geçirilmeli.

**Not:** Toplam olay sayısının bu kadar düşük olması sorunların az olduğunu değil,
telemetrinin kapsamının dar olduğunu gösteriyordu. Bu oturumdaki genişletmeden sonraki
ilk sürümle birlikte gerçek hata dağılımı görünür hale gelecek.

---

## Açık İşler

- [ ] Uyarı kurallarını genişlet (`sentry_setup.py`)
- [ ] Kurulum **başarı** oranı ölçümü — şu an yalnızca hatalar toplanıyor, oran hesaplanamıyor
- [ ] `RtlpHpSegReAlloc` kapatma kararını gözden geçir
- [ ] İlk sürümden sonra: `failure.side:system` dağılımına göre handler önceliklendirmesi
