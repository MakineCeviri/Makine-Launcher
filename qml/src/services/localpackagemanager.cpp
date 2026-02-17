/**
 * @file localpackagemanager.cpp
 * @brief Local translation package management implementation
 * @copyright (c) 2026 MakineAI Team
 *
 * When built with core library (not MAKINEAI_UI_ONLY), catalog/query
 * operations delegate to makineai::packages::PackageCatalog.
 * Install/uninstall file operations (QFile::copy, QProcess, etc.)
 * always remain in this QML service layer.
 */

#include "localpackagemanager.h"
#include "operationjournal.h"
#include "pathsecurity.h"
#include "appprotection.h"
#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>
#include <QDirIterator>
#include <QDateTime>
#include <QProcess>
#include <QtConcurrent>

namespace makineai {

// =============================================================================
// CORE-DELEGATED BUILD
// =============================================================================

#ifndef MAKINEAI_UI_ONLY

// -- Construction / loading ---------------------------------------------------

LocalPackageManager::LocalPackageManager(QObject *parent)
    : QObject(parent)
{
}

bool LocalPackageManager::loadFromPath(const QString& translationDataPath)
{
    m_dataPath = translationDataPath;

    // Delegate catalog loading to core
    bool ok = m_catalog.loadFromPath(translationDataPath.toStdString());

    // Load installed state through core
    std::string statePath = installedStatePath().toStdString();
    m_catalog.loadInstalledState(statePath);

    qDebug() << "LocalPackageManager: loaded" << m_catalog.packageCount()
             << "packages from" << translationDataPath
             << (ok ? "(via core catalog)" : "(core catalog failed)");
    return ok;
}

// -- Conversion helper --------------------------------------------------------

PackageInfo LocalPackageManager::fromCatalogEntry(const packages::PackageCatalogEntry& entry)
{
    PackageInfo info;
    info.packageId     = QString::fromStdString(entry.packageId);
    info.steamAppId    = QString::fromStdString(entry.steamAppId);
    info.gameName      = QString::fromStdString(entry.gameName);
    info.engine        = QString::fromStdString(entry.engine);
    info.version       = QString::fromStdString(entry.version);
    info.installType   = QString::fromStdString(entry.installType);
    info.sizeBytes     = entry.sizeBytes;
    info.fileCount     = entry.fileCount;
    info.dirName       = QString::fromStdString(entry.dirName);
    info.variantType   = QString::fromStdString(entry.variantType);
    info.installMethodType   = QString::fromStdString(entry.installMethodType);
    info.installMethodTarget = QString::fromStdString(entry.installMethodTarget);
    info.installNotes  = QString::fromStdString(entry.installNotes);

    // Convert store IDs
    for (const auto& [store, id] : entry.storeIds) {
        info.storeIds[QString::fromStdString(store)] = QString::fromStdString(id);
    }

    // Convert variants
    for (const auto& v : entry.variants) {
        info.variants.append(QString::fromStdString(v));
    }

    // Convert contributors
    for (const auto& c : entry.contributors) {
        info.contributors.append(QVariantMap{
            {"name", QString::fromStdString(c.name)},
            {"role", QString::fromStdString(c.role)}
        });
    }

    // Convert install steps
    for (const auto& s : entry.installSteps) {
        InstallStep step;
        step.action   = QString::fromStdString(s.action);
        step.src      = QString::fromStdString(s.src);
        step.dest     = QString::fromStdString(s.dest);
        step.exe      = QString::fromStdString(s.exe);
        step.fallback = QString::fromStdString(s.fallback);
        step.workDir  = QString::fromStdString(s.workDir);
        for (const auto& a : s.args) {
            step.args.append(QString::fromStdString(a));
        }
        info.installSteps.append(step);
    }

    return info;
}

// -- Catalog query methods (delegated to core) --------------------------------

bool LocalPackageManager::hasPackage(const QString& steamAppId) const
{
    return m_catalog.hasPackage(steamAppId.toStdString());
}

std::optional<PackageInfo> LocalPackageManager::getPackage(const QString& steamAppId) const
{
    auto entry = m_catalog.getPackage(steamAppId.toStdString());
    if (!entry) return std::nullopt;
    return fromCatalogEntry(*entry);
}

bool LocalPackageManager::isInstalled(const QString& steamAppId) const
{
    return m_catalog.isInstalled(steamAppId.toStdString());
}

QString LocalPackageManager::resolveGameId(const QString& gameId) const
{
    std::string result = m_catalog.resolveGameId(gameId.toStdString());
    return QString::fromStdString(result);
}

QVariantList LocalPackageManager::getVariants(const QString& steamAppId) const
{
    auto variants = m_catalog.getVariants(steamAppId.toStdString());
    QVariantList result;
    result.reserve(static_cast<int>(variants.size()));
    for (const auto& v : variants) {
        result.append(QString::fromStdString(v));
    }
    return result;
}

QString LocalPackageManager::getVariantType(const QString& steamAppId) const
{
    return QString::fromStdString(m_catalog.getVariantType(steamAppId.toStdString()));
}

QStringList LocalPackageManager::getPackageFileList(const QString& steamAppId, const QString& variant) const
{
    auto files = m_catalog.getPackageFileList(steamAppId.toStdString(), variant.toStdString());
    QStringList result;
    result.reserve(static_cast<int>(files.size()));
    for (const auto& f : files) {
        result.append(QString::fromStdString(f));
    }
    return result;
}

QString LocalPackageManager::findMatchingAppId(const QString& folderName) const
{
    return QString::fromStdString(m_catalog.findMatchingAppId(folderName.toStdString()));
}

int LocalPackageManager::packageCount() const
{
    return m_catalog.packageCount();
}

QVariantList LocalPackageManager::allPackagesAsList() const
{
    auto entries = m_catalog.allPackages();
    QVariantList result;
    result.reserve(static_cast<int>(entries.size()));
    for (const auto& entry : entries) {
        result.append(QVariantMap{
            {"steamAppId", QString::fromStdString(entry.steamAppId)},
            {"gameName",   QString::fromStdString(entry.gameName)},
            {"engine",     QString::fromStdString(entry.engine)},
            {"version",    QString::fromStdString(entry.version)},
            {"packageId",  QString::fromStdString(entry.packageId)},
            {"headerImageUrl", QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg")
                .arg(QString::fromStdString(entry.steamAppId))},
        });
    }
    return result;
}

// -- Installed state persistence (delegated to core) --------------------------

// Private helper: save installed state through core catalog
static void saveCatalogInstalledState(packages::PackageCatalog& catalog, const QString& statePath)
{
    catalog.saveInstalledState(statePath.toStdString());
}

#else // MAKINEAI_UI_ONLY

// =============================================================================
// UI-ONLY FALLBACK BUILD (full original implementation)
// =============================================================================

// Fallback mapping used when manifest.json is not available (transition period)
static const QHash<QString, QPair<QString, QString>> s_fallbackMapping = {
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
    {"HL_1120320",     {"1583230", "High On Life"}},
    {"IFR134D2",       {"2221920", "Immortals Fenyx Rising"}},
    {"MEA",            {"1238000", "Mass Effect: Andromeda"}},
    {"POE_1381318",    {"291650",  "Pillars of Eternity"}},
    {"SM2_113010",     {"2651280", "Marvel's Spider-Man 2"}},
    {"TEATS109V1",     {"1708010", "The Expanse: A Telltale Series"}},
    {"TES4OR_04111400",{"22330",   "The Elder Scrolls IV: Oblivion Remastered"}},
    {"COE33_56442",    {"1903340", "Clair Obscur: Expedition 33"}},
    {"JGC_1000",       {"2677660", "Indiana Jones and the Great Circle"}},
    {"D2R_1471776",    {"1293830", "Diablo II: Resurrected"}},
    {"TCP_1544020",    {"1544020", "The Callisto Protocol"}},
};

LocalPackageManager::LocalPackageManager(QObject *parent)
    : QObject(parent)
{
}

bool LocalPackageManager::loadFromPath(const QString& translationDataPath)
{
    m_dataPath = translationDataPath;
    m_packages.clear();
    m_storeIdToSteamAppId.clear();

    QDir baseDir(translationDataPath);
    if (!baseDir.exists()) {
        qWarning() << "Translation data path does not exist:" << translationDataPath;
        return false;
    }

    // Try loading manifest.json first
    QString manifestPath = translationDataPath + "/manifest.json";
    if (QFile::exists(manifestPath)) {
        loadManifest(manifestPath);
    }

    // Scan pak/ directories for legacy packages (mc-main format)
    QString pakPath = translationDataPath + "/pak";
    QDir pakDir(pakPath);
    if (pakDir.exists()) {
        scanPackageDirectories(pakPath);
    }

    // Scan root-level game-name directories (new format)
    scanGameNameDirectories(translationDataPath);

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
        info.packageId = pkgObj["packageId"].toString();
        info.gameName = pkgObj["gameName"].toString();
        info.engine = pkgObj["engine"].toString();
        info.version = pkgObj["version"].toString();
        info.installType = pkgObj["installType"].toString("overlay");
        info.dirName = pkgObj["dirName"].toString();
        info.variantType = pkgObj["variantType"].toString();

        // Parse variants array
        QJsonArray variantsArr = pkgObj["variants"].toArray();
        for (const auto& v : variantsArr) {
            info.variants.append(v.toString());
        }

        // Parse contributors array [{name, role}]
        QJsonArray contribArr = pkgObj["contributors"].toArray();
        for (const auto& c : contribArr) {
            QJsonObject co = c.toObject();
            info.contributors.append(QVariantMap{
                {"name", co["name"].toString()},
                {"role", co["role"].toString()}
            });
        }

        // Parse installNotes (shown to user before installation)
        // Support both "installNotes" and "installNote" field names
        info.installNotes = pkgObj["installNotes"].toString();
        if (info.installNotes.isEmpty())
            info.installNotes = pkgObj["installNote"].toString();

        // Parse installMethod
        QJsonObject installMethod = pkgObj["installMethod"].toObject();
        if (!installMethod.isEmpty()) {
            info.installMethodType = installMethod["type"].toString();
            info.installMethodTarget = installMethod["target"].toString();

            QJsonArray stepsArr = installMethod["steps"].toArray();
            for (const auto& s : stepsArr) {
                QJsonObject so = s.toObject();
                InstallStep step;
                step.action = so["action"].toString();
                step.src = so["src"].toString();
                step.dest = so["dest"].toString();
                step.exe = so["exe"].toString();
                step.fallback = so["fallback"].toString();
                step.workDir = so["workDir"].toString("game");
                QJsonArray argsArr = so["args"].toArray();
                for (const auto& a : argsArr) {
                    step.args.append(a.toString());
                }
                info.installSteps.append(step);
            }
        }

        // Parse storeIds for cross-store resolution
        QJsonObject storeIdsObj = pkgObj["storeIds"].toObject();
        for (auto sit = storeIdsObj.begin(); sit != storeIdsObj.end(); ++sit) {
            const QString store = sit.key();
            const QString storeId = sit.value().toString();
            info.storeIds[store] = storeId;

            // Build reverse index for non-steam stores
            if (store == "epic") {
                m_storeIdToSteamAppId["epic_" + storeId] = info.steamAppId;
            } else if (store == "gog") {
                m_storeIdToSteamAppId["gog_" + storeId] = info.steamAppId;
            }
        }

        m_packages[info.steamAppId] = info;
    }
}

