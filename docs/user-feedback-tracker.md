# Kullanıcı Geri Bildirim Takibi — v0.1.0-beta

> **Kaynak:** `Makine Çeviri feedback.txt` (Discord/DM toplaması)
> **Analiz tarihi:** 2026-07-21
> **Durum:** 21 madde · 3 çözüldü · 3 yayın bekliyor · 2 kısmi · 12 açık · 1 öneri

---

## Kritik Bulgu — Teslimat Hattı Kopuk

Analizin en önemli sonucu, maddelerin tek tek içeriğinden daha belirleyici:

| Ölçüm | Değer |
|---|---|
| Kullanıcıların elindeki son sürüm | **v0.1.0-beta — 26 Mart 2026** |
| O tarihten sonra yazılan `fix` commit'i | **192** |
| Bunlardan kullanıcıya ulaşan | **0** |
| `main` HEAD | 20 Mayıs 2026 |
| `dev` HEAD | 2 Temmuz 2026 |

Aşağıdaki maddelerin **4'ü aylardır `dev` dalında çözülmüş halde bekliyor.** Kullanıcılar
4 aylık bir yapıyı kullandığı için zaten düzeltilmiş hataları raporlamaya devam ediyor.

> Sorun kod kalitesi değil, **yayınlanmamış olması**.

---

## Durum Özeti

| Durum | Sayı | Anlamı |
|---|---|---|
| ✅ Çözüldü | 3 | Bu oturumda düzeltildi ve derlendi |
| 🟡 Yayın bekliyor | 3 | Kodda çözülü, sadece release gerekiyor |
| 🟠 Kısmi / doğrulanmadı | 2 | Düzeltme var ama eksik veya test edilmedi |
| 🔴 Açık | 12 | Henüz çözülmedi |
| 💡 Öneri | 1 | Yeni özellik talebi |

---

## ✅ Çözülenler

### F-05 — Yama kaldırılınca oyun bozuk kalıyor `[satır 10]`

> *"yamayı kaldıra bastığımızda yama kaldırıldı yazsa da ... orijinal dosyaların tekrar
> yerine koyulmaması ve oyunun bozuk kalmaya devam etmesi"*

**Önem:** Kritik — veri kaybı. Kullanıcının oyunu bozuk kalıyor ve uygulama "başarılı" diyor.

**Kök neden:** `localpackagemanager.cpp:2004-2010` — `uninstallPackage`, kurulumda
üzerine yazdığı orijinal dosyaları (`replacedFiles`) bilinçli olarak **silmiyor**, çünkü
`restoreBackup`'ın onları geri koyacağını varsayıyor. Ancak `gameservice.cpp`'de iki kod
yolu bu varsayımı çiğniyordu:

1. `hasBackup()` false → restore hiç denenmiyor, ama kaldırma yine de çalışıyordu
2. `restoreBackup()` başlatılamıyor → `"proceeding with uninstall anyway"` ile devam ediyordu

Her iki durumda da yama dosyaları oyunda kalıyor, UI *"Yama başarıyla kaldırıldı"* diyordu.
`backupmanager.h:160` bu sözleşmeyi zaten yazıyordu: *"tüketici bunu başarısızlık saymalı."*

**Çözüm:** `gameservice.cpp` — her iki yolda da, kurulum orijinal dosyaların üzerine
yazmışsa (`translationReplacedOriginalFiles()`) kaldırma **reddediliyor** ve kullanıcıya
eyleme dönük mesaj veriliyor:

> *"Yama kaldırılamadı: bu yama oyunun orijinal dosyalarının üzerine yazmış ve geri
> yüklenecek bir yedek bulunamadı. Oyun dosyalarını mağaza üzerinden doğrulayın."*

Yalnızca dosya **ekleyen** (overlay) kurulumlarda kaldırma eskisi gibi çalışıyor — orada
risk yok. CDN denetimine göre 237 paketin **166'sı** overlay-safe, yani çoğunluk etkilenmiyor.

**Doğrulama:** `cmake --build --preset dev` → başarılı, link temiz.

#### F-05b — Yedeğin yanlış dosyaları kapsaması (asıl kök neden)

İlk düzeltme "yedek yoksa kaldırmayı reddet" davranışını getirdi, ama **yedeğin neden
yok/boş olduğu** sorusu açıktı. Kök neden bulundu:

