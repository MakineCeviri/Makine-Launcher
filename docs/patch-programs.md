# Yamanın kendi programı — launcher ile sözleşme

> **Tarih:** 2026-10-22, parmak izi ve `${exeDir}` 2026-10-25 · **Launcher:** 0.2.0 (tarif seviyesi 3), 0.2.3 (seviye 4) · **Kod:** `qml/src/services/runstage.h`, `changescan.h`, `gameprint.h`, `localpackagemanager.cpp` (`runPackageProgram`, `executeInstallSteps`, `runUndoPrograms`, `gamePrints`), `gamefolderrules.h` (`missingRequired`), `gameservice.cpp` (`installPackageCommon`, `holdInstallForPrint`)

Bazı yamalar dosya kopyalayarak kurulmaz: çevirmenin aracı oyunun arşivlerini yerinde yeniden yazar
(Heavy Rain'in `HeavyRain_Turkce_Yama.exe`'si, AC Shadows'un `ACS.exe pack`'i). Bu araçlar çevirmenin
bilgisayarında yazılır ve oyunun yerini oradan bilir: `set OYUN=D:\SteamLibrary\steamapps\common\HEAVY RAIN`,
`D:\Games\Assassin's Creed Shadows` varsayılanı. Oyuncunun bilgisayarında o klasör yoktur — ya da vardır ve
başka bir şeydir. Bu belge, böyle bir aracın launcher'la nasıl kusursuz çalıştığını anlatır.

## Tarif

```json
{"type": "script", "steps": [{
  "action": "run",
  "exe": "ACS.exe",
  "args": ["pack", "metinler_Turkish_sahne.tsv", "--game", "${gamePath}"],
  "workDir": "package",
  "uninstall": "ACS.exe",
  "uninstallArgs": ["restore", "${gamePath}"],
  "requires": ["ACShadows.exe", "DataPC_boot_patch_02.forge"]
}]}
```