void LocalPackageManager::scanPackageDirectories(const QString& basePath)
{
    QDir pakDir(basePath);
    const auto entries = pakDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& dirName : entries) {
        // Check if this directory has an extracted_* or version subdirectory
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

        // If already loaded from manifest by packageId, update with disk info
        bool foundInManifest = false;
        for (auto it = m_packages.begin(); it != m_packages.end(); ++it) {
            if (it->packageId == dirName) {
                foundInManifest = true;
                if (hasExtracted) {
                    QDirIterator fileIter(extractedPath, QDir::Files, QDirIterator::Subdirectories);
                    int count = 0;
                    qint64 totalSize = 0;
                    while (fileIter.hasNext()) {
                        fileIter.next();
                        count++;
                        totalSize += fileIter.fileInfo().size();
                    }
                    it->fileCount = count;
                    it->sizeBytes = totalSize;
                }
                break;
            }
        }

        if (foundInManifest) continue;

        // Fallback: use hardcoded mapping if manifest didn't provide this package
        auto mappingIt = s_fallbackMapping.find(dirName);
        if (mappingIt == s_fallbackMapping.end()) {
            continue; // Unknown package, skip
        }

        const QString& steamAppId = mappingIt->first;
        const QString& gameName = mappingIt->second;

        // Skip if already loaded (e.g. manifest had it by steamAppId)
        if (m_packages.contains(steamAppId)) {
            auto& pkg = m_packages[steamAppId];
            if (pkg.packageId.isEmpty()) {
                pkg.packageId = dirName;
            }
            if (hasExtracted && pkg.fileCount == 0) {
                QDirIterator fileIter(extractedPath, QDir::Files, QDirIterator::Subdirectories);
                int count = 0;
                qint64 totalSize = 0;
                while (fileIter.hasNext()) {
                    fileIter.next();
                    count++;
                    totalSize += fileIter.fileInfo().size();
                }
                pkg.fileCount = count;
                pkg.sizeBytes = totalSize;
            }
            continue;
        }

        // Create package info from directory scan + fallback mapping
        PackageInfo info;
        info.packageId = dirName;
        info.steamAppId = steamAppId;
        info.gameName = gameName;
        info.installType = "overlay";
        info.storeIds["steam"] = steamAppId;

        if (hasExtracted) {
            QDirIterator fileIter(extractedPath, QDir::Files, QDirIterator::Subdirectories);
            int count = 0;
            qint64 totalSize = 0;
            while (fileIter.hasNext()) {
                fileIter.next();
                count++;
                totalSize += fileIter.fileInfo().size();
            }
            info.fileCount = count;
            info.sizeBytes = totalSize;
        }

        m_packages[steamAppId] = info;
    }
}