`package_catalog.cpp:550-573` — overlay paketlerinde yedeklenecek dosya listesi doğrudan
`dataPath/dirName[/variant]` dizini taranarak üretiliyor, yollar paketin içindeki haliyle.
Kurulum tarafı ise `stripWrapperPrefix()` ile hatalı paketlenmiş sarmalayıcı klasörleri
("Türkçe Yama/", oyunun kendi adı, "Apex/" …) **sıyırıyor**.

Sonuç, sarmalayıcılı her pakette:

| Aşama | Hedef |
|---|---|
| Yedek listesi | `oyun/Türkçe Yama/data/text.bin` ← oyunda böyle bir dosya **yok** |
| Kurulum | `oyun/data/text.bin` ← **gerçek dosyanın üzerine yazıyor** |

Yedek, var olmayan yolları hedeflediği için pratikte boş kalıyor ama "başarılı" dönüyor;
kurulum gerçek orijinali eziyor. Kaldırma anında geri konacak hiçbir şey yok — kullanıcının
tarif ettiği *"yama kaldırıldı yazıyor ama oyun bozuk kalıyor"* tablosu tam olarak bu.
Hafızadaki "RDR2 wrapper bug" kaydı ve RDR2'nin şikayet listesinde olması bu teşhisi
destekliyor.

**Çözüm:** `getPackageFileList()` artık `gamePath` alıyor ve kurulumun kullandığı
`stripWrapperPrefix()` mantığını **aynen** uyguluyor. Böylece yedek listesi ile kurulumun
üzerine yazacağı hedefler birebir örtüşüyor. Zincir: `gameservice.cpp` → `corebridge` →
`localpackagemanager` → `stripWrapperPrefix`.

**Neden `gamePath` şart:** Sıyırma sezgiseli, bir üst klasörün gerçekten sarmalayıcı mı
yoksa oyunun meşru dizini mi olduğunu **oyun kurulumunda o adın var olup olmadığına**
bakarak ayırt ediyor. Yol verilmezse yalnızca anahtar-kelime eşleşmesine düşer.

**Doğrulama:** Tam yeniden derleme — 166/166, 0 uyarı, link temiz.

---

### F-12 — Program hiç açılmıyor `[satır 26]`

> *"program açılmıyor ... ekrana pencere oluşturuluyor başlatılıyor vs sonra gidiyor"*

**Önem:** Kritik — kullanıcı uygulamayı hiç kullanamıyor, diğer hiçbir sorunu yaşayacak
noktaya bile gelemiyor.

**Kök neden:** Qt 6'nın RHI otomatik seçimi, sürücü bir Vulkan ICD bildirdiğinde Vulkan'ı
seçiyor. ICD eksik veya bozuksa uygulama *"Failed to initialize graphics backend for
Vulkan"* ile başlangıçta ölüyor — pencere açılıp anında kapanıyor.

**Mevcut düzeltmenin yetmediği nokta:** `5870b22` varsayılanı D3D11'e çekmişti, ancak
v0.1.0-beta'ya kadarki yapılarda şu kod `"auto"` değerini `"vulkan"`a çevirip **diske
yazıyordu**:
```cpp
if (m_graphicsBackend == "auto") {
    m_graphicsBackend = "vulkan";
    m_settings.setValue("performance/graphicsBackend", m_graphicsBackend);  // kalıcı
}
```
Yani eski sürümü bir kez çalıştıran **herkesin** ayar dosyasında açıkça `"vulkan"` yazılı.
Yeni koddaki `value(..., "auto")` varsayılanı yalnızca anahtar **yokken** devreye girdiği
için, D3D11 düzeltmesi mevcut kullanıcılara hiç ulaşmayacaktı — yani tam olarak sorunu
yaşayan kitleye.

**Çözüm:** `main.cpp` — tek seferlik migration. `graphicsBackendReset` bayrağı yoksa ve
kayıtlı değer `"vulkan"` ise `"auto"`ya çekiliyor, bayrak işaretleniyor. Bir kez çalışıyor;
kullanıcı sonradan Ayarlar'dan Vulkan'ı yeniden seçerse tercihi korunuyor.

