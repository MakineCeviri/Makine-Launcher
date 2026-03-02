/**
 * @file processscanner.cpp
 * @brief Process Scanner Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "processscanner.h"
#include "gameservice.h"
#include "localpackagemanager.h"
#include "profiler.h"
#include <QDebug>
#include <QDir>
#include <QtConcurrent>

#ifdef Q_OS_WIN
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
    explicit operator bool() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }

    void close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != nullptr) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE h_;
};
#endif

namespace makineai {

ProcessScanner::ProcessScanner(QObject *parent)
    : QObject(parent)
{
    connect(&m_scanTimer, &QTimer::timeout, this, &ProcessScanner::performScan);
    // No hardcoded processes — populated dynamically via rebuildProcessMap()
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
    // Update interval even if already watching (e.g. 10000ms -> 60000ms on minimize)
    if (m_isWatching && m_scanTimer.interval() == intervalMs)
        return;

    m_scanTimer.start(intervalMs);

    if (!m_isWatching) {
        m_isWatching = true;
        emit isWatchingChanged();
        performScan();
    }
}

void ProcessScanner::stopWatching()
{
    if (!m_isWatching) return;

    m_scanTimer.stop();

    m_isWatching = false;
    emit isWatchingChanged();

    qDebug() << "Process scanner stopped";
}

void ProcessScanner::rebuildProcessMap()
{
    m_knownProcesses.clear();
    m_appIdToPath.clear();

    // === Layer 1: Scan actual exe files from installed game directories ===
    // This is the most reliable source - reads real files from the user's disk.
    // Covers all installed games regardless of catalog fingerprint quality.
    if (m_gameService) {
        QVariantList games = m_gameService->games();
        for (const auto& g : games) {
            auto map = g.toMap();
            QString appId = map.value("steamAppId").toString();
            QString path = map.value("installPath").toString();
            if (appId.isEmpty() || path.isEmpty()) continue;

            m_appIdToPath.insert(appId, path);

            // Scan game directory for .exe files in known locations.
            // Game executables live in root or common subdirectories:
            //   root/, Game/, Binaries/Win64/, bin/, bin/x64/, x64/
            // Scanning a fixed list is fast (6 dir listings) and covers 99% of games.
            static const QStringList kExeDirs = {
                QStringLiteral("."),
                QStringLiteral("Game"),
                QStringLiteral("Binaries/Win64"),
                QStringLiteral("Binaries/Win32"),
                QStringLiteral("bin"),
                QStringLiteral("bin/x64"),
                QStringLiteral("x64"),
                QStringLiteral("Shipping"),
                QStringLiteral("Game/Binaries/Win64"),
            };

            // Exe names to skip (installers, redistributables, crash handlers)
            auto isToolExe = [](const QString& name) {
                return name.startsWith(QLatin1String("unins"))
                    || name.startsWith(QLatin1String("setup"))
                    || name.startsWith(QLatin1String("redist"))
                    || name.startsWith(QLatin1String("vcredist"))
                    || name.startsWith(QLatin1String("dxsetup"))
                    || name.startsWith(QLatin1String("dotnet"))
                    || name.contains(QLatin1String("prerequisite"))
                    || name.contains(QLatin1String("crashreport"))
                    || name.contains(QLatin1String("crashhandler"))
                    || name == QLatin1String("updater.exe")
                    || name == QLatin1String("ue4prerequesitiessetup.exe");
            };

            for (const QString& subDir : kExeDirs) {
                QDir dir(path + QLatin1Char('/') + subDir);
                if (!dir.exists()) continue;

                const QStringList exeFiles = dir.entryList(
                    {QStringLiteral("*.exe")}, QDir::Files, QDir::NoSort);

                for (const QString& exe : exeFiles) {
                    QString lowerExe = exe.toLower();
                    if (isToolExe(lowerExe)) continue;

                    if (!m_knownProcesses.contains(lowerExe)) {
                        m_knownProcesses.insert(lowerExe, appId);
                    }
                }
            }
        }
    }

    // === Layer 2: Catalog fingerprints (for games not in library) ===
    // Covers games the user might have installed outside detected stores.
    if (m_packageManager) {
        QVariantMap exeMap = m_packageManager->getAllExeMap();
        for (auto it = exeMap.cbegin(); it != exeMap.cend(); ++it) {
            QString lowerExe = it.key().toLower();
            // Don't overwrite disk-scanned entries (they are more accurate)
            if (!m_knownProcesses.contains(lowerExe)) {
                m_knownProcesses.insert(lowerExe, it.value().toString());
            }
        }
    }

    qDebug() << "ProcessScanner: rebuilt map with" << m_knownProcesses.size()
             << "exe entries and" << m_appIdToPath.size() << "install paths";

    // Trigger an immediate scan with new map
    if (m_isWatching) {
        performScan();
    }
}

void ProcessScanner::performScan()
{
    MAKINE_ZONE_NAMED("ProcessScanner::performScan");

    // Allow scan even with empty map (Layer 3 heavy process detection still works)

    // Snapshot running processes on a worker thread to avoid
    // CreateToolhelp32Snapshot blocking the main thread (~3.5ms).
    (void)QtConcurrent::run([this]() {
        MAKINE_ZONE_NAMED("ProcessScanner::getRunningProcesses (async)");
        QList<ProcessInfo> procs = getRunningProcesses();

        // Process matching + signal emission must happen on the main thread
        QMetaObject::invokeMethod(this, [this, procs = std::move(procs)]() {
            detectRunningGames(procs);
        }, Qt::QueuedConnection);
    });
}

void ProcessScanner::detectRunningGames(const QList<ProcessInfo>& processes)
{
#ifdef Q_OS_WIN
    bool foundGame = false;
    QString foundGameId;
    QString foundGameName;

    // Fast path: if a game is already running, only check if it is still alive.
    // This avoids iterating 300+ processes and doing hash lookups while the
    // system is under heavy load from the game.
    if (m_gameRunning && !m_runningExeName.isEmpty()) {
        bool stillRunning = false;
        for (const auto& proc : processes) {
            if (proc.exeName.toLower() == m_runningExeName) {
                stillRunning = true;
                break;
            }
        }
        if (stillRunning) return;  // Game still running, nothing changed
        // Game closed -- fall through to full scan
    }

    for (const auto& proc : processes) {
        const QString lowerExe = proc.exeName.toLower();

        if (!m_knownProcesses.contains(lowerExe))
            continue;

        QString appId = m_knownProcesses.value(lowerExe);

        // Phase 2: verify install path if available (avoid false positives)
        if (m_appIdToPath.contains(appId)) {
            QString fullPath = getProcessFullPath(proc.pid);
            if (!fullPath.isEmpty()) {
                QString expectedDir = m_appIdToPath.value(appId);
                // Normalize separators for comparison
                fullPath.replace('\\', '/');
                expectedDir.replace('\\', '/');
                if (!fullPath.startsWith(expectedDir, Qt::CaseInsensitive))
                    continue;  // Different directory — likely a different app
            }
        }

        foundGame = true;
        foundGameId = appId;
        m_runningExeName = lowerExe;

        // Get display name from catalog
        if (m_packageManager) {
            foundGameName = m_packageManager->getGameName(appId);
        }
        if (foundGameName.isEmpty()) {
            // Fallback: derive from exe name
            foundGameName = proc.exeName;
            foundGameName.remove(QLatin1String(".exe"), Qt::CaseInsensitive);
        }
        break;
    }

    // === Layer 3: Heavy process list (every 6th scan = ~60 seconds) ===
    // If no known exe matched, update the list of heavy processes (>300MB RAM)
    // for the user to manually select from. User-driven approach for accuracy.
    if (!foundGame && ++m_scanCycle % 6u == 0) {
        updateHeavyProcessList(processes);
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
            // Only emit gameDetected if the game is NOT already in the library
            bool inLibrary = false;
            if (m_gameService) {
                QVariantList games = m_gameService->games();
                for (const auto& g : games) {
                    if (g.toMap().value("steamAppId").toString() == foundGameId) {
                        inLibrary = true;
                        break;
                    }
                }
            }

            if (!inLibrary) {
                emit gameDetected(foundGameId, foundGameName);
            }
            qDebug() << "Game detected:" << foundGameName << "(" << foundGameId << ")"
                     << (inLibrary ? "[in library]" : "[NEW]");
        } else if (!foundGame && wasRunning) {
            m_runningExeName.clear();
            emit gameClosed(previousGameId);
            qDebug() << "Game closed:" << previousGameId;
        }
    }
#else
    Q_UNUSED(processes)
#endif
}

QList<ProcessScanner::ProcessInfo> ProcessScanner::getRunningProcesses()
{
    QList<ProcessInfo> processes;

#ifdef Q_OS_WIN
    HandleGuard snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot.get(), &pe32)) {
        do {
            processes.append({
                QString::fromWCharArray(pe32.szExeFile),
                pe32.th32ProcessID
            });
        } while (Process32NextW(snapshot.get(), &pe32));
    }
#endif

    return processes;
}

#ifdef Q_OS_WIN
QString ProcessScanner::getProcessFullPath(DWORD processId) const
{
    HandleGuard hProcess{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId)};
    if (!hProcess) return {};

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess.get(), 0, path, &size)) {
        return QString::fromWCharArray(path, static_cast<int>(size));
    }
    return {};
}
#endif

#ifdef Q_OS_WIN
SIZE_T ProcessScanner::getProcessMemoryUsage(DWORD processId) const
{
    HandleGuard hProcess{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                     FALSE, processId)};
    if (!hProcess) return 0;

    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(hProcess.get(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#endif

void ProcessScanner::updateHeavyProcessList(const QList<ProcessInfo>& processes)
{
#ifdef Q_OS_WIN
    static constexpr SIZE_T kMinGameMemory = 300ULL * 1024 * 1024;

    // System/tool exe names to skip
    auto isSkipExe = [](const QString& lower) {
        return lower == QLatin1String("explorer.exe")
            || lower == QLatin1String("svchost.exe")
            || lower == QLatin1String("dwm.exe")
            || lower == QLatin1String("csrss.exe")
            || lower == QLatin1String("searchhost.exe")
            || lower == QLatin1String("msedge.exe")
            || lower == QLatin1String("chrome.exe")
            || lower == QLatin1String("firefox.exe")
            || lower == QLatin1String("discord.exe")
            || lower == QLatin1String("steam.exe")
            || lower == QLatin1String("steamwebhelper.exe")
            || lower == QLatin1String("epicgameslauncher.exe")
            || lower == QLatin1String("gogalaxy.exe")
            || lower == QLatin1String("code.exe")
            || lower == QLatin1String("devenv.exe")
            || lower == QLatin1String("makineai.exe");
    };

    struct Candidate {
        QString displayName;
        QString exeName;
        qint64 pid;
        SIZE_T memory;
    };
    QList<Candidate> candidates;

    for (const auto& proc : processes) {
        QString lowerExe = proc.exeName.toLower();

        // Skip already-known (auto-detected) processes
        if (m_knownProcesses.contains(lowerExe)) continue;

        // Skip system/tool exes
        if (isSkipExe(lowerExe)) continue;

        SIZE_T mem = getProcessMemoryUsage(proc.pid);
        if (mem >= kMinGameMemory) {
            // Display name: remove .exe extension
            QString displayName = proc.exeName;
            displayName.remove(QLatin1String(".exe"), Qt::CaseInsensitive);
            candidates.append({displayName, proc.exeName,
                              static_cast<qint64>(proc.pid), mem});
        }
    }

    // Sort by memory descending
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.memory > b.memory; });

    // Build QVariantList for QML (max 5 entries)
    QVariantList newList;
    int count = 0;
    for (const auto& c : candidates) {
        if (++count > 5) break;
        QVariantMap entry;
        entry[QStringLiteral("name")] = c.displayName;
        entry[QStringLiteral("exeName")] = c.exeName;
        entry[QStringLiteral("pid")] = c.pid;
        entry[QStringLiteral("memoryMB")] = static_cast<int>(c.memory / (1024 * 1024));
        newList.append(entry);
    }

    if (newList != m_heavyProcesses) {
        m_heavyProcesses = newList;
        emit heavyProcessesChanged();
        qDebug() << "ProcessScanner: updated heavy process list,"
                 << newList.size() << "candidates";
    }
#else
    Q_UNUSED(processes)
#endif
}
QString ProcessScanner::resolveSelectedProcess(qint64 pid)
{
#ifdef Q_OS_WIN
    if (!m_packageManager) return {};

    DWORD dwPid = static_cast<DWORD>(pid);
    QString fullPath = getProcessFullPath(dwPid);
    if (fullPath.isEmpty()) return {};

    fullPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Extract exe name and game directory
    int lastSlash = fullPath.lastIndexOf(QLatin1Char('/'));
    if (lastSlash < 0) return {};
    QString gameDir = fullPath.left(lastSlash);

    // Walk up directories (max 3 levels) looking for a catalog match
    QString checkDir = gameDir;
    for (int depth = 0; depth < 3; ++depth) {
        QDir dir(checkDir);
        if (!dir.exists()) break;

        QStringList topEntries;
        const auto entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& e : entries) topEntries.append(e.toLower());

        QStringList exeNames;
        const auto exes = dir.entryList({QStringLiteral("*.exe")}, QDir::Files);
        for (const auto& e : exes) exeNames.append(e.toLower());

        QString folderName = dir.dirName();

        QVariantList matches = m_packageManager->findMatchingGamesFromFiles(
            exeNames, {}, topEntries, folderName);

        if (!matches.isEmpty()) {
            auto bestMatch = matches.first().toMap();
            int confidence = bestMatch.value(QStringLiteral("confidence")).toInt();
            if (confidence >= 40) {
                QString appId = bestMatch.value(QStringLiteral("steamAppId")).toString();
                QString gameName = m_packageManager->getGameName(appId);

                // Cache for future auto-detection
                QString lowerExe = fullPath.mid(lastSlash + 1).toLower();
                m_knownProcesses.insert(lowerExe, appId);
                m_appIdToPath.insert(appId, checkDir);

                qDebug() << "ProcessScanner: user resolved process PID" << pid
                         << "as" << appId << gameName
                         << "(confidence:" << confidence << ")";

                emit processResolved(appId, gameName, checkDir);
                return checkDir;
            }
        }

        // Go up one directory level
        int upSlash = checkDir.lastIndexOf(QLatin1Char('/'));
        if (upSlash <= 0) break;
        checkDir = checkDir.left(upSlash);
    }

    qDebug() << "ProcessScanner: could not resolve process PID" << pid;
#else
    Q_UNUSED(pid)
#endif
    return {};
}
} // namespace makineai
