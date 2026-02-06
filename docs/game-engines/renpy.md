# Ren'Py Destegi

MakineAI Ren'Py oyun motoru destegi detaylari.

---

## Genel Bakis

**Destek Durumu:** Tam Destek

**Desteklenen Surumler:**
- Ren'Py 7.x
- Ren'Py 8.x

---

## Nasil Calisir

Ren'Py native lokalizasyon sistemi kullanir:

1. `tl/tr/` klasorune ceviri dosyalari eklenir
2. Oyun icinden dil degistirilebilir
3. Orijinal dosyalar dokunulmaz

### Calisma Prensibi

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

## Otomatik Algilama

MakineAI Ren'Py oyunlarini su dosyalardan tespit eder:

| Dosya | Aciklama |
|-------|----------|
| `renpy/` | Ren'Py engine |
| `game/script.rpy` | Ana script |
| `lib/pythonXX/` | Python runtime |
| `*.rpyc` | Compiled script |

---

## Ceviri Sureci

### Klasor Yapisi

```
game/
├── script.rpy           # Orijinal script
├── gui.rpy              # GUI tanimlari
└── tl/
    └── tr/              # Turkce ceviri
        ├── script.rpy   # Script cevirisi
        ├── gui.rpy      # GUI cevirisi
        ├── common.rpy   # Ortak metinler
        └── options.rpy  # Secenekler
```

### Ceviri Formati

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

### Karakter Isimleri

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

### Translate Bloklari

```python
# Label cevirisi
translate turkish label_name:
    "Cevirilecek metin"

# String cevirisi
translate turkish strings:
    old "English text"
    new "Turkce metin"
```

### Style Cevirisi

```python
translate turkish style default:
    font "DejaVuSans.ttf"
```

### Python Bloklari

```python
translate turkish python:
    gui.text_font = "TurkishFont.ttf"
```

---

## Dil Secimi

### Oyun Icinde

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

`.rpyc` dosyalari compile edilmis:
- `.rpy` kaynak yoksa decompile gerekir
- `unrpyc` araci kullanilabilir

### Image Text

Resim icindeki metinler:
- Ayri gorsel duzenleme gerekir
- Photoshop/GIMP ile

### Conditional Text

Degiskenli metinler:
```python
# Dikkatli cevirilmeli
"You have [gold] gold pieces."
# Turkce
"[gold] altin parcana sahipsin."
```

---

## Ornek Oyunlar

| Oyun | Surum | Durum |
|------|-------|-------|
| Doki Doki Literature Club | 7.x | Calisiyor |
| Katawa Shoujo | 6.x | Calisiyor |
| Long Live the Queen | 6.x | Calisiyor |

---

## Troubleshooting

### Ceviri Yuklenmiyor

1. Klasor adi `tl/tr/` mi kontrol et
2. `.rpy` syntax hatasi var mi kontrol et
3. `translate turkish` dogru yazilmis mi

### Karakter Bozuk

1. Font Turkce karakter destekliyor mu
2. Encoding UTF-8 mi
3. gui.rpy'de font tanimli mi

### Syntax Hatasi

1. Girintileme (indentation) kontrol et
2. Tirnaklar eslesli mi kontrol et
3. Python syntax gecerli mi

---

## Hizli Referans

### Yeni Ceviri Baslat

```bash
# Ren'Py Launcher ile
# 1. Oyunu ac
# 2. "Generate Translations" sec
# 3. Dil adi: "turkish"
```

### Ceviri Test

```python
# script.rpy'de test
label start:
    $ renpy.change_language("turkish")
```

---

## Kaynaklar

- [Ren'Py Translation Docs](https://www.renpy.org/doc/html/translation.html)
- [Ren'Py Language](https://www.renpy.org/doc/html/language_basics.html)
