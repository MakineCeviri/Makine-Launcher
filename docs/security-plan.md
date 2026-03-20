# Makine-Launcher Kapsamli Guvenlik Iyilestirme Plani
# =============================================
# Tarih: 2026-02-12
# Durum: Onay bekliyor (plan mode)
# Onceki tamamlanan isler: Kod temizligi (32K satir), dokuman guncelleme,
#   kod kalitesi plani (6 faz tamamlandi), dosya isimlendirme, README yeniden yazildi
# =============================================

## OZET

Toplam 12 dosya, ~200-250 satir degisiklik. 6 faz.
Build dogrulama: her fazda `just dev` ile.
UI tasarimina DOKUNULMAYACAK.

Core kutuphane (`core/`) zaten zengin guvenlik altyapisina sahip:
- PathValidator, validation.hpp, path_utils.hpp, sandbox.hpp
- credential_store.hpp (Windows Credential Manager)
- security_manager.cpp (OpenSSL + libsodium, Authenticode)
AMA QML katmani (qml/) UI_ONLY modunda oldugu icin core'a bagli degil.
Bu plan QML katmanindaki aciklari kapatir + build hardening yapar.

---

## FAZ 1: Build Hardening — CMake Guvenlik Bayraklari

### 1a. `qml/CMakeLists.txt`

Satir 291 civarinda, `endif()` oncesine eklenecek:

```cmake
# ===== Security Hardening =====
if(MSVC)
    target_compile_options(MakineLauncher PRIVATE
        /GS           # Buffer security check (stack cookies)
        /guard:cf     # Control Flow Guard
        /sdl          # Additional security checks
        /DYNAMICBASE  # ASLR
        /NXCOMPAT     # DEP (Data Execution Prevention)
    )
    target_link_options(MakineLauncher PRIVATE
        /DYNAMICBASE          # ASLR
        /NXCOMPAT             # DEP
        /HIGHENTROPYVA        # 64-bit ASLR
        /CETCOMPAT            # Intel CET shadow stack
    )
elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MakineLauncher PRIVATE
        -fstack-protector-strong      # Stack smashing protection
        -D_FORTIFY_SOURCE=2           # Buffer overflow detection
        -Wformat -Wformat-security    # Format string warnings
    )
    target_link_options(MakineLauncher PRIVATE
        -Wl,--dynamicbase             # ASLR
        -Wl,--nxcompat                # DEP
        -Wl,--high-entropy-va         # 64-bit ASLR
    )
endif()
```

### 1b. `core/CMakeLists.txt`

Satir 184 sonrasi (add_library sonrasi). Ayni blok, target adi `makine_core`.

### 1c. QML Release Hardening — `qml/src/main.cpp`

main() icinde, QGuiApplication olusturulmadan ONCE:
```cpp
#ifdef NDEBUG
    qputenv("QT_QML_NO_DEBUGGER", "1");
    qputenv("QML_DISABLE_DISK_CACHE", "0");
#endif
```

---

## FAZ 2: Path Traversal Korumasi (KRITIK)

### 2a. YENI dosya: `qml/src/services/pathsecurity.h`

CMakeLists.txt'te BACKEND_HEADERS listesine eklenmeli: `src/services/pathsecurity.h`

```cpp
#pragma once
#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace makine::security {

// Resolved path'in base directory icinde kaldigini kontrol et
inline bool isPathContained(const QString& basePath, const QString& fullPath) {
    QString canonBase = QFileInfo(basePath).canonicalFilePath();
    QString canonFull = QFileInfo(fullPath).canonicalFilePath();
    if (canonBase.isEmpty() || canonFull.isEmpty()) {
        canonBase = QDir::cleanPath(basePath);
        canonFull = QDir::cleanPath(fullPath);
    }
    if (!canonBase.endsWith('/') && !canonBase.endsWith('\\'))
        canonBase += '/';
    return canonFull.startsWith(canonBase) ||
           canonFull == canonBase.chopped(1);
}

// base + relative path'i guvenli birlestir, escape tespit ederse bos dondur
inline QString safePathJoin(const QString& basePath, const QString& relativePath) {
    if (relativePath.contains("..") || relativePath.startsWith('/') ||
        relativePath.startsWith('\\') || relativePath.contains("://") ||
        relativePath.contains(QChar(0)))
        return {};
    QString joined = QDir::cleanPath(basePath + '/' + relativePath);
    if (!isPathContained(basePath, joined)) {
        qWarning() << "Path escape blocked:" << relativePath;
        return {};
    }
    return joined;
}

// Kullanici girdisi path'inin guvenli oldugunu dogrula
inline bool isPathSafe(const QString& path) {
    if (path.isEmpty()) return false;
    if (path.contains("..")) return false;
    if (path.contains(QChar(0))) return false;
    if (path.startsWith("\\\\") || path.startsWith("//")) return false;
    return true;
}

} // namespace makine::security
```

