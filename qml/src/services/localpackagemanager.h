// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
#include <QMutex>
#include <QSet>
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

#include "gamefolderrules.h"
#include "gameprint.h"

namespace makine {

class OperationJournal;

struct InstallStep {
    QString action;     // "copy", "copyDir", "run", "delete", "installFont", "setSteamLanguage", "copyToDesktop", "rename", "vpatch", "patchBytes", "forgeReimport", "xdelta"
    QString src;        // source file/dir (relative to package dir)
    QString dest;       // destination (relative to game dir) — for "copy"/"delete"
    QString exe;        // executable to run — for "run"
    QStringList args;   // arguments — for "run"
    QString fallback;   // fallback executable if primary not found — for "run"
    QString workDir;    // "game" (default) or "package" — for "run"
    QString uninstall;  // program that undoes this "run" step — started on uninstall
    QStringList uninstallArgs;  // its arguments, same placeholders as args
    QStringList requiredFiles;  // "requires": game files the root must have before it runs
    QString language;   // Steam language name — for "setSteamLanguage"
    QList<QPair<QString, QString>> patches;  // find/replace byte strings — for "patchBytes"
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

    /// Files recorded at install time that are no longer on disk, as paths
    /// relative to the game folder. Empty when the patch is intact or when
    /// nothing is installed. Directive entries ("_font:", "_desktop:") are
    /// skipped — they do not name files inside the game folder.
    QStringList missingInstalledFiles(const QString& steamAppId) const;
    void updateInstalledStoreVersion(const QString& steamAppId, const QString& storeVersion,
                                      const QString& source);

    void installPackage(const QString& steamAppId, const QString& gamePath,
                        const QString& variant = {},
                        const QStringList& selectedOptions = {});
    void updatePackage(const QString& steamAppId, const QString& gamePath,
                       const QString& variant = {},
                       const QStringList& selectedOptions = {});
    void cancelInstall();
    // restoredFrom: the backup folder the uninstall just restored from; a
    // file it holds is an original and is left in place.
    bool uninstallPackage(const QString& steamAppId, const QString& gamePath,
                          const QString& restoredFrom = {});
    // Undo a failed install that went over an earlier one, with no backup:
    // delete the files it added that `earlierRecord` (the record it went over)
    // does not list, and drop them from the record. The rest stays installed.
    // False when one of them could not be deleted; it stays on record.
    bool removeAttemptFiles(const QString& steamAppId, const QString& gamePath,
                            const QStringList& earlierRecord);

    // Resolve any store ID (epic_xxx, gog_xxx, steamAppId) to canonical steamAppId
    QString resolveGameId(const QString& gameId) const;

    // Variant support
    QVariantList getVariants(const QString& steamAppId) const;
    QString getVariantType(const QString& steamAppId) const;

    // Get list of relative file paths in the translation package
    // gamePath lets the wrapper-stripping heuristic verify a top-level folder
    // against the real game install, so the returned paths match exactly what
    // the install will overwrite. Pass it whenever the target is known.
    QStringList getPackageFileList(const QString& steamAppId, const QString& variant = {},
                                    const QString& gamePath = {}) const;

    // Step actions in any of this package's recipes that this build cannot run.
    QStringList unknownRecipeActions(const QString& steamAppId) const;

    // What this package expects under its game's root: the catalogue's exe
    // names, the folders its recipe (or, once downloaded, its files) go into,
    // the game files it edits in place. folderrules::pickRoot weighs a
    // user-picked folder against it.
    folderrules::Expectations folderExpectations(const QString& steamAppId) const;
    // The "requires" files of the steps an install with this variant and these
    // options runs. folderExpectations gathers every option's and variant's
    // to recognise the folder; as a gate that refused a recipe whose Steam and
    // GOG variants need different files on every folder.
    QStringList requiredFilesFor(const QString& steamAppId, const QString& variant,
                                 const QStringList& selectedOptions) const;
    // The fingerprints of the copy this patch was made on (gameprint.h): the
    // catalogue's, then every game.bin the extracted package carries (OyunBul's,
    // one per variant at most). Empty when there is none.
    QList<gameprint::Print> gamePrints(const QString& steamAppId) const;
    // Where the game's exe sits under `gamePath`, relative ("Game/eldenring.exe"):
    // the fingerprint's when it matches the folder, else the first of the
    // catalogue's exe names found there; empty when neither finds one.
    QString gameExeRel(const QString& steamAppId, const QString& gamePath) const;

