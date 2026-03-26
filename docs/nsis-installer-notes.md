# NSIS Installer Güvenlik & Paketleme Analizi
## D2_Turkish_1.15.exe — Dishonored 2 Çeviri Paketi

### Neden Bu Kadar Sağlam?

Bu installer birden fazla güvenlik katmanı kullanıyor:

---

## 1. Yapı: İç İçe Arşiv (Matryoshka)

```
D2_Turkish_1.15.exe (264 MB)
├── PE Stub (.text, .rdata, .data, .ndata, .rsrc) — 254 KB
└── Overlay (0x3F200'den itibaren — 251.7 MB)
    ├── NSIS FirstHeader (112 bytes)
    │   ├── Magic: EF BE AD DE + "NullsoftInst"
    │   ├── Compressed header size: 102,618 bytes
    │   └── Archive size: 264,065,528 bytes
    ├── Compressed NSIS Header/Script (~100 KB)
    │   └── BCJ2 + LZMA2 sıkıştırma (standart araçlarla decompress edilemez)
    └── Section Data Blokları (12 parça)
        ├── Büyük bloklar (3.7z=119MB, 5.7z=119MB) → AES-256 şifreli
        └── Küçük bloklar (7/9/11.7z=3.5MB) → Şifresiz (farklı oyun versiyonları)
```

## 2. Güvenlik Katmanları

### Katman 1: BCJ2 + LZMA2 Script Sıkıştırma
- NSIS script'i (kurulum mantığı, parola, dosya yolları) **BCJ2 filtresi + LZMA2** ile sıkıştırılmış
- Standart zlib/lzma/raw deflate decompress edemez
- Tüm EXE'de okunabilir string bulunamadı — script tamamen opak

### Katman 2: AES-256 Şifreli 7z Blokları
- Büyük veri blokları (asıl çeviri dosyaları) 7z AES-256 ile şifrelenmiş
- Header'lar da şifreli (kEncodedHeader DEĞİL, tamamen encrypted next header)
- Parola NSIS script'inde gömülü — script sıkıştırılmış olduğu için erişilemiyor
- Start header CRC doğru, ama next header CRC uyuşmuyor (AES-256 şifreleme)

### Katman 3: Çoklu Versiyon Desteği
- 5 farklı 7z bloğu: 3 şifresiz (farklı oyun versiyonları), 2 şifreli (ana veri)
- NSIS script hangi versiyonu kuracağını runtime'da belirliyor
- Bu hem esneklik sağlıyor hem de reverse engineering'i zorlaştırıyor

### Katman 4: Registry Bağımlılığı
- Installer sessiz modda (`/S /D=...`) bile çalışmıyor
- Muhtemelen oyun yolu doğrulaması yapıyor (Steam registry key kontrolü)
- Oyun kurulu değilse kurulum reddediliyor

### Katman 5: NSIS v3 (2024) Modern Koruma
- NSIS "Nullsoft Install System v18-Mar-2024" — en güncel sürüm
- 7z -tNsis ile açılamıyor (eski 7z versiyonu bu NSIS versiyonunu tanımıyor)
- PE yapısı: 5 section (.text, .rdata, .data, .ndata, .rsrc)

## 3. Teknik Detaylar

### PE Overlay Yapısı
```
Offset      | İçerik
------------|------------------------------------------
0x00000     | PE Header + Sections (NSIS EXE stub)
0x3F200     | NSIS Overlay başlangıcı
0x3F200+4   | FirstHeader (112 bytes) — magic + meta
0x3F274     | Compressed NSIS Header (102,618 bytes)
0x58348+    | Section data blokları (7z arşivleri)
```

### 7z Blok Analizi
| Blok | Boyut | Şifreli? | İçerik |
|------|-------|----------|--------|
| 3.7z | 119 MB | EVET (AES-256) | Ana çeviri verileri? |
| 5.7z | 119 MB | EVET (AES-256) | Ana çeviri verileri? |
| 7.7z | 3.5 MB | HAYIR | game1/2/3.index + lang + font (v1) |
| 9.7z | 3.4 MB | HAYIR | Aynı dosyalar (v1.15 - farklı boyutlar) |
| 11.7z | 3.5 MB | HAYIR | Aynı dosyalar (v1 kopyası) |

### CRC Doğrulama
- **Şifresiz bloklar:** Start header CRC ✅, Next header CRC ✅
- **Şifreli bloklar:** Start header CRC ✅, Next header CRC ❌ (AES-256 opak)

## 4. Makine-Launcher İçin Uyarlanabilir Teknikler

### A) İç İçe Arşiv + Bütünlük Kontrolü
```
Makine-Launcher Paket Formatı (önerilen):
├── Manifest (JSON, imzalı)
├── Metadata bloğu (LZMA2 sıkıştırılmış)
│   ├── Oyun bilgisi
│   ├── Versiyon uyumluluk tablosu
│   └── Dosya hash'leri (SHA-256)
└── Veri blokları (7z solid, AES-256 opsiyonel)
    ├── Blok 1: Ana çeviri dosyaları
    ├── Blok 2: Font dosyaları
    └── Blok N: Versiyon-spesifik yamalar
```

### B) Akıllı Dağıtım
- **Differential updates:** Sadece değişen blokları indir
- **Versiyon matrisi:** Farklı oyun versiyonları için farklı bloklar
- **Registry doğrulama:** Oyunun kurulu olduğunu doğrula

### C) Güvenlik
- **Code signing:** EXE + arşiv bütünlüğü
- **Manifest imzası:** Man-in-the-middle koruması
- **Hash zinciri:** Her blok için SHA-256 doğrulama

### D) Anti-Tampering
- Script/metadata sıkıştırması (BCJ2+LZMA2 benzeri)
- Blok şifreleme (AES-256, anahtar manifest'te)
- CRC + hash çift doğrulama

## 5. Sonuç

Bu NSIS installer'ın sağlamlığı şunlardan geliyor:
1. **Katmanlı güvenlik** — PE → NSIS → BCJ2/LZMA2 → AES-256 7z
2. **Opak script** — kurulum mantığı standart araçlarla okunamıyor
3. **Runtime doğrulama** — oyun yolu, registry check
4. **Modern NSIS** — 2024 versiyonu, eski araçlar tanımıyor

Bu tekniklerin çoğu Makine-Launcher'ın network data downloader'ına uyarlanabilir.

---
*Analiz: 2026-02-18, Claude Code*
*Harcanan: ~15 farklı çıkarma denemesi, PE analizi, CRC doğrulama, binary tarama*
