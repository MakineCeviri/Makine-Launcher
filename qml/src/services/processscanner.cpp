/**
 * @file processscanner.cpp
 * @brief Process Scanner Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "processscanner.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

namespace makineai {

ProcessScanner::ProcessScanner(QObject *parent)
    : QObject(parent)
{
    // Initialize known game processes
    m_knownProcesses["eldenring.exe"] = "1245620";
    m_knownProcesses["starfield.exe"] = "1716740";
    m_knownProcesses["b1.exe"] = "2358720";  // Black Myth: Wukong
    m_knownProcesses["cyberpunk2077.exe"] = "1091500";
    m_knownProcesses["bg3.exe"] = "1086940";
    m_knownProcesses["bg3_dx11.exe"] = "1086940";
}

ProcessScanner::~ProcessScanner()
{
    stopWatching();
}

ProcessScanner* ProcessScanner::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new ProcessScanner();
}

void ProcessScanner::startWatching(int intervalMs)
{
    if (m_isWatching) return;

    if (!m_scanTimer) {
        m_scanTimer = new QTimer(this);
        connect(m_scanTimer, &QTimer::timeout, this, &ProcessScanner::performScan);
    }

    m_scanTimer->start(intervalMs);
    m_isWatching = true;
    emit isWatchingChanged();

    // Perform initial scan
    performScan();

    qDebug() << "Process scanner started with interval:" << intervalMs << "ms";
}

void ProcessScanner::stopWatching()
{
    if (!m_isWatching) return;

    if (m_scanTimer) {
        m_scanTimer->stop();
    }

    m_isWatching = false;
    emit isWatchingChanged();

    qDebug() << "Process scanner stopped";
}

bool ProcessScanner::checkGameRunning(const QString& gameId)
{
    Q_UNUSED(gameId)
    // TODO: Check if specific game is running
    return m_gameRunning && m_runningGameId == gameId;
}

void ProcessScanner::scanAntiCheat(const QString& gamePath)
{
    checkAntiCheatSystems(gamePath);
}

void ProcessScanner::performScan()
{
    detectRunningGames();
}

void ProcessScanner::detectRunningGames()
{
#ifdef Q_OS_WIN
    const QStringList processes = getRunningProcesses();
    bool foundGame = false;
    QString foundGameId;
    QString foundGameName;

    for (const QString& process : processes) {
        const QString lowerProcess = process.toLower();

        if (m_knownProcesses.contains(lowerProcess)) {
            foundGame = true;
            foundGameId = m_knownProcesses[lowerProcess];

            // Get game name from process name
            QString name = process;
            name.remove(".exe", Qt::CaseInsensitive);
            foundGameName = name;
            break;
        }
    }

    if (foundGame != m_gameRunning || foundGameId != m_runningGameId) {
        const bool wasRunning = m_gameRunning;
        const QString previousGameId = m_runningGameId;

        m_gameRunning = foundGame;
        m_runningGameId = foundGameId;
        m_runningGameName = foundGameName;

        emit gameRunningChanged();
        emit runningGameChanged();

        if (foundGame && !wasRunning) {
            emit gameDetected(foundGameId, foundGameName);
            qDebug() << "Game detected:" << foundGameName << "(" << foundGameId << ")";
        } else if (!foundGame && wasRunning) {
            emit gameClosed(previousGameId);
            qDebug() << "Game closed:" << previousGameId;
        }
    }
#else
    // Non-Windows platforms - placeholder
    Q_UNUSED(this)
#endif
}

void ProcessScanner::checkAntiCheatSystems(const QString& gamePath)
{
    Q_UNUSED(gamePath)

    // Known anti-cheat indicators
    // In production, would check for:
    // - EasyAntiCheat files
    // - BattlEye files
    // - Vanguard (Riot)
    // - etc.

    // Placeholder for now
    m_hasAntiCheat = false;
    m_antiCheatSummary.clear();
    emit antiCheatChanged();
}

QStringList ProcessScanner::getRunningProcesses()
{
    QStringList processes;

#ifdef Q_OS_WIN
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            processes.append(QString::fromWCharArray(pe32.szExeFile));
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
#endif

    return processes;
}

} // namespace makineai
