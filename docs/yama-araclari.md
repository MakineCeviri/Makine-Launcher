# Yama araçlarımız ve launcher — durum ve tasarım

> **Tarih:** 2026-10-25 · **Yanıtladığı not:** `Makine_Launcher_Yama_Gelistirme.md` (25 Ekim 2026; ACS, ACBF, IFR, ER,
> RDR2Loc, OyunBul) · **Sözleşmenin kendisi:** [`patch-programs.md`](patch-programs.md)

Not, launcher'ın kaynağına bakmadan davranıştan yazılmıştı. Bu belge her maddenin launcher'daki karşılığını,
bu değişiklikle eklenenleri ve sıradaki işlerin tasarımını toplar.

## Durum

| Konu | Durum | Nerede |
|---|---|---|
| Oyunu parmak iziyle tanımak | **Eklendi** — OyunBul'un kuralları birebir (`game.bin` paketin içinde ya da katalogda) | `gameprint.h`, `LocalPackageManager::gamePrints` |
| Kök klasör / exe klasörü ayrımı | **Eklendi** — `${gameRoot}`, `${exeDir}`, `MAKINE_EXE_KLASORU`, OyunBul'un 3 satırlık `game.path`'i | `installsteprules.h`, `runstage.h`, `runPackageProgram` |
| Araçları doğru çalıştırmak (argüman, çıkış kodu, stdin, çıktı, süre, oyun açık, yetki) | Vardı (0.2.0) | `patch-programs.md` |
| Kurmadan önce sürüm kontrolü | **Eklendi** — parmak izi TAM değilse oyuncuya sorulur. `--dry-run` ve Steam `buildid` sırada | `GameService::installPackageCommon` |
| Oyun güncellemesinden sonra yeniden kurmak | Kopyalanan dosyalar için vardı (`checkPatchIntegrity` → "Onar"); program yamaları için sırada (B) | `gameservice.cpp` |
| Kaldırmayı aracın `restore`'una bırakmak | Vardı: `_program:` dosyalarına dokunulmaz, `uninstall` programı beklenir | `patch-programs.md` |
| Daha çok kaynak, tam tarama, elle seçim | Steam, Epic, GOG, Ubisoft (kayıt defteri), ortak klasörler, `Uninstall` ve elle seçim vardı; tam disk taraması ve EA sırada (G) | `corebridge.cpp` |
| Mod Engine, birbirini dışlayan seçenekler, disk alanı | Sırada (D, E, F); yetki (UAC) vardı | — |

## Açık soruların yanıtları (notun §6'sı)

