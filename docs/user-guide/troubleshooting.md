# Sorun Giderme

Bu kilavuz sik karsilasilan sorunlari ve cozumlerini icermektedir.

---

## Oyun Algilanmiyor

### Steam Oyunlari

**Sorun:** Steam oyunlarim listede gorunmuyor.

**Cozum:**
1. Steam'in calistigindan emin olun
2. Ayarlar > Oyun Tespiti > "Yeniden Tara" basin
3. Steam kutuphane klasorunu manuel ekleyin:
   - Ayarlar > Kutuphane Klasorleri > Ekle
   - `C:\Program Files (x86)\Steam\steamapps\common\`

### Epic Games Oyunlari

**Sorun:** Epic oyunlarim gorunmuyor.

**Cozum:**
1. Epic Games Launcher'in kurulu oldugunu dogrulayin
2. Manuel olarak ekleyin:
   - Ana ekranda "+" butonuna basin
   - Epic oyun klasorunu secin

### Manuel Ekleme Calismıyor

**Sorun:** Oyun klasorunu sectim ama eklenmedi.

**Cozum:**
- Dogru klasoru sectiginizden emin olun (oyun .exe dosyasinin bulundugu klasor)
- Oyun motorunun desteklendigini kontrol edin
- Calistirilabilir dosyanin adini kontrol edin

---

## Ceviri Yuklenmiyor

### "Ceviri paketi bulunamadi"

**Sorun:** Oyun icin ceviri paketi yok.

**Cozum:**
- Topluluk paketlerini kontrol edin
- Discord'da talep edin
- Kendi cevirinizi olusturun

### "Patch uygulama hatasi"

**Sorun:** Ceviri paketi indirildi ama uygulanamadi.

**Cozum:**
1. Oyunun kapali oldugunden emin olun
2. Antivirus'u gecici olarak devre disi birakin
3. Yonetici olarak calistirin:
   - MakineAI.exe > Sag tik > "Yonetici olarak calistir"

### "Dosya erisim engellendi"

**Sorun:** Oyun dosyalarina erisilemedi.

**Cozum:**
- Oyunun kapali oldugunu kontrol edin
- Steam/Epic uzerinden dosya butunlugunu dogrulayin
- Oyun klasorunde yazma izniniz oldugundan emin olun

---

## Performans Sorunlari

### Uygulama Yavas Aciliyor

**Muhtemel Neden:** Buyuk oyun kutuphanesi

**Cozum:**
1. Ayarlar > Performans > "Baslangicta Tarama" kapatin
2. Manuel tarama kullanin

### Yuksek RAM Kullanimi

**Normal Deger:** 100-200 MB

**Anormal Kullanim Icin:**
1. Uygulamayi yeniden baslatin
2. Gecici dosyalari temizleyin:
   - Ayarlar > Gelismis > "Onbellegi Temizle"

### Tarama Cok Uzun Suruyor

**Cozum:**
- Taranacak klasorleri sinirlayin
- SSD kullanin (HDD yavas olabilir)
- Antivirus istisna ekleyin

---

## Anti-Cheat Uyarilari

### EasyAntiCheat / BattlEye

**Uyari:** Bu oyunlar online korumali.

**Onemli:**
- Online/competitive oyunlarda ceviri kullanmayin
- Ban riski vardir
- Sadece offline/singleplayer modlarda kullanin

### Desteklenen Durumlar

| Oyun Turu | Ceviri Guvenli mi? |
|-----------|-------------------|
| Singleplayer | Evet |
| Coop (Arkadaslarla) | Genellikle Evet |
| Competitive Online | HAYIR |
| MMO | Riskli |

---

## Yedekleme Sorunlari

### "Yedekleme olusturulamadi"

**Sorun:** Disk alani yetersiz veya yazma izni yok.

**Cozum:**
1. Disk alanini kontrol edin (minimum 1 GB bos)
2. Oyun klasorunde yazma izni kontrol edin
3. Farkli yedekleme konumu secin:
   - Ayarlar > Yedekleme > Konum

### "Geri yukleme basarisiz"

**Sorun:** Yedekten geri yukleme calismadi.

**Cozum:**
1. Oyunun tamamen kapali oldugunu kontrol edin
2. Steam/Epic uzerinden dosya butunlugunu dogrulayin
3. Manuel geri yukleme:
   - `[Oyun]/MakineAI_Backups/[tarih]/` icindeki dosyalari oyuna kopyalayin

---

## Hata Raporlama

Sorununuz cozulmediyse:

### Log Dosyalari

Log dosyalarini bulun:
- `%APPDATA%\MakineAI\logs\`

### Hata Raporu Gonderme

1. Discord'da #bug-reports kanalina yazin
2. Su bilgileri ekleyin:
   - Windows surumu
   - MakineAI surumu
   - Oyun adi ve motoru
   - Hata mesaji (screenshot)
   - Log dosyasi

---

## Sonraki Adimlar

- [SSS](faq.md)
- [Baslangic Kilavuzu](getting-started.md)
- [Discord](https://discord.com/invite/QDezpy4QtV)
