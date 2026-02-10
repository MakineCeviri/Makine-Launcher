# Mimari Genel Bakis

MakineAI'nin sistem mimarisini aciklar.

---

## Iki Katmanli Yapi

```
┌──────────────────────────────────────────────────────┐
│                  Makine (Urun)                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │              Qt6 QML UI Layer                    │ │
│  │  Main.qml → HomeScreen → GameDetail → Settings  │ │
│  └──────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────┐ │
│  │             Qt Services Layer                    │ │
│  │  GameService │ CoreBridge │ SettingsManager      │ │
│  │  BackupManager │ ProcessScanner │ SystemTray     │ │
│  │  LocalPackageManager                             │ │
│  └──────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────┤
│               MakineAI (Motor)                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │  Guncelleme Tespiti   (henuz gelistirilmedi)     │ │
│  │  Degisiklik Analizi   (henuz gelistirilmedi)     │ │
│  │  Adaptasyon Motoru    (henuz gelistirilmedi)     │ │
│  └──────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────┤
│            C++ Core Library (Opsiyonel)                │
│  GameDetector │ PatchEngine │ PackageManager          │
│  (MSVC, vcpkg — sadece release build)                 │
└──────────────────────────────────────────────────────┘
```

---

## Makine: Servis Katmani

### CoreBridge
Oyun tarama ve paket yonetiminin merkezi.

**UI_ONLY modda (MinGW, `just dev`):**
- Steam: Registry + VDF + ACF parse (saf Qt)
- Epic: Manifest JSON tarama
- GOG: Registry tarama
- Motor tespiti: Dosya imzalari (DLL, dizin, uzanti)
- Paket yonetimi: LocalPackageManager uzerinden

**Full modda (MSVC, `just release`):**
- Core kutuphanesi uzerinden tum islemler

### LocalPackageManager
Yerel ceviri paketlerini yonetir:
- `translation_data/` dizinini tarar
- Paket ID → Steam AppID eslestirmesi
- Dosya kopyalama (overlay) kurulum
- Kurulu paket durumu takibi

### GameService
QML ile CoreBridge arasindaki kopru:
- Oyun listesi yonetimi
- Anti-cheat kontrolu (EAC, BattlEye, Vanguard)
- Ceviri durumu sorgulama

### BackupManager
Yedekleme ve geri yukleme:
- Kurulumdan once otomatik yedek
- Async yedekleme (QtConcurrent)
- Oyun basina maks yedek siniri

### ProcessScanner
Calisan oyunlari tespit eder:
- Windows API ile process tarama
- Visibility-aware: minimize'da yavas tarar

### SettingsManager
Uygulama ayarlari:
- Dil, tema, bildirim tercihleri
- Ceviri veri yolu (`translationDataPath`)
- QSettings ile persist

---

## MakineAI: Adaptasyon Motoru (Planlanan)

### Guncelleme Tespiti
```
Yama kurulurken:
  dosya_hash'leri → installed_packages.json'a kaydet

Uygulama acildiginda:
  installed_packages.json'daki hash'leri kontrol et
  Hash uyusmuyorsa → oyun guncellenmis
```

### Degisiklik Analizi
```
Eski dosyalar (yedekten) vs Yeni dosyalar (guncel oyun)
  → Structural diff (metin formatlari icin)
  → Binary diff (ikili formatlari icin)
  → Degisiklik haritasi cikar
```

### Otomatik Uyarlama
```
Degisiklik haritasina gore:
  Degismemis string → koru
  Tasınan string → fuzzy match ile yeni konuma tasi
  Yeni string → "ceviri gerekli" olarak isaretle
  Silinen string → kaldir
```

---

## Veri Akisi

### Tipik Kurulum Akisi

```
1. Kullanici oyunu secer (QML)
       |
       v
2. GameService ceviri paketini kontrol eder
       |
       v
3. BackupManager yedek alir
       |
       v
4. LocalPackageManager dosyalari kopyalar
       |
       v
5. Sonuc: Oyun Turkce calisiyor
```

### Guncelleme Sonrasi Akis (Planlanan)

```
1. Uygulama acilir
       |
       v
2. Dosya hash'leri kontrol edilir
       |
       v
3. Degisiklik tespit edildi!
       |
       v
4. Analiz: Ne degisti?
       |
       v
5. Adaptasyon: Yamayı uyarla
       |
       v
6. Dogrulama: Calisıyor mu?
       |
       v
7. Sonuc: Yama otomatik guncellendi
```

---

## Build Modlari

| Mod | Derleyici | Ozellikler |
|-----|----------|------------|
| UI_ONLY (`dev`) | MinGW | Gercek oyun tarama, yerel paket kurulumu, saf Qt |
| Full (`release`) | MSVC | + Core kutuphane, + vcpkg bagimliliklari |

UI_ONLY modda tum temel ozellikler calisir. Core kutuphane sadece
ileri adaptasyon ozellikleri icin gereklidir.

---

## Mimari Kararlar (ADR)

| ADR | Baslik | Durum |
|-----|--------|-------|
| [0001](../adr/0001-native-cpp-architecture.md) | Native C++ Architecture | Gecerli |
| [0002](../adr/0002-result-based-error-handling.md) | Result-based Error Handling | Gecerli |
| [0003](../adr/0003-translation-pipeline-decision-engine.md) | Translation Pipeline | Gozden gecirilecek |
| [0004](../adr/0004-optional-library-integration.md) | Optional Library Integration | Gecerli |
| [0005](../adr/0005-handler-based-engine-support.md) | Handler-based Engine Support | Gozden gecirilecek |

> **Not:** ADR-0003 ve ADR-0005 yeni vizyon dogrultusunda guncellenmeli.
> Handler pattern core kutuphanede kalacak ancak onceligi adaptasyon motoruna kaydi.

---

## Sonraki Adimlar

- [QML Arayuz](qml-frontend.md)
- [Build Sistemi](build-system.md)
- [Core Kutuphane](core-library.md) (opsiyonel, ileri ozellikler)
