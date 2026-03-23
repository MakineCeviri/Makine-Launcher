# Makine-Launcher Yol Haritasi

**Son Guncelleme:** 2026-03-12

---

## Genel Durum

```
██████████████████████████░░░░  85%  GENEL TAMAMLANMA
```

| Bolum | Tamamlanma | Durum |
|-------|-----------|-------|
| Oyun Tespit & Tarama | %82 | Steam iyi, Epic/GOG minimal, Xbox/GamePass yok |
| Ceviri Paket Kurulumu | %90 | Yerel + R2 kurulum, yedek koruma, DPI scaling |
| Dagitim Sistemi | %95 | 273/273 CDN'de, AES-256-GCM dogrulama, deploy pipeline |
| UI & Kullanici Deneyimi | %95 | 5 ekran, 36 component, 7 dialog; 5 feature devre disi |
| CI/CD & DevOps | %75 | GitHub Actions release pipeline, lokal build+sign |
| Guvenlik | %80 | AES-256-GCM (paket butunlugu), SSL pinning tamam; pin rotation eksik |

---

## Makine — Ceviri Dagitim Platformu

### Faz 1: Temel Islevsellik (%95 Tamamlandi)

- [x] Steam kutuphanesi tarama (Registry + VDF + ACF)
- [x] Epic Games tarama (Manifest JSON)
- [x] GOG tarama (Registry)
- [x] Oyun motoru tespiti (dosya imzalari)
- [x] Anti-cheat tespiti (EAC, BattlEye, Vanguard)
- [x] Ceviri paketi kurma/kaldirma (overlay, script, options)
- [x] Yedekleme ve geri yukleme
- [x] Variant sistemi (version/platform/game secimi)
- [x] InstallOptionsDialog (checkbox-style kurulum secenekleri)
- [x] PackageCatalog (saf C++ is mantigi)
- [ ] Hata yonetimi iyilestirmeleri (devam ediyor)

### Faz 2: Dagitim Sistemi (%95)

| Gorev | Durum | Oncelik |
|-------|-------|---------|
| Hibrit katalog (index.json + on-demand detail) | **Tamamlandi** | Kritik |
| R2 paket indirme (zstd + AES-256-GCM) | **Tamamlandi** | Kritik |
| ETag cache (index + per-game detail) | **Tamamlandi** | Kritik |
| R2 CDN upload (258/258 .mkpkg) | **Tamamlandi** | Kritik |
| ~~Ed25519 paket imzalama~~ | **Kaldirildi** | — | AES-256-GCM auth tag yeterli |
| Pre-fetch (GameDetailScreen acildiginda) | **Tamamlandi** | Orta |
| Deploy pipeline (tek komut dagitim) | **Tamamlandi** | Yuksek |
| Delta guncelleme (sadece degisen dosyalar) | Baslanmadi | Orta |

### Faz 3: Topluluk (%0)

| Gorev | Durum | Oncelik |
|-------|-------|---------|
| Cevirmen katki sistemi | Baslanmadi | Orta |
| Ceviri kalite puanlama | Baslanmadi | Dusuk |
| Oyun talep sistemi | Baslanmadi | Dusuk |

---

## Alpha Release Engelleri (1 kaldi)

| Engel | Durum | Aciklama |
|-------|-------|----------|
| ~~CDN paketleri~~ | ✅ Tamamlandi | 258/258 .mkpkg CDN'de |
| ~~Paket guvenlik~~ | ✅ Tamamlandi | AES-256-GCM auth tag + SHA-256 checksum |
| ~~Deploy pipeline~~ | ✅ Tamamlandi | deploy.py + R2 upload |
| ~~SSL pinning~~ | ✅ Tamamlandi | 4 cert pin, placeholder yok |
| ~~Code signing~~ | ✅ Kismen | Self-signed cert + just sign (OV/EV cert satinalma karari kaldi) |
| **Static Qt build** | Bekliyor | Tek seferlik ~1-2 saat, sonraki build'ler 1-2 dk |
| **MSIX paketleme** | Bekliyor | Microsoft Store submission icin |

---

## Kapatilan / Ertelenmis Ozellikler

| Ozellik | Durum | Neden |
|---------|-------|-------|
| Font analizi | **Kapatildi** | Pratik degeri dusuk |
| Engine Handler'lar | **Kaldirildi** | Stub interface'ler korundu (IEngineHandler) |
| Translation Pipeline | **Kaldirildi** | Stub header'lar silindi (2026-02-25) |
| Translation Memory | **Ertelendi** | Ayri proje (MakineAI) |
| Glossary Service | **Ertelendi** | Stub header silindi (2026-02-25) |
| QA Service | **Ertelendi** | Stub header silindi (2026-02-25) |
| Gaming Companion AI | **v2.0+** | Oncelik degil |

---

## Oncelik Sirasi

```
1. Alpha Release Hazirligi (Static Qt build)
   |
2. GitHub Releases pre-alpha yayini
   |
3. Delta guncelleme + Xbox/GamePass scanner
   |
4. Topluluk ozellikleri
```

---

## Araclar

### Gelistirme
- Qt 6.10.1 + MinGW 13.1.0
- Visual Studio 2022 (Core icin MSVC)
- CMake 3.28+ + Ninja
- vcpkg (19 bagimllik)

### DevOps
- GitHub Actions (CI/CD)
- CodeQL (guvenlik analizi)
- Cloudflare R2 (CDN)
- Sentry (crash reporting)

---

## Son Degisiklikler

### 2026-03-12: Buyuk refactoring + guvenlik sadeletirme
- Ed25519 imza sistemi tamamen kaldirildi — AES-256-GCM auth tag yeterli
- sign_packages.py silindi (543 satir), R2'den 260 .sig dosyasi temizlendi
- Oyun tasima yedek korumasi (4 katmanli: backup skip, path koruma, path migration, scan guncelleme)
- DPI-aware UI scaling (Kompakt/Otomatik/Buyuk secenekleri)
- Kategorize logging (186 qDebug → qCDebug, 15 kategori)
- fmt::format migration tamamlandi
- BepInEx/XUnity tamamen kaldirildi (1132→18 satir)
- Release pipeline + code signing altyapisi (GitHub Actions)
- CI/CD sunucu altyapisi kaldirildi (gereksiz bulundu — solo developer)
- Exception safety iyilestirmeleri (saveCachedGames, restore target validation)

### 2026-03-02: Guvenlik denetimi
- Sentry DSN env variable'a tasindi
- PII stripping eklendi (Windows kullanici adi redaction)

### 2026-03-01: R2 CDN custom domain + dagitim tamamlandi
- cdn.makineceviri.org aktif
- 258/258 .mkpkg paketi R2'ye yuklendi
- Code signing altyapisi kuruldu (self-signed + signtool)

---

*Makine-Launcher — 2026*
