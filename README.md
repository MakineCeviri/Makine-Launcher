<div align="center">

# MakineAI

**Windows oyunlarını Türkçe'ye çeviren masaüstü uygulaması**

[![Version](https://img.shields.io/badge/version-0.1.0--alpha-blue?style=flat-square)](https://github.com/jlceaser/MakineAI/releases)
[![License](https://img.shields.io/github/license/jlceaser/MakineAI?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/github/actions/workflow/status/jlceaser/MakineAI/ci.yml?branch=main&style=flat-square&label=CI)](https://github.com/jlceaser/MakineAI/actions)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?style=flat-square&logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![Qt6](https://img.shields.io/badge/Qt-6.10-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)
[![Windows](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D6?style=flat-square&logo=windows)](https://github.com/jlceaser/MakineAI)

Steam, Epic, GOG kütüphanenizdeki oyunları tek tıkla Türkçe'ye çevirin.
Oyun motorunu otomatik tanır, uygun çeviri yöntemini uygular.

</div>

---

## Özellikler

- **Otomatik motor tanıma** — Unity, Unreal, RPG Maker, Ren'Py, GameMaker, Bethesda
- **Tek tıkla çeviri** — Oyunu seç, "Çevir" de, oyna
- **Kütüphane tarama** — Steam, Epic Games, GOG Galaxy otomatik taranır
- **Native Windows UI** — Qt 6 QML ile hızlı, hafif arayüz
- **Açık kaynak** — MIT lisansı, topluluğun katkılarına açık

> **Not:** Proje alpha aşamasındadır. UI çalışır durumda, core entegrasyonu devam etmektedir.

---

## Hızlı Başlangıç

### Gereksinimler

| Araç | Sürüm |
|------|-------|
| Windows | 10/11 (64-bit) |
| Qt | 6.10+ (MinGW 13.1.0 veya MSVC 2022) |
| CMake | 3.25+ |
| vcpkg | Güncel |
| [just](https://github.com/casey/just) | Task runner |

### Build

```bash
git clone https://github.com/jlceaser/MakineAI.git
cd MakineAI

# Bağımlılıkları kur
just setup

# Build & çalıştır
just run
```

Manuel build:

```bash
cmake --preset dev
cmake --build --preset dev
```

### Preset'ler

| Preset | Derleyici | Mod | Açıklama |
|--------|----------|-----|----------|
| `dev` | MinGW | UI-only | Hızlı geliştirme |
| `debug` | MinGW | UI-only | Debug build |
| `release` | MSVC/vcpkg | Full | Core entegrasyon |
| `core` | MSVC/vcpkg | Core only | Kütüphane build |

---

## Desteklenen Oyun Motorları

| Motor | Sürümler | Yöntem |
|-------|----------|--------|
| Unity | Mono, IL2CPP | BepInEx + XUnity |
| Unreal Engine | 4.x, 5.x | PAK lokalizasyon |
| RPG Maker | MV, MZ, VX Ace | JSON/Ruby |
| Ren'Py | 7.x, 8.x | Native tl/ |
| GameMaker | Studio 2 | data.win |
| Bethesda | Creation Engine | STRINGS + BA2 |

---

## Teknoloji

| Katman | Teknoloji |
|--------|----------|
| Core | C++23, Boost, OpenSSL, curl, SQLite, spdlog, libsodium |
| UI | Qt 6.10 QML, Quick Controls |
| Build | CMake 3.25+, vcpkg (18 bağımlılık), Ninja |
| CI | GitHub Actions, CodeQL, clang-tidy, cppcheck |

---

## Proje Yapısı

```
MakineAI/
├── core/             # C++ core library
│   ├── include/      # Public headers (40+ dosya)
│   ├── src/          # Implementation (~26K satır)
│   └── tests/        # Unit, integration, fuzz, benchmark
├── qml/              # Qt QML application
│   ├── src/          # C++ backend (models, services)
│   ├── qml/          # QML UI components (80 dosya)
│   └── resources/    # Icons, fonts, images
├── docs/             # Documentation (32 dosya)
├── recipes/          # Translation templates (YAML)
└── scripts/          # Python utilities & toolchain
```

---

## Dokümantasyon

- [Kullanıcı Kılavuzu](docs/user-guide/)
- [Geliştirici Kılavuzu](docs/developer-guide/)
- [API Referansı](docs/api-reference/)
- [Oyun Motorları](docs/game-engines/)
- [Vizyon](docs/VISION.md)
- [Yol Haritası](docs/ROADMAP.md)

---

## Katkıda Bulunma

Katkıda bulunmak ister misiniz? [CONTRIBUTING.md](CONTRIBUTING.md) dosyasını okuyun.

Yeni başlayanlar için `good first issue` etiketli issue'lara göz atın.

---

## Lisans

MIT License — detaylar için [LICENSE](LICENSE) dosyasına bakın.

*CEDRA Interactive — 2026*
