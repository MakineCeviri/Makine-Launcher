# Unreal Engine Desteği

Unreal Engine dosya yapısı ve çeviri yöntemi teknik referansı.

> **Not:** Bu doküman teknik referanstır. Makine-Launcher şu an motor bazlı işlem yapmamaktadır — sadece motor tespiti yapar. Bu bilgiler adaptasyon motoru tasarımında kullanılacaktır.

---

## Genel Bakış

**Destek Durumu:** Tam Destek

**Desteklenen Sürümler:**
- Unreal Engine 4.x
- Unreal Engine 5.x

---

## Nasıl Çalışır

Makine-Launcher Unreal oyunlarında PAK dosya sistemi kullanır:

1. Türkçe lokalizasyon PAK dosyası oluşturulur
2. Engine öncelik sistemi ile üstüne yazma yapılır
3. Orijinal dosyalar dokunulmaz

### Çalışma Prensibi

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

## Otomatik Algılama

Makine-Launcher Unreal oyunlarını şu dosyalardan tespit eder:

| Dosya | Açıklama |
|-------|----------|
| `Engine/Binaries/` | Engine runtime |
| `Content/Paks/*.pak` | PAK arşivleri |
| `.uproject` | Proje dosyası |
| `Manifest_*.txt` | Unreal manifest |

---

## Çeviri Süreci

### 1. PAK Dosya Yapısı

```
[Oyun]/
└── Content/
    └── Paks/
        ├── pakchunk0-WindowsNoEditor.pak  # Orijinal
        └── ~mods/
            └── tr_localization_P.pak       # Ceviri
```

### 2. Lokalizasyon Yapısı

PAK içeriği:
```
Content/
└── Localization/
    └── tr/
        ├── Game.locres
        └── Engine.locres
```

### 3. Öncelik Sistemi

`_P` suffix'i en yüksek önceliği verir:
- `pakchunk0.pak` (öncelik: 0)
- `tr_localization_P.pak` (öncelik: 100)

---

## Teknik Detaylar

### .locres Formatı

Binary lokalizasyon format:
- String ID -> Çevrilmiş metin
- Namespace desteği
- Plural form desteği

### PAK Şifreleme

Bazı oyunlar PAK şifreleme kullanıyor:
- AES şifreleme
- Anahtar gerekli (çoğu oyun için biliniyor)

### Asset Referansları

Bazı metinler asset içinde:
- DataTable
- StringTable
- Blueprint

---

## Bilinen Sorunlar

### Şifreli PAK

Şifreleme anahtarı bilinmiyorsa:
- Çeviri uygulanamaz
- Topluluktan anahtar istenebilir

### Cook Edilmiş Asset

Bazı metinler Cook edilmiş:
- Ayrı asset patch gerekebilir
- Daha kompleks işlem

### Font Sorunları

Varsayılan font Türkçe karakter içermeyebilir:
- Font asset patch gerekebilir

---

## Örnek Oyunlar

| Oyun | UE Sürüm | Durum |
|------|----------|-------|
| Fortnite | UE5 | Çalışıyor (Resmi TR) |
| PUBG | UE4 | Çalışıyor |
| Dead by Daylight | UE4 | Çalışıyor |
| FF7 Remake | UE4 | Çalışıyor |

---

## Troubleshooting

### PAK Yüklenmiyor

1. `~mods` klasörü var mı kontrol et
2. `_P` suffix'i var mı kontrol et
3. Dosya boyutu kontrol et (boş olmamalı)

### Çeviri Görünmüyor

1. `.locres` dosya yapısı kontrol et
2. Namespace uyumlu mu kontrol et
3. Oyun dil ayarını kontrol et

### Oyun Çöküyor

1. PAK bütünlüğü kontrol et
2. Engine sürümü uyumlu mu kontrol et
3. Log kontrol et: `[Oyun]/Saved/Logs/`

---

## Kaynaklar

- [UE Localization](https://docs.unrealengine.com/5.0/en-US/localization-in-unreal-engine/)
- [PAK Format](https://github.com/panzi/u4pak)
