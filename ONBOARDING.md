# Makine Çeviri'ye hoş geldin

Bu rehber Makine Çeviri'nin geliştirmesini devralacak kişi için. Sonunda bilgisayarında launcher'ı
derleyip çalıştırmış, testleri geçirmiş, sistemin nasıl bir arada durduğunu bilir ve ilk işine
başlamış olacaksın. Claude Code ile açarsan Claude seni adım adım götürür; her depoda bir
`CLAUDE.md` var, Claude onları kendiliğinden okur.

> **Önce oku:** Makine-Launcher'da [`docs/handoff-2026-10-22.md`](docs/handoff-2026-10-22.md). Bugünkü
> durum, açık işler ve pahalıya öğrenilmiş tuzaklar orada. Bu rehber kurulumu ve çalışma düzenini anlatır.

---

## 1. Makine Çeviri nedir, nerede çalışır

Oyunlar için Türkçe yama dağıtan ücretsiz bir platform. Oyuncu launcher'ı kurar; launcher
bilgisayardaki oyunları bulur, Türkçe yamayı indirip doğru klasöre kurar, istenince geri alır.

| Depo (GitHub `MakineCeviri`) | Ne | Nerede çalışır |
|---|---|---|
| **Makine-Launcher** (açık) | Masaüstü uygulaması — Qt 6 / QML + C++23 | Oyuncunun Windows'u. Dağıtım: Microsoft Store (MSIX) + GitHub Releases (ZIP) |
| **Makine-WEB** (özel) | makineceviri.org + API + yönetici paneli (`/admin`) — Astro + Hono, TypeScript | Cloudflare Workers; veritabanı D1 `makine-catalog`; dosyalar R2 `cdn.makineceviri.org` |
| **Makine-Launcher-Assets** (özel) | Katalog `assets/index.json`, kapaklar, bantlar | `main`'e push = R2'ye eşitlenir |
| **Makine-Bot** (özel) | Discord botu (Rust) + MakineAI Worker (Cloudflare) | Bot kurucunun sunucusunda (Docker), Worker Cloudflare'de |
| **Makine-LauncherPlugin-*** (açık) | Eklenti SDK'sı, şablonu, örnekler | Launcher'ın eklenti sistemi (henüz geliştirici bayrağı arkasında) |

Makine-Backend ve Makine-Launcher-Dev emekli; dokunma.

**Bir yamanın yolculuğu:** çevirmen paketi panelden yükler → panel `.makine` paketini (tar + zstd +
AES-256-GCM) R2'ye, kurulum tarifini D1'e yazar ve katalog sürümünü artırır → launcher'lar yeni
`index.json`'u alır, oyunun sayfası açılınca tarifi `/api/v2/games/<id>`'den çeker → paketi indirip
SHA-256'sını doğrular, değişecek oyun dosyalarını yedekler, kurar. Ayrıntı: `CLAUDE.md` → "Package
catalog — hybrid model", [`docs/install-flow.md`](docs/install-flow.md).

---

## 2. Erişimler — kurucudan iste

Bunlar olmadan da launcher'ı derleyip arayüz üzerinde çalışabilirsin (bkz. 4. adım). Yama kurmayı
denemek, yayın yapmak ve siteye dokunmak için gerekenler:

| Ne | Ne için | Nasıl gelir |
|---|---|---|
| GitHub `MakineCeviri` org üyeliği | Özel depolar (WEB, Assets, Bot), push | Kurucu davet eder; GitHub kullanıcı adını ilet |
| Paket şifreleme anahtarı | Gerçek paketleri açan launcher derlemesi, paket üretme | Güvenli kanaldan (asla sohbet/e-posta düz metni değil). `scripts/.encryption_key` dosyasına koyarsın |
| Cloudflare (Makine hesabı) | D1, R2, Workers, DNS | Hesaba üye daveti ya da kapsamı dar bir API jetonu |
| Sentry `makineceviri` org | Hata raporları (launcher + site) | Org daveti |
| Panel hesabı (`makineceviri.org/admin`) | Paket yayınlama | Discord ile giriş; kurucu rol verir |
| Microsoft Partner Center | Store'a sürüm gönderme | Şimdilik kurucuda kalıyor |

**Kural:** anahtar, jeton, şifre hiçbir dosyaya, commit'e, belgeye girmez. `.env*`, `encryption_key.h`,
`scripts/.encryption_key`, `*.pfx` `*.pem` `*.key` zaten `.gitignore`'da. Belgelerde yalnız **nerede**
durduğu yazılır, değeri asla.

