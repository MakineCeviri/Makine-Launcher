# RPG Maker Destegi

MakineAI RPG Maker oyun motoru destegi detaylari.

---

## Genel Bakis

**Destek Durumu:** Tam Destek

**Desteklenen Surumler:**
- RPG Maker MV (JavaScript)
- RPG Maker MZ (JavaScript)
- RPG Maker VX Ace (Ruby)

---

## Nasil Calisir

RPG Maker oyunlari JSON/Ruby tabanli dil dosyalari kullanir:

### MV/MZ

1. `www/data/` altindaki JSON dosyalari cevirilir
2. Plugin sistemi ile hook (opsiyonel)

### VX Ace

1. `Data/` altindaki `.rvdata2` dosyalari cevirilir
2. Ruby script degisikligi (opsiyonel)

---

## Otomatik Algilama

MakineAI RPG Maker oyunlarini su dosyalardan tespit eder:

| Motor | Dosya |
|-------|-------|
| MV | `www/js/rpg_core.js` |
| MZ | `js/rmmz_core.js` |
| VX Ace | `Data/System.rvdata2` |

---

## Ceviri Sureci (MV/MZ)

### JSON Dosyalari

```
www/data/
├── System.json      # Sistem metinleri
├── Actors.json      # Karakter isimleri
├── Classes.json     # Sinif isimleri
├── Skills.json      # Yetenek isimleri
├── Items.json       # Esya isimleri
├── Weapons.json     # Silah isimleri
├── Armors.json      # Zirh isimleri
├── Enemies.json     # Dusman isimleri
├── States.json      # Durum isimleri
├── CommonEvents.json # Ortak eventler
└── MapXXX.json      # Harita diyaloglari
```

### Ornek JSON

```json
{
  "id": 1,
  "name": "Potion",
  "description": "Restores 50 HP.",
  "iconIndex": 32,
  "price": 50
}
```

Cevirilmis:
```json
{
  "id": 1,
  "name": "Iksir",
  "description": "50 HP yeniler.",
  "iconIndex": 32,
  "price": 50
}
```

---

## Ceviri Sureci (VX Ace)

### RGSS3 Dosyalari

```
Data/
├── System.rvdata2   # Sistem
├── Actors.rvdata2   # Karakterler
├── Classes.rvdata2  # Siniflar
├── Skills.rvdata2   # Yetenekler
├── Items.rvdata2    # Esyalar
├── Map001.rvdata2   # Harita 1
└── ...
```

### Ruby Script

Scripts.rvdata2 icindeki Vocab modulu:

```ruby
module Vocab
  # Shop Screen
  ShopBuy         = "Satın Al"
  ShopSell        = "Sat"
  ShopCancel      = "İptal"

  # Battle
  Escape          = "Kaç"
  Attack          = "Saldır"
end
```

---

## Teknik Detaylar

### Encoding

- MV/MZ: UTF-8 (varsayilan)
- VX Ace: UTF-8 veya Shift_JIS

### Plugin Sistemi (MV/MZ)

Opsiyonel plugin ile dinamik ceviri:

```javascript
// plugins/TurkishTranslation.js
(function() {
    var _Window_Base_drawText = Window_Base.prototype.drawText;
    Window_Base.prototype.drawText = function(text, x, y, maxWidth, align) {
        text = translateText(text);
        _Window_Base_drawText.call(this, text, x, y, maxWidth, align);
    };
})();
```

### Font Degisimi

Turkce karakter icin font degisimi:

```json
// System.json
{
  "gameTitle": "Oyun Adı",
  "locale": "tr",
  "advanced": {
    "mainFontFilename": "TurkishFont.ttf"
  }
}
```

---

## Bilinen Sorunlar

### Sifreli Oyunlar

Bazi oyunlar sifreleme kullaniyor:
- `*.rpgmvp`, `*.rpgmvo` uzantili
- Sifre cozme gerekli

### Hardcoded Metinler

Bazi metinler plugin icinde:
- Her plugin ayri incelenmeli

### Karakter Limiti

Bazi UI elementleri sinirli alan:
- Ceviri kisaltma gerekebilir

---

## Ornek Oyunlar

| Oyun | Motor | Durum |
|------|-------|-------|
| Omori | MV | Calisiyor |
| To the Moon | XP/VX | Calisiyor |
| OneShot | MV | Calisiyor |
| Corpse Party | VX Ace | Calisiyor |

---

## Troubleshooting

### JSON Parse Hatasi

1. UTF-8 BOM kontrolu
2. JSON syntax kontrolu
3. Escape karakterleri kontrol

### Ceviri Gorunmuyor

1. Dogru dosya duzenlendi mi kontrol et
2. Oyun cache temizle
3. www/save/ klasorunu sil (save uyumsuzlugu)

### Karakter Bozuk

1. Font dosyasi var mi kontrol et
2. Encoding UTF-8 mi kontrol et
3. System.json locale ayari

---

## Kaynaklar

- [RPG Maker MV/MZ Docs](https://help.mn-up.com/)
- [RGSS3 Reference](https://www.rpgmakerweb.com/)
