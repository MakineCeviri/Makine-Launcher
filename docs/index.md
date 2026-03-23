# Makine-Launcher Dokümantasyonu

Türkçe oyun çeviri platformuna hoş geldiniz!

---

## Proje Hakkında

Türk oyuncular için çeviri dağıtım platformu — oyun tespit, paket kur/kaldır, yedekle, güncelle.

Detaylar: [Vizyon](VISION.md) | [Yol Haritası](ROADMAP.md)

---

## Hızlı Başlangıç

- [Başlangıç Kılavuzu](user-guide/getting-started.md) - İlk çevirinizi 2 dakikada yapın
- [Kurulum](user-guide/installation.md) - Sistem gereksinimleri ve kurulum

---

## Geliştirici Dokümantasyonu

- [Geliştirme Ortamı](developer-guide/setup.md) - Kurulum ve araçlar
- [Mimari](developer-guide/architecture.md) - Sistem tasarımı
- [QML Arayüz](developer-guide/qml-frontend.md) - Tema, bileşenler, state yönetimi
- [Build Sistemi](developer-guide/build-system.md) - CMake, vcpkg, preset'ler
- [Test Yazma](developer-guide/testing.md) - GTest, CTest
- [Core Kütüphane](developer-guide/core-library.md) - İleri özellikler (opsiyonel)

---

## API Referansı

- [Services API](api-reference/services-api.md) - Qt servisleri (CoreBridge, GameService, vb.)

---

## Oyun Motoru Referansı

Teknik referans — her motorun dosya yapısı ve çeviri yöntemi:

| Motor | Doküman |
|-------|---------|
| Unity | [unity.md](game-engines/unity.md) |
| Unreal Engine | [unreal.md](game-engines/unreal.md) |
| RPG Maker | [rpgmaker.md](game-engines/rpgmaker.md) |
| Ren'Py | [renpy.md](game-engines/renpy.md) |
| GameMaker | [gamemaker.md](game-engines/gamemaker.md) |
| Bethesda | [bethesda.md](game-engines/bethesda.md) |

> **Not:** Bu dokümanlar motor dosya formatları hakkında teknik referans bilgi içerir.

---

## Diğer Kaynaklar

- [Mimari Kararlar (ADR)](adr/README.md)
- [Güvenlik Modeli](security/security-model.md)
- [CONTRIBUTING.md](../CONTRIBUTING.md)
- [CHANGELOG.md](../CHANGELOG.md)

---

*Makine-Launcher v0.1.0-alpha*