void LocalPackageManager::scanGameNameDirectories(const QString& basePath)
{
    QDir baseDir(basePath);
    const auto entries = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // Skip known non-package directories
    static const QSet<QString> skipDirs = {"pak", "mc-main", ".git"};

    for (const QString& dirName : entries) {
        if (skipDirs.contains(dirName)) continue;

        // Find manifest entry that matches this dirName
        bool found = false;
        for (auto it = m_packages.begin(); it != m_packages.end(); ++it) {
            if (it->dirName == dirName) {
                found = true;
                // Update file stats if not already set
                if (it->fileCount == 0) {
                    QString scanPath = basePath + "/" + dirName;
                    // If has variants, count files in first variant
                    if (!it->variants.isEmpty()) {
                        scanPath = basePath + "/" + dirName + "/" + it->variants.first();
                    }
                    QDirIterator fileIter(scanPath, QDir::Files, QDirIterator::Subdirectories);
                    int count = 0;
                    qint64 totalSize = 0;
                    while (fileIter.hasNext()) {
                        fileIter.next();
                        count++;
                        totalSize += fileIter.fileInfo().size();
                    }
                    it->fileCount = count;
                    it->sizeBytes = totalSize;
                }
                break;
            }
        }

        if (!found) {
            qDebug() << "scanGameNameDirectories: unrecognized directory" << dirName;
        }
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

QString LocalPackageManager::resolveGameId(const QString& gameId) const
{
    // Direct match -- already a steamAppId
    if (m_packages.contains(gameId)) {
        return gameId;
    }

    // Reverse lookup via store IDs (epic_xxx, gog_xxx)
    auto it = m_storeIdToSteamAppId.find(gameId);
    if (it != m_storeIdToSteamAppId.end()) {
        return it.value();
    }

    return {};
}

QVariantList LocalPackageManager::getVariants(const QString& steamAppId) const
{
    auto it = m_packages.find(steamAppId);
    if (it == m_packages.end()) return {};

    QVariantList result;
    result.reserve(it->variants.size());
    for (const QString& v : it->variants) {
        result.append(v);
    }
    return result;
}

QString LocalPackageManager::getVariantType(const QString& steamAppId) const
{
    auto it = m_packages.find(steamAppId);
    if (it == m_packages.end()) return {};
    return it->variantType;
}

QStringList LocalPackageManager::getPackageFileList(const QString& steamAppId, const QString& variant) const
{
    auto pkgIt = m_packages.find(steamAppId);
    if (pkgIt == m_packages.end()) return {};

    const PackageInfo& pkg = *pkgIt;

    // For script-based installs, compute target files from install steps
    // This ensures backup covers the actual game files that will be overwritten
    if (!pkg.installSteps.isEmpty()) {
        QStringList targetFiles;
        QString sourcePath = m_dataPath + "/" + pkg.dirName;
        for (const InstallStep& step : pkg.installSteps) {
            if (step.action == "copy") {
                targetFiles.append(step.dest);
            } else if (step.action == "copyDir") {
                // Scan the source dir to get the actual file list
                QString srcDir = QDir::cleanPath(sourcePath + "/" + step.src);
                QDirIterator dirIt(srcDir, QDir::Files, QDirIterator::Subdirectories);
                while (dirIt.hasNext()) {
                    dirIt.next();
                    QString relPath = dirIt.filePath().mid(srcDir.length() + 1);
                    targetFiles.append(step.dest + "/" + relPath);
                }
            }
            // "run", "delete", "installFont" don't produce predictable target files
        }
        return targetFiles;
    }

    // Default: scan package directory for overlay installs
    QString sourcePath;

    if (!pkg.dirName.isEmpty()) {
        sourcePath = !variant.isEmpty()
            ? m_dataPath + "/" + pkg.dirName + "/" + variant
            : m_dataPath + "/" + pkg.dirName;
    }

    if (sourcePath.isEmpty() || !QDir(sourcePath).exists()) {
        QString pkgDirPath = m_dataPath + "/pak/" + pkg.packageId;
        QDir pkgDir(pkgDirPath);
        if (pkgDir.exists()) {
            const auto subDirs = pkgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& sub : subDirs) {
                if (sub.startsWith("extracted_")) {
                    sourcePath = pkgDirPath + "/" + sub;
                    break;
                }
            }
        }
    }

    if (sourcePath.isEmpty() || !QDir(sourcePath).exists()) return {};

    QStringList files;
    QDirIterator it(sourcePath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files.append(it.filePath().mid(sourcePath.length() + 1));
    }
    return files;
}

QString LocalPackageManager::findMatchingAppId(const QString& folderName) const
{
    const QString normalized = folderName.toLower().trimmed();

    // Exact match against dirName or gameName (case-insensitive)
    for (auto it = m_packages.constBegin(); it != m_packages.constEnd(); ++it) {
        if (it->dirName.toLower() == normalized || it->gameName.toLower() == normalized)
            return it.key();
    }

    // Substring match: folder contains gameName or vice versa
    for (auto it = m_packages.constBegin(); it != m_packages.constEnd(); ++it) {
        const QString dirLower = it->dirName.toLower();
        const QString nameLower = it->gameName.toLower();
        if (normalized.contains(nameLower) || nameLower.contains(normalized) ||
            normalized.contains(dirLower) || dirLower.contains(normalized))
            return it.key();
    }

    return {};
}

int LocalPackageManager::packageCount() const
{
    return m_packages.size();
}

QVariantList LocalPackageManager::allPackagesAsList() const
{
    QVariantList result;
    result.reserve(m_packages.size());
    for (auto it = m_packages.constBegin(); it != m_packages.constEnd(); ++it) {
        const auto& pkg = it.value();
        result.append(QVariantMap{
            {"steamAppId", pkg.steamAppId},
            {"gameName", pkg.gameName},
            {"engine", pkg.engine},
            {"version", pkg.version},
            {"packageId", pkg.packageId},
            {"headerImageUrl", QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg").arg(pkg.steamAppId)},
        });
    }
    return result;
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
        InstalledPackageInfo info;

        if (it.value().isString()) {
            // Legacy format: steamAppId -> version string
            info.version = it.value().toString();
        } else if (it.value().isObject()) {
            // New format: steamAppId -> { version, gamePath, files, installedAt }
            QJsonObject obj = it.value().toObject();
            info.version = obj["version"].toString();
            info.gamePath = obj["gamePath"].toString();
            info.installedAt = obj["installedAt"].toInteger();
            QJsonArray filesArr = obj["files"].toArray();
            for (const auto& f : filesArr) {
                info.installedFiles.append(f.toString());
            }
        }

        m_installed[it.key()] = info;
    }
}

void LocalPackageManager::saveInstalledState()
{
    QString path = installedStatePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject root;
    for (auto it = m_installed.constBegin(); it != m_installed.constEnd(); ++it) {
        const InstalledPackageInfo& info = it.value();
        QJsonObject obj;
        obj["version"] = info.version;
        obj["gamePath"] = info.gamePath;
        obj["installedAt"] = info.installedAt;
        QJsonArray filesArr;
        for (const QString& f : info.installedFiles) {
            filesArr.append(f);
        }
        obj["files"] = filesArr;
        root[it.key()] = obj;
    }

    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    fileutils::atomicWriteJson(path, data);
}

#endif // MAKINEAI_UI_ONLY

// =============================================================================
// SHARED CODE (both core-delegated and UI-only builds)
// =============================================================================

QString LocalPackageManager::installedStatePath() const
{
    return AppPaths::installedPackagesFile();
}

// -- Install package ----------------------------------------------------------

void LocalPackageManager::installPackage(const QString& steamAppId, const QString& gamePath,
                                         const QString& variant)
{
    INTEGRITY_GATE();

    // Retrieve package info (works in both build modes)
    auto maybePkg = getPackage(steamAppId);
    if (!maybePkg) {
        emit installCompleted(false, tr("Package not found for AppID: %1").arg(steamAppId));
        return;
    }

    const PackageInfo pkg = *maybePkg;
    QString sourcePath;

    // Try new game-name directory format first
    if (!pkg.dirName.isEmpty()) {
        if (!variant.isEmpty()) {
            sourcePath = m_dataPath + "/" + pkg.dirName + "/" + variant;
        } else {
            sourcePath = m_dataPath + "/" + pkg.dirName;
        }
    }

    // Fall back to legacy pak/ format if game-name dir doesn't exist
    if (sourcePath.isEmpty() || !QDir(sourcePath).exists()) {
        QString pkgDirPath = m_dataPath + "/pak/" + pkg.packageId;
        QDir pkgDir(pkgDirPath);
        if (pkgDir.exists()) {
            const auto subDirs = pkgDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& sub : subDirs) {
                if (sub.startsWith("extracted_")) {
                    sourcePath = pkgDirPath + "/" + sub;
                    break;
                }
            }
        }
    }

    if (sourcePath.isEmpty() || !QDir(sourcePath).exists()) {
        emit installCompleted(false, tr("No translation data found for: %1").arg(pkg.gameName));
        return;
    }

    // Handle userPath install type: copy to user-relative path instead of game dir
    if (pkg.installMethodType == "userPath" && !pkg.installMethodTarget.isEmpty()) {
        const QString userHome = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        const QString targetPath = QDir::cleanPath(userHome + "/" + pkg.installMethodTarget);

        (void)QtConcurrent::run([this, steamAppId, targetPath, sourcePath, pkg]() {
            emit installProgress(0.0, tr("Dosyalar hazirlaniyor..."));

            QDir().mkpath(targetPath);

            QList<QPair<QString, QString>> filesToCopy;
            QDirIterator it(sourcePath, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString relPath = it.filePath().mid(sourcePath.length() + 1);
                // Skip internal dirs (e.g. _fonts)
                if (relPath.startsWith("_")) continue;
                filesToCopy.append({it.filePath(), relPath});
            }

            int total = filesToCopy.size();
            int copied = 0;
            int errors = 0;
            QStringList installedFiles;

            for (const auto& [srcPath, relPath] : filesToCopy) {
                QString destPath = QDir::cleanPath(targetPath + "/" + relPath);

                QFileInfo destInfo(destPath);
                if (!QDir().mkpath(destInfo.absolutePath())) {
                    errors++; continue;
                }
                if (QFile::exists(destPath)) QFile::remove(destPath);
                if (QFile::copy(srcPath, destPath)) {
                    copied++;
                    installedFiles.append(relPath);
                } else {
                    errors++;
                }

                if ((copied + errors) % 20 == 0 || (copied + errors) == total) {
                    emit installProgress(static_cast<double>(copied + errors) / total,
                        tr("%1/%2 dosya kopyalandi").arg(copied).arg(total));
                }
            }

            if (errors == 0) {
                QMetaObject::invokeMethod(this, [this, steamAppId, targetPath, pkg, installedFiles]() {
#ifndef MAKINEAI_UI_ONLY
                    packages::InstalledPackageState state;
                    state.version = pkg.version.toStdString();
                    state.gamePath = targetPath.toStdString();
                    for (const QString& f : installedFiles)
                        state.installedFiles.push_back(f.toStdString());
                    state.installedAt = QDateTime::currentSecsSinceEpoch();
                    m_catalog.markInstalled(steamAppId.toStdString(), state);
                    saveCatalogInstalledState(m_catalog, installedStatePath());
#else
                    InstalledPackageInfo instInfo;
                    instInfo.version = pkg.version;
                    instInfo.gamePath = targetPath;
                    instInfo.installedFiles = installedFiles;
                    instInfo.installedAt = QDateTime::currentSecsSinceEpoch();
                    m_installed[steamAppId] = instInfo;
                    saveInstalledState();
#endif
                }, Qt::QueuedConnection);
                emit installCompleted(true, tr("%1 dosya basariyla kuruldu").arg(copied));
            } else {
                emit installCompleted(false, tr("%1/%2 dosya kopyalanamadi").arg(errors).arg(total));
            }
        });
        return;
    }

    // Check for custom install recipe
    if (!pkg.installSteps.isEmpty()) {
        (void)QtConcurrent::run([this, steamAppId, gamePath, sourcePath, pkg]() {
            executeInstallSteps(pkg, gamePath, sourcePath);
        });
        return;
    }

    // Default overlay: copy all files preserving directory structure
    (void)QtConcurrent::run([this, steamAppId, gamePath, sourcePath, pkg]() {
        const QString extractedPath = sourcePath;
        emit installProgress(0.0, tr("Dosyalar hazirlanyor..."));

        // Begin crash recovery journal
        if (m_journal) {
            JournalEntry je;
            je.type = OpType::Install;
            je.gameId = steamAppId;
            je.gamePath = gamePath;
            m_journal->beginOperation(je);
        }

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
        int lastReported = 0;
        QStringList installedFiles;

        QString canonGamePath = QDir(gamePath).canonicalPath();
        if (canonGamePath.isEmpty())
            canonGamePath = QDir::cleanPath(gamePath);

        for (const auto& [srcPath, relPath] : filesToCopy) {
            QString destPath = QDir::cleanPath(gamePath + "/" + relPath);

            // Prevent path traversal: ensure destination stays within game directory
            if (!destPath.startsWith(canonGamePath)) {
                qWarning() << "Path traversal blocked during install:" << relPath;
                errors++;
                continue;
            }

            // Ensure destination directory exists
            QFileInfo destInfo(destPath);
            if (!QDir().mkpath(destInfo.absolutePath())) {
                qWarning() << "Failed to create directory:" << destInfo.absolutePath();
                errors++;
                continue;
            }

            // Copy file (overwrite if exists)
            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            if (QFile::copy(srcPath, destPath)) {
                copied++;
                installedFiles.append(relPath);
                if (m_journal) m_journal->recordFileModified(relPath);
            } else {
                qWarning() << "Failed to copy:" << srcPath << "->" << destPath;
                errors++;
            }

            // Throttle progress signals: every 20 files or at completion
            int done = copied + errors;
            if (done - lastReported >= 20 || done == total) {
                lastReported = done;
                double progress = static_cast<double>(done) / total;
                emit installProgress(progress,
                    tr("%1/%2 dosya kopyalandi").arg(copied).arg(total));
            }
        }

        if (errors == 0) {
            // Mark as installed with file tracking
            QMetaObject::invokeMethod(this, [this, steamAppId, gamePath, pkg, installedFiles]() {
#ifndef MAKINEAI_UI_ONLY
                packages::InstalledPackageState state;
                state.version = pkg.version.toStdString();
                state.gamePath = gamePath.toStdString();
                for (const QString& f : installedFiles)
                    state.installedFiles.push_back(f.toStdString());
                state.installedAt = QDateTime::currentSecsSinceEpoch();
                m_catalog.markInstalled(steamAppId.toStdString(), state);
                saveCatalogInstalledState(m_catalog, installedStatePath());
#else
                InstalledPackageInfo instInfo;
                instInfo.version = pkg.version;
                instInfo.gamePath = gamePath;
                instInfo.installedFiles = installedFiles;
                instInfo.installedAt = QDateTime::currentSecsSinceEpoch();
                m_installed[steamAppId] = instInfo;
                saveInstalledState();
#endif
                if (m_journal) m_journal->commitOperation();
            }, Qt::QueuedConnection);

            emit installCompleted(true,
                tr("%1 dosya basariyla kuruldu").arg(copied));
        } else {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false,
                tr("%1/%2 dosya kopyalanamadi").arg(errors).arg(total));
        }
    });
}

