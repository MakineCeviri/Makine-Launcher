# MakineAI Özgün Çeviri Sistemleri

**Tarih:** 2026-01-20
**Versiyon:** 1.0

---

## 1. Genel Mimari

### 1.1 Çeviri Pipeline Genel Görünümü

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        MakineAI TRANSLATION PIPELINE                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐         │
│  │   GAME   │───▶│  ENGINE  │───▶│ STRATEGY │───▶│  PATCH   │         │
│  │ DETECTION│    │ DETECTION│    │ SELECTION│    │ EXECUTION│         │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘         │
│       │               │               │               │                 │
│       ▼               ▼               ▼               ▼                 │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐         │
│  │  Steam   │    │  Unity   │    │ Runtime  │    │  Backup  │         │
│  │  Epic    │    │  Unreal  │    │ File-base│    │  Apply   │         │
│  │  GOG     │    │ Bethesda │    │  Binary  │    │  Verify  │         │
│  │  Manual  │    │ GameMaker│    │          │    │  Rollback│         │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Karar Matrisi

```cpp
// C++ Core: src/patch_engine/strategy_selector.cpp

enum class TranslationStrategy {
    RUNTIME_HOOK,      // BepInEx + XUnity (Unity)
    FILE_REPLACEMENT,  // Direct file swap
    LOCRES_EDIT,       // Unreal .locres modification
    BINARY_PATCH,      // Last resort
    HYBRID            // Combination approach
};

TranslationStrategy selectStrategy(const GameInfo& game) {
    switch (game.engine) {
        case Engine::UNITY_MONO:
        case Engine::UNITY_IL2CPP:
            return TranslationStrategy::RUNTIME_HOOK;

        case Engine::UNREAL_4:
        case Engine::UNREAL_5:
            return TranslationStrategy::LOCRES_EDIT;

        case Engine::BETHESDA:
            return TranslationStrategy::FILE_REPLACEMENT; // .strings files

        case Engine::GAMEMAKER:
            return TranslationStrategy::FILE_REPLACEMENT; // data.win

        case Engine::RPGMAKER_MV:
        case Engine::RPGMAKER_MZ:
            return TranslationStrategy::FILE_REPLACEMENT; // JSON files

        case Engine::RENPY:
            return TranslationStrategy::FILE_REPLACEMENT; // .rpy files

        default:
            return TranslationStrategy::BINARY_PATCH;
    }
}
```

---

## 2. Oyun Motoru Tespit Algoritması

### 2.1 İmza Tabanlı Tespit

```cpp
// C++ Core: include/makineai/game_detector.hpp

struct EngineSignature {
    std::string name;
    std::vector<std::string> file_patterns;
    std::vector<std::string> dll_signatures;
    std::vector<std::string> folder_patterns;
};

const std::vector<EngineSignature> ENGINE_SIGNATURES = {
    // Unity Mono
    {
        "Unity Mono",
        {"UnityPlayer.dll", "mono.dll", "Assembly-CSharp.dll"},
        {"globalgamemanagers", "level*", "sharedassets*.assets"},
        {"*_Data/Managed", "*_Data/Mono"}
    },

    // Unity IL2CPP
    {
        "Unity IL2CPP",
        {"UnityPlayer.dll", "GameAssembly.dll"},
        {"globalgamemanagers", "CAB-*"},
        {"*_Data/il2cpp_data"}
    },

    // Unreal Engine 4/5
    {
        "Unreal Engine",
        {"UE4Game*.exe", "UE5Game*.exe", "*-Win64-Shipping.exe"},
        {"*.pak", "*.locres", "*.uasset"},
        {"Engine/Content", "Content/Paks"}
    },

    // Bethesda Creation Engine
    {
        "Bethesda",
        {"*.ba2", "*.bsa", "*.strings"},
        {"Data/*.esm", "Data/*.esp"},
        {"Data/Strings", "Data/Interface"}
    },

    // GameMaker Studio 2
    {
        "GameMaker",
        {"data.win", "game.unx", "game.ios"},
        {"options.ini"},
        {}
    },

    // RPG Maker MV/MZ
    {
        "RPG Maker MV/MZ",
        {"Game.exe", "nw.exe"},
        {"data/*.json", "js/plugins.js"},
        {"www/data", "www/js"}
    },

    // RPG Maker VX/Ace
    {
        "RPG Maker VX/Ace",
        {"Game.exe", "RGSS*.dll"},
        {"Data/*.rvdata2", "Data/*.rxdata"},
        {"Data", "Graphics"}
    },

    // Ren'Py
    {
        "Ren'Py",
        {"renpy.exe", "lib/python*"},
        {"*.rpy", "*.rpyc"},
        {"game", "renpy"}
    }
};
```