### 2b. `localpackagemanager.cpp` — Install traversal fix (KRITIK)

Dosya: `qml/src/services/localpackagemanager.cpp`
Include ekle: `#include "pathsecurity.h"`

Satir 346-347 (for dongusu icinde):
```cpp
// ONCE:
QString destPath = gamePath + "/" + relPath;

// SONRA:
QString destPath = security::safePathJoin(gamePath, relPath);
if (destPath.isEmpty()) {
    qWarning() << "Path traversal blocked in package:" << relPath;
    errors++;
    continue;
}
```

### 2c. `localpackagemanager.cpp` — Uninstall traversal fix (KRITIK)

Satir 410-411:
```cpp
// ONCE:
QString fullPath = basePath + "/" + relPath;

// SONRA:
QString fullPath = security::safePathJoin(basePath, relPath);
if (fullPath.isEmpty()) {
    qWarning() << "Path traversal blocked during uninstall:" << relPath;
    failed++;
    continue;
}
```

### 2d. `backupmanager.cpp` — Restore traversal fix (KRITIK)

Dosya: `qml/src/services/backupmanager.cpp`
Include ekle: `#include "pathsecurity.h"`

Satir 228-229:
```cpp
// ONCE:
const QString destFile = restoreDir + "/" + relativePath;

// SONRA:
const QString destFile = security::safePathJoin(restoreDir, relativePath);
if (destFile.isEmpty()) {
    qWarning() << "Path traversal blocked during restore:" << relativePath;
    continue;
}
```

---

## FAZ 3: Input Validation

### 3a. VDF Parser — Recursion depth + file size limit

Dosya: `qml/src/services/vdfparser.h`

Satir 79 (parseKeyValues fonksiyonu):
```cpp
// ONCE:
inline void parseKeyValues(const std::string& s, size_t& pos, Node& node) {

// SONRA:
static constexpr int kMaxRecursionDepth = 32;
static constexpr size_t kMaxVdfFileSize = 10 * 1024 * 1024; // 10 MB

inline void parseKeyValues(const std::string& s, size_t& pos, Node& node, int depth = 0) {
    if (depth > kMaxRecursionDepth) return;
```

Satir 93 (recursive call):
```cpp
// ONCE:
parseKeyValues(s, pos, child);

// SONRA:
parseKeyValues(s, pos, child, depth + 1);
```

Dosya: `qml/src/services/corebridge.cpp` (satir ~144):
```cpp
// ONCE:
std::string vdfContent = vdfFile.readAll().toStdString();

// SONRA:
QByteArray rawContent = vdfFile.readAll();
if (rawContent.size() > static_cast<qsizetype>(makine::vdf::detail::kMaxVdfFileSize)) {
    qWarning() << "VDF file too large, skipping:" << vdfPath;
    return;
}
std::string vdfContent = rawContent.toStdString();
```

### 3b. GameService — steamAppId numeric validation

Dosya: `qml/src/services/gameservice.cpp`
Satir ~378 (fetchSteamDetails icinde, URL olusturmadan once):
```cpp
static const QRegularExpression numericOnly("^\\d{1,10}$");
if (!numericOnly.match(steamAppId).hasMatch()) {
    qWarning() << "Invalid steamAppId format:" << steamAppId;
    emit steamDetailsFetchError(steamAppId, tr("Invalid Steam App ID"));
    return;
}
```

### 3c. GameService — Network response size limit

Satir ~385 (reply olusturulduktan sonra):
```cpp
static constexpr qint64 kMaxResponseBytes = 5 * 1024 * 1024;
connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
    if (received > kMaxResponseBytes) {
        qWarning() << "Steam API response too large, aborting";
        reply->abort();
    }
});
```

