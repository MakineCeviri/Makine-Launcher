/**
 * @file selfupdater.cpp
 * @brief Win32 self-swap implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "selfupdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSharedMemory>
#include <QLoggingCategory>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Softpub.h>
#include <wintrust.h>
#pragma comment(lib, "wintrust")
#endif

Q_LOGGING_CATEGORY(lcSelfUpdater, "makine.updater")

namespace makine {

bool SelfUpdater::swapExecutable(const QString& newExePath)
{
#ifdef Q_OS_WIN
    QString appPath = QCoreApplication::applicationFilePath();
    QString oldPath = appPath + QStringLiteral(".old");

    std::wstring wAppPath = appPath.toStdWString();
    std::wstring wOldPath = oldPath.toStdWString();
    std::wstring wNewPath = newExePath.toStdWString();

    // Delete previous .old if exists
    DeleteFileW(wOldPath.c_str());

    // Rename current EXE to .old (NTFS allows renaming a running EXE)
    if (!MoveFileW(wAppPath.c_str(), wOldPath.c_str())) {
        DWORD err = GetLastError();
        qCWarning(lcSelfUpdater) << "SelfUpdater: Failed to rename current EXE, error:" << err;
        return false;
    }

    // Move new EXE into place
    if (!MoveFileW(wNewPath.c_str(), wAppPath.c_str())) {
        DWORD err = GetLastError();
        qCWarning(lcSelfUpdater) << "SelfUpdater: Failed to move new EXE, error:" << err;
        // Rollback: restore the original. If this also fails, the app EXE
        // is missing — drop a marker so swapAndRestart can surface the brick.
        if (!MoveFileW(wOldPath.c_str(), wAppPath.c_str())) {
            DWORD rollbackErr = GetLastError();
            qCCritical(lcSelfUpdater) << "SelfUpdater: ROLLBACK FAILED — app EXE missing!"
                                      << "Backup at:" << oldPath
                                      << "Rollback error:" << rollbackErr;
            QFile marker(appPath + QStringLiteral(".rollback_failed"));
            if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                const QByteArray msg = QStringLiteral(
                    "Makine Launcher self-update rollback failed.\n"
                    "Original EXE preserved at: %1\n"
                    "Move/Win32 error code: %2\n").arg(oldPath).arg(rollbackErr).toUtf8();
                marker.write(msg);
            }
        } else {
            qCWarning(lcSelfUpdater) << "SelfUpdater: rolled back to original EXE";
        }
        return false;
    }

    // Schedule .old for deletion on reboot
    MoveFileExW(wOldPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    return true;
#else
    Q_UNUSED(newExePath)
    qCWarning(lcSelfUpdater) << "SelfUpdater: swapExecutable not implemented on this platform";
    return false;
#endif
}

bool SelfUpdater::launchDetached(const QString& exePath, const QStringList& args)
{
#ifdef Q_OS_WIN
    QString cmdLine = QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(exePath));
    for (const auto& arg : args)
        cmdLine += QStringLiteral(" %1").arg(arg);

    std::wstring wCmdLine = cmdLine.toStdWString();

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL ok = CreateProcessW(nullptr, wCmdLine.data(), nullptr, nullptr, FALSE,
                             CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                             nullptr, nullptr, &si, &pi);

    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread)  CloseHandle(pi.hThread);

    return ok != 0;
#else
    Q_UNUSED(exePath)
    Q_UNUSED(args)
    return false;
#endif
}

void SelfUpdater::releaseInstanceGuard()
{
    QSharedMemory guard(QStringLiteral("MakineLauncher_SingleInstance_Guard"));
    if (guard.attach())
        guard.detach();
}

bool SelfUpdater::verifySignature(const QString& filePath)
{
#ifdef Q_OS_WIN
    std::wstring widePath = filePath.toStdWString();

    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = widePath.c_str();

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_SAFER_FLAG;

    LONG result = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    // Clean up state
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    return (result == ERROR_SUCCESS);
#else
    Q_UNUSED(filePath)
    return true; // No signature verification on non-Windows
#endif
}

void SelfUpdater::swapAndRestart(const QString& newExePath)
{
    releaseInstanceGuard();

    if (!swapExecutable(newExePath)) {
        qCWarning(lcSelfUpdater) << "SelfUpdater: swap failed, aborting restart";
#ifdef Q_OS_WIN
        const QString appPath = QCoreApplication::applicationFilePath();
        const QString markerPath = appPath + QStringLiteral(".rollback_failed");
        if (QFile::exists(markerPath)) {
            const QString backupPath = appPath + QStringLiteral(".old");
            const std::wstring wTitle = L"Makine Launcher — Update Failed";
            const std::wstring wMsg = QStringLiteral(
                "Self-update failed and rollback could not restore the original EXE.\n\n"
                "The launcher is currently broken. Manual restore:\n"
                "1) Close this dialog.\n"
                "2) Rename:\n      %1\n   back to:\n      %2\n").arg(backupPath, appPath).toStdWString();
            MessageBoxW(nullptr, wMsg.c_str(), wTitle.c_str(), MB_ICONERROR | MB_OK);
        }
#endif
        // Cannot return from [[noreturn]] — force exit
        ::_exit(1);
    }

    QString appPath = QCoreApplication::applicationFilePath();
    launchDetached(appPath, {QStringLiteral("--post-update")});

    qCDebug(lcSelfUpdater) << "SelfUpdater: Self-swap complete, new version launching";

#ifdef Q_OS_WIN
    // Brief sleep to let the OS finish spawning the child process
    Sleep(500);
#endif
    ::_exit(0);
}

} // namespace makine