| Alan | Anlamı |
|---|---|
| `exe` / `args` | Paketteki program ve argümanları. `${gamePath}` (ya da `${gameRoot}`) launcher'ın bulduğu oyunun kök klasörü, `${exeDir}` oyunun exe'sinin klasörü (Elden Ring'de `<kök>\Game`; bilinmiyorsa kök), `${packageDir}` paketin (geçici) kopyası |
| `workDir` | `package`: program paketin kopyasında çalışır (yanındaki dosyaları bulur); `game`: oyun klasöründe |
| `uninstall` / `uninstallArgs` | Kaldırırken çalışacak program; argüman yoksa kurulumunkiler kullanılır |
| `requires` | Oyun klasöründe **programdan önce** bulunması gereken dosyalar (oyun klasörüne göre). Kurulumda yalnız seçilen sürümün/seçeneklerin adımlarındakiler aranır |

`requires` içeren bir `run` adımı seviye 3'tür: panel yayınlarken `install_methods_compat`'a seviye 3 satırı
yazar, 0.1.7 ve öncesi kendi kurmak yerine "launcher'ı güncelle" notunu görür. Argümanlarında `${gameRoot}` ya da
`${exeDir}` olan adım seviye 4'tür (0.2.3): eski launcher programa yer tutucunun kendisini verirdi.

`${exeDir}` şuradan bulunur: paketin parmak izi (aşağıda) klasörü tutuyorsa onun exe'si, yoksa katalogdaki exe
adlarından oyun klasöründe ilk bulunan. Elden Ring'in `ER.exe`'si `Data0.bhd`'nin olduğu `Game\` klasörünü ister:
`["pack", "metinler_Turkish_tam.tsv", "--game", "${exeDir}"]`.

## Oyunun parmak izi (OyunBul)

Çevirmenlerin yamalarla dağıttığı **OyunBul**, yamanın hazırlandığı oyun kopyasının parmak izini `game.bin`'e
yazar: ana exe, yanındaki öteki exe'ler (başka mağazanın sürümü exe'sine başka ad verir) ve oyunun ağacına yayılmış
~10 örnek dosya; her biri göreli yol + boyut + baştaki ve sondaki 64 KB'ın özeti. Launcher aynı kurallarla
(`gameprint.h`, OyunBul'un yazdığı bir `game.bin` ile birebir sınanır: `tests/ui/test_gameprint.cpp`) bir klasöre
bakar:

| Sonuç | Koşul |
|---|---|
| TAM | exe (ya da yedek exe) ve örneklerin en az %80'i birebir |
| sürüm farklı | exe yerinde, örneklerin en az %60'ı aynı göreli yolda |
| exe tahmini | bilinen exe'lerin hiçbiri yok ama örneklerin en az %80'i yerinde (sürüm farklı sayılır) |
| eşleşmedi | geri kalanı |

**Nereden gelir** (`LocalPackageManager::gamePrints`, ikisi birden olabilir):
- katalogdaki `fingerprint`: `exe` + `altExes` + nesne olarak `keyFiles` (aşağıda). Ad olarak `keyFiles`
  (`"Game"`) eski anlamını korur; 0.2.2 ve öncesi nesneleri atlar.
- paketin içindeki `game.bin` (kökte ya da en çok 3 klasör aşağıda; sürüm klasörü başına bir tane olabilir,
  en iyi tutan sayılır). D1'e hiçbir şey eklemeden çalışır: çevirmen `game.bin`'i paketin içine koyar.

```json
"fingerprint": {
  "exeNames": ["heavyrain.exe"],
  "rootName": "HEAVY RAIN",
  "exe":      {"rel": "HeavyRain.exe", "size": 26214400, "hash": "<32 hex>", "version": "1.0.0.0"},
  "altExes":  [{"rel": "...", "size": 0, "hash": "<32 hex>"}],
  "keyFiles": ["Resources", {"rel": "Resources/BigFile_WIN.d01", "size": 1058013184, "hash": "<32 hex>"}],
  "match": 0.6, "exact": 0.8
}
```

`hash`: SHA-256(uzunluk 8 bayt LE + ilk `min(64 KB, uzunluk)` bayt + uzunluk 64 KB'tan büyükse
`max(64 KB, uzunluk − 64 KB)` ofsetinden sona kadar), ilk 16 bayt. `game.bin` düzeni `gameprint.h`'nin başında.

**Ne zaman bakılır:**
- **Kurulum ön kontrolünde** (`requires`'tan sonra, yazma izninden önce). Yama o klasörde zaten kuruluysa
  bakılmaz: yama örneklenen dosyaların bir kısmını değiştirmiştir.
  - TAM → kurulum sürer.
  - eşleşmedi ama bir üst/alt klasör tutuyor (repack'in `<Oyun>\<Oyun>`'u, oyunun içinden bir klasör) → oyun
    oraya bağlanır, kurulum yeniden başlar.
  - sürüm farklı / eşleşmedi → **oyuna dokunulmadan durur**, sayfada bir soru açılır: "Oyunun sürümü yamanınkinden
    farklı" ya da "Bu klasör yamanın oyununa benzemiyor" (+ sayfadaki "Klasörü değiştir"). "Yine de kur" aynı
    sürüm/seçeneklerle devam eder (`GameService::installAnyway`). Sayım kanalına `game_version_differs` /
    `fingerprint_mismatch` (taraf `user`) gider, Sentry'ye gitmez.
- **Klasör seçilince / sayfa açılınca** (`inspectGameFolder`): parmak izinin tuttuğu klasör, düzen kurallarının
  bulduğundan önce gelir; parmak izi varken hiçbir klasör tutmuyorsa karar "emin değil"dir.
- **Program çalışırken**: `${exeDir}`, `MAKINE_EXE_KLASORU` ve kopyadaki `game.path` (aşağıda).

## Kurulum sırası

1. **Klasör doğrulanır** (`GameService::installPackageCommon` ön kontrolü; `LocalPackageManager::requiredFilesFor`
   seçilen sürümün ve seçeneklerin adımlarından). `requires` dosyalarından biri yoksa hiçbir şey çalışmaz.
   Klasörün bir üstü/altı (repack'in `<Oyun>/<Oyun>`'u, Game Pass'in `Content`'i) hepsini içeriyorsa oyun oraya
   bağlanır ve kurulum yeniden başlar; yoksa kullanıcıya eksik dosyalar ve "Klasörü değiştir" gösterilir. Yama
   o klasörde zaten kuruluysa (güncelleme) klasör değiştirilemez: mesaj oyunu güncelleyip dosyalarını
   doğrulamayı söyler. Yazma izni (UAC) bu kontrolden sonra istenir.
2. **`<oyun>\.makine` yazılır**: launcher'ın klasör notu; `.bat`'ın okuyabileceği biçimde
   (`set "OYUN=…"`, `set "MAKINE_OYUN=…"`, `set "MAKINE_OYUN_ID=…"`, `set "MAKINE_EXE_KLASORU=…"`). Kaldırınca silinir.
3. **Oyun klasörünün dökümü alınır** (dosya başına boyut + değişme zamanı, `changescan::take`).
4. **Paket geçici klasöre kopyalanır** (`%TEMP%\MakineLauncher\run\<appId>-<zaman>`; 4 MB üstü dosyalar hard
   link). Kopyada:
   - her `.bat`/`.cmd`'de oyun klasörünü ayarlayan satır — `set OYUN=D:\…`, `set "GAME_DIR=…"`,
     `set OYUN=game.path` gibi yer tutucular — `set "OYUN=%MAKINE_OYUN%"` olur; değer tırnaklıysa
     (`set OYUN="D:\…"`, `set OYUN= "…"`) tırnak kalır: `set OYUN="%MAKINE_OYUN%"` (betik `%OYUN%`'u tırnaksız
     kullanır, oyuncunun klasöründe boşluk olabilir). Aynı klasörün betikte yazıldığı başka yerler
     (`cd /d "D:\Games\…"`) de `%MAKINE_OYUN%` olur. Hangi satır: değişken adı oyun klasörü diyorsa (OYUN,
     GAME_DIR, OYUN_KLASORU …) — o zaman mağaza kütüphanesindeki herhangi bir klasör (`steamapps\common\<x>`,
     `Epic Games\<x>`, `XboxGames\<x>` …) yeter — ya da klasörün adı oyunun adıysa. Mağazanın başka klasörleri
     (`steamapps\common\Steamworks Shared`, `Epic Games\Launcher`) olduğu gibi kalır. Doğru klasörü zaten
     yazan satıra dokunulmaz.
   - yalnız bir oyun klasörü tutan küçük dosya (`game.path`) launcher'ın klasörünü tutar.
   - her `game.bin`'in (OyunBul) yanına OyunBul'un üç satırlık `game.path`'i yazılır: exe'nin klasörü, oyunun
     kökü, exe'nin tam yolu. Yamanın `.bat`'ı `OyunBul.exe bul` derse kayıtlı yolu geçerli bulup hemen biter;
     bütün diskleri taramaz, oyunun başka bir kopyasını da seçemez. `set /p` ile okuyan betik de doğru yolu alır.

   Paketin kendisi değişmez; bir sonraki kurulum yine çevirmenin dosyalarından başlar.
5. **Program çalışır**: argümanlarda `${gamePath}` / `${gameRoot}` / `${exeDir}`, ortamda `MAKINE_OYUN` /
   `MAKINE_PAKET` / `MAKINE_OYUN_ID` / `MAKINE_EXE_KLASORU`
   (ortam değeri UTF-16'dır, Türkçe harfli klasörler `.bat`'ın kod sayfasından etkilenmez). Çıktısı oyunun
   sayfasında görünür, sorduğu soruya oradan yanıt verilir. Kendi penceresi olan program (PE alt sistemi GUI)
   için sayfa "işlemi o pencerede yapın, bitince kapatın" der; launcher pencere kapanana kadar bekler (en fazla 2 saat).
   Program bir iş nesnesinde (job object) çalışır: başlattığı programlar da — `start arac.exe` diyen bir betik
   hemen biter — paketin kopyasından çalışıyorsa ya da betik/yorumlayıcıysa (`cmd`, `powershell`, `python` …)
   bitene kadar beklenir; açtığı tarayıcı ya da not defteri beklenmez. "Durdur" bütün ağacı kapatır.
6. **Döküm yeniden alınır, fark bulunur.** Program 0 ile çıktı ama oyunda hiçbir dosya değişmediyse kurulum
   **olmamış** sayılır (pencere "Kur"a basılmadan kapandı; araç oyunun arşivlerini bulamadı). Değişen ve eklenen
   dosyalar kayda `_program:<yol>` olarak yazılır; uninstall onları silmez (bir kısmı aracın kendi yedeği),
   geri almayı araca bırakır.
7. **Kaldırma programı saklanır**: paketin kopyası `<AppData>\programs\<appId>`'ye; katalog yeni bir paket
   yayımlayıp eski açılmış paket silinse de kaldırma yapılabilir.
8. **Programın yanına yazdıkları saklanır**: geçici kopya program bitince silinir; programın kendi yanına yazdığı
   metin dosyaları (Heavy Rain'in `HR_yama_gunluk.txt`'si gibi günlükler) önce
   `<AppData>\logs\programs\<appId>\<zaman>\`'a kopyalanır ve yolu sayfada yazılır (oyunun son on çalışması).

**Yedek yok, ve gerekmez.** Yalnız kendi programıyla kurulan yamada (bütün adımlar `run`) launcher kendi
yedeğini almaz: programın oyunda ne yazacağını önceden hiçbir liste bilemez, yedeği ve geri almayı program
kendisi yapar (Heavy Rain'in aracı orijinalleri arşivlerin yanına `.hryedek` olarak alır, "Yamayı Kaldır"
ile geri koyar; ACS.exe `restore`). Launcher'ın istediği tek şey programın **nasıl geri alınacağı**dır
(`uninstall`): o yoksa yama hiç kurulmaz ("katalog hatası", Sentry `run_no_undo`), çünkü kurulsa launcher'dan
kaldırılamazdı. Panel bu tarifi yayınlatmaz; programın kendi "Kaldır"ı varsa kaldırma = aynı program.
Dosya kopyalayan adımlar da olan karışık tarifte launcher kopyaladıklarını yedekler, programınkileri yine
programa bırakır.

## Kaldırma sırası

`GameService::runUndoPrograms` → `LocalPackageManager::runUndoPrograms`: kayıtta `_program:` varsa (ya da kayıt
bu izlemeden önce yazılmışsa) `uninstall` programı saklanan kopyadan, aynı geçici kopya ve klasör düzeltmesiyle
çalıştırılır ve **beklenir**; çıktısı sayfada, sorduğu soruya oradan yanıt verilir, "Durdur" onu durdurur (yama
kurulu kalır). Aynı anda tek kaldırma programı çalışır.

- **Hata** (bulunamadı, başlamadı, 0 dışı kod, 2 saat): kayıt kalır, mesaj gösterilir. İkinci hatadan sonraki
  "Kaldır" yamayı programsız kaldırır: launcher'ın kopyaladıkları silinir, programın değiştirdikleri için
  mağazanın dosya onarımı başlar (bulunamayan ya da yeni tarifte adı değişen program kaydı sonsuza dek kilitlemesin
  diye). Kullanıcının durdurması ya da UAC'ye "Hayır" demesi sayılmaz.
- **0 ile çıkıp oyunda hiçbir şey değişmediyse** ilk seferde uyarılır (kaldırma penceresi basılmadan kapandı),
  ikinci "Kaldır"da kayıt silinir (yama zaten gitmiş olabilir).
- Sonra launcher'ın kendi kopyaladıkları ve `.makine` silinir. Bir dosya silinemezse kayıtta kalan `.makine`
  yeniden denemede programın ikinci kez çalışmasını önler.

Tarifte `uninstall` yoksa "Kaldır" programın değiştirdiklerini mağazanın dosya onarımına bırakır (sayfa ve
onay penceresi bunu söyler).

**Güncelleme** eski sürümü önce kendi programıyla kaldırır, sonra yenisini kurar (`reinstallForUpdate`): araç
yamalı dosyaları "orijinal" diye yedeklemesin.

**Başarısız bir kurulumda** program oyunda bir şey değiştirdiyse kayıt kalır (yedek olsa da) ve sayfa
"Kaldır"ı gösterir: geri almayı yine programın kendisi yapar.

## Sentry'de nasıl görünür

Her sonuç kendi `fail.reason` koduyla gider (`failurereasons.h`; aynı oyun + neden günde bir kez), sayım
kanalında hepsi sayılır:

| Kod | Ne oldu |
|---|---|
| `folder_mismatch` | Klasörde `requires` dosyaları yok, program çalıştırılmadı |
| `requires_missing_installed` | Aynısı, ama yama o klasörde kurulu (oyun güncellenmiş/dosyaları eksik) |
| `tool_missing` · `tool_stage_failed` | Program pakette yok · geçici kopya hazırlanamadı |
| `tool_start_failed` · `tool_exit_code` · `tool_timeout` · `elevation_declined` | Başlamadı · 0 dışı kod · 2 saat · UAC'ye "Hayır" |
| `tool_version_mismatch` · `tool_blocked_by_policy` | 0 dışı kod, çıktısı nedenini söylüyor (`steprules::toolOutputCause`): oyunun bu sürümünü tanımadı (`FARKLI SURUM`) · Windows'un uygulama denetimi (Device Guard / App Control) programı engelledi (taraf `user`). Oyuncu da nedeni adımın ayrıntısında görür |
| `program_no_change` | Program 0 ile çıktı ama oyunda hiçbir şey değişmedi (kurulum olmamış sayıldı) |
| `game_version_differs` · `fingerprint_mismatch` | Parmak izi: oyun başka sürüm · klasör yamanın oyununa benzemiyor; kurulum oyuncuya soruldu (yalnız sayım, Sentry'ye gitmez) |
| `uninstall_tool` | Kaldırma programı başarısız (mesajda neden: bulunamadı, kod …) |
| `uninstall_tool_no_change` | Kaldırma programı oyunda hiçbir şey değiştirmedi |
| `uninstall_tool_skipped` | İki hatadan sonra programsız kaldırıldı, mağaza onarımı başladı |

`just telemetry-check` sıralı listeyi verir; geliştirme sürümlerinin olayları `development` ortamındadır.

## Çevirmen için kısa kural

- Paketin içine **`game.bin`** koy (OyunBul: `OyunBul.exe ornekle <oyun_klasörü>`): **temiz** kurulumdan al,
  yamanın değiştireceği dosyaları `--haric` ile dışarıda bırak (`--haric "*.forge;update_4.rpf"`). Launcher
  kurmadan önce oyuncunun oyununu onunla karşılaştırır, başka sürümse sorar, `${exeDir}`'i ondan bulur.
- Oyun klasörünü **argümanla** al (`--oyun <klasör>`, `--game <klasör>`, `%1`) ya da `MAKINE_OYUN` ortam
  değişkeninden / oyun klasöründeki `.makine`'den oku. Betikte sabit yazdıysan launcher düzeltir.
- Kaldırma komutu olsun (`restore <klasör>`, `--kaldir`); yoksa "Kaldır" yamayı geri alamaz.
- Başarısızlıkta 0 dışı kodla çık. Python/Node gerektiren betikler oyuncuda çalışmaz: oyuncuya giden şey
  tek başına çalışan program olmalı (HeavyRainTool'da `HR_yama\HeavyRain_Turkce_Yama.exe`).
- Yedeği **oyun klasörüne** yaz (ACS'nin `acs_text_yedek`'i gibi), programın yanına değil: program her seferinde
  yeni bir geçici kopyadan çalışır, yanına yazdığı kopyayla birlikte silinir.
- Betikte `%OYUN%`'u tırnakla kullan (`"%OYUN%\Resources"`): oyuncunun klasörü `C:\Program Files (x86)\…`
  olabilir; tırnaksız `)` bir `if (…)` bloğunu erken kapatır.
- Yönetici izni isteyen (requireAdministrator) program ortam değişkenlerini almaz (Windows UAC ile açılan
  programa launcher'ın ortamını geçirmez): klasörü argümandan ya da `.makine`'den oku.
- `.makine` ve düzeltilen `game.path` UTF-8'dir. Bir `.bat` bunları okuyacaksa önce `chcp 65001` desin.
- Panel (`/admin/paketler`) programın kendi kullanım metninden (`ACS.exe pack … [--game klasor]`,
  `restore [oyun_klasoru]`) argümanları kendisi çıkarır; çevirmenin betikleri `--oyun "%OYUN%"` diyorsa onu kullanır.

## Denendi (2026-10-22, dev build)

| Durum | Sonuç |
|---|---|
| Heavy Rain paketi (CDN, `set OYUN=game.path`) sahte klasöre | Betik `--oyun "C:\Users\…\Desktop\heavy rain"` aldı; araç arşivi bulamadı, 0 ile çıktı → "hiçbir dosyayı değiştirmedi", kayıt yok, `.makine` silindi |
| Aynı, `requires: HeavyRain.exe, Resources/BigFile_WIN.idx, …` | Program çalışmadan durdu: eksik dosyalar + "Klasörü değiştir" |
| Deneme betiği (sabit `D:\SteamLibrary\…\HEAVY RAIN`) dosya yazıp siler | Kurulum: 2 dosya eklendi, kayıtta `_program:`; Kaldır: betik saklanan kopyadan çalıştı, 2 dosya silindi, kayıt ve kopya gitti |
| `ACS.exe pack … --game <sahte klasör>` | Gerçek klasör argümanla gitti; sahte forge atlandı, 0 değişiklik → kurulum olmamış sayıldı (`Console.ReadKey` yönlendirilmiş stdin'de takılmıyor) |
