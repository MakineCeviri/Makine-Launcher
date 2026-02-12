# Mimari Genel Bakış

MakineAI'nin sistem mimarisini açıklar.

---

## İki Katmanlı Yapı

```
┌──────────────────────────────────────────────────────┐
│                  Makine (Ürün)                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │              Qt6 QML UI Layer                    │ │
│  │  Main.qml → HomeScreen → GameDetail → Settings  │ │
│  └──────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────┐ │
│  │             Qt Services Layer                    │ │
│  │  GameService │ CoreBridge │ SettingsManager      │ │
│  │  BackupManager │ ProcessScanner │ SystemTray     │ │
│  │  LocalPackageManager │ BatchOperationService     │ │
│  └──────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────┤
│               MakineAI (Motor)                        │
│  ┌──────────────────────────────────────────────────┐ │
│  │  Guncelleme Tespiti   (iskelet mevcut)           │ │
│  │  Degisiklik Analizi   (henuz gelistirilmedi)     │ │
│  │  Adaptasyon Motoru    (henuz gelistirilmedi)     │ │
│  └──────────────────────────────────────────────────┘ │
├──────────────────────────────────────────────────────┤
│            C++ Core Library (Opsiyonel)                │
│  GameDetector │ PatchEngine │ PackageManager          │
│  AssetParser │ VersionTracker │ Security              │
│  (MSVC, vcpkg — sadece release build)                 │
└──────────────────────────────────────────────────────┘
```

---

## Makine: Servis Katmanı

### CoreBridge
Oyun tarama ve paket yönetiminin merkezi.

**UI_ONLY modda (MinGW, `just dev`):**
- Steam: Registry + VDF + ACF parse (saf Qt)
- Epic: Manifest JSON tarama
- GOG: Registry tarama
- Motor tespiti: Dosya imzaları (DLL, dizin, uzantı)
- Paket yönetimi: LocalPackageManager üzerinden

**Full modda (MSVC, `just release`):**
- Core kütüphanesi üzerinden tüm işlemler

### LocalPackageManager
Yerel çeviri paketlerini yönetir:
- `translation_data/` dizinini tarar
- Paket ID → Steam AppID eşleştirmesi
- Dosya kopyalama (overlay) kurulum
- Kurulu paket durumu takibi

### GameService
QML ile CoreBridge arasındaki köprü:
- Oyun listesi yönetimi
- Anti-cheat kontrolü (EAC, BattlEye, Vanguard)
- Çeviri durumu sorgulama

### BackupManager
Yedekleme ve geri yükleme:
- Kurulumdan önce otomatik yedek
- Async yedekleme (QtConcurrent)
- Oyun başına maks yedek sınırı

### ProcessScanner
Çalışan oyunları tespit eder:
- Windows API ile process tarama
- Visibility-aware: minimize'da yavaş tarar

### SystemTrayManager
Native Win32 sistem tepsisi:
- Shell_NotifyIconW ile doğrudan entegrasyon
- Qt6Widgets bağımlılığına gerek yok
- Context menu: Göster / Ayarlar / Çıkış

### SettingsManager
Uygulama ayarları:
- Dil, tema, bildirim tercihleri
- Çeviri veri yolu (`translationDataPath`)
- QSettings ile persist

### IntegrityService
Binary bütünlük kontrolü:
- SHA-256 ile executable doğrulama
- Dev build'de atlanıyor

### UpdateDetectionService (İskelet)
Oyun güncelleme tespiti:
- Tier 1: Store metadata kontrol (Steam buildid, Epic manifest)
- Tier 2: Dosya hash snapshot karşılaştırma
- Sınıf yapısı ve API tanımlı, implementasyon bekleniyor

---

## MakineAI: Adaptasyon Motoru (Planlanan)

### Güncelleme Tespiti
```
Yama kurulurken:
  dosya_hash'leri → installed_packages.json'a kaydet

Uygulama acildiginda:
  installed_packages.json'daki hash'leri kontrol et
  Hash uyusmuyorsa → oyun guncellenmis
```

### Değişiklik Analizi
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
  Tasinan string → fuzzy match ile yeni konuma tasi
  Yeni string → "ceviri gerekli" olarak isaretle
  Silinen string → kaldir
```

---

## Veri Akışı

### Tipik Kurulum Akışı

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

### Güncelleme Sonrası Akış (Planlanan)

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
5. Adaptasyon: Yamayi uyarla
       |
       v
6. Dogrulama: Calisiyor mu?
       |
       v
7. Sonuc: Yama otomatik guncellendi
```

---

## Build Modları

| Mod | Derleyici | Özellikler |
|-----|----------|------------|
| UI_ONLY (`dev`) | MinGW | Gerçek oyun tarama, yerel paket kurulumu, saf Qt |
| Full (`release`) | MSVC | + Core kütüphane, + vcpkg bağımlılıkları |

UI_ONLY modda tüm temel özellikler çalışır. Core kütüphane sadece
ileri adaptasyon özellikleri için gereklidir.

---

## Mimari Kararlar (ADR)

| ADR | Başlık | Durum |
|-----|--------|-------|
| [0001](../adr/0001-native-cpp-architecture.md) | Native C++ Architecture | Geçerli |
| [0002](../adr/0002-result-based-error-handling.md) | Result-based Error Handling | Geçerli |
| [0004](../adr/0004-optional-library-integration.md) | Optional Library Integration | Geçerli |
| [0006](../adr/0006-adaptation-engine-direction.md) | Adaptation Engine Direction | Geçerli |

---

## Sonraki Adımlar

- [QML Arayüz](qml-frontend.md)
- [Build Sistemi](build-system.md)
- [Core Kütüphane](core-library.md) (opsiyonel, ileri özellikler)
