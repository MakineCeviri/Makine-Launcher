# Qt Services API Referansı

Makine-Launcher Qt servis sınıflarının API referansı.

> **Not:** Bu dökümanlar UI_ONLY (dev) build'de kullanılan saf Qt servislerini tanımlar.
> Tüm servisler `qml/src/services/` altındadır.

---

## CoreBridge (Singleton)

Oyun tarama ve paket yönetiminin merkezi. UI_ONLY modda gerçek Steam/Epic/GOG
tarama yapar. Full modda Core kütüphanesine yönlendirir.

```cpp
class CoreBridge : public QObject {
    Q_OBJECT

public:
    static CoreBridge* instance();

    // Oyun tarama
    Q_INVOKABLE void scanAllLibraries();
    Q_INVOKABLE void scanSteamLibrary();
    Q_INVOKABLE void scanEpicLibrary();
    Q_INVOKABLE void scanGogLibrary();

    // Motor tespiti
    Q_INVOKABLE QString detectEngine(const QString& gamePath);
    Q_INVOKABLE bool hasAntiCheat(const QString& gamePath);

    // Paket islemleri
    Q_INVOKABLE void installTranslation(const QString& gameId, const QString& packagePath);
    Q_INVOKABLE void uninstallTranslation(const QString& gameId);

    // Tespit edilen oyunlar
    QList<DetectedGame> detectedGames() const;

signals:
    void scanStarted();
    void scanCompleted(int count);
    void gameDetected(const QString& gameId, const QString& gameName);
    void translationInstalled(const QString& gameId);
    void translationUninstalled(const QString& gameId);
    void error(const QString& message);
};
```

---

## GameService

QML ile CoreBridge arasındaki köprü. Oyun listesi, çeviri kurulum/kaldırma,
anti-cheat uyarıları yönetir.

```cpp
class GameService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

public:
    Q_INVOKABLE void scanGames();
    Q_INVOKABLE QVariantMap getGame(const QString& gameId);
    Q_INVOKABLE void applyTranslation(const QString& gameId, const QString& packageId);
    Q_INVOKABLE void removeTranslation(const QString& gameId);

signals:
    void scanningChanged();
    void scanProgress(float progress, const QString& message);
    void translationApplied(const QString& gameId);
    void translationRemoved(const QString& gameId);
    void error(const QString& message);
};
```

---

## LocalPackageManager

Yerel çeviri paketlerini yönetir. `translation_data/` dizinini tarar,
paket ID → Steam AppID eşleştirmesi yapar.

```cpp
class LocalPackageManager : public QObject {
    Q_OBJECT

public:
    void scanLocalPackages();
    bool hasPackageForGame(const QString& steamAppId) const;
    QVariantMap getPackageForGame(const QString& steamAppId) const;
    bool installPackage(const QString& steamAppId, const QString& gamePath);
    bool uninstallPackage(const QString& steamAppId, const QString& gamePath);

signals:
    void packagesChanged();
    void packageInstalled(const QString& gameId);
    void packageUninstalled(const QString& gameId);
};
```

---

## BackupManager

Yedekleme ve geri yükleme. Kurulumdan önce otomatik yedek alır,
async çalışır (QtConcurrent).

```cpp
class BackupManager : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void createBackup(const QString& gameId, const QString& gamePath);
    Q_INVOKABLE void restoreBackup(const QString& backupId);
    Q_INVOKABLE void deleteBackup(const QString& backupId);
    Q_INVOKABLE QVariantList getBackups(const QString& gameId);

signals:
    void backupCreated(const QString& backupId);
    void backupRestored(const QString& backupId);
    void backupProgress(float progress);
    void error(const QString& message);
};
```

---

## SettingsManager

Uygulama ayarları. QSettings (Windows Registry) ile persist eder.

```cpp
class SettingsManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool autoDetectGames READ autoDetectGames WRITE setAutoDetectGames NOTIFY autoDetectGamesChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(QString translationDataPath READ translationDataPath WRITE setTranslationDataPath NOTIFY translationDataPathChanged)

public:
    Q_INVOKABLE void resetToDefaults();

signals:
    void languageChanged();
    void themeChanged();
    void autoDetectGamesChanged();
    void showNotificationsChanged();
    void startWithWindowsChanged();
    void minimizeToTrayChanged();
    void translationDataPathChanged();
};
```

---

## ProcessScanner

Çalışan oyun tespiti. Win32 CreateToolhelp32Snapshot API kullanır.
Visibility-aware: pencere minimize'da tarama intervali artar (3s → 30s).

```cpp
class ProcessScanner : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)

public:
    Q_INVOKABLE void startScanning();
    Q_INVOKABLE void stopScanning();

signals:
    void gameRunningChanged();
    void gameDetected(const QString& processName);
    void gameClosed(const QString& processName);
};
```

---

## SystemTrayManager

Native Win32 sistem tepsisi. Shell_NotifyIconW ile doğrudan entegrasyon,
Qt6Widgets bağımlılığına gerek yok.

```cpp
class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    void initialize(HWND parentWindow);
    void showNotification(const QString& title, const QString& message);
    void updateTooltip(const QString& text);

signals:
    void trayIconClicked();
    void showRequested();
    void settingsRequested();
    void quitRequested();
};
```

---

## IntegrityService

Binary bütünlük kontrolü. SHA-256 ile executable'ı doğrular.
Dev build'de .sha256 dosyası yoksa atlar.

```cpp
class IntegrityService : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void verifyBinaryIntegrity();

signals:
    void verificationComplete(bool valid);
    void verificationSkipped();
};
```

---

## BatchOperationService

Toplu işlem kuyruğu. Birden fazla oyuna çeviri kurulum/kaldırma
işlemlerini sıralayarak çalıştırır.

```cpp
class BatchOperationService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(float progress READ progress NOTIFY progressChanged)

public:
    Q_INVOKABLE void addOperation(const QString& gameId, const QString& operation);
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void operationCompleted(const QString& gameId, bool success);
    void allCompleted();
};
```

---

## UpdateDetectionService (İskelet)

Oyun güncelleme tespiti. Sınıf yapısı ve API tanımlı,
implementasyon henüz tamamlanmadı.

```cpp
class UpdateDetectionService : public QObject {
    Q_OBJECT

public:
    // Tier 1: Store metadata kontrol
    Q_INVOKABLE void checkAllGamesQuick();
    Q_INVOKABLE void checkGameQuick(const QString& gameId);

    // Tier 2: Dosya hash snapshot
    Q_INVOKABLE void createSnapshot(const QString& gameId);
    Q_INVOKABLE void checkCompatibility(const QString& gameId);

signals:
    void updateDetected(const QString& gameId, const QString& details);
    void compatibilityResult(const QString& gameId, bool compatible);
};
```
