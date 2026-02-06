# Qt Services API Referansi

MakineAI Qt servis siniflarinin API referansi.

---

## GameService

Oyun islemleri servisi:

```cpp
class GameService : public QObject {
    Q_OBJECT

    // Properties
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY gamesChanged)

public:
    // Oyun tarama
    Q_INVOKABLE void scanGames();
    Q_INVOKABLE void scanGamesAsync();
    Q_INVOKABLE void cancelScan();

    // Oyun islemleri
    Q_INVOKABLE QVariantMap getGame(const QString& gameId);
    Q_INVOKABLE void addManualGame(const QString& path);
    Q_INVOKABLE void removeGame(const QString& gameId);
    Q_INVOKABLE void refreshGame(const QString& gameId);

    // Ceviri islemleri
    Q_INVOKABLE void applyTranslation(const QString& gameId, const QString& packageId);
    Q_INVOKABLE void removeTranslation(const QString& gameId);

signals:
    void gamesChanged();
    void scanningChanged();
    void scanProgress(float progress, const QString& message);
    void translationApplied(const QString& gameId);
    void translationRemoved(const QString& gameId);
    void error(const QString& message);
};
```

---

## TranslationService

Ceviri paketi servisi:

```cpp
class TranslationService : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadingChanged)

public:
    // Paket listesi
    Q_INVOKABLE void fetchPackages(const QString& gameId);
    Q_INVOKABLE QVariantMap getPackage(const QString& packageId);

    // Indirme
    Q_INVOKABLE void downloadPackage(const QString& packageId);
    Q_INVOKABLE void cancelDownload();

    // Yerel paketler
    Q_INVOKABLE QVariantList getLocalPackages();
    Q_INVOKABLE void deleteLocalPackage(const QString& packageId);

signals:
    void packagesChanged();
    void downloadingChanged();
    void downloadProgress(float progress);
    void downloadCompleted(const QString& packageId);
    void error(const QString& message);
};
```

---

## SettingsManager

Ayar yonetim servisi:

```cpp
class SettingsManager : public QObject {
    Q_OBJECT

    // Genel ayarlar
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool autoScan READ autoScan WRITE setAutoScan NOTIFY autoScanChanged)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY notificationsChanged)

    // Yedekleme
    Q_PROPERTY(bool autoBackup READ autoBackup WRITE setAutoBackup NOTIFY autoBackupChanged)
    Q_PROPERTY(QString backupPath READ backupPath WRITE setBackupPath NOTIFY backupPathChanged)
    Q_PROPERTY(int maxBackups READ maxBackups WRITE setMaxBackups NOTIFY maxBackupsChanged)

    // Kutuphane klasorleri
    Q_PROPERTY(QStringList libraryPaths READ libraryPaths NOTIFY libraryPathsChanged)

public:
    // Kutuphane yonetimi
    Q_INVOKABLE void addLibraryPath(const QString& path);
    Q_INVOKABLE void removeLibraryPath(const QString& path);

    // Kaydet/Yukle
    Q_INVOKABLE void save();
    Q_INVOKABLE void load();
    Q_INVOKABLE void reset();

signals:
    void languageChanged();
    void themeChanged();
    void autoScanChanged();
    void notificationsChanged();
    void autoBackupChanged();
    void backupPathChanged();
    void maxBackupsChanged();
    void libraryPathsChanged();
    void settingsChanged();
};
```

---

## BackupManager

Yedekleme servisi:

```cpp
class BackupManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList backups READ backups NOTIFY backupsChanged)

public:
    // Yedek listesi
    Q_INVOKABLE void fetchBackups(const QString& gameId);
    Q_INVOKABLE QVariantList getAllBackups();

    // Yedekleme islemleri
    Q_INVOKABLE void createBackup(const QString& gameId);
    Q_INVOKABLE void restoreBackup(const QString& backupId);
    Q_INVOKABLE void deleteBackup(const QString& backupId);
    Q_INVOKABLE void deleteAllBackups(const QString& gameId);

signals:
    void backupsChanged();
    void backupCreated(const QString& backupId);
    void backupRestored(const QString& backupId);
    void backupDeleted(const QString& backupId);
    void error(const QString& message);
};
```

---

## ProcessScanner

Calisanan oyun tespiti:

```cpp
class ProcessScanner : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantMap runningGame READ runningGame NOTIFY runningGameChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

public:
    Q_INVOKABLE void startScanning();
    Q_INVOKABLE void stopScanning();
    Q_INVOKABLE QVariantMap detectRunningGame();

signals:
    void runningGameChanged();
    void scanningChanged();
    void gameDetected(const QVariantMap& game);
    void gameClosed(const QString& gameId);
};
```

---

## CoreBridge

Core library koprusu:

```cpp
class CoreBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    static CoreBridge* instance();

    // Lifecycle
    Q_INVOKABLE bool initialize();
    Q_INVOKABLE void shutdown();

    // Durum
    Q_INVOKABLE bool isHealthy();
    Q_INVOKABLE QVariantMap getMetrics();
    Q_INVOKABLE QString getVersion();

    // Core erisimi (internal)
    makineai::Core& core();

signals:
    void initializedChanged();
    void healthChanged(bool healthy);
    void error(const QString& message);
};
```

---

## QML Kullanim Ornekleri

### GameService

```qml
import MakineAI 1.0

Item {
    GameService {
        id: gameService

        onGamesChanged: {
            console.log("Games updated:", games.length)
        }

        onScanProgress: (progress, message) => {
            progressBar.value = progress
            statusText.text = message
        }

        onError: (message) => {
            errorDialog.show(message)
        }
    }

    Button {
        text: gameService.scanning ? "Taranıyor..." : "Tara"
        enabled: !gameService.scanning
        onClicked: gameService.scanGames()
    }

    ListView {
        model: gameService.games
        delegate: GameCard {
            gameName: modelData.name
            engineType: modelData.engine
            onClicked: gameService.applyTranslation(modelData.id, selectedPackage)
        }
    }
}
```

### SettingsManager

```qml
import MakineAI 1.0

Item {
    SettingsManager {
        id: settings
    }

    Column {
        Switch {
            text: "Otomatik Tarama"
            checked: settings.autoScan
            onCheckedChanged: settings.autoScan = checked
        }

        Switch {
            text: "Otomatik Yedekleme"
            checked: settings.autoBackup
            onCheckedChanged: settings.autoBackup = checked
        }

        Button {
            text: "Kaydet"
            onClicked: settings.save()
        }
    }
}
```

---

## Signal/Slot Ornekleri

### Connections

```qml
Connections {
    target: gameService

    function onTranslationApplied(gameId) {
        notificationToast.show("Ceviri uygulandi: " + gameId)
    }

    function onError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
}
```

### Binding

```qml
BusyIndicator {
    running: gameService.scanning || translationService.downloading
}

Text {
    text: gameService.gameCount + " oyun bulundu"
}
```
