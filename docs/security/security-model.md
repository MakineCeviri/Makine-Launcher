# Guvenlik Modeli

MakineAI guvenlik mimarisi ve politikalari.

---

## Genel Bakis

MakineAI, kullanici verilerini ve sistemini korumak icin katmanli guvenlik modeli kullanir.

### Temel Ilkeler

1. **Minimum Yetki** - Sadece gerekli izinler istenir
2. **Veri Guvenligi** - Kullanici verileri korunur
3. **Dogrulama** - Tum paketler dogrulanir
4. **Seffaflik** - Islemler loglanir

---

## Dosya Guvenligi

### Yedekleme

Her patch isleminden once otomatik yedekleme:

```
[Oyun]/MakineAI_Backups/
└── 2026-02-03_14-30-00/
    ├── orijinal_dosya.dat
    ├── manifest.json
    └── checksum.sha256
```

### Dosya Izinleri

MakineAI sadece su klasorlere yazar:
- Oyun kurulum klasoru (patch icin)
- `%APPDATA%/MakineAI/` (ayarlar)
- `%LOCALAPPDATA%/MakineAI/` (cache, logs)

### Rollback

Herhangi bir sorundan tek tikla geri donme:
- Yedekten otomatik geri yukleme
- Dosya butunlugu dogrulama
- Islem loglama

---

## Paket Guvenligi

### Imza Dogrulama

Tum ceviri paketleri imzalanir:

```
Paket Yapisi:
├── manifest.json
├── translations/
├── signature.sig      # RSA-2048 imza
└── checksum.sha256    # Dosya hashleri
```

### Dogrulama Sureci

```
Paket Indir
    |
    v
SHA-256 Hash Kontrol
    |
    v
RSA-2048 Imza Dogrula
    |
    v
Manifest Kontrol
    |
    v
Kuruluma Izin Ver
```

### Guvenli Indirme

- HTTPS zorunlu
- Certificate pinning
- Timeout ve retry politikalari

---

## Audit Logging

### Loglanan Islemler

| Islem | Detaylar |
|-------|----------|
| Paket indirme | Paket ID, kaynak, boyut |
| Patch uygulama | Oyun ID, dosyalar, sonuc |
| Geri alma | Yedek ID, dosyalar |
| Ayar degisikligi | Anahtar, eski/yeni deger |
| Hata | Tip, mesaj, stack trace |

### Log Formati

```json
{
  "timestamp": "2026-02-03T14:30:00Z",
  "event": "package_install",
  "severity": "info",
  "data": {
    "package_id": "pkg_123",
    "game_id": "game_456",
    "result": "success"
  }
}
```

### Log Konumu

```
%LOCALAPPDATA%/MakineAI/logs/
├── makineai.log        # Ana log
├── audit.log           # Guvenlik logu
└── error.log           # Hata logu
```

---

## Veri Gizliligi

### Toplanan Veriler

**Minimum veri politikasi:**

| Veri | Toplaniyor mu | Amac |
|------|---------------|------|
| Oyun kutuphanesi | Hayir | - |
| Kisisel bilgiler | Hayir | - |
| Kullanim istatistikleri | Opsiyonel | Iyilestirme |
| Hata raporlari | Opsiyonel | Bug fix |

### Veri Depolama

Yerel veriler:
- SQLite veritabani (sifrelenmis)
- Ayar dosyalari (JSON)
- Cache dosyalari

### Veri Silme

Kaldirma sirasinda:
- Tum MakineAI verileri silinir
- Oyun yedekleri kullaniciya birakilir

---

## Sandbox Modeli

### Izole Islemler

Riskli islemler izole ortamda:
- Arsiv cikarma
- Script calistirma
- Dosya parse

### Kaynak Limitleri

| Kaynak | Limit |
|--------|-------|
| RAM | 500 MB max |
| CPU | Dusuk oncelik |
| Disk | Cache limiti |
| Network | Rate limiting |

---

## Anti-Cheat Uyumlulugu

### Tespit Edilen Sistemler

| Sistem | Tespit | Aksiyon |
|--------|--------|---------|
| EasyAntiCheat | Evet | Uyari goster |
| BattlEye | Evet | Uyari goster |
| Vanguard | Evet | Engelle |
| PunkBuster | Evet | Uyari goster |

### Kullanici Uyarisi

Anti-cheat tespit edildiginde:
```
! UYARI: Bu oyunda anti-cheat sistemi tespit edildi.
  Online modda ceviri kullanmak hesabinizin
  yasaklanmasina neden olabilir.

  [Yine de devam et] [Iptal]
```

---

## Zafiyet Raporlama

### Guvenlik Acigi Bildirimi

Bir guvenlik acigi buldunuz mu?

1. **Email:** security@makineai.com
2. **Konu:** [SECURITY] Kisa aciklama
3. **Icerik:**
   - Aciklamanin detayi
   - Tekrar etme adimlari
   - Potansiyel etki

### Sorunlu Ifsa Politikasi

- Bildirimi aldiktan sonra 48 saat icinde onay
- 90 gun icinde duzeltme hedefi
- Duzeltme sonrasi koordineli aciklama

---

## Guvenlik Kontrol Listesi

### Kurulum Oncesi

- [ ] Resmi kaynaktan indirildi mi?
- [ ] Dosya hash'i dogrulandi mi?
- [ ] Antivirus tarandi mi?

### Kullanim Sirasinda

- [ ] Yedekleme aktif mi?
- [ ] Audit log acik mi?
- [ ] Anti-cheat uyarilari kontrol edildi mi?

### Kaldirma Sirasinda

- [ ] Yedekler korunacak mi?
- [ ] Veriler silinecek mi?

---

## Referanslar

- [OWASP Top 10](https://owasp.org/Top10/)
- [CWE/SANS Top 25](https://cwe.mitre.org/top25/)
