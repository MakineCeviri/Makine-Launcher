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

    void installPackage(const QString& steamAppId, const QString& gamePath);
    bool uninstallPackage(const QString& steamAppId, const QString& gamePath);

    int packageCount() const { return m_packages.size(); }
    QStringList availableAppIds() const { return m_packages.keys(); }
    QVariantList allPackagesAsList() const;

signals:
    void installProgress(double progress, const QString& status);
    void installCompleted(bool success, const QString& message);

private:
    void loadManifest(const QString& manifestPath);
    void scanPackageDirectories(const QString& basePath);
    void loadInstalledState();
    void saveInstalledState();
    QString installedStatePath() const;

    // steamAppId -> PackageInfo
    QHash<QString, PackageInfo> m_packages;
    // steamAppId -> installed version
    QHash<QString, QString> m_installed;
    QString m_dataPath;
};

} // namespace makineai