### 2.2 Alt-Motor Tespiti (Unity)

```cpp
// Unity Mono vs IL2CPP ayrımı
enum class UnityBackend {
    MONO,      // Assembly-CSharp.dll mevcut
    IL2CPP     // GameAssembly.dll mevcut
};

UnityBackend detectUnityBackend(const std::filesystem::path& gamePath) {
    auto dataFolder = findDataFolder(gamePath);

    // IL2CPP kontrolü
    if (std::filesystem::exists(dataFolder / "il2cpp_data") ||
        std::filesystem::exists(gamePath / "GameAssembly.dll")) {
        return UnityBackend::IL2CPP;
    }

    // Mono kontrolü
    auto managedPath = dataFolder / "Managed";
    if (std::filesystem::exists(managedPath / "Assembly-CSharp.dll")) {
        return UnityBackend::MONO;
    }

    // Varsayılan olarak IL2CPP (daha yaygın)
    return UnityBackend::IL2CPP;
}
```

### 2.3 Unreal Engine Versiyon Tespiti

```cpp
// Unreal 4 vs 5 ayrımı
enum class UnrealVersion {
    UE4,
    UE5,
    UNKNOWN
};

UnrealVersion detectUnrealVersion(const std::filesystem::path& gamePath) {
    // .pak dosyası magic number kontrolü
    auto pakFiles = glob(gamePath, "**/*.pak");

    for (const auto& pakFile : pakFiles) {
        auto magic = readBytes(pakFile, 0, 4);

        // UE5 yeni format
        if (magic == "\x5F\x00\x00\x00") {
            return UnrealVersion::UE5;
        }

        // UE4 klasik format
        if (magic == "\x00\x00\x00\x00") {
            return UnrealVersion::UE4;
        }
    }

    // EXE adından tespit
    for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
        auto filename = entry.path().filename().string();
        if (filename.find("UE5") != std::string::npos) {
            return UnrealVersion::UE5;
        }
        if (filename.find("UE4") != std::string::npos) {
            return UnrealVersion::UE4;
        }
    }

    return UnrealVersion::UNKNOWN;
}
```

---

## 3. Çeviri Stratejileri

### 3.1 Runtime Hook (Unity)

**Açıklama:** BepInEx mod loader + XUnity.AutoTranslator kullanarak oyun çalışırken metin çevirisi.

```
┌─────────────────────────────────────────────────────────────────┐
│                    UNITY RUNTIME HOOK FLOW                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Game Start                                                     │
│      │                                                          │
│      ▼                                                          │
│  ┌──────────┐                                                   │
│  │ BepInEx  │ ── Doorstop hook (winhttp.dll)                   │
│  │  Loader  │                                                   │
│  └────┬─────┘                                                   │
│       │                                                         │
│       ▼                                                         │
│  ┌──────────┐                                                   │
│  │ XUnity   │ ── Text component hook                           │
│  │ AutoTrans│ ── UI.Text, TextMeshPro, NGUI, etc.              │
│  └────┬─────┘                                                   │
│       │                                                         │
│       ▼                                                         │
│  ┌──────────┐                                                   │
│  │ MakineAI │ ── Custom translation loader                     │
│  │ Plugin   │ ── Turkish character support                     │
│  │          │ ── Font replacement                               │
│  └──────────┘                                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Kurulum Dosya Yapısı:**

```
GameFolder/
├── BepInEx/
│   ├── core/
│   │   ├── BepInEx.dll
│   │   ├── BepInEx.Preloader.dll
│   │   └── 0Harmony.dll
│   ├── plugins/
│   │   └── XUnity.AutoTranslator/
│   │       ├── XUnity.AutoTranslator.Plugin.dll
│   │       └── XUnity.Common.dll
│   ├── config/
│   │   └── AutoTranslatorConfig.ini
│   └── Translation/
│       └── tr/
│           └── Text/
│               └── _AutoGeneratedTranslations.txt
├── doorstop_config.ini
└── winhttp.dll
```

**MakineAI Özelleştirmesi:**

```ini
; BepInEx/config/AutoTranslatorConfig.ini
[Service]
Endpoint=CustomTranslation

