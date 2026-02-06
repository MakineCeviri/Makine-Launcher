# Bethesda (Creation Engine) Destegi

MakineAI Bethesda Creation Engine oyun motoru destegi detaylari.

---

## Genel Bakis

**Destek Durumu:** Tam Destek

**Desteklenen Oyunlar:**
- The Elder Scrolls V: Skyrim (SE/AE)
- Fallout 4
- Starfield

---

## Nasil Calisir

Creation Engine oyunlarinda lokalizasyon sistemi:

1. STRINGS dosyalari (.STRINGS, .ILSTRINGS, .DLSTRINGS)
2. BA2 arsivleri
3. ESP/ESM plugin'leri

### Calisma Prensibi

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

## Otomatik Algilama

MakineAI Bethesda oyunlarini su dosyalardan tespit eder:

| Dosya | Aciklama |
|-------|----------|
| `Data/*.esm` | Master dosyalari |
| `Data/*.ba2` | Arsiv dosyalari |
| `Data/Strings/` | Lokalizasyon |
| `SkyrimSE.exe` | Oyun executable |

---

## Ceviri Sureci

### Klasor Yapisi

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

| Uzanti | Icerik |
|--------|--------|
| .STRINGS | Genel metinler (UTF-8) |
| .ILSTRINGS | IL string (null-terminated) |
| .DLSTRINGS | Dialog string (length-prefixed) |

---

## Teknik Detaylar

### BA2 Arsiv

BA2 formati Bethesda Archive:
- Sikistirilmis dosyalar
- Texture ve general turleri

### ESP/ESM Plugin

Eger metin degisikligi kayit gerekiyorsa:
- Creation Kit ile ESP olustur
- FormID eşlestirmesi

### Load Order

Plugin onceliklendirme:
```
# plugins.txt
*Skyrim.esm
*Update.esm
*TurkishTranslation.esp
```

---

## Skyrim Ozel

### Ozel Gereksinimler

- SKSE (Skyrim Script Extender)
- Skyrim Script Extender ini ayarlari

### Font Degisimi

Turkce karakter icin:
```
Data/Interface/fonts_tr.swf
```

### SkyUI Uyumlulugu

SkyUI ile ceviri uyumu:
- MCM ceviri dosyalari
- Interface ceviri

---

## Fallout 4 Ozel

### Ozel Gereksinimler

- F4SE (Fallout 4 Script Extender)
- Ba2 Tool

### Klasor Yapisi

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

Creation Club icerigi:
- Ayri STRINGS gerekir
- DLC bazli ceviriler

### Voice Acting

Seslendirme dosyalari:
- FUZ/XWM format
- Dubbing ayri islem

### Font Limiti

Vanilla font sinirliliklari:
- Turkce ozel karakterler
- Font replacement gerekli

---

## Ornek Oyunlar

| Oyun | Surum | Durum |
|------|-------|-------|
| Skyrim SE | AE 1.6+ | Calisiyor |
| Skyrim VR | - | Calisiyor |
| Fallout 4 | Latest | Calisiyor |
| Starfield | - | Deneysel |

---

## Araclar

### xEdit (SSEEdit/FO4Edit)

Record duzenleme:
- STRINGS referanslari
- FormID yonetimi

### BA2 Extractor

Arsiv cikarma:
- Bethesda Archive Extractor
- BSA Browser

### Creation Kit

Resmi modlama araci:
- ESP/ESM olusturma
- String kayitlari

---

## Troubleshooting

### STRINGS Yuklenmiyor

1. Dosya isimlendirmesi dogru mu (GameName_language.STRINGS)
2. Data/Strings/ klasorunde mi
3. INI'de sLanguage ayari

### Bos Metinler

1. FormID eslestirmesi dogru mu
2. STRINGS tipi dogru mu (STRINGS vs ILSTRINGS)
3. Encoding dogru mu

### Crash

1. Plugin load order kontrol et
2. STRINGS butunlugu kontrol et
3. Eksik master kontrol et

---

## INI Ayarlari

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
