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
| `v0.1.5-beta` | 21 Eylül 2026 | GitHub Releases (ZIP) + Microsoft Store — **güncel** | Var (ZIP + SHA256SUMS.txt) |

0.1.2–0.1.4 etiketleri arşiv düzeni için sonradan atıldı; Store paketinin hangi commit'ten üretildiği o sırada kaydedilmemişti. Etiketler, sürüm bump commit'leri ile MSIX sahneleme zamanından geriye götürülerek en yakın duruma yerleştirildi. 0.1.3'te numara önce Store paketine verilmiş, kaynak ağacı [`492f506`](https://github.com/MakineCeviri/Makine-Launcher/commit/492f506825fffc4053b677fcf35bcbba36f3e417) ile sonradan hizalanmıştı — **bu yüzden her Store gönderiminde önce etiket atılmalı.**

## Yeni sürüm çıkarma

```bash
# 1. Sürümü yükselt: CMakeLists.txt > VERSION + MAKINE_VERSION_SUFFIX
# 2. CHANGELOG.md: [Unreleased] içeriğini "## [X.Y.Z] - YYYY-MM-DD" bölümüne taşı
# 3. Notları yaz — yayımlanacak gövdenin kendisi
$EDITOR docs/release-notes/v0.1.5-beta.md

# 4. Paketi üret (dinamik Qt — DLL'ler ZIP içinde; statik kit yok)
#    Önce sembolleri Sentry'ye yükler ve Sentry'den geri okur (scripts/upload_symbols.py);
#    yükleyemezse durur — sembolsüz yayının çökmeleri okunamaz (0.1.4'ün hepsi öyleydi).
just release-zip-dynamic 0.1.5-beta          # imzalı: just release-zip-dynamic-signed 0.1.5-beta
# → dist/Makine-Launcher-v0.1.5-beta-win64.zip + dist/SHA256SUMS.txt

# 5. Yayınla (etiketi sunucu tarafında oluşturur, pre-release DEĞİL)
gh release create v0.1.5-beta   dist/Makine-Launcher-v0.1.5-beta-win64.zip dist/SHA256SUMS.txt   --target $(git rev-parse HEAD)   --title "v0.1.5-beta — <kısa tanım>"   --notes-file docs/release-notes/v0.1.5-beta.md   --latest

# 6. Doğrula
gh release view v0.1.5-beta
curl -sI https://github.com/MakineCeviri/Makine-Launcher/releases/latest | head -1   # 302 + /tag/v0.1.5-beta

# 7. Bir önceki sürümün başlığına "(Arşiv)" ekle, gövdesine güncel sürüm bandını koy
# 7b. Birkaç gün sonra: just telemetry-watch — yeni sürümün çökmesiz oturum oranı
#     bir öncekiyle karşılaştırılır (oturumlar hata kotasına takılmaz)
# 8. Store paketi ayrı akış: just msix-dynamic <4 parçalı sürüm> <identity> <publisher>
```

## Arşiv politikası

- **Eski kayıt silinmez.** Dışarıya verilmiş bağlantılar kırılmasın diye etiket ve sayfa yerinde kalır; başlığa `(Arşiv)`, gövdenin başına güncel sürüme yönlendiren bant eklenir.
- **Etiket hafif (lightweight) olmalı.** GitHub, bir sürümün `created_at` alanını etiketin gösterdiği **commit'in tarihinden** alır ve liste bu alana göre sıralanır. Annotated etikette tarih etiketin oluşturulduğu gün olur; geçmişe dönük bir kayıt listenin en üstüne çıkıp kronolojiyi bozar.
- **İkili paket arşivlenmiyorsa** gövdede açıkça yazılır. Özgün yapının SHA-256 özeti biliniyorsa korunur — elinde eski dosya olan kullanıcı doğrulayabilsin.
- Kullanılmamış sürüm numaraları (0.1.1) için kayıt açılmaz; yukarıdaki tabloda açıklanır.
- **Store'a gönderilen her paket için de etiket at.** Sürüm numarası yalnız MSIX komut satırında kalırsa hangi commit'in yayımlandığı kaybolur.

## Bilinen sınırlar

- **İçeriden güncelleme GitHub'ı kullanamaz.** `UpdateService` indirme host'u olarak yalnızca `cdn.makineceviri.org` ve `makineceviri.org` adreslerine izin verir (`qml/src/services/updateservice.cpp`), ve `SelfUpdater` indirilen dosyayı çalıştırılabilir gibi takas eder — ZIP ile çalışmaz. Store dışı kurulumda güncelleme şimdilik elle yapılır.
- **`assets/update.json` güncel değil** (R2'de `0.1.0-pre-alpha`, `url` boş). Store dışı otomatik güncelleme açılacaksa hem bu dosya hem de bir kurulum EXE'si gerekir.
- **Dev kanalı gövdeden `SHA256:` satırını okur.** `MAKINE_DEV_TOOLS` yapıları `releases/latest` çağırıp gövdedeki `SHA256: <hash>` satırını ve `.exe` uzantılı varlığı arar; ZIP-only bir sürümde sessizce boşta kalır (çökmez).
- **Statik Qt kiti kurulu değil** — tek dosya EXE üretilemiyor. Yayın paketi Qt DLL'lerini taşıyan ZIP'tir (~50 MB).

## Ortam tuzakları

- **`.ps1` dosyaları UTF-8 BOM + CRLF olmalı** (`.gitattributes` zaten `eol=crlf` diyor). BOM'suz UTF-8'de Windows PowerShell 5.1 dosyayı ANSI okur; Türkçe karakterler ve em dash akıllı tırnağa dönüşüp ayrıştırmayı bozar.
- **`Get-FileHash` bu makinede 5.1'den çağrıldığında yok** — `PSModulePath` içine PowerShell 7 modül dizini sızdığı için Utility modülü yüklenemiyor. Yayın scriptleri .NET `SHA256` kullanır.
- **Etiket push'u pre-push kapılarını çalıştırır** (build + test, dakikalar sürer). `gh release create --target <sha>` etiketi sunucu tarafında oluşturur; zaten push edilmiş bir commit'i etiketlemek için bu yeterlidir.