// -- Execute install steps (recipe-based) -------------------------------------

void LocalPackageManager::executeInstallSteps(const PackageInfo& pkg, const QString& gamePath,
                                               const QString& packageDir)
{
    INTEGRITY_GATE();
    const int total = pkg.installSteps.size();
    int current = 0;
    int errors = 0;
    QStringList installedFiles;

    // canonicalPath() returns empty for non-existent paths; fall back to cleanPath
    QString canonGamePath = QDir(gamePath).canonicalPath();
    if (canonGamePath.isEmpty())
        canonGamePath = QDir::cleanPath(gamePath);

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Install;
        je.gameId = pkg.steamAppId;
        je.gamePath = gamePath;
        m_journal->beginOperation(je);
    }

    emit installProgress(0.0, tr("Kurulum adimlari hazirlaniyor..."));

    for (const InstallStep& step : pkg.installSteps) {
        current++;
        double progress = static_cast<double>(current) / (total + 1);

        if (step.action == "copy") {
            // Copy a file from package dir to game dir
            QString srcPath = QDir::cleanPath(packageDir + "/" + step.src);
            QString destPath = QDir::cleanPath(gamePath + "/" + step.dest);

            // Path traversal protection
            if (!destPath.startsWith(canonGamePath)) {
                qWarning() << "Path traversal blocked in recipe copy:" << step.dest;
                errors++;
                continue;
            }

            if (!QFile::exists(srcPath)) {
                qWarning() << "Recipe copy source not found:" << srcPath;
                errors++;
                continue;
            }

            emit installProgress(progress, tr("Kopyalaniyor: %1").arg(step.dest));

            QFileInfo destInfo(destPath);
            if (!QDir().mkpath(destInfo.absolutePath())) {
                qWarning() << "Failed to create directory:" << destInfo.absolutePath();
                errors++;
                continue;
            }

            if (QFile::exists(destPath)) {
                QFile::remove(destPath);
            }

            if (QFile::copy(srcPath, destPath)) {
                installedFiles.append(step.dest);
                if (m_journal) m_journal->recordFileModified(step.dest);
            } else {
                qWarning() << "Recipe copy failed:" << srcPath << "->" << destPath;
                errors++;
            }

        } else if (step.action == "copyDir") {
            // Recursively copy a directory from package dir to game dir
            QString srcDir = QDir::cleanPath(packageDir + "/" + step.src);
            QString destDir = QDir::cleanPath(gamePath + "/" + step.dest);

            if (!destDir.startsWith(canonGamePath)) {
                qWarning() << "Path traversal blocked in recipe copyDir:" << step.dest;
                errors++;
                continue;
            }

            if (!QDir(srcDir).exists()) {
                qWarning() << "copyDir source not found:" << srcDir;
                errors++;
                continue;
            }

            emit installProgress(progress, tr("Kopyalaniyor: %1/").arg(step.dest));

            QDirIterator dirIt(srcDir, QDir::Files, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                QString relPath = dirIt.filePath().mid(srcDir.length() + 1);
                QString destPath = QDir::cleanPath(destDir + "/" + relPath);

                QFileInfo destInfo(destPath);
                if (!QDir().mkpath(destInfo.absolutePath())) {
                    errors++; continue;
                }
                if (QFile::exists(destPath)) QFile::remove(destPath);
                if (QFile::copy(dirIt.filePath(), destPath)) {
                    QString fullRelPath = step.dest + "/" + relPath;
                    installedFiles.append(fullRelPath);
                    if (m_journal) m_journal->recordFileModified(fullRelPath);
                } else {
                    qWarning() << "copyDir failed:" << dirIt.filePath() << "->" << destPath;
                    errors++;
                }
            }

        } else if (step.action == "delete") {
            // Delete a file in the game directory
            QString destPath = QDir::cleanPath(gamePath + "/" + step.dest);

            if (!destPath.startsWith(canonGamePath)) {
                qWarning() << "Path traversal blocked in recipe delete:" << step.dest;
                errors++;
                continue;
            }

            emit installProgress(progress, tr("Siliniyor: %1").arg(step.dest));

            if (QFile::exists(destPath)) {
                if (!QFile::remove(destPath)) {
                    qWarning() << "Recipe delete failed:" << destPath;
                    errors++;
                }
            }

        } else if (step.action == "installFont") {
            // Install TTF fonts from a package subdirectory (Windows per-user fonts)
            QString fontSrcDir = QDir::cleanPath(packageDir + "/" + step.src);

#ifdef Q_OS_WIN
            // Per-user font directory (no admin required, Windows 10 1809+)
            QString userFontsDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                                   + "/AppData/Local/Microsoft/Windows/Fonts";
            QDir().mkpath(userFontsDir);

            emit installProgress(progress, tr("Fontlar yukleniyor..."));

            QDirIterator fontIt(fontSrcDir, {"*.ttf", "*.otf"}, QDir::Files);
            while (fontIt.hasNext()) {
                fontIt.next();
                QString fontName = fontIt.fileName();
                QString destFont = userFontsDir + "/" + fontName;

                if (QFile::exists(destFont)) QFile::remove(destFont);
                if (QFile::copy(fontIt.filePath(), destFont)) {
                    QString fontEntry = "_font:" + fontName;
                    installedFiles.append(fontEntry);
                    if (m_journal) m_journal->recordFileModified(fontEntry);
                    qDebug() << "Font installed:" << fontName;
                } else {
                    qWarning() << "Font install failed:" << fontName;
                    errors++;
                }
            }
#else
            qWarning() << "installFont action only supported on Windows";
#endif

        } else if (step.action == "run") {
            // Resolve executable: check game dir first (may have been copied),
            // then package dir, then fallback
            QString exePath;
            QString gameExe = QDir::cleanPath(gamePath + "/" + step.exe);
            QString pkgExe = QDir::cleanPath(packageDir + "/" + step.exe);

            if (QFile::exists(gameExe)) {
                exePath = gameExe;
            } else if (QFile::exists(pkgExe)) {
                exePath = pkgExe;
            } else if (!step.fallback.isEmpty()) {
                QString gameFb = QDir::cleanPath(gamePath + "/" + step.fallback);
                QString pkgFb = QDir::cleanPath(packageDir + "/" + step.fallback);
                if (QFile::exists(gameFb)) exePath = gameFb;
                else if (QFile::exists(pkgFb)) exePath = pkgFb;
            }

            if (exePath.isEmpty()) {
                qWarning() << "Recipe run: executable not found:" << step.exe;
                errors++;
                continue;
            }

            emit installProgress(progress, tr("Calistiriliyor: %1").arg(QFileInfo(exePath).fileName()));

            // Determine working directory
            QString workDir = gamePath;
            if (step.workDir == "package") {
                workDir = packageDir;
            }

            // Resolve argument placeholders
            QStringList resolvedArgs;
            for (const QString& arg : step.args) {
                QString resolved = arg;
                resolved.replace("${gamePath}", gamePath);
                resolved.replace("${packageDir}", packageDir);
                resolvedArgs.append(resolved);
            }

            QProcess proc;
            proc.setWorkingDirectory(workDir);
            proc.setProcessChannelMode(QProcess::MergedChannels);
            proc.start(exePath, resolvedArgs);

            if (!proc.waitForStarted(10000)) {
                qWarning() << "Recipe run: failed to start:" << exePath;
                errors++;
                continue;
            }

            // Poll with periodic feedback (up to 30 minutes total)
            constexpr int kPollIntervalMs = 3000;
            constexpr int kMaxWaitMs = 1800000;
            int elapsed = 0;
            while (!proc.waitForFinished(kPollIntervalMs)) {
                elapsed += kPollIntervalMs;
                if (elapsed >= kMaxWaitMs) {
                    qWarning() << "Recipe run: timeout:" << exePath;
                    proc.kill();
                    proc.waitForFinished(2000);
                    errors++;
                    break;
                }
                int mins = elapsed / 60000;
                int secs = (elapsed % 60000) / 1000;
                emit installProgress(progress,
                    tr("Calisiyor: %1 (%2:%3)")
                        .arg(QFileInfo(exePath).fileName())
                        .arg(mins, 2, 10, QChar('0'))
                        .arg(secs, 2, 10, QChar('0')));
            }

            if (elapsed >= kMaxWaitMs) continue; // already counted as error

            if (proc.exitCode() != 0) {
                qWarning() << "Recipe run: non-zero exit:" << proc.exitCode()
                           << "output:" << proc.readAll().left(500);
                errors++;
            } else {
                qDebug() << "Recipe run OK:" << exePath;
            }
        } else {
            qWarning() << "Recipe: unknown action:" << step.action;
        }
    }

    if (errors == 0) {
        QMetaObject::invokeMethod(this, [this, steamAppId = pkg.steamAppId, gamePath, pkg, installedFiles]() {
#ifndef MAKINEAI_UI_ONLY
            packages::InstalledPackageState state;
            state.version = pkg.version.toStdString();
            state.gamePath = gamePath.toStdString();
            for (const QString& f : installedFiles)
                state.installedFiles.push_back(f.toStdString());
            state.installedAt = QDateTime::currentSecsSinceEpoch();
            m_catalog.markInstalled(steamAppId.toStdString(), state);
            saveCatalogInstalledState(m_catalog, installedStatePath());
#else
            InstalledPackageInfo instInfo;
            instInfo.version = pkg.version;
            instInfo.gamePath = gamePath;
            instInfo.installedFiles = installedFiles;
            instInfo.installedAt = QDateTime::currentSecsSinceEpoch();
            m_installed[steamAppId] = instInfo;
            saveInstalledState();
#endif
            if (m_journal) m_journal->commitOperation();
        }, Qt::QueuedConnection);

        emit installCompleted(true,
            tr("%1 adim basariyla tamamlandi").arg(total));
    } else {
        if (m_journal) m_journal->abortOperation();
        emit installCompleted(false,
            tr("%1/%2 adimda hata olustu").arg(errors).arg(total));
    }
}

