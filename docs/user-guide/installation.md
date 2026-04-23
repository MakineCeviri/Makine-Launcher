# Kurulum

Bu kılavuz Makine-Launcher'ın kurulumunu adım adım anlatmaktadır.

---

## Sistem Gereksinimleri

### Minimum

| Gereksinim | Değer |
|------------|-------|
| İşletim Sistemi | Windows 10 (64-bit) |
| RAM | 4 GB |
| Disk Alanı | 500 MB |
| Ekran | 1280x720 |

### Önerilen

| Gereksinim | Değer |
|------------|-------|
| İşletim Sistemi | Windows 11 |
| RAM | 8 GB |
| Disk Alanı | 1 GB |
| Ekran | 1920x1080 |

---

## Kurulum Adımları

### Adım 1: İndirme

Son sürümü resmi web sitesinden indirin:

[makineceviri.org](https://makineceviri.org)

### Adım 2: Arşivi Açma

1. İndirilen ZIP dosyasını sağ tıklayın
2. "Tümünü ayıkla..." seçin
3. İstediğiniz konuma çıkartın

**Önerilen Konum:** `C:\Program Files\MakineLauncher\` veya `C:\Games\MakineLauncher\`

### Adım 3: İlk Çalıştırma

1. `Makine-Launcher.exe` dosyasını çift tıklayın
2. Windows güvenlik uyarısı çıkarsa "Yine de çalıştır" seçin
3. Uygulama açılır ve otomatik tarama başlar

---

## Otomatik Oyun Tespiti

Makine-Launcher aşağıdaki platformları otomatik tarar:

### Steam

- Varsayılan: `C:\Program Files (x86)\Steam\`
- Kütüphane klasörleri otomatik bulunur
- Steam API üzerinden oyun listesi alınır

### Epic Games

- Varsayılan: `C:\Program Files\Epic Games\`
- Manifest dosyalarından oyun listesi
- Kurulu oyunlar taranır

### GOG Galaxy

- Varsayılan: `C:\Program Files (x86)\GOG Galaxy\`
- GOG Galaxy veritabanından liste
- Standalone kurulumlar da desteklenir

### Manuel Ekleme

Farklı konumdaki oyunlar için:

1. Ana ekranda "+" butonuna basın
2. Oyun klasörünü seçin
3. Makine-Launcher motoru otomatik tespit eder

---

## Güncelleme

### Otomatik Güncelleme (Planlanan)

Gelecek sürümlerde:
- Başlangıçta güncelleme kontrolü
- Arka planda indirme
- Tek tıkla güncelleme

### Manuel Güncelleme

1. Yeni sürümü indirin
2. Mevcut klasörün üstüne çıkartın
3. Ayarlar ve yedekler korunur

---

## Sorun Giderme

### Windows SmartScreen Uyarısı

"Windows bilgisayarınızı korudu" uyarısı çıkarsa:

1. "Daha fazla bilgi" tıklayın
2. "Yine de çalıştır" tıklayın

### Antivirüs Uyarısı

Bazı antivirüsler yanlış pozitif verebilir:

1. Makine-Launcher klasörünü istisna olarak ekleyin
2. Veya antivirüsü geçici olarak devre dışı bırakın

### DLL Hatası

"VCRUNTIME140.dll bulunamadı" hatası için:

[Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) indirip kurun.

---

## Kaldırma

Makine-Launcher'ı kaldırmak için:

1. Makine-Launcher klasörünü silin
2. Ayarlar: `%APPDATA%\MakineLauncher\` klasörünü silin (opsiyonel)
3. Yedekler: Oyun klasörlerindeki `MakineLauncher_Backups` silin (opsiyonel)

---

## Sonraki Adımlar

- [Hızlı Başlangıç](getting-started.md)
