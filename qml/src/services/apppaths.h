#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>

namespace makineai {

/**
 * Centralized application path management.
 *
 * Directory layout under AppData/Local/MakineAI/:
 *   logs/             - Debug and crash logs
 *   cache/            - Transient data (game cache, Steam details)
 *   data/             - Persistent state (installed packages, journal)
 *   backups/          - Game file backups before patching
 *   update_detection/ - Hash snapshots for update tracking
 *   crash-reports/    - Crash dumps and diagnostic info
 *
 * Temp directory (AppData/Local/Temp/MakineAI/):
 *   images/           - Steam header image disk cache
 *   MakineAI-update/  - Downloaded installer files
 */
class AppPaths {
public:
    // Root: AppData/Local/MakineAI
    static QString root() {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }

    // --- Subdirectories ---

    static QString logsDir()            { return root() + QStringLiteral("/logs"); }
    static QString cacheDir()           { return root() + QStringLiteral("/cache"); }
    static QString dataDir()            { return root() + QStringLiteral("/data"); }
    static QString backupsDir()         { return root() + QStringLiteral("/backups"); }
    static QString updateDetectionDir() { return root() + QStringLiteral("/update_detection"); }
    static QString crashReportsDir()    { return root() + QStringLiteral("/crash-reports"); }

    // Temp: AppData/Local/Temp/MakineAI
    static QString tempRoot() {
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
               + QStringLiteral("/MakineAI");
    }
    static QString imageCacheDir()  { return tempRoot() + QStringLiteral("/images"); }
    static QString downloadTempDir(){ return tempRoot() + QStringLiteral("/downloads"); }
    static QString updateTempDir()  { return tempRoot() + QStringLiteral("/MakineAI-update"); }

    // --- Concrete file paths ---

    static QString debugLog()             { return logsDir()  + QStringLiteral("/makineai_debug.log"); }
    static QString gamesCacheFile()       { return cacheDir() + QStringLiteral("/games_cache.json"); }
    static QString steamDetailsCacheFile(){ return cacheDir() + QStringLiteral("/steam_details_cache.json"); }
    static QString manifestIndexFile()    { return cacheDir() + QStringLiteral("/manifest_index.json"); }
    static QString manifestEtagFile()     { return cacheDir() + QStringLiteral("/manifest_etag.txt"); }
    static QString packageDetailDir()     { return cacheDir() + QStringLiteral("/packages"); }
    static QString packagesDir()         { return root() + QStringLiteral("/packages"); }
    static QString installedPackagesFile(){ return dataDir()  + QStringLiteral("/installed_packages.json"); }
    static QString pendingOperationFile() { return dataDir()  + QStringLiteral("/pending_operation.json"); }
    static QString perfReportFile()      { return logsDir()  + QStringLiteral("/perf_report.json"); }

    // --- Bootstrap ---

    /// Create all required directories. Call once at startup.
    static void ensureDirectories() {
        const QStringList dirs = {
            logsDir(), cacheDir(), dataDir(), backupsDir(),
            updateDetectionDir(), crashReportsDir(),
            imageCacheDir(), downloadTempDir(), updateTempDir(), packageDetailDir(),
            packagesDir()
        };
        for (const auto& d : dirs)
            QDir().mkpath(d);
    }

    /// Migrate files from old flat layout to new organized layout.
    /// Safe to call multiple times — skips if source doesn't exist or dest already exists.
    static void migrateFromFlatLayout() {
        const QString oldRoot = root();

        auto moveIfNeeded = [&](const QString& oldName, const QString& newPath) {
            const QString oldPath = oldRoot + "/" + oldName;
            if (QFile::exists(oldPath) && !QFile::exists(newPath)) {
                // Ensure destination directory exists
                QDir().mkpath(QFileInfo(newPath).absolutePath());
                QFile::rename(oldPath, newPath);
            }
        };

        moveIfNeeded("makineai_debug.log",       debugLog());
        moveIfNeeded("games_cache.json",          gamesCacheFile());
        moveIfNeeded("steam_details_cache.json",  steamDetailsCacheFile());
        moveIfNeeded("installed_packages.json",   installedPackagesFile());
        moveIfNeeded("pending_operation.json",    pendingOperationFile());
        // backups/ and update_detection/ stay in same location — no migration needed
    }
};

} // namespace makineai