// -- Uninstall package --------------------------------------------------------

bool LocalPackageManager::uninstallPackage(const QString& steamAppId, const QString& gamePath)
{
    INTEGRITY_GATE();

#ifndef MAKINEAI_UI_ONLY
    // Get installed state from core catalog
    auto maybeState = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!maybeState) return false;

    const auto& coreState = *maybeState;
    const QString basePath = coreState.gamePath.empty()
        ? gamePath
        : QString::fromStdString(coreState.gamePath);

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Uninstall;
        je.gameId = steamAppId;
        je.gamePath = basePath;
        m_journal->beginOperation(je);
    }

    // Delete installed files (with path traversal protection)
    const QString canonBase = QDir(basePath).canonicalPath();
    int deleted = 0;
    int failed = 0;

    for (const auto& relPathStd : coreState.installedFiles) {
        const QString relPath = QString::fromStdString(relPathStd);

        // Handle font entries: "_font:filename.ttf" -> user fonts dir
        if (relPath.startsWith("_font:")) {
#ifdef Q_OS_WIN
            QString fontName = relPath.mid(6); // strip "_font:" prefix
            QString fontPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts/" + fontName;
            if (QFile::exists(fontPath)) {
                if (QFile::remove(fontPath)) {
                    deleted++;
                    qDebug() << "Font removed:" << fontName;
                } else {
                    qWarning() << "Failed to remove font:" << fontName;
                    failed++;
                }
            }
#endif
            continue;
        }

        QString fullPath = QDir::cleanPath(basePath + "/" + relPath);
        // Prevent path traversal: ensure resolved path stays within game directory
        if (!fullPath.startsWith(canonBase)) {
            qWarning() << "Path traversal blocked:" << relPath;
            continue;
        }
        if (QFile::exists(fullPath)) {
            if (QFile::remove(fullPath)) {
                deleted++;
                if (m_journal) m_journal->recordFileModified(relPath);
            } else {
                qWarning() << "Failed to remove:" << fullPath;
                failed++;
            }
        }
    }

    qDebug() << "Uninstall" << steamAppId << ":" << deleted << "files deleted," << failed << "failed";

    m_catalog.markUninstalled(steamAppId.toStdString());
    saveCatalogInstalledState(m_catalog, installedStatePath());
    if (m_journal) m_journal->commitOperation();
    return true;

