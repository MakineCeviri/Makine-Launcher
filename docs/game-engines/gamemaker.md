# GameMaker Desteği

GameMaker dosya yapısı ve çeviri yöntemi teknik referansı.

> **Not:** Bu doküman teknik referanstır. Makine-Launcher şu an motor bazlı işlem yapmamaktadır — sadece motor tespiti yapar. Bu bilgiler adaptasyon motoru tasarımında kullanılacaktır.

---

## Genel Bakış

**Destek Durumu:** Tam Destek

**Desteklenen Sürümler:**
- GameMaker Studio 2
- GameMaker Studio 1.x (sınırlı)

---

## Nasıl Çalışır

GameMaker oyunlarında `data.win` dosyası düzenlenir:

1. STRG (String) chunk parse edilir
2. Metinler çevrilir
3. Yeni data.win oluşturulur

### Çalışma Prensibi

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

## Otomatik Algılama

Makine-Launcher GameMaker oyunlarını şu dosyalardan tespit eder:

| Dosya | Açıklama |
|-------|----------|
| `data.win` | Ana veri dosyası |
| `options.ini` | Oyun ayarları |
| `runner.exe` | GameMaker runner |

---

## Çeviri Süreci

### data.win Yapısı

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

String tablosu formatı:
```
[String Count: 4 bytes]
[Offset 1: 4 bytes]
[Offset 2: 4 bytes]
...
[String 1: null-terminated]
[String 2: null-terminated]
...
```

### Çeviri Dosyası

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

- UTF-8 desteği (GMS2)
- Eski sürümlerde ANSI

### Font Değişimi

GameMaker font asset'leri:
- FONT chunk'ta tanımlı
- Glyph bitmap içeriyor
- Türkçe karakter için yeni font gerekebilir

### Texture Metinleri

Sprite içindeki metinler:
- TXTR chunk'ta
- Görsel düzenleme gerekir

---

## Bilinen Sorunlar

### Dinamik Metinler

GML script'te oluşturulan metinler:
```gml
var msg = "You have " + string(gold) + " gold";
```
Bu tarz metinler STRG'de olmayabilir.

### Hardcoded Metinler

draw_text() ile çizilen:
- STRG'de bulunur
- Ancak context belirsiz olabilir

### Font Limiti

Türkçe karakterler mevcut font'ta yoksa:
- Font asset değiştirilmeli
- Veya texture patch gerekli

---

## Örnek Oyunlar

| Oyun | Sürüm | Durum |
|------|-------|-------|
| Undertale | GMS1 | Çalışıyor |
| Deltarune | GMS2 | Çalışıyor |
| Hotline Miami | GMS1 | Çalışıyor |
| Hyper Light Drifter | GMS1 | Çalışıyor |

---

## Araçlar

### UndertaleModTool

Açık kaynak data.win editörü:
- String düzenleme
- Kod düzenleme
- Asset export/import

### data.win Parser

Makine-Launcher içinde:
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
3. Hash doğrulama yap

### Çeviri Görünmüyor

1. Doğru string index mi kontrol et
2. Encoding UTF-8 mi
3. Oyun cache temizle

### Oyun Çöküyor

1. String uzunlukları sınırı aşıyor mu
2. data.win yapısı bozulmuş mu
3. Font karakterleri mevcut mu

---

## Kod Örneği

### String Çıkarma

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
