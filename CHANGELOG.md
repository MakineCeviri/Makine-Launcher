# Changelog

Tüm önemli değişiklikler bu dosyada belgelenecektir.

Format [Keep a Changelog](https://keepachangelog.com/tr/1.0.0/) standardına,
sürümleme [Semantic Versioning](https://semver.org/lang/tr/) standardına uygundur.

## [Unreleased]

_Sonraki sürüm için değişiklikler burada listelenecek._

---

## [0.1.0-alpha] - 2026-02-03

### Eklenen
- **Native Qt6/QML UI** - Flutter'dan geçiş tamamlandı
- **F12 Screenshot** - Oyun anı yakalama (Gaming Companion AI altyapısı)
- **VISION.md** - Uygulamanın nihai hedefi belgelendi
- **ROADMAP.md** - Geliştirme yol haritası
- **DevOps Automation** - CI/CD, issue templates, code quality
- **C++ Core Library** - Tam entegre çeviri motoru
- **Oyun Motorları Desteği:**
  - Unity (IL2CPP + Mono)
  - Unreal Engine 4/5
  - RPG Maker MV/MZ
  - Ren'Py
  - GameMaker Studio 2
- **Oyun Tarayıcıları:**
  - Steam
  - Epic Games
  - GOG Galaxy
  - Manuel ekleme
- **Translation Pipeline** - Akıllı çeviri karar motoru
- **GPU Optimizasyonları** - Görünmeyen animasyonlar durduruldu
- **Performance Monitor** - F3 ile FPS overlay
- **30+ UI Bileşeni** - Native Qt tasarım sistemi
- **vcpkg Entegrasyonu** - 22+ C++ bağımlılık
- **CMake Presets** - Modern build konfigürasyonu

### Değiştirilen
- Mimari: Flutter + FFI → Qt6/QML + Native C++
- Build sistemi: Manuel → CMake Presets + just
- UI framework: FluentUI → Native Qt Components

### Kaldırılan
- Flutter codebase (archived)
- FFI bridge layer
- FluentUI dependency

### Güvenlik
- Path traversal koruması eklendi
- Input validation güçlendirildi

---

## [0.0.8] - 2026-01-15 [Archived]

### Not
Bu sürüm Flutter-based eski mimariye aittir ve artık desteklenmemektedir.
Yeni sürümler için v0.1.0+ kullanın.

---

## Sürüm Karşılaştırması

| Özellik | v0.0.x (Flutter) | v0.1.x (Qt Native) |
|---------|------------------|---------------------|
| UI Framework | Flutter | Qt6/QML |
| Core Binding | FFI | Direct C++ |
| Build Size | ~50MB | ~15MB |
| Startup Time | ~3s | <1s |
| Memory Usage | ~300MB | ~150MB |
| Windows Support | ✅ | ✅ |
| Linux Support | ❌ | Planned |
| macOS Support | ❌ | Planned |

---

[Unreleased]: https://github.com/jlceaser/MakineAI/compare/v0.1.0-alpha...HEAD
[0.1.0-alpha]: https://github.com/jlceaser/MakineAI/releases/tag/v0.1.0-alpha
[0.0.8]: https://github.com/jlceaser/MakineAI/releases/tag/v0.0.8
