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
#include "profiler.h"
#include "operationjournal.h"
#include "pathsecurity.h"
#include "appprotection.h"
#include "apppaths.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

QStringList LocalPackageManager::getPackageFileList(const QString& steamAppId,
                                                     const QString& variant,
                                                     const QString& gamePath) const
{
    auto files = m_catalog.getPackageFileList(steamAppId.toStdString(), variant.toStdString());

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
    return result;
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

LocalPackageManager::CopyResult LocalPackageManager::tryCopyFile(
    const QString& src, const QString& dest)
{
    // Single attempt: try the copy and classify the failure mode.
    auto attempt = [&]() -> CopyResult {
        // Remove existing dest first; if removal fails, classify immediately
        if (QFile::exists(dest)) {
            if (!QFile::remove(dest)) {
                QFile probe(dest);
                if (!probe.open(QIODevice::WriteOnly)) {
                    auto fe = probe.error();
                    if (fe == QFileDevice::PermissionsError)
                        return {false, CopyError::PermissionDenied};
                    if (fe == QFileDevice::ResourceError)
                        return {false, CopyError::DiskFull};
                }
                return {false, CopyError::FileLocked};
            }
        }

        if (QFile::copy(src, dest)) return {true, CopyError::None};

        QFile probe(dest);
        if (!probe.open(QIODevice::WriteOnly)) {
            auto fe = probe.error();
            if (fe == QFileDevice::PermissionsError)
                return {false, CopyError::PermissionDenied};
            if (fe == QFileDevice::ResourceError)
                return {false, CopyError::DiskFull};
            if (fe == QFileDevice::OpenError && QFileInfo(dest).absoluteDir().exists())
                return {false, CopyError::FileLocked};
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

LocalPackageManager::ProcessResult LocalPackageManager::runProcess(
    const QString& exePath, const QStringList& args, const QString& workDir,
    std::function<void(int elapsedMs)> progressCallback)
{
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(exePath, args);

    ProcessResult result;
    if (!proc.waitForStarted(10000)) {
        qCWarning(lcPackageManager) << "Process failed to start:" << exePath;
        return result;
    }
    result.started = true;

    constexpr int kPollMs = 1000;        // Poll faster so cancel feels responsive
    constexpr int kMaxMs  = 1800000;     // 30 minutes hard cap
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

QString LocalPackageManager::resolveSourcePath(const PackageInfo& pkg, const QString& variant) const
{
    QString sourcePath;

    // Try new game-name directory format first
    if (!pkg.dirName.isEmpty()) {
        sourcePath = !variant.isEmpty()
            ? m_dataPath + "/" + pkg.dirName + "/" + variant
            : m_dataPath + "/" + pkg.dirName;
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

    // Return empty string if nothing found
    if (sourcePath.isEmpty() || !QDir(sourcePath).exists())
        return {};

    return sourcePath;
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
        return QFileInfo::exists(cand) ? QStringList{cand} : QStringList{};
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
    if (QFileInfo::exists(strict))
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
        if (!QFileInfo::exists(QDir::cleanPath(gamePath + QLatin1Char('/') + rel)))
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

    QString canonGamePath = QDir(gamePath).canonicalPath();
    const QString cleanGamePath = QDir::cleanPath(gamePath);
    if (canonGamePath.isEmpty())
        canonGamePath = cleanGamePath;

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

        // Prevent path traversal: ensure destination stays within game directory
        if (!destPath.startsWith(canonGamePath) && !destPath.startsWith(cleanGamePath)) {
            qCWarning(lcPackageManager) << "Path traversal blocked:" << relPath;
            result.errors++;
            continue;
        }

        // Ensure destination directory exists
        QFileInfo destInfo(destPath);
        if (!QDir().mkpath(destInfo.absolutePath())) {
            qCWarning(lcPackageManager) << "Failed to create directory:" << destInfo.absolutePath();
            result.errors++;
            continue;
        }

        bool destExists = QFile::exists(destPath);

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
            if (m_journal) m_journal->commitOperation();
            emit installCompleted(false, tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
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
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
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
        QString testPath = gamePath + "/.makine_write_test";
        QFile testFile(testPath);
        if (!testFile.open(QIODevice::WriteOnly)) {
            emit installCompleted(false,
                tr("Bu klasöre yazma izni yok. Uygulamayı yönetici olarak çalıştırmayı deneyin."));
            return;
        }
        testFile.close();
        testFile.remove();
    }

    // Handle variant-specific options install (e.g. GTA Trilogy: variant selects game, then options for patch/dubbing)
    if (!variant.isEmpty() && !selectedOptions.isEmpty()
        && pkg.variantInstallOptions.contains(variant))
    {
        QString variantDir = m_dataPath + "/" + pkg.dirName + "/" + variant;
        if (!QDir(variantDir).exists()) {
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
        if (!QDir(baseDir).exists()) {
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
        emit installCompleted(false, tr("Çeviri dosyaları bulunamadı: %1. İndirme eksik veya bozuk "
                "olabilir. Çözüm: internet bağlantınızı kontrol edin, yamayı "
                "kaldırıp yeniden indirin.").arg(pkg.gameName));
        return;
    }

    // Handle userPath install type: copy to user-relative path instead of game dir
    if (pkg.installMethodType == "userPath" && !pkg.installMethodTarget.isEmpty()) {
        const QString userHome = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        const QString targetPath = QDir::cleanPath(userHome + "/" + pkg.installMethodTarget);

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
            QDirIterator it(sourcePath, QDir::Files, QDirIterator::Subdirectories);
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
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
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
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
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
                emit installCompleted(false, tr("%1/%2 dosya kopyalanamadı. Olası neden: oyun açık, "
                "antivirüs engelliyor veya yazma izni yok. Çözüm: oyunu ve "
                "Steam'i kapatın, Makine Launcher'ı yönetici olarak "
                "çalıştırın, antivirüste oyun klasörünü izinli yapın, sonra "
                "tekrar deneyin.").arg(errors).arg(total));
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
            "çalıştırın ve talimatları izleyin.").arg(sourcePath));
        return;
    }

    // Forge-archive injection (AC Odyssey/Valhalla …) needs a QuickBMS/c1
    // forge toolchain the launcher does not bundle. Fail loudly instead of
    // silently overlay-copying the tool files and reporting success — that
    // produced the "yama kuruldu ama oyun İngilizce" false-success bug.
    // The package itself was downloaded into sourcePath; surface that path
    // so the user can run the included tool, rather than being redirected
    // off-launcher.
    if (pkg.installMethodType == "forge_inject") {
        emit installCompleted(false, tr("Bu yama, oyunun .forge "
            "arşivlerine enjeksiyon gerektirdiği için otomatik kurulamıyor. "
            "Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
            "Klasörün içindeki kurulum aracını çalıştırın ve istendiğinde "
            "oyun klasörünüzü gösterin.").arg(sourcePath));
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
    {
        if (!kOverlaySafeTypes.contains(pkg.installMethodType)) {
            qCWarning(lcPackageManager)
                << "No handler for install method" << pkg.installMethodType
                << "— refusing silent overlay for" << pkg.gameName;
            const QString& m = pkg.installMethodType;
            QString guide;
            if (m == "external")
                guide = tr("Bu yamanın çevirisi harici bir kurulum aracıyla "
                    "uygulanır, Makine Launcher tarafından otomatik "
                    "kurulamıyor. Yama dosyaları şu klasöre çıkarıldı:\n%1\n"
                    "Klasördeki kurulum talimatını izleyin.").arg(sourcePath);
            else if (m == "installer")
                guide = tr("Bu yama kendi kurulum sihirbazıyla gelir. Çözüm: "
                    "yama arşivini açın, içindeki kurulum (.exe) dosyasını "
                    "çalıştırın ve istendiğinde oyun klasörünü seçin.");
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
                    "başlatıcısında mod listesinden etkinleştirin.").arg(sourcePath);
            else
                guide = tr("Bu yama otomatik kurulamıyor (kurulum yöntemi: "
                    "%1). Yama dosyaları şu klasöre çıkarıldı:\n%2\n"
                    "Klasördeki kurulum talimatını izleyin.")
                    .arg(m, sourcePath);
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
        QDirIterator it(sourcePath, QDir::Files, QDirIterator::Subdirectories);
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
                tr("%1/%2 dosya kopyalanamadı. Olası neden: oyun açık, "
                "antivirüs engelliyor veya yazma izni yok. Çözüm: oyunu ve "
                "Steam'i kapatın, Makine Launcher'ı yönetici olarak "
                "çalıştırın, antivirüste oyun klasörünü izinli yapın, sonra "
                "tekrar deneyin.").arg(result.errors).arg(filesToCopy.size()));
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
            if (QFile::exists(filePath))
                QFile::remove(filePath);
        }

        // Re-run install (which handles recipe/options)
        installPackage(steamAppId, gamePath, variant, selectedOptions);
        return;
    }

    // --- Default overlay update ---

    const QString sourcePath = resolveSourcePath(pkg, variant);

    if (sourcePath.isEmpty()) {
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
        QDirIterator it(sourcePath, QDir::Files, QDirIterator::Subdirectories);
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
                if (QFile::exists(filePath)) {
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
    const QString& canonGamePath,
    const QString& cleanGamePath,
    double progress,
    int current, int total,
    const QString& progressPrefix,
    const QString& steamAppId,
    QStringList& installedFiles)
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
            return StepOutcome::SoftError;
        }
        auto [ok, err] = tryCopyFile(src, dst);
        if (ok) return StepOutcome::Ok;
        if (err == CopyError::DiskFull)
            return fatal(tr("Disk alanı yetersiz, kurulum durduruldu. Çözüm: oyunun "
                "bulunduğu diskte yer açın (birkaç GB genelde yeterli), sonra "
                "yamayı tekrar kurun."));
        if (err == CopyError::PermissionDenied)
            return fatal(tr("Oyun klasörüne yazılamıyor (izin reddedildi). Çözüm:\n"
                "1) Oyunu ve Steam'i tamamen kapatın\n"
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
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
                "2) Makine Launcher'ı yönetici olarak çalıştırın "
                "(sağ tık → Yönetici olarak çalıştır)\n"
                "3) Antivirüs / Windows Defender'da oyun klasörünü "
                "istisnaya ekleyin\n"
                "4) Klasör 'salt okunur' ise özelliklerinden kaldırın\n"
                "Sonra yamayı tekrar kurun."));
            qCWarning(lcPackageManager) << logLabel << "failed (locked):" << src << "->" << dst;
            return StepOutcome::SoftError;
        }
        qCWarning(lcPackageManager) << logLabel << "failed:" << src << "->" << dst;
        return StepOutcome::SoftError;
    };

    if (step.action == "copy" || step.action == "copyFile") {
        QString srcPath  = resolvePackageSource(packageDir, step.src);
        QString destPath = QDir::cleanPath(gamePath   + "/" + step.dest);

        if (!destPath.startsWith(canonGamePath) && !destPath.startsWith(cleanGamePath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in copy:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama dosya hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (!QFile::exists(srcPath)) {
            qCWarning(lcPackageManager) << "Copy source not found:" << srcPath;
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        StepOutcome outcome = copyOne(srcPath, destPath, "copy");
        if (outcome == StepOutcome::Ok) {
            installedFiles.append(step.dest);
            if (m_journal) m_journal->recordFileModified(step.dest);
        }
        return outcome;

    } else if (step.action == "copyDir") {
        QString srcDir  = resolvePackageSource(packageDir, step.src);
        QString destDir = QDir::cleanPath(gamePath   + "/" + step.dest);

        if (!destDir.startsWith(canonGamePath) && !destDir.startsWith(cleanGamePath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in copyDir:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama klasör hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        if (!QDir(srcDir).exists()) {
            qCWarning(lcPackageManager) << "copyDir source not found:" << srcDir;
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: %4/").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        QDirIterator dirIt(srcDir, QDir::Files, QDirIterator::Subdirectories);
        while (dirIt.hasNext()) {
            dirIt.next();
            QString relPath  = dirIt.filePath().mid(srcDir.length() + 1);
            QString destPath = QDir::cleanPath(destDir + "/" + relPath);
            StepOutcome outcome = copyOne(dirIt.filePath(), destPath, "copyDir");
            if (outcome == StepOutcome::Ok) {
                QString fullRelPath = step.dest + "/" + relPath;
                installedFiles.append(fullRelPath);
                if (m_journal) m_journal->recordFileModified(fullRelPath);
            } else if (outcome != StepOutcome::SoftError) {
                return outcome; // Fatal or Cancelled — propagate immediately
            }
            // SoftError inside copyDir: continue to next file (errors counted by caller)
        }
        return StepOutcome::Ok;

    } else if (step.action == "delete") {
        QString destPath = QDir::cleanPath(gamePath + "/" + step.dest);

        if (!destPath.startsWith(canonGamePath) && !destPath.startsWith(cleanGamePath)) {
            qCCritical(lcPackageManager) << "Path traversal blocked in delete:" << step.dest;
            return fatal(tr("Güvenlik ihlali: yama silme hedefi oyun klasörü dışına çıkmaya çalıştı"));
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: Siliniyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.dest));

        if (QFile::exists(destPath)) {
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
        QString canonExe  = QFileInfo(exePath).canonicalFilePath();
        QString canonGame = QDir(gamePath).canonicalPath();
        QString canonPkg  = QDir(packageDir).canonicalPath();
        if (!canonExe.startsWith(canonGame) && !canonExe.startsWith(canonPkg)) {
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
        if (!result.started || result.timedOut) {
            return StepOutcome::SoftError;
        }
        if (result.exitCode != 0) {
            qCWarning(lcPackageManager) << "Run: non-zero exit:" << result.exitCode
                       << "output:" << result.output.left(500);
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
        if (!QFile::exists(srcPath)) {
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

        if (!srcPath.startsWith(canonGamePath) || !destPath.startsWith(canonGamePath)) {
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

    } else if (step.action == "setSteamLanguage") {
        if (step.language.isEmpty()) {
            qCWarning(lcPackageManager) << "setSteamLanguage: language not specified";
            return StepOutcome::SoftError;
        }
        emit installProgress(progress,
            tr("%1Adım %2/%3: Steam dili ayarlanıyor %4").arg(progressPrefix).arg(current).arg(total).arg(step.language));

        // Game path is typically: .../steamapps/common/GameName — go up two levels
        QDir dir(gamePath);
        if (!dir.cdUp() || !dir.cdUp()) {
            qCWarning(lcPackageManager) << "setSteamLanguage: cannot find steamapps dir from:" << gamePath;
            return StepOutcome::SoftError;
        }
        QString acfPath = dir.absoluteFilePath("appmanifest_" + steamAppId + ".acf");
        QFile acfFile(acfPath);
        if (!acfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(lcPackageManager) << "setSteamLanguage: cannot open ACF:" << acfPath;
            return StepOutcome::SoftError;
        }
        QString content = QString::fromUtf8(acfFile.readAll());
        acfFile.close();

        // Replace "language" value in ACF key-value format:  \t"language"\t\t"english"\n
        static const QRegularExpression langRe(QStringLiteral(R"(("language"\s+")([^"]*)("))"));
        auto match = langRe.match(content);
        if (!match.hasMatch()) {
            qCWarning(lcPackageManager) << "setSteamLanguage: 'language' key not found in ACF:" << acfPath;
            return StepOutcome::SoftError;
        }
        QString oldLang = match.captured(2);
        content.replace(match.capturedStart(2), match.capturedLength(2), step.language);

        if (!acfFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qCWarning(lcPackageManager) << "setSteamLanguage: cannot write ACF:" << acfPath;
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
    const int total = pkg.installSteps.size();
    int current = 0;
    int errors = 0;
    QStringList installedFiles;
    QStringList errorDetails;

    // canonicalPath() returns empty for non-existent paths; fall back to cleanPath
    QString canonGamePath = QDir(gamePath).canonicalPath();
    const QString cleanGamePath = QDir::cleanPath(gamePath);
    if (canonGamePath.isEmpty())
        canonGamePath = cleanGamePath;

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Install;
        je.gameId = pkg.steamAppId;
        je.gamePath = gamePath;
        m_journal->beginOperation(je);
    }

    emit installProgress(0.0, tr("Kurulum adımları hazırlanıyor..."));

    for (const InstallStep& step : pkg.installSteps) {
        if (isCancelled()) {
            if (m_journal) m_journal->abortOperation();
            emit installCompleted(false, tr("Kurulum iptal edildi"));
            return;
        }

        current++;
        double progress = static_cast<double>(current) / (total + 1);

        StepOutcome outcome = executeStep(step, gamePath, packageDir,
                                          canonGamePath, cleanGamePath,
                                          progress, current, total,
                                          QString{}, pkg.steamAppId,
                                          installedFiles);
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
                .arg(current).arg(step.action, step.src);
            errorDetails.append(detail);
        }
    }

    if (errors == 0) {
        QMetaObject::invokeMethod(this, [this, steamAppId = pkg.steamAppId, gamePath, pkg, installedFiles]() {
            packages::InstalledPackageState state;
            state.version = pkg.version.toStdString();
            state.gamePath = gamePath.toStdString();
            for (const QString& f : installedFiles)
                state.installedFiles.push_back(f.toStdString());
            state.installedAt = QDateTime::currentSecsSinceEpoch();
            m_catalog.markInstalled(steamAppId.toStdString(), state);
            saveCatalogInstalledState(m_catalog, installedStatePath());
            if (m_journal) m_journal->commitOperation();
        }, Qt::QueuedConnection);

        emit installCompleted(true,
            tr("%1 adım başarıyla tamamlandı").arg(total));
    } else {
        if (m_journal) m_journal->commitOperation();
        QString msg = tr("%1/%2 adımda hata oluştu").arg(errors).arg(total);
        if (!errorDetails.isEmpty())
            msg += QStringLiteral("\n") + errorDetails.join(QStringLiteral("\n"));
        msg += QStringLiteral("\n\n") + tr("Çözüm: oyunu ve Steam'i kapatın, "
            "Makine Launcher'ı yönetici olarak çalıştırın, antivirüste oyun "
            "klasörünü izinli yapın. Sorun sürerse yamayı kaldırıp yeniden "
            "indirin.");
        emit installCompleted(false, msg);
    }
}

// -- Options-based install -----------------------------------------------------

void LocalPackageManager::installWithOptions(const PackageInfo& pkg, const QString& gamePath,
                                              const QString& basePackageDir, const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("LPM::installWithOptions");
    INTEGRITY_GATE();

    int totalSteps = 0;
    // Count total steps across all selected options
    for (const InstallOptionQt& opt : pkg.installOptions) {
        if (selectedOptions.contains(opt.id))
            totalSteps += opt.steps.size();
    }

    // Check for combined steps
    QStringList sortedIds = selectedOptions;
    sortedIds.sort();
    QString combinedKey = sortedIds.join("+");
    if (pkg.combinedSteps.contains(combinedKey))
        totalSteps += pkg.combinedSteps[combinedKey].size();

    if (totalSteps == 0) {
        emit installCompleted(false, tr("Seçilen seçenekler için kurulum "
            "adımı yok. Çözüm: farklı bir kurulum seçeneği belirleyin ya da "
            "yamayı kaldırıp yeniden indirin."));
        return;
    }

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::Install;
        je.gameId = pkg.steamAppId;
        je.gamePath = gamePath;
        m_journal->beginOperation(je);
    }

    QString canonGamePath = QDir(gamePath).canonicalPath();
    const QString cleanGamePath = QDir::cleanPath(gamePath);
    if (canonGamePath.isEmpty())
        canonGamePath = cleanGamePath;

    int current = 0;
    int errors = 0;
    QStringList installedFiles;
    // Mirror executeInstallSteps: collect per-step failure context so the
    // user (and we, when triaging reports) can see which option/action
    // actually failed instead of a bare "%1 adımda hata oluştu". This was
    // the root cause behind the "Elden Ring 2 adımda hata" report — no way
    // to tell if the failure was a missing source, an AV-blocked run, a
    // copyToDesktop permission issue, etc.
    QStringList errorDetails;
    auto recordSoftError = [&](const InstallStep& step, const QString& prefix) {
        const QString target = step.dest.isEmpty()
            ? (step.src.isEmpty() ? step.exe : step.src)
            : step.dest;
        errorDetails.append(QStringLiteral("%1Adım %2: %3 %4")
            .arg(prefix).arg(current).arg(step.action, target));
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
        QString optionDir = QDir::cleanPath(basePackageDir + "/" + opt.subDir);
        if (!opt.subDir.isEmpty() && !QDir(optionDir).exists()) {
            qCWarning(lcPackageManager) << "Option subDir not found, falling back to package root:"
                                        << optionDir;
            optionDir = QDir::cleanPath(basePackageDir);
        }
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

        for (const InstallStep& step : opt.steps) {
            if (isCancelled()) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }

            current++;
            double progress = static_cast<double>(current) / (totalSteps + 1);

            StepOutcome outcome = executeStep(step, gamePath, optionDir,
                                              canonGamePath, cleanGamePath,
                                              progress, current, totalSteps,
                                              prefix, pkg.steamAppId,
                                              installedFiles);
            if (outcome == StepOutcome::FatalError) return;
            if (outcome == StepOutcome::Cancelled) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }
            if (outcome == StepOutcome::SoftError) {
                errors++;
                recordSoftError(step, prefix);
            }
        }
    }

    // Execute combined steps if multiple options selected
    if (pkg.combinedSteps.contains(combinedKey)) {
        for (const InstallStep& step : pkg.combinedSteps[combinedKey]) {
            current++;
            double progress = static_cast<double>(current) / (totalSteps + 1);

            StepOutcome outcome = executeStep(step, gamePath, basePackageDir,
                                              canonGamePath, cleanGamePath,
                                              progress, current, totalSteps,
                                              QString{}, pkg.steamAppId,
                                              installedFiles);
            if (outcome == StepOutcome::FatalError) return;
            if (outcome == StepOutcome::Cancelled) {
                if (m_journal) m_journal->abortOperation();
                emit installCompleted(false, tr("Kurulum iptal edildi"));
                return;
            }
            if (outcome == StepOutcome::SoftError) {
                errors++;
                recordSoftError(step, QString{});
            }
        }
    }

    if (errors == 0) {
        QMetaObject::invokeMethod(this, [this, steamAppId = pkg.steamAppId, gamePath, pkg, installedFiles]() {
            packages::InstalledPackageState state;
            state.version = pkg.version.toStdString();
            state.gamePath = gamePath.toStdString();
            for (const QString& f : installedFiles)
                state.installedFiles.push_back(f.toStdString());
            state.installedAt = QDateTime::currentSecsSinceEpoch();
            m_catalog.markInstalled(steamAppId.toStdString(), state);
            saveCatalogInstalledState(m_catalog, installedStatePath());
            if (m_journal) m_journal->commitOperation();
        }, Qt::QueuedConnection);

        emit installCompleted(true, tr("Kurulum başarıyla tamamlandı"));
    } else {
        if (m_journal) m_journal->commitOperation();
        QString msg = tr("%1 adımda hata oluştu").arg(errors);
        if (!errorDetails.isEmpty())
            msg += QStringLiteral("\n") + errorDetails.join(QStringLiteral("\n"));
        msg += QStringLiteral("\n\n") + tr("Çözüm: oyunu ve Steam'i kapatın, "
            "Makine Launcher'ı yönetici olarak çalıştırın, antivirüste oyun "
            "klasörünü izinli yapın. 'run' adımı başarısız oluyorsa antivirüs "
            "yamanın .exe dosyasını engelliyor olabilir — istisnaya ekleyip "
            "tekrar deneyin. Sorun sürerse yamayı kaldırıp yeniden indirin.");
        emit installCompleted(false, msg);
    }
}

// -- Uninstall package --------------------------------------------------------

bool LocalPackageManager::uninstallPackage(const QString& steamAppId, const QString& gamePath)
{
    MAKINE_ZONE_NAMED("LPM::uninstallPackage");
    INTEGRITY_GATE();
    CrashReporter::addBreadcrumb("package",
        QStringLiteral("uninstallPackage: %1").arg(steamAppId).toUtf8().constData());

    // Get installed state from core catalog
    auto maybeState = m_catalog.getInstalledState(steamAppId.toStdString());
    if (!maybeState) return false;

    const auto& coreState = *maybeState;
    const QString basePath = coreState.gamePath.empty()
        ? gamePath
        : QString::fromStdString(coreState.gamePath);

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
    const QString canonBase = QDir(basePath).canonicalPath();
    int deleted = 0;
    int failed = 0;

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
                    failed++;
                }
            }
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
                        failed++;
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
                    failed++;
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
                    failed++;
                }
            } else {
                qCWarning(lcPackageManager) << "Unity bundle backup not found:" << backupPath;
            }
            continue;
        }

        QString fullPath = QDir::cleanPath(basePath + "/" + relPath);
        // Prevent path traversal: ensure resolved path stays within game directory
        if (!fullPath.startsWith(canonBase)) {
            qCWarning(lcPackageManager) << "Path traversal blocked:" << relPath;
            continue;
        }
        // Skip originals we replaced — restoreBackup put the originals back
        // over them; removing here would leave a hole the game expects.
        if (replacedSet.count(relPathStd) > 0) {
            qCDebug(lcPackageManager) << "Preserving restored original:" << relPath;
            continue;
        }
        if (QFile::exists(fullPath)) {
            if (QFile::remove(fullPath)) {
                deleted++;
                if (m_journal) m_journal->recordFileModified(relPath);
            } else {
                qCWarning(lcPackageManager) << "Failed to remove:" << fullPath;
                failed++;
            }
        }
    }

    qCDebug(lcPackageManager) << "Uninstall" << steamAppId << ":" << deleted << "files deleted," << failed << "failed";

    m_catalog.markUninstalled(steamAppId.toStdString());
    saveCatalogInstalledState(m_catalog, installedStatePath());
    if (m_journal) m_journal->commitOperation();
    return true;
}

} // namespace makine