### 3d. GameService — addManualGame path hardening

Include ekle: `#include "pathsecurity.h"`
addManualGame() basinda mevcut kontrollerden ONCE:
```cpp
if (!security::isPathSafe(path)) {
    emit scanError(tr("Guvenli olmayan oyun klasoru yolu: %1").arg(path));
    return;
}
```

---

## FAZ 4: Settings Security — DPAPI Encryption

### 4a. `settingsmanager.h` — Deklarasyonlar

Private bolume ekle:
```cpp
static QByteArray protectData(const QByteArray& plaintext);
static QByteArray unprotectData(const QByteArray& encrypted);
```

### 4b. `settingsmanager.cpp` — DPAPI implementasyonu

Include ekle:
```cpp
#ifdef Q_OS_WIN
#include <Windows.h>
#include <dpapi.h>
#endif
```

Fonksiyonlar:
```cpp
QByteArray SettingsManager::protectData(const QByteArray& plaintext) {
#ifdef Q_OS_WIN
    DATA_BLOB input{
        static_cast<DWORD>(plaintext.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()))
    };
    DATA_BLOB output{};
    if (CryptProtectData(&input, L"MakineLauncher", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        QByteArray result(reinterpret_cast<const char*>(output.pbData),
                          static_cast<int>(output.cbData));
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }
#else
    Q_UNUSED(plaintext)
#endif
    return plaintext;
}

QByteArray SettingsManager::unprotectData(const QByteArray& encrypted) {
#ifdef Q_OS_WIN
    DATA_BLOB input{
        static_cast<DWORD>(encrypted.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))
    };
    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        QByteArray result(reinterpret_cast<const char*>(output.pbData),
                          static_cast<int>(output.cbData));
        SecureZeroMemory(output.pbData, output.cbData);
        LocalFree(output.pbData);
        return result;
    }
#else
    Q_UNUSED(encrypted)
#endif
    return encrypted;
}
```

### 4c. loadSettings/saveSettings degisiklikleri

saveSettings():
```cpp
m_settings.setValue("paths/translationData_enc",
    protectData(m_translationDataPath.toUtf8()).toBase64());
m_settings.remove("paths/translationData"); // eski plaintext'i temizle
```

loadSettings():
```cpp
QByteArray encPath = QByteArray::fromBase64(
    m_settings.value("paths/translationData_enc").toByteArray());
if (!encPath.isEmpty()) {
    m_translationDataPath = QString::fromUtf8(unprotectData(encPath));
} else {
    // Migration: eski plaintext'ten oku
    m_translationDataPath = m_settings.value("paths/translationData",
        "C:/cedra/translation_data/mc-main").toString();
}
```

---

## FAZ 5: Memory Security + Minor Fixes

### 5a. `systemtraymanager.cpp` — memset → aggregate init

```cpp
// ONCE (satir 149):
memset(&m_nid, 0, sizeof(m_nid));

// SONRA:
m_nid = NOTIFYICONDATAW{};
```

---

## FAZ 6: Dogrulama

1. `just dev` → build basarili (MinGW UI-only)
2. Uygulamayi calistir → ana ekran yuklensin
3. Console'da binding/runtime hatasi olmasin
4. Manuel test: oyun tarama, paket kurulum, yedekleme
5. `git diff --stat` → beklenen dosya listesi

---

## DOSYA OZETI

| # | Dosya | Degisiklik | Seviye |
|---|-------|------------|--------|
| 1 | `qml/CMakeLists.txt` | Security compiler/linker flags | Orta |
| 2 | `core/CMakeLists.txt` | Security compiler/linker flags | Orta |
| 3 | `qml/src/main.cpp` | QML debugger disable (release) | Dusuk |
| 4 | **`qml/src/services/pathsecurity.h`** | **YENI: path traversal utilities** | **Kritik** |
| 5 | **`qml/src/services/localpackagemanager.cpp`** | **Install + uninstall traversal fix** | **Kritik** |
| 6 | **`qml/src/services/backupmanager.cpp`** | **Restore traversal fix** | **Kritik** |
| 7 | `qml/src/services/vdfparser.h` | Recursion depth + size limit | Yuksek |
| 8 | `qml/src/services/corebridge.cpp` | VDF file size check | Yuksek |
| 9 | `qml/src/services/gameservice.cpp` | steamAppId validation + response limit + path check | Yuksek |
| 10 | `qml/src/services/settingsmanager.h` | DPAPI function declarations | Orta |
| 11 | `qml/src/services/settingsmanager.cpp` | DPAPI encrypt/decrypt + migration | Orta |
| 12 | `qml/src/services/systemtraymanager.cpp` | Aggregate init | Dusuk |

