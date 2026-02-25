# MakineAI Yol Haritasi

**Son Guncelleme:** 2026-02-25

---

## Genel Durum

```
██████████████████████░░░░░░  75%  GENEL TAMAMLANMA
```

| Bolum | Tamamlanma | Durum |
|-------|-----------|-------|
| Makine: Oyun Tespit & Tarama | %95 | Steam/Epic/GOG tarama + anti-cheat + motor tespiti |
| Makine: Ceviri Paket Kurulumu | %85 | Yerel + R2 paketler kurulabiliyor, variant destegi |
| Makine: Dagitim Sistemi | %60 | Hibrit katalog (index+detail), R2 indirme, ETag cache |
| MakineAI: Guncelleme Tespiti | %10 | UpdateDetection + FileIntegrity modulleri mevcut |
| MakineAI: Adaptasyon Motoru | %5 | Memory Translation Extractor tasarlandi |
| UI & Kullanici Deneyimi | %85 | Alpha kalitesinde, component konsolidasyonu tamamlandi |
| CI/CD & DevOps | %75 | GitHub Actions pipeline calisiyor |

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

### Faz 2: Dagitim Sistemi (%60)

| Gorev | Durum | Oncelik |
|-------|-------|---------|
| Hibrit katalog (index.json + on-demand detail) | **Tamamlandi** | Kritik |
| R2 paket indirme (zstd + AES-256-GCM) | **Tamamlandi** | Kritik |
| ETag cache (index + per-game detail) | **Tamamlandi** | Yuksek |
| MakineAI-Assets repo (index + packages + images) | **Tamamlandi** | Kritik |
| Pre-fetch (GameDetailScreen acildiginda) | **Tamamlandi** | Orta |
| Paket imzalama/dogrulama | Baslanmadi | Yuksek |
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
1. Alpha Release Hazirligi (UI kalite, component temizligi)
   |
2. MakineAI Faz A: Guncelleme tespiti (hash + versiyon)
   |
3. MakineAI Faz B: Analiz (Memory Extractor + diff)
   |
4. MakineAI Faz C: Otomatik uyarlama
   |
5. Makine Faz 3: Topluluk ozellikleri
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

---

## Son Degisiklikler

### 2026-02-25: Alpha hazirlik — buyuk temizlik
- Hibrit katalog sistemi (index.json + on-demand detail) tamamlandi
- fwd.hpp %51 kucultuldu (373 -> 176 satir, 48 kullanilmayan forward declaration silindi)
- 4 bos stub header silindi (glossary_service, translation_memory, qa_service, translation_pipeline)
- scanner_base.hpp silindi (kullanilmayan v2 scanner interface)
- Settings component'leri konsolide edildi (6 dosyadan ~503 satir tekrar kaldirdildi)
- 3 shared component guncellendi (SettingsCard, ToggleSetting, DisabledSetting)
- Integration test'ler devre disi birakildi (handler impl bekleniyor)
- MakineAI-Assets'ten eski manifest.json silindi (245 KB tasarruf)

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
