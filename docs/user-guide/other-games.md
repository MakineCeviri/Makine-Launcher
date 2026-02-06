# Diger Oyunlar (Mod 2)

Bu sayfada MakineAI'nin **patch sistemi** ile destekledigi oyunlar ve topluluk cevirileri anlatilmaktadir.

## Nasil Calisir?

Desteklenmeyen motorlardaki oyunlar icin MakineAI:

1. Topluluk tarafindan olusturulan ceviri paketlerini kullanir
2. Oyun dosyalarina patch uygular
3. Orijinal dosyalari **otomatik yedekler**
4. Geri alma ozelligi saglar

Bu yontem daha genis oyun yelpazesini destekler ancak motor-spesifik optimizasyonlar icermez.

---

## Patch Sistemi Sureci

### 1. Ceviri Paketi Arama

MakineAI, merkezi sunucudan mevcut ceviri paketlerini listeler:

- Resmi ceviriler (dogrulanmis)
- Topluluk cevirileri (topluluk tarafindan)
- Taslak ceviriler (devam eden)

### 2. Paket Indirme

Secilen paket indirilir:
- Sikistirilmis format (.mtp - MakineAI Translation Package)
- Imza dogrulama (RSA-2048)
- Butunluk kontrolu (SHA-256)

### 3. Yedekleme

Patch oncesi otomatik yedekleme:
```
[Oyun Klasoru]/
  MakineAI_Backups/
    2026-02-03_14-30-00/
      - orijinal_dosya_1.dat
      - orijinal_dosya_2.dll
      - backup_manifest.json
```

### 4. Patch Uygulama

Ceviri dosyalari oyuna uygulanir:
- Binary diff/patch
- String replacement
- Asset injection

### 5. Dogrulama

Patch sonrasi kontrol:
- Dosya butunlugu
- Oyun baslatma testi (opsiyonel)

---

## Geri Alma (Rollback)

Ceviriyi kaldirmak icin:

1. MakineAI'yi acin
2. Ilgili oyunu secin
3. **"Ceviriyi Kaldir"** butonuna basin
4. Yedekten geri yukleme yapilir

### Otomatik Geri Alma

Su durumlarda otomatik geri alma tetiklenir:
- Oyun guncellendikten sonra (opsiyonel ayar)
- Patch hata verdikten sonra
- Dosya butunlugu bozuldugunda

---

## Topluluk Cevirileri

### Ceviri Paketi Olusturma

Topluluk uyeleri ceviri paketi olusturabilir:

1. Oyun dosyalarini analiz edin
2. Ceviri dosyalarini hazirlayin
3. MakineAI Package Builder ile paketleyin
4. Topluluga gonderin

### Paket Yapisi

```
my_game_turkish.mtp/
  manifest.json       # Paket metadata
  translations/       # Ceviri dosyalari
    strings.json
    dialogs.json
  patches/            # Binary patchler (opsiyonel)
  scripts/            # Ozel scriptler (opsiyonel)
  README.md           # Kullanim notlari
```

### Kalite Kontrol

Topluluk paketleri su ashamalardan gecer:

| Asama | Aciklama |
|-------|----------|
| **Taslak** | Yeni yuklenmis, test edilmemis |
| **Beta** | Topluluk tarafindan test ediliyor |
| **Dogrulanmis** | QA gecmis, kararlı |
| **Resmi** | Gelistirici onaylı |

---

## Bilinen Sinirlamalar

### DRM Korumalı Oyunlar

Bazi DRM sistemleri patch'i engelleyebilir:
- Denuvo (dosya degisikliklerini algilar)
- EAC/BattlEye (anti-cheat)

**Cozum:** Ceviri yerine mod olarak yukleme (desteklenen oyunlarda)

### Surekli Guncellenen Oyunlar

Online/live-service oyunlarda:
- Her guncelleme patch'i bozabilir
- Otomatik yedekleme kritik
- Topluluk paketleri guncellenmeli

### Coklu Dil Destegi

Bazi oyunlar sadece tek dil destekler:
- Patch orijinal dili degistirir
- Geri alma ile orijinal dil gelir

---

## Sik Sorulan Sorular

### Hangi oyunlar patch sistemi ile destekleniyor?

Teorik olarak tum oyunlar. Ancak:
- Topluluk paketi olmali
- DRM engeli olmamali
- Dosya yapisi bilinmeli

### Patch guvenli mi?

Evet:
- Orijinal dosyalar yedeklenir
- Imza dogrulama yapilir
- Geri alma her zaman mumkun

### Kendi cevirimi nasil yuklerim?

1. [Katki Kilavuzu](../CONTRIBUTING.md) okuyun
2. Package Builder araciní kullanin
3. Discord uzerinden gonderin

---

## Sonraki Adimlar

- [Sorun giderme](troubleshooting.md)
- [Sik sorulan sorular](faq.md)
- [Desteklenen motorlara don](supported-games.md)
