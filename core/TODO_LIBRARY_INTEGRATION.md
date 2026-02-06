# MakineAI - Library Integration TODOs

Bu dosya yeni eklenen kutuphanelerin entegrasyon planini icerir.
Kutuphaneler vcpkg.json'da tanimli ve CMakeLists.txt'de optional olarak bagli.

## Status Legend
- [ ] Planlanmis
- [x] Tamamlandi
- [~] Kismen tamamlandi

---

## 1. TASKFLOW - Paralel Islem Framework

**Dosyalar:**
- `src/game_detector/game_detector.cpp`
- `src/asset_parser/asset_parser.cpp`
- `src/translation/translation_memory.cpp`

**Gorevler:**
- [ ] `GameDetector::scanAll()` - Paralel store tarama (Steam, Epic, GOG ayni anda)
- [ ] `AssetParser::parseDirectory()` - Paralel asset parsing
- [ ] `TranslationMemoryService::findBatchMatches()` - Paralel ceviri eslestirme
- [ ] Engine detection paralellestirme

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_TASKFLOW
tf::Executor executor(std::thread::hardware_concurrency());
tf::Taskflow taskflow;

for (const auto& scanner : scanners_) {
    taskflow.emplace([&]() {
        auto result = scanner->scan();
        // merge results...
    });
}

