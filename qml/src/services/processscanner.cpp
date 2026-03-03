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

    // === Layer 3: GPU process list (every 6th scan = ~60 seconds) ===
    // If no known exe matched, find GPU-using processes (DirectX/Vulkan)
    // for the user to manually select from.
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
bool ProcessScanner::isProcessUsingGpu(DWORD processId) const
{
    // Games load 3D rendering DLLs. Enumerate loaded modules to detect GPU usage.
    HandleGuard snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                                  processId)};
    if (!snapshot) return false;

    MODULEENTRY32W me32;
    me32.dwSize = sizeof(MODULEENTRY32W);

    if (!Module32FirstW(snapshot.get(), &me32))
        return false;

    do {
        QString moduleName = QString::fromWCharArray(me32.szModule).toLower();

        // DirectX 9/11/12 and Vulkan rendering DLLs
        if (moduleName == QLatin1String("d3d11.dll")
            || moduleName == QLatin1String("d3d12.dll")
            || moduleName == QLatin1String("d3d9.dll")
            || moduleName == QLatin1String("vulkan-1.dll")
            || moduleName == QLatin1String("d3d12core.dll")) {
            return true;
        }
    } while (Module32NextW(snapshot.get(), &me32));

    return false;
}
#endif

#ifdef Q_OS_WIN
namespace {
struct VisibleWindowData {
    DWORD targetPid;
    bool found;
};

BOOL CALLBACK findVisibleWindowCb(HWND hwnd, LPARAM lParam)
{
    auto* data = reinterpret_cast<VisibleWindowData*>(lParam);

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != data->targetPid) return TRUE;

    if (!IsWindowVisible(hwnd)) return TRUE;

    // Skip tiny windows (tooltips, tray icons, toolbar widgets)
    RECT rect;
    if (GetWindowRect(hwnd, &rect)) {
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;
        if (w >= 200 && h >= 200) {
            data->found = true;
            return FALSE;  // Stop enumeration
        }
    }
    return TRUE;
}
} // anonymous namespace

bool ProcessScanner::hasVisibleWindow(DWORD processId) const
{
    VisibleWindowData data{processId, false};
    EnumWindows(findVisibleWindowCb, reinterpret_cast<LPARAM>(&data));
    return data.found;
}
#endif

