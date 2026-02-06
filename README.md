# MakineAI

Windows oyunlarını Türkçe'ye çevirmek için geliştirilmiş masaüstü uygulaması.

**Sürüm:** 0.1.0-alpha
**Platform:** Windows 10/11 (64-bit)
**Teknoloji:** Qt 6 + C++20

---

## Geliştirme Ortamı

### Gereksinimler

- Qt 6.10+ (MinGW 13.1.0 veya MSVC 2022)
- CMake 3.25+
- vcpkg
- [just](https://github.com/casey/just) task runner

### Build

```bash
# Bağımlılıkları kur
just setup

# Tümünü build et (release)
just all

# Çalıştır (debug)
just run

# Sadece core / sadece QML
just core
just qml

# Test
just test
```

Manuel build (cmake):
```bash
cmake --preset dev
cmake --build --preset dev
```

---

## Proje Yapısı

```
MakineAI/
├── core/             # C++ core library
│   ├── include/      # Public headers
│   ├── src/          # Implementation
│   └── tests/        # Unit & integration tests
├── qml/              # Qt QML application
│   ├── src/          # C++ backend (models, services)
│   ├── qml/          # QML UI components & screens
│   └── resources/    # Icons, fonts, images
├── docs/             # Documentation (ADR, API, guides)
├── recipes/          # Translation templates (YAML)
├── scripts/          # Python utilities & toolchain
└── vcpkg_installed/  # C++ dependency cache (gitignored)
```

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

## Teknolojiler

| Katman | Teknoloji |
|--------|-----------|
| Core | C++20, Boost, OpenSSL, SQLite, spdlog |
| UI | Qt 6 QML, Quick Controls |
| Build | CMake, vcpkg, Ninja |

---

## Dökümantasyon

Detaylı dökümantasyon için [docs/](docs/) klasörüne bakın:

- [Kullanıcı Kılavuzu](docs/user-guide/)
- [Geliştirici Kılavuzu](docs/developer-guide/)
- [API Referansı](docs/api-reference/)
- [Oyun Motorları](docs/game-engines/)

---

## Lisans

MIT License - Detaylar için [LICENSE](LICENSE) dosyasına bakın.

---

*CEDRA Interactive - 2026*
