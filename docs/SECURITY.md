# MakineAI Guvenlik Dokumantasyonu

**Tarih:** 2026-01-20
**Versiyon:** 1.0

---

## 1. Guvenlik Mimarisi Genel Bakis

### 1.1 Guvenlik Katmanlari

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        MakineAI SECURITY LAYERS                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Layer 1: APPLICATION SECURITY                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ - Code signing (Authenticode)                                    │   │
│  │ - Self-integrity check                                           │   │
│  │ - Anti-tampering                                                 │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 2: PACKAGE SECURITY                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ - RSA-2048 signature verification                                │   │
│  │ - SHA256 hash validation                                         │   │
│  │ - Package integrity manifest                                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 3: INPUT VALIDATION                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ - Path traversal protection                                      │   │
│  │ - Null byte injection prevention                                 │   │
│  │ - UNC path blocking                                              │   │
│  │ - Symlink attack protection                                      │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 4: RUNTIME SECURITY                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ - Anti-cheat detection                                           │   │
│  │ - Backup/restore mechanism                                       │   │
│  │ - Atomic operations                                              │   │
│  │ - Version tracking                                               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  Layer 5: NETWORK SECURITY                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │ - HTTPS-only connections                                         │   │
│  │ - Certificate pinning                                            │   │
│  │ - No execution of downloaded code                                │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Kriptografik Islemler

### 2.1 Hash Algoritmalari

| Algoritma | Kullanim | Durum |
|-----------|----------|-------|
| SHA-256 | Paket dogrulama, dosya butunlugu | Varsayilan |
| SHA-384 | Ekstra guvenlik gerektiren durumlar | Destekleniyor |
| SHA-512 | Maksimum guvenlik | Destekleniyor |
| MD5 | Sadece legacy uyumluluk | Onerili degil |

**Kod Ornegi:**

```cpp
// SecurityManager kullanimi
auto& security = Core::instance().securityManager();

// Dosya hash'i hesapla
auto hash = security.hashFile("game.exe", HashAlgorithm::SHA256);
if (hash) {
    std::cout << "Hash: " << *hash << std::endl;
}

// Veri hash'i dogrula
bool valid = security.verifyHash(data, expectedHash, HashAlgorithm::SHA256);
```

### 2.2 RSA Imza Dogrulama

MakineAI ceviri paketlerini RSA-2048 ile imzalar ve dogrular.

**Imza Akisi:**

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Package   │────▶│  SHA-256    │────▶│   RSA-2048  │
│   Content   │     │    Hash     │     │   Sign/Verify│
└─────────────┘     └─────────────┘     └─────────────┘
```

**Kod Ornegi:**

```cpp
// Public key yukle
security.loadPublicKeyPEM(R"(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...
-----END PUBLIC KEY-----
)");

// Paket imzasini dogrula
PackageSignature sig;
sig.packageHash = "sha256:abc123...";
sig.signature = "base64_signature...";
sig.publicKeyId = "makineai-signing-key-v1";

auto result = security.verifyPackageSignature(packagePath, sig);
if (result && result->valid) {
    std::cout << "Paket guvenli!" << std::endl;
}
```

### 2.3 Windows Authenticode

Uygulama EXE'si Authenticode ile imzalanir.

```cpp
// EXE imzasini dogrula
auto result = security.verifyAuthenticode("MakineAI.exe");
if (result && result->valid) {
    std::cout << "Imzali: " << result->signedBy << std::endl;
}
```

---

## 3. Yol Guvenlik Dogrulamasi

### 3.1 Path Traversal Korunmasi

MakineAI, path traversal saldirilarindan koruma saglar:

| Saldiri Turu | Ornek | Koruma |
|--------------|-------|--------|
| Dogrudan traversal | `../../../etc/passwd` | `hasTraversal()` |
| URL encoded | `%2e%2e%2f` | URL decode + kontrol |
| Null byte injection | `file.txt\0.exe` | `hasNullBytes()` |
| UNC path | `\\server\share` | `isUncPath()` |
| Symlink | `symlink -> /etc` | Opsiyonel cozum |

**Kod Ornegi:**

```cpp
// Guvenli yol dogrulama
PathValidationOptions options;
options.allowRelative = false;
options.allowUncPaths = false;
options.allowTraversal = false;
options.allowNullBytes = false;
options.allowedDirs = {gameDir, backupDir};

