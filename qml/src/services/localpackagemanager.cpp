/**
 * @file localpackagemanager.cpp
 * @brief Local translation package management implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "localpackagemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>
#include <QDirIterator>
#include <QtConcurrent>

namespace makineai {

// Known package ID -> Steam AppID mapping
// Package directories use internal IDs; we map them to Steam AppIDs
static const QHash<QString, QPair<QString, QString>> s_packageMapping = {
    // {packageDirName, {steamAppId, gameName}}
    {"ER1080",         {"1245620", "Elden Ring"}},
    {"BMW_10714712",   {"2358720", "Black Myth: Wukong"}},
    {"SF10310Hv16",    {"1716740", "Starfield"}},
    {"RDR2TRV17F",     {"1174180", "Red Dead Redemption 2"}},
    {"AH1010",         {"668580",  "Atomic Heart"}},
    {"CS21015V1",      {"949230",  "Cities: Skylines II"}},
    {"ACM107",         {"3035570", "Assassin's Creed Mirage"}},
    {"ACS100",         {"3159330", "Assassin's Creed Shadows"}},
    {"ACV170DLC123",   {"2208920", "Assassin's Creed Valhalla"}},
    {"ACO156D1234",    {"812140",  "Assassin's Creed Odyssey"}},
    {"ACOR160D12F",    {"582160",  "Assassin's Creed Origins"}},
    {"AW2_1012V1",     {"3611110", "Alan Wake 2"}},
    {"BAK1620V1",      {"208650",  "Batman: Arkham Knight"}},
    {"COTL101877",     {"1313140", "Cult of the Lamb"}},
    {"COTDG12444",     {"1123770", "Curse of the Dead Gods"}},
    {"DOS2",           {"435150",  "Divinity: Original Sin 2"}},
    {"DOSEE",          {"373420",  "Divinity: Original Sin Enhanced Edition"}},
    {"EW104D",         {"1065310", "Evil West"}},
    {"HL_1120320",     {"1120320", "Half-Life: Alyx"}},
    {"IFR134D2",       {"2221920", "Immortals Fenyx Rising"}},
    {"MEA",            {"1238000", "Mass Effect: Andromeda"}},
    {"POE_1381318",    {"291650",  "Pillars of Eternity"}},
    {"SM2_113010",     {"2651280", "Marvel's Spider-Man 2"}},
    {"TEATS109V1",     {"1708010", "The Expanse: A Telltale Series"}},
    {"TES4OR_04111400",{"22330",   "The Elder Scrolls IV: Oblivion Remastered"}},
    {"COE33_56442",    {"1903340", "Clair Obscur: Expedition 33"}},
    {"JGC_1000",       {"2677660", "Indiana Jones and the Great Circle"}},
    {"D2R_1471776",    {"990080",  "Hogwarts Legacy"}},
};

LocalPackageManager::LocalPackageManager(QObject *parent)
    : QObject(parent)
{
}

bool LocalPackageManager::loadFromPath(const QString& translationDataPath)
{
    m_dataPath = translationDataPath;
    m_packages.clear();

    QDir baseDir(translationDataPath);
    if (!baseDir.exists()) {
        qWarning() << "Translation data path does not exist:" << translationDataPath;
        return false;
    }

    // Check for pak/ subdirectory
    QString pakPath = translationDataPath + "/pak";
    QDir pakDir(pakPath);
    if (!pakDir.exists()) {
        qWarning() << "No pak/ directory in:" << translationDataPath;
        return false;
    }

    // Try loading manifest.json first
    QString manifestPath = translationDataPath + "/manifest.json";
    if (QFile::exists(manifestPath)) {
        loadManifest(manifestPath);
    }

    // Scan pak/ directories for packages
    scanPackageDirectories(pakPath);

    // Load installed state
    loadInstalledState();

    qDebug() << "LocalPackageManager: loaded" << m_packages.size() << "packages from" << translationDataPath;
    return !m_packages.isEmpty();
}

void LocalPackageManager::loadManifest(const QString& manifestPath)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Manifest parse error:" << err.errorString();
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject packages = root["packages"].toObject();

    for (auto it = packages.begin(); it != packages.end(); ++it) {
        QJsonObject pkgObj = it.value().toObject();
        PackageInfo info;
        info.steamAppId = it.key();
        info.gameName = pkgObj["gameName"].toString();
        info.engine = pkgObj["engine"].toString();
        info.version = pkgObj["version"].toString();
        info.installType = pkgObj["installType"].toString("overlay");
        m_packages[info.steamAppId] = info;
    }
}

void LocalPackageManager::scanPackageDirectories(const QString& basePath)
{
    QDir pakDir(basePath);
    const auto entries = pakDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& dirName : entries) {
        // Check if this directory has an extracted_* subdirectory
        QString fullPath = basePath + "/" + dirName;
        QDir pkgDir(fullPath);

        bool hasExtracted = false;
        QString extractedPath;
        const auto subDirs = pkgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& sub : subDirs) {
            if (sub.startsWith("extracted_")) {
                hasExtracted = true;
                extractedPath = fullPath + "/" + sub;
                break;
            }
        }

        // Map directory name to Steam AppID
        auto mappingIt = s_packageMapping.find(dirName);
        if (mappingIt == s_packageMapping.end()) {
            continue; // Unknown package, skip
        }

        const QString& steamAppId = mappingIt->first;
        const QString& gameName = mappingIt->second;

        // If already loaded from manifest, just verify path exists
        if (m_packages.contains(steamAppId)) {
            auto& pkg = m_packages[steamAppId];
            pkg.packageId = dirName;
            if (hasExtracted) {
                // Count files for size estimation
                QDirIterator it(extractedPath, QDir::Files, QDirIterator::Subdirectories);
                int count = 0;
                qint64 totalSize = 0;
                while (it.hasNext()) {
                    it.next();
                    count++;
                    totalSize += it.fileInfo().size();
                }
                pkg.fileCount = count;
                pkg.sizeBytes = totalSize;
            }
            continue;
        }

        // Create package info from directory scan
        PackageInfo info;
        info.packageId = dirName;
        info.steamAppId = steamAppId;
        info.gameName = gameName;
        info.installType = "overlay";

        if (hasExtracted) {
            QDirIterator it(extractedPath, QDir::Files, QDirIterator::Subdirectories);
            int count = 0;
            qint64 totalSize = 0;
            while (it.hasNext()) {
                it.next();
                count++;
                totalSize += it.fileInfo().size();
            }
            info.fileCount = count;
            info.sizeBytes = totalSize;
        }

        m_packages[steamAppId] = info;
    }
}

bool LocalPackageManager::hasPackage(const QString& steamAppId) const
{
    return m_packages.contains(steamAppId);
}

std::optional<PackageInfo> LocalPackageManager::getPackage(const QString& steamAppId) const
{
    auto it = m_packages.find(steamAppId);
    if (it != m_packages.end()) {
        return *it;
    }
    return std::nullopt;
}

bool LocalPackageManager::isInstalled(const QString& steamAppId) const
{
    return m_installed.contains(steamAppId);
}

void LocalPackageManager::installPackage(const QString& steamAppId, const QString& gamePath)
{
    auto pkgIt = m_packages.find(steamAppId);
    if (pkgIt == m_packages.end()) {
        emit installCompleted(false, tr("Package not found for AppID: %1").arg(steamAppId));
        return;
    }

    const PackageInfo& pkg = *pkgIt;
    QString pkgDirPath = m_dataPath + "/pak/" + pkg.packageId;
    QDir pkgDir(pkgDirPath);

    // Find extracted directory
    QString extractedPath;
    const auto subDirs = pkgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sub : subDirs) {
        if (sub.startsWith("extracted_")) {
            extractedPath = pkgDirPath + "/" + sub;
            break;
        }
    }

    if (extractedPath.isEmpty()) {
        emit installCompleted(false, tr("No extracted data found for: %1").arg(pkg.gameName));
        return;
    }

    // Run installation in background thread
    (void)QtConcurrent::run([this, steamAppId, gamePath, extractedPath, pkg]() {
        emit installProgress(0.0, tr("Dosyalar hazirlanyor..."));

        // Collect all files to copy
        QList<QPair<QString, QString>> filesToCopy; // source, relative path
        QDirIterator it(extractedPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString relPath = it.filePath().mid(extractedPath.length() + 1);
            filesToCopy.append({it.filePath(), relPath});
        }

        if (filesToCopy.isEmpty()) {
            emit installCompleted(false, tr("No files to install"));
            return;
        }

        int total = filesToCopy.size();
        int copied = 0;
        int errors = 0;

        for (const auto& [srcPath, relPath] : filesToCopy) {
            QString destPath = gamePath + "/" + relPath;

            // Ensure destination directory exists
            QFileInfo destInfo(destPath);
            QDir().mkpath(destInfo.absolutePath());

            // Copy file (overwrite if exists)
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            if (QFile::copy(srcPath, destPath)) {
                copied++;
            } else {
                qWarning() << "Failed to copy:" << srcPath << "->" << destPath;
                errors++;
            }

            double progress = static_cast<double>(copied + errors) / total;
            emit installProgress(progress,
                tr("%1/%2 dosya kopyalandi").arg(copied).arg(total));
        }

        if (errors == 0) {
            // Mark as installed
            QMetaObject::invokeMethod(this, [this, steamAppId, pkg]() {
                m_installed[steamAppId] = pkg.version;
                saveInstalledState();
            }, Qt::QueuedConnection);

            emit installCompleted(true,
                tr("%1 dosya basariyla kuruldu").arg(copied));
        } else {
            emit installCompleted(false,
                tr("%1/%2 dosya kopyalanamadi").arg(errors).arg(total));
        }
    });
}

bool LocalPackageManager::uninstallPackage(const QString& steamAppId, const QString& gamePath)
{
    Q_UNUSED(gamePath)
    // Uninstall is handled by BackupManager restore
    m_installed.remove(steamAppId);
    saveInstalledState();
    return true;
}

void LocalPackageManager::loadInstalledState()
{
    QFile file(installedStatePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) return;

    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        m_installed[it.key()] = it.value().toString();
    }
}

void LocalPackageManager::saveInstalledState()
{
    QString path = installedStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    for (auto it = m_installed.constBegin(); it != m_installed.constEnd(); ++it) {
        root[it.key()] = it.value();
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

QString LocalPackageManager::installedStatePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/installed_packages.json";
}

} // namespace makineai
