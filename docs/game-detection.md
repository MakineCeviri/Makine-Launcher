# Oyun Tanıma — Zincir, Kanıtlar, Açık Boşluklar

> **Araştırma ve düzeltme tarihi:** 2026-07-22
> **Doğrulama aracı:** `Makine-Launcher.exe --selftest-scan`

---

## 1. Tanıma Zinciri

Tarama iki bağımsız aşamadan oluşur. Karıştırılmamalı: birincisi *oyunu bulur*,
ikincisi *o oyunun yaması var mı* sorusunu cevaplar. Kullanıcının gördüğü
"oyunum kütüphanede yok" şikâyeti çoğunlukla ikinci aşamanın başarısızlığıdır.

### Aşama 1 — Keşif (oyun nerede?)

| Sıra | Kaynak | Nereden okur | Öncelik |
|---|---|---|---|
| 1 | `steam` | `libraryfolders.vdf` → `appmanifest_*.acf` | 0 (en yüksek) |
| 2 | `epic` | `%ProgramData%\Epic\...\Manifests\*.item` | 1 |
| 3 | `gog` | `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games` | 2 |
| 4 | `filesystem` | Bilinen kapsayıcı klasörler (aşağıda) | 4 |
| 5 | `registry` | `Uninstall` anahtarları + yayıncı beyaz listesi | 3 |

Aynı yola işaret eden kayıtlar önceliğe göre teke indirilir; ardından katalog
eşleşmesinden sonra ikinci bir kez `steamAppId` bazında tekilleştirilir.

### Aşama 2 — Eşleştirme (yaması var mı?)

Sırayla denenir, ilk tutan kazanır:

