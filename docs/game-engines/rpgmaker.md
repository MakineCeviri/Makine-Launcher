# RPG Maker Desteği

RPG Maker dosya yapısı ve çeviri yöntemi teknik referansı.

> **Not:** Bu doküman teknik referanstır. Makine-Launcher şu an motor bazlı işlem yapmamaktadır — sadece motor tespiti yapar. Bu bilgiler adaptasyon motoru tasarımında kullanılacaktır.

---

## Genel Bakış

**Destek Durumu:** Tam Destek

**Desteklenen Sürümler:**
- RPG Maker MV (JavaScript)
- RPG Maker MZ (JavaScript)
- RPG Maker VX Ace (Ruby)

---

## Nasıl Çalışır

RPG Maker oyunları JSON/Ruby tabanlı dil dosyaları kullanır:

### MV/MZ

1. `www/data/` altındaki JSON dosyaları çevrilir
2. Plugin sistemi ile hook (opsiyonel)

### VX Ace

1. `Data/` altındaki `.rvdata2` dosyaları çevrilir
2. Ruby script değişikliği (opsiyonel)

---

## Otomatik Algılama

Makine-Launcher RPG Maker oyunlarını şu dosyalardan tespit eder:

| Motor | Dosya |
|-------|-------|
| MV | `www/js/rpg_core.js` |
| MZ | `js/rmmz_core.js` |
| VX Ace | `Data/System.rvdata2` |

---

## Çeviri Süreci (MV/MZ)

### JSON Dosyaları

```
www/data/
├── System.json      # Sistem metinleri
├── Actors.json      # Karakter isimleri
├── Classes.json     # Sınıf isimleri
├── Skills.json      # Yetenek isimleri
├── Items.json       # Eşya isimleri
├── Weapons.json     # Silah isimleri
├── Armors.json      # Zırh isimleri
├── Enemies.json     # Düşman isimleri
├── States.json      # Durum isimleri
├── CommonEvents.json # Ortak eventler
└── MapXXX.json      # Harita diyalogları
```

### Örnek JSON

```json
{
  "id": 1,
  "name": "Potion",
  "description": "Restores 50 HP.",
  "iconIndex": 32,
  "price": 50
}
```

Çevrilmiş:
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

## Çeviri Süreci (VX Ace)

### RGSS3 Dosyaları

```
Data/
├── System.rvdata2   # Sistem
├── Actors.rvdata2   # Karakterler
├── Classes.rvdata2  # Sınıflar
├── Skills.rvdata2   # Yetenekler
├── Items.rvdata2    # Eşyalar
├── Map001.rvdata2   # Harita 1
└── ...
```

### Ruby Script

Scripts.rvdata2 içindeki Vocab modülü:

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

- MV/MZ: UTF-8 (varsayılan)
- VX Ace: UTF-8 veya Shift_JIS

### Plugin Sistemi (MV/MZ)

Opsiyonel plugin ile dinamik çeviri:

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

### Font Değişimi

Türkçe karakter için font değişimi:

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

### Şifreli Oyunlar

Bazı oyunlar şifreleme kullanıyor:
- `*.rpgmvp`, `*.rpgmvo` uzantılı
- Şifre çözme gerekli

### Hardcoded Metinler

Bazı metinler plugin içinde:
- Her plugin ayrı incelenmeli

### Karakter Limiti

Bazı UI elementleri sınırlı alan:
- Çeviri kısaltma gerekebilir

---

## Örnek Oyunlar

| Oyun | Motor | Durum |
|------|-------|-------|
| Omori | MV | Çalışıyor |
| To the Moon | XP/VX | Çalışıyor |
| OneShot | MV | Çalışıyor |
| Corpse Party | VX Ace | Çalışıyor |

---

## Troubleshooting

### JSON Parse Hatası

1. UTF-8 BOM kontrolü
2. JSON syntax kontrolü
3. Escape karakterleri kontrol

### Çeviri Görünmüyor

1. Doğru dosya düzenlendi mi kontrol et
2. Oyun cache temizle
3. www/save/ klasörünü sil (save uyumsuzluğu)

### Karakter Bozuk

1. Font dosyası var mı kontrol et
2. Encoding UTF-8 mi kontrol et
3. System.json locale ayarı

---

## Kaynaklar

- [RPG Maker MV/MZ Docs](https://help.mn-up.com/)
- [RGSS3 Reference](https://www.rpgmakerweb.com/)