    // Whether any of this package's recipes rewrites a game file in place.
    bool recipeWritesInPlace(const QString& steamAppId) const;

    // A recipe made only of "run" steps: the patch's own program installs it,
    // so nothing it writes can be listed (and backed up) in advance. WithUndo
    // when every step also names the program that undoes it.
    enum class OwnProgram { No, WithUndo, WithoutUndo };
    OwnProgram ownProgramRecipe(const QString& steamAppId) const;
    // That program's file name ("HeavyRain_Turkce_Yama.exe"), empty when the
    // recipe is not one; the page says what will run.
    QString ownProgramName(const QString& steamAppId) const;

    // What starting a patch's undo programs came to (runUndoPrograms).
    struct UndoResult {
        bool ran = false;            // a program was started
        bool ok = true;              // every one exited 0
        bool changedNothing = false; // and the game folder is as it was before
        bool stopped = false;        // not ok because the user cancelled or declined the prompt
        QString message;             // when not ok: what to tell the user
    };
    // Whether uninstall owes this patch its undo programs: the recipe names
    // them and the record says the install's program changed the game (or
    // predates the record saying so).
    bool needsUndoPrograms(const QString& steamAppId) const;
    // Run, waiting for each, the "uninstall" program of every top-level run
    // step, from the copy kept at install (or the extracted package). Its
    // output goes to processOutput like an install's. Blocks: call it off the
    // UI thread.
    UndoResult runUndoPrograms(const QString& steamAppId, const QString& gamePath);

    // An answer typed on the install page for the program a "run" step is
    // running. Called from the UI thread while the program runs on the
    // install worker; dropped when no program is running.
    void sendProcessInput(const QString& text);

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
    // What the program of a "run" step printed, as it arrives.
    void processOutput(const QString& text);

private:
    // Recipe-based install: execute custom install steps for a package
    void executeInstallSteps(const PackageInfo& pkg, const QString& gamePath,
                             const QString& packageDir);

    // Options-based install: execute steps for each selected option
    void installWithOptions(const PackageInfo& pkg, const QString& gamePath,
                            const QString& basePackageDir, const QStringList& selectedOptions);

    QString installedStatePath() const;

    // Quarantined: antivirus refused or removed the file (Windows 225/226).
    enum class CopyError { None, DiskFull, PermissionDenied, FileLocked, Quarantined, Other };
    struct CopyResult { bool ok; CopyError error; };
    CopyResult tryCopyFile(const QString& src, const QString& dest);
    // What to tell the player when antivirus blocks a package file.
    static QString quarantinedMessage(const QString& file);

    // Outcome of a single step execution.
    enum class StepOutcome { Ok, SoftError, FatalError, Cancelled };

