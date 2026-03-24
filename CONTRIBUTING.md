# Katkıda Bulunma Rehberi

Makine-Launcher'a katkıda bulunmak istediğiniz için teşekkür ederiz!

Makine-Launcher, Türkçe oyun çeviri ekosisteminin masaüstü uygulamasıdır. Qt 6 / QML ve C++23 ile geliştirilmektedir. Topluluğumuz, kaliteli Türkçe oyun çevirilerini herkes için erişilebilir kılmayı amaçlar.

---

## Gereksinimler

| Araç | Sürüm | Not |
|------|--------|-----|
| Qt | 6.10.1 | MinGW 13.1.0 kit dahil |
| CMake | 3.28+ | Qt ile birlikte gelir |
| MinGW | 13.1.0 | Qt installer ile kurulur |
| MSVC | 2022 (opsiyonel) | Release build için |
| vcpkg | Güncel | Bağımlılık yönetimi |
| just | Güncel | Komut çalıştırıcı |
| Git | 2.x | Sürüm kontrolü |

---

## Geliştirme Ortamı Kurulumu

### 1. Depoyu klonlayın

```bash
git clone https://github.com/MakineCeviri/Makine-Launcher.git
cd Makine-Launcher

# Geliştiriciler (yazma erişimi olanlar) ayrıca private Dev deposunu ekleyebilir:
# git remote add dev https://github.com/MakineCeviri/Makine-Launcher-Dev.git
```

### 2. PATH ayarlarını yapın

Bash ortamınıza aşağıdaki satırları ekleyin:

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:$PATH"
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"
```

### 3. Derleme ve çalıştırma

```bash
just dev        # Tam derleme (Core + UI, vcpkg gerektirir)
just dev-ui     # Yalnızca UI derlemesi (vcpkg gerekmez)
just run        # Uygulamayı çalıştırma
just test       # Testleri çalıştırma
```

---

## Katkı Süreci

### 1. Issue oluşturun veya mevcut bir issue seçin

Yeni bir özellik veya hata düzeltmesi üzerinde çalışmadan önce, ilgili bir [GitHub Issue](https://github.com/MakineCeviri/Makine-Launcher/issues) oluşturun veya mevcut bir issue üzerinde çalışacağınızı belirtin. Issue ve PR'lar public repo üzerinden yapılır.

### 2. Branch oluşturun

```bash
git checkout -b feat/ozellik-adi      # Yeni özellik
git checkout -b fix/hata-aciklamasi   # Hata düzeltmesi
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

---

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

### Kapsamlar

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

---

## Kod Stili

### Dosya Adlandırma

| Katman | Uzantı | Stil | Örnek |
|--------|--------|------|-------|
| Core C++ | `.hpp` / `.cpp` | `snake_case` | `game_detector.hpp` |
| UI C++ | `.h` / `.cpp` | `camelCase` | `gameService.h` |
| QML | `.qml` | `PascalCase` | `GameDetailScreen.qml` |

### Genel Kurallar

- **Standart:** C++23
- **Sınıflar:** `PascalCase` · **Fonksiyonlar/değişkenler:** `camelCase` · **Sabitler:** `UPPER_SNAKE_CASE`
- **Header guard:** `#pragma once`
- **Kod yorumları:** İngilizce

---

## Bilinen MinGW Sorunları

Bu projeyi MinGW GCC 13.1 ile derlerken dikkat edilmesi gereken noktalar:

| Sorun | Çözüm |
|-------|-------|
| `<regex>` başlığı çalışmaz | `find()`, `starts_with()`, `ends_with()` kullanın |
| `<set>` / `<map>` implicit değil | Her zaman açıkça `#include` edin |
| spdlog ADL çakışması | `spdlog::info()` tam nitelikli kullanın |

---

## Sorun Bildirme

[GitHub Issues](https://github.com/MakineCeviri/Makine-Launcher/issues) sayfasını kullanın:

- Sorunu net ve tekrarlanabilir şekilde açıklayın
- İşletim sistemi, Qt sürümü ve derleyici bilgilerini ekleyin
- Mümkünse ekran görüntüsü veya log çıktısı paylaşın
- Mevcut issue'ları kontrol ederek tekrar açmaktan kaçının

---

## Lisans

Bu projeye katkıda bulunarak, katkılarınızın projenin lisansı olan [AGPL-3.0 + Commons Clause](LICENSE) kapsamında lisanslanacağını kabul etmiş olursunuz.

---

Sorularınız mı var? [GitHub Discussions](https://github.com/MakineCeviri/Makine-Launcher/discussions) üzerinden bize ulaşabilirsiniz.

> **Not:** Bu proje iki depolu yapıda çalışır. Topluluk katkıları public [Makine-Launcher](https://github.com/MakineCeviri/Makine-Launcher) deposuna yapılır. Geliştirme ekibi private Makine-Launcher-Dev deposunu kullanır ve curated release'ler public repoya push edilir.