**Sıralama doğrulandı:** `configureQtEnvironment()` (satır 1424) `QGuiApplication`'dan
(1454) önce çalışıyor; `SettingsManager` app'in child'ı olarak sonra kurulduğu için
migration'ın yazdığı değeri okuyor. Tutarsızlık yok.

**Doğrulama:** `cmake --build --preset dev` → başarılı, link temiz.

---

### F-18 — Stellaris: "mod paketini bulamadım"

> *"kurulumda hata oldu deyip mod paketini oyunun mod klasörüne çıkarın etkinleştirin
> diyor tamam anladım diyorum ama mod paketini bir türlü bulamadım"*

**Kök neden:** Stellaris paketi `installMethod.type = "paradox-mod"`. Bu tipin handler'ı
yok, dürüstlük kapısı devreye giriyor ve kullanıcıya yönlendirme mesajı gösteriliyor.
Ancak `paradox-mod` mesajı, diğer tüm yönlendirme mesajlarından farklı olarak
**`sourcePath` içermiyordu**:

```cpp
// external, installer, forge_inject → hepsinde "şu klasöre çıkarıldı: %1" var
else if (m == "paradox-mod")
    guide = tr("... Belgeler/Paradox Interactive/<oyun>/mod klasörüne çıkarın ...");
    //        ↑ hangi klasörden kopyalayacağı hiç söylenmiyor
```

Kullanıcının elindeki tek şey şifreli `.makine` dosyası — açması ya da bulması mümkün
değil. Yani kendisinden yapamayacağı bir şey isteniyordu.

**Çözüm:** `localpackagemanager.cpp` — mesaja çıkarılan klasör yolu eklendi, ifade
"oyun klasörüne kurulmaz" vurgusuyla netleştirildi.

**Doğrulama:** `cmake --build --preset dev` → başarılı.

---

## 🟡 Yayın Bekleyenler — kod hazır, sadece release gerekiyor

### F-01 — "Kurulu Oyunlar" yazısı kayıyor `[satır 1]`
Yenile'ye basınca başlık aşağı kayıp geri dönüyor.
**Çözüm:** `1ceebee` — `GameSection` başlığına sabit 24x24 aksiyon alanı + opacity geçişi.
(`BusyIndicator`'ın örtük ~48px yüksekliği satır yüksekliğini şişiriyordu.)

### F-02 — Arama alanı kapanmıyor `[satır 4]`
Başka yere tıklayınca kapanmıyor, uygulamayı yeniden başlatmak gerekiyor.
**Çözüm:** `71343e8` — blur için `TapHandler`'a geçildi (`MouseArea`,
`HorizontalGameStrip`'in altında kalıp tıklamayı yutuyordu) + katalog yeniden
oluşturma çağrıları tek zamanlamada birleştirildi.

### F-16 — Güncelleme denetiminde alakasız renk `[satır 38]`
**Çözüm:** `136b864` — üst bar tonlaması kaldırıldı, yerine sağ-alt köşede %50 opaklıklı
geçici bildirim geldi. (Kullanıcının önerdiği tasarımın birebir karşılığı.)

---

## 🟠 Kısmi / Doğrulanmamış

### F-03 — Elden Ring "2. adımda hata" `[satır 6]`
> *"bu durum çok fazla oluyor, Windows güvenlik kaynaklı olma olasılığı yüksek,
> daha çok dosyaların kopyalanması sorunu"*

**Mevcut düzeltme:** `f5e68ac` — `resolvePackageSource` yinelemeli derinleştirme 1→3;
`4045a97` — adım bazlı hata detayı (aksiyon + hedef + adım no).
**Durum:** Hipoteze dayalı, **runtime doğrulaması yapılmadı.** Önceki oturumda kullanıcı
wrapper-derinlik hipotezine şüpheyle yaklaşmıştı ("pek öyle hissetmiyorum").
CDN denetimi Elden Ring paketinin (1245620) şema olarak sağlam olduğunu gösterdi —
sorun büyük olasılıkla çalışma anında yol/izin kaynaklı.

### F-04 / F-09 — AC oyunlarında apexyama yönlendirmesi `[satır 8, 18, 28]`
> *"onlarca kişi aynı hatayı yazıyor ... apex yama sitesinde hem ücretli hem de o
> yamalar yok ... şuna bir çözüm bulun lütfen"*

**Önem:** Yüksek — itibar sorunu, tekrar eden şikayet.
**Yapılan:** `9f1a472` — hata mesajlarından üçüncü taraf yönlendirmeleri kaldırıldı.
**Kalan:** `forge_inject` handler'ı **hâlâ yok**. AC Odyssey ve Valhalla `.forge` arşiv
enjeksiyonu gerektiriyor, uygulama bunu yapamadığı için dürüstlük kapısına takılıyor.
CDN denetimi: handler'ı olmayan **19 paket** var (12 `external`, 2 `forge_inject`,
kalanı workshop/d2r_mod/paradox-mod/installer/vpatch).
**Ürün kararı gerekiyor:** handler yaz · listeden çıkar · ya da JSON'a `helpUrl`/`manualSteps`
alanı ekleyip arayüzde düzgün rehber göster.

