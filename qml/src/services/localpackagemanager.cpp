// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file localpackagemanager.cpp
 * @brief Local translation package management implementation
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Network-based package catalog with on-demand detail enrichment.
 * operations delegate to makine::packages::PackageCatalog.
 * Install/uninstall file operations (QFile::copy, QProcess, etc.)
 * always remain in this QML service layer.
 */

#include "localpackagemanager.h"
#include "bytepatch.h"
#include "forgereimport.h"
#include "installsteprules.h"
#include "vpatchapply.h"
#include "xdeltaapply.h"
#include "elevatedops.h"
#include "profiler.h"
#include "operationjournal.h"
#include "pathsecurity.h"
#include "rawfile.h"
#include "appprotection.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QDirIterator>
#include <QRegularExpression>
#include <QDateTime>
#include <QProcess>
#include <QStorageInfo>
#include <QThread>
#include <QtConcurrent>
#include <QSet>
#include <QCoreApplication>
#include <QDesktopServices>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#  include <objbase.h>
#endif

#include <functional>
#include <set>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

Q_LOGGING_CATEGORY(lcPackageManager, "makine.package")

namespace makine {

// Defined further below, next to the overlay install path. Declared here so
// getPackageFileList() can apply the exact same wrapper stripping the install
// performs: the pre-install backup must cover the paths that actually get
// overwritten, not the wrapped paths as they sit inside the package.
static void stripWrapperPrefix(QList<QPair<QString, QString>>& files,
                               const QString& gamePath);

// =============================================================================
// CONSTRUCTION / LOADING
// =============================================================================

LocalPackageManager::LocalPackageManager(QObject *parent)
    : QObject(parent)
{
}

bool LocalPackageManager::loadFromIndex(const QString& indexPath, const QString& packageCacheRoot)
{
    MAKINE_ZONE_NAMED("LPM::loadFromIndex");
    // Called from the scan worker as well as the main thread, always with the
    // same root: assign only on change, so repeated loads never rewrite a
    // QString the other thread may be reading.
    if (m_dataPath != packageCacheRoot)
        m_dataPath = packageCacheRoot;

    bool ok = false;
    try {
        ok = m_catalog.loadFromIndex(indexPath.toStdString(),
                                     packageCacheRoot.toStdString());

        std::string statePath = installedStatePath().toStdString();
        m_catalog.loadInstalledState(statePath);
    } catch (const std::exception& e) {
        qCWarning(lcPackageManager) << "Failed to load package index:" << e.what();
        return false;
    }

    qCDebug(lcPackageManager) << "LocalPackageManager: loaded" << m_catalog.packageCount()
             << "packages from index (network-only)";
    return ok;
}

bool LocalPackageManager::enrichPackageDetail(const QString& steamAppId, const QByteArray& jsonData)
{
    return m_catalog.enrichPackage(steamAppId.toStdString(), jsonData.toStdString());
}

bool LocalPackageManager::isDetailLoaded(const QString& steamAppId) const
{
    return m_catalog.isDetailLoaded(steamAppId.toStdString());
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
    info.tier          = QString::fromStdString(entry.tier);
    info.lastUpdated   = QString::fromStdString(entry.lastUpdated);
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
    auto convertSteps = [](const std::vector<packages::InstallStep>& src) {
        QList<InstallStep> result;
        for (const auto& s : src) {
            InstallStep step;
            step.action   = QString::fromStdString(s.action);
            step.src      = QString::fromStdString(s.src);
            step.dest     = QString::fromStdString(s.dest);
            step.exe      = QString::fromStdString(s.exe);
            step.fallback = QString::fromStdString(s.fallback);
            step.workDir  = QString::fromStdString(s.workDir);
            step.language = QString::fromStdString(s.language);
            for (const auto& a : s.args) {
                step.args.append(QString::fromStdString(a));
            }
            for (const auto& [find, replace] : s.patches) {
                step.patches.append({QString::fromStdString(find),
                                     QString::fromStdString(replace)});
            }
            result.append(step);
        }
        return result;
    };

    info.installSteps = convertSteps(entry.installSteps);

    // Convert install options
    for (const auto& opt : entry.installOptions) {
        InstallOptionQt optQt;
        optQt.id              = QString::fromStdString(opt.id);
        optQt.label           = QString::fromStdString(opt.label);
        optQt.description     = QString::fromStdString(opt.description);
        optQt.icon            = QString::fromStdString(opt.icon);
        optQt.defaultSelected = opt.defaultSelected;
        optQt.subDir          = QString::fromStdString(opt.subDir);
        optQt.steps           = convertSteps(opt.steps);
        info.installOptions.append(optQt);
    }

    // Convert combined steps
    for (const auto& [key, steps] : entry.combinedSteps) {
        info.combinedSteps[QString::fromStdString(key)] = convertSteps(steps);
    }

    info.specialDialog = QString::fromStdString(entry.specialDialog);

    // Convert variant install options
    for (const auto& [variantName, vc] : entry.variantInstallOptions) {
        VariantConfigQt vcQt;
        for (const auto& opt : vc.installOptions) {
            InstallOptionQt optQt;
            optQt.id              = QString::fromStdString(opt.id);
            optQt.label           = QString::fromStdString(opt.label);
            optQt.description     = QString::fromStdString(opt.description);
            optQt.icon            = QString::fromStdString(opt.icon);
            optQt.defaultSelected = opt.defaultSelected;
            optQt.subDir          = QString::fromStdString(opt.subDir);
            optQt.steps           = convertSteps(opt.steps);
            vcQt.installOptions.append(optQt);
        }
        for (const auto& [key, steps] : vc.combinedSteps) {
            vcQt.combinedSteps[QString::fromStdString(key)] = convertSteps(steps);
        }
        info.variantInstallOptions[QString::fromStdString(variantName)] = vcQt;
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

std::optional<InstalledPackageInfo> LocalPackageManager::getInstalledInfo(const QString& steamAppId) const
{
    auto state = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!state) return std::nullopt;

    InstalledPackageInfo info;
    info.version = QString::fromStdString(state->version);
    info.gamePath = QString::fromStdString(state->gamePath);
    info.installedAt = state->installedAt;
    info.gameStoreVersion = QString::fromStdString(state->gameStoreVersion);
    info.gameSource = QString::fromStdString(state->gameSource);
    for (const auto& f : state->installedFiles)
        info.installedFiles.append(QString::fromStdString(f));
    for (const auto& f : state->addedFiles)
        info.addedFiles.append(QString::fromStdString(f));
    for (const auto& f : state->replacedFiles)
        info.replacedFiles.append(QString::fromStdString(f));
    return info;
}

QStringList LocalPackageManager::missingInstalledFiles(const QString& steamAppId) const
{
    auto state = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!state) return {};

    const QString gamePath = QString::fromStdString(state->gamePath);
    if (gamePath.isEmpty()) return {};

    QStringList missing;
    for (const auto& f : state->installedFiles) {
        const QString rel = QString::fromStdString(f);
        if (rel.startsWith(QLatin1Char('_'))) continue;
        if (!rawfile::exists(QDir::cleanPath(gamePath + QLatin1Char('/') + rel)))
            missing.append(rel);
    }
    return missing;
}

void LocalPackageManager::updateInstalledStoreVersion(const QString& steamAppId,
                                                        const QString& storeVersion,
                                                        const QString& source)
{
    auto state = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!state) return;

    state->gameStoreVersion = storeVersion.toStdString();
    state->gameSource = source.toStdString();
    m_catalog.markInstalled(steamAppId.toStdString(), *state);

    try {
        m_catalog.saveInstalledState(installedStatePath().toStdWString());
    } catch (const std::exception& e) {
        qCWarning(lcPackageManager) << "Failed to save installed state:" << e.what();
    }
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

static QString resolveOptionDir(const QString& baseDir, const QString& subDir);
static void collectCopyTargets(const QList<InstallStep>& steps, const QString& packageDir,
                               QStringList& out, QSet<QString>& seen);

QStringList LocalPackageManager::getPackageFileList(const QString& steamAppId,
                                                     const QString& variant,
                                                     const QString& gamePath) const
{
    // Resolve the source the same way the install will. Letting the catalog
    // guess meant the two could point at different trees: the install found the
    // package, the backup list came back empty, and the else-branch in
    // installPackageCommon reads an empty list as "nothing to back up" rather
    // than "could not look". The user was then left with a patched game that
    // uninstall refuses to touch, permanently.
    QString sourcePath;
    if (const auto pkg = getPackage(steamAppId))
        sourcePath = resolveSourcePath(*pkg, variant);

    auto files = m_catalog.getPackageFileList(steamAppId.toStdString(), variant.toStdString(),
                                              sourcePath.toStdString());

    // The catalog lists paths as they sit inside the package. The overlay
    // install strips mis-packaged wrapper folders ("Türkçe Yama/", the game's
    // own name, "Apex/", …) before copying, so files land in the game root.
    // Backing up the unstripped paths would target "<wrapper>/file" — which
    // the game does not have — while the install overwrites "file". The backup
    // then looks successful but holds nothing, and uninstall has no original
    // to restore, leaving the game patched forever. Strip here with the same
    // rules so both sides describe the same destinations.
    QList<QPair<QString, QString>> pairs;
    pairs.reserve(static_cast<int>(files.size()));
    for (const auto& f : files)
        pairs.append({QString(), QString::fromStdString(f)});

    stripWrapperPrefix(pairs, gamePath);

    QStringList result;
    result.reserve(pairs.size());
    for (const auto& p : pairs)
        result.append(p.second);

    // Files the RECIPE rewrites that the package does not contain.
    //
    // The list above enumerates what we copy in, which is the whole story for
    // an overlay package. A vpatch step is different: it rewrites an archive
    // the game already owns, and that file appears nowhere in the package. Left
    // out, the backup would not hold it, uninstall could not put it back, and
    // the guard in GameService that refuses to patch without a backup would be
    // satisfied by a list that covers everything except the file being changed.
    //
    // Every declared target, not just the selected options': backing up one
    // file too many costs disk, backing up one too few costs the game.
    if (const auto pkg = getPackage(steamAppId)) {
        // Copy targets of the recipe that will actually run (see
        // collectCopyTargets), from the same folders the install reads.
        QSet<QString> seen(result.cbegin(), result.cend());
        if (!variant.isEmpty() && pkg->variantInstallOptions.contains(variant)) {
            const QString base = m_dataPath + "/" + pkg->dirName + "/" + variant;
            const auto& vc = pkg->variantInstallOptions[variant];
            for (const InstallOptionQt& opt : vc.installOptions)
                collectCopyTargets(opt.steps, resolveOptionDir(base, opt.subDir), result, seen);
            for (const auto& steps : vc.combinedSteps)
                collectCopyTargets(steps, base, result, seen);
        } else if (pkg->installMethodType == QLatin1String("options")) {
            const QString base = m_dataPath + "/" + pkg->dirName;
            for (const InstallOptionQt& opt : pkg->installOptions)
                collectCopyTargets(opt.steps, resolveOptionDir(base, opt.subDir), result, seen);
            for (const auto& steps : pkg->combinedSteps)
                collectCopyTargets(steps, base, result, seen);
        } else if (!pkg->installSteps.isEmpty() && !sourcePath.isEmpty()) {
            collectCopyTargets(pkg->installSteps, sourcePath, result, seen);
        }

        const auto collect = [&result](const QList<InstallStep>& steps) {
            for (const InstallStep& st : steps) {
                if ((st.action == QLatin1String("vpatch")
                     || st.action == QLatin1String("forgeReimport")
                     || st.action == QLatin1String("xdelta"))
                    && !st.dest.isEmpty() && !result.contains(st.dest))
                    result.append(st.dest);
                // patchBytes writes dest, or src in place when dest is empty.
                if (st.action == QLatin1String("patchBytes")) {
                    const QString target = st.dest.isEmpty() ? st.src : st.dest;
                    if (!target.isEmpty() && !result.contains(target))
                        result.append(target);
                }
            }
        };
        collect(pkg->installSteps);
        for (const InstallOptionQt& opt : pkg->installOptions) collect(opt.steps);
        for (const auto& steps : pkg->combinedSteps) collect(steps);
        for (const auto& vc : pkg->variantInstallOptions) {
            for (const InstallOptionQt& opt : vc.installOptions) collect(opt.steps);
            for (const auto& steps : vc.combinedSteps) collect(steps);
        }
    }

    // Checked on the final list: the catalog's part is empty for every recipe
    // made only of in-place steps (forgeReimport, xdelta), which the recipe
    // targets above then fill.
    if (result.isEmpty() && !sourcePath.isEmpty()) {
        // Resolved yet empty. Says so out loud rather than letting the install
        // proceed unbacked on a silently empty list.
        qCWarning(lcPackageManager)
            << "getPackageFileList: resolved" << sourcePath
            << "but it lists no files — install would run without a backup";
    }
    return result;
}

QStringList LocalPackageManager::unknownRecipeActions(const QString& steamAppId) const
{
    QStringList out;
    const auto pkg = getPackage(steamAppId);
    if (!pkg) return out;
    const auto scan = [&out](const QList<InstallStep>& steps) {
        for (const InstallStep& st : steps) {
            if (!st.action.isEmpty() && !steprules::isKnownAction(st.action)
                && !out.contains(st.action))
                out << st.action;
        }
    };
    scan(pkg->installSteps);
    for (const InstallOptionQt& opt : pkg->installOptions) scan(opt.steps);
    for (const auto& steps : pkg->combinedSteps) scan(steps);
    for (const auto& vc : pkg->variantInstallOptions) {
        for (const InstallOptionQt& opt : vc.installOptions) scan(opt.steps);
        for (const auto& steps : vc.combinedSteps) scan(steps);
    }
    return out;
}

bool LocalPackageManager::recipeWritesInPlace(const QString& steamAppId) const
{
    const auto pkg = getPackage(steamAppId);
    if (!pkg) return false;
    const auto any = [](const QList<InstallStep>& steps) {
        for (const InstallStep& st : steps)
            if (steprules::isInPlaceAction(st.action)) return true;
        return false;
    };
    if (any(pkg->installSteps)) return true;
    for (const InstallOptionQt& opt : pkg->installOptions) if (any(opt.steps)) return true;
    for (const auto& steps : pkg->combinedSteps) if (any(steps)) return true;
    for (const auto& vc : pkg->variantInstallOptions) {
        for (const InstallOptionQt& opt : vc.installOptions) if (any(opt.steps)) return true;
        for (const auto& steps : vc.combinedSteps) if (any(steps)) return true;
    }
    return false;
}

QString LocalPackageManager::findMatchingAppId(const QString& folderName) const
{
    return QString::fromStdString(m_catalog.findMatchingAppId(folderName.toStdString()));
}

QVariantList LocalPackageManager::findMatchingGamesFromFiles(
    const QStringList& exeNames, const QString& engine,
    const QStringList& topEntries, const QString& folderName) const
{
    std::vector<std::string> exeVec, topVec;
    exeVec.reserve(exeNames.size());
    topVec.reserve(topEntries.size());
    for (const auto& e : exeNames) exeVec.push_back(e.toStdString());
    for (const auto& t : topEntries) topVec.push_back(t.toStdString());

    auto matches = m_catalog.findMatchingGames(
        exeVec, engine.toStdString(), topVec, folderName.toStdString());

    QVariantList result;
    for (const auto& m : matches) {
        QVariantMap entry;
        entry[QStringLiteral("steamAppId")] = QString::fromStdString(m.steamAppId);
        entry[QStringLiteral("confidence")] = m.confidence;
        entry[QStringLiteral("matchedBy")] = QString::fromStdString(m.matchedBy);
        result.append(entry);
    }
    return result;
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
        });
    }
    return result;
}

QVariantMap LocalPackageManager::getAllExeMap() const
{
    QVariantMap result;
    auto map = m_catalog.getAllExeMap();
    for (const auto& [exe, appId] : map) {
        result.insert(QString::fromStdString(exe), QString::fromStdString(appId));
    }
    return result;
}

QString LocalPackageManager::getGameName(const QString& steamAppId) const
{
    auto pkg = m_catalog.getPackage(steamAppId.toStdString());
    if (pkg) return QString::fromStdString(pkg->gameName);
    return {};
}

// -- Installed state persistence (delegated to core) --------------------------

// Private helper: save installed state through core catalog
static void saveCatalogInstalledState(packages::PackageCatalog& catalog, const QString& statePath)
{
    try {
        catalog.saveInstalledState(statePath.toStdString());
    } catch (const std::exception& e) {
        qCWarning(lcPackageManager) << "Failed to persist installed state:" << e.what();
    }
}

// =============================================================================
// SHARED CODE
// =============================================================================



QString LocalPackageManager::installedStatePath() const
{
    return AppPaths::installedPackagesFile();
}

// -- Smart copy helper --------------------------------------------------------

QString LocalPackageManager::quarantinedMessage(const QString& file)
{
    return tr("Antivirüs (ör. Windows Defender) bir yama dosyasını engelledi: %1\n\n"
              "Bu dosya çevirinin çalışması için gerekli. Çözüm:\n"
              "1) Windows Güvenliği > Virüs ve tehdit koruması > Koruma geçmişi'nden "
              "dosyayı geri yükleyin (\"İzin ver\")\n"
              "2) Oyun klasörünü ve Makine Launcher'ı dışlamalara ekleyin\n"
              "Sonra yamayı tekrar kurun.").arg(QFileInfo(file).fileName());
}