**Paket anahtarı değiştirilemez:** dağıtılmış her launcher'a gömülü. Sızarsa paketlerin gizliliği
gider ama değiştirmek bütün eski launcher'ları bozar — onu korumanın tek yolu kimseye düz metin
göndermemek.

---

## 3. Bilgisayarını hazırla (Windows 10/11)

| Araç | Sürüm / not |
|---|---|
| Git | `winget install Git.Git` — Git Bash ile çalışıyoruz |
| Qt | **6.11.1**, online installer'dan: *Qt 6.11.1 → MinGW 13.1.0 64-bit* ve *Developer and Designer Tools → MinGW 13.1.0 64-bit, CMake, Ninja*. Yol `C:\Qt` olmalı (preset'ler `C:\Qt\6.11.1\mingw_64`'ü bekler) |
| vcpkg | `git clone https://github.com/microsoft/vcpkg C:\vcpkg` → `C:\vcpkg\bootstrap-vcpkg.bat` → `setx VCPKG_ROOT C:\vcpkg`. **Klasik mod**, triplet `x64-mingw-dynamic` |
| just | `winget install Casey.Just` |
| Python | 3.11+ (`winget install Python.Python.3.13`) — betikler ve paketleme |
| Node.js | 22 (`winget install OpenJS.NodeJS.LTS`) — yalnız Makine-WEB / Bot Worker için |
| Claude Code | `npm install -g @anthropic-ai/claude-code` (ya da masaüstü uygulaması) |

