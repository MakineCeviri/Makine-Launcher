# Yamanın kendi programı — launcher ile sözleşme

> **Tarih:** 2026-10-22 · **Launcher:** 0.2.0 (tarif seviyesi 3) · **Kod:** `qml/src/services/runstage.h`, `changescan.h`, `localpackagemanager.cpp` (`runPackageProgram`, `executeInstallSteps`, `runUndoPrograms`), `gamefolderrules.h` (`missingRequired`)

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
| `exe` / `args` | Paketteki program ve argümanları. `${gamePath}` launcher'ın bulduğu oyun klasörü, `${packageDir}` paketin (geçici) kopyası |
| `workDir` | `package`: program paketin kopyasında çalışır (yanındaki dosyaları bulur); `game`: oyun klasöründe |
| `uninstall` / `uninstallArgs` | Kaldırırken çalışacak program; argüman yoksa kurulumunkiler kullanılır |
| `requires` | Oyun klasöründe **programdan önce** bulunması gereken dosyalar (oyun klasörüne göre). Kurulumda yalnız seçilen sürümün/seçeneklerin adımlarındakiler aranır |

`requires` içeren bir `run` adımı seviye 3'tür: panel yayınlarken `install_methods_compat`'a seviye 3 satırı
yazar, 0.1.7 ve öncesi kendi kurmak yerine "launcher'ı güncelle" notunu görür.

## Kurulum sırası

1. **Klasör doğrulanır** (`GameService::installPackageCommon` ön kontrolü; `LocalPackageManager::requiredFilesFor`
   seçilen sürümün ve seçeneklerin adımlarından). `requires` dosyalarından biri yoksa hiçbir şey çalışmaz.
   Klasörün bir üstü/altı (repack'in `<Oyun>/<Oyun>`'u, Game Pass'in `Content`'i) hepsini içeriyorsa oyun oraya
   bağlanır ve kurulum yeniden başlar; yoksa kullanıcıya eksik dosyalar ve "Klasörü değiştir" gösterilir. Yama
   o klasörde zaten kuruluysa (güncelleme) klasör değiştirilemez: mesaj oyunu güncelleyip dosyalarını
   doğrulamayı söyler. Yazma izni (UAC) bu kontrolden sonra istenir.
2. **`<oyun>\.makine` yazılır**: launcher'ın klasör notu; `.bat`'ın okuyabileceği biçimde
   (`set "OYUN=…"`, `set "MAKINE_OYUN=…"`, `set "MAKINE_OYUN_ID=…"`). Kaldırınca silinir.
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

   Paketin kendisi değişmez; bir sonraki kurulum yine çevirmenin dosyalarından başlar.
5. **Program çalışır**: argümanlarda `${gamePath}`, ortamda `MAKINE_OYUN` / `MAKINE_PAKET` / `MAKINE_OYUN_ID`
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
| `program_no_change` | Program 0 ile çıktı ama oyunda hiçbir şey değişmedi (kurulum olmamış sayıldı) |
| `uninstall_tool` | Kaldırma programı başarısız (mesajda neden: bulunamadı, kod …) |
| `uninstall_tool_no_change` | Kaldırma programı oyunda hiçbir şey değiştirmedi |
| `uninstall_tool_skipped` | İki hatadan sonra programsız kaldırıldı, mağaza onarımı başladı |

`just telemetry-check` sıralı listeyi verir; geliştirme sürümlerinin olayları `development` ortamındadır.

## Çevirmen için kısa kural

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