LocalPackageManager::CopyResult LocalPackageManager::tryCopyFile(
    const QString& src, const QString& dest)
{
    m_lastCopyWinError = 0;
    // Windows reports access denied as OpenError like any other failed open,
    // so the error code cannot tell a folder we may not write (C:\Program
    // Files) from a file held open by the game or a virus scan. The folder
    // can: a permission wall is not worth the lock back-off below.
    const auto lockedOrDenied = [&dest]() {
        return ElevatedOps::canWrite(QFileInfo(dest).absolutePath())
            ? CopyError::FileLocked : CopyError::PermissionDenied;
    };

    // Single attempt: try the copy and classify the failure mode.
    auto attempt = [&]() -> CopyResult {
        // Remove existing dest first; if removal fails, classify immediately
        if (rawfile::exists(dest)) {
            // A read-only original refuses deletion exactly like a locked one;
            // clear the attribute before deciding which it is.
            if (!QFile::remove(dest)
                && !(QFile::setPermissions(dest, QFile::permissions(dest)
                                                   | QFileDevice::WriteOwner
                                                   | QFileDevice::WriteUser)
                     && QFile::remove(dest))) {
                // Append: a plain WriteOnly open truncates the original.
                QFile probe(dest);
                if (!probe.open(QIODevice::WriteOnly | QIODevice::Append)) {
                    auto fe = probe.error();
                    if (fe == QFileDevice::PermissionsError)
                        return {false, CopyError::PermissionDenied};
                    if (fe == QFileDevice::ResourceError)
                        return {false, CopyError::DiskFull};
                }
                return {false, lockedOrDenied()};
            }
        }

#ifdef Q_OS_WIN
        // CopyFileW rather than QFile::copy: the latter drops the Windows error,
        // and "locked" in the field was three different things — access denied
        // (5), a sharing violation (32) and antivirus (225/226).
        const auto win = [](const QString& p) {
            QString n = QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
            if (n.size() >= 248 && !n.startsWith(QLatin1String("\\\\?\\")))
                n = n.startsWith(QLatin1String("\\\\"))
                    ? QLatin1String("\\\\?\\UNC\\") + n.mid(2)
                    : QLatin1String("\\\\?\\") + n;
            return n.toStdWString();
        };
        if (::CopyFileW(win(src).c_str(), win(dest).c_str(), FALSE)) return {true, CopyError::None};
        m_lastCopyWinError = ::GetLastError();
        if (m_lastCopyWinError == 225 || m_lastCopyWinError == 226)   // ERROR_VIRUS_INFECTED / _DELETED
            return {false, CopyError::Quarantined};
        if (m_lastCopyWinError == ERROR_DISK_FULL || m_lastCopyWinError == ERROR_HANDLE_DISK_FULL)
            return {false, CopyError::DiskFull};
        if (m_lastCopyWinError == ERROR_SHARING_VIOLATION || m_lastCopyWinError == ERROR_LOCK_VIOLATION)
            return {false, CopyError::FileLocked};
#else
        if (QFile::copy(src, dest)) return {true, CopyError::None};
#endif

        // NewOnly: a file that is still there is not ours to truncate or remove.
        QFile probe(dest);
        if (!probe.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            auto fe = probe.error();
            if (fe == QFileDevice::PermissionsError)
                return {false, CopyError::PermissionDenied};
            if (fe == QFileDevice::ResourceError)
                return {false, CopyError::DiskFull};
            if (fe == QFileDevice::OpenError && QFileInfo(dest).absoluteDir().exists())
                return {false, lockedOrDenied()};
        } else {
            probe.close();
            probe.remove();
        }
        return {false, CopyError::Other};
    };

    // First try — the common case where the destination is free.
    CopyResult result = attempt();
    if (result.ok || result.error != CopyError::FileLocked)
        return result;

    // FileLocked: antivirus / Steam scanner / Game Bar may hold the file
    // for several seconds. Back off exponentially before giving up.
    static constexpr int kBackoffMs[] = {150, 500, 2000, 5000}; // ~7.6s total
    for (int delay : kBackoffMs) {
        if (isCancelled()) return result;
        QThread::msleep(delay);
        result = attempt();
        if (result.ok || result.error != CopyError::FileLocked)
            return result;
    }
    return result;
}

#ifdef Q_OS_WIN
namespace {

struct ElevatedRun {
    bool   started  = false;
    bool   declined = false;   // user dismissed the UAC prompt
    DWORD  lastError = 0;
    HANDLE process  = nullptr;
};

// Start a binary through the shell with the "runas" verb.
//
// CreateProcess — and therefore QProcess — cannot start an executable whose
// manifest asks for requireAdministrator: it fails with
// ERROR_ELEVATION_REQUIRED (740) and no amount of retrying helps. Translation
// patchers ship as exactly such binaries (ERING_TR.exe), which is why the
// step failed for 78 users with "İstenen işlem için yükseltme gerekiyor".
//
// Relaunching the launcher elevated is not an option: the shipped build is an
// MSIX package, and Store apps have no "Run as administrator". Raising the UAC
// prompt for the child is the only path that works there.
//
// Trade-off against QProcess: an elevated child cannot inherit our pipes, so
// stdout is not captured. Exit code and cancellation still work via the handle.
// The child runs outside our MSIX container, so it is handed real paths
// (ElevatedOps::realPath) — the virtualised ones only we can see made the
// Elden Ring patcher fail with ERROR_PATH_NOT_FOUND for most of its users.
ElevatedRun startElevated(const QString& exePath, const QStringList& args,
                          const QString& workDir)
{
    ElevatedRun out;
    const QString nativeExe =
        QDir::toNativeSeparators(ElevatedOps::realPath(exePath));
    const QString nativeDir =
        QDir::toNativeSeparators(ElevatedOps::realPath(workDir));

    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString& a : args) {
        quoted << (a.contains(QLatin1Char(' '))
                       ? QStringLiteral("\"") + a + QStringLiteral("\"")
                       : a);
    }
    const QString params = quoted.join(QLatin1Char(' '));

    // ShellExecuteEx expects COM on the calling thread; install steps run on a
    // QtConcurrent worker where it is not initialised.
    const HRESULT hr = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool ownsCom = SUCCEEDED(hr);

    SHELLEXECUTEINFOW info{};
    info.cbSize       = sizeof(info);
    info.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC
                        | SEE_MASK_FLAG_NO_UI;
    info.lpVerb       = L"runas";
    info.lpFile       = reinterpret_cast<LPCWSTR>(nativeExe.utf16());
    info.lpParameters = params.isEmpty()
                            ? nullptr
                            : reinterpret_cast<LPCWSTR>(params.utf16());
    info.lpDirectory  = reinterpret_cast<LPCWSTR>(nativeDir.utf16());
    info.nShow        = SW_SHOWNORMAL;

    if (ShellExecuteExW(&info)) {
        out.started = true;
        out.process = info.hProcess;
    } else {
        out.lastError = GetLastError();
        out.declined  = (out.lastError == ERROR_CANCELLED);
    }

    if (ownsCom) CoUninitialize();
    return out;
}

} // namespace
#endif // Q_OS_WIN

LocalPackageManager::ProcessResult LocalPackageManager::runProcess(
    const QString& exePath, const QStringList& args, const QString& workDir,
    std::function<void(int elapsedMs)> progressCallback)
{
    constexpr int kPollMs = 1000;        // Poll faster so cancel feels responsive
    constexpr int kMaxMs  = 1800000;     // 30 minutes hard cap

    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(exePath, args);
    // Nothing is ever typed into these tools. Closing stdin turns a
    // "press a key to exit" pause (QuickBMS on an error) into an immediate
    // exit instead of a 30-minute wait.
    proc.closeWriteChannel();

    ProcessResult result;
    if (!proc.waitForStarted(10000)) {
        // Record WHY. "failed to start" on its own cannot be triaged: the same
        // line covers a missing file, a binary blocked by antivirus, a denied
        // permission and a wrong architecture — and the user is told to check
        // their antivirus in every case. Eight users hit this on Elden Ring
        // with nothing in the report to tell the causes apart.
        const QFileInfo info(exePath);
        qCWarning(lcPackageManager)
            << "Process failed to start:" << exePath
            << "| error:" << static_cast<int>(proc.error()) << proc.errorString()
            << "| exists:" << info.exists() << "size:" << info.size()
            << "| workDir:" << workDir
            << "| workDirExists:" << QDir(workDir).exists();

#ifdef Q_OS_WIN
        // A binary that requires elevation cannot be started any other way.
        // Retry through the shell so Windows can raise the UAC prompt; without
        // this the step is unfixable by the user, because the Store build of
        // the launcher cannot itself be run as administrator.
        if (info.exists() && proc.error() == QProcess::FailedToStart) {
            const ElevatedRun run = startElevated(exePath, args, workDir);
            if (run.declined) {
                qCWarning(lcPackageManager)
                    << "Elevated start declined by user:" << exePath;
                result.elevationDeclined = true;
                return result;
            }
            if (!run.started) {
                qCWarning(lcPackageManager)
                    << "Elevated start failed:" << exePath
                    << "| win32:" << static_cast<int>(run.lastError);
                return result;
            }
            qCInfo(lcPackageManager) << "Started elevated:" << exePath;
            result.started = true;

            int elevElapsed = 0;
            for (;;) {
                if (WaitForSingleObject(run.process, kPollMs) == WAIT_OBJECT_0)
                    break;
                if (isCancelled()) {
                    TerminateProcess(run.process, 1);
                    CloseHandle(run.process);
                    qCInfo(lcPackageManager)
                        << "Elevated process cancelled by user:" << exePath;
                    result.cancelled = true;
                    return result;
                }
                elevElapsed += kPollMs;
                if (elevElapsed >= kMaxMs) {
                    TerminateProcess(run.process, 1);
                    CloseHandle(run.process);
                    qCWarning(lcPackageManager)
                        << "Elevated process timeout:" << exePath;
                    result.timedOut = true;
                    return result;
                }
                if (progressCallback) progressCallback(elevElapsed);
            }
            DWORD code = 0;
            GetExitCodeProcess(run.process, &code);
            CloseHandle(run.process);
            result.exitCode = static_cast<int>(code);
            // Elevated children cannot share our pipes, so there is no output.
            return result;
        }
#endif
        return result;
    }
    result.started = true;

    int elapsed = 0;

    while (!proc.waitForFinished(kPollMs)) {
        if (isCancelled()) {
            qCInfo(lcPackageManager) << "Process cancelled by user:" << exePath;
            proc.kill();
            proc.waitForFinished(2000);
            result.cancelled = true;
            return result;
        }
        elapsed += kPollMs;
        if (elapsed >= kMaxMs) {
            qCWarning(lcPackageManager) << "Process timeout:" << exePath;
            proc.kill();
            proc.waitForFinished(2000);
            result.timedOut = true;
            return result;
        }
        if (progressCallback) progressCallback(elapsed);
    }

    result.exitCode = proc.exitCode();
    result.output = proc.readAll();
    return result;
}

// -- Shared helpers -----------------------------------------------------------

// A package folder as the user should see it. Under MSIX our AppData is
// virtualised: the path we hold does not exist in Explorer, so "Yama dosyaları
// şu klasöre çıkarıldı: <path>" sent 275 users to a folder they could not find.
static QString shownPath(const QString& path)
{
    return QDir::toNativeSeparators(ElevatedOps::realPath(path));
}

// A source folder that exists but holds no file is not a usable source.
//
// QDir::exists() is true for an empty directory, so a download that created the
// folder and then failed part-way still resolved. Install then continued and
// reported "Yama dosyaları şu klasöre çıkarıldı: <path>" for a path the user
// opens to find nothing — reported by mail for Wasteland 3 among others. Treat
// empty as absent so the caller falls back, or says "yeniden indirin" instead.
//
// Stops at the first hit, so this stays O(1) on healthy packages rather than
// walking a 5000-file tree.
static bool dirHasFiles(const QString& path)
{
    if (path.isEmpty())
        return false;
    QDirIterator it(path, rawfile::kFiles, QDirIterator::Subdirectories);
    return it.hasNext();
}

QString LocalPackageManager::resolveSourcePath(const PackageInfo& pkg, const QString& variant) const
{
    QString sourcePath;

    // Says out loud what used to be inferred from a downstream symptom. With no
    // variant the search starts at the package root, where a variant-foldered
    // archive holds one copy of every recipe path — the resolver then reports
    // "ambiguous at depth 1" and the step fails as if the package were broken.
    // A flat archive is unaffected, so this warns rather than refuses.
    if (variant.isEmpty() && !pkg.variants.isEmpty()) {
        qCWarning(lcPackageManager)
            << "resolveSourcePath: package declares variants" << pkg.variants
            << "but none was selected — searching from the package root";
    }

    // Try new game-name directory format first
    if (!pkg.dirName.isEmpty()) {
        sourcePath = !variant.isEmpty()
            ? m_dataPath + "/" + pkg.dirName + "/" + variant
            : m_dataPath + "/" + pkg.dirName;
    }

    // Variant folders inside a .makine archive are not named exactly like the
    // declared variant string. Hollow Knight declares "1.5.78"/"1.5.80" but
    // ships "v1.5.78.11833"/"1.5.80": the exact path resolves one variant and
    // misses the other, so the recipe falls back to the package root, finds two
    // equally-plausible copies of "hollow_knight_Data/resources.assets" and
    // refuses to guess ("ambiguous at depth 1"). Match tolerantly — ignore a
    // leading "v" and allow a longer build suffix — but require a UNIQUE hit,
    // because copying the wrong game version's assets breaks the game.
    if (!variant.isEmpty() && !pkg.dirName.isEmpty()
        && !sourcePath.isEmpty() && !dirHasFiles(sourcePath))
    {
        const QDir base(m_dataPath + "/" + pkg.dirName);
        QStringList hits;
        for (const QString& sub : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (steprules::variantFolderMatches(variant, sub))
                hits << sub;
        }
        if (hits.size() == 1) {
            sourcePath = base.filePath(hits.constFirst());
            qCInfo(lcPackageManager) << "resolveSourcePath: variant" << variant
                                     << "matched archive folder" << hits.constFirst();
        } else if (hits.size() > 1) {
            qCWarning(lcPackageManager) << "resolveSourcePath: variant" << variant
                << "matches multiple folders" << hits << "— refusing to guess";
        }
    }

    // Fall back to legacy pak/ format if game-name dir is missing or empty
    if (!dirHasFiles(sourcePath)) {
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

    // Return empty string if nothing found
    if (!dirHasFiles(sourcePath)) {
        if (!sourcePath.isEmpty() && QDir(sourcePath).exists()) {
            // Distinguishable in telemetry from "never downloaded": the folder
            // is there, the payload is not.
            qCWarning(lcPackageManager)
                << "resolveSourcePath: package folder exists but is empty:" << sourcePath;
        }
        return {};
    }

    return sourcePath;
}

// Fuzzy key for matching a recipe's folder name against one on disk.
//
// NOT a mirror of the extractor, despite what this comment used to claim. It
// said mkpkformat.h rewrites non-ASCII to '_', so "Türkçe Yama" would be on
// disk as "T_rk_e Yama". Half true, which cost a long false lead: a tar that
// keeps UTF-8 in the header's name field was decoded verbatim (measured
// 2026-09-19), but package_pipeline.py writes PAX, where that field holds
// "T?rk?e Yama" and the real name sits in a PAX record the extractor skipped
// until 2026-10-08 — so those packages did land as "T_rk_e Yama".
//
// The function still earns its place — collapsing both sides to a common key
// matches a correct on-disk name AND one an older build really did mangle, so
// a subDir lookup succeeds either way. It is a tolerance, not a mirror.
static QString extractorMangledName(const QString& name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar c : name) {
        const bool illegal = c.unicode() > 127
            || c == u'?' || c == u'*' || c == u'"'
            || c == u'<' || c == u'>' || c == u'|';
        out += illegal ? QLatin1Char('_') : c;
    }
    return out;
}

// Strip mis-packaged top-level wrapper directories so overlay files land
// in the game root instead of game/<wrapper>/. Many patches ship wrapped
// in a human-named folder — "Türkçe Yama/", the game's own name
// ("Watch Dogs Legion/"), a source tag ("Apex/"), etc. Copying that
// wrapper verbatim drops the translation one level too deep: the game
// never reads it, yet every file copy succeeds, so the install reports a
// silent false success ("yama kuruldu ama dil değişmiyor").
//
// Strips iteratively (collapses nested wrappers like
// "Apex/Watch Dogs Legion/…") while EVERY file shares one top dir AND
// that dir is a wrapper, where "wrapper" means:
//   (A) its name matches a known TR-patch keyword, OR
//   (B) no entry of that name exists in the real game install — a
//       structurally-rooted package always targets a path the game
//       already has (paks/, Content/, data_win64/, …), so a top dir
//       absent from the game is packaging noise, not real structure.
// The existence check (B) naturally halts at the first real game dir, so
// legitimately-rooted packages are never over-stripped, and the result
// is correct under ANY packaging layout (it never relies on guessing a
// specific package's internals). gamePath empty ⇒ can't verify ⇒ fall
// back to keyword-only (zero regression). Fixes all existing/future
// mis-wrapped overlay packages at install time, no re-packaging.
static void stripWrapperPrefix(QList<QPair<QString, QString>>& files,
                               const QString& gamePath)
{
    if (files.isEmpty())
        return;

    auto topDir = [](const QString& rel) -> QString {
        const int slash = rel.indexOf('/');
        return slash < 0 ? QString() : rel.left(slash);
    };

    static const QStringList kWrapperNames = {
        "turkceyama", "turkce", "turkishpatch", "turkish",
        "tryama", "yama", "ceviri", "turkishtranslation", "patch"
    };

    // Iterate so nested wrappers collapse fully; hard cap guards against
    // pathological deep nesting.
    for (int guard = 0; guard < 8; ++guard) {
        const QString wrapper = topDir(files.first().second);
        if (wrapper.isEmpty())
            return;  // at package root — nothing (more) wrapped
        for (const auto& f : files) {
            if (topDir(f.second) != wrapper)
                return;  // files span multiple top dirs — not one wrapper
        }

        // (A) Known TR-patch keyword? Normalize: lower-case +
        // transliterate Turkish letters + drop spaces/_/- so
        // "Türkçe Yama", "TURKCE_YAMA", "turkish-patch" all match.
        QString norm = wrapper.toLower();
        norm.replace(QChar(0x00E7), QChar('c'))   // ç
            .replace(QChar(0x011F), QChar('g'))   // ğ
            .replace(QChar(0x0131), QChar('i'))   // ı
            .replace(QChar(0x00F6), QChar('o'))   // ö
            .replace(QChar(0x015F), QChar('s'))   // ş
            .replace(QChar(0x00FC), QChar('u'));  // ü
        norm.remove(QChar(' ')).remove(QChar('_')).remove(QChar('-'));
        const bool knownName = kWrapperNames.contains(norm);

        // (B) Does the game install actually contain this top entry? If
        // it does, the package is correctly rooted here — stop. If not
        // (or we can't check), treat as wrapper only when (A) failed too.
        const bool existsInGame = !gamePath.isEmpty()
            && QFileInfo::exists(QDir::cleanPath(gamePath + "/" + wrapper));
        if (!knownName && (existsInGame || gamePath.isEmpty()))
            return;  // real game subdir (or unverifiable) — leave it

        const int cut = wrapper.length() + 1;  // remove "<wrapper>/"
        for (auto& f : files)
            f.second = f.second.mid(cut);

        qCInfo(lcPackageManager) << "stripWrapperPrefix: removed wrapper folder"
                                 << wrapper << (knownName ? "(known-name)" : "(absent-in-game)")
                                 << "from" << files.size() << "files";
    }
}

