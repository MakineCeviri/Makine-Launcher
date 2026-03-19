# Bethesda (Creation Engine) Desteği

Bethesda Creation Engine dosya yapısı ve çeviri yöntemi teknik referansı.

> **Not:** Bu doküman teknik referanstır. Makine-Launcher şu an motor bazlı işlem yapmamaktadır — sadece motor tespiti yapar. Bu bilgiler adaptasyon motoru tasarımında kullanılacaktır.

---

## Genel Bakış

**Destek Durumu:** Tam Destek

**Desteklenen Oyunlar:**
- The Elder Scrolls V: Skyrim (SE/AE)
- Fallout 4
- Starfield

---

## Nasıl Çalışır

Creation Engine oyunlarında lokalizasyon sistemi:

1. STRINGS dosyaları (.STRINGS, .ILSTRINGS, .DLSTRINGS)
2. BA2 arşivleri
3. ESP/ESM plugin'leri

### Çalışma Prensibi

```
Ceviri Paketi
    |
    v
STRINGS Dosyalari Olustur
    |
    v
BA2 Arsivine Paketle
    |
    v
Data/ Klasorune Yerlestir
    |
    v
Load Order ile Yukle
```

---

## Otomatik Algılama

Makine-Launcher Bethesda oyunlarını şu dosyalardan tespit eder:

| Dosya | Açıklama |
|-------|----------|
| `Data/*.esm` | Master dosyaları |
| `Data/*.ba2` | Arşiv dosyaları |
| `Data/Strings/` | Lokalizasyon |
| `SkyrimSE.exe` | Oyun executable |

---

## Çeviri Süreci

### Klasör Yapısı

```
Data/
├── Skyrim.esm
├── Skyrim - Patch.bsa
├── Strings/
│   ├── Skyrim_turkish.STRINGS      # Ana metinler
│   ├── Skyrim_turkish.ILSTRINGS    # IL metinler
│   └── Skyrim_turkish.DLSTRINGS    # Dialog metinler
└── TurkishTranslation.esp          # Plugin (opsiyonel)
```

### STRINGS Format

Binary format:
```
[Record Count: 4 bytes]
[Data Size: 4 bytes]
[ID1: 4 bytes][Offset1: 4 bytes]
[ID2: 4 bytes][Offset2: 4 bytes]
...
[String Data]
```

### Farklar

| Uzantı | İçerik |
|--------|--------|
| .STRINGS | Genel metinler (UTF-8) |
| .ILSTRINGS | IL string (null-terminated) |
| .DLSTRINGS | Dialog string (length-prefixed) |

---

## Teknik Detaylar

### BA2 Arşiv

BA2 formatı Bethesda Archive:
- Sıkıştırılmış dosyalar
- Texture ve general türleri

### ESP/ESM Plugin

Eğer metin değişikliği kayıt gerekiyorsa:
- Creation Kit ile ESP oluştur
- FormID eşleştirmesi

### Load Order

Plugin önceliklendirme:
```
# plugins.txt
*Skyrim.esm
*Update.esm
*TurkishTranslation.esp
```

---

## Skyrim Özel

### Özel Gereksinimler

- SKSE (Skyrim Script Extender)
- Skyrim Script Extender ini ayarları

### Font Değişimi

Türkçe karakter için:
```
Data/Interface/fonts_tr.swf
```

### SkyUI Uyumluluğu

SkyUI ile çeviri uyumu:
- MCM çeviri dosyaları
- Interface çeviri

---

## Fallout 4 Özel

### Özel Gereksinimler

- F4SE (Fallout 4 Script Extender)
- Ba2 Tool

### Klasör Yapısı

```
Data/
├── Fallout4.esm
├── Fallout4 - Interface.ba2
└── Strings/
    ├── Fallout4_tr.STRINGS
    ├── Fallout4_tr.ILSTRINGS
    └── Fallout4_tr.DLSTRINGS
```

---

## Bilinen Sorunlar

### CC Content

Creation Club içeriği:
- Ayrı STRINGS gerekir
- DLC bazlı çeviriler

### Voice Acting

Seslendirme dosyaları:
- FUZ/XWM format
- Dubbing ayrı işlem

### Font Limiti

Vanilla font sınırlılıkları:
- Türkçe özel karakterler
- Font replacement gerekli

---

## Örnek Oyunlar

| Oyun | Sürüm | Durum |
|------|-------|-------|
| Skyrim SE | AE 1.6+ | Çalışıyor |
| Skyrim VR | - | Çalışıyor |
| Fallout 4 | Latest | Çalışıyor |
| Starfield | - | Deneysel |

---

## Araçlar

### xEdit (SSEEdit/FO4Edit)

Record düzenleme:
- STRINGS referansları
- FormID yönetimi

### BA2 Extractor

Arşiv çıkarma:
- Bethesda Archive Extractor
- BSA Browser

### Creation Kit

Resmi modlama aracı:
- ESP/ESM oluşturma
- String kayıtları

---

## Troubleshooting

### STRINGS Yüklenmiyor

1. Dosya isimlendirmesi doğru mu (GameName_language.STRINGS)
2. Data/Strings/ klasöründe mi
3. INI'de sLanguage ayarı

### Boş Metinler

1. FormID eşleştirmesi doğru mu
2. STRINGS tipi doğru mu (STRINGS vs ILSTRINGS)
3. Encoding doğru mu

### Crash

1. Plugin load order kontrol et
2. STRINGS bütünlüğü kontrol et
3. Eksik master kontrol et

---

## INI Ayarları

### Skyrim.ini / SkyrimPrefs.ini

```ini
[General]
sLanguage=TURKISH

[Archive]
sResourceArchiveList2=Skyrim - Patch.bsa, TurkishPatch.ba2
```

### Fallout4.ini

```ini
[General]
sLanguage=tr

[Archive]
sResourceArchive2List=... TurkishPatch.ba2
```

---

## Kaynaklar

- [Creation Kit Wiki](https://ck.uesp.net/)
- [xEdit](https://github.com/TES5Edit/TES5Edit)
- [UESP STRINGS Format](https://en.uesp.net/wiki/Skyrim_Mod:String_Table_File_Format)