auto result = PathValidator::validate(userInput, options);
if (result.valid) {
    // Guvenle kullan
    processFile(result.sanitizedPath);
} else {
    spdlog::error("Gecersiz yol: {}", result.reason);
}
```

### 3.2 PathGuard RAII Kullanimi

```cpp
// RAII guard ile guvenli yol erisimi
if (PathGuard guard{userPath, options}; guard.ok()) {
    // guard.path() guvenli
    readFile(guard.path());
} else {
    spdlog::error("Yol hatasi: {}", guard.reason());
}
```

### 3.3 Guvenli Path Birlestirme

```cpp
// Guvenli path birlestirme (escape onleme)
auto safePath = PathValidator::joinSafe(baseDir, relativeInput);
if (safePath) {
    // Guvenli kullanim
} else {
    // Tehlikeli yol girisi
}
```

---

## 4. Paket Butunluk Sistemi

### 4.1 Manifest Yapisi

```json
{
  "files": [
    {
      "path": "BepInEx/Translation/tr/Text/_AutoGeneratedTranslations.txt",
      "hash": "sha256:abc123...",
      "size": 1234567,
      "modified": 1705766400000
    }
  ]
}
```

### 4.2 IntegrityChecker Kullanimi

```cpp
// Manifest olustur
StringList files = {"file1.txt", "file2.dll"};
auto manifest = IntegrityChecker::createManifest(baseDir, files);

// Manifest kaydet
IntegrityChecker::saveManifest("manifest.json", *manifest);

// Butunluk dogrula
StringList modifiedFiles;
auto valid = IntegrityChecker::verify(baseDir, *manifest, &modifiedFiles);

if (!valid) {
    for (const auto& file : modifiedFiles) {
        spdlog::warn("Degistirilmis dosya: {}", file);
    }
}
```

---

## 5. Anti-Cheat Uyumluluk

### 5.1 Tespit Edilen Anti-Cheat Sistemleri

| Anti-Cheat | Durum | MakineAI Davranisi |
|------------|-------|-------------------|
| Easy Anti-Cheat | ENGELLENDI | Kurulum reddedilir |
| BattlEye | ENGELLENDI | Kurulum reddedilir |
| VAC | RISKLI | Kullanici uyarilir |
| Vanguard | ENGELLENDI | Kurulum reddedilir |
| Ricochet | ENGELLENDI | Kurulum reddedilir |
| PunkBuster | ENGELLENDI | Kurulum reddedilir |
| Denuvo (DRM) | GUVENLI | Normal kurulum |

### 5.2 Algılama Mekanizmasi

```cpp
// Anti-cheat tespiti
AntiCheatStatus status = detectAntiCheat(gameInfo);

switch (status) {
    case AntiCheatStatus::BLOCKED:
        showError("Bu oyun anti-cheat korumali. Ceviri yuklenemez.");
        return;

    case AntiCheatStatus::RISKY:
        if (!showWarning("Risk uyarisi", "Devam etmek istiyor musunuz?")) {
            return;
        }
        break;

    case AntiCheatStatus::SINGLEPLAYER:
        showInfo("Sadece singleplayer modda kullanin.");
        break;

    case AntiCheatStatus::SAFE:
        // Normal devam
        break;
}
```

---

## 6. Yedekleme ve Geri Yukleme

### 6.1 Atomik Islemler

MakineAI tum yama islemlerini atomik olarak gerceklestirir:

```
┌─────────────────────────────────────────────────────────────────┐
│                    ATOMIC PATCH OPERATION                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. PREPARE                                                     │
│     └── Disk alani kontrolu                                     │
│     └── Dosya erisim kontrolu                                   │
│                                                                 │
│  2. BACKUP                                                      │
│     └── Orijinal dosyalari yedekle                              │
│     └── Manifest olustur                                        │
│                                                                 │
│  3. APPLY                                                       │
│     └── Yama dosyalarini kopyala                                │
│     └── Her adimda hata kontrolu                                │
│                                                                 │
│  4. VERIFY                                                      │
│     └── Kurulmus dosyalari dogrula                              │
│                                                                 │
│  5. COMMIT / ROLLBACK                                           │
│     └── Basariliysa: Yedekleri sakla                            │
│     └── Basarisizsa: Yedeklerden geri yukle                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 Yedek Yapisi