---

## 🔴 Açık Maddeler

### F-08 / F-11 — Yama kurulduktan sonra oyun açılmıyor `[satır 16, 23]` — kök neden bulundu

> ME Andromeda: *"yamayı yükledim, açılmıyor, Steam'de çalışıyor gözüküyor ama tepki yok"*
> RDR2: aynı tablo
> Sir Brante: *"Steam Build ID 23684887 ... 'Loading...' ekranından sonra kapanıyor.
> Temiz kurulumda sorunsuz çalışıyor."*

**Paketler açılıp incelendi. Sebep kurulum kodu değil, yamanın çalışma yöntemi:**

| Paket | Üst seviye içerik |
|---|---|
| ME Andromeda | `dinput8.dll`, `meatr.dll`, `AnselSDK64.dll` (+`_org`), `Patch/` |
| RDR2 | `dinput8.dll`, `ScriptHookRDR2.dll`, `rdr2-translator.asi`, `fontfix.asi` |

Her ikisi de **DLL injection** ile çalışıyor: `dinput8.dll` proxy olarak yükleniyor ve
ASI/ScriptHook üzerinden çeviri oyun sürecine enjekte ediliyor. Kullanıcının tarifi
(*"Steam'de çalışıyor gözüküyor ama hiçbir tepki yok"*) bozuk injection'ın klasik
belirtisidir: süreç başlar, proxy DLL yüklenir, sürüm uyumsuzluğunda pencere hiç açılmaz.

ScriptHook ve ASI yükleyiciler **oyunun her güncellemesinde bozulur** — RDR2'de bu her
Rockstar yamasında yaşanır. Yani bu bir **paket/oyun sürüm uyumsuzluğu**, kurulum hatası
değil. Launcher dosyaları doğru yere koyuyor.

**Gereken çözüm (veri tarafı):** Paket şemasına hedef oyun sürümü alanı
(`verifiedBuildId` / `targetGameVersion`) eklenip kurulumdan önce karşılaştırılmalı.
`gameStoreVersion` alanı kodda zaten var — eksik olan paket tarafındaki referans değer.
Sir Brante'nin verdiği Build ID (23684887) bu mekanizmanın ilk test vakası olabilir.

**Kod tarafında yapılan:** Bu durumdaki kullanıcı en azından oyununu kurtarabilmeli →
mağaza doğrulama kısayolu eklendi (aşağıya bakın).

### F-07 — Cuphead / Brothers kurulamıyor `[satır 14]`
> *"herhangi bir yama indirmeye çalıştığımda ... yönetici olarak çalıştırmama rağmen
> düzelmiyor, ikisinde de aynı uyarı"*

**Paketler açılıp incelendi — ikisi de yapısal olarak kusursuz:**

| Paket | Üst seviye | Değerlendirme |
|---|---|---|
| Cuphead | `Cuphead_Data/` | Unity veri klasörü, oyun kökünde mevcut |
| Brothers | `P13/` | UE3 dizini, oyun kökünde mevcut |

İkisi de overlay-safe, sarmalayıcı yok, dosya yolları doğru. Kullanıcının *"herhangi bir
yama"* demesi sorunun pakete değil **kendi ortamına** özgü olduğunu gösteriyor
(disk/izin/antivirüs veya bozuk indirme).