// Helper: collect every path under `root` (recursively, up to targetDepth
// levels of wrapper directories) at which `<dir>/<relSrc>` exists. Used
// by resolvePackageSource() for iterative-deepening search — kept as a
// separate function so we can early-exit on ambiguity per depth level.
static QStringList collectAtDepth(const QString& root, const QString& relSrc,
                                  int targetDepth)
{
    if (targetDepth == 0) {
        const QString cand = QDir::cleanPath(root + "/" + relSrc);
        return rawfile::exists(cand) ? QStringList{cand} : QStringList{};
    }
    QStringList results;
    const auto subs = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sub : subs) {
        results.append(collectAtDepth(
            QDir::cleanPath(root + "/" + sub), relSrc, targetDepth - 1));
        if (results.size() > 1)
            return results;          // ambiguity at this depth — stop early
    }
    return results;
}

// Resolve a recipe-relative source against the package dir, tolerant of
// however many wrapper/sub-directories the package is (in)consistently
// nested in. The most common cases are one wrapper ("Türkçe Yama/...")
// or two ("<Game Name>/Türkçe Yama/...", which is how some .makine
// archives — e.g. Elden Ring — extract). Used by EVERY step source lookup
// so no install path (recipe or options, copy/copyFile/copyDir, run,
// copyToDesktop) can hard-fail with "Adım N hata" just because the
// package nesting differs from the recipe.
//
// Strategy: iterative deepening. The strict path always wins (zero
// regression). Otherwise search depth=1, then depth=2, then depth=3,
// returning the first depth at which exactly ONE unambiguous candidate
// exists. Ambiguity at a depth → strict (we never guess between
// equally-shallow matches). Returns strict on total miss so the caller's
// existing not-found handling/message is unchanged.
static QString resolvePackageSource(const QString& packageDir, const QString& relSrc)
{
    const QString strict = QDir::cleanPath(packageDir + "/" + relSrc);
    if (rawfile::exists(strict))
        return strict;

    constexpr int kMaxWrapperDepth = 3;
    for (int depth = 1; depth <= kMaxWrapperDepth; ++depth) {
        const QStringList found = collectAtDepth(packageDir, relSrc, depth);
        if (found.size() == 1) {
            qCInfo(lcPackageManager) << "resolvePackageSource: resolved" << relSrc
                                     << "at wrapper depth" << depth
                                     << "->" << found.first();
            return found.first();
        }
        if (found.size() > 1) {
            qCWarning(lcPackageManager) << "resolvePackageSource: ambiguous at depth"
                                        << depth << "for" << relSrc
                                        << "(candidates:" << found.size() << ")";
            return strict;          // never guess between same-depth matches
        }
    }
    return strict;
}

// An option's source folder as installWithOptions resolves it: the declared
// subDir, else a folder matching it through the extractor's old substitution,
// else the package root (packages are not always wrapped in their subDir).
static QString resolveOptionDir(const QString& baseDir, const QString& subDir)
{
    const QString strict = QDir::cleanPath(baseDir + "/" + subDir);
    if (subDir.isEmpty() || QDir(strict).exists())
        return strict;
    const QString wanted = extractorMangledName(subDir);
    const auto entries = QDir(baseDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        if (extractorMangledName(entry).compare(wanted, Qt::CaseInsensitive) == 0) {
            qCInfo(lcPackageManager) << "Option subDir matched through extractor mangling:"
                                     << subDir << "->" << entry;
            return QDir::cleanPath(baseDir + "/" + entry);
        }
    }
    qCWarning(lcPackageManager) << "Option subDir not found, falling back to package root:"
                                << strict;
    return QDir::cleanPath(baseDir);
}

// Game files a recipe's copy steps write, resolved the way executeStep
// resolves them. The core's own listing reads the strict source path, and for
// an options package the raw package tree rather than the option steps'
// destinations, so an original those steps overwrote was never backed up.
static void collectCopyTargets(const QList<InstallStep>& steps, const QString& packageDir,
                               QStringList& out, QSet<QString>& seen)
{
    const auto add = [&](const QString& rel) {
        if (!seen.contains(rel)) { seen.insert(rel); out.append(rel); }
    };
    for (const InstallStep& st : steps) {
        if (st.action == QLatin1String("copy") || st.action == QLatin1String("copyFile")) {
            if (!st.dest.isEmpty()) add(st.dest);
        } else if (st.action == QLatin1String("copyDir")) {
            const QString srcDir = resolvePackageSource(packageDir, st.src);
            QDirIterator it(srcDir, rawfile::kFiles, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                add(st.dest + "/" + it.filePath().mid(srcDir.length() + 1));
            }
        }
    }
}

// Install methods that are genuinely a structure-preserving overlay copy:
// no installMethod / empty type, "direct" (copy package contents into the
// game dir), "overlay", and "copy". EVERY other non-empty type needs a
// dedicated handler or a recipe — overlay-copying its payload and
// reporting success is the silent "yama kuruldu ama dil değişmiyor" lie.
// Single source of truth shared by the install honesty gate and the
// update re-dispatch.
//
// "copy" was previously rejected by the honesty gate as unknown, which
// failed Alien Isolation (214490) and MGS Snake Eater (2417610) at
// install time. Their package JSON shape is:
//   "installMethod": { "type": "copy", "targetPath": "DATA",
//                      "steps": ["DATA klasörünü oyun dizinine
//                                  kopyalayın", ...] }
// The "steps" entries are human prose, not structured InstallStep
// objects — the launcher can't execute them. The real intended
// behaviour is identical to overlay/direct: drop the package payload
// onto gamePath (the package already contains the DATA/ subtree). So
// "copy" is semantically overlay-safe and goes here. If a future
// package author wants a recipe, they should use "script"/"copyDir"/
// "copyFile" with structured step objects.
//
// "file-replace" joins for the same reason. All three shipping packages of
// this type were unpacked and inspected before adding it — every one is a
// plain structure-preserving payload rooted at the game directory, with no
// wrapper folder and no step recipe:
//   Darkest Dungeon II (1940340) — "Darkest Dungeon II_Data/StreamingAssets/…"
//   DOOM 2016 (379720)           — "base/…", "Mods/…"
//   Dark Souls: Remastered (570940) — "font/…", "menu/…", "msg/…"
// The name only describes that the payload overwrites existing files, which
// is precisely what the overlay path does (and what replacedFiles records).
static const QStringList kOverlaySafeTypes = { "", "direct", "overlay", "copy", "file-replace" };

// Every reason this recipe cannot be run as written, de-duplicated. Callers
// pre-flight against this and refuse the whole recipe before mutating the game:
// a half-applied recipe (valid steps done, then one that cannot execute) leaves
// the game in the silent "kuruldu ama çalışmıyor" corruption class.
//
// An empty action is a human-text string step (documentation inside a "copy"-
// type package); it is tolerated and falls through to the overlay path,
// matching kOverlaySafeTypes above.
//
// The rules themselves live in installsteprules.h so they can be unit-tested.
static QStringList unexecutableSteps(const QList<InstallStep>& steps)
{
    QStringList out;
    for (const InstallStep& s : steps) {
        if (s.action.isEmpty()) continue;               // string-step doc noise
        QString defect;
        if (!steprules::isKnownAction(s.action))
            defect = s.action;
        else if (const QString field = steprules::missingField(
                     s.action, s.src, s.dest, s.exe, s.language, s.patches);
                 !field.isEmpty())
            defect = s.action + " (" + field + ")";
        if (!defect.isEmpty() && !out.contains(defect))
            out << defect;
    }
    return out;
}

// Executables this recipe installs FOR the user to run themselves.
static QStringList userInstalledTools(const QList<InstallStep>& steps)
{
    QStringList out;
    for (const InstallStep& s : steps) {
        if (s.action == QLatin1String("copyToDesktop") && !s.src.isEmpty())
            out << s.src;
    }
    return out;
}

// Remove the "run" steps that launch one of those tools, returning their names.
//
// See steprules::runStepIsUserLaunchedTool for why: a recipe that hands the
// user an executable has already said who runs it, and running it ourselves
// unattended cannot work when the tool is interactive. Dropping the step is
// not the same as hiding a failure — the tool is still installed where the
// recipe put it. The caller only logs the names; no recipe in the catalogue
// has this shape any more (Elden Ring now uses patchBytes).
static QStringList dropUserLaunchedTools(QList<InstallStep>& steps)
{
    const QStringList tools = userInstalledTools(steps);
    if (tools.isEmpty()) return {};

    QStringList dropped;
    for (int i = steps.size() - 1; i >= 0; --i) {
        if (steps[i].action != QLatin1String("run")) continue;
        if (!steprules::runStepIsUserLaunchedTool(steps[i].exe, tools)) continue;
        dropped << steps[i].exe;
        steps.removeAt(i);
    }
    return dropped;
}

// Post-install verification: which of the files we just wrote are no longer on
// disk?
//
// A successful QFile::copy only means the write was accepted. Real-time
// antivirus scans the result immediately afterwards and routinely quarantines
// exactly what game patches ship — dinput8.dll proxies, *.asi plugins,
// ScriptHook*.dll — so the copy succeeds, the file vanishes milliseconds later,
// and the install still reports success. The user then sees the game fail to
// start with no explanation, which is indistinguishable from a broken patch.
//
// Checking existence right after the copy turns that silent loss into a
// specific, actionable message. Entries beginning with '_' are pseudo-paths
// (_desktop:, _font:, _rename:, _steamlang:) that do not live under gamePath.
static QStringList missingAfterInstall(const QString& gamePath, const QStringList& relPaths)
{
    QStringList missing;
    for (const QString& rel : relPaths) {
        if (rel.startsWith(QLatin1Char('_')))
            continue;
        if (!rawfile::exists(QDir::cleanPath(gamePath + QLatin1Char('/') + rel)))
            missing.append(rel);
    }
    return missing;
}

// Shared message for the case above. Kept in one place so the install and
// update paths cannot drift apart in what they tell the user.
static QString quarantineGuidance(int missingCount, int totalCount)
{
    return LocalPackageManager::tr(
        "Kurulum tamamlandı ancak yazılan %1 dosyadan %2 tanesi oyun klasöründe "
        "bulunamadı. En olası neden: bir antivirüs yazılımı (çoğunlukla Windows "
        "Defender) dosyaları kopyalandıktan hemen sonra karantinaya aldı.\n\n"
        "Çözüm:\n"
        "1) Windows Güvenliği > Virüs ve tehdit koruması > Ayarları yönet > "
        "Dışlamalar bölümüne oyun klasörünü ekleyin\n"
        "2) Karantinadaki dosyaları geri yükleyin (Koruma geçmişi)\n"
        "3) Yamayı yeniden kurun\n\n"
        "Yama dosyaları oyunun çalışması için gereklidir; eksik hâlde oyun "
        "açılmayabilir.").arg(totalCount).arg(missingCount);
}

