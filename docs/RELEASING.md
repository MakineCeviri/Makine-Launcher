# Sürüm Yayınlama — GitHub Releases

> Paketin nasıl üretildiği: [`release-packaging-guide.md`](release-packaging-guide.md).
> Bu belge **yayın düzenini** tanımlar: etiket, başlık, notlar, varlık adları ve arşiv politikası.

## Neden burası önemli

- Repo trafiğinin çoğu bu sayfada toplanıyor — Eylül 2026'da 14 günde 545 görüntüleme / 396 tekil ziyaretçi ile en çok gezilen yol.
- makineceviri.org/indir sayfasındaki **"Doğrudan kurulum paketi → GitHub Sürümleri"** bağlantısı buraya çıkar.
- Microsoft Store dışındaki tek kurulum yolu burasıdır. Sayfa boş kaldığında Store'a erişemeyen kullanıcının indirebileceği hiçbir şey kalmaz.

## Sürüm şeması

| Öğe | Kural | Örnek |
|-----|-------|-------|
| Etiket | `vX.Y.Z` — beta sürdüğü sürece `-beta` eki. Tek biçim; `…-SourceCode` gibi ikinci etiket açılmaz | `v0.1.5-beta` |
| Başlık | `vX.Y.Z-beta — <kısa tanım>`, arşive düşünce sonuna `(Arşiv)` | `v0.1.5-beta — Yama onarımı` |
| Notlar | `docs/release-notes/<etiket>.md` — yayımlanan gövdenin birebir kaynağı | `docs/release-notes/v0.1.5-beta.md` |
| Paket | `Makine-Launcher-v<sürüm>-win64.zip` + `SHA256SUMS.txt` | — |
| Pre-release | **İşaretleme.** Yalnızca pre-release varsa `/releases/latest` 404 döner ve sitedeki indirme bağlantısı boşa düşer. Beta olduğu başlıkta ve notlarda yazar | — |

## Sürüm geçmişi