**Teşhis edilemiyor çünkü kullanıcının aldığı tam hata mesajı hiçbir yere ulaşmıyordu.**
Bu oturumda kapatıldı: kurulum/kaldırma hataları artık Sentry'ye event olarak gidiyor
(aşağıya bakın). Bir sonraki raporda bu vakanın gerçek mesajı görünür olacak.

### F-06 — Epic Games oyunları tespit edilmiyor `[satır 12, 30]` — 🟠 iyileştirildi

> TWD Definitive Series: *"`C:\Program Files\Epic Games\TWDTTDS` klasörünü seçiyorum
> fakat Launcher oyunu tespit etmiyor"* — temiz kurulum + bir kez çalıştırma denenmiş.

**Katalog durumu:** Oyun katalogda **var** — appId `1449690`,
`dirName: "The Walking Dead The Telltale Definitive Series"`, `exe: [telltalewidescreenpatcher.exe, wdc.exe]`.
Yani yama mevcut, sorun eşleştirmede.

**İki eşleştirme yolu da tıkanıyordu:**

1. **Klasör adı:** `findMatchingAppId("TWDTTDS")` — kısaltma ile tam ad asla eşleşmez.
2. **Dosya parmak izi:** `findMatchingGamesFromFiles` yalnızca **kök dizini** tarıyordu
   (`entryInfoList`, özyinelemesiz). Unreal `"<Oyun>/Binaries/Win64/Oyun.exe"` düzeni
   kullanır, Epic kurulumları sık sık bir seviye daha ekler. Kökte `.exe` yoksa eşleştiriciye
   **boş exe listesi** gidiyor → parmak izi eşleşmesi hiç çalışamıyor → oyun desteklenmiyor
   gibi görünüyor. Manuel eklemenin de başarısız olması bunu doğruluyor: her iki yol da
   aynı boş listeye düşüyor.

**Çözüm:** `corebridge.cpp` — kökte aday `.exe` bulunamazsa sınırlı derinlikte özyinelemeli
arama devreye giriyor. Sınırlar: **derinlik ≤ 3**, **≤ 4000 girdi**, **≤ 12 exe**; büyük
kurulum dizinleri tespiti kilitleyemesin diye. Filtre mantığı (`launcher`, `crash`,
`redist`, `setup`, … ele) tek bir lambda'ya taşındı, iki yol da aynı ölçütü kullanıyor.

**Dürüst not:** Bu, eşleşme olasılığını ciddi biçimde artırır ama **kesin çözüm olarak
işaretlenemez** — kullanıcının makinesinde `wdc.exe`'nin gerçek derinliği bilinmiyor.
Doğrulamak için o kurulumdan klasör ağacı gerekiyor. Ayrıca uzun vadeli asıl çözüm,
paketlere Epic/GOG `storeIds` eşlemesi eklemek.

**Doğrulama:** `cmake --build --preset dev` → başarılı, 0 hata, 0 uyarı.

### F-10 / F-15 — İndirme çalışmıyor `[satır 20, 32]`
> *"makine launcher'a ne oldu patladı mı, indirilmiyor hiçbir şey"*
> *"Batman Arkham Knight yaması inmiyor"*

Belirsiz ifadeler; CDN erişimi, manifest senkronu veya paket bazlı sorun olabilir.
Ayrıştırma gerekiyor.

---

### F-19 — Yes Your Grace: üç ayrı sorun

> *"kütüphanede yes your grace bulamıyorum ve arama kısmına 2 harf girince kendisi
> atıyor ve oto tarama da oyunu bulamıyor"*

| Alt madde | Teşhis |
|---|---|
| **a)** Kütüphanede yok | **Yes Your Grace katalogda hiç yok.** 239 paket tarandı, eşleşme sıfır. Yama mevcut değil. |
| **b)** 2 harf girince çöküyor | 🟡 `71343e8` — `CatalogProxyModel` yeniden oluşturma çağrıları birleştirildi. Sentry'deki `RtlpHpSegReAlloc` heap bozulmasıyla (43 event) örtüşüyor. Kodda çözülü, **yayınlanmamış**. |
| **c)** Oto tarama bulamıyor | (a)'nın sonucu — katalogda olmayan oyun taramada da eşleşmez. |

**Not:** Kullanıcının oyunu web sitesinde görmüş olma ihtimali var. 15 Mayıs'ta 55 oyun
launcher'dan gizlendi ancak bu işlem **web sitesini kapsamıyordu** — web/launcher
tutarsızlığı ayrıca denetlenmeli.

