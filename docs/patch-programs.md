# Yamanın kendi programı — launcher ile sözleşme

> **Tarih:** 2026-10-22 · **Launcher:** 0.1.8 (tarif seviyesi 3) · **Kod:** `qml/src/services/runstage.h`, `changescan.h`, `localpackagemanager.cpp` (`runPackageProgram`, `executeInstallSteps`, `runUndoPrograms`), `gamefolderrules.h` (`missingRequired`)

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
| `requires` | Oyun klasöründe **programdan önce** bulunması gereken dosyalar (oyun klasörüne göre) |

`requires` içeren bir `run` adımı seviye 3'tür: panel yayınlarken `install_methods_compat`'a seviye 3 satırı
yazar, 0.1.7 ve öncesi kendi kurmak yerine "launcher'ı güncelle" notunu görür.

## Kurulum sırası

1. **Klasör doğrulanır** (`GameService::installPackageCommon` ön kontrolü). `requires` dosyalarından biri yoksa
   hiçbir şey çalışmaz. Klasörün bir üstü/altı (repack'in `<Oyun>/<Oyun>`'u, Game Pass'in `Content`'i) hepsini
   içeriyorsa oyun oraya bağlanır ve kurulum yeniden başlar; yoksa kullanıcıya eksik dosyalar ve
   "Klasörü değiştir" gösterilir. Yazma izni (UAC) bu kontrolden sonra istenir.
2. **`<oyun>\.makine` yazılır**: launcher'ın klasör notu; `.bat`'ın okuyabileceği biçimde
   (`set "OYUN=…"`, `set "MAKINE_OYUN=…"`, `set "MAKINE_OYUN_ID=…"`). Kaldırınca silinir.
3. **Oyun klasörünün dökümü alınır** (dosya başına boyut + değişme zamanı, `changescan::take`).
4. **Paket geçici klasöre kopyalanır** (`%TEMP%\MakineLauncher\run\<appId>-<zaman>`; 4 MB üstü dosyalar hard
   link). Kopyada:
   - her `.bat`/`.cmd`'de oyun klasörünü ayarlayan satır — `set OYUN=D:\…`, `set "GAME_DIR=…"`,
     `set OYUN= "…"`, `set OYUN=game.path` gibi yer tutucular — `set "OYUN=%MAKINE_OYUN%"` olur; aynı klasörün
     betikte yazıldığı başka yerler (`cd /d "D:\Games\…"`) de `%MAKINE_OYUN%` olur. Hangi satır: değişken adı
     oyun klasörü diyorsa (OYUN, GAME_DIR, OYUN_KLASORU …) ya da değer bir mağaza kütüphanesindeki oyun
     klasörüyse (`steamapps\common\<x>`, `Epic Games\<x>`, `XboxGames\<x>` …) ya da klasörün adı oyunun adıysa.
     Doğru klasörü zaten yazan satıra dokunulmaz.
   - yalnız bir oyun klasörü tutan küçük dosya (`game.path`) launcher'ın klasörünü tutar.

   Paketin kendisi değişmez; bir sonraki kurulum yine çevirmenin dosyalarından başlar.
5. **Program çalışır**: argümanlarda `${gamePath}`, ortamda `MAKINE_OYUN` / `MAKINE_PAKET` / `MAKINE_OYUN_ID`
   (ortam değeri UTF-16'dır, Türkçe harfli klasörler `.bat`'ın kod sayfasından etkilenmez). Çıktısı oyunun
   sayfasında görünür, sorduğu soruya oradan yanıt verilir. Kendi penceresi olan program (PE alt sistemi GUI)
   için sayfa "işlemi o pencerede yapın, bitince kapatın" der; launcher pencere kapanana kadar bekler (en fazla 2 saat).
6. **Döküm yeniden alınır, fark bulunur.** Program 0 ile çıktı ama oyunda hiçbir dosya değişmediyse kurulum
   **olmamış** sayılır (pencere "Kur"a basılmadan kapandı; araç oyunun arşivlerini bulamadı). Değişen ve eklenen
   dosyalar kayda `_program:<yol>` olarak yazılır; uninstall onları silmez (bir kısmı aracın kendi yedeği),
   geri almayı araca bırakır.
7. **Kaldırma programı saklanır**: paketin kopyası `<AppData>\programs\<appId>`'ye; katalog yeni bir paket
   yayımlayıp eski açılmış paket silinse de kaldırma yapılabilir.

## Kaldırma sırası

`GameService::runUndoPrograms` → `LocalPackageManager::runUndoPrograms`: kayıtta `_program:` varsa (ya da kayıt
bu izlemeden önce yazılmışsa) `uninstall` programı saklanan kopyadan, aynı geçici kopya ve klasör düzeltmesiyle
çalıştırılır ve **beklenir**; çıktısı sayfada. Hata kodu → kayıt kalır, mesaj gösterilir. 0 ile çıkıp oyunda
hiçbir şey değişmediyse ilk seferde uyarılır (kaldırma penceresi basılmadan kapandı), ikinci "Kaldır"da kayıt
silinir (yama zaten gitmiş olabilir). Sonra launcher'ın kendi kopyaladıkları ve `.makine` silinir.

Başarısız bir kurulumda program oyunda bir şey değiştirdiyse kayıt kalır ve sayfa "Kaldır"ı gösterir:
geri almayı yine programın kendisi yapar.

## Çevirmen için kısa kural

- Oyun klasörünü **argümanla** al (`--oyun <klasör>`, `--game <klasör>`, `%1`) ya da `MAKINE_OYUN` ortam
  değişkeninden / oyun klasöründeki `.makine`'den oku. Betikte sabit yazdıysan launcher düzeltir.
- Kaldırma komutu olsun (`restore <klasör>`, `--kaldir`); yoksa "Kaldır" yamayı geri alamaz.
- Başarısızlıkta 0 dışı kodla çık. Python/Node gerektiren betikler oyuncuda çalışmaz: oyuncuya giden şey
  tek başına çalışan program olmalı (HeavyRainTool'da `HR_yama\HeavyRain_Turkce_Yama.exe`).
- Panel (`/admin/paketler`) programın kendi kullanım metninden (`ACS.exe pack … [--game klasor]`,
  `restore [oyun_klasoru]`) argümanları kendisi çıkarır; çevirmenin betikleri `--oyun "%OYUN%"` diyorsa onu kullanır.

## Denendi (2026-10-22, dev build)

| Durum | Sonuç |
|---|---|
| Heavy Rain paketi (CDN, `set OYUN=game.path`) sahte klasöre | Betik `--oyun "C:\Users\…\Desktop\heavy rain"` aldı; araç arşivi bulamadı, 0 ile çıktı → "hiçbir dosyayı değiştirmedi", kayıt yok, `.makine` silindi |
| Aynı, `requires: HeavyRain.exe, Resources/BigFile_WIN.idx, …` | Program çalışmadan durdu: eksik dosyalar + "Klasörü değiştir" |
| Deneme betiği (sabit `D:\SteamLibrary\…\HEAVY RAIN`) dosya yazıp siler | Kurulum: 2 dosya eklendi, kayıtta `_program:`; Kaldır: betik saklanan kopyadan çalıştı, 2 dosya silindi, kayıt ve kopya gitti |
| `ACS.exe pack … --game <sahte klasör>` | Gerçek klasör argümanla gitti; sahte forge atlandı, 0 değişiklik → kurulum olmamış sayıldı (`Console.ReadKey` yönlendirilmiş stdin'de takılmıyor) |