```
%LOCALAPPDATA%\MakineAI\data\backups\
├── steam_1245620_v1.12.0/           # Oyun ID + Versiyon
│   ├── manifest.json                 # Yedek bilgileri
│   ├── backup_info.json              # Yama bilgileri
│   └── files/
│       └── original_files...         # Orijinal dosyalar
```

---

## 7. Guvenli Indirme

### 7.1 HTTPS Zorunlulugu

Tum ag baglantilari HTTPS uzerinden yapilir:

```cpp
// curl ile HTTPS zorunlulugu
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

// HTTP'yi HTTPS'e yonlendir
// HTTP baglantisi reddedilir
```

### 7.2 Indirme Sonrasi Dogrulama

```cpp
// Indirilen paket dogrulama
auto downloadedHash = security.hashFile(downloadedPackage);
if (*downloadedHash != expectedHash) {
    fs::remove(downloadedPackage);
    return Error("Hash mismatch - download corrupted");
}

// Imza dogrulama
auto sigResult = security.verifyPackageSignature(downloadedPackage, signature);
if (!sigResult || !sigResult->valid) {
    fs::remove(downloadedPackage);
    return Error("Invalid signature");
}
```

---

## 8. Kod Imzalama (Code Signing)

### 8.1 Authenticode Sertifikasi

MakineAI, Windows Authenticode sertifikasi ile imzalanir:

| Ozellik | Deger |
|---------|-------|
| Sertifika Turu | EV Code Signing |
| Algoritma | SHA-256 |
| Timestamp | RFC 3161 |
| Gecerlilik | 3 yil |

### 8.2 Imzalama Scripti

```powershell
# build/sign.ps1
$cert = Get-ChildItem -Path Cert:\CurrentUser\My -CodeSigningCert
$timestamp = "http://timestamp.digicert.com"

Set-AuthenticodeSignature `
    -FilePath "MakineAI.exe" `
    -Certificate $cert `
    -TimestampServer $timestamp `
    -HashAlgorithm SHA256
```

---

## 9. Guvenlik Kontrol Listesi

### 9.1 Release Oncesi Kontroller

- [ ] Authenticode imza dogrulama
- [ ] Tum bagimliliklar taranmis (CVE)
- [ ] Path validation testleri gecti
- [ ] Paket imzalama sistemi calisir
- [ ] Anti-cheat tespiti dogru
- [ ] HTTPS sertifika dogrulama aktif
- [ ] Yedekleme sistemi test edildi
- [ ] Rollback mekanizmasi test edildi

### 9.2 Guvenlik Testleri

```bash
# Path traversal testleri
./test_security --path-traversal

# Hash dogrulama testleri
./test_security --hash-verify

# Imza dogrulama testleri
./test_security --signature-verify

# Butunluk testleri
./test_security --integrity
```

---

## 10. Bilinen Guvenlik Onlemleri

### 10.1 OWASP Top 10 Uyumluluk

| Risk | Onlem |
|------|-------|
| Injection | Input validation, parametreli sorgular |
| Broken Authentication | N/A (lokal uygulama) |
| Sensitive Data Exposure | Hassas veri saklanmaz |
| XXE | XML parser disabled |
| Broken Access Control | Path validation |
| Security Misconfiguration | Varsayilan guvenli ayarlar |
| XSS | N/A (masaustu uygulama) |
| Insecure Deserialization | JSON parser guvenli |
| Using Components with Known Vulnerabilities | Dependency scanning |
| Insufficient Logging | spdlog ile loglama |

### 10.2 Defense in Depth

```
User Input
    │
    ▼
┌────────────────┐
│ Input          │ <- Ilk savunma katmani
│ Validation     │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Path           │ <- Ikinci savunma katmani
│ Sanitization   │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Access         │ <- Ucuncu savunma katmani
│ Control        │
└───────┬────────┘
        │
        ▼
┌────────────────┐
│ Operation      │ <- Dorduncu savunma katmani
│ Verification   │
└───────┬────────┘
        │
        ▼
    Secure
    Operation
```

---

## 11. Guvenlik Aciklari Bildirimi

Guvenlik aciklari icin:

1. **E-posta:** security@makineai.com
2. **PGP Key:** [Public key ID]
3. **Response Time:** 24-48 saat icinde ilk yanit

**Responsible Disclosure Policy:**
- 90 gun disclosure deadline
- Duzeltme yapildiktan sonra kredi verilir
- Bug bounty programi planlanmaktadir

---

*Bu belge MakineAI projesinin guvenlik mimarisini tanimlar.*
