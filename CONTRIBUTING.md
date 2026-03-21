# Katkıda Bulunma Rehberi

Makine-Launcher'a katkıda bulunmak istediğiniz için teşekkür ederiz! Bu belge, projeye katkı sürecini açıklar.

Makine-Launcher, Türkçe oyun çeviri ekosisteminin masaüstü uygulamasıdır. Qt6/QML ve C++23 ile geliştirilmektedir. Topluluğumuz, kaliteli Türkçe oyun çevirilerini herkes için erişilebilir kılmayı amaçlar.

## Gereksinimler

| Araç | Sürüm | Not |
|------|--------|-----|
| Qt | 6.10.1 | MinGW 13.1.0 kit dahil |
| CMake | 3.25+ | Qt ile birlikte gelir |
| MinGW | 13.1.0 | Qt installer ile kurulur |
| MSVC | 2022 (opsiyonel) | Release build için |
| vcpkg | Güncel | Bağımlılık yönetimi |
| just | Güncel | Komut çalıştırıcı |
| Git | 2.x | Sürüm kontrolü |

## Geliştirme Ortamı Kurulumu

### 1. Depoyu klonlayın

```bash
git clone https://github.com/MakineCeviri/Makine-Launcher.git
cd Makine-Launcher
```

### 2. PATH ayarlarını yapın

Bash ortamınıza aşağıdaki satırları ekleyin:

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:$PATH"
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"
```

### 3. Derleme ve çalıştırma

```bash
# Tam derleme (Core + UI, vcpkg gerektirir)
just dev

# Yalnızca UI derlemesi (vcpkg gerekmez)
just dev-ui

# Uygulamayı çalıştırma
just run

# Testleri çalıştırma
just test
```

## Katkı Süreci

### 1. Issue oluşturun veya mevcut bir issue seçin

Yeni bir özellik veya hata düzeltmesi üzerinde çalışmadan önce, ilgili bir [GitHub Issue](https://github.com/MakineCeviri/Makine-Launcher/issues) oluşturun veya mevcut bir issue üzerinde çalışacağınızı belirtin.

### 2. Branch oluşturun

```bash
# Yeni özellik için
git checkout -b feat/ozellik-adi

# Hata düzeltmesi için
git checkout -b fix/hata-aciklamasi
```

### 3. Değişikliklerinizi yapın

Aşağıdaki kod stili kurallarına uyduğunuzdan emin olun.

### 4. Pull Request açın

- Branch'inizi push edin ve GitHub üzerinden PR açın.
- PR açıklamasında ilgili issue numarasını referans verin.
- Değişikliklerinizi kısa ve net bir şekilde açıklayın.
- CI kontrollerinin geçtiğinden emin olun.

### 5. Code Review

- En az bir proje bakımcısının onayı gereklidir.
- İstenen değişiklikleri yapın ve tekrar review talep edin.
- Onay sonrası merge işlemi bakımcılar tarafından yapılır.

## Commit Kuralları

[Conventional Commits](https://www.conventionalcommits.org/) formatını kullanıyoruz:

```
type(scope): açıklama
```

### Tipler

| Tip | Kullanım |
|-----|----------|
| `feat` | Yeni özellik |
| `fix` | Hata düzeltmesi |
| `refactor` | Davranış değişikliği olmayan kod yeniden yapılandırması |
| `build` | Derleme sistemi, bağımlılıklar |
| `ci` | CI/CD değişiklikleri |
| `docs` | Dokümantasyon |
| `test` | Test ekleme veya güncelleme |
| `chore` | Bakım, temizlik |

### Kapsamlar (Scope)

| Kapsam | Alan |
|--------|------|
| `core` | C++ çekirdek kütüphanesi (`core/`) |
| `ui` | QML arayüzü ve backend servisleri (`qml/`) |
| `build` | CMake, preset'ler, justfile |
| `ci` | GitHub Actions, hook'lar |
| `docs` | Dokümantasyon dosyaları |

### Örnekler

```
feat(ui): add game detail hero banner
fix(core): handle empty manifest on first sync
refactor(ui): extract TranslationActionButton states
build: update vcpkg baseline
```

## Kod Stili

### Dosya Adlandırma

| Katman | Dosya Uzantısı | Stil |
|--------|----------------|------|
| Core C++ | `.hpp` / `.cpp` | `snake_case` |
| UI C++ | `.h` / `.cpp` | `camelCase` |
| QML | `.qml` | `PascalCase` |

### Genel Kurallar

- **Sınıflar:** `PascalCase`
- **Fonksiyonlar ve değişkenler:** `camelCase`
- **Sabitler:** `UPPER_SNAKE_CASE`
- **Kod yorumları:** İngilizce yazılmalıdır
- **Header guard:** `#pragma once` kullanın
- **Standart:** C++23

## Bilinen MinGW Sorunları

Bu projeyi MinGW GCC 13.1 ile derlerken dikkat edilmesi gereken noktalar:

- **`<regex>` başlığı çalışmaz.** Bunun yerine `std::string::find()`, `starts_with()`, `ends_with()` gibi string metodlarını kullanın.
- **`#include <set>` ve `#include <map>` açıkça yazılmalıdır.** MSVC bunları dolaylı olarak dahil eder, MinGW etmez.
- **spdlog ADL çakışması:** `using namespace makine` aktifken `spdlog::info()` yerine `makine::info()` çağrılabilir. Her zaman tam nitelikli `spdlog::info()` kullanın.

## Sorun Bildirme

Hata veya öneri bildirmek için [GitHub Issues](https://github.com/MakineCeviri/Makine-Launcher/issues) sayfasını kullanın. Issue açarken:

- Sorunu net ve tekrarlanabilir şekilde açıklayın.
- İşletim sistemi, Qt sürümü ve derleyici bilgilerini ekleyin.
- Mümkünse ekran görüntüsü veya log çıktısı paylaşın.
- Mevcut issue'ları kontrol ederek tekrar açmaktan kaçının.

## Lisans

Bu projeye katkıda bulunarak, katkılarınızın projenin lisansı olan [AGPL-3.0 + Commons Clause](LICENSE) kapsamında lisanslanacağını kabul etmiş olursunuz.

---

Sorularınız mı var? [GitHub Discussions](https://github.com/MakineCeviri/Makine-Launcher/discussions) üzerinden bize ulaşabilirsiniz.