[General]
Language=tr

[TextFrameworks]
EnableUGUI=True
EnableTextMeshPro=True
EnableNGUI=True

[MakineAI]
; Özel MakineAI ayarları
UseLocalPackage=True
PackagePath=BepInEx/Translation/tr
AutoUpdateTranslation=True
TurkishFontSupport=True
```

### 3.2 File Replacement (Bethesda, GameMaker, RPG Maker)

**Açıklama:** Orijinal dosyaları yedekleyip çevrilmiş versiyonlarla değiştirme.

```cpp
// C++ Core: src/patch_engine/file_replacement.cpp

class FileReplacementPatcher {
public:
    Result<void> apply(const PatchPackage& package) {
        // 1. Yedekleme
        for (const auto& file : package.files) {
            auto originalPath = gamePath_ / file.relativePath;
            auto backupPath = backupDir_ / file.relativePath;

            if (std::filesystem::exists(originalPath)) {
                std::filesystem::create_directories(backupPath.parent_path());
                std::filesystem::copy(originalPath, backupPath);
                backupManifest_.add(file.relativePath, hashFile(originalPath));
            }
        }

        // 2. Yama uygulama
        for (const auto& file : package.files) {
            auto targetPath = gamePath_ / file.relativePath;
            std::filesystem::create_directories(targetPath.parent_path());

            if (file.type == PatchType::REPLACE) {
                std::filesystem::copy(file.patchedContent, targetPath);
            } else if (file.type == PatchType::MERGE) {
                mergeFile(targetPath, file.patchedContent);
            }
        }

        // 3. Manifest kaydet
        saveBackupManifest();

        return Ok();
    }

    Result<void> restore() {
        for (const auto& entry : backupManifest_.entries) {
            auto originalPath = gamePath_ / entry.relativePath;
            auto backupPath = backupDir_ / entry.relativePath;

            std::filesystem::copy(backupPath, originalPath,
                std::filesystem::copy_options::overwrite_existing);
        }

        return Ok();
    }
};
```

### 3.3 Locres Edit (Unreal Engine)

**Açıklama:** Unreal Engine .locres dosyalarını düzenleyerek çeviri.

```cpp
// C++ Core: include/formats/unreal_locres.hpp

struct LocresEntry {
    std::string namespaceKey;
    std::string key;
    std::string sourceString;
    std::string localizedString;
};

class UnrealLocresParser {
public:
    Result<std::vector<LocresEntry>> parse(const std::filesystem::path& locresPath) {
        auto data = readFile(locresPath);

        // .locres format:
        // Magic: 0x0E14DA4A (little-endian)
        // Version: uint8
        // StringCount: int32
        // Entries...

        if (data.size() < 8) {
            return Error("Invalid locres file");
        }

        auto magic = readUint32(data, 0);
        if (magic != 0x0E14DA4A) {
            return Error("Invalid locres magic");
        }

        auto version = data[4];
        auto stringCount = readInt32(data, 5);

        std::vector<LocresEntry> entries;
        size_t offset = 9;

        for (int i = 0; i < stringCount; i++) {
            LocresEntry entry;

            // Read namespace
            auto nsLen = readInt32(data, offset);
            offset += 4;
            entry.namespaceKey = readString(data, offset, nsLen);
            offset += nsLen;

            // Read key
            auto keyLen = readInt32(data, offset);
            offset += 4;
            entry.key = readString(data, offset, keyLen);
            offset += keyLen;

            // Read source string
            auto srcLen = readInt32(data, offset);
            offset += 4;
            entry.sourceString = readString(data, offset, srcLen);
            offset += srcLen;

            entries.push_back(entry);
        }

        return entries;
    }

