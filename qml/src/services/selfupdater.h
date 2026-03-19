/**
 * @file selfupdater.h
 * @brief Platform-specific executable self-swap and restart utilities
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Isolates Win32-specific self-update mechanics (rename running EXE,
 * move new EXE into place, launch child process, terminate).
 * All functions are static and stateless.
 */

#pragma once

#include <QString>
#include <QStringList>

namespace makine {

struct SelfUpdater {
    /**
     * @brief Rename running EXE to .old, move newExePath into its place.
     * Uses NTFS rename-while-running. Rolls back on failure.
     * @return true on success
     */
    static bool swapExecutable(const QString& newExePath);

    /**
     * @brief Launch an executable with CreateProcessW + DETACHED_PROCESS.
     * Reliable even when the calling process is about to _exit().
     * @return true if the child process was created
     */
    static bool launchDetached(const QString& exePath, const QStringList& args);

    /**
     * @brief Release the QSharedMemory single-instance guard.
     * Must be called before _exit() since destructors are skipped.
     */
    static void releaseInstanceGuard();

    /**
     * @brief Verify Authenticode digital signature (Windows only).
     * @return true if the file has a valid, trusted signature
     */
    static bool verifySignature(const QString& filePath);

    /**
     * @brief Full self-update flow: release guard -> swap -> launch -> _exit(0).
     * Does NOT return.
     */
    [[noreturn]] static void swapAndRestart(const QString& newExePath);
};

} // namespace makine
