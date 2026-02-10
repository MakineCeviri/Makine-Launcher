# MakineAI Yol Haritasi

**Son Guncelleme:** 2026-02-10

---

## Genel Durum

```
████████████████░░░░░░░░░░░░  55%  GENEL TAMAMLANMA
```

| Bolum | Tamamlanma | Durum |
|-------|-----------|-------|
| Makine: Oyun Tespit & Tarama | %90 | Steam/Epic/GOG gercek tarama calisiyor |
| Makine: Ceviri Paket Kurulumu | %70 | Yerel paketler kurulabiliyor |
| Makine: Sunucu & Dagitim | %5 | Henuz baslanmadi |
| MakineAI: Guncelleme Tespiti | %0 | Tasarim asamasi |
| MakineAI: Adaptasyon Motoru | %0 | Tasarim asamasi |
| UI & Kullanici Deneyimi | %80 | Alpha kalitesinde, calisir durumda |
| CI/CD & DevOps | %75 | Pipeline calisiyor |

---

## Makine — Ceviri Dagitim Platformu

### Faz 1: Temel Islevsellik ✅ (%85 Tamamlandi)

- [x] Steam kutuphanesi tarama (Registry + VDF + ACF)
- [x] Epic Games tarama (Manifest JSON)
- [x] GOG tarama (Registry)
- [x] Oyun motoru tespiti (dosya imzalari)
- [x] Anti-cheat tespiti (EAC, BattlEye, Vanguard)
- [x] Ceviri paketi kurma/kaldirma (dosya kopyalama)
- [x] Yedekleme ve geri yukleme
- [x] Performans: lazy loading, visibility-aware timers
- [ ] Hata yonetimi iyilestirmeleri (devam ediyor)

### Faz 2: Dagitim Sistemi (%5)

| Gorev | Durum | Oncelik |
|-------|-------|---------|
| Ceviri paketi sunucu formati | Baslanmadi | Kritik |
| Paket indirme mekanizmasi | Baslanmadi | Kritik |
| Versiyon kontrolu ve guncelleme | Baslanmadi | Yuksek |
| Paket imzalama/dogrulama | Baslanmadi | Yuksek |
| CDN veya GitHub Releases entegrasyonu | Baslanmadi | Orta |

### Faz 3: Topluluk (%0)

| Gorev | Durum | Oncelik |
|-------|-------|---------|
| Cevirmen katki sistemi | Baslanmadi | Orta |
| Ceviri kalite puanlama | Baslanmadi | Dusuk |
| Oyun talep sistemi | Baslanmadi | Dusuk |

---

## MakineAI — Adaptasyon Motoru

### Faz A: Guncelleme Tespiti (%0)

> **Gercek Sorun:** Oyun guncellendi → Turkce yama bozuldu

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Dosya hash kaydı | Baslanmadi | Yama kurulurken her dosyanin hash'ini kaydet |
| Degisiklik tespiti | Baslanmadi | Uygulama acildiginda dosya hash'lerini karsilastir |
| Steam versiyon kontrolu | Baslanmadi | Steam API'den oyun versiyonunu al |
| Kullanici bildirimi | Baslanmadi | "Oyun guncellendi, yama kontrol ediliyor" |

### Faz B: Analiz (%0)

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Dosya diff sistemi | Baslanmadi | Eski vs yeni dosya karsilastirmasi |
| String degisiklik haritasi | Baslanmadi | Hangi stringler eklendi/silindi/tasinidi |
| Yapi degisikligi tespiti | Baslanmadi | Dosya formati/yapisi degisti mi |

### Faz C: Otomatik Uyarlama (%0)

| Gorev | Durum | Aciklama |
|-------|-------|----------|
| Degismeyen dosyalari koru | Baslanmadi | Hash eslesen dosyalara dokunma |
| Fuzzy string matching | Baslanmadi | Tasinan stringleri bul ve yeniden esle |
| Yeni string isaretleme | Baslanmadi | Ceviri gerektiren yeni stringleri belirle |
| Akilli merge | Baslanmadi | Catisan dosyalari birlestir |
| Adaptasyon dogrulama | Baslanmadi | Uyarlanan yamanin butunlugunu kontrol et |

---

## Kapali / Ertelenmis Ozellikler

Asagidaki ozellikler mevcut yonle uyumsuz veya onceligi dusuk:

| Ozellik | Durum | Neden |
|---------|-------|-------|
| Font analizi | **Kapatildi** | Pratik degeri dusuk, motor cesitliligi cok fazla |
| Genel string extraction (handler) | **Ertelendi** | Motor bazli araclara birakiliyor (BepInEx, UE4SS vb.) |
| Translation Memory | **Ertelendi** | Adaptasyon motoruna entegre edilecek (Faz C) |
| QA servisi | **Ertelendi** | Topluluk asamasinda degerlendirilecek |
| Gaming Companion AI | **v2.0+** | Oncelik degil, temel sorunlar cozmeli |

---

## Oncelik Sirasi

```
1. Makine Faz 2: Dagitim sistemi (sunucu + indirme)
   ↓
2. MakineAI Faz A: Guncelleme tespiti
   ↓
3. MakineAI Faz B: Analiz
   ↓
4. MakineAI Faz C: Otomatik uyarlama
   ↓
5. Makine Faz 3: Topluluk ozellikleri
```

---

## Risk Matrisi

| Risk | Etki | Olasilik | Onlem |
|------|------|----------|-------|
| Oyun guncelleme formatlari cok cesitli | Yuksek | Yuksek | Motor bazli adaptasyon stratejileri |
| Sunucu maliyeti | Orta | Orta | GitHub Releases / CDN baslangici |
| Cevirmen toplulugu yetersiz | Yuksek | Orta | Once mevcut cevirilerle baslangic |
| Binary dosya formatlari degisken | Yuksek | Yuksek | Metin tabanli dosyalara oncelik ver |

---

## Araclar

### Gelistirme
- Qt 6.10.1 + MinGW 13.1.0
- Visual Studio 2022 (Core icin MSVC)
- CMake 3.25+ + Ninja
- vcpkg (18 bagimlilk)

### DevOps
- GitHub Actions (CI/CD)
- CodeQL (guvenlik analizi)
- clang-format + clang-tidy (kod kalitesi)

---

*Bu dokuman aktif olarak guncellenmektedir.*
*MakineAI — 2026*