    Result<void> write(const std::filesystem::path& locresPath,
                       const std::vector<LocresEntry>& entries) {
        std::vector<uint8_t> data;

        // Write header
        writeUint32(data, 0x0E14DA4A);  // Magic
        data.push_back(0x02);            // Version
        writeInt32(data, entries.size()); // String count

        // Write entries
        for (const auto& entry : entries) {
            writeString(data, entry.namespaceKey);
            writeString(data, entry.key);
            writeString(data, entry.localizedString.empty()
                        ? entry.sourceString
                        : entry.localizedString);
        }

        return writeFile(locresPath, data);
    }
};
```

**Unreal Pak İşlemleri:**

```cpp
// .pak dosyası extract/repack işlemleri
class UnrealPakManager {
public:
    Result<void> extractLocres(const std::filesystem::path& pakPath,
                               const std::filesystem::path& outputDir) {
        // UnrealPak.exe veya internal parser kullan
        auto pakReader = PakReader::open(pakPath);

        for (const auto& entry : pakReader.entries()) {
            if (entry.path.extension() == ".locres") {
                auto content = pakReader.read(entry);
                auto outputPath = outputDir / entry.path;
                std::filesystem::create_directories(outputPath.parent_path());
                writeFile(outputPath, content);
            }
        }

        return Ok();
    }

    Result<void> repackWithLocres(const std::filesystem::path& originalPak,
                                   const std::filesystem::path& modifiedLocres,
                                   const std::filesystem::path& outputPak) {
        // 1. Orijinal pak'ı aç
        auto pakReader = PakReader::open(originalPak);

        // 2. Yeni pak oluştur
        auto pakWriter = PakWriter::create(outputPak);

        for (const auto& entry : pakReader.entries()) {
            if (entry.path == modifiedLocres.filename()) {
                // Değiştirilmiş locres dosyasını kullan
                pakWriter.add(entry.path, readFile(modifiedLocres));
            } else {
                // Orijinal dosyayı kopyala
                pakWriter.add(entry.path, pakReader.read(entry));
            }
        }

        pakWriter.finalize();
        return Ok();
    }
};
```

### 3.4 Binary Patch (Son Çare)

**Açıklama:** Doğrudan binary düzenleme. Sadece basit formatlar için.

```cpp
// C++ Core: src/patch_engine/binary_patcher.cpp

class BinaryPatcher {
public:
    // String tabanlı binary patch
    Result<void> patchStrings(const std::filesystem::path& binaryPath,
                              const std::vector<StringPatch>& patches) {
        auto data = readFile(binaryPath);

        for (const auto& patch : patches) {
            // Orijinal string'i bul
            auto pos = findString(data, patch.original);

            if (pos == std::string::npos) {
                logger_.warn("String not found: {}", patch.original);
                continue;
            }

            // Boyut kontrolü
            if (patch.replacement.size() > patch.original.size()) {
                return Error("Replacement string too long");
            }

            // Değiştir (null-pad ile doldur)
            std::memcpy(data.data() + pos,
                       patch.replacement.c_str(),
                       patch.replacement.size());

            // Kalan alanı null ile doldur
            for (size_t i = patch.replacement.size();
                 i < patch.original.size(); i++) {
                data[pos + i] = '\0';
            }
        }

        return writeFile(binaryPath, data);
    }
};
```

---

## 4. Türkçe Karakter Desteği

### 4.1 Font Sistemi

```cpp
// Türkçe karakterler için font desteği
const std::vector<char32_t> TURKISH_CHARS = {
    U'Ç', U'ç', U'Ğ', U'ğ', U'I', U'ı', U'İ', U'i',
    U'Ö', U'ö', U'Ş', U'ş', U'Ü', U'ü'
};

class TurkishFontSupport {
public:
    // Font dosyasında Türkçe karakter kontrolü
    bool hasTurkishChars(const std::filesystem::path& fontPath) {
        auto font = loadFont(fontPath);

        for (auto ch : TURKISH_CHARS) {
            if (!font.hasGlyph(ch)) {
                return false;
            }
        }
        return true;
    }