### F-20 — Spider-Man 2: yama kuruldu, altyazı gelmedi

> *"spiderman 2 yamasını indirdim ancak oyunda altyazılar gelmedi"*

**Katalog durumu:** `apexTier = pro`, `source = apex`, **`checksum` yok.**
Checksum'ı olmayan tek şikayet paketi bu. Yani indirilen verinin bütünlüğü hiçbir
noktada doğrulanmıyor; eksik/bozuk inen paket "başarıyla kuruldu" olarak raporlanabilir.

Ayrıca kurulum sonrası doğrulama yok — beklenen dosyaların gerçekten yazıldığı ve
oyunun onları okuduğu kontrol edilmiyor. Bu, F-08/F-11 ile aynı sınıfta:
**sessiz başarı**.

### F-21 — Darkest Dungeon: yama kurulmuyor

**İki farklı paket var, ayrımı önemli:**

| Paket | appId | Tip | Durum |
|---|---|---|---|
| Darkest Dungeon | `262060` | `<boş>` → overlay-safe | 🔴 Kurulabilir olmalı — başka bir kök neden var |
| Darkest Dungeon **II** | `1940340` | `file-replace` | ✅ **Çözüldü** — aşağıya bakın |

DD2 ise sebep belliydi ve giderildi. DD1 ise Cuphead/Brothers ile aynı gruba giriyor:
overlay-safe olmasına rağmen kurulmuyor → ortak ve henüz bulunmamış bir kök neden.

#### `file-replace` overlay-safe kabul edildi (DD2 + DOOM + Dark Souls Remastered)

Bu tipteki **üç paketin tamamı indirilip açılarak** incelendi (AES-256-GCM çözme +
zstd + tar), varsayımla değil kanıtla karar verildi:

| Paket | Üst seviye girdiler | Sonuç |
|---|---|---|
| Darkest Dungeon II | `Darkest Dungeon II_Data/` | Unity veri klasörü, oyunda mevcut |
| DOOM (2016) | `base/`, `Mods/` | İki gerçek oyun dizini |
| Dark Souls: Remastered | `font/`, `menu/`, `msg/` | Üç gerçek oyun dizini |

Hiçbirinde sarmalayıcı klasör ya da adım tarifi yok; hepsi oyun köküne göre köklenmiş,
yapı koruyan yük. Tip adı yalnızca "mevcut dosyaların üzerine yazar" demek — ki bu zaten
overlay yolunun yaptığı ve `replacedFiles`'ın kaydettiği şey.

DOOM ve Dark Souls Remastered çoklu üst dizine sahip olduğu için sıyırma sezgiseline
takılmaz; DD2 tek üst dizinli ama o klasör oyunda var olduğundan (B) kriteri sıyırmayı
durdurur. Üçü de güvenli.

**Çözüm:** `kOverlaySafeTypes`'a `"file-replace"` eklendi — `1c5ae7a`'daki `"copy"`
eklemesiyle aynı gerekçe.

**Doğrulama:** `cmake --build --preset dev` → başarılı, 0 hata.

---

## 💡 Öneri

### F-17 — "Tümünü Gör" butonu `[satır 40]`
Yatay kaydırma yerine tüm yamaları tek ekranda görme seçeneği. Katalog zaten
`CatalogProxyModel` üzerinden filtrelenebiliyor; grid görünümü eklemek makul kapsamda.

---

## Altyapı — Bu Oturumda Yapılanlar

Aşağıdakiler olmadan hiçbir düzeltme derlenip doğrulanamıyordu:

| İş | Detay |
|---|---|
| Qt kurtarma | Qt 6.10.1 MinGW (hem `mingw_64` hem `mingw_64_static`) sistemden silinmişti. Qt 6.11.1 MinGW `aqtinstall` ile kuruldu. |
| Preset güncelleme | `mingw-base` → `C:/Qt/6.11.1/mingw_64` |
| ccache | O da silinmişti; build'in asıl çökme sebebiydi (`CreateProcess failed`). winget ile 4.13.6 kuruldu. |
| Doğrulama | Temiz configure + tam build başarılı; `Makine-Launcher.exe` üretiliyor. |