---

## CORE PLACEHOLDER DURUMU

Asagidaki dosyalardaki placeholder'lar KASITLI olarak birakiliyor:
- `core/src/security/security_manager.cpp:89-101` — Placeholder RSA public key
  - `EMBEDDED_KEY_IS_REAL = false` guard'i release build'de zaten hata donduruyor
- `core/include/makine/ssl_pinning.hpp:50-70` — Placeholder certificate pins
  - Sunucu altyapisi hazir oldugunda gercek degerlerle degistirilecek
- `core/src/package_builder/package_builder.cpp:1006` — Placeholder signature

Bu degerler sunucu altyapisi (api.makineceviri.org, cdn.makineceviri.org) hazir oldugunda
gercek sertifika ve anahtar degerleriyle degistirilecek.

---

## GUVENLIK AUDIT SONUCLARI (REFERANS)

### Bulunan Aciklar

| Seviye | Dosya | Sorun | Cozum |
|--------|-------|-------|-------|
| KRITIK | localpackagemanager.cpp:346 | ZIP Slip — install | Faz 2b |
| KRITIK | localpackagemanager.cpp:410 | Path traversal — uninstall | Faz 2c |
| KRITIK | backupmanager.cpp:227 | Path traversal — restore | Faz 2d |
| YUKSEK | vdfparser.h:79 | Recursion depth yok (DoS) | Faz 3a |
| YUKSEK | corebridge.cpp:144 | VDF dosya boyutu limiti yok | Faz 3a |
| YUKSEK | gameservice.cpp:381 | steamAppId URL injection | Faz 3b |
| ORTA | gameservice.cpp:397 | Response size limiti yok | Faz 3c |
| ORTA | settingsmanager.cpp:252 | Plaintext hassas veri | Faz 4 |
| ORTA | CMakeLists.txt (her iki) | Guvenlik bayraklari eksik | Faz 1 |
| DUSUK | systemtraymanager.cpp:149 | memset yerine aggregate init | Faz 5 |
| DUSUK | main.cpp | QML debugger release'de acik | Faz 1c |

### Mevcut Guvenlik Altyapisi (Core — zaten implementeli)

| Dosya | Icerik |
|-------|--------|
| `core/include/makine/security.hpp` | PathValidator, PathGuard, SecurityManager, IntegrityChecker |
| `core/include/makine/validation.hpp` | Path/string/ID/URL/hash validators |
| `core/include/makine/path_utils.hpp` | Traversal detection, sanitization, safe join |
| `core/include/makine/sandbox.hpp` | SandboxPolicy, SandboxContext, ScopedSandbox, RAII |
| `core/include/makine/credential_store.hpp` | Windows Credential Manager |
| `core/include/makine/ssl_pinning.hpp` | Certificate pinning (placeholder pins) |
| `core/src/security/security_manager.cpp` | OpenSSL + libsodium, Authenticode |
| `core/src/security/credential_store.cpp` | CredWrite/CredRead |
| `core/src/security/ssl_pinning.cpp` | CURL pin application |
| `core/src/security/sandbox.cpp` | Sandbox context implementation |

---

## BUILD KOMUTLARI (REFERANS)

```bash
# PATH ayarlama (MinGW):
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Program Files/Git/usr/bin:$PATH"

# Build:
just dev  # veya: cmake --preset dev && cmake --build --preset dev

# Run (Qt DLL'leri PATH'te olmali):
export PATH="/c/Qt/6.10.1/mingw_64/bin:$PATH"
```

---

## NOTLAR

- Commit'lerde Co-Authored-By EKLEME — kullanicinin kendi adina olmali
- Kod yorumlari Ingilizce, kullanici iletisimi Turkce
- Repo: PRIVATE (github.com/MakineCeviri/Makine-Launcher)
