# GameMaker Destegi

MakineAI GameMaker oyun motoru destegi detaylari.

---

## Genel Bakis

**Destek Durumu:** Tam Destek

**Desteklenen Surumler:**
- GameMaker Studio 2
- GameMaker Studio 1.x (sinirli)

---

## Nasil Calisir

GameMaker oyunlarinda `data.win` dosyasi duzenlenir:

1. STRG (String) chunk parse edilir
2. Metinler cevirilir
3. Yeni data.win olusturulur

### Calisma Prensibi

```
data.win
    |
    v
STRG Chunk Cikart
    |
    v
String Tablosu Parse
    |
    v
Cevirileri Uygula
    |
    v
Yeni data.win Olustur
```

---

## Otomatik Algilama

MakineAI GameMaker oyunlarini su dosyalardan tespit eder:

| Dosya | Aciklama |
|-------|----------|
| `data.win` | Ana veri dosyasi |
| `options.ini` | Oyun ayarlari |
| `runner.exe` | GameMaker runner |

---

## Ceviri Sureci

### data.win Yapisi

```
data.win Chunks:
├── FORM (Header)
├── GEN8 (General Info)
├── OPTN (Options)
├── STRG (Strings) ← Hedef
├── TXTR (Textures)
├── AUDO (Audio)
├── SPRT (Sprites)
├── BGND (Backgrounds)
├── PATH (Paths)
├── SCRP (Scripts)
├── FONT (Fonts)
├── OBJT (Objects)
├── ROOM (Rooms)
└── ...
```

### STRG Chunk

String tablosu formati:
```
[String Count: 4 bytes]
[Offset 1: 4 bytes]
[Offset 2: 4 bytes]
...
[String 1: null-terminated]
[String 2: null-terminated]
...
```

### Ceviri Dosyasi

JSON format:
```json
{
  "strings": [
    {
      "index": 0,
      "original": "Press Start",
      "translated": "Basla'ya Bas"
    },
    {
      "index": 1,
      "original": "Game Over",
      "translated": "Oyun Bitti"
    }
  ]
}
```

---

## Teknik Detaylar

### Encoding

- UTF-8 destegi (GMS2)
- Eski surumlerde ANSI

### Font Degisimi

GameMaker font asset'leri:
- FONT chunk'ta tanimli
- Glyph bitmap iceriyor
- Turkce karakter icin yeni font gerekebilir

### Texture Metinleri

Sprite icindeki metinler:
- TXTR chunk'ta
- Gorsel duzenleme gerekir

---

## Bilinen Sorunlar

### Dinamik Metinler

GML script'te olusturulan metinler:
```gml
var msg = "You have " + string(gold) + " gold";
```
Bu tarz metinler STRG'de olmayabilir.

### Hardcoded Metinler

draw_text() ile cizilen:
- STRG'de bulunur
- Ancak context belirsiz olabilir

### Font Limiti

Turkce karakterler mevcut font'ta yoksa:
- Font asset degistirilmeli
- Veya texture patch gerekli

---

## Ornek Oyunlar

| Oyun | Surum | Durum |
|------|-------|-------|
| Undertale | GMS1 | Calisiyor |
| Deltarune | GMS2 | Calisiyor |
| Hotline Miami | GMS1 | Calisiyor |
| Hyper Light Drifter | GMS1 | Calisiyor |

---

## Araclar

### UndertaleModTool

Acik kaynak data.win editoru:
- String duzenleme
- Kod duzenleme
- Asset export/import

### data.win Parser

MakineAI icinde:
```cpp
class DataWinParser {
    std::vector<std::string> extractStrings(const std::string& path);
    void replaceStrings(const std::string& path, const StringMap& translations);
};
```

---

## Troubleshooting

### data.win Bozuk

1. Orijinal yedek al
2. Dosya boyutu kontrol et
3. Hash dogrulama yap

### Ceviri Gorunmuyor

1. Dogru string index mi kontrol et
2. Encoding UTF-8 mi
3. Oyun cache temizle

### Oyun Cokuyor

1. String uzunluklari siniri asiyor mu
2. data.win yapisi bozulmus mu
3. Font karakterleri mevcut mu

---

## Kod Ornegi

### String Cikarma

```cpp
std::vector<std::string> extractStrings(const std::string& dataPath) {
    auto data = readFile(dataPath);
    auto strgOffset = findChunk(data, "STRG");

    uint32_t count = readU32(data, strgOffset + 4);
    std::vector<std::string> strings;

    for (uint32_t i = 0; i < count; i++) {
        auto strOffset = readU32(data, strgOffset + 8 + i * 4);
        strings.push_back(readNullTermString(data, strOffset));
    }

    return strings;
}
```

---

## Kaynaklar

- [UndertaleModTool](https://github.com/krzys-h/UndertaleModTool)
- [data.win Format](https://pcy.ulyssis.be/undertale/datawin-format.html)