Git Bash'te PATH (her oturumda, ya da `~/.bashrc`'ye):

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:$PATH"
export PATH="/c/Qt/6.11.1/mingw_64/bin:$PATH"   # çalışırken Qt DLL'leri
```

Sistemde başka bir CMake (pip'ten 4.x) varsa Qt'nin CMake'i önde olmalı; yoksa yapılandırma reddedilir.

---

## 4. İlk derleme

```bash
mkdir -p /c/Workspace/Makine && cd /c/Workspace/Makine
git clone https://github.com/MakineCeviri/Makine-Launcher.git
cd Makine-Launcher
git switch dev                      # geliştirme dalı; main ayrı geçmişli
just setup-tests                    # vcpkg: openssl, nlohmann-json, zstd, spdlog, gtest (openssl ~25 dk sürer)
```

**Paket anahtarı:** gerçek anahtar gelene kadar geçici bir anahtarla derle (CI da böyle yapıyor).
Bu derleme her şeyi gösterir ama CDN'deki gerçek paketleri açamaz:

```bash
python -c "import pathlib, secrets; pathlib.Path('scripts/.encryption_key').write_text(secrets.token_hex(32))"
python scripts/generate_key_header.py   # → qml/src/services/encryption_key.h
```

Gerçek anahtar gelince `scripts/.encryption_key`'in içeriğini onunla değiştirip son komutu tekrar çalıştır.

```bash
just dev        # Core + UI + testler → build/dev
just test       # 32 test takımı; hepsi geçmeli
just run        # launcher'ı aç
```

Yalnız arayüz üzerinde çalışacaksan vcpkg'siz, anahtarsız: `just dev-ui`.

Derleme çıktısı hep `build/` altında; Masaüstüne asla bir şey yazma.

---

## 5. Claude Code ile çalışmak

- Depo kökünde `claude` çalıştır. Kök `CLAUDE.md` (mimari, derleme, tuzak tablosu) ve `.claude/CLAUDE.md`
  (güncel durum) kendiliğinden yüklenir. Başka bir depoya geçince orada yeni oturum aç.
- İlk oturumda iyi bir başlangıç: *"docs/handoff-2026-10-22.md'yi oku, durumu ve açık işleri özetle."*
- Yaptığı bir şeyin "çalıştığını" söylemesi yetmez: derleme ve test çıktısını, gerekiyorsa launcher'ı
  açıp ekranı görmesini iste. Oyun tanıma, kurulum ve yayın makineye özgüdür.
- Yeni öğrendiğin bir tuzak ya da karar → ilgili `CLAUDE.md`'ye, `docs/`'a ya da bilinen sorunlar tablosuna yaz.
  Bir sonraki kişi (ya da bir sonraki Claude oturumu) aynı yere tekrar düşmesin.

---

## 6. Çalışma düzeni

**Dallar:** Makine-Launcher'da `dev` = geliştirme ve sürüm etiketleri; `main` = varsayılan dal, ayrı
geçmişli (ortak atası yok — birleştirme yapılmaz). Zamanlanmış iş akışları `main`'de durur.
Makine-WEB'de **`main`'e push canlıya çıkar** (CI: test → build → Cloudflare deploy); büyük değişikliği
dalda yap, PR'la birleştir.

**Commit:** [Conventional Commits](https://www.conventionalcommits.org/), açıklama Türkçe:
`fix(ui): …`, `feat(core): …`, `docs: …` — türler `feat fix refactor build ci docs test chore`, kapsamlar
`core ui build ci docs`. Kod yorumları İngilizce. Commit'lerde ortak yazar satırı (`Co-Authored-By`) yok.

**CI:** `dev`'e her push Windows'ta derleyip bütün testleri çalıştırır (`.github/workflows/ci.yml`).
Kırmızıysa önce onu düzelt.

**Kod kuralları** (ayrıntı kök `CLAUDE.md`): çekirdek C++ `snake_case` dosyalar `core/`, arayüz servisleri
`qml/src/services/`, QML `PascalCase` ve iş mantığı içermez. Yeni arayüz [`docs/design.md`](docs/design.md)'deki
bileşenlerle (Card, AppButton, ListSection…) ve `Theme` token'larıyla yapılır; renk/yazı boyutu uydurulmaz.
Arayüz kendini anlatır: her düğmenin ve durumun ne yaptığını düz Türkçe söyle.

**Katalog verisi:** bir tarifi düzeltmek **D1** değişikliğidir (panelden), R2'deki eski `assets/packages/*.json`
değil. Launcher'lar `index.json`'u yalnız katalog sürümü artınca yeniden alır: panelden yayın bunu kendisi
yapar; panel dışı bir değişiklikte "Launcher'lara duyur". Yeni bir kurulum adımı türü eski launcher'ları
bozar → `steprules::kRecipeLevel`'i artır, eski tarifi `install_methods_compat`'a koy.

**D1'de** `wrangler d1 migrations apply` kullanma (eski migration'lar elle uygulanmış, toplu uygulama
veriyi ezer); tek dosyayı `wrangler d1 execute --remote --file …` ile çalıştır.

**Sürüm çıkarmak:** [`docs/RELEASING.md`](docs/RELEASING.md) — `just release-zip-dynamic <sürüm>` (testler +
dağıtım ZIP'i + Sentry'ye semboller), `dev`'de etiket, GitHub Release, eski sürüm notlarını arşive al.
Store için `just msix-dynamic …`.

**Telemetri:** iki kanal — sayım (site API → D1) ve Sentry (`makineceviri/makine-launcher`).
`just telemetry-check` en çok kullanıcıya dokunan hataların sıralı listesini verir;
[`docs/telemetry.md`](docs/telemetry.md). Geliştirme derlemelerinin olayları `development` ortamındadır.

---

## 7. Şu an nerede duruyoruz (2026-10-28)

- **Launcher 0.2.4-beta** GitHub Releases'ta. Microsoft Store 0.2.3'ü dağıtıyor; 0.2.4.0 sertifikasyonda.
- Güncel durum `.claude/CLAUDE.md`'nin başındaki "Durum" satırlarında, her sürümde güncellenir.
- Sıradaki işler önem sırasıyla [`docs/handoff-2026-10-22.md`](docs/handoff-2026-10-22.md) → "Açık işler" ve
  [`docs/v0.2.0-todo.md`](docs/v0.2.0-todo.md)'de.

**İlk hafta için önerilen yol:**
1. Derle, testleri geçir, launcher'ı aç; kendi oyunlarından birine bir yama kur ve kaldır.
2. `docs/install-flow.md` + `docs/patch-programs.md` + `docs/design.md`'yi oku.
3. `just telemetry-check`'in ilk sırasındaki hatayı al, kökünü bul, düzelt, `dev`'e gönder.

---

## 8. Takıldığında

- Önce depodaki belgeler: kök `CLAUDE.md`'deki bilinen sorunlar tablosu (MinGW, QML, Win32 tuzakları),
  `docs/handoff-2026-10-22.md` → "Tuzaklar".
- `git push` takılırsa (Git Credential Manager): `git -c credential.helper= -c 'credential.helper=!gh auth git-credential' push`.
- Erişim, anahtar, Store, iş kararları → kurucu.