    // Unity için font replacement
    Result<void> installTurkishFont(const std::filesystem::path& gamePath,
                                     const std::filesystem::path& fontPath) {
        // Unity font asset oluştur
        auto assetPath = gamePath / "*_Data" / "Fonts" / "turkish.ttf";
        std::filesystem::copy(fontPath, assetPath);

        // XUnity config'e ekle
        auto configPath = gamePath / "BepInEx" / "config" / "AutoTranslatorConfig.ini";
        appendToConfig(configPath, "[Font]", "OverrideFont=turkish.ttf");

        return Ok();
    }
};
```

### 4.2 Encoding Dönüşümü

```cpp
// UTF-8 / UTF-16 / Windows-1254 (Turkish) dönüşümü
class TurkishEncoding {
public:
    // Windows-1254 (Turkish ANSI) to UTF-8
    std::string cp1254ToUtf8(const std::string& input) {
        // Windows-1254 karakter eşlemeleri
        static const std::unordered_map<uint8_t, std::string> CP1254_TO_UTF8 = {
            {0xC7, "Ç"}, {0xE7, "ç"},
            {0xD0, "Ğ"}, {0xF0, "ğ"},  // Note: D0/F0 are Ğ/ğ in CP1254
            {0xDD, "I"}, {0xFD, "ı"},
            {0xDE, "Ş"}, {0xFE, "ş"},
            // ... diğer eşlemeler
        };

        std::string result;
        for (uint8_t ch : input) {
            auto it = CP1254_TO_UTF8.find(ch);
            if (it != CP1254_TO_UTF8.end()) {
                result += it->second;
            } else if (ch < 128) {
                result += static_cast<char>(ch);
            } else {
                // Varsayılan Latin-1 dönüşümü
                result += static_cast<char>(0xC0 | (ch >> 6));
                result += static_cast<char>(0x80 | (ch & 0x3F));
            }
        }
        return result;
    }

    // UTF-8 to Windows-1254
    std::string utf8ToCp1254(const std::string& input) {
        // Tersine dönüşüm
        // ...
    }
};
```

---

## 5. Paket Formatı

### 5.1 MakineAI Çeviri Paketi (.mtpkg)

```
package.mtpkg (ZIP format)
├── manifest.json         # Paket metadatası
├── signature.sig         # RSA-2048 imza
├── translations/
│   ├── strings.json      # Çeviri veritabanı
│   ├── fonts/            # Özel fontlar
│   └── assets/           # Değiştirilmiş asset'ler
└── scripts/
    └── post_install.lua  # Kurulum sonrası script (opsiyonel)
```

**manifest.json:**

```json
{
    "package_version": "1.0.0",
    "game_id": "steam:1245620",
    "game_name": "Elden Ring",
    "game_version": "1.12.0",
    "engine": "unity_il2cpp",
    "strategy": "runtime_hook",
    "translation_version": "2.1.0",
    "translator": "MakineAI Topluluk",
    "language": {
        "source": "en",
        "target": "tr"
    },
    "dependencies": {
        "bepinex": "5.4.22",
        "xunity_autotranslator": "5.3.0"
    },
    "files": [
        {
            "path": "BepInEx/Translation/tr/Text/_AutoGeneratedTranslations.txt",
            "hash": "sha256:abc123...",
            "size": 1234567
        }
    ],
    "checksum": "sha256:def456...",
    "created_at": "2026-01-20T12:00:00Z"
}
```

### 5.2 Paket İmzalama

```cpp
// C++ Core: src/security/package_signer.cpp

class PackageSigner {
public:
    Result<void> sign(const std::filesystem::path& packagePath,
                      const RSAPrivateKey& privateKey) {
        // 1. Manifest hash'i hesapla
        auto manifest = readFile(packagePath / "manifest.json");
        auto manifestHash = sha256(manifest);

        // 2. RSA-2048 imza oluştur
        auto signature = rsaSign(manifestHash, privateKey);

        // 3. İmzayı kaydet
        writeFile(packagePath / "signature.sig", signature);

        return Ok();
    }

    Result<bool> verify(const std::filesystem::path& packagePath,
                        const RSAPublicKey& publicKey) {
        // 1. Manifest hash'i hesapla
        auto manifest = readFile(packagePath / "manifest.json");
        auto manifestHash = sha256(manifest);

        // 2. İmzayı oku
        auto signature = readFile(packagePath / "signature.sig");

        // 3. İmzayı doğrula
        return rsaVerify(manifestHash, signature, publicKey);
    }
};
```

---

## 6. Versiyon Takip Sistemi

### 6.1 Oyun Dosyası Hash'leme

```cpp
// C++ Core: src/version_tracker.cpp