executor.run(taskflow).wait();
#endif
```

---

## 2. MIO - Memory-Mapped I/O

**Dosyalar:**
- `src/game_detector/game_detector.cpp`
- `src/security/security_manager.cpp`
- `src/asset_parser/*.cpp`

**Gorevler:**
- [ ] `readUnityVersion()` - Buyuk globalgamemanagers dosyalari
- [ ] `hashFile()` - Buyuk dosya hash hesaplama
- [ ] Unity asset bundle okuma
- [ ] Unreal pak dosyasi okuma

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_MIO
std::error_code ec;
mio::mmap_source mmap(file.string(), ec);
if (!ec) {
    // Zero-copy file access
    process(mmap.data(), mmap.size());
}
#endif
```

---

## 3. EFSW - Filesystem Watcher

**Dosyalar:**
- `src/game_detector/game_detector.cpp` (yeni fonksiyon)
- `qml/src/services/gameservice.cpp`

**Gorevler:**
- [ ] Oyun klasoru degisiklik izleme
- [ ] Yeni oyun kurulumu otomatik algilama
- [ ] Yama dosyasi degisiklik izleme
- [ ] Hot-reload destegi

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_EFSW
class GameFolderListener : public efsw::FileWatchListener {
public:
    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                         const std::string& filename, efsw::Action action,
                         std::string oldFilename) override {
        if (action == efsw::Actions::Add) {
            // Yeni oyun algilandi
            emit gameInstalled(dir + "/" + filename);
        }
    }
};

efsw::FileWatcher watcher;
watcher.addWatch(steamPath, listener, true);
watcher.watch();
#endif
```

---

## 4. BIT7Z / LIBARCHIVE - Arsiv Islemleri

**Dosyalar:**
- `src/asset_parser/unity_bundle_parser.cpp`
- `src/asset_parser/unreal_pak_parser.cpp`
- `src/package_manager/package_manager.cpp`

**Gorevler:**
- [ ] Unity .unity3d arsiv acma
- [ ] Unreal .pak dosyasi okuma
- [ ] 7z/zip/rar ceviri paketi destegi
- [ ] Sifreli arsiv destegi (minizip-ng)

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_BIT7Z
bit7z::Bit7zLibrary lib{"7z.dll"};
bit7z::BitArchiveReader archive{lib, archivePath, bit7z::BitFormat::SevenZip};

for (const auto& item : archive) {
    if (item.isDir()) continue;
    auto buffer = archive.extract(item.index());
    // process buffer...
}
#endif

#ifdef MAKINEAI_HAS_LIBARCHIVE
struct archive* a = archive_read_new();
archive_read_support_format_all(a);
archive_read_support_filter_all(a);

if (archive_read_open_filename(a, path.c_str(), 10240) == ARCHIVE_OK) {
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        // extract entry...
    }
}
archive_read_free(a);
#endif
```

---

## 5. SIMDJSON - Hizli JSON Parsing

**Dosyalar:**
- `src/translation/translation_memory.cpp`
- `src/package_manager/package_manager.cpp`
- RPG Maker MV/MZ handler

**Gorevler:**
- [ ] Buyuk ceviri JSON dosyalari (100MB+)
- [ ] RPG Maker System.json, Map*.json
- [ ] Paket manifest parsing
- [ ] Recipe dosyalari

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_SIMDJSON
simdjson::ondemand::parser parser;
simdjson::padded_string json = simdjson::padded_string::load(path);
simdjson::ondemand::document doc = parser.iterate(json);

for (auto entry : doc["translations"]) {
    std::string_view source = entry["source"];
    std::string_view target = entry["target"];
    // process...
}
#endif
```

---

## 6. SIMDUTF - Hizli UTF Donusumu

**Dosyalar:**
- `src/translation/translation_memory.cpp`
- Tum text processing islemleri

**Gorevler:**
- [ ] UTF-8 <-> UTF-16 donusumu (Windows API uyumu)
- [ ] Turkce karakter normalizasyonu
- [ ] BOM handling
- [ ] Invalid UTF tespiti

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_SIMDUTF
// UTF-8 validation
bool valid = simdutf::validate_utf8(text.data(), text.size());

// UTF-8 to UTF-16 conversion
size_t utf16_len = simdutf::utf16_length_from_utf8(text.data(), text.size());
std::u16string utf16(utf16_len, '\0');
simdutf::convert_utf8_to_utf16(text.data(), text.size(), utf16.data());
#endif
```

---

## 7. SQLITECPP - Modern SQLite Wrapper

**Dosyalar:**
- `src/database/database.cpp`

**Gorevler:**
- [ ] Mevcut raw SQLite kodunu SQLiteCpp'ye migrate et
- [ ] Transaction wrapper
- [ ] Prepared statement caching
- [ ] RAII-based connection management

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_SQLITECPP
SQLite::Database db(dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

SQLite::Statement query(db, "SELECT * FROM games WHERE engine = ?");
query.bind(1, static_cast<int>(engine));

while (query.executeStep()) {
    GameInfo game;
    game.name = query.getColumn("name").getString();
    game.installPath = query.getColumn("install_path").getString();
    // ...
}
#endif
```

---

## 8. LIBSODIUM - Modern Kriptografi

**Dosyalar:**
- `src/security/security_manager.cpp`

**Gorevler:**
- [ ] BLAKE2b hash (SHA256'dan hizli)
- [ ] Ed25519 imza dogrulama
- [ ] Secure random generation
- [ ] Paket sifreleme/cozme

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_SODIUM
// Initialize
if (sodium_init() < 0) {
    return error;
}

// BLAKE2b hash (faster than SHA256)
unsigned char hash[crypto_generichash_BYTES];
crypto_generichash(hash, sizeof(hash),
                   data.data(), data.size(),
                   nullptr, 0);

// Ed25519 signature verification
if (crypto_sign_verify_detached(signature, message, message_len, public_key) != 0) {
    return error;
}
#endif
```

---

## 9. CONCURRENTQUEUE - Lock-Free Queue

**Dosyalar:**
- `src/translation/translation_memory.cpp`
- `qml/src/services/*.cpp`

**Gorevler:**
- [ ] Real-time ceviri streaming
- [ ] Producer-consumer pattern
- [ ] Background task queue
- [ ] UI thread communication

**Ornek Kullanim:**
```cpp
#ifdef MAKINEAI_HAS_CONCURRENTQUEUE
moodycamel::ConcurrentQueue<TranslationTask> taskQueue;

// Producer (worker thread)
taskQueue.enqueue(task);

// Consumer (UI thread)
TranslationTask task;
if (taskQueue.try_dequeue(task)) {
    processTask(task);
}
#endif
```

---

## Oncelik Sirasi

1. **YUKSEK** (Hemen uygulanmali)
   - TASKFLOW - Paralel tarama ve parsing
   - MIO - Buyuk dosya okuma
   - SIMDJSON - JSON performansi

2. **ORTA** (MVP icin)
   - BIT7Z/LIBARCHIVE - Arsiv destegi
   - SIMDUTF - UTF islemleri
   - SQLITECPP - Database iyilestirme

3. **DUSUK** (Post-MVP)
   - EFSW - Dosya izleme
   - LIBSODIUM - Gelismis kripto
   - CONCURRENTQUEUE - Streaming

---

## Test Plani

Her kutuphane icin:
1. Feature flag kontrolu (#ifdef)
2. Fallback davranis (kutuphane yoksa)
3. Performans karsilastirma
4. Memory kullanim testi

---

*Son guncelleme: 2026-01-23*
