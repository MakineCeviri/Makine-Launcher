<div align="center">

# MakineAI

**Windows oyunlarini Turkce'ye ceviren ve cevirileri guncel tutan masaustu uygulamasi**

[![Version](https://img.shields.io/badge/version-0.1.0--alpha-blue?style=flat-square)](https://github.com/jlceaser/MakineAI/releases)
[![License](https://img.shields.io/badge/license-Proprietary-red?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/github/actions/workflow/status/jlceaser/MakineAI/ci.yml?branch=main&style=flat-square&label=CI)](https://github.com/jlceaser/MakineAI/actions)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?style=flat-square&logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![Qt6](https://img.shields.io/badge/Qt-6.10-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)
[![Windows](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D6?style=flat-square&logo=windows)](https://github.com/jlceaser/MakineAI)

Steam, Epic, GOG kutuphanenizdeki oyunlari tek tikla Turkce'ye cevirin.
Oyun guncellendiginde cevirileri otomatik uyarlasin.

</div>

---

## Ne Yapar?

**Makine** — Turkce ceviri kutuphanesi ve dagitim platformu
- Kurulu oyunlari otomatik tespit eder (Steam, Epic, GOG)
- Topluluk cevirilerini tek tikla kurar
- Yedekleme ile guvenli kurulum/kaldirma

**MakineAI** — Akilli adaptasyon motoru
- Oyun guncellemelerini tespit eder
- Kirilmis cevirileri otomatik uyarlar
- Cevirmen mudahalesi olmadan yamalari guncel tutar

---

## Ozellikler

- **Otomatik oyun tespiti** — Steam, Epic Games, GOG kutuphaneleri taranir
- **Motor tanima** — Unity, Unreal, RPG Maker, Ren'Py, GameMaker, Bethesda, Godot, Source
- **Tek tikla kurulum** — Ceviri paketini sec, kur, oyna
- **Guncelleme korumasi** — Oyun guncellense bile ceviri calismaya devam eder *(gelistiriliyor)*
- **Guvenli yedekleme** — Kurulumdan once otomatik yedek, tek tikla geri yukleme
- **Native Windows UI** — Qt 6 QML ile hizli, hafif arayuz

> **Not:** Proje alpha asamasindadir. Oyun tarama ve yerel paket kurulumu calisiyor.
> Sunucu dagitimi ve adaptasyon motoru gelistirme asamasindadir.

---

## Hizli Baslangic

### Gereksinimler

| Arac | Surum |
|------|-------|
| Windows | 10/11 (64-bit) |
| Qt | 6.10+ (MinGW 13.1.0 veya MSVC 2022) |
| CMake | 3.25+ |
| vcpkg | Guncel |
| [just](https://github.com/casey/just) | Task runner (istege bagli) |

### Build

```bash
git clone https://github.com/jlceaser/MakineAI.git
cd MakineAI

# Build & calistir
just run

# veya manuel
cmake --preset dev
cmake --build --preset dev
```

### Preset'ler

| Preset | Derleyici | Mod | Aciklama |
|--------|----------|-----|----------|
| `dev` | MinGW | UI + gercek tarama | Hizli gelistirme |
| `debug` | MinGW | UI + gercek tarama | Debug build |
| `release` | MSVC/vcpkg | Full | Core entegrasyon |

---

## Proje Yapisi

```
MakineAI/
├── qml/              # Ana uygulama
│   ├── src/          # C++ backend (services, models)
│   ├── qml/          # QML UI (80+ dosya)
│   └── resources/    # Icons, fonts, images
├── core/             # C++ core library (ileri ozellikler)
├── docs/             # Dokumantasyon
├── recipes/          # Ceviri sablonlari (YAML)
└── scripts/          # Yardimci araclar
```

---

## Teknoloji

| Katman | Teknoloji |
|--------|----------|
| UI | Qt 6.10 QML, Quick Controls |
| Backend | C++23, pure Qt (MinGW) |
| Core (ileri) | C++23, Boost, OpenSSL, curl, vcpkg |
| Build | CMake 3.25+, Ninja, vcpkg |
| CI | GitHub Actions, CodeQL |

---

## Dokumantasyon

- [Vizyon](docs/VISION.md) — Projenin ruhu ve yonu
- [Yol Haritasi](docs/ROADMAP.md) — Mevcut durum ve hedefler
- [Kullanici Kilavuzu](docs/user-guide/) — Baslangic rehberi
- [Gelistirici Kilavuzu](docs/developer-guide/) — Mimari ve build
- [API Referansi](docs/api-reference/) — Teknik detaylar

---

## Lisans

Proprietary License — MakineAI. Tum haklari saklidir.

Detaylar icin [LICENSE](LICENSE) dosyasina bakin.

*MakineAI — 2026*