struct GameVersion {
    std::string gameId;
    std::string version;
    std::string exeHash;
    std::unordered_map<std::string, std::string> criticalFileHashes;
    std::chrono::system_clock::time_point detectedAt;
};

class VersionTracker {
public:
    Result<GameVersion> detectVersion(const GameInfo& game) {
        GameVersion version;
        version.gameId = game.id;

        // Ana EXE hash'i
        auto exePath = findMainExecutable(game.path);
        version.exeHash = sha256File(exePath);

        // Kritik dosya hash'leri
        auto criticalFiles = getCriticalFiles(game.engine);
        for (const auto& file : criticalFiles) {
            auto filePath = game.path / file;
            if (std::filesystem::exists(filePath)) {
                version.criticalFileHashes[file] = sha256File(filePath);
            }
        }

        // Steam/Epic'ten versiyon bilgisi al
        version.version = getVersionFromStore(game);

        version.detectedAt = std::chrono::system_clock::now();

        return version;
    }

    bool hasGameUpdated(const GameInfo& game, const GameVersion& savedVersion) {
        auto currentVersion = detectVersion(game);

        // EXE hash değişti mi?
        if (currentVersion.exeHash != savedVersion.exeHash) {
            return true;
        }

        // Kritik dosyalar değişti mi?
        for (const auto& [file, hash] : savedVersion.criticalFileHashes) {
            auto it = currentVersion.criticalFileHashes.find(file);
            if (it == currentVersion.criticalFileHashes.end() ||
                it->second != hash) {
                return true;
            }
        }

        return false;
    }
};
```

### 6.2 Otomatik Güncelleme Algılama

```cpp
// Arka planda oyun güncellemelerini kontrol et
class UpdateWatcher {
public:
    void startWatching(const std::vector<GameInfo>& games) {
        watcherThread_ = std::thread([this, games]() {
            while (running_) {
                for (const auto& game : games) {
                    if (hasInstalledTranslation(game)) {
                        auto savedVersion = loadSavedVersion(game);

                        if (versionTracker_.hasGameUpdated(game, savedVersion)) {
                            // Bildirim gönder
                            notifyGameUpdated(game);

                            // Yedekleri geri yükle (güvenlik için)
                            restoreBackups(game);
                        }
                    }
                }

                // 5 dakikada bir kontrol
                std::this_thread::sleep_for(std::chrono::minutes(5));
            }
        });
    }
};
```

---

## 7. Anti-Cheat Uyumluluğu

### 7.1 Desteklenen/Desteklenmeyen Oyunlar

```cpp
// Anti-cheat türlerine göre sınıflandırma
enum class AntiCheatStatus {
    SAFE,           // Anti-cheat yok veya modlamaya izin veriyor
    SINGLEPLAYER,   // Sadece singleplayer modda güvenli
    RISKY,          // Risk var, kullanıcı uyarılmalı
    BLOCKED         // Kesinlikle desteklenmez
};

const std::unordered_map<std::string, AntiCheatStatus> ANTICHEAT_STATUS = {
    // Easy Anti-Cheat
    {"EasyAntiCheat", AntiCheatStatus::BLOCKED},

    // BattlEye
    {"BEService", AntiCheatStatus::BLOCKED},

    // Valve Anti-Cheat
    {"VAC", AntiCheatStatus::RISKY},

    // Denuvo (DRM, anti-cheat değil)
    {"Denuvo", AntiCheatStatus::SAFE},

    // PunkBuster
    {"PunkBuster", AntiCheatStatus::BLOCKED},

    // Ricochet (CoD)
    {"Ricochet", AntiCheatStatus::BLOCKED},

    // Vanguard (Valorant)
    {"Vanguard", AntiCheatStatus::BLOCKED},
};