LocalPackageManager::OverlayResult LocalPackageManager::copyOverlayFiles(
    const QList<QPair<QString, QString>>& filesToCopy,
    const QString& gamePath,
    const QString& progressPrefix,
    const std::function<bool(const QString&, bool)>& fileClassifier)
{
    OverlayResult result;
    const int total = filesToCopy.size();
    int lastReported = 0;

    // Files the normal (asInvoker) path may not write — typically a game under
    // C:\Program Files. Instead of aborting the whole install, they are queued
    // and replayed once through the elevated helper, so the user sees at most
    // one UAC prompt and only when it is genuinely needed.
    struct Deferred { QString relPath; bool destExisted; };
    QList<Deferred> deferred;
    QList<ElevatedOps::Op> elevatedOps;


    for (const auto& [srcPath, relPath] : filesToCopy) {
        // Cancellation check
        if (isCancelled()) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulum iptal edildi"));
            // Signal early exit via sentinel: errors = -1
            result.errors = -1;
            return result;
        }

        QString destPath = QDir::cleanPath(gamePath + "/" + relPath);

        // Prevent path traversal: ensure destination stays within game directory.
        //
        // security::isPathContained and not a startsWith pair: a bare prefix
        // test has no directory boundary, so "<game>Evil/x" passes a check
        // against "<game>". It also resolves junctions, which Steam libraries
        // really use, and compares case-insensitively on Windows.
        if (!security::isPathContained(gamePath, destPath)) {
            qCWarning(lcPackageManager) << "Path traversal blocked:" << relPath;
            result.errors++;
            continue;
        }

        bool destExists = rawfile::exists(destPath);

        // Ensure destination directory exists. A failure here is usually the
        // same permission wall as the copy itself, so defer rather than count
        // it as an error — the helper creates parent directories for us.
        QFileInfo destInfo(destPath);
        if (!QDir().mkpath(destInfo.absolutePath())) {
            qCWarning(lcPackageManager) << "Failed to create directory:" << destInfo.absolutePath();
            if (ElevatedOps::available()) {
                deferred.append({relPath, destExists});
                elevatedOps.append({ElevatedOps::Kind::Copy, srcPath, relPath});
            } else {
                result.errors++;
            }
            continue;
        }

        auto [copyOk, copyErr] = tryCopyFile(srcPath, destPath);
        if (copyOk) {
            result.copied++;
            result.installedFiles.append(relPath);
            if (fileClassifier(relPath, destExists))
                result.replacedFiles.append(relPath);
            else
                result.addedFiles.append(relPath);
            if (m_journal) m_journal->recordFileModified(relPath);
        } else if (copyErr == CopyError::DiskFull) {
            if (m_journal) m_journal->commitOperation();
            emit installCompleted(false, tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun."));
            result.errors = -1;
            return result;
        } else if (copyErr == CopyError::PermissionDenied) {
            // Defer instead of aborting: the launcher runs asInvoker and cannot
            // itself be elevated in the Store build, so this is exactly the
            // case makine-elevate exists for. Only if the helper is missing do
            // we fall back to failing the install outright.
            if (ElevatedOps::available()) {
                deferred.append({relPath, destExists});
                elevatedOps.append({ElevatedOps::Kind::Copy, srcPath, relPath});
            } else {
                if (m_journal) m_journal->commitOperation();
                emit installCompleted(false, tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                    "1) Oyunu ve Steam'i tamamen kapatın\n"
                    "2) Antivirüs / Windows Defender'da oyun klasörünü "
                    "istisnaya ekleyin\n"
                    "3) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                    "Sonra yamayı tekrar kurun."));
                result.errors = -1;
                return result;
            }
        } else if (copyErr == CopyError::Quarantined) {
            if (m_journal) m_journal->commitOperation();
            emit installCompleted(false, quarantinedMessage(relPath));
            result.errors = -1;
            return result;
        } else if (copyErr == CopyError::FileLocked) {
            QThread::msleep(150);
            auto [ok2, err2] = tryCopyFile(srcPath, destPath);
            if (ok2) {
                result.copied++;
                result.installedFiles.append(relPath);
                if (fileClassifier(relPath, destExists))
                    result.replacedFiles.append(relPath);
                else
                    result.addedFiles.append(relPath);
                if (m_journal) m_journal->recordFileModified(relPath);
            } else if (err2 == CopyError::DiskFull || err2 == CopyError::PermissionDenied) {
                if (m_journal) m_journal->commitOperation();
                emit installCompleted(false, err2 == CopyError::DiskFull
                    ? tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun.")
                    : tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) İzin penceresi açılırsa 'Evet' deyin\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
                result.errors = -1;
                return result;
            } else {
                qCWarning(lcPackageManager) << "Failed to copy (locked):" << srcPath << "->" << destPath;
                result.errors++;
            }
        } else {
            qCWarning(lcPackageManager) << "Failed to copy:" << srcPath << "->" << destPath;
            result.errors++;
        }

        // Throttle progress signals: every 10 files or at completion
        int done = result.copied + result.errors;
        if (done - lastReported >= 10 || done == total) {
            lastReported = done;
            double progress = static_cast<double>(done) / total;
            emit installProgress(progress,
                tr("%1 %2/%3: %4").arg(progressPrefix).arg(result.copied).arg(total)
                    .arg(QFileInfo(relPath).fileName()));
        }
    }

    // Replay everything the unprivileged pass could not write, in one elevated
    // batch. One UAC prompt for the whole install rather than per file.
    if (!elevatedOps.isEmpty()) {
        emit installProgress(0.98, tr("Yönetici izni bekleniyor (%1 dosya)...")
                                       .arg(elevatedOps.size()));
        QString elevErr;
        QList<int> failedIdx;
        const bool ok = ElevatedOps::run(gamePath, elevatedOps, &elevErr, &failedIdx);
        const QSet<int> failed(failedIdx.begin(), failedIdx.end());

        for (int i = 0; i < deferred.size(); ++i) {
            if (!ok && (failed.contains(i) || failed.isEmpty())) {
                result.errors++;
                continue;
            }
            const Deferred& d = deferred.at(i);
            result.copied++;
            result.installedFiles.append(d.relPath);
            if (fileClassifier(d.relPath, d.destExisted))
                result.replacedFiles.append(d.relPath);
            else
                result.addedFiles.append(d.relPath);
            if (m_journal) m_journal->recordFileModified(d.relPath);
        }

        if (!ok) {
            qCWarning(lcPackageManager)
                << "Elevated copy pass incomplete:" << elevErr
                << "(" << elevatedOps.size() << "queued )";
            if (ElevatedOps::lastRunDeclined()) {
                if (m_journal) m_journal->commitOperation();
                emit installCompleted(false, tr(
                    "Bu oyun yönetici izni gereken bir klasörde kurulu "
                    "(%1).\n\nYama dosyalarını yazabilmek için izin penceresinde "
                    "\"Evet\" demeniz gerekiyor. Tekrar deneyip onaylayın.")
                        .arg(gamePath));
                result.errors = -1;
                return result;
            }
        } else {
            qCInfo(lcPackageManager) << "Elevated copy pass wrote"
                                     << elevatedOps.size() << "files";
        }
    }

    return result;
}

void LocalPackageManager::saveInstallState(const QString& steamAppId, const QString& gamePath,
                                            const PackageInfo& pkg,
                                            const QStringList& installedFiles,
                                            const QStringList& addedFiles,
                                            const QStringList& replacedFiles)
{
    QMetaObject::invokeMethod(this, [this, steamAppId, gamePath, pkg,
                                      installedFiles, addedFiles, replacedFiles]() {
        packages::InstalledPackageState state;
        state.version = pkg.version.toStdString();
        state.gamePath = gamePath.toStdString();
        for (const QString& f : installedFiles)
            state.installedFiles.push_back(f.toStdString());
        for (const QString& f : addedFiles)
            state.addedFiles.push_back(f.toStdString());
        for (const QString& f : replacedFiles)
            state.replacedFiles.push_back(f.toStdString());
        state.installedAt = QDateTime::currentSecsSinceEpoch();
        m_catalog.markInstalled(steamAppId.toStdString(), state);
        saveCatalogInstalledState(m_catalog, installedStatePath());
        if (m_journal) m_journal->commitOperation();
    }, Qt::QueuedConnection);
}

// -- Install package ----------------------------------------------------------

void LocalPackageManager::cancelInstall()
{
    m_cancelRequested.store(true, std::memory_order_relaxed);
}

void LocalPackageManager::installPackage(const QString& steamAppId, const QString& gamePath,
                                         const QString& variant,
                                         const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("LPM::installPackage");
    INTEGRITY_GATE();
    CrashReporter::addBreadcrumb("package",
        QStringLiteral("installPackage: %1").arg(steamAppId).toUtf8().constData());
    m_cancelRequested.store(false, std::memory_order_relaxed);

    if (gamePath.trimmed().isEmpty()) {
        emit installCompleted(false, tr("Oyun klasörü belirtilmedi"));
        return;
    }

    // Retrieve package info (works in both build modes)
    auto maybePkg = getPackage(steamAppId);
    if (!maybePkg) {
        emit installCompleted(false, tr("Yama paketi bulunamadı. İndirme tamamlanmamış olabilir. "
                "Çözüm: yamayı yeniden indirin, disk alanını ve internet "
                "bağlantınızı kontrol edin."));
        return;
    }

    const PackageInfo pkg = *maybePkg;

    // Disk space check before install
    {
        QStorageInfo storage(gamePath);
        if (storage.isValid() && storage.isReady()) {
            qint64 requiredBytes = pkg.sizeBytes > 0 ? pkg.sizeBytes * 2 : 500LL * 1024 * 1024;
            qint64 availableBytes = storage.bytesAvailable();
            if (availableBytes < requiredBytes) {
                qint64 requiredMB = requiredBytes / (1024 * 1024);
                qint64 availableMB = availableBytes / (1024 * 1024);
                emit installCompleted(false,
                    tr("Yetersiz disk alanı: %1 MB gerekli, %2 MB mevcut")
                        .arg(requiredMB).arg(availableMB));
                return;
            }
        }
    }

    // Write permission pre-check
    {
        const QString testPath = gamePath + QStringLiteral("/.makine_write_test");
        QFile testFile(testPath);
        if (testFile.open(QIODevice::WriteOnly)) {
            testFile.close();
            testFile.remove();
        } else if (!ElevatedOps::available()) {
            emit installCompleted(false,
                tr("Bu klasöre yazma izni yok. Çözüm: oyunu ve Steam'i "
                   "tamamen kapatın, sonra tekrar deneyin; izin penceresi "
                   "açılırsa 'Evet' deyin."));
            return;
        } else {
            // Not writable directly is not the end of the install:
            // copyOverlayFiles defers exactly this case to makine-elevate,
            // which asks for consent once and writes with it. Refusing here
            // meant that path could never run for a game under Program Files —
            // 51 events across 13 users, every one of them advised to "run as
            // administrator", which an MSIX package has no way to do.
            qCInfo(lcPackageManager)
                << "Game directory is not directly writable; the install will "
                   "ask for elevation:" << gamePath;
        }
    }

    // Handle variant-specific options install (e.g. GTA Trilogy: variant selects game, then options for patch/dubbing)
    if (!variant.isEmpty() && !selectedOptions.isEmpty()
        && pkg.variantInstallOptions.contains(variant))
    {
        QString variantDir = m_dataPath + "/" + pkg.dirName + "/" + variant;
        // dirHasFiles, not exists(): an interrupted download leaves the folder
        // behind empty, and a recipe run against nothing fails step by step as
        // "1 adımda hata oluştu" — the launcher blaming the game for a payload
        // that never arrived. Same reason resolveSourcePath stopped trusting
        // exists() in 1df0f32; these two option paths were missed.
        if (!dirHasFiles(variantDir)) {
            emit installCompleted(false, tr("Çeviri dosyaları bulunamadı: %1. İndirme eksik veya bozuk "
                "olabilir. Çözüm: internet bağlantınızı kontrol edin, yamayı "
                "kaldırıp yeniden indirin.").arg(pkg.gameName));
            return;
        }
        // Build a temporary PackageInfo with variant-specific options
        PackageInfo variantPkg = pkg;
        const auto& vc = pkg.variantInstallOptions[variant];
        variantPkg.installOptions = vc.installOptions;
        variantPkg.combinedSteps = vc.combinedSteps;

        (void)QtConcurrent::run([this, steamAppId, gamePath, variantDir, variantPkg, selectedOptions]() {
            installWithOptions(variantPkg, gamePath, variantDir, selectedOptions);
        });
        return;
    }

    // Handle options-based install (package-level options, e.g. Elden Ring)
    if (pkg.installMethodType == "options" && !selectedOptions.isEmpty()) {
        QString baseDir = m_dataPath + "/" + pkg.dirName;
        if (!dirHasFiles(baseDir)) {
            emit installCompleted(false, tr("Çeviri dosyaları bulunamadı: %1. İndirme eksik veya bozuk "
                "olabilir. Çözüm: internet bağlantınızı kontrol edin, yamayı "
                "kaldırıp yeniden indirin.").arg(pkg.gameName));
            return;
        }
        (void)QtConcurrent::run([this, steamAppId, gamePath, baseDir, pkg, selectedOptions]() {
            installWithOptions(pkg, gamePath, baseDir, selectedOptions);
        });
        return;
    }

    const QString sourcePath = resolveSourcePath(pkg, variant);

    if (sourcePath.isEmpty()) {
        // Two unrelated failures used to share one message. A package folder
        // that exists and holds no files is not an interrupted download:
        // fetching it again produces the same nothing, and telling the user to
        // check their connection sends them after a problem that is ours.
        const QString pkgDir = m_dataPath + QStringLiteral("/") + pkg.dirName;
        if (!pkg.dirName.isEmpty() && QDir(pkgDir).exists() && !dirHasFiles(pkgDir)) {
            emit installCompleted(false,
                tr("%1 yama paketi boş çıktı. Bu bizim tarafımızdaki bir paketleme "
                   "hatası — tekrar indirmek çözmez. Sorun bize bildirildi.")
                    .arg(pkg.gameName));
            return;
        }
        emit installCompleted(false, tr("Çeviri dosyaları bulunamadı: %1. İndirme eksik veya bozuk "
                "olabilir. Çözüm: internet bağlantınızı kontrol edin, yamayı "
                "kaldırıp yeniden indirin.").arg(pkg.gameName));
        return;
    }

    // userPath IS supported — but only with a target to copy to. Without one the
    // recipe fell through to the no-handler gate below, which told the user
    // "kurulum yöntemi: userPath desteklenmiyor". That is not true, and it sent
    // four users looking for a launcher limitation instead of a catalog entry
    // missing its "target" field.
    if (pkg.installMethodType == "userPath" && pkg.installMethodTarget.isEmpty()) {
        qCWarning(lcPackageManager) << "userPath package has no target:" << pkg.gameName;
        CrashReporter::reportFailure("package", steamAppId,
            QStringLiteral("userPath recipe carries no target path"));
        emit installCompleted(false,
            tr("%1 için kurulum hedefi katalogda eksik. Bu bizim tarafımızdaki bir "
               "veri hatası — sorun bize bildirildi.").arg(pkg.gameName));
        return;
    }

    // Handle userPath install type: copy to user-relative path instead of game dir
    if (pkg.installMethodType == "userPath" && !pkg.installMethodTarget.isEmpty()) {
        // "Documents/…" means the Documents folder itself, which OneDrive's
        // folder backup moves away from <home>/Documents — the game reads
        // from where it really is (The Sims 4, Stellaris).
        QString base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QString rel = pkg.installMethodTarget;
        if (rel.startsWith(QLatin1String("Documents/"), Qt::CaseInsensitive)) {
            base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            rel = rel.mid(10);
        }
        const QString targetPath = QDir::cleanPath(base + "/" + rel);

        (void)QtConcurrent::run([this, steamAppId, targetPath, sourcePath, pkg]() {
            emit installProgress(0.0, tr("Dosyalar hazırlanıyor..."));

            QDir().mkpath(targetPath);

            // Begin crash recovery journal so a mid-install crash can replay/undo.
            if (m_journal) {
                JournalEntry je;
                je.type = OpType::Install;
                je.gameId = steamAppId;
                je.gamePath = targetPath;
                m_journal->beginOperation(je);
            }

            QList<QPair<QString, QString>> filesToCopy;
            QDirIterator it(sourcePath, rawfile::kFiles, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString relPath = it.filePath().mid(sourcePath.length() + 1);
                // Skip internal dirs (e.g. _fonts)
                if (relPath.startsWith("_")) continue;
                filesToCopy.append({it.filePath(), relPath});
            }

            // Root fix: same wrapper generalization as the overlay path —
            // a mis-packaged top folder would otherwise drop the files at
            // target/<wrapper>/ instead of the real userPath target, the
            // silent "kuruldu ama dil değişmiyor" bug. targetPath is freshly
            // created/empty so any single top dir is packaging noise here.
            stripWrapperPrefix(filesToCopy, targetPath);
            if (filesToCopy.isEmpty()) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulacak dosya bulunamadı. Yama paketi boş veya bozuk "
                "olabilir. Çözüm: yamayı kaldırıp yeniden indirin, internet "
                "bağlantınızı kontrol edin."));
                return;
            }

            int total = filesToCopy.size();
            int copied = 0;
            int errors = 0;
            QStringList installedFiles;

            for (const auto& [srcPath, relPath] : filesToCopy) {
                if (isCancelled()) {
                    if (m_journal) m_journal->abortOperation();
                    emit installCompleted(false, tr("Kurulum iptal edildi"));
                    return;
                }

                QString destPath = QDir::cleanPath(targetPath + "/" + relPath);

                QFileInfo destInfo(destPath);
                if (!QDir().mkpath(destInfo.absolutePath())) {
                    errors++; continue;
                }
                auto [copyOk, copyErr] = tryCopyFile(srcPath, destPath);
                if (copyOk) {
                    copied++;
                    installedFiles.append(relPath);
                    if (m_journal) m_journal->recordFileModified(relPath);
                } else if (copyErr == CopyError::DiskFull) {
                    if (m_journal) m_journal->commitOperation();
                    emit installCompleted(false, tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun."));
                    return;
                } else if (copyErr == CopyError::PermissionDenied) {
                    if (m_journal) m_journal->commitOperation();
                    emit installCompleted(false, tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) İzin penceresi açılırsa 'Evet' deyin\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
                    return;
                } else if (copyErr == CopyError::Quarantined) {
                    if (m_journal) m_journal->commitOperation();
                    emit installCompleted(false, quarantinedMessage(relPath));
                    return;
                } else if (copyErr == CopyError::FileLocked) {
                    QThread::msleep(150);
                    auto [ok2, err2] = tryCopyFile(srcPath, destPath);
                    if (ok2) {
                        copied++;
                        installedFiles.append(relPath);
                        if (m_journal) m_journal->recordFileModified(relPath);
                    } else if (err2 == CopyError::DiskFull || err2 == CopyError::PermissionDenied) {
                        if (m_journal) m_journal->commitOperation();
                        emit installCompleted(false, err2 == CopyError::DiskFull
                            ? tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun.")
                            : tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) İzin penceresi açılırsa 'Evet' deyin\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
                        return;
                    } else { errors++; }
                } else {
                    errors++;
                }

                if ((copied + errors) % 10 == 0 || (copied + errors) == total) {
                    emit installProgress(static_cast<double>(copied + errors) / total,
                        tr("Kopyalanıyor %1/%2: %3").arg(copied).arg(total).arg(QFileInfo(relPath).fileName()));
                }
            }

            if (errors == 0) {
                QMetaObject::invokeMethod(this, [this, steamAppId, targetPath, pkg, installedFiles]() {
                    packages::InstalledPackageState state;
                    state.version = pkg.version.toStdString();
                    state.gamePath = targetPath.toStdString();
                    for (const QString& f : installedFiles)
                        state.installedFiles.push_back(f.toStdString());
                    state.installedAt = QDateTime::currentSecsSinceEpoch();
                    m_catalog.markInstalled(steamAppId.toStdString(), state);
                    saveCatalogInstalledState(m_catalog, installedStatePath());
                    if (m_journal) m_journal->commitOperation();
                }, Qt::QueuedConnection);
                emit installCompleted(true, tr("%1 dosya başarıyla kuruldu").arg(copied));
            } else {
                if (m_journal) m_journal->commitOperation();
                emit installCompleted(false, tr("%1/%2 dosya kopyalanamadı. Olası neden: oyun ya da "
                "başlatıcısı açık veya antivirüs dosyaları tutuyor. Çözüm: "
                "oyunu ve başlatıcısını (Steam, EA, Ubisoft…) kapatın, "
                "antivirüste oyun klasörünü izinli yapın, sonra tekrar deneyin.").arg(errors).arg(total));
            }
        });
        return;
    }

    // Unity bundle patching is handled by the Makine engine (not launcher).
    // The .makine package has already been downloaded and extracted into
    // sourcePath — point the user there so they can run the bundled
    // installer/instructions themselves, instead of bouncing them off to a
    // third-party site.
    if (pkg.installMethodType == "unityPatch") {
        emit installCompleted(false, tr("Bu yama Unity bundle yaması "
            "gerektiriyor; Makine Launcher bunu henüz otomatik kuramıyor. "
            "Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
            "Klasörün içindeki kurulum aracını (UnityEX / UABEA) "
            "çalıştırın ve talimatları izleyin.").arg(shownPath(sourcePath)));
        return;
    }

    // Forge-archive injection (AC Odyssey/Valhalla …) needs a QuickBMS/c1
    // forge toolchain the launcher does not bundle. Fail loudly instead of
    // silently overlay-copying the tool files and reporting success — that
    // produced the "yama kuruldu ama oyun İngilizce" false-success bug.
    // The package itself was downloaded into sourcePath; surface that path
    // so the user can run the included tool, rather than being redirected
    // off-launcher.
    //
    // A forge_inject package WITH steps runs them: forgeReimport/xdelta
    // steps, the legacy installer's QuickBMS/xdelta3 commands done
    // in-process. The type stays forge_inject on purpose — launchers older
    // than those steps keep refusing here.
    if (pkg.installMethodType == "forge_inject" && pkg.installSteps.isEmpty()) {
        emit installCompleted(false, tr("Bu yama, oyunun .forge "
            "arşivlerine enjeksiyon gerektirdiği için otomatik kurulamıyor. "
            "Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
            "Klasörün içindeki kurulum aracını çalıştırın ve istendiğinde "
            "oyun klasörünüzü gösterin.").arg(shownPath(sourcePath)));
        return;
    }

    // Check for custom install recipe
    if (!pkg.installSteps.isEmpty()) {
        (void)QtConcurrent::run([this, steamAppId, gamePath, sourcePath, pkg]() {
            executeInstallSteps(pkg, gamePath, sourcePath);
        });
        return;
    }

    // Honesty gate. Every method that genuinely installs by a structure-
    // preserving copy has an EMPTY type (no installMethod / "direct") or
    // "overlay". Any OTHER non-empty type that reached here has no recipe
    // steps and no dedicated handler — "external" (hosted elsewhere),
    // "installer" (runs an .exe), "workshop" (Steam subscription),
    // "paradox-mod"/"d2r_mod" (mod-manager path), or a future unknown.
    // Overlay-copying their payload and reporting success is exactly the
    // silent "yama kuruldu ama dil değişmiyor" lie. Fail loud with
    // guidance so the user uses the real source instead of being deceived.
    //
    // One exception, and it is not a loophole: a COPY-shaped type carrying no
    // recipe at all. The catalogue API answers with {"type":"script","steps":[]}
    // wherever the published CDN entry has no installMethod, which is how 67 of
    // 237 packages — Watch Dogs, Thief, DOOM, Skyrim SE, Control — ended up
    // refused for naming a method that only ever meant "copy the files".
    // steprules::emptyRecipeIsPlainOverlay draws the line at process-vs-copy.
    {
        const bool noRecipeAtAll = pkg.installSteps.isEmpty()
                                && pkg.installOptions.isEmpty()
                                && pkg.combinedSteps.isEmpty();
        // …unless the archive ships one tree PER VARIANT. api/v2/games/<id>
        // drops the catalogue's variantType/variants for all 15 variant
        // packages, so pkg.variants is empty, nobody asked the user to choose,
        // and a wholesale overlay would drop <game>/v0.88.0/BepInEx/… into the
        // game root — a patch that reports success and loads nothing. Read the
        // shape off the extracted archive instead of asking.
        const QStringList rootEntries =
            QDir(sourcePath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        const bool variantFoldered = steprules::rootLooksVariantFoldered(rootEntries);

        const bool copyShapedButEmpty =
            noRecipeAtAll && steprules::emptyRecipeIsPlainOverlay(pkg.installMethodType)
            && !variantFoldered;

        if (variantFoldered && noRecipeAtAll) {
            qCWarning(lcPackageManager)
                << "Package root holds variant folders" << rootEntries
                << "but no variant was selected — refusing blind overlay for" << pkg.gameName;
            emit installCompleted(false, tr(
                "Bu yama birden fazla oyun sürümü için ayrı dosyalar içeriyor "
                "(%1) ve hangisinin kurulacağı belirlenemedi.\n"
                "Uygulamayı güncelleyip tekrar deneyin; sorun sürerse hangi oyun "
                "sürümünü kullandığınızı bize bildirin.")
                .arg(rootEntries.join(QStringLiteral(", "))));
            return;
        }

        if (!kOverlaySafeTypes.contains(pkg.installMethodType) && !copyShapedButEmpty) {
            qCWarning(lcPackageManager)
                << "No handler for install method" << pkg.installMethodType
                << "— refusing silent overlay for" << pkg.gameName;
            const QString& m = pkg.installMethodType;
            QString guide;
            if (m == "external")
                guide = tr("Bu yamanın çevirisi harici bir kurulum aracıyla "
                    "uygulanır, Makine Launcher tarafından otomatik "
                    "kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
                    "Klasördeki kurulum talimatını izleyin.").arg(shownPath(sourcePath));
            else if (m == "installer")
                // The download is an encrypted .makine file the user cannot
                // open; the extracted folder is the only "archive" there is.
                guide = tr("Bu yama kendi kurulum aracıyla gelir ve otomatik "
                    "kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
                    "İçindeki kurulum (.exe) dosyasını çalıştırın ve istendiğinde "
                    "oyun klasörünü seçin.").arg(shownPath(sourcePath));
            else if (m == "workshop")
                guide = tr("Bu çeviri Steam Workshop üzerinden dağıtılıyor. "
                    "Çözüm: Steam'de oyunun Workshop sayfasını açın, Türkçe "
                    "yama öğesine abone olun; Steam otomatik indirip "
                    "etkinleştirir.");
            else if (m == "paradox-mod")
                // Without the extracted path the user has nowhere to copy from:
                // the download is an encrypted .makine blob they cannot find or
                // open. Every other guide surfaces sourcePath; this one must too.
                guide = tr("Bu çeviri bir Paradox modudur, oyun klasörüne "
                    "kurulmaz. Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
                    "Bu klasörün içindekileri Belgeler/Paradox Interactive/"
                    "<oyun>/mod klasörüne kopyalayın, ardından oyun "
                    "başlatıcısında mod listesinden etkinleştirin.").arg(shownPath(sourcePath));
            else
                guide = tr("Bu yama otomatik kurulamıyor (kurulum yöntemi: "
                    "%1). Yama dosyaları şu klasöre çıkarıldı:\n%2\n"
                    "Klasördeki kurulum talimatını izleyin.")
                    .arg(m, shownPath(sourcePath));
            // The extracted folder is the only way forward, and retrying cannot
            // change the answer — Battlefield 6 alone was retried 128 times by
            // 16 users (external_tool). Open the folder for them.
            if (m == "external" || m == "installer" || m == "paradox-mod") {
                const QUrl folder = QUrl::fromLocalFile(sourcePath);
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                    [folder]() { QDesktopServices::openUrl(folder); }, Qt::QueuedConnection);
            }
            emit installCompleted(false, guide);
            return;
        }
    }

    // Default overlay: copy all files preserving directory structure
    (void)QtConcurrent::run([this, steamAppId, gamePath, sourcePath, pkg]() {
        emit installProgress(0.0, tr("Dosyalar hazırlanıyor..."));

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
        QDirIterator it(sourcePath, rawfile::kFiles, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString relPath = it.filePath().mid(sourcePath.length() + 1);
            filesToCopy.append({it.filePath(), relPath});
        }

        // Root fix: drop a mis-packaged "Türkçe Yama/"-style wrapper folder
        stripWrapperPrefix(filesToCopy, gamePath);

        if (filesToCopy.isEmpty()) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulacak dosya bulunamadı. Yama paketi boş veya bozuk "
                "olabilir. Çözüm: yamayı kaldırıp yeniden indirin, internet "
                "bağlantınızı kontrol edin."));
            return;
        }

        // Install: a file is "replaced" if it already existed at the destination
        auto classifier = [](const QString& /*relPath*/, bool destExists) -> bool {
            return destExists;
        };

        OverlayResult result = copyOverlayFiles(filesToCopy, gamePath, tr("Kopyalanıyor"), classifier);

        // errors == -1 means copyOverlayFiles already emitted installCompleted
        if (result.errors < 0) return;

        if (result.errors == 0) {
            const QStringList missing = missingAfterInstall(gamePath, result.installedFiles);
            if (!missing.isEmpty()) {
                qCCritical(lcPackageManager)
                    << "post-install verification failed for" << steamAppId << "-"
                    << missing.size() << "of" << result.installedFiles.size()
                    << "files are gone from disk (antivirus quarantine?)";
                // Record what we did write so a later uninstall can still clean
                // up, then report the loss instead of a false success.
                saveInstallState(steamAppId, gamePath, pkg,
                                 result.installedFiles, result.addedFiles, result.replacedFiles);
                emit installCompleted(false,
                    quarantineGuidance(missing.size(), result.installedFiles.size()));
                return;
            }
            saveInstallState(steamAppId, gamePath, pkg,
                             result.installedFiles, result.addedFiles, result.replacedFiles);
            emit installCompleted(true,
                tr("%1 dosya başarıyla kuruldu").arg(result.copied));
        } else {
            if (m_journal) m_journal->commitOperation();
            emit installCompleted(false,
                tr("%1/%2 dosya kopyalanamadı. Olası neden: oyun ya da "
                "başlatıcısı açık veya antivirüs dosyaları tutuyor. Çözüm: "
                "oyunu ve başlatıcısını (Steam, EA, Ubisoft…) kapatın, "
                "antivirüste oyun klasörünü izinli yapın, sonra tekrar deneyin.").arg(result.errors).arg(filesToCopy.size()));
        }
    });
}