void ProcessScanner::updateHeavyProcessList(const QList<ProcessInfo>& processes)
{
#ifdef Q_OS_WIN
    // Multi-signal game detection for unknown processes.
    // Filter chain (ordered cheapest to most expensive):
    //   1. Skip known exe names (hash lookup, Layer 1/2)
    //   2. Skip by name patterns (exact list + prefix/suffix/contains)
    //   3. Skip by install path (system directories, Microsoft, tools)
    //   4. Must load a 3D rendering DLL (DirectX/Vulkan — module enumeration)
    //   5. Must have a visible window >= 200x200

    auto isSkipExe = [](const QString& lower) {
        // --- Exact matches: common GPU-using non-game processes ---
        if (lower == QLatin1String("explorer.exe")
            || lower == QLatin1String("svchost.exe")
            || lower == QLatin1String("dwm.exe")
            || lower == QLatin1String("csrss.exe")
            || lower == QLatin1String("lsass.exe")
            || lower == QLatin1String("winlogon.exe")
            || lower == QLatin1String("wininit.exe")
            || lower == QLatin1String("conhost.exe")
            || lower == QLatin1String("dllhost.exe")
            || lower == QLatin1String("searchhost.exe")
            || lower == QLatin1String("runtimebroker.exe")
            || lower == QLatin1String("sihost.exe")
            || lower == QLatin1String("taskhostw.exe")
            || lower == QLatin1String("taskmgr.exe")
            || lower == QLatin1String("applicationframehost.exe")
            || lower == QLatin1String("systemsettings.exe")
            || lower == QLatin1String("shellexperiencehost.exe")
            || lower == QLatin1String("startmenuexperiencehost.exe")
            || lower == QLatin1String("textinputhost.exe")
            || lower == QLatin1String("widgetservice.exe")
            || lower == QLatin1String("widgets.exe")
            || lower == QLatin1String("wmiprvse.exe")
            || lower == QLatin1String("fontdrvhost.exe")
            || lower == QLatin1String("audiodg.exe")
            || lower == QLatin1String("ctfmon.exe")
            || lower == QLatin1String("securityhealthservice.exe")
            || lower == QLatin1String("smartscreen.exe")
            || lower == QLatin1String("searchapp.exe")
            || lower == QLatin1String("searchui.exe")
            || lower == QLatin1String("lockapp.exe")
            || lower == QLatin1String("yourphone.exe")
            || lower == QLatin1String("phoneexperiencehost.exe")
            || lower == QLatin1String("peopleexperiencehost.exe")
            || lower == QLatin1String("mspaint.exe")
            || lower == QLatin1String("mstsc.exe")
            || lower == QLatin1String("snippingtool.exe")
            || lower == QLatin1String("screenclippinghost.exe")
            || lower == QLatin1String("calculator.exe")
            || lower == QLatin1String("notepad.exe")
            || lower == QLatin1String("cmd.exe")
            || lower == QLatin1String("powershell.exe")
            || lower == QLatin1String("pwsh.exe")
            || lower == QLatin1String("windowsterminal.exe")
            || lower == QLatin1String("winget.exe")
            || lower == QLatin1String("wt.exe")
            // Browsers
            || lower == QLatin1String("msedge.exe")
            || lower == QLatin1String("chrome.exe")
            || lower == QLatin1String("firefox.exe")
            || lower == QLatin1String("opera.exe")
            || lower == QLatin1String("brave.exe")
            || lower == QLatin1String("vivaldi.exe")
            || lower == QLatin1String("arc.exe")
            || lower == QLatin1String("tor.exe")
            // Communication / Media
            || lower == QLatin1String("discord.exe")
            || lower == QLatin1String("slack.exe")
            || lower == QLatin1String("teams.exe")
            || lower == QLatin1String("ms-teams.exe")
            || lower == QLatin1String("zoom.exe")
            || lower == QLatin1String("telegram.exe")
            || lower == QLatin1String("whatsapp.exe")
            || lower == QLatin1String("signal.exe")
            || lower == QLatin1String("spotify.exe")
            || lower == QLatin1String("vlc.exe")
            || lower == QLatin1String("mpv.exe")
            || lower == QLatin1String("potplayer.exe")
            || lower == QLatin1String("potplayer64.exe")
            || lower == QLatin1String("potplayermini.exe")
            || lower == QLatin1String("potplayermini64.exe")
            || lower == QLatin1String("wmplayer.exe")
            || lower == QLatin1String("video.ui.exe")
            || lower == QLatin1String("microsoft.photos.exe")
            || lower == QLatin1String("microsoft.media.player.exe")
            // Game launchers / Stores
            || lower == QLatin1String("steam.exe")
            || lower == QLatin1String("steamwebhelper.exe")
            || lower == QLatin1String("epicgameslauncher.exe")
            || lower == QLatin1String("gogalaxy.exe")
            || lower == QLatin1String("gogalaxycommunication.exe")
            || lower == QLatin1String("eadesktop.exe")
            || lower == QLatin1String("ubisoftconnect.exe")
            || lower == QLatin1String("upc.exe")
            || lower == QLatin1String("origin.exe")
            || lower == QLatin1String("originwebhelperservice.exe")
            || lower == QLatin1String("battle.net.exe")
            || lower == QLatin1String("xboxpcapp.exe")
            || lower == QLatin1String("gamingservices.exe")
            || lower == QLatin1String("gamingservicesnet.exe")
            || lower == QLatin1String("gamebar.exe")
            || lower == QLatin1String("gamebarftserver.exe")
            || lower == QLatin1String("gamebarpresencewriter.exe")
            || lower == QLatin1String("gamelauncher.exe")
            || lower == QLatin1String("playnite.desktopapp.exe")
            || lower == QLatin1String("launchpad.exe")
            // Dev / IDE
            || lower == QLatin1String("code.exe")
            || lower == QLatin1String("devenv.exe")
            || lower == QLatin1String("rider64.exe")
            || lower == QLatin1String("clion64.exe")
            || lower == QLatin1String("idea64.exe")
            || lower == QLatin1String("androidstudio64.exe")
            || lower == QLatin1String("sublime_text.exe")
            // Streaming / Recording / Overlay
            || lower == QLatin1String("obs64.exe")
            || lower == QLatin1String("obs32.exe")
            || lower == QLatin1String("streamlabs obs.exe")
            || lower == QLatin1String("xsplit.core.exe")
            || lower == QLatin1String("wallpaperengine.exe")
            || lower == QLatin1String("wallpaperengine32.exe")
            || lower == QLatin1String("lghub.exe")
            || lower == QLatin1String("icue.exe")
            || lower == QLatin1String("synapse3.exe")
            || lower == QLatin1String("steelseries gg.exe")
            // 3D / Design / Video editing (GPU-heavy non-games)
            || lower == QLatin1String("blender.exe")
            || lower == QLatin1String("afterfx.exe")
            || lower == QLatin1String("premiere pro.exe")
            || lower == QLatin1String("adobe premiere pro.exe")
            || lower == QLatin1String("resolve.exe")
            || lower == QLatin1String("davinci resolve.exe")
            || lower == QLatin1String("photoshop.exe")
            || lower == QLatin1String("illustrator.exe")
            || lower == QLatin1String("3dsmax.exe")
            || lower == QLatin1String("maya.exe")
            || lower == QLatin1String("cinema 4d.exe")
            || lower == QLatin1String("sketchup.exe")
            || lower == QLatin1String("zbrush.exe")
            || lower == QLatin1String("substancepainter.exe")
            || lower == QLatin1String("houdini.exe")
            || lower == QLatin1String("fusion360.exe")
            || lower == QLatin1String("solidworks.exe")
            || lower == QLatin1String("autocad.exe")
            // GPU monitoring / benchmarking
            || lower == QLatin1String("gpu-z.exe")
            || lower == QLatin1String("msiafterburner.exe")
            || lower == QLatin1String("hwinfo64.exe")
            || lower == QLatin1String("hwinfo32.exe")
            || lower == QLatin1String("hwmonitor.exe")
            || lower == QLatin1String("gpumon.exe")
            || lower == QLatin1String("furmark.exe")
            || lower == QLatin1String("kombustor.exe")
            // Self
            || lower == QLatin1String("makineai.exe")) {
            return true;
        }

        // --- Prefix patterns: entire families of non-game processes ---

        // Microsoft processes (UWP, system)
        if (lower.startsWith(QLatin1String("microsoft."))   // UWP: microsoft.photos, microsoft.xbox
            || lower.startsWith(QLatin1String("windows"))    // windowsterminal, windows.ui
            // GPU driver families
            || lower.startsWith(QLatin1String("nvidia"))
            || lower.startsWith(QLatin1String("nv"))         // nvcontainer, nvdisplay, nvtelemetry
            || lower.startsWith(QLatin1String("amd"))
            || lower.startsWith(QLatin1String("radeon"))
            || lower.startsWith(QLatin1String("igfx"))       // Intel GPU
            || lower.startsWith(QLatin1String("intel"))
            // Adobe family
            || lower.startsWith(QLatin1String("adobe"))
            || lower.startsWith(QLatin1String("creative cloud"))
            || lower.startsWith(QLatin1String("cclibrary"))) {
            return true;
        }

        // --- Suffix patterns: process role indicators ---
        if (lower.endsWith(QLatin1String("helper.exe"))
            || lower.endsWith(QLatin1String("service.exe"))
            || lower.endsWith(QLatin1String("services.exe"))
            || lower.endsWith(QLatin1String("broker.exe"))
            || lower.endsWith(QLatin1String("host.exe"))
            || lower.endsWith(QLatin1String("server.exe"))
            || lower.endsWith(QLatin1String("daemon.exe"))
            || lower.endsWith(QLatin1String("agent.exe"))
            || lower.endsWith(QLatin1String("updater.exe"))
            || lower.endsWith(QLatin1String("installer.exe"))
            || lower.endsWith(QLatin1String("setup.exe"))
            || lower.endsWith(QLatin1String("launcher.exe"))
            || lower.endsWith(QLatin1String("overlay.exe"))
            || lower.endsWith(QLatin1String("tray.exe"))
            || lower.endsWith(QLatin1String("monitor.exe"))
            || lower.endsWith(QLatin1String("manager.exe"))
            || lower.endsWith(QLatin1String("watcher.exe"))
            || lower.endsWith(QLatin1String("controller.exe"))
            || lower.endsWith(QLatin1String("bootstrap.exe"))
            || lower.endsWith(QLatin1String("worker.exe"))) {
            return true;
        }

        // --- Contains patterns: catch-all for helper/system substrings ---
        if (lower.contains(QLatin1String("crash"))
            || lower.contains(QLatin1String("webhelper"))
            || lower.contains(QLatin1String("cefprocess"))
            || lower.contains(QLatin1String("renderer"))
            || lower.contains(QLatin1String("anticheat"))
            || lower.contains(QLatin1String("anti-cheat"))
            || lower.contains(QLatin1String("easyanticheat"))
            || lower.contains(QLatin1String("battleye"))
            || lower.contains(QLatin1String("vanguard"))
            || lower.contains(QLatin1String("update"))
            || lower.contains(QLatin1String("uninstall"))) {
            return true;
        }

        return false;
    };

    // Check if a process path belongs to a system/tool directory (not a game)
    auto isSystemPath = [this](DWORD pid) -> bool {
        QString path = getProcessFullPath(pid);
        if (path.isEmpty()) return false;  // Can't determine — allow through

        QString lp = path.toLower();
        lp.replace(QLatin1Char('\\'), QLatin1Char('/'));

        // Windows system directories — NEVER contain games
        if (lp.startsWith(QLatin1String("c:/windows/"))
            || lp.startsWith(QLatin1String("c:/program files/common files/"))
            || lp.startsWith(QLatin1String("c:/program files (x86)/common files/"))
            || lp.startsWith(QLatin1String("c:/programdata/"))) {
            return true;
        }

        // Microsoft Store / UWP apps (games here would be in catalog already)
        if (lp.contains(QLatin1String("/windowsapps/"))) {
            return true;
        }

        // Microsoft tool directories
        if (lp.contains(QLatin1String("/microsoft/"))
            || lp.contains(QLatin1String("/microsoft "))  // "Program Files/Microsoft Visual..."
            || lp.contains(QLatin1String("/windows kits/"))
            || lp.contains(QLatin1String("/windows nt/"))) {
            return true;
        }

        // Known non-game program folders
        if (lp.contains(QLatin1String("/adobe/"))
            || lp.contains(QLatin1String("/autodesk/"))
            || lp.contains(QLatin1String("/blender foundation/"))
            || lp.contains(QLatin1String("/davinci resolve/"))
            || lp.contains(QLatin1String("/google/chrome/"))
            || lp.contains(QLatin1String("/mozilla firefox/"))
            || lp.contains(QLatin1String("/obs studio/"))
            || lp.contains(QLatin1String("/nvidia corporation/"))
            || lp.contains(QLatin1String("/amd/"))
            || lp.contains(QLatin1String("/jetbrains/"))) {
            return true;
        }

        return false;
    };

    struct Candidate {
        QString displayName;
        QString exeName;
        qint64 pid;
    };
    QList<Candidate> candidates;

    for (const auto& proc : processes) {
        QString lowerExe = proc.exeName.toLower();

        // Filter 1: Skip already-known (auto-detected) processes
        if (m_knownProcesses.contains(lowerExe)) continue;

        // Filter 2: Skip by exe name patterns
        if (isSkipExe(lowerExe)) continue;

        // Filter 3: Skip system/tool directories
        if (isSystemPath(proc.pid)) continue;

        // Filter 4: Must load a 3D rendering DLL (DirectX 9/11/12, Vulkan)
        if (!isProcessUsingGpu(proc.pid)) continue;

        // Filter 5: Must have a visible window (>= 200x200)
        if (!hasVisibleWindow(proc.pid)) continue;

        QString displayName = proc.exeName;
        displayName.remove(QLatin1String(".exe"), Qt::CaseInsensitive);
        candidates.append({displayName, proc.exeName,
                          static_cast<qint64>(proc.pid)});
    }

    // Max 3 entries — with 5-layer filter, false positives are near-zero
    QVariantList newList;
    int count = 0;
    for (const auto& c : candidates) {
        if (++count > 3) break;
        QVariantMap entry;
        entry[QStringLiteral("name")] = c.displayName;
        entry[QStringLiteral("exeName")] = c.exeName;
        entry[QStringLiteral("pid")] = c.pid;
        newList.append(entry);
    }

    if (newList != m_heavyProcesses) {
        m_heavyProcesses = newList;
        emit heavyProcessesChanged();
        qDebug() << "ProcessScanner: GPU process list updated,"
                 << newList.size() << "game candidates";
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

    // No catalog match — game is not supported
    // Extract display name from the exe path for the notification
    QString exeName = fullPath.mid(lastSlash + 1);
    exeName.remove(QLatin1String(".exe"), Qt::CaseInsensitive);
    emit processNotSupported(exeName);

    qDebug() << "ProcessScanner: process PID" << pid << "not in catalog (" << exeName << ")";
#else
    Q_UNUSED(pid)
#endif
    return {};
}
} // namespace makineai
