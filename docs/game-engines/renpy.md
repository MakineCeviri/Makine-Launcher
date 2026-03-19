# Ren'Py Desteği

Ren'Py dosya yapısı ve çeviri yöntemi teknik referansı.

> **Not:** Bu doküman teknik referanstır. Makine-Launcher şu an motor bazlı işlem yapmamaktadır — sadece motor tespiti yapar. Bu bilgiler adaptasyon motoru tasarımında kullanılacaktır.

---

## Genel Bakış

**Destek Durumu:** Tam Destek

**Desteklenen Sürümler:**
- Ren'Py 7.x
- Ren'Py 8.x

---

## Nasıl Çalışır

Ren'Py native lokalizasyon sistemi kullanır:

1. `tl/tr/` klasörüne çeviri dosyaları eklenir
2. Oyun içinden dil değiştirilebilir
3. Orijinal dosyalar dokunulmaz

### Çalışma Prensibi

```
Ceviri Dosyasi (.rpy)
    |
    v
game/tl/tr/
    |
    v
Renpy Lokalizasyon Sistemi
    |
    v
Oyun Icinde Dil Secimi
    |
    v
Turkce Metin Goster
```

---

## Otomatik Algılama

Makine-Launcher Ren'Py oyunlarını şu dosyalardan tespit eder:

| Dosya | Açıklama |
|-------|----------|
| `renpy/` | Ren'Py engine |
| `game/script.rpy` | Ana script |
| `lib/pythonXX/` | Python runtime |
| `*.rpyc` | Compiled script |

---

## Çeviri Süreci

### Klasör Yapısı

```
game/
├── script.rpy           # Orijinal script
├── gui.rpy              # GUI tanımları
└── tl/
    └── tr/              # Türkçe çeviri
        ├── script.rpy   # Script çevirisi
        ├── gui.rpy      # GUI çevirisi
        ├── common.rpy   # Ortak metinler
        └── options.rpy  # Seçenekler
```

### Çeviri Formatı

```python
# game/tl/tr/script.rpy

translate turkish start_label:
    # "Hello, welcome to my game!"
    "Merhaba, oyunuma hosgeldin!"

translate turkish strings:
    old "Start Game"
    new "Oyuna Basla"

    old "Load Game"
    new "Oyun Yukle"

    old "Settings"
    new "Ayarlar"
```

### Karakter İsimleri

```python
translate turkish python:
    define e = Character("Eileen", color="#c8ffc8")
    # Turkce karakter tanimı
    define e = Character("Aylin", color="#c8ffc8")
```

---

## Teknik Detaylar

### Encoding

- UTF-8 zorunlu
- BOM olmadan

### Translate Blokları

```python
# Label cevirisi
translate turkish label_name:
    "Cevirilecek metin"

# String cevirisi
translate turkish strings:
    old "English text"
    new "Turkce metin"
```

### Style Çevirisi

```python
translate turkish style default:
    font "DejaVuSans.ttf"
```

### Python Blokları

```python
translate turkish python:
    gui.text_font = "TurkishFont.ttf"
```

---

## Dil Seçimi

### Oyun İçinde

```python
# options.rpy
define config.language = "turkish"
```

### Otomatik Tespit

```python
init python:
    import os
    if os.environ.get('LANG', '').startswith('tr'):
        config.language = "turkish"
```

---

## Bilinen Sorunlar

### Compiled Scripts

`.rpyc` dosyaları compile edilmiş:
- `.rpy` kaynak yoksa decompile gerekir
- `unrpyc` aracı kullanılabilir

### Image Text

Resim içindeki metinler:
- Ayrı görsel düzenleme gerekir
- Photoshop/GIMP ile

### Conditional Text

Değişkenli metinler:
```python
# Dikkatli cevirilmeli
"You have [gold] gold pieces."
# Turkce
"[gold] altin parcana sahipsin."
```

---

## Örnek Oyunlar

| Oyun | Sürüm | Durum |
|------|-------|-------|
| Doki Doki Literature Club | 7.x | Çalışıyor |
| Katawa Shoujo | 6.x | Çalışıyor |
| Long Live the Queen | 6.x | Çalışıyor |

---

## Troubleshooting

### Çeviri Yüklenmiyor

1. Klasör adı `tl/tr/` mi kontrol et
2. `.rpy` syntax hatası var mı kontrol et
3. `translate turkish` doğru yazılmış mı

### Karakter Bozuk

1. Font Türkçe karakter destekliyor mu
2. Encoding UTF-8 mi
3. gui.rpy'de font tanımlı mı

### Syntax Hatası

1. Girintileme (indentation) kontrol et
2. Tırnaklar eşleşli mi kontrol et
3. Python syntax geçerli mi

---

## Hızlı Referans

### Yeni Çeviri Başlat

```bash
# Ren'Py Launcher ile
# 1. Oyunu ac
# 2. "Generate Translations" sec
# 3. Dil adi: "turkish"
```

### Çeviri Test

```python
# script.rpy'de test
label start:
    $ renpy.change_language("turkish")
```

---

## Kaynaklar

- [Ren'Py Translation Docs](https://www.renpy.org/doc/html/translation.html)
- [Ren'Py Language](https://www.renpy.org/doc/html/language_basics.html)