// =============================================================================
// UPDATE PACKAGE (in-place, no backup)
// =============================================================================

void LocalPackageManager::updatePackage(const QString& steamAppId, const QString& gamePath,
                                        const QString& variant,
                                        const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("LPM::updatePackage");
    INTEGRITY_GATE();
    CrashReporter::addBreadcrumb("package",
        QStringLiteral("updatePackage: %1").arg(steamAppId).toUtf8().constData());
    m_cancelRequested.store(false, std::memory_order_relaxed);

    auto maybePkg = getPackage(steamAppId);
    if (!maybePkg) {
        emit installCompleted(false, tr("Yama paketi bulunamadı. İndirme tamamlanmamış olabilir. "
                "Çözüm: yamayı yeniden indirin, disk alanını ve internet "
                "bağlantınızı kontrol edin."));
        return;
    }

    const PackageInfo pkg = *maybePkg;

    // Must already be installed to update
    auto oldState = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!oldState) {
        emit installCompleted(false, tr("Yama kurulu değil, güncelleme "
            "yapılamaz. Çözüm: önce 'Kur' ile yamayı kurun, sonra "
            "güncelleyebilirsiniz."));
        return;
    }

    // Re-dispatch anything that is not a plain overlay (recipe, options,
    // userPath, unityPatch, forge_inject, external, installer, …) through
    // installPackage — the single authority that applies it correctly or
    // fails loud. Without this, a non-overlay method silently fell through
    // to "Default overlay update" below: the same "kuruldu/güncellendi ama
    // dil değişmiyor" lie, on the update path. Stale-file cleanup is a
    // harmless no-op for methods that never added files under gamePath.
    const bool overlaySafeUpdate = pkg.installSteps.isEmpty()
                                 && kOverlaySafeTypes.contains(pkg.installMethodType);
    if (!overlaySafeUpdate) {
        // Collect old addedFiles for cleanup
        QStringList oldAdded;
        for (const auto& f : oldState->addedFiles)
            oldAdded.append(QString::fromStdString(f));

        // Delete stale added files (files we added that may no longer be in new recipe)
        for (const QString& relPath : oldAdded) {
            if (relPath.startsWith("_")) continue; // skip meta entries like _steamlang
            QString filePath = QDir::cleanPath(gamePath + "/" + relPath);
            if (rawfile::exists(filePath))
                QFile::remove(filePath);
        }

        // Re-run install (which handles recipe/options)
        installPackage(steamAppId, gamePath, variant, selectedOptions);
        return;
    }

    // --- Default overlay update ---

    const QString sourcePath = resolveSourcePath(pkg, variant);

    if (sourcePath.isEmpty()) {
        // Two unrelated failures used to share one message. A package folder
        // that exists and holds no files is not an interrupted download:
        // fetching it again produces the same nothing, and telling the user to
        // check their connection sends them after a problem that is ours.
        const QString pkgDir = m_dataPath + QStringLiteral("/") + pkg.dirName;
        if (!pkg.dirName.isEmpty() && QDir(pkgDir).exists() && !dirHasFiles(pkgDir)) {
            emit installCompleted(false,
                tr("%1 yama paketi boş çıktı. Bu bizim tarafımızdaki bir paketleme "
                   "hatası — tekrar indirmek çözmez. Sorun bize bildirildi.")
                    .arg(pkg.gameName));
            return;
        }
        emit installCompleted(false, tr("Çeviri dosyaları bulunamadı: %1. İndirme eksik veya bozuk "
                "olabilir. Çözüm: internet bağlantınızı kontrol edin, yamayı "
                "kaldırıp yeniden indirin.").arg(pkg.gameName));
        return;
    }

    // Collect old state sets
    std::set<std::string> oldAddedSet(oldState->addedFiles.begin(), oldState->addedFiles.end());
    std::set<std::string> oldReplacedSet(oldState->replacedFiles.begin(), oldState->replacedFiles.end());

    (void)QtConcurrent::run([this, steamAppId, gamePath, sourcePath, pkg,
                              oldAddedSet, oldReplacedSet]() {
        emit installProgress(0.0, tr("Güncelleme hazırlanıyor..."));

        if (m_journal) {
            JournalEntry je;
            je.type = OpType::Install;
            je.gameId = steamAppId;
            je.gamePath = gamePath;
            m_journal->beginOperation(je);
        }

        // Scan new package files
        QList<QPair<QString, QString>> filesToCopy;
        QDirIterator it(sourcePath, rawfile::kFiles, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString relPath = it.filePath().mid(sourcePath.length() + 1);
            filesToCopy.append({it.filePath(), relPath});
        }

        // Root fix: drop a mis-packaged "Türkçe Yama/"-style wrapper folder
        stripWrapperPrefix(filesToCopy, gamePath);

        if (filesToCopy.isEmpty()) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulacak dosya bulunamadı. Yama paketi boş veya bozuk "
                "olabilir. Çözüm: yamayı kaldırıp yeniden indirin, internet "
                "bağlantınızı kontrol edin."));
            return;
        }

        // Build new file set for stale detection
        std::set<std::string> newFileSet;
        for (const auto& [src, rel] : filesToCopy)
            newFileSet.insert(rel.toStdString());

        // Delete stale files: old addedFiles that are NOT in the new package
        // Only delete files WE added — replacedFiles are backed up originals, don't touch
        int staleDeleted = 0;
        for (const auto& oldFile : oldAddedSet) {
            if (newFileSet.count(oldFile) == 0) {
                QString filePath = QDir::cleanPath(gamePath + "/" + QString::fromStdString(oldFile));
                if (rawfile::exists(filePath)) {
                    QFile::remove(filePath);
                    staleDeleted++;
                }
            }
        }
        if (staleDeleted > 0)
            qCDebug(lcPackageManager) << "updatePackage: removed" << staleDeleted << "stale added files";

        // Update: a file is "replaced" if it was already replaced in the original install
        auto classifier = [&oldReplacedSet](const QString& relPath, bool /*destExists*/) -> bool {
            return oldReplacedSet.count(relPath.toStdString()) > 0;
        };

        OverlayResult result = copyOverlayFiles(filesToCopy, gamePath, tr("Güncelleniyor"), classifier);

        // errors == -1 means copyOverlayFiles already emitted installCompleted
        if (result.errors < 0) return;

        if (result.errors == 0) {
            const QStringList missing = missingAfterInstall(gamePath, result.installedFiles);
            if (!missing.isEmpty()) {
                qCCritical(lcPackageManager)
                    << "post-update verification failed for" << steamAppId << "-"
                    << missing.size() << "of" << result.installedFiles.size()
                    << "files are gone from disk (antivirus quarantine?)";
                saveInstallState(steamAppId, gamePath, pkg,
                                 result.installedFiles, result.addedFiles, result.replacedFiles);
                emit installCompleted(false,
                    quarantineGuidance(missing.size(), result.installedFiles.size()));
                return;
            }
            saveInstallState(steamAppId, gamePath, pkg,
                             result.installedFiles, result.addedFiles, result.replacedFiles);
            emit installCompleted(true,
                tr("%1 dosya başarıyla güncellendi").arg(result.copied));
        } else {
            if (m_journal) m_journal->commitOperation();
            emit installCompleted(false,
                tr("%1/%2 dosya güncellenemedi").arg(result.errors).arg(filesToCopy.size()));
        }
    });
}

// -- Single-step executor (shared by recipe-based and options-based installs) --