    // Execute one InstallStep. Handles copy/copyDir/run/delete/installFont/
    // setSteamLanguage/copyToDesktop/rename actions.
    // packageDir   — source directory for src-relative paths (package or option subdir)
    // progressPrefix — prepended to status messages (empty for recipe steps,
    //                  "optLabel — " for option steps)
    // installedFiles — appended on success; caller owns the list
    // replacedFiles — copies that overwrote a file the game already had, so
    //                uninstall leaves them to the restore instead of deleting
    // failDetail   — set on SoftError to a short cause when the action has more
    //                than one way to fail. "run" has four (not started /
    //                elevation declined / timeout / non-zero exit) and all four
    //                reached Sentry as the same sentence, so a 915-event bucket
    //                could not be split by cause.
    // Returns SoftError to increment errors and continue, FatalError to abort
    // (failDetail then holds the message; the caller reports it through
    // failInstall and returns), Cancelled to abort cleanly.
    StepOutcome executeStep(const InstallStep& step,
                            const QString& gamePath,
                            const QString& packageDir,
                            double progress,
                            int current, int total,
                            const QString& progressPrefix,
                            const QString& steamAppId,
                            QStringList& installedFiles,
                            QSet<QString>& replacedFiles,
                            QString* failDetail = nullptr);

    // Run an external process with polling timeout.
    // progressCallback receives elapsed ms for UI feedback (optional).
    struct ProcessResult {
        bool started = false;
        bool timedOut = false;
        bool cancelled = false;
        // The binary demanded administrator rights and the user dismissed the
        // UAC prompt. Distinct from a plain start failure: nothing is wrong
        // with the package or the machine, so the advice must differ.
        bool elevationDeclined = false;
        int exitCode = -1;
        QByteArray output;
    };
    // extraEnv: added to the program's environment (MAKINE_OYUN and the
    // others, see runstage.h).
    ProcessResult runProcess(const QString& exePath, const QStringList& args,
                             const QString& workDir,
                             std::function<void(int elapsedMs)> progressCallback = nullptr,
                             const QHash<QString, QString>& extraEnv = {});

    // One start of a package's program, as install and uninstall both run it:
    // from a stage of `packageDir` whose scripts read the launcher's game
    // folder, that folder in its arguments (${gamePath}) and environment
    // (runstage.h). `refused` is set when it was not started at all (not
    // found, not allowed, not staged); `shownName` is the program's file name.
    ProcessResult runPackageProgram(const QString& program, const QString& fallback,
                                    const QStringList& args, const QString& workDirMode,
                                    const QString& gamePath, const QString& packageDir,
                                    const QString& gameId,
                                    std::function<void(int elapsedMs)> progressCallback,
                                    QString* refused, QString* shownName);

    // --- Shared helpers (used by both installPackage and updatePackage) ---

    // Resolve the source path for a package+variant, falling back to legacy pak/ format.
    // Returns empty string if nothing is found.
    QString resolveSourcePath(const PackageInfo& pkg, const QString& variant) const;

    // Result of copyOverlayFiles: per-category file lists and counters.
    // errors == -1: the copy stopped (cancel, disk full, …) and fatal says why.
    struct OverlayResult {
        int copied{0};
        int errors{0};
        QStringList installedFiles;
        QStringList addedFiles;
        QStringList replacedFiles;
        QString fatal;
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

    // Report a failed or cancelled install that may have written files. What
    // it wrote is merged into the install record first (steprules::
    // mergeWrittenFiles), on the main thread and before installCompleted, so
    // the rollback that answers the signal can find and remove it.
    void failInstall(const QString& steamAppId, const QString& gamePath,
                     const QString& version, const QStringList& written,
                     const QStringList& writtenReplaced, const QString& message);

    packages::PackageCatalog m_catalog;
    static PackageInfo fromCatalogEntry(const packages::PackageCatalogEntry& entry);

    bool isCancelled() const { return m_cancelRequested.load(std::memory_order_relaxed); }

    QString m_dataPath;
    OperationJournal* m_journal{nullptr};
    std::atomic<bool> m_cancelRequested{false};
    // Typed answers waiting for the running program (sendProcessInput).
    QMutex m_inputMutex;
    QByteArray m_pendingInput;     // guarded by m_inputMutex
    bool m_acceptsInput{false};    // guarded by m_inputMutex
    // Windows error of the last failed copy, for the failure report (0 = none).
    unsigned long m_lastCopyWinError{0};
};

} // namespace makine
