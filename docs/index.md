# MakineAI Dokumantasyonu

Turkce oyun ceviri platformuna hos geldiniz!

---

## Proje Hakkinda

MakineAI iki parcadan olusur:

- **Makine** — Ceviri dagitim platformu (oyun tespit, paket kur/kaldir, yedekle)
- **MakineAI** — Adaptasyon motoru (oyun guncelleme tespiti, otomatik ceviri uyarlama)

Detaylar: [Vizyon](VISION.md) | [Yol Haritasi](ROADMAP.md)

---

## Hizli Baslangic

- [Baslangic Kilavuzu](user-guide/getting-started.md) - Ilk cevirinizi 2 dakikada yapin
- [Kurulum](user-guide/installation.md) - Sistem gereksinimleri ve kurulum

---

## Kullanici Kilavuzu

- [Desteklenen Oyunlar](user-guide/supported-games.md) - Motor bazli ceviri destegi
- [Diger Oyunlar](user-guide/other-games.md) - Topluluk ceviri paketleri
- [Sorun Giderme](user-guide/troubleshooting.md) - Sik karsilasilan sorunlar
- [SSS](user-guide/faq.md) - Sik sorulan sorular

---

## Gelistirici Dokumantasyonu

- [Gelistirme Ortami](developer-guide/setup.md) - Kurulum ve araclar
- [Mimari](developer-guide/architecture.md) - Sistem tasarimi (Makine + MakineAI)
- [QML Arayuz](developer-guide/qml-frontend.md) - Tema, bilesenler, state yonetimi
- [Build Sistemi](developer-guide/build-system.md) - CMake, vcpkg, preset'ler
- [Test Yazma](developer-guide/testing.md) - GTest, CTest
- [Core Kutuphane](developer-guide/core-library.md) - Ileri ozellikler (opsiyonel)

---

## API Referansi

- [Core API](api-reference/core-api.md) - C++ Core kutuphanesi
- [Services API](api-reference/services-api.md) - Qt servisleri
- [QML Types](api-reference/qml-types.md) - QML bilesenleri

---

## Oyun Motoru Referansi

Teknik referans — her motorun dosya yapisi ve ceviri yontemi:

| Motor | Dokuman |
|-------|---------|
| Unity | [unity.md](game-engines/unity.md) |
| Unreal Engine | [unreal.md](game-engines/unreal.md) |
| RPG Maker | [rpgmaker.md](game-engines/rpgmaker.md) |
| Ren'Py | [renpy.md](game-engines/renpy.md) |
| GameMaker | [gamemaker.md](game-engines/gamemaker.md) |
| Bethesda | [bethesda.md](game-engines/bethesda.md) |

> **Not:** Bu dokulmanlar motor dosya formatlari hakkinda referans bilgi icerir.
> Adaptasyon motoru bu bilgileri kullanarak guncelleme sonrasi uyarlama yapacaktir.

---

## Diger Kaynaklar

- [Mimari Kararlar (ADR)](adr/README.md)
- [Guvenlik Modeli](security/security-model.md)
- [CONTRIBUTING.md](../CONTRIBUTING.md)
- [CHANGELOG.md](../CHANGELOG.md)

---

*MakineAI v0.1.0-alpha*