LocalPackageManager::StepOutcome LocalPackageManager::executeStep(
    const InstallStep& step,
    const QString& gamePath,
    const QString& packageDir,
    double progress,
    int current, int total,
    const QString& progressPrefix,
    const QString& steamAppId,
    QStringList& installedFiles,
    QSet<QString>& replacedFiles,
    QString* failDetail)
{
    // Helper: emit fatal error, commit journal, return FatalError
    auto fatal = [&](const QString& msg) -> StepOutcome {
        if (m_journal) m_journal->commitOperation();
        emit installCompleted(false, msg);
        return StepOutcome::FatalError;
    };

    // Helper: copy one file with retry-on-lock and fatal-error promotion
    auto copyOne = [&](const QString& src, const QString& dst,
                       const QString& logLabel) -> StepOutcome {
        QFileInfo dstInfo(dst);
        if (!QDir().mkpath(dstInfo.absolutePath())) {
            qCWarning(lcPackageManager) << "Failed to create directory:" << dstInfo.absolutePath();
            if (failDetail) *failDetail = tr("klasör oluşturulamadı");
            return StepOutcome::SoftError;
        }
        auto [ok, err] = tryCopyFile(src, dst);
        if (ok) return StepOutcome::Ok;
        if (err == CopyError::Quarantined)
            return fatal(quarantinedMessage(dst));
        if (err == CopyError::DiskFull)
            return fatal(tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun."));
        if (err == CopyError::PermissionDenied)
            return fatal(tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) İzin penceresi açılırsa 'Evet' deyin\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
        if (err == CopyError::FileLocked) {
            QThread::msleep(150);
            auto [ok2, err2] = tryCopyFile(src, dst);
            if (ok2) return StepOutcome::Ok;
            if (err2 == CopyError::DiskFull)
                return fatal(tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun."));
            if (err2 == CopyError::PermissionDenied)
                return fatal(tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) İzin penceresi açılırsa 'Evet' deyin\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
            qCWarning(lcPackageManager) << logLabel << "failed (locked):" << src << "->" << dst
                                        << "winerr" << m_lastCopyWinError;
            if (failDetail) *failDetail = m_lastCopyWinError
                ? tr("dosya kilitli, Windows hata %1").arg(m_lastCopyWinError) : tr("dosya kilitli");
            return StepOutcome::SoftError;
        }
        qCWarning(lcPackageManager) << logLabel << "failed:" << src << "->" << dst
                                    << "winerr" << m_lastCopyWinError;
        if (failDetail) *failDetail = m_lastCopyWinError
            ? tr("dosya kopyalanamadı, Windows hata %1").arg(m_lastCopyWinError) : tr("dosya kopyalanamadı");
        return StepOutcome::SoftError;
    };

    if (step.action == "copy" || step.action == "copyFile") {
        QString srcPath  = resolvePackageSource(packageDir, step.src);
        QString destPath = QDir::cleanPath(gamePath   + "/" + step.dest);

        if (!security::isPathContained(gamePath, destPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in copy:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama dosya hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (!rawfile::exists(srcPath)) {
            qCWarning(lcPackageManager) << "Copy source not found:" << srcPath;
            if (failDetail) *failDetail = tr("paket dosyası bulunamadı");
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        // The game's own file, unless an earlier step of this install wrote it.
        const bool original = rawfile::exists(destPath) && !installedFiles.contains(step.dest);
        StepOutcome outcome = copyOne(srcPath, destPath, "copy");
        if (outcome == StepOutcome::Ok) {
            installedFiles.append(step.dest);
            if (original) replacedFiles.insert(step.dest);
            if (m_journal) m_journal->recordFileModified(step.dest);
        }
        return outcome;

    } else if (step.action == "copyDir") {
        QString srcDir  = resolvePackageSource(packageDir, step.src);
        QString destDir = QDir::cleanPath(gamePath   + "/" + step.dest);

        if (!security::isPathContained(gamePath, destDir)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in copyDir:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama klasör hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (!QDir(srcDir).exists()) {
            qCWarning(lcPackageManager) << "copyDir source not found:" << srcDir;
            if (failDetail) *failDetail = tr("paket klasörü bulunamadı");
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: %4/").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        int dirFiles = 0;
        int dirFailed = 0;
        QDirIterator dirIt(srcDir, rawfile::kFiles, QDirIterator::Subdirectories);
        while (dirIt.hasNext()) {
            dirIt.next();
            ++dirFiles;
            QString relPath  = dirIt.filePath().mid(srcDir.length() + 1);
            QString destPath = QDir::cleanPath(destDir + "/" + relPath);
            const QString fullRelPath = step.dest + "/" + relPath;
            const bool original = rawfile::exists(destPath) && !installedFiles.contains(fullRelPath);
            StepOutcome outcome = copyOne(dirIt.filePath(), destPath, "copyDir");
            if (outcome == StepOutcome::Ok) {
                installedFiles.append(fullRelPath);
                if (original) replacedFiles.insert(fullRelPath);
                if (m_journal) m_journal->recordFileModified(fullRelPath);
            } else if (outcome != StepOutcome::SoftError) {
                return outcome; // Fatal or Cancelled — propagate immediately
            } else {
                ++dirFailed;    // keep going: the rest of the folder still lands
            }
        }
        // A folder copied in part is a failed step. Returning Ok here counted
        // a half-copied patch as installed.
        if (dirFailed > 0) {
            if (failDetail)
                *failDetail = tr("%1/%2 dosya kopyalanamadı (%3)")
                                  .arg(dirFailed).arg(dirFiles).arg(*failDetail);
            return StepOutcome::SoftError;
        }
        return StepOutcome::Ok;

    } else if (step.action == "delete") {
        QString destPath = QDir::cleanPath(gamePath + "/" + step.dest);

        if (!security::isPathContained(gamePath, destPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in delete:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama silme hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: Siliniyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        if (rawfile::exists(destPath)) {
            if (!QFile::remove(destPath)) {
                qCWarning(lcPackageManager) << "Delete failed:" << destPath;
                return StepOutcome::SoftError;
            }
        }
        return StepOutcome::Ok;

    } else if (step.action == "installFont") {
        QString fontSrcDir = QDir::cleanPath(packageDir + "/" + step.src);

#ifdef Q_OS_WIN
        // Per-user font directory (no admin required, Windows 10 1809+)
        QString userFontsDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts";
        QDir().mkpath(userFontsDir);

        emit installProgress(progress,
            tr("%1Adım %2/%3: Fontlar yükleniyor").arg(progressPrefix).arg(current).arg(total));

        QDirIterator fontIt(fontSrcDir, {"*.ttf", "*.otf"}, QDir::Files);
        while (fontIt.hasNext()) {
            fontIt.next();
            QString fontName = fontIt.fileName();
            QString destFont = userFontsDir + "/" + fontName;
            StepOutcome outcome = copyOne(fontIt.filePath(), destFont, "installFont");
            if (outcome == StepOutcome::Ok) {
                QString fontEntry = "_font:" + fontName;
                installedFiles.append(fontEntry);
                if (m_journal) m_journal->recordFileModified(fontEntry);
                qCDebug(lcPackageManager) << "Font installed:" << fontName;
            } else if (outcome != StepOutcome::SoftError) {
                return outcome;
            } else {
                qCWarning(lcPackageManager) << "Font install failed:" << fontName;
            }
        }
#else
        qCWarning(lcPackageManager) << "installFont action only supported on Windows";
#endif
        return StepOutcome::Ok;

    } else if (step.action == "run") {
        // Resolve executable: game dir first, then package dir (wrapper-
        // tolerant), then fallback. Going through resolvePackageSource
        // for the package side means ".makine archives that nest the exe
        // under a wrapper (<game>/Türkçe Yama/ERING_TR.exe) still resolve
        // — without it the step silently soft-failed.
        QString exePath;
        QString gameExe = QDir::cleanPath(gamePath + "/" + step.exe);

        if      (QFile::exists(gameExe))                 exePath = gameExe;
        else if (QString c = resolvePackageSource(packageDir, step.exe);
                 QFile::exists(c))                       exePath = c;
        else if (!step.fallback.isEmpty()) {
            QString gameFb = QDir::cleanPath(gamePath + "/" + step.fallback);
            if      (QFile::exists(gameFb))              exePath = gameFb;
            else if (QString c = resolvePackageSource(packageDir, step.fallback);
                     QFile::exists(c))                   exePath = c;
        }
        if (exePath.isEmpty()) {
            qCWarning(lcPackageManager) << "Run: executable not found:" << step.exe;
            return StepOutcome::SoftError;
        }
        if (exePath.contains("..")) {
            qCWarning(lcPackageManager) << "Run: path traversal rejected:" << exePath;
            return StepOutcome::SoftError;
        }
        if (!security::isPathContained(gamePath, exePath)
            && !security::isPathContained(packageDir, exePath)) {
            qCWarning(lcPackageManager) << "Run: executable outside allowed directories:" << exePath;
            return StepOutcome::SoftError;
        }
        QString ext = QFileInfo(exePath).suffix().toLower();
        if (ext != "exe" && ext != "bat" && ext != "cmd") {
            qCWarning(lcPackageManager) << "Run: disallowed executable type:" << ext;
            return StepOutcome::SoftError;
        }

        emit installProgress(progress,
            tr("%1Adım %2/%3: %4").arg(progressPrefix).arg(current).arg(total).arg(QFileInfo(exePath).fileName()));

        QString workDir = (step.workDir == "package") ? packageDir : gamePath;

        // Resolve argument placeholders
        QStringList resolvedArgs;
        for (const QString& arg : step.args) {
            QString resolved = arg;
            resolved.replace("${gamePath}", gamePath);
            resolved.replace("${packageDir}", packageDir);
            resolvedArgs.append(resolved);
        }
        qCInfo(lcPackageManager) << "Run: executing" << exePath
                << "args:" << resolvedArgs << "workDir:" << workDir;

        QString exeFileName = QFileInfo(exePath).fileName();
        auto result = runProcess(exePath, resolvedArgs, workDir,
            [&](int elapsedMs) {
                int mins = elapsedMs / 60000;
                int secs = (elapsedMs % 60000) / 1000;
                emit installProgress(progress,
                    tr("%1Adım %2/%3: %4 (%5:%6)")
                        .arg(progressPrefix).arg(current).arg(total).arg(exeFileName)
                        .arg(mins, 2, 10, QChar('0'))
                        .arg(secs, 2, 10, QChar('0')));
            });

        if (result.cancelled) return StepOutcome::Cancelled;
        // Name the cause. Four different problems used to leave the same trace:
        // a binary antivirus quarantined, a UAC prompt the user dismissed, an
        // interactive tool that never returns, and a tool that ran and refused.
        // Each needs a different answer, and all four arrived as one bucket.
        if (result.elevationDeclined) {
            if (failDetail) *failDetail = QStringLiteral("yönetici izni verilmedi");
            return StepOutcome::SoftError;
        }
        if (!result.started) {
            if (failDetail) *failDetail = QStringLiteral("araç başlatılamadı");
            return StepOutcome::SoftError;
        }
        if (result.timedOut) {
            if (failDetail) *failDetail = QStringLiteral("araç 30 dakikada bitmedi");
            return StepOutcome::SoftError;
        }
        if (result.exitCode != 0) {
            qCWarning(lcPackageManager) << "Run: non-zero exit:" << result.exitCode
                       << "output:" << result.output.left(500);
            if (failDetail)
                *failDetail = QStringLiteral("araç hata kodu %1 verdi").arg(result.exitCode);
            return StepOutcome::SoftError;
        }
        qCDebug(lcPackageManager) << "Run OK:" << exePath;
        return StepOutcome::Ok;

    } else if (step.action == "copyToDesktop") {
        // Wrapper-tolerant package lookup, then game-dir fallback. Same
        // motivation as the "run" branch — copyToDesktop frequently
        // targets a file ("ERING_TR.exe") that lives inside the nested
        // wrapper directory of an extracted .makine archive.
        QString srcPath = resolvePackageSource(packageDir, step.src);
        if (!QFile::exists(srcPath))
            srcPath = QDir::cleanPath(gamePath + "/" + step.src);
        if (!rawfile::exists(srcPath)) {
            qCWarning(lcPackageManager) << "copyToDesktop source not found:" << step.src;
            return StepOutcome::SoftError;
        }
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString destPath    = QDir::cleanPath(desktopPath + "/" + step.dest);

        emit installProgress(progress,
            tr("%1Adım %2/%3: Masaüstüne kopyalanıyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        StepOutcome outcome = copyOne(srcPath, destPath, "copyToDesktop");
        if (outcome == StepOutcome::Ok) {
            installedFiles.append("_desktop:" + step.dest);
            if (m_journal) m_journal->recordFileModified("_desktop:" + step.dest);
            qCDebug(lcPackageManager) << "Copied to desktop:" << step.dest;
        }
        return outcome;

    } else if (step.action == "rename") {
        QString srcPath  = QDir::cleanPath(gamePath + "/" + step.src);
        QString destPath = QDir::cleanPath(gamePath + "/" + step.dest);

        if (!security::isPathContained(gamePath, srcPath)
            || !security::isPathContained(gamePath, destPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in rename:" << step.src << "->" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama yeniden adlandırma hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: Yeniden adlandırılıyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        if (QFile::exists(srcPath)) {
            if (QFile::exists(destPath)) QFile::remove(destPath);
            if (QFile::rename(srcPath, destPath)) {
                installedFiles.append("_rename:" + step.src + ":" + step.dest);
                if (m_journal) m_journal->recordFileModified("_rename:" + step.src + ":" + step.dest);
                qCDebug(lcPackageManager) << "Renamed:" << step.src << "->" << step.dest;
            } else {
                qCWarning(lcPackageManager) << "Rename failed:" << srcPath << "->" << destPath;
                return StepOutcome::SoftError;
            }
        } else {
            qCDebug(lcPackageManager) << "Rename source not found (skipping):" << srcPath;
        }
        return StepOutcome::Ok;

    } else if (step.action == "vpatch") {
        // Apply a VPatch binary delta to a file the game already has.
        //
        // The two packages that use this (Fahrenheit 312840, AC III 911400)
        // ship appliers the launcher cannot drive: an NSIS plugin DLL with no
        // command line, and a patch.exe Windows will not start without
        // elevation. vpatchapply.h does the work instead.
        //
        // Nothing is written over the game file until the produced bytes hash
        // to the digest the patch declares: the output goes to a sibling temp
        // file, and only a verified result is swapped in. A wrong or partial
        // result leaves the original untouched.
        const QString patchPath = resolvePackageSource(packageDir, step.src);
        const QString targetPath = QDir::cleanPath(gamePath + "/" + step.dest);

        if (!security::isPathContained(gamePath, targetPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in vpatch:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (patchPath.isEmpty() || !QFile::exists(patchPath)) {
            qCWarning(lcPackageManager) << "vpatch: patch file not found:" << step.src;
            if (failDetail) *failDetail = tr("yama dosyası okunamadı");
            return StepOutcome::SoftError;
        }
        if (!QFile::exists(targetPath)) {
            // What the folder holds instead: AC III's Ubisoft Connect copy
            // missed all four .forge targets at the root, and the event could
            // not say whether they live elsewhere or under other names.
            const QFileInfo target(targetPath);
            qCWarning(lcPackageManager) << "vpatch: target missing:" << targetPath
                << "— same-type files there:"
                << QDir(target.absolutePath())
                       .entryList({QStringLiteral("*.") + target.suffix()}, rawfile::kFiles)
                       .mid(0, 12);
            if (failDetail) *failDetail = tr("oyun dosyası bulunamadı");
            return StepOutcome::SoftError;
        }

        emit installProgress(progress,
            tr("%1Adım %2/%3: Yamalanıyor %4").arg(progressPrefix).arg(current).arg(total)
                .arg(QFileInfo(targetPath).fileName()));

        // A delta produces a file about the size of the original, and we hold
        // both until the swap. Refuse early rather than fill the user's disk.
        const qint64 targetSize = QFileInfo(targetPath).size();
        const QStorageInfo storage(QFileInfo(targetPath).absolutePath());
        if (storage.isValid() && storage.bytesAvailable() < targetSize + (64LL << 20)) {
            qCWarning(lcPackageManager) << "vpatch: not enough free space for" << targetPath
                << "need" << targetSize << "have" << storage.bytesAvailable();
            if (failDetail) *failDetail = tr("disk alanı yetersiz");
            return StepOutcome::SoftError;
        }

        const QString tempPath = targetPath + QStringLiteral(".makine-new");
        QFile::remove(tempPath);

        vpatch::Result vr = vpatch::Result::Error;
        {
            QFile patchFile(patchPath), sourceFile(targetPath), destFile(tempPath);
            if (!patchFile.open(QIODevice::ReadOnly) || !sourceFile.open(QIODevice::ReadOnly)
                || !destFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
                qCWarning(lcPackageManager) << "vpatch: cannot open files for" << step.dest;
                if (failDetail) *failDetail = tr("dosya açılamadı");
                QFile::remove(tempPath);
                return StepOutcome::SoftError;
            }
            vr = vpatch::apply(patchFile, sourceFile, destFile);
        }

        if (vr == vpatch::Result::UpToDate) {
            // Already patched — a re-install, or the user applied it by hand.
            QFile::remove(tempPath);
            qCInfo(lcPackageManager) << "vpatch: already up to date:" << step.dest;
            return StepOutcome::Ok;
        }
        if (vr != vpatch::Result::Success) {
            QFile::remove(tempPath);
            qCWarning(lcPackageManager) << "vpatch failed:" << vpatch::resultName(vr)
                                        << "target" << step.dest;
            if (failDetail) {
                *failDetail = (vr == vpatch::Result::NoMatch)
                    ? tr("oyun sürümü bu yamayla eşleşmiyor")
                    : (vr == vpatch::Result::Corrupt)
                        ? tr("yama dosyası okunamadı")
                        : tr("yamalanan dosya doğrulanamadı");
            }
            return StepOutcome::SoftError;
        }

        // Verified. Swap it in.
        const QString oldPath = targetPath + QStringLiteral(".makine-old");
        QFile::remove(oldPath);
        if (!QFile::rename(targetPath, oldPath)) {
            QFile::remove(tempPath);
            qCWarning(lcPackageManager) << "vpatch: cannot move original aside:" << targetPath;
            if (failDetail) *failDetail = tr("oyun dosyası değiştirilemedi");
            return StepOutcome::SoftError;
        }
        if (!QFile::rename(tempPath, targetPath)) {
            QFile::rename(oldPath, targetPath);   // put it back
            QFile::remove(tempPath);
            qCWarning(lcPackageManager) << "vpatch: cannot install patched file:" << targetPath;
            if (failDetail) *failDetail = tr("oyun dosyası değiştirilemedi");
            return StepOutcome::SoftError;
        }
        QFile::remove(oldPath);

        // "_vpatch:" and not the plain path: uninstall must NOT delete this
        // file. It existed before us and BackupManager::restoreBackup puts the
        // original back over it; deleting would leave the game without an
        // archive it needs. getPackageFileList lists vpatch targets so the
        // backup really holds them.
        installedFiles.append(QStringLiteral("_vpatch:") + step.dest);
        if (m_journal) m_journal->recordFileModified(QStringLiteral("_vpatch:") + step.dest);
        qCInfo(lcPackageManager) << "vpatch applied:" << step.dest;
        return StepOutcome::Ok;

    } else if (step.action == "patchBytes") {
        // Same-length find-and-replace in a game file, written to dest (or
        // back over src). Elden Ring: eldenring.exe -> start_protected_game.exe
        // with its two mount strings pointed at Game/. Rules in bytepatch.h.
        const QString destRel  = step.dest.isEmpty() ? step.src : step.dest;
        const QString srcPath  = QDir::cleanPath(gamePath + "/" + step.src);
        const QString destPath = QDir::cleanPath(gamePath + "/" + destRel);

        if (!security::isPathContained(gamePath, srcPath)
            || !security::isPathContained(gamePath, destPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in patchBytes:"
                                         << step.src << "->" << destRel;
            return fatal(tr("Güvenlik ihlali: yama hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        QList<bytepatch::Pair> pairs;
        if (!bytepatch::decodePairs(step.patches, &pairs)) {   // pre-flight refuses this too
            if (failDetail) *failDetail = tr("kurulum tarifi hatalı");
            return StepOutcome::SoftError;
        }
        QFile in(srcPath);
        if (!in.open(QIODevice::ReadOnly)) {
            qCWarning(lcPackageManager) << "patchBytes: cannot read" << srcPath;
            if (failDetail) *failDetail = tr("oyun dosyası bulunamadı");
            return StepOutcome::SoftError;
        }
        QByteArray data = in.readAll();
        in.close();

        emit installProgress(progress,
            tr("%1Adım %2/%3: Yamalanıyor %4").arg(progressPrefix).arg(current).arg(total)
                .arg(QFileInfo(destPath).fileName()));

        const bytepatch::Result br = bytepatch::apply(data, pairs);
        if (br != bytepatch::Result::Patched && br != bytepatch::Result::AlreadyPatched) {
            qCWarning(lcPackageManager) << "patchBytes failed:" << bytepatch::resultName(br)
                                        << "source" << step.src;
            if (failDetail) *failDetail = tr("oyun sürümü bu yamayla eşleşmiyor");
            return StepOutcome::SoftError;
        }

        // A file that existed before us is the game's: uninstall restores it
        // from the backup (getPackageFileList lists it) and must not delete
        // it, which is what the "_vpatch:" marker already means. A file we
        // create is ours and goes on uninstall.
        const bool existed = QFile::exists(destPath);
        if (!(br == bytepatch::Result::AlreadyPatched && destPath == srcPath)) {
            QSaveFile out(destPath);
            if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()
                || !out.commit()) {
                qCWarning(lcPackageManager) << "patchBytes: cannot write" << destPath
                                            << out.errorString();
                if (failDetail) *failDetail = tr("oyun dosyası değiştirilemedi");
                return StepOutcome::SoftError;
            }
        }
        const QString record = existed ? QStringLiteral("_vpatch:") + destRel : destRel;
        installedFiles.append(record);
        if (m_journal) m_journal->recordFileModified(record);
        qCInfo(lcPackageManager) << "patchBytes" << bytepatch::resultName(br) << step.src
                                 << "->" << destRel;
        return StepOutcome::Ok;

    } else if (step.action == "forgeReimport") {
        // Write a folder of "<index>.acv" files into a .forge archive: the
        // legacy installer's QuickBMS reimport (AC Valhalla, AC Mirage), done
        // in-process. The archive is edited in place; it was backed up first
        // (getPackageFileList lists it) and uninstall restores it from there.
        // Rules in forgereimport.h.
        const QString folder = resolvePackageSource(packageDir, step.src);
        const QString forgePath = QDir::cleanPath(gamePath + "/" + step.dest);
        if (!security::isPathContained(gamePath, forgePath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in forgeReimport:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        // An archive this install does not have belongs to DLC the user does
        // not own: nothing there to translate. The legacy installer ran the
        // tool anyway and let it fail.
        if (!QFile::exists(forgePath)) {
            qCInfo(lcPackageManager) << "forgeReimport: not in this install, skipping:" << step.dest;
            return StepOutcome::Ok;
        }
        QMap<quint32, QByteArray> files;
        if (!QFileInfo(folder).isDir() || !forgereimport::loadFolder(folder, &files)
            || files.isEmpty()) {
            qCWarning(lcPackageManager) << "forgeReimport: no reimport files in" << step.src;
            if (failDetail) *failDetail = tr("yama dosyası okunamadı");
            return StepOutcome::SoftError;
        }

        emit installProgress(progress,
            tr("%1Adım %2/%3: Yamalanıyor %4").arg(progressPrefix).arg(current).arg(total)
                .arg(QFileInfo(forgePath).fileName()));

        forgereimport::Result fr = forgereimport::Result::IoError;
        {
            QFile forge(forgePath);
            if (forge.open(QIODevice::ReadWrite)) {
                fr = forgereimport::apply(forge, files);
                if (fr == forgereimport::Result::Done && !forge.flush())
                    fr = forgereimport::Result::IoError;
            } else {
                qCWarning(lcPackageManager) << "forgeReimport: cannot open" << forgePath
                                            << forge.errorString();
            }
        }
        if (fr != forgereimport::Result::Done) {
            qCWarning(lcPackageManager) << "forgeReimport failed:" << forgereimport::resultName(fr)
                                        << "target" << step.dest;
            if (failDetail) {
                *failDetail = (fr == forgereimport::Result::IoError)
                    ? tr("oyun dosyası değiştirilemedi")
                    : tr("oyun sürümü bu yamayla eşleşmiyor");
            }
            return StepOutcome::SoftError;
        }
        installedFiles.append(QStringLiteral("_vpatch:") + step.dest);
        if (m_journal) m_journal->recordFileModified(QStringLiteral("_vpatch:") + step.dest);
        qCInfo(lcPackageManager) << "forgeReimport:" << files.size() << "files ->" << step.dest;
        return StepOutcome::Ok;

    } else if (step.action == "xdelta") {
        // Rebuild a game archive from an xdelta3 delta: the legacy installer's
        // c1.exe (xdelta3 3.0.4) for AC Odyssey, done in-process. Same
        // temp-and-swap as vpatch: the archive is replaced only by a result
        // whose every window matched the checksum the delta carries.
        const QString deltaPath = resolvePackageSource(packageDir, step.src);
        const QString targetPath = QDir::cleanPath(gamePath + "/" + step.dest);
        if (!security::isPathContained(gamePath, targetPath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in xdelta:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (!QFile::exists(targetPath)) {   // DLC not owned — see forgeReimport
            qCInfo(lcPackageManager) << "xdelta: not in this install, skipping:" << step.dest;
            return StepOutcome::Ok;
        }
        if (!QFile::exists(deltaPath)) {
            qCWarning(lcPackageManager) << "xdelta: delta not found:" << step.src;
            if (failDetail) *failDetail = tr("yama dosyası okunamadı");
            return StepOutcome::SoftError;
        }

        emit installProgress(progress,
            tr("%1Adım %2/%3: Yamalanıyor %4").arg(progressPrefix).arg(current).arg(total)
                .arg(QFileInfo(targetPath).fileName()));

        // Original and result side by side until the swap.
        const qint64 targetSize = QFileInfo(targetPath).size();
        const QStorageInfo storage(QFileInfo(targetPath).absolutePath());
        if (storage.isValid() && storage.bytesAvailable() < targetSize + (64LL << 20)) {
            qCWarning(lcPackageManager) << "xdelta: not enough free space for" << targetPath
                << "need" << targetSize << "have" << storage.bytesAvailable();
            if (failDetail) *failDetail = tr("disk alanı yetersiz");
            return StepOutcome::SoftError;
        }

        const QString tempPath = targetPath + QStringLiteral(".makine-new");
        QFile::remove(tempPath);

        xdelta::Result xr = xdelta::Result::Corrupt;
        bool cancelled = false;
        {
            QFile deltaFile(deltaPath), sourceFile(targetPath), out(tempPath);
            uchar* source = nullptr;
            if (!deltaFile.open(QIODevice::ReadOnly) || !sourceFile.open(QIODevice::ReadOnly)
                || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || (source = sourceFile.map(0, sourceFile.size())) == nullptr) {
                qCWarning(lcPackageManager) << "xdelta: cannot open files for" << step.dest;
                if (failDetail) *failDetail = tr("dosya açılamadı");
                out.close();
                QFile::remove(tempPath);
                return StepOutcome::SoftError;
            }
            const QByteArray delta = deltaFile.readAll();
            // Odyssey's main archive is 3 GB: report as it grows, and let
            // the user stop it.
            const QString name = QFileInfo(targetPath).fileName();
            qint64 written = 0, nextReport = 0;
            xr = xdelta::apply(reinterpret_cast<const std::uint8_t*>(delta.constData()),
                               std::size_t(delta.size()), source, std::size_t(sourceFile.size()),
                               [&](const std::uint8_t* data, std::size_t size) {
                                   if (isCancelled()) { cancelled = true; return false; }
                                   if (out.write(reinterpret_cast<const char*>(data), qint64(size))
                                       != qint64(size))
                                       return false;
                                   written += qint64(size);
                                   if (written >= nextReport) {
                                       nextReport = written + (256LL << 20);
                                       emit installProgress(progress,
                                           tr("%1Adım %2/%3: Yamalanıyor %4 — %5 / %6 MB")
                                               .arg(progressPrefix).arg(current).arg(total)
                                               .arg(name).arg(written >> 20).arg(targetSize >> 20));
                                   }
                                   return true;
                               });
            sourceFile.unmap(source);
            if (xr == xdelta::Result::Success && !out.flush()) xr = xdelta::Result::WriteError;
        }

        if (cancelled) {
            QFile::remove(tempPath);
            return StepOutcome::Cancelled;
        }
        if (xr != xdelta::Result::Success) {
            QFile::remove(tempPath);
            if (xr != xdelta::Result::UpToDate) {
                qCWarning(lcPackageManager) << "xdelta failed:" << xdelta::resultName(xr)
                                            << "target" << step.dest;
                if (failDetail) {
                    *failDetail = (xr == xdelta::Result::NoMatch)
                        ? tr("oyun sürümü bu yamayla eşleşmiyor")
                        : (xr == xdelta::Result::Corrupt)
                            ? tr("yama dosyası okunamadı")
                            : tr("oyun dosyası değiştirilemedi");
                }
                return StepOutcome::SoftError;
            }
            qCInfo(lcPackageManager) << "xdelta: already up to date:" << step.dest;
        } else {
            const QString oldPath = targetPath + QStringLiteral(".makine-old");
            QFile::remove(oldPath);
            if (!QFile::rename(targetPath, oldPath)) {
                QFile::remove(tempPath);
                qCWarning(lcPackageManager) << "xdelta: cannot move original aside:" << targetPath;
                if (failDetail) *failDetail = tr("oyun dosyası değiştirilemedi");
                return StepOutcome::SoftError;
            }
            if (!QFile::rename(tempPath, targetPath)) {
                QFile::rename(oldPath, targetPath);   // put it back
                QFile::remove(tempPath);
                qCWarning(lcPackageManager) << "xdelta: cannot install patched file:" << targetPath;
                if (failDetail) *failDetail = tr("oyun dosyası değiştirilemedi");
                return StepOutcome::SoftError;
            }
            QFile::remove(oldPath);
            qCInfo(lcPackageManager) << "xdelta applied:" << step.dest;
        }
        // Ours either way — a re-install finds it already patched.
        installedFiles.append(QStringLiteral("_vpatch:") + step.dest);
        if (m_journal) m_journal->recordFileModified(QStringLiteral("_vpatch:") + step.dest);
        return StepOutcome::Ok;

    } else if (step.action == "setSteamLanguage") {
        if (step.language.isEmpty()) {
            qCWarning(lcPackageManager) << "setSteamLanguage: language not specified";
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: Steam dili ayarlanıyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.language));

        // Game path is typically: .../steamapps/common/GameName — go up two levels.
        //
        // No manifest, or none naming a language, means there is no Steam
        // setting to change: a GOG/repack copy, or a Steam game whose language
        // was never chosen. That is not a failed install — the files are in
        // and the install note tells the user which language to pick — yet it
        // failed every Mad Max install that was not a configured Steam one.
        QDir dir(gamePath);
        if (!dir.cdUp() || !dir.cdUp()) {
            qCInfo(lcPackageManager) << "setSteamLanguage: no steamapps dir above" << gamePath
                                     << "— not a Steam install, skipped";
            return StepOutcome::Ok;
        }
        QString acfPath = dir.absoluteFilePath("appmanifest_" + steamAppId + ".acf");
        QFile acfFile(acfPath);
        if (!acfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCInfo(lcPackageManager) << "setSteamLanguage: no Steam manifest" << acfPath
                                     << "— not a Steam install, skipped";
            return StepOutcome::Ok;
        }
        QString content = QString::fromUtf8(acfFile.readAll());
        acfFile.close();

        // Replace "language" value in ACF key-value format:  \t"language"\t\t"english"\n
        static const QRegularExpression langRe(QStringLiteral(R"(("language"\s+")([^"]*)("))"));
        auto match = langRe.match(content);
        if (!match.hasMatch()) {
            qCInfo(lcPackageManager) << "setSteamLanguage: no 'language' key in" << acfPath
                                     << "— skipped";
            return StepOutcome::Ok;
        }
        QString oldLang = match.captured(2);
        content.replace(match.capturedStart(2), match.capturedLength(2), step.language);

        if (!acfFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qCWarning(lcPackageManager) << "setSteamLanguage: cannot write ACF:" << acfPath;
            if (failDetail) *failDetail = tr("Steam ayar dosyası yazılamadı");
            return StepOutcome::SoftError;
        }
        acfFile.write(content.toUtf8());
        acfFile.close();

        installedFiles.append("_steamlang:" + steamAppId + ":" + oldLang);
        if (m_journal) m_journal->recordFileModified("_steamlang:" + steamAppId);
        qCDebug(lcPackageManager) << "setSteamLanguage:" << oldLang << "->" << step.language
                 << "for appId" << steamAppId;
        return StepOutcome::Ok;

    } else {
        qCWarning(lcPackageManager) << "Unknown step action:" << step.action;
        return StepOutcome::SoftError;
    }
}

// -- Execute install steps (recipe-based) -------------------------------------

void LocalPackageManager::executeInstallSteps(const PackageInfo& pkg, const QString& gamePath,
                                               const QString& packageDir)
{
    MAKINE_ZONE_NAMED("LPM::executeInstallSteps");
    INTEGRITY_GATE();

    // Pre-flight: refuse a recipe we cannot fully execute BEFORE touching the
    // game. A half-applied recipe (valid steps done, then an unknown action
    // failing mid-way) is the silent-corruption class guarded against in the
    // install-state flow. Fail loud with the extracted path instead.
    QList<InstallStep> steps = pkg.installSteps;
    const QStringList userTools = dropUserLaunchedTools(steps);
    if (!userTools.isEmpty()) {
        qCInfo(lcPackageManager) << "Not running user-launched tool(s)" << userTools
            << "— the recipe installs them for the user:" << pkg.gameName;
    }

    if (const QStringList bad = unexecutableSteps(steps); !bad.isEmpty()) {
        qCWarning(lcPackageManager) << "Recipe has unexecutable steps"
            << bad << "— refusing install for" << pkg.gameName;
        emit installCompleted(false, tr("Bu yama, uygulamanın şu an "
            "desteklemediği bir kurulum adımı içeriyor (%1). Otomatik "
            "kurulamıyor.\nYama dosyaları şu klasöre çıkarıldı:\n%2\n"
            "Klasördeki kurulum talimatını izleyin.")
            .arg(bad.join(QStringLiteral(", ")), shownPath(packageDir)));
        return;
    }

    const int total = steps.size();
    int current = 0;
    int errors = 0;
    QStringList installedFiles;
    QSet<QString> replacedFiles;
    QStringList errorDetails;

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Install;
        je.gameId = pkg.steamAppId;
        je.gamePath = gamePath;
        m_journal->beginOperation(je);
    }

    emit installProgress(0.0, tr("Kurulum adımları hazırlanıyor..."));

    for (const InstallStep& step : steps) {
        if (isCancelled()) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulum iptal edildi"));
            return;
        }

        current++;
        double progress = static_cast<double>(current) / (total + 1);

        QString failDetail;
        StepOutcome outcome = executeStep(step, gamePath, packageDir,
                                          progress, current, total,
                                          QString{}, pkg.steamAppId,
                                          installedFiles, replacedFiles, &failDetail);
        if (outcome == StepOutcome::FatalError) return; // fatal() already emitted
        if (outcome == StepOutcome::Cancelled) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulum iptal edildi"));
            return;
        }
        if (outcome == StepOutcome::SoftError) {
            errors++;
            // Collect detail about what failed
            QString detail = QStringLiteral("Adım %1: %2 %3")
                .arg(current).arg(step.action,
                                  step.src.isEmpty() ? step.exe : step.src);
            if (!failDetail.isEmpty()) detail += QStringLiteral(" — ") + failDetail;
            errorDetails.append(detail);
        }
    }

    if (errors == 0) {
        // Which files were the game's own decides what uninstall deletes. Not
        // recorded before, so uninstall deleted the originals the restore had
        // just put back — Far Cry 3 lost common.fat, Half-Life its .wav files.
        QStringList added;
        for (const QString& f : installedFiles)
            if (!f.startsWith(QLatin1Char('_')) && !replacedFiles.contains(f)) added << f;
        saveInstallState(pkg.steamAppId, gamePath, pkg, installedFiles, added,
                         replacedFiles.values());

        emit installCompleted(true,
            tr("%1 adım başarıyla tamamlandı").arg(total));
    } else {
        if (m_journal) m_journal->commitOperation();
        QString msg = tr("%1/%2 adımda hata oluştu").arg(errors).arg(total);
        if (!errorDetails.isEmpty())
            msg += QStringLiteral("\n") + errorDetails.join(QStringLiteral("\n"));
        // See installWithOptions(): "run as administrator" is not reachable in
        // the Store build, so it must not be offered as the remedy.
        msg += QStringLiteral("\n\n") + tr("Çözüm: oyunu ve Steam'i kapatın, "
            "antivirüste oyun klasörünü izinli yapın, sonra tekrar deneyin. "
            "Windows yönetici izni isterse açılan pencerede 'Evet' deyin. "
            "Sorun sürerse yamayı kaldırıp yeniden indirin.");
        emit installCompleted(false, msg);
    }
}

// -- Options-based install -----------------------------------------------------

void LocalPackageManager::installWithOptions(const PackageInfo& pkg, const QString& gamePath,
                                              const QString& basePackageDir, const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("LPM::installWithOptions");
    INTEGRITY_GATE();

    // Check for combined steps
    QStringList sortedIds = selectedOptions;
    sortedIds.sort();
    QString combinedKey = sortedIds.join("+");

    // Every step this install will run, gathered before anything is counted so
    // the tools the recipe installs for the user are known up front. Those are
    // dropped from the run list across the WHOLE selection, not per option: a
    // recipe may copy the tool to the desktop in one option and run it in
    // another. See steprules::runStepIsUserLaunchedTool.
    QHash<QString, QList<InstallStep>> optionSteps;
    QList<InstallStep> combined;
    {
        QList<InstallStep> selected;
        for (const InstallOptionQt& opt : pkg.installOptions)
            if (selectedOptions.contains(opt.id)) selected += opt.steps;
        if (pkg.combinedSteps.contains(combinedKey))
            selected += pkg.combinedSteps[combinedKey];
        const QStringList tools = userInstalledTools(selected);

        const auto keep = [&tools](const InstallStep& st) {
            return st.action != QLatin1String("run")
                || !steprules::runStepIsUserLaunchedTool(st.exe, tools);
        };
        QStringList dropped;
        for (const InstallOptionQt& opt : pkg.installOptions) {
            if (!selectedOptions.contains(opt.id)) continue;
            QList<InstallStep> kept;
            for (const InstallStep& st : opt.steps) {
                if (keep(st)) kept << st; else dropped << st.exe;
            }
            optionSteps.insert(opt.id, kept);
        }
        if (pkg.combinedSteps.contains(combinedKey)) {
            for (const InstallStep& st : pkg.combinedSteps[combinedKey]) {
                if (keep(st)) combined << st; else dropped << st.exe;
            }
        }
        if (!dropped.isEmpty()) {
            qCInfo(lcPackageManager) << "Not running user-launched tool(s)" << dropped
                << "— the recipe installs them for the user:" << pkg.gameName;
        }
    }

    int totalSteps = 0;
    for (const InstallOptionQt& opt : pkg.installOptions) {
        if (selectedOptions.contains(opt.id))
            totalSteps += optionSteps.value(opt.id).size();
    }
    totalSteps += combined.size();

    if (totalSteps == 0) {
        emit installCompleted(false, tr("Seçilen seçenekler için kurulum "
            "adımı yok. Çözüm: farklı bir kurulum seçeneği belirleyin ya da "
            "yamayı kaldırıp yeniden indirin."));
        return;
    }

    // Pre-flight: same guard as executeInstallSteps — refuse recipes with
    // actions we cannot execute before mutating the game directory. Collect the
    // steps actually selected (per-option + the combined recipe, if any).
    {
        QList<InstallStep> preflight;
        for (const InstallOptionQt& opt : pkg.installOptions)
            if (selectedOptions.contains(opt.id)) preflight += optionSteps.value(opt.id);
        preflight += combined;
        if (const QStringList bad = unexecutableSteps(preflight); !bad.isEmpty()) {
            qCWarning(lcPackageManager) << "Options recipe has unexecutable steps"
                << bad << "— refusing install for" << pkg.gameName;
            emit installCompleted(false, tr("Bu yama, uygulamanın şu an "
                "desteklemediği bir kurulum adımı içeriyor (%1). Otomatik "
                "kurulamıyor.").arg(bad.join(QStringLiteral(", "))));
            return;
        }
    }

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Install;
        je.gameId = pkg.steamAppId;
        je.gamePath = gamePath;
        m_journal->beginOperation(je);
    }


    int current = 0;
    int errors = 0;
    QStringList installedFiles;
    QSet<QString> replacedFiles;
    // Mirror executeInstallSteps: collect per-step failure context so the
    // user (and we, when triaging reports) can see which option/action
    // actually failed instead of a bare "%1 adımda hata oluştu". This was
    // the root cause behind the "Elden Ring 2 adımda hata" report — no way
    // to tell if the failure was a missing source, an AV-blocked run, a
    // copyToDesktop permission issue, etc.
    QStringList errorDetails;
    auto recordSoftError = [&](const InstallStep& step, const QString& prefix,
                               const QString& cause) {
        const QString target = step.dest.isEmpty()
            ? (step.src.isEmpty() ? step.exe : step.src)
            : step.dest;
        QString detail = QStringLiteral("%1Adım %2: %3 %4")
            .arg(prefix).arg(current).arg(step.action, target);
        if (!cause.isEmpty()) detail += QStringLiteral(" — ") + cause;
        errorDetails.append(detail);
    };

    emit installProgress(0.0, tr("Kurulum seçenekleri hazırlanıyor..."));

    // Execute steps for each selected option
    for (const InstallOptionQt& opt : pkg.installOptions) {
        if (!selectedOptions.contains(opt.id)) continue;

        // Resolve the option source dir tolerantly. The recipe declares a
        // subDir (e.g. "Türkçe Yama") but packages are not always wrapped in
        // it (repackaging / wrapper-strip inconsistency). When the strict
        // path is missing, fall back to the package root so step sources are
        // still found — this was the dominant cause of "1 adımda hata oluştu"
        // (e.g. Elden Ring), independent of any game-side mod.
        //
        // resolveOptionDir also matches the folder through the extractor's
        // old substitution ("T_rk_e Yama" for "Türkçe Yama"). That was the
        // real cause behind Elden Ring's "1 adımda hata oluştu" (48 events,
        // 8 users): optionDir is what executeStep receives as packageDir, so
        // falling back to the package root also moved the run step's working
        // directory.
        const QString optionDir = resolveOptionDir(basePackageDir, opt.subDir);
        if (!QDir(optionDir).exists()) {
            qCWarning(lcPackageManager) << "Option source dir not found:" << optionDir;
            errors++;
            errorDetails.append(QStringLiteral("%1 — %2: %3")
                .arg(opt.label, tr("kaynak klasör yok"), optionDir));
            continue;
        }

        emit installProgress(static_cast<double>(current) / totalSteps,
            tr("%1 — Hazırlanıyor...").arg(opt.label));

        QString prefix = opt.label + " — ";

        for (const InstallStep& step : optionSteps.value(opt.id)) {
            if (isCancelled()) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }

            current++;
            double progress = static_cast<double>(current) / (totalSteps + 1);

            QString failDetail;
            StepOutcome outcome = executeStep(step, gamePath, optionDir,
                                              progress, current, totalSteps,
                                              prefix, pkg.steamAppId,
                                              installedFiles, replacedFiles, &failDetail);
            if (outcome == StepOutcome::FatalError) return;
            if (outcome == StepOutcome::Cancelled) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }
            if (outcome == StepOutcome::SoftError) {
                errors++;
                recordSoftError(step, prefix, failDetail);
            }
        }
    }

    // Execute combined steps if multiple options selected
    {
        for (const InstallStep& step : combined) {
            current++;
            double progress = static_cast<double>(current) / (totalSteps + 1);

            QString failDetail;
            StepOutcome outcome = executeStep(step, gamePath, basePackageDir,
                                              progress, current, totalSteps,
                                              QString{}, pkg.steamAppId,
                                              installedFiles, replacedFiles, &failDetail);
            if (outcome == StepOutcome::FatalError) return;
            if (outcome == StepOutcome::Cancelled) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }
            if (outcome == StepOutcome::SoftError) {
                errors++;
                recordSoftError(step, QString{}, failDetail);
            }
        }
    }

    if (errors == 0) {
        // Which files were the game's own decides what uninstall deletes. Not
        // recorded before, so uninstall deleted the originals the restore had
        // just put back — Far Cry 3 lost common.fat, Half-Life its .wav files.
        QStringList added;
        for (const QString& f : installedFiles)
            if (!f.startsWith(QLatin1Char('_')) && !replacedFiles.contains(f)) added << f;
        saveInstallState(pkg.steamAppId, gamePath, pkg, installedFiles, added,
                         replacedFiles.values());

        emit installCompleted(true, tr("Kurulum başarıyla tamamlandı"));
    } else {
        if (m_journal) m_journal->commitOperation();
        QString msg = tr("%1 adımda hata oluştu").arg(errors);
        if (!errorDetails.isEmpty())
            msg += QStringLiteral("\n") + errorDetails.join(QStringLiteral("\n"));
        // Do not tell the user to run the launcher as administrator: the
        // shipped build is an MSIX package and Store apps have no such option,
        // so that advice sent 78 Elden Ring users down a dead end. Steps that
        // need elevation now raise a UAC prompt for the tool itself.
        msg += QStringLiteral("\n\n") + tr("Çözüm: oyunu ve Steam'i kapatın, "
            "antivirüste oyun klasörünü izinli yapın, sonra tekrar deneyin. "
            "Yama bir kurulum aracı çalıştırıyorsa Windows yönetici izni "
            "isteyebilir — açılan pencerede 'Evet' deyin. Sorun sürerse "
            "yamayı kaldırıp yeniden indirin.");
        emit installCompleted(false, msg);
    }
}

// -- Uninstall package --------------------------------------------------------

bool LocalPackageManager::uninstallPackage(const QString& steamAppId, const QString& gamePath,
                                           const QString& restoredFrom)
{
    MAKINE_ZONE_NAMED("LPM::uninstallPackage");
    INTEGRITY_GATE();
    CrashReporter::addBreadcrumb("package",
        QStringLiteral("uninstallPackage: %1").arg(steamAppId).toUtf8().constData());

    // Get installed state from core catalog
    auto maybeState = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!maybeState) return false;

    const auto& coreState = *maybeState;
    // The recorded path, unless the game has moved since: the restore just
    // wrote to gamePath, and deleting at a folder that is gone removed nothing
    // while the patch files stayed in the new one.
    const QString recordedPath = QString::fromStdString(coreState.gamePath);
    const QString basePath = (recordedPath.isEmpty()
                              || (!QDir(recordedPath).exists() && QDir(gamePath).exists()))
        ? gamePath
        : recordedPath;

    // Files we REPLACED at install time — BackupManager::restoreBackup
    // copies the originals back over them in the uninstall flow. Deleting
    // them again here would leave a void and break the game, so the plain
    // relPath loop must skip them. (Files we only ADDED, prefix entries,
    // and other categories are still removed below.)
    const std::set<std::string> replacedSet(coreState.replacedFiles.begin(),
                                            coreState.replacedFiles.end());

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Uninstall;
        je.gameId = steamAppId;
        je.gamePath = basePath;
        m_journal->beginOperation(je);
    }

    // Delete installed files (with path traversal protection)
    int deleted = 0;
    std::vector<std::string> remaining;  // records that could not be undone

    for (const auto& relPathStd : coreState.installedFiles) {
        const QString relPath = QString::fromStdString(relPathStd);

        // Handle desktop entries: "_desktop:filename" -> user Desktop
        if (relPath.startsWith("_desktop:")) {
            QString fileName = relPath.mid(9); // strip "_desktop:" prefix
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
                                  + "/" + fileName;
            if (QFile::exists(desktopPath)) {
                if (QFile::remove(desktopPath)) {
                    deleted++;
                    qCDebug(lcPackageManager) << "Desktop file removed:" << fileName;
                } else {
                    qCWarning(lcPackageManager) << "Failed to remove desktop file:" << fileName;
                    remaining.push_back(relPathStd);
                }
            }
            continue;
        }

        // Files a vpatch step rewrote in place. They belong to the game, not
        // to us: restoreBackup copies the originals back over them, and
        // deleting here would leave the game without an archive it needs.
        if (relPath.startsWith("_vpatch:")) {
            qCDebug(lcPackageManager) << "Leaving vpatch target to the restore:"
                                      << relPath.mid(8);
            continue;
        }

        // Handle rename entries: "_rename:oldName:newName" -> reverse rename
        if (relPath.startsWith("_rename:")) {
            QStringList parts = relPath.mid(8).split(':');
            if (parts.size() >= 2) {
                QString origName = parts[0];
                QString renamedName = parts[1];
                QString renamedPath = QDir::cleanPath(basePath + "/" + renamedName);
                QString origPath = QDir::cleanPath(basePath + "/" + origName);
                if (QFile::exists(renamedPath)) {
                    if (QFile::exists(origPath)) QFile::remove(origPath);
                    if (QFile::rename(renamedPath, origPath)) {
                        deleted++;
                        qCDebug(lcPackageManager) << "Rename reversed:" << renamedName << "->" << origName;
                    } else {
                        qCWarning(lcPackageManager) << "Failed to reverse rename:" << renamedName;
                        remaining.push_back(relPathStd);
                    }
                }
            }
            continue;
        }

        // Handle font entries: "_font:filename.ttf" -> user fonts dir
        if (relPath.startsWith("_font:")) {
#ifdef Q_OS_WIN
            QString fontName = relPath.mid(6); // strip "_font:" prefix
            QString fontPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts/" + fontName;
            if (QFile::exists(fontPath)) {
                if (QFile::remove(fontPath)) {
                    deleted++;
                    qCDebug(lcPackageManager) << "Font removed:" << fontName;
                } else {
                    qCWarning(lcPackageManager) << "Failed to remove font:" << fontName;
                    remaining.push_back(relPathStd);
                }
            }
#endif
            continue;
        }

        // Handle Steam language restore: "_steamlang:appId:originalLanguage"
        if (relPath.startsWith("_steamlang:")) {
            QStringList parts = relPath.mid(11).split(':');
            if (parts.size() >= 2) {
                QString appId = parts[0];
                QString origLang = parts[1];
                QDir dir(basePath);
                if (dir.cdUp() && dir.cdUp()) {
                    QString acfPath = dir.absoluteFilePath("appmanifest_" + appId + ".acf");
                    QFile acfFile(acfPath);
                    if (acfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QString content = QString::fromUtf8(acfFile.readAll());
                        acfFile.close();
                        static const QRegularExpression langRe(QStringLiteral(R"(("language"\s+")([^"]*)("))"));
                        content.replace(langRe, "\\1" + origLang + "\\3");
                        if (acfFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                            acfFile.write(content.toUtf8());
                            acfFile.close();
                            qCDebug(lcPackageManager) << "Steam language restored to:" << origLang << "for appId" << appId;
                        }
                    }
                }
            }
            continue;
        }

        // Handle Unity bundle patch restore: "_unitypatch:relPath" -> restore .makine_backup
        if (relPath.startsWith("_unitypatch:")) {
            QString bundleRelPath = relPath.mid(12); // strip "_unitypatch:" prefix
            QString bundleFullPath = QDir::cleanPath(basePath + "/" + bundleRelPath);
            QString backupPath = bundleFullPath + ".makine_backup";

            if (QFile::exists(backupPath)) {
                if (QFile::exists(bundleFullPath)) QFile::remove(bundleFullPath);
                if (QFile::rename(backupPath, bundleFullPath)) {
                    deleted++;
                    qCDebug(lcPackageManager) << "Unity bundle restored:" << bundleRelPath;
                } else {
                    qCWarning(lcPackageManager) << "Failed to restore Unity bundle:" << bundleRelPath;
                    remaining.push_back(relPathStd);
                }
            } else {
                qCWarning(lcPackageManager) << "Unity bundle backup not found:" << backupPath;
            }
            continue;
        }

        QString fullPath = QDir::cleanPath(basePath + "/" + relPath);
        // Prevent path traversal: ensure resolved path stays within game directory
        if (!security::isPathContained(basePath, fullPath)) {
            qCWarning(lcPackageManager) << "Path traversal blocked:" << relPath;
            continue;
        }
        // Skip originals we replaced — restoreBackup put the originals back
        // over them; removing here would leave a hole the game expects.
        if (replacedSet.count(relPathStd) > 0) {
            qCDebug(lcPackageManager) << "Preserving restored original:" << relPath;
            continue;
        }
        // Same for a file the backup holds: it was the game's before the
        // install. Recipe installs recorded no replaced list before 0.1.6, so
        // for those states this is the only thing that tells an original from
        // an added file (crash recovery uses the same rule).
        if (!restoredFrom.isEmpty()
            && rawfile::exists(QDir::cleanPath(restoredFrom + "/" + relPath))) {
            qCDebug(lcPackageManager) << "Preserving restored original (in backup):" << relPath;
            continue;
        }
        if (rawfile::exists(fullPath)) {
            // A read-only attribute (copied over from the package or set by
            // the game) fails the delete even with write access to the folder.
            QFile victim(fullPath);
            if (victim.remove()
                || (QFile::setPermissions(fullPath, QFile::permissions(fullPath)
                                                        | QFileDevice::WriteOwner
                                                        | QFileDevice::WriteUser)
                    && victim.remove())) {
                deleted++;
                if (m_journal) m_journal->recordFileModified(relPath);
            } else {
                qCWarning(lcPackageManager) << "Failed to remove:" << fullPath
                                            << "—" << victim.errorString();
                remaining.push_back(relPathStd);
            }
        }
    }

    qCDebug(lcPackageManager) << "Uninstall" << steamAppId << ":" << deleted << "files deleted,"
                              << remaining.size() << "failed";

    if (!remaining.empty()) {
        // Keep what is still on disk on record. Dropping the record reported
        // success and left those files behind with nothing to retry from.
        packages::InstalledPackageState left = coreState;
        left.installedFiles = std::move(remaining);
        m_catalog.markInstalled(steamAppId.toStdString(), left);
        saveCatalogInstalledState(m_catalog, installedStatePath());
        if (m_journal) m_journal->commitOperation();
        return false;
    }

    m_catalog.markUninstalled(steamAppId.toStdString());
    saveCatalogInstalledState(m_catalog, installedStatePath());
    if (m_journal) m_journal->commitOperation();
    return true;
}

} // namespace makine
