# MakineAI

Windows oyunlarını Türkçe'ye çevirmek için geliştirilmiş masaüstü uygulaması.

**Sürüm:** 0.1.0-alpha
**Platform:** Windows 10/11 (64-bit)
**Teknoloji:** Qt 6 + C++20

---

## Geliştirme Ortamı

### Gereksinimler

- Visual Studio 2022
- Qt 6.8+
- CMake 3.25+
- vcpkg

### Build

```bash
# Ortam değişkenleri
set VCPKG_ROOT=C:\vcpkg
set Qt6_DIR=C:\Qt\6.8.1\msvc2022_64

# Build
cmake --preset dev
cmake --build build/dev

# Çalıştır
build/dev/MakineAI.exe
```

---

## Proje Yapısı

```
MakineAI/
├── core/           # C++ core library
│   ├── include/    # Public headers
│   ├── src/        # Implementation
│   └── tests/      # Unit tests
├── qml/            # Qt QML application
│   ├── src/        # C++ backend
│   ├── qml/        # QML UI
│   └── resources/  # Assets
├── docs/           # Documentation
├── recipes/        # Translation templates
└── scripts/        # Build scripts
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
