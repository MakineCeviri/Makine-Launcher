# MakineAI Yol Haritasi

**Son Guncelleme:** 2026-03-03

---

## Genel Durum

```
██████████████████████████░░░░  85%  GENEL TAMAMLANMA
```

| Bolum | Tamamlanma | Durum |
|-------|-----------|-------|
| Makine: Oyun Tespit & Tarama | %95 | Steam/Epic/GOG tarama + anti-cheat + motor tespiti |
| Makine: Ceviri Paket Kurulumu | %90 | Yerel + R2 paketler kurulabiliyor, variant destegi, imzalama |
| Makine: Dagitim Sistemi | %95 | 258/258 paket CDN'de, Ed25519 imza, ETag cache |
| MakineAI: Guncelleme Tespiti | %10 | UpdateDetection + FileIntegrity modulleri mevcut |
| MakineAI: Adaptasyon Motoru | %5 | Memory Translation Extractor tasarlandi |
| UI & Kullanici Deneyimi | %96 | 6 ekran, 32 component, 7 dialog tamamlandi |
| CI/CD & DevOps | %90 | Deploy pipeline, imzalama, R2 upload tamamlandi |
| Guvenlik | %85 | Ed25519, AES-256-GCM, SSL pinning, code signing altyapisi |

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
| Ed25519 paket imzalama/dogrulama | **Tamamlandi** | Yuksek |
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

## MakineAI — Adaptasyon Motoru

### Faz A: Guncelleme Tespiti (%10)

> **Gercek Sorun:** Oyun guncellendi -> Turkce yama bozuldu

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Dosya hash kaydi | **Modul hazir** | FileIntegrity modulu mevcut |
| Degisiklik tespiti | **Modul hazir** | UpdateDetection modulu mevcut |
| Steam versiyon kontrolu | Baslanmadi | Steam API/VDF'den version al |
| Kullanici bildirimi | Baslanmadi | "Oyun guncellendi" uyarisi |

### Faz B: Analiz (%5)

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Memory Translation Extractor | **Tasarlandi** | Process memory'den string cikarma |
| Dosya diff sistemi | Baslanmadi | Eski vs yeni dosya karsilastirmasi |
| String degisiklik haritasi | Baslanmadi | Hangi stringler eklendi/silindi/tasindi |

### Faz C: Otomatik Uyarlama (%0)

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Degismeyen dosyalari koru | Baslanmadi | Hash eslesen dosyalara dokunma |
| Fuzzy string matching | Baslanmadi | Tasinan stringleri bul ve yeniden esle |
| Akilli merge | Baslanmadi | Catisan dosyalari birlestir |
| Adaptasyon dogrulama | Baslanmadi | Uyarlanan yamanin butunlugunu kontrol et |

---

## Alpha Release Engelleri (2 kaldi)

| Engel | Durum | Aciklama |
|-------|-------|----------|
| ~~CDN paketleri~~ | ✅ Tamamlandi | 258/258 .mkpkg + .sig |
| ~~Guvenlik anahtari~~ | ✅ Tamamlandi | Ed25519 public key embedded |
| ~~Deploy pipeline~~ | ✅ Tamamlandi | deploy.py + sign_packages.py |
| ~~SSL pinning~~ | ✅ Tamamlandi | 4 cert pin, placeholder yok |
| **Static Qt build** | Bekliyor | Tek seferlik ~1-2 saat, sonraki build'ler 1-2 dk |
| **MSIX paketleme** | Bekliyor | Microsoft Store submission icin |

---

## Kapatilan / Ertelenmis Ozellikler

| Ozellik | Durum | Neden |
|---------|-------|-------|
| Font analizi | **Kapatildi** | Pratik degeri dusuk |
| Engine Handler'lar | **Kaldirildi** | Stub interface'ler korundu (IEngineHandler) |
| Translation Pipeline | **Kaldirildi** | Stub header'lar silindi (2026-02-25) |
| Translation Memory | **Ertelendi** | Adaptasyon motoruna entegre edilecek |
| Glossary Service | **Ertelendi** | Stub header silindi (2026-02-25) |
| QA Service | **Ertelendi** | Stub header silindi (2026-02-25) |
| Gaming Companion AI | **v2.0+** | Oncelik degil |

---

## Oncelik Sirasi

```
1. Alpha Release Hazirligi (Static Qt build + MSIX)
   |
2. Microsoft Store Submission
   |
3. MakineAI Faz A: Guncelleme tespiti (hash + versiyon)
   |
4. MakineAI Faz B: Analiz (Memory Extractor + diff)
   |
5. MakineAI Faz C: Otomatik uyarlama
   |
6. Makine Faz 3: Topluluk ozellikleri
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

### 2026-03-02: Guvenlik denetimi + imza duzeltmesi
- Ed25519 imza uyumsuzlugu duzeltildi (sign_packages.py hash string fix)
- 258 paket R2'de yeniden imzalandi (--force)
- Sentry DSN env variable'a tasindi
- PII stripping eklendi (Windows kullanici adi redaction)
- CRYPTO_memcmp (constant-time hash karsilastirma)

### 2026-03-01: R2 CDN custom domain + dagitim tamamlandi
- cdn.makineceviri.net aktif (Worker route cakismasi cozuldu)
- 258/258 .mkpkg paketi R2'ye yuklendi
- Tum dataUrl'ler cdn.makineceviri.net'e guncellendi
- Code signing altyapisi kuruldu (self-signed + signtool)

### 2026-02-25: Alpha hazirlik — buyuk temizlik
- Hibrit katalog sistemi (index.json + on-demand detail) tamamlandi
- fwd.hpp %51 kucultuldu (373 -> 176 satir)
- 4 bos stub header silindi
- Settings component'leri konsolide edildi
- Integration test'ler devre disi birakildi

### 2026-02-18: Kod kalitesi & performans
- Guvenlik denetimi (14 bulgu duzeltildi)
- Modurerlik iyilestirmeleri (GameService decoupling)
- Qt Quick performans optimizasyonlari

### 2026-02-14: Dead code temizligi
- Dead Q_INVOKABLE/Q_PROPERTY/signal'lar kaldirildi
- Dimensions.qml 76 dead property kaldirildi

### 2026-02-12: Buyuk temizlik
- ~32K satir dead code kaldirildi
- 13 olu QML bileseni silindi
- ADR-0006 olusturuldu (adaptasyon motoru yon degisikligi)

---

*MakineAI — 2026*