1. **`gamePath` her zaman kök mü?** Evet: tarama ve elle seçim kökü bulur (`resolveGameRoot`: `ELDEN RING\Game`
   → `ELDEN RING`). `Game\` için artık `${exeDir}` var; ER: `["pack", "…_tam.tsv", "--game", "${exeDir}"]`
   (tarif seviyesi 4).
2. **Kaldırırken `_program:` dosyaları:** silinmez, yedekten de dönmez. Tarifteki `uninstall` programı (ACS'de
   `restore`) saklanan kopyadan çalıştırılır ve beklenir. İki kez başarısız olursa yama programsız kaldırılır ve
   değişen dosyalar için mağazanın dosya onarımı başlatılır. Heavy Rain'in 25 veri dosyası bu yüzden kayıttadır:
   launcher onları silmez, Heavy Rain'in programı geri koyar.
3. **`filesystem` / `registry` kuralı:** `filesystem` her sürücüde bilinen klasörlere (`Games`, `Oyunlar`,
   `SteamLibrary`, `XboxGames`, …) bakar, 3 seviye içinde `.exe` olan klasörü aday sayar; `registry` `Uninstall`
   kayıtlarından yayıncı listesiyle süzer. Katalogla eşleşmeyenler `games_cache.json`'da kalır ama kütüphanede
   görünmez (kütüphane yalnız yaması olan oyunları listeler). Genel exe adları (`launcher.exe`, `game.exe`,
   `start_protected_game.exe`) eşleştirmede sayılmaz, Rockstar'ın `Launcher.exe`'si HITMAN sanılmaz.
4. **Ubisoft / GOG / EA:** Ubisoft (`…\Ubisoft\Launcher\Installs`) ve GOG kayıt defterinden okunuyor. EA'nın kendi
   tarayıcısı yok; ortak klasörler ve `Uninstall` kaydıyla bulunur.
5. **`run` adımı nasıl başlıyor:** konsol penceresi açılmaz, stdin bir boru (sayfadan yanıt yazılabilir; `Console.ReadKey`
   takılmaz), çıktı birleşik ve canlı sayfada, iş nesnesi içinde, en çok 2 saat. Çıkış kodu 0 dışı → hata;
   0 ama oyunda değişiklik yok → kurulum olmamış.
6. **`requires` dosyası yoksa:** hiçbir şey çalışmaz. Bir üst/alt klasörde varsa oyun oraya bağlanıp kurulum
   yeniden başlar; yoksa eksik dosyalar ve "Klasörü değiştir" gösterilir (klasör seçme akışı:
   `GameFolderBar` → `inspectGameFolder`).

## Bu değişiklikte

**Parmak izi** (`gameprint.h`): OyunBul.cs'nin `Hasher.Partial`, `Fingerprint.Load` ve `Verify` kuralları; OyunBul.exe'nin
yazdığı bir `game.bin` ile sınanır (`test_gameprint`). Kaynaklar: paketteki `game.bin` (D1'e dokunmadan, bugün
çalışır) ve katalogdaki `fingerprint` (`exe`, `altExes`, nesne `keyFiles`; çekirdek ayrıştırıyor, sunucu henüz
vermiyor — A).

**Kurulum ön kontrolü:** `requires`'tan sonra, yazma izninden önce. TAM → devam; bir üst/alt klasör tutuyorsa
oraya bağlanır; sürüm farklı / eşleşmedi → oyuna dokunmadan durur ve sayfada sorar ("Yine de kur"). Yama zaten
kuruluysa bakılmaz (yama örneklenen dosyaları değiştirmiştir: bu PC'deki RDR2 yamalıyken `update_4.rpf` ve exe
farklı çıkar). Sayım: `game_version_differs`, `fingerprint_mismatch` (taraf `user`, Sentry'ye gitmez).

**Klasör seçimi:** parmak izinin tuttuğu klasör düzen kurallarından önce gelir; parmak izi varken hiçbir klasör
tutmuyorsa karar "emin değil".

**Program çalışırken:** `${exeDir}` / `${gameRoot}`, `MAKINE_EXE_KLASORU` (ortam ve `.makine`), paketteki her
`game.bin`'in yanına OyunBul'un `game.path`'i — `OyunBul.exe bul` diyen `.bat` diskleri taramadan hemen biter.

**Makine-WEB:** `recipeLevel()` `${gameRoot}`/`${exeDir}` gördüğünde 4 döner, panel `install_methods_compat`
satırını o seviyeyle yazar (0.2.2 ve öncesine "launcher'ı güncelle"); panelin hata listesi iki yeni kodu açıklar.

## Sırada — tasarım

**A. Katalogda parmak izi.** D1 `fingerprints`'e `print` (JSON) sütunu; `/api/v2/games/{id}` bunu `fingerprint`'in
`rootName`/`exe`/`altExes`/`keyFiles`(nesne)/`match`/`exact` alanlarına açar; panel `game.bin` yüklemesini
tarayıcıda çözer (düzen `gameprint.h`'nin başında) ve gösterir (`OyunBul.exe bilgi` gibi). Launcher tarafı hazır.
Kazanç: paket indirilmeden, tarama sırasında da oyun doğrulanır.

**B. Program yamalarında oyun güncellemesi.** Kurulum sonunda `changescan`'in zaten aldığı döküm, değişen
dosyalar için (boyut + zaman) kayda yazılır. `checkPatchIntegrity` onları karşılaştırır; değişmişse sayfa
"Oyun güncellendi, yama gitmiş olabilir" der. "Yeniden kur": yama gitmişse kaldırma programı çalıştırılmaz
(eski yedek yeni sürümün üstüne yazılırdı); programın eklediği dosyalar (`acs_text_yedek\` gibi eski yedek)
silinir, kayıt düşer, kurulum baştan yapılır. Değişen dosyaların yalnız bir kısmı yenilendiyse önce mağazanın
dosya onarımı (eski yamalı forge'u da orijinale döndürür), sonra kurulum. Araçlara `--yedek-sifirla` gelirse
tarif onu kullanır.

**C. Deneme çalıştırması.** `run` adımına `checkArgs` (ör. `["pack", "x.tsv", "--game", "${gameRoot}", "--dry-run"]`):
asıl adımdan önce çalışır; 0 dışı → kurulum oyuna dokunmadan durur, çıktısı sayfada. Yeni alan → tarif seviyesi 5.

**D. Mod Engine (Elden Ring).** Tarife `launch` (program + argümanlar, ör. me3 `launch -p eldenring-tr.me3 -g eldenring`).
Sayfadaki "Oyunu başlat" onu çalıştırır; program yoksa kurulum yönlendirmesi; anti-hile uyarı penceresi
(var olan `AntiCheatWarningDialog`) "çevrimiçi oynamayın" der.

**E. Birbirini dışlayan seçenekler.** Seçeneğe `excludes: ["<seçenek id>"]`; `InstallOptionsDialog` birini seçince
ötekini kapatır. Aynı oyuna iki ayrı paket olamaz (kayıt oyun başına bir), bu yüzden RDR2'nin iki yöntemi aynı
paketin iki seçeneği olmalı.

**F. Disk alanı.** Tarifte `requiredFreeSpace` (bayt); ön kontrolde oyunun sürücüsüne bakılır, yetmezse durur.

**G. "Bilgisayarımda ara".** Parmak izi olan ama bulunamayan oyunun sayfasında: OyunBul'un sırası (bilinen yerler
→ bütün sürücüler, sürücü başına bir iş parçacığı, junction'lar atlanır), örnek dosya adlarından aday kök,
`gameprint::verify`. Bu PC'de C: + D: (~1 TB) 31 sn.

## Çevirmen tarafı

- `game.bin`'i **temiz** kurulumdan al, yamanın değiştireceği dosyaları `--haric` ile dışarıda bırak, paketin
  içine koy. `OyunBul.exe` + `BUL.bat` launcher için gerekmez ama bırakılabilir: launcher `game.path`'i yazar.
- `Game\` isteyen araç (ER) için tarifte `${exeDir}`; ötekiler `${gamePath}`.

## Denendi (2026-10-25, dev build)

| Durum | Sonuç |
|---|---|
| OyunBul.exe'nin yazdığı `game.bin` + aynı ağaç (68–293 KB dosyalar, Türkçe adlı) | TAM; 1/5 örnek değişince TAM, 2/5 → sürüm farklı; exe'nin ortası değişince TAM (yalnız baş/son okunur), sonu değişince sürüm farklı; ana exe silinince yedek exe ile TAM; bilinen exe'ler yokken tahmin; yalnız `CrashReporter.exe` → eşleşmedi |
| Gerçek RDR2 `game.bin` (Yeni klasör) + `D:\Games\Red Dead Redemption 2\Red Dead Redemption 2` | `SURUM FARKLI (10/10 yerinde, 9/10 birebir, exe farkli)` — OyunBul `dogrula` ile aynı; 120 GB'lık oyunda 8 ms. Dış klasörden (`…\Red Dead Redemption 2`) kök bulundu |
| Tam test takımı (`ctest`, 33 takım) + Makine-WEB `npm test` | geçti |
| Launcher açılışı | QML hatasız yüklendi |

Denenmedi: gerçek bir pakette "Yine de kur" penceresi (katalogda henüz `game.bin`'li paket yok).
