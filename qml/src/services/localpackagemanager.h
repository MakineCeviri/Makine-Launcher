/**
 * @file localpackagemanager.h
 * @brief Local translation package management
 * @copyright (c) 2026 MakineAI Team
 *
 * Manages translation packages from the local filesystem.
 * When built with core library (not MAKINEAI_UI_ONLY), delegates
 * catalog/query operations to makineai::packages::PackageCatalog.
 * Install/uninstall file operations always stay in this QML service.
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

#ifndef MAKINEAI_UI_ONLY
#include <makineai/package_catalog.hpp>
#endif

namespace makineai {

class OperationJournal;

struct InstallStep {
    QString action;     // "copy", "copyDir", "run", "delete", "installFont"
    QString src;        // source file/dir (relative to package dir)
    QString dest;       // destination (relative to game dir) — for "copy"/"delete"
    QString exe;        // executable to run — for "run"
    QStringList args;   // arguments — for "run"
    QString fallback;   // fallback executable if primary not found — for "run"
    QString workDir;    // "game" (default) or "package" — for "run"
};

struct PackageInfo {
    QString packageId;   // e.g. "SF10310Hv19"
    QString steamAppId;  // e.g. "1716740"
    QString gameName;
    QString engine;
    QString version;
    QString installType; // "overlay", "runtime", "replace"
    qint64 sizeBytes{0};
    int fileCount{0};
    QHash<QString, QString> storeIds; // store -> id (e.g. "epic" -> "abc123")
    QString dirName;                // filesystem directory name in translation_data
    QStringList variants;           // ["1.00", "1.04"] or ["Steam", "Gamepass"]
    QString variantType;            // "version" or "platform" (for UI label)
    QVariantList contributors;      // [{name, role}] from manifest
    QList<InstallStep> installSteps; // custom install recipe (empty = default overlay)
    QString installMethodType;      // "script", "userPath" (empty = default overlay)
    QString installMethodTarget;    // for "userPath": target relative to user home
    QString installNotes;           // pre-install notes shown to user (e.g. "Change language to English")
};

struct InstalledPackageInfo {
    QString version;
    QString gamePath;
    QStringList installedFiles;
    qint64 installedAt{0};
};

class LocalPackageManager : public QObject
{
    Q_OBJECT

public:
    explicit LocalPackageManager(QObject *parent = nullptr);

    bool loadFromPath(const QString& translationDataPath);

    bool hasPackage(const QString& steamAppId) const;
    std::optional<PackageInfo> getPackage(const QString& steamAppId) const;

    bool isInstalled(const QString& steamAppId) const;

    void installPackage(const QString& steamAppId, const QString& gamePath,
                        const QString& variant = {});
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

    void setJournal(OperationJournal* journal) { m_journal = journal; }

    int packageCount() const;
    QVariantList allPackagesAsList() const;

signals:
    void installProgress(double progress, const QString& status);
    void installCompleted(bool success, const QString& message);

private:
    // Recipe-based install: execute custom install steps for a package
    void executeInstallSteps(const PackageInfo& pkg, const QString& gamePath,
                             const QString& packageDir);

    QString installedStatePath() const;

#ifndef MAKINEAI_UI_ONLY
    // Core catalog handles manifest loading, queries, installed state, etc.
    packages::PackageCatalog m_catalog;

    // Convert core PackageCatalogEntry to QML-facing PackageInfo
    static PackageInfo fromCatalogEntry(const packages::PackageCatalogEntry& entry);
#else
    // UI-only fallback: full QJsonDocument-based implementation
    void loadManifest(const QString& manifestPath);
    void scanPackageDirectories(const QString& basePath);
    void scanGameNameDirectories(const QString& basePath);
    void loadInstalledState();
    void saveInstalledState();

    // steamAppId -> PackageInfo
    QHash<QString, PackageInfo> m_packages;
    // steamAppId -> installed package info
    QHash<QString, InstalledPackageInfo> m_installed;
    // Reverse index: any store ID -> steamAppId
    QHash<QString, QString> m_storeIdToSteamAppId;
#endif

    QString m_dataPath;
    OperationJournal* m_journal{nullptr};
};

} // namespace makineai
