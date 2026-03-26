/**
 * @file processscanner.h
 * @brief Running game process detection
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Three-layer detection:
 * 1. Known exe match (disk scan + catalog fingerprints) - automatic
 * 2. GPU process detection (DirectX/Vulkan DLL loading) - shows candidates to user
 * 3. User selects process - system resolves game and adds to library
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QQmlEngine>
#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace makine {

class GameService;
class LocalPackageManager;

class ProcessScanner : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isWatching READ isWatching NOTIFY isWatchingChanged)
    Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY gameRunningChanged)
    Q_PROPERTY(QString runningGameId READ runningGameId NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameName READ runningGameName NOTIFY runningGameChanged)
    Q_PROPERTY(bool hasAntiCheat READ hasAntiCheat NOTIFY antiCheatChanged)
    Q_PROPERTY(QString antiCheatSummary READ antiCheatSummary NOTIFY antiCheatChanged)
    Q_PROPERTY(QVariantList heavyProcesses READ heavyProcesses NOTIFY heavyProcessesChanged)

public:
    explicit ProcessScanner(QObject *parent = nullptr);
    ~ProcessScanner() override;

    static ProcessScanner* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    void setGameService(GameService* service) { m_gameService = service; }
    void setPackageManager(LocalPackageManager* manager) { m_packageManager = manager; }

    bool isWatching() const { return m_isWatching; }
    bool gameRunning() const { return m_gameRunning; }
    QString runningGameId() const { return m_runningGameId; }
    QString runningGameName() const { return m_runningGameName; }
    bool hasAntiCheat() const { return m_hasAntiCheat; }
    QString antiCheatSummary() const { return m_antiCheatSummary; }
    QVariantList heavyProcesses() const { return m_heavyProcesses; }

    static constexpr int kDefaultScanIntervalMs = 10000;
    Q_INVOKABLE void startWatching(int intervalMs = kDefaultScanIntervalMs);
    Q_INVOKABLE void stopWatching();

    /**
     * @brief User selected a heavy process as their game
     *
     * Called from QML when user picks a process from the heavy process list.
     * Resolves the process path, identifies the game via fingerprinting,
     * and adds it to the library.
     *
     * @param pid Process ID of the selected process
     * @return Game install path (empty if resolution failed)
     */
    Q_INVOKABLE QString resolveSelectedProcess(qint64 pid);

public slots:
    void rebuildProcessMap();

signals:
    void isWatchingChanged();
    void gameRunningChanged();
    void runningGameChanged();
    void antiCheatChanged();
    void heavyProcessesChanged();

    // Emitted when a KNOWN game is detected (auto, not in library)
    void gameDetected(const QString& gameId, const QString& gameName);
    void gameClosed(const QString& gameId);
    void antiCheatDetected(const QString& system, const QString& severity);

    // Emitted when user resolves a process to a game
    void processResolved(const QString& gameId, const QString& gameName, const QString& installPath);

    // Emitted when user selects a process but it is not in the catalog
    void processNotSupported(const QString& processName);

private slots:
    void performScan();

private:
    struct ProcessInfo {
        QString exeName;
#ifdef Q_OS_WIN
        DWORD pid{0};
#else
        uint32_t pid{0};
#endif
    };

    void detectRunningGames(const QList<ProcessInfo>& processes);
    void updateHeavyProcessList(const QList<ProcessInfo>& processes);
    QList<ProcessInfo> getRunningProcesses();

#ifdef Q_OS_WIN
    QString getProcessFullPath(DWORD processId) const;
    bool isProcessUsingGpu(DWORD processId) const;
    bool hasVisibleWindow(DWORD processId) const;
#endif

    QTimer m_scanTimer;
    bool m_isWatching{false};
    bool m_gameRunning{false};
    QString m_runningGameId;
    QString m_runningGameName;
    bool m_hasAntiCheat{false};
    QString m_antiCheatSummary;

    QString m_runningExeName;  // For fast-path check
    unsigned m_scanCycle{0};

    QHash<QString, QString> m_knownProcesses;  // exe name -> appId
    QHash<QString, QString> m_appIdToPath;     // appId -> install dir

    QVariantList m_heavyProcesses;  // [{name, pid}] GPU-using processes with visible windows
    std::atomic<bool> m_scanInProgress{false};

    GameService* m_gameService{nullptr};
    LocalPackageManager* m_packageManager{nullptr};
};

} // namespace makine