**Not:** `release-static` preset'i hâlâ çalışmıyor — `mingw_64_static` elle derlenen bir
yapı ve Qt installer'ında bulunmuyor. Tek-EXE dağıtım için ayrıca ele alınmalı.

---

## Katalog Denetimi — 237 paket, CDN'den canlı çekildi

`assets/index.json` + her paketin `assets/packages/{id}.json` detayı tarandı.

### Kurulum yöntemi dağılımı

| Tip | Adet | Kod desteği |
|---|---|---|
| `<boş>` | 80 | ✅ overlay |
| `direct` | 63 | ✅ overlay |
| `script` | 47 | ✅ `steps` dizisiyle çalışıyor (47/47 geçerli liste) |
| `overlay` | 13 | ✅ |
| `external` | 12 | ❌ **handler yok** (`steps: []` boş) |
| `copyDir` | 4 | ✅ steps |
| `userPath` | 3 | ✅ `target` alanıyla işleniyor |
| `file-replace` | 3 | ✅ **bu oturumda eklendi** (üçü de açılıp doğrulandı) |
| `copy` | 2 | ✅ (`1c5ae7a` ile eklendi) |
| `forge_inject` | 2 | ❌ ayrı fail-loud yolu |
| `paradox-mod`/`installer`/`modengine`/`workshop` | 4 | ❌ **handler yok** |
| `vpatch`/`copyFile`/`d2r_mod`/`options` | 4 | ✅ steps veya özel işlem |

### Otomatik kurulamayan 21 paket

**`external` (12)** — CoD 4, CoD MW2, CoD MW3, CoD MW Remastered, CoD MW2 Campaign
Remastered, The Outer Worlds, The Medium, HITMAN WoA, High on Life 2, AC Shadows,
Styx: Blades of Greed, Nioh 3
→ Hepsinin `steps` alanı boş dizi. Paket verisi eksik.

**`file-replace` (3)** — DOOM (2016), Dark Souls: Remastered, Darkest Dungeon II
→ ✅ **Çözüldü.** Üç paketin tamamı açılıp incelendi, hepsi oyun köküne köklenmiş yapı
koruyan yük çıktı; `kOverlaySafeTypes`'a eklendi. Otomatik kurulamayan paket sayısı
**21 → 18**'e indi.

**Tekil (4)** — Stellaris (`paradox-mod`), Dwarf Fortress (`installer`),
Dark Souls II (`modengine`), The Last Spell (`workshop`)

**`forge_inject` (2)** — AC Odyssey, AC Valhalla
→ En çok şikayet edilen ikili. `9f1a472` ile mesajdan apexyama yönlendirmesi kaldırılıp
`sourcePath` gösterimi geldi, **yayınlanmamış**.

### Veri bütünlüğü boşlukları

- **24 paketin `dataUrl`'ü yok** → indirilecek dosya bulunmuyor
- **40 pakette `externalUrl`**, 31'inde `apexTier` alanı var

### CDN canlı denetimi (239 paketin tamamına HTTP HEAD)

| Bulgu | Adet | Sonuç |
|---|---|---|
| **0 bayt dosya** | **13** | İndirilir, sonra çözme/kurulum hata verir |
| **HTTP 404** | **1** | Call of Duty: Modern Warfare Remastered — dosya yok |
| Beyan/gerçek boyut uyuşmazlığı | 170 | `expectedSize` disk kontrolünde kullanılıyor |

**0 bayt olanlar** (hepsi `external` tipli): AC Shadows, CoD 4, CoD MW2, CoD MW2 Campaign
Remastered, CoD MW3, Death Stranding 2, HITMAN WoA, High on Life 2, Nioh 3,
Resident Evil Requiem, Styx: Blades of Greed, The Medium, The Outer Worlds.

Bu paketler zaten dürüstlük kapısına takılıyor, ama kullanıcı **önce boş dosyayı
indiriyor**. F-10 (*"indirilmiyor hiçbir şey"*) bu davranışın sonucu olabilir.
→ `external` tipli paketlerde indirme hiç başlatılmamalı, doğrudan yönlendirme
gösterilmeli. **Açık iş.**

### Checksum notu — yanlış alarm düzeltmesi