#else // MAKINEAI_UI_ONLY

    auto instIt = m_installed.find(steamAppId);
    if (instIt == m_installed.end()) {
        return false;
    }

    const InstalledPackageInfo& instInfo = instIt.value();
    const QString basePath = instInfo.gamePath.isEmpty() ? gamePath : instInfo.gamePath;

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Uninstall;
        je.gameId = steamAppId;
        je.gamePath = basePath;
        m_journal->beginOperation(je);
    }

    // Delete installed files (with path traversal protection)
    const QString canonBase = QDir(basePath).canonicalPath();
    int deleted = 0;
    int failed = 0;
    for (const QString& relPath : instInfo.installedFiles) {
        // Handle font entries: "_font:filename.ttf" -> user fonts dir
        if (relPath.startsWith("_font:")) {
#ifdef Q_OS_WIN
            QString fontName = relPath.mid(6); // strip "_font:" prefix
            QString fontPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts/" + fontName;
            if (QFile::exists(fontPath)) {
                if (QFile::remove(fontPath)) {
                    deleted++;
                    qDebug() << "Font removed:" << fontName;
                } else {
                    qWarning() << "Failed to remove font:" << fontName;
                    failed++;
                }
            }
#endif
            continue;
        }

        QString fullPath = QDir::cleanPath(basePath + "/" + relPath);
        // Prevent path traversal: ensure resolved path stays within game directory
        if (!fullPath.startsWith(canonBase)) {
            qWarning() << "Path traversal blocked:" << relPath;
            continue;
        }
        if (QFile::exists(fullPath)) {
            if (QFile::remove(fullPath)) {
                deleted++;
                if (m_journal) m_journal->recordFileModified(relPath);
            } else {
                qWarning() << "Failed to remove:" << fullPath;
                failed++;
            }
        }
    }

    qDebug() << "Uninstall" << steamAppId << ":" << deleted << "files deleted," << failed << "failed";

    m_installed.remove(steamAppId);
    saveInstalledState();
    if (m_journal) m_journal->commitOperation();
    return true;

#endif // MAKINEAI_UI_ONLY
}

} // namespace makineai
