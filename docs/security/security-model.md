# Güvenlik Modeli

Makine-Launcher güvenlik mimarisi ve politikaları.

> **Durum:** Bu doküman hedef güvenlik mimarisini tanımlar.
> Paket imzalama, sunucu dağıtımı ve HTTPS entegrasyonu henüz implemente edilmemiştir.
> Yedekleme, dosya izinleri, anti-cheat tespiti ve audit logging aktiftir.

---

## Genel Bakış

Makine-Launcher, kullanıcı verilerini ve sistemini korumak için katmanlı güvenlik modeli kullanır.

### Temel İlkeler

1. **Minimum Yetki** - Sadece gerekli izinler istenir
2. **Veri Güvenliği** - Kullanıcı verileri korunur
3. **Doğrulama** - Tüm paketler zorunlu olarak imza doğrulamasından geçer
4. **Şeffaflık** - İşlemler loglanır

---

## Dosya Güvenliği

### Yedekleme

Her patch işleminden önce otomatik yedekleme:

```
[Oyun]/MakineLauncher_Backups/
└── 2026-02-03_14-30-00/
    ├── orijinal_dosya.dat
    ├── manifest.json
    └── checksum.sha256
```

### Dosya İzinleri

Makine-Launcher sadece şu klasörlere yazar:
- Oyun kurulum klasörü (patch için)
- `%APPDATA%/MakineLauncher/` (ayarlar)
- `%LOCALAPPDATA%/MakineLauncher/` (cache, logs)

### Rollback

Herhangi bir sorundan tek tıkla geri dönme:
- Yedekten otomatik geri yükleme
- Dosya bütünlüğü doğrulama
- İşlem loglama

---

## Paket Güvenliği

### İmza Doğrulama (Zorunlu)

Tüm çeviri paketleri imzalanır ve doğrulanır:

```
Paket Yapısı:
├── manifest.json
├── translations/
├── signature.sig      # Ed25519 imza
└── checksum.sha256    # Dosya hashleri
```

### Doğrulama Süreci

```
Paket İndir
    │
    ▼
SHA-256 Hash Kontrol
    │
    ▼
Ed25519 İmza Doğrula (zorunlu)
    │
    ▼
Manifest Kontrol
    │
    ▼
Kuruluma İzin Ver
```

**Not:** İmza doğrulaması zorunludur. Geçerli imzası olmayan paketler reddedilir.

### Güvenli İndirme

- HTTPS zorunlu
- Certificate pinning (Makine-Launcher API endpointleri)
- Timeout ve retry politikaları

---

## Kriptografi

### Kullanılan Algoritmalar

| Amaç | Algoritma |
|------|-----------|
| Paket imzalama | Ed25519 |
| Hash doğrulama | SHA-256, BLAKE2b |
| Veri şifreleme | AES-256-GCM, XChaCha20-Poly1305 |
| Anahtar türetme | Argon2id |
| Yerel veri koruma | Windows DPAPI |

### Anahtar Yönetimi

- Public key uygulama binary'sine gömülüdür (compile-time)
- Credential'lar Windows Credential Manager'da saklanır
- Yerel veritabanı DPAPI ile şifrelenir

---

## Audit Logging

### Loglanan İşlemler

| İşlem | Detaylar |
|-------|----------|
| Paket indirme | Paket ID, kaynak, boyut |
| Patch uygulama | Oyun ID, dosyalar, sonuç |
| Geri alma | Yedek ID, dosyalar |
| Ayar değişikliği | Anahtar, eski/yeni değer |
| İmza doğrulama | Paket ID, sonuç |
| Hata | Tip, mesaj, stack trace |

### Log Formatı

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
%LOCALAPPDATA%/MakineLauncher/logs/
├── makine.log          # Ana log
├── audit.log           # Güvenlik logu
└── error.log           # Hata logu
```

---

## Veri Gizliliği

### Toplanan Veriler

**Minimum veri politikası:**

| Veri | Toplanıyor mu | Amaç |
|------|---------------|------|
| Oyun kütüphanesi | Hayır | - |
| Kişisel bilgiler | Hayır | - |
| Kullanım istatistikleri | Opsiyonel | İyileştirme |
| Hata raporları | Opsiyonel | Bug fix |

### Veri Depolama

Yerel veriler:
- SQLite veritabanı (DPAPI ile şifrelenmiş)
- Ayar dosyaları (JSON)
- Cache dosyaları

### Veri Silme

Kaldırma sırasında:
- Tüm Makine-Launcher verileri silinir
- Oyun yedekleri kullanıcıya bırakılır

---

## Sandbox Modeli

### İzole İşlemler

Riskli işlemler izole ortamda:
- Arşiv çıkarma
- Script çalıştırma
- Dosya parse

### Kaynak Limitleri

| Kaynak | Limit |
|--------|-------|
| RAM | 500 MB max |
| CPU | Düşük öncelik |
| Disk | Cache limiti |
| Network | Rate limiting |

---

## Anti-Cheat Uyumluluğu

### Tespit Edilen Sistemler

| Sistem | Tespit | Aksiyon |
|--------|--------|---------|
| EasyAntiCheat | Evet | Uyarı göster |
| BattlEye | Evet | Uyarı göster |
| Vanguard | Evet | Engelle |
| PunkBuster | Evet | Uyarı göster |

### Kullanıcı Uyarısı

Anti-cheat tespit edildiğinde:
```
⚠ UYARI: Bu oyunda anti-cheat sistemi tespit edildi.
  Online modda çeviri kullanmak hesabınızın
  yasaklanmasına neden olabilir.

  [Yine de devam et] [İptal]
```

---

## Zafiyet Raporlama

### Güvenlik Açığı Bildirimi

Bir güvenlik açığı buldunuz mu?

1. **Email:** security@makineceviri.org
2. **Konu:** [SECURITY] Kısa açıklama
3. **İçerik:**
   - Açıklamanın detayı
   - Tekrar etme adımları
   - Potansiyel etki

### Sorumlu İfşa Politikası

- Bildirimi aldıktan sonra 48 saat içinde onay
- 90 gün içinde düzeltme hedefi
- Düzeltme sonrası koordineli açıklama

---

## Güvenlik Kontrol Listesi

### Kurulum Öncesi

- [ ] Resmi kaynaktan indirildi mi?
- [ ] Dosya hash'i doğrulandı mı?
- [ ] Antivirüs tarandı mı?

### Kullanım Sırasında

- [ ] Yedekleme aktif mi?
- [ ] Audit log açık mı?
- [ ] Anti-cheat uyarıları kontrol edildi mi?

### Kaldırma Sırasında

- [ ] Yedekler korunacak mı?
- [ ] Veriler silinecek mi?

---

## Referanslar

- [OWASP Top 10](https://owasp.org/Top10/)
- [CWE/SANS Top 25](https://cwe.mitre.org/top25/)