AntiCheatStatus detectAntiCheat(const GameInfo& game) {
    // DLL imzalarını kontrol et
    for (const auto& entry : std::filesystem::directory_iterator(game.path)) {
        auto filename = entry.path().filename().string();

        for (const auto& [acName, status] : ANTICHEAT_STATUS) {
            if (filename.find(acName) != std::string::npos) {
                return status;
            }
        }
    }

    // Çalışan process'leri kontrol et
    // ...

    return AntiCheatStatus::SAFE;
}
```

### 7.2 Kullanıcı Uyarı Sistemi

```cpp
// UI'da gösterilecek uyarı mesajları
struct AntiCheatWarning {
    std::string title;
    std::string message;
    std::string recommendation;
    bool allowContinue;
};

AntiCheatWarning getWarning(AntiCheatStatus status, const std::string& gameName) {
    switch (status) {
        case AntiCheatStatus::BLOCKED:
            return {
                "Çeviri Desteklenmiyor",
                gameName + " aktif anti-cheat koruması içeriyor.",
                "Bu oyun için çeviri yaması yüklenemez.",
                false
            };

        case AntiCheatStatus::RISKY:
            return {
                "Risk Uyarısı",
                gameName + " için çeviri yaması hesap güvenliğinizi riske atabilir.",
                "Sadece offline/singleplayer modda kullanmanızı öneririz.",
                true
            };

        case AntiCheatStatus::SINGLEPLAYER:
            return {
                "Singleplayer Modu",
                "Bu çeviri sadece singleplayer modda kullanılabilir.",
                "Multiplayer oynamadan önce yamayı kaldırın.",
                true
            };

        default:
            return {"", "", "", true};
    }
}
```

---

## 8. MakineAI'ın Özgün Özellikleri

### 8.1 Rakiplerden Farklar

| Özellik | XUnity.AutoTranslator | Translator++ | MTool | MakineAI |
|---------|----------------------|--------------|-------|----------|
| Tek tıklama kurulum | Hayır | Hayır | Evet (Japonca) | Evet (Türkçe) |
| Türkçe karakter | Kısmi | Kısmi | Hayır | Tam destek |
| Unity IL2CPP | Evet (karmaşık) | Hayır | Hayır | Tek tıklama |
| Unreal Engine | Hayır | Hayır | Hayır | Evet |
| Bethesda | Hayır | Hayır | Hayır | Evet |
| Otomatik yedekleme | Hayır | Hayır | Hayır | Evet |
| Versiyon takibi | Hayır | Hayır | Hayır | Evet |
| Topluluk deposu | Hayır | Hayır | Hayır | Evet |
| Anti-cheat algılama | Hayır | Hayır | Hayır | Evet |
| Türkçe UI | Hayır | Hayır | Hayır | Evet |

### 8.2 Teknik Yenilikler

1. **Birleşik Motor Algılama**: Tüm büyük oyun motorlarını tek sistemde algılama
2. **Akıllı Strateji Seçimi**: Motor ve oyun durumuna göre optimal çeviri stratejisi
3. **Atomik Yama İşlemleri**: Ya hep ya hiç - yarım kalmış yamalar yok
4. **Türkçe-First Tasarım**: Türkçe karakterler ve UI için optimize edilmiş
5. **Merkezi Paket Deposu**: Topluluk çevirilerini tek noktada toplama
6. **Güvenlik Katmanı**: Paket imzalama, anti-cheat algılama

---

## 9. Implementasyon Yol Haritası

### Faz 1: Temel Altyapı (0.1.0-alpha)
- [x] Oyun algılama (Steam/Epic/GOG)
- [x] Motor imza tespiti
- [ ] Unity runtime kurulum (BepInEx + XUnity)
- [ ] Basit dosya değiştirme (JSON, metin)
- [ ] Yedekleme/geri yükleme

### Faz 2: Genişletilmiş Destek (0.2.0)
- [ ] Unreal .pak/.locres desteği
- [ ] GameMaker data.win desteği
- [ ] Bethesda .ba2/.strings desteği
- [ ] Paket imzalama
- [ ] Versiyon takibi

### Faz 3: Topluluk Platformu (1.0.0)
- [ ] Merkezi çeviri deposu
- [ ] Çevirmen araçları
- [ ] Kalite sistemi
- [ ] Otomatik güncelleme

---

*Bu belge MakineAI projesinin teknik spesifikasyonunu içermektedir.*
