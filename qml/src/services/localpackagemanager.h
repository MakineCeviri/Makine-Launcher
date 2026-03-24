#pragma once

/**
 * @file localpackagemanager.h
 * @brief Local translation package management
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Manages translation packages via network-based catalog.
 * Delegates catalog/query operations to makine::packages::PackageCatalog.
 * Install/uninstall file operations stay in this QML service.
 */

#include <QObject>
#include <QHash>
#include <QString>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <optional>
#include <atomic>
#include <functional>
#include <set>
#include <string>

#include <makine/package_catalog.hpp>

namespace makine {

class OperationJournal;

struct InstallStep {
    QString action;     // "copy", "copyDir", "run", "delete", "installFont", "setSteamLanguage", "copyToDesktop", "rename"
    QString src;        // source file/dir (relative to package dir)
    QString dest;       // destination (relative to game dir) — for "copy"/"delete"
    QString exe;        // executable to run — for "run"
    QStringList args;   // arguments — for "run"
    QString fallback;   // fallback executable if primary not found — for "run"
    QString workDir;    // "game" (default) or "package" — for "run"
    QString language;   // Steam language name — for "setSteamLanguage"
};

struct InstallOptionQt {
    QString id;              // "patch", "dubbing"
    QString label;           // "Türkçe Yama"
    QString description;     // "Metin çevirisi, fontlar"
    QString icon;            // "text", "voice"
    bool defaultSelected{false};
    QString subDir;          // Subdirectory in package dir
    QList<InstallStep> steps;
};

struct VariantConfigQt {
    QList<InstallOptionQt> installOptions;
    QMap<QString, QList<InstallStep>> combinedSteps;
};

struct PackageInfo {
    QString packageId;   // e.g. "SF10310Hv19"
    QString steamAppId;  // e.g. "1716740"
    QString gameName;
    QString engine;
    QString version;
    QString installType; // "overlay", "runtime", "replace"
    QString tier;        // "free" or "plus"
    QString lastUpdated; // ISO date
    qint64 sizeBytes{0};
    int fileCount{0};
    QHash<QString, QString> storeIds; // store -> id (e.g. "epic" -> "abc123")
    QString dirName;                // filesystem directory name in translation_data
    QStringList variants;           // ["1.00", "1.04"] or ["Steam", "Gamepass"]
    QString variantType;            // "version" or "platform" (for UI label)
    QVariantList contributors;      // [{name, role}] from manifest
    QList<InstallStep> installSteps; // custom install recipe (empty = default overlay)
    QString installMethodType;      // "script", "userPath", "options" (empty = default overlay)
    QString installMethodTarget;    // for "userPath": target relative to user home
    QString installNotes;           // pre-install notes shown to user (e.g. "Change language to English")
    QList<InstallOptionQt> installOptions;  // for "options" type
    QMap<QString, QList<InstallStep>> combinedSteps; // e.g. "dubbing+patch" -> steps
    QString specialDialog;          // "" or "eldenRing"
    QMap<QString, VariantConfigQt> variantInstallOptions; // variant -> options config
};

struct InstalledPackageInfo {
    QString version;
    QString gamePath;
    QStringList installedFiles;     // All files (compatibility)
    QStringList addedFiles;         // New files (didn't exist in game)
    QStringList replacedFiles;      // Overwritten files (existed, backed up)
    QString gameStoreVersion;       // Store buildid at install time
    QString gameSource;             // "steam", "epic", "gog"
    qint64 installedAt{0};
};

class LocalPackageManager : public QObject
{
    Q_OBJECT

public:
    explicit LocalPackageManager(QObject *parent = nullptr);

    bool loadFromIndex(const QString& indexPath, const QString& packageCacheRoot);
    bool enrichPackageDetail(const QString& steamAppId, const QByteArray& jsonData);
    bool isDetailLoaded(const QString& steamAppId) const;

    bool hasPackage(const QString& steamAppId) const;
    std::optional<PackageInfo> getPackage(const QString& steamAppId) const;

    bool isInstalled(const QString& steamAppId) const;
    std::optional<InstalledPackageInfo> getInstalledInfo(const QString& steamAppId) const;
    void updateInstalledStoreVersion(const QString& steamAppId, const QString& storeVersion,
                                      const QString& source);

    void installPackage(const QString& steamAppId, const QString& gamePath,
                        const QString& variant = {},
                        const QStringList& selectedOptions = {});
    void updatePackage(const QString& steamAppId, const QString& gamePath,
                       const QString& variant = {},
                       const QStringList& selectedOptions = {});
    void cancelInstall();
    bool uninstallPackage(const QString& steamAppId, const QString& gamePath);

    // Resolve any store ID (epic_xxx, gog_xxx, steamAppId) to canonical steamAppId
    QString resolveGameId(const QString& gameId) const;

