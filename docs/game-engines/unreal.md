# Unreal Engine Destegi

MakineAI Unreal Engine oyun motoru destegi detaylari.

---

## Genel Bakis

**Destek Durumu:** Tam Destek

**Desteklenen Surumler:**
- Unreal Engine 4.x
- Unreal Engine 5.x

---

## Nasil Calisir

MakineAI Unreal oyunlarinda PAK dosya sistemi kullanir:

1. Turkce lokalizasyon PAK dosyasi olusturulur
2. Engine oncelik sistemi ile ustune yazma yapilir
3. Orijinal dosyalar dokunulmaz

### Calisma Prensibi

```
Ceviri Paketi
    |
    v
Lokalizasyon PAK Olustur
    |
    v
[Oyun]/Content/Paks/~mods/
    |
    v
Engine Oncelik Sistemi
    |
    v
Turkce Metin Goster
```

---

## Otomatik Algilama

MakineAI Unreal oyunlarini su dosyalardan tespit eder:

| Dosya | Aciklama |
|-------|----------|
| `Engine/Binaries/` | Engine runtime |
| `Content/Paks/*.pak` | PAK arsivleri |
| `.uproject` | Proje dosyasi |
| `Manifest_*.txt` | Unreal manifest |

---

## Ceviri Sureci

### 1. PAK Dosya Yapisi

```
[Oyun]/
└── Content/
    └── Paks/
        ├── pakchunk0-WindowsNoEditor.pak  # Orijinal
        └── ~mods/
            └── tr_localization_P.pak       # Ceviri
```

### 2. Lokalizasyon Yapisi

PAK icerigi:
```
Content/
└── Localization/
    └── tr/
        ├── Game.locres
        └── Engine.locres
```

### 3. Oncelik Sistemi

`_P` suffix'i en yuksek onceligi verir:
- `pakchunk0.pak` (oncelik: 0)
- `tr_localization_P.pak` (oncelik: 100)

---

## Teknik Detaylar

### .locres Formati

Binary lokalizasyon format:
- String ID -> Cevirilmis metin
- Namespace destegi
- Plural form destegi

### PAK Sifreleme

Bazi oyunlar PAK sifreleme kullaniyor:
- AES sifreleme
- Anahtar gerekli (cogu oyun icin biliniyor)

### Asset Referanslari

Bazi metinler asset icinde:
- DataTable
- StringTable
- Blueprint

---

## Bilinen Sorunlar

### Sifreli PAK

Sifreleme anahtari bilinmiyorsa:
- Ceviri uygulanamaz
- Topluluktan anahtar istenebilir

### Cook Edilmis Asset

Bazi metinler Cook edilmis:
- Ayri asset patch gerekebilir
- Daha kompleks islem

### Font Sorunlari

Varsayilan font Turkce karakter icermeyebilir:
- Font asset patch gerekebilir

---

## Ornek Oyunlar

| Oyun | UE Surum | Durum |
|------|----------|-------|
| Fortnite | UE5 | Calisiyor (Resmi TR) |
| PUBG | UE4 | Calisiyor |
| Dead by Daylight | UE4 | Calisiyor |
| FF7 Remake | UE4 | Calisiyor |

---

## Troubleshooting

### PAK Yuklenmiyor

1. `~mods` klasoru var mi kontrol et
2. `_P` suffix'i var mi kontrol et
3. Dosya boyutu kontrol et (bos olmamalı)

### Ceviri Gorunmuyor

1. `.locres` dosya yapisi kontrol et
2. Namespace uyumlu mu kontrol et
3. Oyun dil ayarini kontrol et

### Oyun Cokuyor

1. PAK butunlugu kontrol et
2. Engine surumu uyumlu mu kontrol et
3. Log kontrol et: `[Oyun]/Saved/Logs/`

---

## Kaynaklar

- [UE Localization](https://docs.unrealengine.com/5.0/en-US/localization-in-unreal-engine/)
- [PAK Format](https://github.com/panzi/u4pak)
