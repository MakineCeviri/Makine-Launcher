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

// RAII wrapper for Windows HANDLE (snapshot, process, etc.)
class HandleGuard {
public:
    explicit HandleGuard(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~HandleGuard() { close(); }
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    HANDLE get() const { return h_; }
    explicit operator bool() const { return h_ != INVALID_HANDLE_VALUE; }

    void close() {
        if (h_ != INVALID_HANDLE_VALUE) { CloseHandle(h_); h_ = INVALID_HANDLE_VALUE; }
    }

private:
    HANDLE h_;
};
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
    if (!m_scanTimer) {
        m_scanTimer = new QTimer(this);
        connect(m_scanTimer, &QTimer::timeout, this, &ProcessScanner::performScan);
    }

    // Update interval even if already watching (e.g. 3000ms → 30000ms on minimize)
    if (m_isWatching && m_scanTimer->interval() == intervalMs)
        return;

    m_scanTimer->start(intervalMs);

    if (!m_isWatching) {
        m_isWatching = true;
        emit isWatchingChanged();
        performScan();
    }
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
    // Non-Windows platforms - no process scanning
#endif
}

QStringList ProcessScanner::getRunningProcesses()
{
    QStringList processes;

#ifdef Q_OS_WIN
    HandleGuard snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot.get(), &pe32)) {
        do {
            processes.append(QString::fromWCharArray(pe32.szExeFile));
        } while (Process32NextW(snapshot.get(), &pe32));
    }
#endif

    return processes;
}

} // namespace makineai