| Katman | Yöntem | Not |
|---|---|---|
| 1 | `steamAppId` doğrudan | Steam için tek adımda biter |
| 2 | `storeIds` ters index | **Şu an ölü** — index bu alanı taşımıyor |
| 3 | Klasör adı → katalog | Sahne etiketi sıyrılır (`…-InsaneRamZes`) |
| 4 | Görünen ad → katalog | |
| 5 | **Parmak izi (exe adı)** | Tüm Steam-dışı kaynaklar *(2026-07-22'de düzeltildi)* |

Parmak izi eşiği 60 puan; exe adı tam eşleşmesi tek başına 60 verir.

### Filesystem kapsayıcı klasörleri

Her bağlı sürücüde aranır:

```
Games · Oyunlar · Oyun · SteamLibrary · XboxGames · GOG Games
GOG Galaxy/Games · Program Files/Epic Games
Program Files[ (x86)]/Rockstar Games · …/EA Games
Program Files (x86)/Origin Games
Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games
```

Artı kullanıcının Ayarlar'dan eklediği yollar. Bir klasörün oyun sayılması için
3 seviye derinliğe kadar en az bir `.exe` içermesi gerekir.

---

## 2. Bu Oturumda Düzeltilenler

### 2.1 Parmak izi eşleştirmesi Epic/GOG'a ulaşmıyordu

**Kök neden.** Parmak izi araması yalnızca `filesystem` ve `registry` kaynakları
için çalışıyordu. Oysa Epic oyunları kendi iç kod adlarıyla kurar — *The Walking
Dead: The Telltale Definitive Series* diske `TWDTTDS` olarak iner. Ne klasör adı
ne görünen ad katalogla eşleşir, ve son çare olan parmak izi hiç denenmezdi.
Sonuç: paketi mevcut olan oyun "desteklenmiyor" görünür.

**Kanıt (red-green).** Klasör adı ve görünen adı katalogda karşılığı olmayan
(`XYZ9TEST`), içinde yalnızca `vampire.exe` bulunan sahte bir Epic girdisiyle:

| Kapsam | Sonuç |
|---|---|
| Eski (`filesystem`/`registry` ile sınırlı) | `matched=3` — eşleşmedi |
| Yeni (tüm Steam-dışı kaynaklar) | `matched=4` — eşleşti |

### 2.2 Epic DLC'leri oyun olarak listeleniyordu

**Kök neden.** Epic; DLC, film müziği ve ek içerik için de manifest yazar.
Bu girdiler filtrelenmediğinde kütüphaneye oyun olmayan satırlar ekleniyordu.
Aynı klasörü paylaşanları yol tekilleştirmesi zaten eliyordu, ama **ayrı klasöre
kurulan** ek içerik doğrudan sızıyordu.

**Kanıt (red-green).** DLC'ye ayrı bir kurulum klasörü verildiğinde:

| Filtre | Sonuç |
|---|---|
| Kapalı | `detected=23` — DLC listeye girdi |
| Açık | `detected=22` — elendi |

Filtre üç alana bakar: `bIsIncludedItem`, `MainGameCatalogItemId`, `bIsExecutable`.

### 2.3 Microsoft Store / Game Pass yanlış klasörü hedefliyordu

Xbox oyunları `XboxGames\<Oyun>\Content\<oyun>.exe` düzenindedir. Kurulum yolu
bir seviye yukarıyı gösterirse yama, oyunun yanına değil sarmalayıcının yanına
kurulur — dosyalar kopyalanır, oyun hiçbir şey görmez.

**Kanıt.** `D:\XboxGames\ZZMakineTest\Content\vampire.exe` düzeneğiyle:

```
Vampire: The Masquerade - Bloodlines [filesystem] D:/XboxGames/ZZMakineTest/Content
```

Yol `Content` alt klasörünü gösteriyor.

### 2.4 Tarama sonucu ölçülemiyordu

Tarama bittiğinde hiçbir yerde sayı kalmıyordu. "Oyunum bulunmuyor" raporu
elimizde hiçbir veri bırakmıyordu.

Eklenen:
- **Tarama özeti** — log'a ve Sentry breadcrumb'ına: `games=N matched=M catalog=K [kaynak dağılımı]`.
  Yalnızca toplu sayı; oyun adı ve yol gönderilmez, `docs/telemetry.md` gizlilik
  taahhüdü korunur.
- **Kesin arıza raporu** — katalog boşsa (indeks eşitlenmemiş) veya hiçbir tarayıcı
  oyun bulamadıysa Sentry'ye `operation:scan` etiketiyle olay gider.
- **`--selftest-scan`** — gerçek `scanAllLibraries()`'i çalıştırır, özeti ve eşleşen
  her oyunun **yolunu** basıp çıkar. Uygulama açıkken de çalışır (tek-örnek kilidi
  atlanır), çünkü teşhis tam da kullanıcı eksik kütüphaneye bakarken gerekir.

```
$ Makine-Launcher.exe --selftest-scan
scan self-test: detected=21 catalog=239 matched=3
  Red Dead Redemption 2 [filesystem] D:/Games/Red Dead Redemption 2
  Death Stranding 2: On the Beach [filesystem] C:/Games/Death.Stranding.2.On.The.Beach-InsaneRamZes
  MiSide [steam] c:/program files (x86)/steam/steamapps/common/MiSide
```

Sayı kadar **yol** da önemli: yanlış klasöre eşleşen bir oyun, yamayı oyunun
içine değil yanına kurar.

---

## 3. Açık Boşluklar

| # | Boşluk | Etki | Tarafı |
|---|---|---|---|
| 1 | **`aliases` katalogda yok (0/239)** | Eşleştirmenin 1.5 ve 1.5b katmanları hiç çalışmıyor. "Yes Your Grace" gibi alternatif adlarla anılan oyunlar yalnızca birebir ad tutarsa bulunur | Veri |
| 2 | **`storeIds` katalogda yok (0/239)** | Epic/GOG mağaza kimliğiyle doğrudan çözümleme ölü; her seferinde parmak izine düşülüyor | Veri |
| 3 | **`fingerprint.keyFiles` kirli** | RDR2 paketinde `keyFiles: ["Türkçe Yama"]` — yamanın klasör adı yazılmış, oyunun dosyası değil. Bu ancak yama kurulduktan *sonra* doğru olur; kurulmadan önce sinyal vermez | Veri |
| 4 | **Core'daki `GameDetector` ölü** | `core/src/game_detector/` içindeki üç tarayıcı her açılışta kaydediliyor ("Registered 3 game scanners") ama `scan()` hiç çağrılmıyor. QML katmanı kendi taramasını yapıyor. İki paralel uygulama — düzeltmenin yanlış tarafa yapılması riski | Kod |
| 5 | **Ubisoft / EA / Battle.net tarayıcısı yok** | Yalnızca varsayılan kurulum klasörü ve `Uninstall` kaydı üzerinden yakalanıyorlar; kütüphane taşınmışsa bulunamaz | Kod |
| 6 | **Xbox oyunları çoğunlukla yazılamaz** | Tespit ediliyor ama Game Pass klasörleri korumalıdır; kurulum izin hatasıyla düşer. Kullanıcıya bunun neden olduğu söylenmiyor | Ürün |

Sıra önerisi: 1 ve 2 veri tarafında düşük maliyetli ve etkisi doğrudan; 3 yanlış
sinyal ürettiği için 1'den önce temizlenmeli.

---

## 4. Regresyon Kontrolü

Tanımaya dokunan her değişiklikten sonra:

```bash
Makine-Launcher.exe --selftest-scan
```

`detected` ve `matched` değerleri düşmemeli. Düşerse hangi kaynağın kaybolduğu
log'daki tarama özetinden görülür.

> **Kural:** "Derleniyor" tanımanın çalıştığını göstermez. Tanıma tamamen
> makineye özgüdür — hangi mağaza, hangi klasör, hangi oyun. Ölçmeden iddia
> edilmez.
