# Kurulum

Bu kilavuz MakineAI'nin kurulumunu adim adim anlatmaktadir.

---

## Sistem Gereksinimleri

### Minimum

| Gereksinim | Deger |
|------------|-------|
| Isletim Sistemi | Windows 10 (64-bit) |
| RAM | 4 GB |
| Disk Alani | 500 MB |
| Ekran | 1280x720 |

### Onerilen

| Gereksinim | Deger |
|------------|-------|
| Isletim Sistemi | Windows 11 |
| RAM | 8 GB |
| Disk Alani | 1 GB |
| Ekran | 1920x1080 |

---

## Kurulum Adimlari

### Adim 1: Indirme

Son surumu resmi web sitesinden indirin:

[makineai.com](https://makineai.com)

### Adim 2: Arsivi Acma

1. Indirilen ZIP dosyasini sag tiklayin
2. "Tümünü ayıkla..." secin
3. Istediginiz konuma cikartin

**Onerilen Konum:** `C:\Program Files\MakineAI\` veya `C:\Games\MakineAI\`

### Adim 3: Ilk Calistirma

1. `MakineAI.exe` dosyasini cift tiklayin
2. Windows guvenlik uyarisi cikarsa "Yine de calistir" secin
3. Uygulama acilir ve otomatik tarama baslar

---

## Otomatik Oyun Tespiti

MakineAI asagidaki platformlari otomatik tarar:

### Steam

- Varsayilan: `C:\Program Files (x86)\Steam\`
- Kutuphane klasorleri otomatik bulunur
- Steam API uzerinden oyun listesi alinir

### Epic Games

- Varsayilan: `C:\Program Files\Epic Games\`
- Manifest dosyalarindan oyun listesi
- Kurulu oyunlar taranir

### GOG Galaxy

- Varsayilan: `C:\Program Files (x86)\GOG Galaxy\`
- GOG Galaxy veritabanindan liste
- Standalone kurulumlar da desteklenir

### Manuel Ekleme

Farkli konumdaki oyunlar icin:

1. Ana ekranda "+" butonuna basin
2. Oyun klasorunu secin
3. MakineAI motoru otomatik tespit eder

---

## Guncelleme

### Otomatik Guncelleme (Planlanan)

Gelecek surumlerde:
- Baslangiçta guncelleme kontrolu
- Arka planda indirme
- Tek tikla guncelleme

### Manuel Guncelleme

1. Yeni surumu indirin
2. Mevcut klasorun ustune cikartin
3. Ayarlar ve yedekler korunur

---

## Sorun Giderme

### Windows SmartScreen Uyarisi

"Windows bilgisayarinizi korudu" uyarisi cikarsa:

1. "Daha fazla bilgi" tiklayin
2. "Yine de calistir" tiklayin

### Antivirus Uyarisi

Bazi antivirusler yanlis pozitif verebilir:

1. MakineAI klasorunu istisna olarak ekleyin
2. Veya antivirusu gecici olarak devre disi birakin

### DLL Hatasi

"VCRUNTIME140.dll bulunamadi" hatasi icin:

[Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) indirip kurun.

---

## Kaldirma

MakineAI'yi kaldirmak icin:

1. MakineAI klasorunu silin
2. Ayarlar: `%APPDATA%\MakineAI\` klasorunu silin (opsiyonel)
3. Yedekler: Oyun klasorlerindeki `MakineAI_Backups` silin (opsiyonel)

---

## Sonraki Adimlar

- [Hizli Baslangic](getting-started.md)
- [Desteklenen Oyunlar](supported-games.md)
