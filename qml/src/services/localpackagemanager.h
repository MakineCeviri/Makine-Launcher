/**
 * @file localpackagemanager.h
 * @brief Local translation package management for UI_ONLY builds
 * @copyright (c) 2026 MakineAI Team
 *
 * Manages translation packages from the local filesystem.
 * Reads manifest.json, tracks installed packages, performs
 * file copy (overlay) installation with backup support.
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

namespace makineai {

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

    int packageCount() const { return m_packages.size(); }
    QVariantList allPackagesAsList() const;

signals:
    void installProgress(double progress, const QString& status);
    void installCompleted(bool success, const QString& message);

private:
    void loadManifest(const QString& manifestPath);
    void scanPackageDirectories(const QString& basePath);
    void scanGameNameDirectories(const QString& basePath);
    void loadInstalledState();
    void saveInstalledState();
    QString installedStatePath() const;

    // steamAppId -> PackageInfo
    QHash<QString, PackageInfo> m_packages;
    // steamAppId -> installed package info
    QHash<QString, InstalledPackageInfo> m_installed;
    // Reverse index: any store ID -> steamAppId
    QHash<QString, QString> m_storeIdToSteamAppId;
    QString m_dataPath;
};

} // namespace makineai