| Sürüm | Tarih | Dağıtım | GitHub kaydı |
|-------|-------|---------|--------------|
| `v0.1.0-pre-alpha` | 13 Mart 2026 | Kapalı test | Arşiv (ikili yok) |
| `v0.1.0-beta` | 26 Mart 2026 | Kapalı beta, tek dosya statik EXE | Arşiv (ikili yok) |
| `v0.1.0-beta-SourceCode` | 2 Mayıs 2026 | Aynı hattın son yayın yapısı | Arşiv (özgün SHA-256 korunuyor) |
| `v0.1.1` | — | Hiç kullanılmadı, atlandı | — |
| `v0.1.2-beta` | 22 Temmuz 2026 | Microsoft Store (MSIX 0.1.2.0) | Arşiv (ikili yok) |
| `v0.1.3-beta` | 24 Temmuz 2026 | Microsoft Store (MSIX 0.1.3.0) | Arşiv (ikili yok) |
| `v0.1.4-beta` | 29 Temmuz 2026 | Microsoft Store (MSIX 0.1.4.0) | Arşiv (ikili yok, Store'a yönlendirir) |
| `v0.1.5-beta` | 6 Ekim 2026 | GitHub Releases (ZIP) + Microsoft Store (MSIX 0.1.5.0) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.1.6-beta` | 20 Ekim 2026 | GitHub Releases (ZIP) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.1.7-beta` | 22 Ekim 2026 | GitHub Releases (ZIP) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.2.0-beta` | 23 Ekim 2026 | GitHub Releases (ZIP) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.2.1-beta` | 23 Ekim 2026 | GitHub Releases (ZIP) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.2.2-beta` | 23 Ekim 2026 | GitHub Releases (ZIP) | Arşiv (ZIP + SHA256SUMS.txt korunuyor) |
| `v0.2.3-beta` | 25 Ekim 2026 | Microsoft Store (MSIX 0.2.3.0) — Store'da yayında (mağaza kataloğunda `Makineeviri.MakineAI_0.2.3.0`, 27 Ekim'de görüldü); GitHub ZIP'i yok | Etiket var (`47253d3`, MSIX'in yapıldığı commit), sürüm sayfası yok |
| `v0.2.4-beta` | 27 Ekim 2026 | GitHub Releases (ZIP) — **güncel**; Microsoft Store (MSIX 0.2.4.0) — 27 Ekim'de sertifikasyona | Var (ZIP + SHA256SUMS.txt); Makine-Companion `08f33f5` |

0.1.2–0.1.4 etiketleri arşiv düzeni için sonradan atıldı; Store paketinin hangi commit'ten üretildiği o sırada kaydedilmemişti. Etiketler, sürüm bump commit'leri ile MSIX sahneleme zamanından geriye götürülerek en yakın duruma yerleştirildi. 0.1.3'te numara önce Store paketine verilmiş, kaynak ağacı [`492f506`](https://github.com/MakineCeviri/Makine-Launcher/commit/492f506825fffc4053b677fcf35bcbba36f3e417) ile sonradan hizalanmıştı — **bu yüzden her Store gönderiminde önce etiket atılmalı.**

## Yeni sürüm çıkarma

```bash
# 1. Sürümü yükselt: CMakeLists.txt > VERSION + MAKINE_VERSION_SUFFIX
# 2. CHANGELOG.md: [Unreleased] içeriğini "## [X.Y.Z] - YYYY-MM-DD" bölümüne taşı
# 3. Notları yaz — yayımlanacak gövdenin kendisi
$EDITOR docs/release-notes/v0.1.5-beta.md
# 3b. Uygulamanın içindeki notlar (Yenilikler sayfası): qml/resources/whatsnew.json'ın
#     başına sürümün girdisi (oyuncunun diliyle, 3-6 madde), "roadmap"ı da gözden geçir.
#     test_releasenotes ilk girdinin derlenen sürüm olmasını ister.

# 4. Paketi üret (dinamik Qt — DLL'ler ZIP içinde; statik kit yok)
#    Önce sembolleri Sentry'ye yükler ve Sentry'den geri okur (scripts/upload_symbols.py);
#    yükleyemezse durur — sembolsüz yayının çökmeleri okunamaz (0.1.4'ün hepsi öyleydi).
just release-zip-dynamic 0.1.5-beta          # imzalı: just release-zip-dynamic-signed 0.1.5-beta
# → dist/Makine-Launcher-v0.1.5-beta-win64.zip + dist/SHA256SUMS.txt

# 4b. ZIP'teki exe'yi TEMİZ PROFİLLE aç: geliştirici makinesinde katalog önbellekte
#     olduğu için ilk açılış hiç denenmiyor — 0.2.0 her yeni kurulumda ilk sayfada
#     dondu, bizde hiç görünmedi. Profili kenara al, aç, kontrol et, geri koy:
#       reg export "HKCU\Software\MakineCeviri\Makine-Launcher" <yedek>.reg
#       mv "$LOCALAPPDATA/MakineCeviri/Makine-Launcher"{,.bak}; reg delete "HKCU\Software\MakineCeviri\Makine-Launcher" /f
#       (aç: karşılama → oyunlar → tercihler → ana sayfa; günlükte qrc:/RangeError yok, pencere yanıt veriyor)
#       rm -rf …/Makine-Launcher; mv …/Makine-Launcher{.bak,}; reg import <yedek>.reg

# 5. Yayınla (etiketi sunucu tarafında oluşturur, pre-release DEĞİL)
gh release create v0.1.5-beta   dist/Makine-Launcher-v0.1.5-beta-win64.zip dist/SHA256SUMS.txt   --target $(git rev-parse HEAD)   --title "v0.1.5-beta — <kısa tanım>"   --notes-file docs/release-notes/v0.1.5-beta.md   --latest

# 6. Doğrula
gh release view v0.1.5-beta
curl -sI https://github.com/MakineCeviri/Makine-Launcher/releases/latest | head -1   # 302 + /tag/v0.1.5-beta

# 6b. Launcher'lara yeni sürümü duyur: CDN'deki manifest (store = Store'da ŞU AN olan sürüm)
just launcher-json 0.1.5-beta <store'daki sürüm>
cd ../Makine-Launcher-Assets && git add assets/launcher.json && git commit -m "data(launcher): 0.1.5-beta" && git push
curl -s https://cdn.makineceviri.org/assets/launcher.json | head -4                  # R2'ye eşitlendi mi

# 7. Bir önceki sürümün başlığına "(Arşiv)" ekle, gövdesine güncel sürüm bandını koy
# 7b. Birkaç gün sonra: just telemetry-watch — yeni sürümün çökmesiz oturum oranı
#     bir öncekiyle karşılaştırılır (oturumlar hata kotasına takılmaz)
# 8. Store paketi ayrı akış: just msix-dynamic <4 parçalı sürüm> <identity> <publisher>
#    Partner Center'daki "Bu sürümdeki yenilikler" metni: packaging/msix/store-whats-new.txt
# 8b. Store paketi YAYIMLANINCA (sertifika bitip mağazada görünce): just launcher-json <latest> <yeni store sürümü>
#     ve Assets'e push. Önce yapılırsa Store'dan kuranlar boş bir Store sayfasına gönderilir.
```

## Arşiv politikası

- **Eski kayıt silinmez.** Dışarıya verilmiş bağlantılar kırılmasın diye etiket ve sayfa yerinde kalır; başlığa `(Arşiv)`, gövdenin başına güncel sürüme yönlendiren bant eklenir.
- **Etiket hafif (lightweight) olmalı.** GitHub, bir sürümün `created_at` alanını etiketin gösterdiği **commit'in tarihinden** alır ve liste bu alana göre sıralanır. Annotated etikette tarih etiketin oluşturulduğu gün olur; geçmişe dönük bir kayıt listenin en üstüne çıkıp kronolojiyi bozar.
- **İkili paket arşivlenmiyorsa** gövdede açıkça yazılır. Özgün yapının SHA-256 özeti biliniyorsa korunur — elinde eski dosya olan kullanıcı doğrulayabilsin.
- Kullanılmamış sürüm numaraları (0.1.1) için kayıt açılmaz; yukarıdaki tabloda açıklanır.
- **Store'a gönderilen her paket için de etiket at.** Sürüm numarası yalnız MSIX komut satırında kalırsa hangi commit'in yayımlandığı kaybolur.

## Bilinen sınırlar

- **İçeriden güncelleme GitHub'ı kullanamaz.** `UpdateService` indirme host'u olarak yalnızca `cdn.makineceviri.org` ve `makineceviri.org` adreslerine izin verir (`qml/src/services/updateservice.cpp`), ve `SelfUpdater` indirilen dosyayı çalıştırılabilir gibi takas eder — ZIP ile çalışmaz. 0.2.3'ten beri launcher yeni sürümü **haber verir**: ZIP'te "İndir" sürüm sayfasını açar (kurulumu oyuncu yapar), Store'da "Store'da güncelle" Store'u açar.
- **İki güncelleme dosyası.** 0.2.3+ `assets/launcher.json`'ı okur (`releasenotes.h`; `just launcher-json`). 0.2.2 ve öncesi `assets/update.json`'ı okur; o dosya `0.1.0-pre-alpha` kaydıyla bilerek eski bırakıldı. Yeni bir sürüm yazılırsa `url`'süz kayıt o launcher'larda Ayarlar'da "Denetlenemedi" gösterir. 0.1.6'dan beri eski sürümlü kayıt "güncel" sayılır.
- **Dev kanalı gövdeden `SHA256:` satırını okur.** `MAKINE_DEV_TOOLS` yapıları `releases/latest` çağırıp gövdedeki `SHA256: <hash>` satırını ve `.exe` uzantılı varlığı arar; ZIP-only bir sürümde sessizce boşta kalır (çökmez).
- **Statik Qt kiti kurulu değil** — tek dosya EXE üretilemiyor. Yayın paketi Qt DLL'lerini taşıyan ZIP'tir (~50 MB).

## Ortam tuzakları

- **`.ps1` dosyaları UTF-8 BOM + CRLF olmalı** (`.gitattributes` zaten `eol=crlf` diyor). BOM'suz UTF-8'de Windows PowerShell 5.1 dosyayı ANSI okur; Türkçe karakterler ve em dash akıllı tırnağa dönüşüp ayrıştırmayı bozar.
- **`Get-FileHash` bu makinede 5.1'den çağrıldığında yok** — `PSModulePath` içine PowerShell 7 modül dizini sızdığı için Utility modülü yüklenemiyor. Yayın scriptleri .NET `SHA256` kullanır.
- **`build/release-mingw/` kaldırılan bağımlılıkların DLL'lerini tutar.** ZIP ve MSIX scriptleri oradaki her `*.dll`'i pakete kopyalar; vcpkg bağımlılığı çıkınca DLL'i dizinde kalıp pakete girer (0.1.5'te `libsimdjson`, `libsodium`, `libsqlite3`). Paketlemeden önce `objdump -p build/release-mingw/Makine-Launcher.exe | grep "DLL Name"` ile karşılaştır, fazlasını sil.
- **`gh release edit` arşivlenen kaydı taslağa çevirebiliyor.** 0.1.6 yayınında `gh release edit v0.1.5-beta --title … --notes-file …` kaydı `draft: true` yaptı: sayfası ve ZIP bağlantısı herkese 404 oldu. Düzenlemeden sonra `gh release view <etiket> --json isDraft` ile bak; taslağa düştüyse `gh api -X PATCH repos/MakineCeviri/Makine-Launcher/releases/<id> -F draft=false -f make_latest=false` (`make_latest=false` olmazsa eski sürüm "latest"i geri alabilir).
- **vcpkg'nin PowerShell yolu Store güncellemesinde eskir.** `build/<preset>/CMakeCache.txt` `Z_VCPKG_POWERSHELL_PATH` / `Z_VCPKG_PWSH_PATH`'i `C:\Program Files\WindowsApps\Microsoft.PowerShell_<sürüm>…\pwsh.exe` olarak tutar; PowerShell güncellenince klasör değişir ve bağlama sonrası `applocal.ps1` adımı "Sistem belirtilen yolu bulamıyor" ile düşer (0.1.6'da `makine-elevate.exe`). Sürümden bağımsız takma adı ver: `cmake --preset release-mingw -DZ_VCPKG_POWERSHELL_PATH=C:/Users/<ad>/AppData/Local/Microsoft/WindowsApps/pwsh.exe -DZ_VCPKG_PWSH_PATH=<aynı>` (dev önbelleği zaten böyle).
- **Etiket push'u pre-push kapılarını çalıştırır** (build + test, dakikalar sürer). `gh release create --target <sha>` etiketi sunucu tarafında oluşturur; zaten push edilmiş bir commit'i etiketlemek için bu yeterlidir.