İlk incelemede "çeviri indirmelerinde checksum doğrulaması yok" olarak işaretlenmişti.
Bu **yanlış**: paketler AES-256-GCM ile şifreli ve `translationdownloader.cpp:396`'daki
yoruma göre **auth tag bütünlük kapısı olarak çalışıyor**. Bozuk veya eksik inen paket
şifre çözmede zaten başarısız olur — SHA-256'dan güçlü bir garanti. Spider-Man 2'nin
"altyazı gelmedi" sorunu indirme bütünlüğünden kaynaklanmıyor.

---

## Sistemik Düzeltmeler — tek tek maddeler yerine akışın tamamı

### 1. Hata görünürlüğü (kör nokta kapatıldı)

**Sorun:** `qCWarning` yalnızca Sentry *breadcrumb*'ı üretiyordu; breadcrumb'lar bir
çökmeye iliştirilir, tek başlarına gönderilmez. Sonuç: her "yama kurulamadı" raporu,
kullanıcının gördüğü mesaj bilinmeden geliyordu. Cuphead/Brothers vakası tam olarak bu
yüzden teşhis edilemedi.

**Çözüm:** `GameService::reportOperationFailure()` — kurulum ve kaldırmanın **her**
başarısızlık yolu artık Sentry'ye `error` seviyesinde event gönderiyor, oyun etiketiyle:

| Bağlanan yol | Yakalanan durum |
|---|---|
| `packageInstallCompleted(false)` | Kurulum başarısız |
| `packageInstallError` | Kurulum hata sinyali |
| `backupRestoreFailed` | Geri yükleme başarısız |
| Yedek yok / restore başlatılamadı | Kaldırma reddedildi |
| `finalizeUninstall` başarısız | `uninstallPackage` false döndü |

### 2. PII sızıntısı önlendi (1. maddenin önkoşulu)

Hata mesajları paket klasör yolunu içeriyor (`"... çıkarıldı: C:\Users\<ad>\..."`).
`beforeSend`'deki temizlik yalnızca **yığın çerçevelerini** geziyor, mesaj gövdesine
dokunmuyordu — yani hata raporlamayı açmak Windows kullanıcı adını sızdıracaktı.

- `captureMessage()` artık gövdeyi `sanitizePath()`'ten geçiriyor
- `sanitizePath()` **tüm** eşleşmeleri değiştiriyor (önceden ilkinde `break` ediyordu;
  birden fazla yol içeren mesaj kısmen temizlenmiş kalırdı)

### 3. Onarım yolu — "kaldırınca kesinlikle düzelmeli"

Yedeğimiz yoksa kaldırmayı reddetmek dürüst, ama kullanıcıyı çözümsüz bırakıyordu.
Mağazanın kendi doğrulaması bu durumda oyunu orijinaline döndürebilen tek mekanizma.

- `GameService::repairGameFiles()` — Steam için `steam://validate/<appid>` doğrudan
  tetikleniyor; tek tık, menü gezme yok
- Epic/GOG'da eşdeğer URL şeması yok → o mağazalar için adım adım yönerge gösteriliyor
- `HeroSection.qml` — herhangi bir kurulum/kaldırma hatasında **"Oyun dosyalarını
  doğrula / onar"** eylemi beliriyor; mevcut hata bildirimi kutusuna dokunulmadan
  kardeş eleman olarak eklendi
- `gameRepairStarted` sinyali sonucu aynı bildirim alanında gösteriyor

---

## Telemetri Kör Noktası

Sentry'de toplam **3 issue** var (hepsi Windows heap bozulması), buna karşılık kullanıcı
şikayetleri çok daha geniş. Sebep: `crashreporter.cpp:44` — Qt uyarı mesajları yalnızca
*breadcrumb* olarak kaydediliyor, Sentry'ye **event olarak gönderilmiyor**. Kurulum
hataları, çeviri başarısızlıkları, tespit sorunları `qCWarning` ile loglandığı için
hiçbir yere ulaşmıyor.

Ayrıca 43 event almış `RtlpHpSegReAlloc` crash'i **"resolved" işaretli** olmasına rağmen
13 Temmuz'a kadar event almaya devam etmiş — kapatılmış ama düzelmemiş.

**Sonuç:** Hata sayısı değil, **görünürlük** sorunu var. Kullanıcı geri bildirimi elle
toplanıyor; bu yüzden "çok hata var" hissi oluşuyor.
