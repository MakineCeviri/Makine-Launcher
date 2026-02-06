# Mimari Genel Bakis

MakineAI'nin sistem mimarisini aciklar.

---

## Katman Yapisi

```
+----------------------------------------------------------+
|                    Qt6 QML UI Layer                       |
|  +------------------------------------------------------+ |
|  |  Main.qml -> HomeScreen -> Components -> Dialogs     | |
|  +------------------------------------------------------+ |
+----------------------------------------------------------+
|                   Qt Services Layer                       |
|  +------------------------------------------------------+ |
|  |  GameService  |  CoreBridge  |  SettingsManager      | |
|  +------------------------------------------------------+ |
+----------------------------------------------------------+
|                   C++ Core Library                        |
|  +-----------+------------+-----------+-----------------+ |
|  | GameDet.  | AssetParser| PatchEng. | PackageManager  | |
|  +-----------+------------+-----------+-----------------+ |
|  |                  Engine Handlers                     | |
|  |  Unity | Unreal | RPGMaker | Ren'Py | GameMaker     | |
|  +------------------------------------------------------+ |
+----------------------------------------------------------+
```

---

## Katman Aciklamalari

### 1. QML UI Layer

Kullanici arayuzu katmani:

- **Main.qml** - Ana pencere ve navigation
- **HomeScreen.qml** - Ana ekran, oyun listesi
- **GameDetailScreen.qml** - Oyun detay ekrani
- **SettingsScreen.qml** - Ayarlar

**Theme Sistemi:**
- `Theme.qml` - Renkler ve stiller
- `Dimensions.qml` - Boyutlar ve spacing

### 2. Qt Services Layer

QML ile Core arasindaki kopru:

- **GameService** - Oyun islemleri (tarama, ceviri)
- **TranslationService** - Ceviri islemleri
- **SettingsManager** - Ayar yonetimi
- **BackupManager** - Yedekleme
- **ProcessScanner** - Calisanan oyun tespiti
- **CoreBridge** - Core library baglantisi

### 3. C++ Core Library

Ana is mantigi:

- **GameDetector** - Steam, Epic, GOG tarama
- **AssetParser** - Oyun dosyasi analizi
- **PatchEngine** - Patch uygulama/geri alma
- **PackageManager** - Ceviri paketi yonetimi
- **RuntimeManager** - BepInEx/XUnity kurulumu
- **SecurityManager** - Imza dogrulama

### 4. Engine Handlers

Motor-spesifik islemler:

- **UnityHandler** - Unity Mono/IL2CPP
- **UnrealHandler** - Unreal Engine .pak
- **RpgMakerHandler** - RPG Maker MV/MZ
- **RenPyHandler** - Ren'Py .rpy
- **GameMakerHandler** - GameMaker data.win
- **BethesdaHandler** - Creation Engine .ba2

---

## Veri Akisi

### Tipik Islem Akisi

```
1. Kullanici Aksiyonu
       |
       v
2. QML Component (buton tiklama)
       |
       v
3. Qt Service (Q_INVOKABLE metod)
       |
       v
4. Core Library (is mantigi)
       |
       v
5. Engine Handler (motor-spesifik)
       |
       v
6. Sonuc (Result<T>)
       |
       v
7. Qt Service (signal emit)
       |
       v
8. QML UI (guncelleme)
```

### Ornek: Oyun Tarama

```cpp
// QML
GameService.scanGames()

// Qt Service
void GameService::scanGames() {
    auto games = CoreBridge::instance()->gameDetector().scanAll();
    emit gamesChanged(games);
}

// Core
std::vector<GameInfo> GameDetector::scanAll() {
    std::vector<GameInfo> games;
    games += steamScanner_.scan();
    games += epicScanner_.scan();
    games += gogScanner_.scan();
    return games;
}
```

---

## Hata Yonetimi

Result-based error handling kullanilir:

```cpp
// Result tipi
template<typename T>
class Result {
    std::variant<T, Error> data_;
public:
    bool success() const;
    T& value();
    Error& error();
};

// Kullanim
Result<GameInfo> result = gameDetector.detect(path);
if (!result.success()) {
    logger()->error("Failed: {}", result.error().message());
    return;
}
auto game = result.value();
```

---

## Asenkron Islemler

Uzun suren islemler icin AsyncOperation:

```cpp
// Asenkron tarama
AsyncOperation<std::vector<GameInfo>> scanGamesAsync(
    ProgressCallback progress = nullptr
);

// Kullanim
auto op = core.scanGamesAsync([](float progress) {
    qDebug() << "Progress:" << progress * 100 << "%";
});

op.then([](auto games) {
    // Sonuc geldiginde
});
```

---

## Mimari Kararlar (ADR)

Detayli mimari kararlar icin:

| ADR | Baslik |
|-----|--------|
| [0001](../adr/0001-native-cpp-architecture.md) | Native C++ Architecture |
| [0002](../adr/0002-result-based-error-handling.md) | Result-based Error Handling |
| [0003](../adr/0003-translation-pipeline-decision-engine.md) | Translation Pipeline |
| [0004](../adr/0004-optional-library-integration.md) | Optional Library Integration |
| [0005](../adr/0005-handler-based-engine-support.md) | Handler-based Engine Support |

---

## Sonraki Adimlar

- [Core Kutuphane](core-library.md)
- [QML Arayuz](qml-frontend.md)
- [Build Sistemi](build-system.md)
