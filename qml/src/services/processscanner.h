/**
 * @file processscanner.h
 * @brief Running game process detection
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QQmlEngine>

namespace makineai {

/**
 * @brief Process Scanner - Monitors running processes for game detection
 *
 * Provides:
 * - Real-time process monitoring
 * - Game process detection
 * - Anti-cheat detection warnings
 */
class ProcessScanner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool isWatching READ isWatching NOTIFY isWatchingChanged)
    Q_PROPERTY(bool gameRunning READ gameRunning NOTIFY gameRunningChanged)
    Q_PROPERTY(QString runningGameId READ runningGameId NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameName READ runningGameName NOTIFY runningGameChanged)
    Q_PROPERTY(bool hasAntiCheat READ hasAntiCheat NOTIFY antiCheatChanged)
    Q_PROPERTY(QString antiCheatSummary READ antiCheatSummary NOTIFY antiCheatChanged)

public:
    explicit ProcessScanner(QObject *parent = nullptr);
    ~ProcessScanner() override;

    static ProcessScanner* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    bool isWatching() const { return m_isWatching; }
    bool gameRunning() const { return m_gameRunning; }
    QString runningGameId() const { return m_runningGameId; }
    QString runningGameName() const { return m_runningGameName; }
    bool hasAntiCheat() const { return m_hasAntiCheat; }
    QString antiCheatSummary() const { return m_antiCheatSummary; }

    // Q_INVOKABLE methods
    static constexpr int kDefaultScanIntervalMs = 3000;
    Q_INVOKABLE void startWatching(int intervalMs = kDefaultScanIntervalMs);
    void stopWatching();
signals:
    void isWatchingChanged();
    void gameRunningChanged();
    void runningGameChanged();
    void antiCheatChanged();
    void gameDetected(const QString& gameId, const QString& gameName);
    void gameClosed(const QString& gameId);
    void antiCheatDetected(const QString& system, const QString& severity);

private slots:
    void performScan();

private:
    void detectRunningGames();
    QStringList getRunningProcesses();

    QTimer* m_scanTimer{nullptr};
    bool m_isWatching{false};
    bool m_gameRunning{false};
    QString m_runningGameId;
    QString m_runningGameName;
    bool m_hasAntiCheat{false};
    QString m_antiCheatSummary;

    // Known game processes (simplified - would be loaded from database)
    QMap<QString, QString> m_knownProcesses;
};

} // namespace makineai