    // Variant support
    QVariantList getVariants(const QString& steamAppId) const;
    QString getVariantType(const QString& steamAppId) const;

    // Get list of relative file paths in the translation package
    QStringList getPackageFileList(const QString& steamAppId, const QString& variant = {}) const;

    // Match a folder name against package catalog (case-insensitive, matches gameName or dirName)
    QString findMatchingAppId(const QString& folderName) const;

    // Match a game folder against catalog fingerprints using multi-signal scoring
    QVariantList findMatchingGamesFromFiles(const QStringList& exeNames,
                                            const QString& engine,
                                            const QStringList& topEntries,
                                            const QString& folderName = {}) const;

    void setJournal(OperationJournal* journal) { m_journal = journal; }

    int packageCount() const;
    QVariantList allPackagesAsList() const;

    // Build exe name -> steamAppId map from catalog fingerprints (for ProcessScanner)
    QVariantMap getAllExeMap() const;

    // Get game name by steamAppId (for ProcessScanner display)
    QString getGameName(const QString& steamAppId) const;

signals:
    void installProgress(double progress, const QString& status);
    void installCompleted(bool success, const QString& message);

private:
    // Recipe-based install: execute custom install steps for a package
    void executeInstallSteps(const PackageInfo& pkg, const QString& gamePath,
                             const QString& packageDir);

    // Options-based install: execute steps for each selected option
    void installWithOptions(const PackageInfo& pkg, const QString& gamePath,
                            const QString& basePackageDir, const QStringList& selectedOptions);

    QString installedStatePath() const;

    enum class CopyError { None, DiskFull, PermissionDenied, FileLocked, Other };
    struct CopyResult { bool ok; CopyError error; };
    CopyResult tryCopyFile(const QString& src, const QString& dest);

    // Outcome of a single step execution.
    enum class StepOutcome { Ok, SoftError, FatalError, Cancelled };

    // Execute one InstallStep. Handles copy/copyDir/run/delete/installFont/
    // setSteamLanguage/copyToDesktop/rename actions.
    // packageDir   — source directory for src-relative paths (package or option subdir)
    // progressPrefix — prepended to status messages (empty for recipe steps,
    //                  "optLabel — " for option steps)
    // installedFiles — appended on success; caller owns the list
    // Returns SoftError to increment errors and continue, FatalError to abort
    // (caller should emit installCompleted and return), Cancelled to abort cleanly.
    StepOutcome executeStep(const InstallStep& step,
                            const QString& gamePath,
                            const QString& packageDir,
                            const QString& canonGamePath,
                            const QString& cleanGamePath,
                            double progress,
                            int current, int total,
                            const QString& progressPrefix,
                            const QString& steamAppId,
                            QStringList& installedFiles);

    // Run an external process with polling timeout.
    // progressCallback receives elapsed ms for UI feedback (optional).
    struct ProcessResult {
        bool started = false;
        bool timedOut = false;
        int exitCode = -1;
        QByteArray output;
    };
    ProcessResult runProcess(const QString& exePath, const QStringList& args,
                             const QString& workDir,
                             std::function<void(int elapsedMs)> progressCallback = nullptr);

    // --- Shared helpers (used by both installPackage and updatePackage) ---

    // Resolve the source path for a package+variant, falling back to legacy pak/ format.
    // Returns empty string if nothing is found.
    QString resolveSourcePath(const PackageInfo& pkg, const QString& variant) const;

    // Result of copyOverlayFiles: per-category file lists and counters.
    struct OverlayResult {
        int copied{0};
        int errors{0};
        QStringList installedFiles;
        QStringList addedFiles;
        QStringList replacedFiles;
    };

    // Copy all files from filesToCopy into gamePath, with path-traversal protection,
    // retry-on-lock, DiskFull/PermissionDenied fatal errors, and throttled progress signals.
    // progressPrefix is shown in the progress status message (e.g. "Kopyalanıyor" / "Güncelleniyor").
    // fileClassifier(relPath, destFileExists) returns true if the file should be classified
    // as "replaced"; false means it is classified as "added".
    OverlayResult copyOverlayFiles(
        const QList<QPair<QString, QString>>& filesToCopy,
        const QString& gamePath,
        const QString& progressPrefix,
        const std::function<bool(const QString&, bool)>& fileClassifier);

    // Persist installed state to the catalog on the main thread (Qt::QueuedConnection).
    void saveInstallState(const QString& steamAppId, const QString& gamePath,
                          const PackageInfo& pkg,
                          const QStringList& installedFiles,
                          const QStringList& addedFiles,
                          const QStringList& replacedFiles);

    packages::PackageCatalog m_catalog;
    static PackageInfo fromCatalogEntry(const packages::PackageCatalogEntry& entry);

    bool isCancelled() const { return m_cancelRequested.load(std::memory_order_relaxed); }

    QString m_dataPath;
    OperationJournal* m_journal{nullptr};
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace makine
