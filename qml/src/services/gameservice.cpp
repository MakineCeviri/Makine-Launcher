// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "gameservice.h"
#include <QHash>

#include "gamefolderrules.h"
#include "gamenamerules.h"
#include "postinstallrules.h"
#include "imagecachemanager.h"
#include "steamdetailsservice.h"
#include "backupmanager.h"
#include "apppaths.h"
#include "profiler.h"
#include "crashreporter.h"
#include "elevatedops.h"
#include "installsteprules.h"
#include "pathsecurity.h"
#include "rawfile.h"
#include "packagestamp.h"
#include "processscanner.h"
#include "processconsole.h"
#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QUrl>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QLoggingCategory>
#include <QtConcurrent>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

Q_LOGGING_CATEGORY(lcGameService, "makine.game")

namespace {
constexpr int kAutoScanDelayMs = 500;
// Newest output of an install's own program kept for the game page (~1000
// lines); the page lays the whole text out again on every update.
constexpr qsizetype kInstallConsoleCap = qsizetype{64} * 1024;

// Name-match rule (both directions) lives in gamenamerules.h so it can be
// exercised directly — see tests/ui/test_gamenamerules.cpp.
using makine::namerules::gameNamesLikelyMatch;
using makine::namerules::normalizeGameName;
using makine::namerules::Separators;
namespace folderrules = makine::folderrules;

} // namespace

namespace makine {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_steamDetails(new SteamDetailsService(this))
    , m_installTimeoutTimer(new QTimer(this))
    , m_supportedGamesModel(new SupportedGamesModel(this))
{
    connect(m_steamDetails, &SteamDetailsService::detailsFetched,
            this, &GameService::steamDetailsFetched);
    connect(m_steamDetails, &SteamDetailsService::detailsFetchError,
            this, &GameService::steamDetailsFetchError);

    // Past LocalPackageManager's 2-hour run-step ceiling. The install is
    // cancelled, not forgotten: freeing the slot while the worker still ran
    // let its late result land on the next install, which could then be
    // rolled back. The slot frees when the cancelled install reports back.
    m_installTimeoutTimer->setSingleShot(true);
    m_installTimeoutTimer->setInterval(125 * 60 * 1000);
    connect(m_installTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_installingGameId.isEmpty()) return;
        const QString stuck = m_installingGameId;
        qCWarning(lcGameService) << "Install timed out — cancelling" << stuck;
        cancelInstallation();
        emit translationInstallCompleted(stuck, false,
            tr("Kurulum yanıt vermiyor — uygulamayı yeniden başlatıp tekrar deneyin"));
    });

    // Every uninstall path ends in this signal, so the in-flight guard set by
    // uninstallTranslation is released in one place.
    connect(this, &GameService::translationUninstalled, this,
            [this](const QString& gameId) { m_uninstalling.remove(gameId); });

    connect(this, &GameService::gameListChanged, this, &GameService::libraryChanged);
    connect(this, &GameService::translationStatusChanged, this, &GameService::libraryChanged);
    connect(this, &GameService::supportedGamesChanged, this, &GameService::libraryChanged);
}

void GameService::initialize()
{
    // A minimized start initializes without a first frame, and the first frame
    // still calls this once the window is opened from the tray. A second pass
    // would connect every CoreBridge signal twice.
    if (m_initialized) return;
    m_initialized = true;
    CrashReporter::addBreadcrumb("game", "GameService::initialize");
    setupCoreBridge();
    loadFolderOverrides();

    // Load caches in background thread to avoid blocking the UI
    const QString gamesCachePath = AppPaths::gamesCacheFile();

    (void)QtConcurrent::run([this, gamesCachePath]() {
        MAKINE_THREAD_NAME("Worker-Init");
        MAKINE_ZONE_NAMED("GameService::initialize (async)");

        // Parse game cache (thread-safe: only reads file + creates local list)
        QList<GameInfo> games;
        {
            QFile file(gamesCachePath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
                file.close();

                if (err.error == QJsonParseError::NoError && doc.isArray()) {
                    const auto arr = doc.array();
                    games.reserve(arr.count());
                    for (const auto& value : arr) {
                        if (!value.isObject()) continue;
                        const QJsonObject obj = value.toObject();
                        GameInfo g;
                        g.id = obj["id"].toString();
                        g.name = obj["name"].toString();
                        g.installPath = obj["installPath"].toString();
                        g.steamAppId = obj["steamAppId"].toString();
                        g.source = obj["source"].toString();
                        g.engine = obj["engine"].toString();
                        g.isVerified = obj["isVerified"].toBool();
                        g.isInstalled = obj["isInstalled"].toBool();
                        g.hasTranslation = obj["hasTranslation"].toBool();
                        if (!g.id.isEmpty() && !g.name.isEmpty()) {
                            // Validate install path still exists on disk
                            if (g.isInstalled && !g.installPath.isEmpty()
                                && !QDir(g.installPath).exists()) {
                                g.isInstalled = false;
                                // Keep installPath — needed for backup association and error messages
                            }
                            games.append(g);
                        }
                    }
                }
            }
        }

        // Parsed here, adopted on the main thread below: the service's cache is
        // read and written there, and a fetch may already be under way.
        QHash<QString, SteamDetails> steamDetails = SteamDetailsService::readCache();

        // Pre-warm package installed cache (avoids 260× main-thread calls later)
        QHash<QString, bool> pkgCache;
        CoreBridge* cb = CoreBridge::instance();
        if (cb) {
            const QVariantList catalog = cb->allSupportedGames();
            pkgCache.reserve(catalog.size());
            for (const auto& item : catalog) {
                const QString appId = item.toMap().value("steamAppId").toString();
                if (!appId.isEmpty())
                    pkgCache[appId] = cb->isPackageInstalled(appId);
            }
        }

        qCDebug(lcGameService) << "Parsed" << games.count() << "games,"
                 << pkgCache.size() << "package statuses (background)";

        // Deliver results to main thread
        QMetaObject::invokeMethod(this, [this,
                                         g = std::move(games),
                                         p = std::move(pkgCache),
                                         d = std::move(steamDetails)]() mutable {
            MAKINE_ZONE_NAMED("GameService::initialize (main thread)");

            m_steamDetails->adoptCache(d);
            m_games = std::move(g);
            m_packageInstalledCache = std::move(p);
            rebuildCache();

            emit gameListChanged();
            emit translationStatusChanged();
            emit supportedGamesChanged();
            ensureSupportedGamesCache();

            // Always schedule a background library scan to detect
            // newly installed/uninstalled games. Use a short delay
            // if cache is empty (first run), longer if we have cached
            // data (cache serves as instant preview).
            const int delayMs = m_games.isEmpty() ? kAutoScanDelayMs : 2000;
            qCDebug(lcGameService) << "Background library scan scheduled in" << delayMs << "ms"
                     << "(cached:" << m_games.count() << "games)";
            QTimer::singleShot(delayMs, this, &GameService::scanAllLibraries);
        }, Qt::QueuedConnection);
    });
}

GameService::~GameService()
{
    if (m_coreBridge)
        disconnect(m_coreBridge, nullptr, this, nullptr);
}

void GameService::setManifestSync(ManifestSyncService* sync)
{
    m_manifestSync = sync;
    if (sync) {
        connect(sync, &ManifestSyncService::catalogReady, this, [this]() {
            // Remote catalog updated — invalidate supported games cache
            invalidateSupportedCache();
            qCDebug(lcGameService) << "catalogReady received — deferring game refresh";

            // Defer ALL work to next event loop iteration.
            // This ensures refreshPackageManifest() (connected in wireSignals,
            // which runs AFTER this handler) has already reloaded the catalog
            // into LocalPackageManager before we query hasTranslationPackage().
            QTimer::singleShot(0, this, [this]{
                // Refresh ALL games against the now-available catalog:
                // 1. Re-resolve non-Steam games without steamAppId
                // 2. Update hasTranslation for ALL games (fixes race where
                //    scan completed before catalog was loaded)
                bool changed = false;
                if (m_coreBridge) {
                    for (auto& game : m_games) {
                        // Step 1: Resolve non-Steam games without steamAppId
                        if (game.steamAppId.isEmpty()
                            && game.source != QLatin1String("steam")) {
                            QDir gameDir(game.installPath);
                            QString resolved = m_coreBridge->findMatchingAppId(gameDir.dirName());

                            // For Epic games: also try matching by display name (not just folder name)
                            if (resolved.isEmpty() && game.source == QLatin1String("epic")) {
                                resolved = m_coreBridge->findMatchingAppId(game.name);
                                if (!resolved.isEmpty()) {
                                    qCInfo(lcGameService) << "Resolved Epic game" << game.name
                                                          << "via display name:" << resolved;
                                }
                            }

                            if (!resolved.isEmpty()) {
                                game.steamAppId = resolved;
                                game.id = resolved;
                                changed = true;
                                qCDebug(lcGameService) << "Late-resolved" << game.name
                                         << "-> steamAppId:" << resolved;
                            }
                        }

                        // Step 2: Refresh hasTranslation for ALL games
                        if (!game.steamAppId.isEmpty()) {
                            bool hasTrans = m_coreBridge->hasTranslationPackage(game.steamAppId);
                            if (hasTrans) {
                                // Reject ID-spoofed cracks: catalog name must resemble local name
                                auto pkg = m_coreBridge->getPackageForGame(game.steamAppId);
                                if (pkg && !gameNamesLikelyMatch(game.name, pkg->gameName)) {
                                    qCWarning(lcGameService) << "Game name mismatch — appId" << game.steamAppId
                                                              << "local:" << game.name << "catalog:" << pkg->gameName
                                                              << "→ rejecting catalog match";
                                    hasTrans = false;
                                }
                            }
                            if (game.hasTranslation != hasTrans) {
                                game.hasTranslation = hasTrans;
                                changed = true;
                                qCDebug(lcGameService) << "Updated hasTranslation for" << game.name
                                         << "(" << game.steamAppId << ") ->" << hasTrans;
                            }
                        }
                    }
                    if (changed) {
                        rebuildCache();
                        saveCachedGames();
                    }
                }

                // Rebuild model with enriched data
                invalidateSupportedCache();
                supportedGames();
                emit supportedGamesChanged();
                emit gameListChanged();
            });
        });
    }
}

void GameService::setImageCache(ImageCacheManager* cache)
{
    m_imageCache = cache;
}

GameService* GameService::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new GameService();
}

QVariantList GameService::games() const
{
    if (m_cacheValid) {
        return m_gamesCache;
    }
    MAKINE_ZONE_NAMED("GameService::games (cache miss)");

    m_gamesCache.clear();
    m_gamesCache.reserve(m_games.count());

    for (const auto& game : m_games) {
        if (!game.isInstalled) continue;      // Only show installed games
        if (!game.hasTranslation) continue;   // Only show supported games
        QVariantMap map = game.toVariantMap();
        map["hasUpdate"] = false;
        m_gamesCache.append(map);
    }

    m_cacheValid = true;
    return m_gamesCache;
}

void GameService::noteScreen(const QString& screen, const QString& gameId)
{
    const QString text = gameId.isEmpty() ? screen : screen + QStringLiteral(" ") + gameId;
    CrashReporter::addBreadcrumb("navigation", text.left(120).toUtf8().constData());
}

void GameService::scanAllLibraries()
{
    MAKINE_ZONE_NAMED("GameService::scanAllLibraries");
    // Without the bridge nothing would ever report back, and isScanning
    // stayed true for good (the welcome flow asked before setup ran).
    if (m_isScanning || !m_coreBridge) return;

    m_isScanning = true;
    emit isScanningChanged();

    m_scanProgress = 0;
    m_scanStatus = tr("Oyun kütüphaneleri taranıyor...");

    m_coreBridge->scanAllLibraries();
}

void GameService::setupCoreBridge()
{
    m_coreBridge = CoreBridge::instance();

    connect(m_coreBridge, &CoreBridge::scanProgress,
            this, &GameService::onScanProgress);
    connect(m_coreBridge, &CoreBridge::scanCompleted,
            this, &GameService::onScanCompleted);
    connect(m_coreBridge, &CoreBridge::gameDetected,
            this, &GameService::onGameDetected);
    // Forward package install signals with gameId context
    connect(m_coreBridge, &CoreBridge::packageInstallProgress,
            this, [this](double progress, const QString& status) {
                if (!m_installingGameId.isEmpty()) {
                    emit translationInstallProgress(m_installingGameId, progress, status);
                }
            });
    connect(m_coreBridge, &CoreBridge::packageProcessOutput,
            this, [this](const QString& text) {
                if (m_installingGameId.isEmpty() || m_installingGameId != m_installConsoleGameId)
                    return;
                processconsole::appendTo(m_installConsole, text, kInstallConsoleCap);
                emit translationInstallOutput(m_installingGameId);
            });

    connect(m_coreBridge, &CoreBridge::packageInstallCompleted,
            this, [this](bool success, const QString& message) {
                m_installTimeoutTimer->stop();
                QString gameId = m_installingGameId;
                m_installingGameId.clear();
                m_installCancelRequested = false;
                if (success && !gameId.isEmpty()) {
                    // Update package installed cache
                    m_packageInstalledCache[gameId] = true;
                    // Update game's hasTranslation flag
                    auto idxIt = m_gameIdToIndex.constFind(gameId);
                    if (idxIt != m_gameIdToIndex.constEnd()) {
                        int idx = *idxIt;
                        if (idx >= 0 && idx < m_games.count()) {
                            m_games[idx].hasTranslation = true;
                            m_games[idx].isVerified = true;
                            invalidateAllCaches();
                            // Granular model update (no full reset)
                            m_supportedGamesModel->updatePackageStatus(m_games[idx].steamAppId, true);
                            emit translationStatusChanged();
                            emit supportedGamesChanged();


                        }
                    }
                    emit translationInstallCompleted(gameId, success, message);
                    return;
                }
                // Fail with a known game: try to roll back to the pre-install
                // state so the user is not left with a half-patched game.
                reportOperationFailure("install", gameId, message);
                if (!gameId.isEmpty()) {
                    performInstallRollback(gameId, message);
                    return;
                }
                emit translationInstallCompleted(gameId, success, message);
            });

    connect(m_coreBridge, &CoreBridge::packageInstallError,
            this, [this](const QString& error) {
                m_installTimeoutTimer->stop();
                QString gameId = m_installingGameId;
                m_installingGameId.clear();
                m_installCancelRequested = false;
                reportOperationFailure("install", gameId, error);
                if (!gameId.isEmpty()) {
                    performInstallRollback(gameId, error);
                } else {
                    emit translationInstallCompleted(gameId, false, error);
                }
            });

    // Load user-configured scan paths
    QStringList customPaths = QSettings().value(QStringLiteral("scan/customPaths")).toStringList();
    if (!customPaths.isEmpty()) {
        m_coreBridge->setCustomGamePaths(customPaths);
        qCDebug(lcGameService) << "Loaded" << customPaths.size() << "custom scan paths";
    }
}

void GameService::addCustomScanPath(const QString& path)
{
    QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty() || !QDir(cleanPath).exists()) return;

    QSettings settings;
    QStringList paths = settings.value(QStringLiteral("scan/customPaths")).toStringList();
    // Case-insensitive check on Windows (C:\Games == c:\games)
    bool exists = std::any_of(paths.begin(), paths.end(), [&](const QString& p) {
        return p.compare(cleanPath, Qt::CaseInsensitive) == 0;
    });
    if (!exists) {
        paths.append(cleanPath);
        settings.setValue(QStringLiteral("scan/customPaths"), paths);
        if (m_coreBridge) m_coreBridge->setCustomGamePaths(paths);
        qCDebug(lcGameService) << "Added custom scan path:" << cleanPath;
    }
}

void GameService::removeCustomScanPath(const QString& path)
{
    QSettings settings;
    QStringList paths = settings.value(QStringLiteral("scan/customPaths")).toStringList();
    paths.removeAll(QDir::cleanPath(path));
    settings.setValue(QStringLiteral("scan/customPaths"), paths);
    if (m_coreBridge) m_coreBridge->setCustomGamePaths(paths);
}

QStringList GameService::customScanPaths() const
{
    return QSettings().value(QStringLiteral("scan/customPaths")).toStringList();
}

void GameService::onScanProgress(qreal progress, const QString& status)
{
    bool changed = false;
    if (m_scanProgress != progress) {
        m_scanProgress = progress;
        changed = true;
    }
    if (m_scanStatus != status) {
        m_scanStatus = status;
        changed = true;
    }
    Q_UNUSED(changed);
}

void GameService::onScanCompleted(int count)
{
    MAKINE_ZONE_NAMED("GameService::onScanCompleted");

    const auto& detected = m_coreBridge->detectedGames();

    // Guard: if scan found 0 games but we had cached data, keep the cache.
    // This protects against registry access failures, VDF parse errors, etc.
    if (detected.isEmpty() && !m_games.isEmpty()) {
        qCWarning(lcGameService) << "Scan returned 0 games but cache has"
                   << m_games.count() << "— keeping cached data";
        m_isScanning = false;
        emit isScanningChanged();
        emit scanCompleted(0);
        return;
    }

    // Preserve manually added games across re-scan
    QList<GameInfo> manualGames;
    for (const auto& g : m_games) {
        if (g.source == QLatin1String("manual"))
            manualGames.append(g);
    }

    // Convert detected games to GameInfo
    m_games.clear();
    for (const auto& det : detected) {
        GameInfo game;
        game.id = det.id;
        game.name = det.name;
        game.installPath = det.installPath;
        game.source = det.source;
        game.engine = det.engine;
        game.steamAppId = det.steamAppId;
        game.isInstalled = true;
        game.hasTranslation = det.hasTranslation;  // Already set by worker thread

        // Reject ID-spoofed cracks: appid match alone isn't enough — verify
        // the catalog name resembles the locally detected game name.
        if (game.hasTranslation && !game.steamAppId.isEmpty() && m_coreBridge) {
            auto pkg = m_coreBridge->getPackageForGame(game.steamAppId);
            if (pkg && !gameNamesLikelyMatch(game.name, pkg->gameName)) {
                qCWarning(lcGameService) << "Game name mismatch — appId" << game.steamAppId
                                          << "local:" << game.name << "catalog:" << pkg->gameName
                                          << "→ rejecting catalog match";
                game.hasTranslation = false;
            }
        }

        game.isVerified = game.hasTranslation;

        m_games.append(game);
    }

    qCDebug(lcGameService) << "Scan result:" << m_games.count() << "games from stores"
             << "(manual:" << manualGames.count() << ")";

    // Re-add manual games that weren't found by scan (avoid duplicates by ID/path)
    QSet<QString> scannedIds;
    QSet<QString> scannedPaths;
    scannedIds.reserve(m_games.count());
    scannedPaths.reserve(m_games.count());
    for (const auto& g : m_games) {
        scannedIds.insert(g.id);
        scannedPaths.insert(g.installPath);
    }
    for (const auto& manual : manualGames) {
        if (scannedIds.contains(manual.id) || scannedPaths.contains(manual.installPath))
            continue;
        {
            // Validate manual game's install path still exists
            if (manual.isInstalled && !manual.installPath.isEmpty()
                && !QDir(manual.installPath).exists()) {
                continue;  // Game uninstalled from disk — drop it
            }
            m_games.append(manual);
        }
    }

    rebuildCache();
    applyFolderOverrides();

    // Update backup originalPaths for games that moved to a new location
    BackupManager* bm = BackupManager::instance();
    if (bm) {
        for (const auto& game : m_games) {
            if (bm->hasBackup(game.id))
                bm->updateOriginalPaths(game.id, game.installPath);
        }
    }

    // Clear stale package-installed cache — it may have been populated
    // before m_localPkgManager was initialized (returns false for all).
    // Next access will re-query via CoreBridge with the now-available catalog.
    m_packageInstalledCache.clear();

    m_isScanning = false;
    emit isScanningChanged();
    emit scanCompleted(count);

    CrashReporter::addBreadcrumb("game",
        QStringLiteral("Scan completed: %1 games detected").arg(count).toUtf8().constData());

    saveCachedGames();

    // Defer signal emission to next event loop iteration so onScanCompleted
    // returns immediately. QML binding re-evaluation happens after control
    // returns to the event loop, preventing a 3.5s main thread freeze.
    QTimer::singleShot(0, this, [this]{
        emit gameListChanged();
        emit translationStatusChanged();
        emit supportedGamesChanged();
    });

    // Defer supported games cache warm-up: let UI render first, then
    // lazy-compute on next QML access via supportedGames() getter.
    // The cache flag is already invalid from rebuildCache() above.

}

void GameService::onGameDetected(const QString& gameId, const QString& gameName)
{
    emit gameDetected(gameId);
    qCDebug(lcGameService) << "Game detected:" << gameName << "(" << gameId << ")";
}

namespace {

QString cleanEnvPath(const char* name)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(value));
}

// Folders a patch must never be written into, nor anywhere below them.
QStringList protectedGameTrees()
{
    return {cleanEnvPath("SystemRoot"), cleanEnvPath("ProgramData"),
            cleanEnvPath("CommonProgramFiles"), cleanEnvPath("CommonProgramFiles(x86)")};
}

// Folders that are no game's own folder, though a game may well sit below them.
QStringList protectedGameFolders()
{
    QStringList folders;
    for (const auto location : {QStandardPaths::HomeLocation, QStandardPaths::DesktopLocation,
                                QStandardPaths::DocumentsLocation, QStandardPaths::DownloadLocation})
        folders << QStandardPaths::writableLocation(location);
    folders << cleanEnvPath("ProgramFiles") << cleanEnvPath("ProgramFiles(x86)");
    return folders;
}

QString folderProblemText(folderrules::FolderProblem problem)
{
    using folderrules::FolderProblem;
    switch (problem) {
    case FolderProblem::Missing:
        return GameService::tr("Seçilen klasör bulunamadı.");
    case FolderProblem::DriveRoot:
        return GameService::tr("Sürücünün kendisi seçildi. Oyunun kendi klasörünü seçin.");
    case FolderProblem::Protected:
        return GameService::tr("Bu bir Windows ya da kullanıcı klasörü. Oyunun kendi klasörünü seçin.");
    case FolderProblem::NoExecutable:
        return GameService::tr("Bu klasörde oyun dosyası (.exe) bulunamadı. "
                               "Oyunun kurulu olduğu klasörü seçin.");
    case FolderProblem::Collection:
        return GameService::tr("Bu klasörde birden çok oyun var. "
                               "Eklemek istediğiniz oyunun kendi klasörünü seçin.");
    case FolderProblem::None:
        break;
    }
    return {};
}

} // namespace

void GameService::addManualGame(const QString& path)
{
    MAKINE_ZONE_NAMED("GameService::addManualGame");
    // QML hands over the folder dialog's URL, the process scanner a native path.
    const QString chosen = folderrules::toLocalPath(path);
    const QStringList trees = protectedGameTrees();
    const QStringList folders = protectedGameFolders();

    // Everything below reads the disk — judging the folder walks up to four
    // levels of it — so none of it runs on the UI thread.
    CoreBridge* cb = m_coreBridge;
    (void)QtConcurrent::run([this, chosen, trees, folders, cb]() {
        MAKINE_THREAD_NAME("Worker-ManualGame");
        MAKINE_ZONE_NAMED("addManualGame (async)");

        const folderrules::FolderVerdict verdict =
            folderrules::judgeGameFolder(chosen, trees, folders);
        if (verdict.problem != folderrules::FolderProblem::None) {
            const QString reason = folderProblemText(verdict.problem);
            QMetaObject::invokeMethod(this, [this, chosen, reason]() {
                qCWarning(lcGameService) << "addManualGame: rejected" << chosen << "-" << reason;
                emit manualGameRejected(chosen, reason);
            }, Qt::QueuedConnection);
            return;
        }

        // Users open the folder the .exe sits in; recipes are written against
        // the one the store installs into ("ELDEN RING", not "ELDEN RING/Game").
        QString root = verdict.root;
        const QString folderName = QDir(root).dirName();

        // Step 1: Try folder-name matching (fast, backward compatible)
        QString matchedAppId;
        if (cb) matchedAppId = cb->findMatchingAppId(folderName);

        // Step 2: If no match, try file-based fingerprint matching
        if (matchedAppId.isEmpty() && cb) {
            QVariantList candidates = cb->findMatchingGamesFromFiles(root);
            if (!candidates.isEmpty()) {
                QVariantMap best = candidates.first().toMap();
                if (best.value(QStringLiteral("confidence")).toInt() >= 70) {
                    matchedAppId = best.value(QStringLiteral("steamAppId")).toString();
                    qCDebug(lcGameService) << "Fingerprint match:" << matchedAppId
                             << "confidence:" << best.value(QStringLiteral("confidence")).toInt()
                             << "matchedBy:" << best.value(QStringLiteral("matchedBy")).toString();
                }
            }
        }

        // A catalogue game: its patch says where the root is, the same way as
        // when the folder is picked on the game's own page.
        if (!matchedAppId.isEmpty() && cb) {
            const auto ev = folderrules::pickRoot(chosen, folderName, cb->folderExpectations(matchedAppId));
            if (!ev.root.isEmpty()
                && folderrules::checkGameFolder(ev.root, trees, folders) == folderrules::FolderProblem::None)
                root = ev.root;
        }
        if (root != chosen)
            qCInfo(lcGameService) << "addManualGame:" << chosen << "-> game root" << root;

        QString engine;
        if (cb) engine = cb->detectEngine(root);

        QMetaObject::invokeMethod(this, [this, root, folderName, engine, matchedAppId]() {
            finalizeManualGame(root, folderName, engine, matchedAppId);
        }, Qt::QueuedConnection);
    });
}

void GameService::finalizeManualGame(const QString& path, const QString& folderName,
                                      const QString& engine, const QString& matchedAppId)
{
    MAKINE_ZONE_NAMED("GameService::finalizeManualGame");

    // Already in the library — added before, or found by a scan.
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    for (const auto& game : m_games) {
        if (QFileInfo(game.installPath).canonicalFilePath()
                .compare(canonicalPath, Qt::CaseInsensitive) == 0) {
            emit manualGameAdded(game.id);
            return;
        }
    }

    // Retry matching on main thread — catalog may have loaded
    // since the background thread attempted matching
    QString resolvedAppId = matchedAppId;
    if (resolvedAppId.isEmpty() && m_coreBridge) {
        resolvedAppId = m_coreBridge->findMatchingAppId(folderName);
        if (!resolvedAppId.isEmpty()) {
            qCDebug(lcGameService) << "Manual game matched on retry:" << folderName
                     << "-> steamAppId:" << resolvedAppId;
        }
    }

    // If still no match, try matching via ManifestSync catalog by name. The
    // whole name, not a part of it: "contains" took a folder named "Game" for
    // the first catalog title with "game" in it.
    const QString folderKey = normalizeGameName(folderName, Separators::Compact);
    if (resolvedAppId.isEmpty() && m_manifestSync && !folderKey.isEmpty()) {
        const QVariantList catalog = m_manifestSync->catalog();
        const QString lowerFolder = folderName.toLower();
        for (const auto& item : catalog) {
            const QVariantMap entry = item.toMap();
            const QString nameKey = normalizeGameName(
                entry.value(QStringLiteral("gameName")).toString(), Separators::Compact);
            const QString dirLower = entry.value(QStringLiteral("dirName")).toString().toLower();
            if (nameKey == folderKey || dirLower == lowerFolder) {
                resolvedAppId = entry.value(QStringLiteral("steamAppId")).toString();
                qCDebug(lcGameService) << "Manual game matched via ManifestSync:"
                         << folderName << "-> steamAppId:" << resolvedAppId;
                break;
            }
        }
    }

    GameInfo game;
    game.installPath = path;
    game.source = "manual";
    game.isInstalled = true;
    game.engine = engine.isEmpty() ? "Unknown" : engine;

    if (!resolvedAppId.isEmpty()) {
        game.id = resolvedAppId;
        game.steamAppId = resolvedAppId;
        game.hasTranslation = true;

        auto pkg = m_coreBridge ? m_coreBridge->getPackageForGame(resolvedAppId) : std::nullopt;
        game.name = (pkg.has_value()) ? pkg->gameName : folderName;
    } else {
        // From the folder, not the library size: manual games outlive rescans,
        // so a count-based id collided and re-pointed the existing game.
        game.id = QStringLiteral("manual_") + QString::fromLatin1(
            QCryptographicHash::hash(path.toLower().toUtf8(), QCryptographicHash::Sha1)
                .toHex().left(12));
        game.name = folderName;
        game.hasTranslation = false;
        qCWarning(lcGameService) << "Manual game not matched to catalog:" << folderName;
    }

    // Avoid duplicate ID collision (e.g. steam scan already found this game)
    if (m_gameIdToIndex.contains(game.id)) {
        int idx = m_gameIdToIndex[game.id];
        if (idx >= 0 && idx < m_games.count()) {
            m_games[idx].installPath = path;
            m_games[idx].isInstalled = true;
            invalidateGameListCache();
            emit gameListChanged();
            emit manualGameAdded(game.id);
            return;
        }
    }

    m_games.append(game);
    m_gameIdToIndex[game.id] = m_games.count() - 1;
    if (!game.steamAppId.isEmpty())
        m_steamAppIdToIndex[game.steamAppId] = m_games.count() - 1;

    invalidateAllCaches();
    emit gameListChanged();
    emit supportedGamesChanged();
    emit gameDetected(game.id);
    emit manualGameAdded(game.id);

    qCDebug(lcGameService) << "Manual game added:" << game.name << "id:" << game.id
             << "engine:" << game.engine << "hasTranslation:" << game.hasTranslation;

    // Persist to disk so manual games survive app restart
    saveCachedGames();
}

void GameService::forgetGame(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::forgetGame");

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd()) {
        qCWarning(lcGameService) << "forgetGame: unknown game" << gameId;
        return;
    }

    int idx = it.value();
    if (idx < 0 || idx >= m_games.count())
        return;

    const auto& game = m_games[idx];

    // Block removal if translation is still installed
    if (m_coreBridge && m_coreBridge->isPackageInstalled(game.steamAppId.isEmpty() ? game.id : game.steamAppId)) {
        qCWarning(lcGameService) << "forgetGame: translation still installed for" << gameId;
        emit translationInstallCompleted(gameId, false,
            tr("Önce çeviri yamasını kaldırın"));
        return;
    }

    qCInfo(lcGameService) << "Removing game from library:" << game.name << "id:" << gameId;

    // Clean up index maps
    m_gameIdToIndex.remove(gameId);
    if (!game.steamAppId.isEmpty())
        m_steamAppIdToIndex.remove(game.steamAppId);
    m_packageInstalledCache.remove(game.steamAppId.isEmpty() ? game.id : game.steamAppId);

    // Remove from list and rebuild index maps
    m_games.removeAt(idx);
    m_gameIdToIndex.clear();
    m_steamAppIdToIndex.clear();
    for (int i = 0; i < m_games.count(); ++i) {
        m_gameIdToIndex[m_games[i].id] = i;
        if (!m_games[i].steamAppId.isEmpty())
            m_steamAppIdToIndex[m_games[i].steamAppId] = i;
    }

    invalidateAllCaches();
    saveCachedGames();

    emit gameListChanged();
    emit supportedGamesChanged();
    emit gameRemoved(gameId);
}

bool GameService::changeGamePath(const QString& gameId, const QString& newPath)
{
    MAKINE_ZONE_NAMED("GameService::changeGamePath");

    if (newPath.isEmpty() || !QDir(newPath).exists()) {
        qCWarning(lcGameService) << "changeGamePath: invalid path" << newPath;
        return false;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd()) {
        qCWarning(lcGameService) << "changeGamePath: unknown game" << gameId;
        return false;
    }

    int idx = it.value();
    if (idx < 0 || idx >= m_games.count())
        return false;

    m_games[idx].installPath = newPath;
    m_games[idx].isInstalled = true;

    // Update backup originalPaths to match the new game location
    BackupManager* bm = BackupManager::instance();
    if (bm) bm->updateOriginalPaths(gameId, newPath);

    invalidateGameListCache();
    saveCachedGames();

    emit gameListChanged();

    qCInfo(lcGameService) << "Game path updated:" << gameId << "→" << newPath;
    return true;
}

// ── Game folder ─────────────────────────────────────────────────────────────

namespace {

QString gameFoldersFile() { return AppPaths::dataDir() + QStringLiteral("/game_folders.json"); }

bool sameFolder(const QString& a, const QString& b)
{
    return !a.isEmpty() && !b.isEmpty()
        && QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
}

// What the user should see: the real location under MSIX, Windows separators.
QString shownFolder(const QString& path)
{
    return path.isEmpty() ? QString() : QDir::toNativeSeparators(ElevatedOps::realPath(path));
}

} // namespace

int GameService::gameIndexFor(const QString& gameId) const
{
    int idx = m_gameIdToIndex.value(gameId, -1);
    if (idx < 0) idx = m_steamAppIdToIndex.value(gameId, -1);
    return idx < m_games.count() ? idx : -1;
}

void GameService::loadFolderOverrides()
{
    QFile f(gameFoldersFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        m_folderOverrides.insert(it.key(), it.value().toString());
}

void GameService::saveFolderOverrides() const
{
    QJsonObject o;
    for (auto it = m_folderOverrides.cbegin(); it != m_folderOverrides.cend(); ++it)
        o.insert(it.key(), it.value());
    QDir().mkpath(AppPaths::dataDir());
    QSaveFile f(gameFoldersFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

// A folder the user chose wins over the one a scan finds — as long as it is
// still there; one that is gone goes back to the scan's.
void GameService::applyFolderOverrides()
{
    // A patch keeps the folder it went into. A scan that now finds the game a
    // level deeper (an unpacked "<Game>/<Game>") must not move the game out
    // from under it: uninstall and the backup's restore both follow installPath.
    if (m_coreBridge) {
        for (GameInfo& g : m_games) {
            const auto inst = m_coreBridge->getInstalledInfo(g.id);
            if (inst && QDir(inst->gamePath).exists() && !sameFolder(inst->gamePath, g.installPath)
                && folderrules::isSameOrUnder(g.installPath, inst->gamePath))
                g.installPath = inst->gamePath;
        }
    }

    bool dropped = false;
    for (auto it = m_folderOverrides.begin(); it != m_folderOverrides.end();) {
        if (!QDir(it.value()).exists()) {
            it = m_folderOverrides.erase(it);
            dropped = true;
            continue;
        }
        if (const int idx = gameIndexFor(it.key()); idx >= 0)
            m_games[idx].installPath = it.value();
        ++it;
    }
    if (dropped) saveFolderOverrides();
}

void GameService::inspectGameFolder(const QString& gameId, const QString& picked, bool audit)
{
    MAKINE_ZONE_NAMED("GameService::inspectGameFolder");
    const QString chosen = folderrules::toLocalPath(picked);
    if (!m_coreBridge) {
        emit gameFolderInspected(gameId, {{QStringLiteral("verdict"), QStringLiteral("refused")},
            {QStringLiteral("problem"), tr("Uygulama hâlâ açılıyor; birkaç saniye sonra tekrar dene.")}});
        return;
    }
    const auto pkg = m_coreBridge->getPackageForGame(gameId);
    const QString appId = pkg ? pkg->gameId : gameId;
    const QString gameName = pkg ? pkg->gameName : QString();
    const auto installed = m_coreBridge->getInstalledInfo(gameId);
    const QString patchedAt = installed ? installed->gamePath : QString();
    const int idx = gameIndexFor(gameId);
    const QString current = idx >= 0 ? m_games[idx].installPath : QString();
    const QStringList trees = protectedGameTrees();
    const QStringList folders = protectedGameFolders();
    CoreBridge* cb = m_coreBridge;

    // Walks the pick, the folders around it and the package — never on the UI thread.
    (void)QtConcurrent::run([this, cb, gameId, chosen, appId, gameName, patchedAt, current,
                             trees, folders, audit]() {
        MAKINE_THREAD_NAME("Worker-GameFolder");
        QVariantMap r;
        r[QStringLiteral("audit")] = audit;
        r[QStringLiteral("picked")] = QDir::toNativeSeparators(chosen);

        QString problem;
        folderrules::RootEvidence ev;
        if (chosen.isEmpty() || !QFileInfo::exists(chosen)) {
            problem = folderProblemText(folderrules::FolderProblem::Missing);
        } else {
            ev = folderrules::pickRoot(chosen, gameName, cb->folderExpectations(appId));
            if (ev.root.isEmpty()) {
                const auto v = folderrules::judgeGameFolder(chosen, trees, folders);
                problem = folderProblemText(v.problem != folderrules::FolderProblem::None
                                                ? v.problem : folderrules::FolderProblem::Collection);
            } else if (const auto p = folderrules::checkGameFolder(ev.root, trees, folders);
                       p != folderrules::FolderProblem::None) {
                problem = folderProblemText(p);
            }
        }
        // The patch must come off the folder it is in before the game moves:
        // its record and its backup both point there.
        const bool patchedElsewhere = problem.isEmpty() && !patchedAt.isEmpty()
                                      && !sameFolder(patchedAt, ev.root);
        r[QStringLiteral("patchedElsewhere")] = patchedElsewhere;
        r[QStringLiteral("patchedAt")] = shownFolder(patchedAt);
        if (patchedElsewhere) {
            problem = tr("Türkçe yama şu an şu klasörde kurulu:\n%1\n"
                         "Klasörü değiştirmeden önce yamayı kaldır; yoksa oradaki yama ve "
                         "yedeği sahipsiz kalır.").arg(shownFolder(patchedAt));
        }

        // Another catalogue game recognises the folder more surely than this one.
        QString otherGame;
        if (problem.isEmpty() && !audit) {
            const QVariantList matches = cb->findMatchingGamesFromFiles(ev.root);
            int ours = 0;
            for (const QVariant& m : matches) {
                const QVariantMap mm = m.toMap();
                if (mm.value(QStringLiteral("steamAppId")).toString() == appId)
                    ours = mm.value(QStringLiteral("confidence")).toInt();
            }
            if (!matches.isEmpty()) {
                const QVariantMap top = matches.first().toMap();
                const QString topId = top.value(QStringLiteral("steamAppId")).toString();
                const int conf = top.value(QStringLiteral("confidence")).toInt();
                if (topId != appId && conf >= 70 && conf > ours + 10) {
                    const auto other = cb->getPackageForGame(topId);
                    otherGame = other ? other->gameName : topId;
                }
            }
        }

        // Another game's folder, and nothing of this one's in it: installing
        // there would write this patch over that game. Not a "yine de" case.
        if (!otherGame.isEmpty() && ev.exe.isEmpty() && ev.filesFound == 0) {
            problem = tr("Bu klasör %1 oyununa ait; %2 yaması buraya kurulamaz.")
                          .arg(otherGame, gameName.isEmpty() ? tr("bu oyunun") : gameName);
        }

        const bool missingFiles = ev.filesFound < ev.filesTotal;
        const bool evidence = !ev.exe.isEmpty() || ev.filesFound > 0
                              || (ev.dirsTotal > 0 && ev.dirsFound * 2 >= ev.dirsTotal);
        r[QStringLiteral("verdict")] = !problem.isEmpty() ? QStringLiteral("refused")
            : (evidence && otherGame.isEmpty() && !missingFiles) ? QStringLiteral("ok")
            : QStringLiteral("unsure");
        r[QStringLiteral("problem")] = problem;
        r[QStringLiteral("root")] = ev.root;
        r[QStringLiteral("rootShown")] = shownFolder(ev.root);
        r[QStringLiteral("climbed")] = !ev.root.isEmpty()
            && !sameFolder(ev.root, QFileInfo(chosen).isFile() ? QFileInfo(chosen).absolutePath() : chosen);
        r[QStringLiteral("exe")] = QDir::toNativeSeparators(ev.exe);
        r[QStringLiteral("dirsFound")] = ev.dirsFound;
        r[QStringLiteral("dirsTotal")] = ev.dirsTotal;
        r[QStringLiteral("filesFound")] = ev.filesFound;
        r[QStringLiteral("filesTotal")] = ev.filesTotal;
        r[QStringLiteral("otherGame")] = otherGame;
        r[QStringLiteral("same")] = sameFolder(ev.root, current);
        // canWrite leaves a probe file behind for a moment; an audit writes nothing.
        r[QStringLiteral("writable")] = audit || ev.root.isEmpty() || ElevatedOps::canWrite(ev.root);
        r[QStringLiteral("engine")] = audit || ev.root.isEmpty() ? QString() : cb->detectEngine(ev.root);

        QMetaObject::invokeMethod(this, [this, gameId, r]() {
            qCInfo(lcGameService) << "inspectGameFolder" << gameId << r.value(QStringLiteral("picked"))
                                  << "->" << r.value(QStringLiteral("root"))
                                  << r.value(QStringLiteral("verdict"));
            emit gameFolderInspected(gameId, r);
        }, Qt::QueuedConnection);
    });
}

bool GameService::setGameFolder(const QString& gameId, const QString& root, const QString& engine)
{
    MAKINE_ZONE_NAMED("GameService::setGameFolder");
    const QString path = folderrules::toLocalPath(root);
    if (path.isEmpty() || !QFileInfo(path).isDir() || !m_coreBridge) return false;
    if (const auto inst = m_coreBridge->getInstalledInfo(gameId); inst && !sameFolder(inst->gamePath, path)) {
        qCWarning(lcGameService) << "setGameFolder: patch installed at" << inst->gamePath
                                 << "- refusing to move" << gameId;
        return false;
    }

    QString id = gameId;
    if (const int idx = gameIndexFor(gameId); idx >= 0) {
        GameInfo& g = m_games[idx];
        id = g.id;
        g.installPath = path;
        g.isInstalled = true;
        if (g.source != QLatin1String("manual")) {
            m_folderOverrides.insert(g.id, path);
            saveFolderOverrides();
        }
        if (BackupManager* bm = BackupManager::instance())
            bm->updateOriginalPaths(g.id, path);
    } else {
        // A catalogue game no scan found: bound here, kept like a hand-added one.
        const auto pkg = m_coreBridge->getPackageForGame(gameId);
        if (!pkg) return false;
        GameInfo g;
        g.id = pkg->gameId;
        g.steamAppId = pkg->gameId;
        g.name = pkg->gameName;
        g.installPath = path;
        g.source = QStringLiteral("manual");
        g.engine = engine.isEmpty() ? QStringLiteral("Unknown") : engine;
        g.isInstalled = true;
        g.hasTranslation = true;
        g.isVerified = true;
        id = g.id;
        m_games.append(g);
        rebuildCache();
        emit gameDetected(g.id);
    }

    invalidateAllCaches();
    saveCachedGames();
    emit gameListChanged();
    emit supportedGamesChanged();
    emit gameFolderChanged(gameId);
    qCInfo(lcGameService) << "Game folder set:" << id << "->" << path;
    return true;
}

QVariantMap GameService::gameFolderInfo(const QString& gameId) const
{
    const int idx = gameIndexFor(gameId);
    if (idx < 0) return {};
    const GameInfo& g = m_games[idx];
    return {
        {QStringLiteral("path"), g.installPath},
        {QStringLiteral("shown"), shownFolder(g.installPath)},
        {QStringLiteral("source"), g.source},
        {QStringLiteral("chosen"), m_folderOverrides.contains(g.id) || g.source == QLatin1String("manual")},
        {QStringLiteral("resettable"), m_folderOverrides.contains(g.id)},
        {QStringLiteral("exists"), QDir(g.installPath).exists()},
    };
}

void GameService::openGameFolder(const QString& gameId) const
{
    const int idx = gameIndexFor(gameId);
    if (idx < 0) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(ElevatedOps::realPath(m_games[idx].installPath)));
}

void GameService::resetGameFolder(const QString& gameId)
{
    const int idx = gameIndexFor(gameId);
    const QString id = idx >= 0 ? m_games[idx].id : gameId;
    if (!m_folderOverrides.remove(id)) return;
    saveFolderOverrides();
    scanAllLibraries();
}

QVariantMap GameService::getGameById(const QString& id) const
{
    MAKINE_ZONE_NAMED("GameService::getGameById");
    // First check installed games
    auto idxIt = m_gameIdToIndex.constFind(id);
    if (idxIt != m_gameIdToIndex.constEnd()) {
        int index = *idxIt;
        if (index >= 0 && index < m_games.count()) {
            QVariantMap map = m_games[index].toVariantMap();
            // Enrich with cached packageInstalled status
            auto cacheIt = m_packageInstalledCache.constFind(id);
            if (cacheIt != m_packageInstalledCache.constEnd()) {
                map["packageInstalled"] = *cacheIt;
            } else if (m_coreBridge) {
                bool installed = m_coreBridge->isPackageInstalled(id);
                m_packageInstalledCache[id] = installed;
                map["packageInstalled"] = installed;
            }
            return map;
        }
    }

    // Fallback: search supported games catalog (non-installed games with translation packages)
    if (m_coreBridge) {
        const QVariantList catalog = m_coreBridge->allSupportedGames();
        for (const auto& entry : catalog) {
            QVariantMap map = entry.toMap();
            if (map.value("id").toString() == id || map.value("steamAppId").toString() == id) {
                return map;
            }
        }
    }

    return {};
}

QVariantMap GameService::getGameDetails(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getGameDetails");

    // Contributors: prefer CDN package detail (authoritative), fallback to local
    QVariantList contributors;
    if (m_manifestSync && m_manifestSync->hasPackageDetail(gameId)) {
        const QVariantMap detail = m_manifestSync->getPackageDetail(gameId);
        contributors = detail.value(QStringLiteral("contributors")).toList();
    }
    if (contributors.isEmpty() && m_coreBridge) {
        auto pkg = m_coreBridge->getPackageForGame(gameId);
        if (pkg)
            contributors = pkg->contributors;
    }

    // Install notes from local package manifest
    QString installNotes;
    if (m_coreBridge)
        installNotes = m_coreBridge->getInstallNotesForGame(gameId);

    return QVariantMap{
        {"contributors",  contributors},
        {"installNotes",  installNotes},
        {"isUnityGame",   false},
        {"runtimeNeeded", false}
    };
}

void GameService::fetchSteamDetails(const QString& steamAppId)
{
    m_steamDetails->fetchDetails(steamAppId);
}

QVariantMap GameService::getSteamDetails(const QString& steamAppId)
{
    return m_steamDetails->getDetails(steamAppId);
}

QVariantList GameService::supportedGames() const
{
    MAKINE_ZONE_NAMED("GameService::supportedGames");
    if (m_supportedCacheValid && !m_supportedGamesCache.isEmpty()) {
        return m_supportedGamesCache;
    }

    // Primary: remote catalog from ManifestSyncService (GitHub index.json)
    // Fallback: local catalog from CoreBridge → LocalPackageManager
    if (m_manifestSync && m_manifestSync->catalogCount() > 0) {
        m_supportedGamesCache = m_manifestSync->catalog();
    } else if (m_coreBridge) {
        m_supportedGamesCache = m_coreBridge->allSupportedGames();
    } else {
        return {};
    }
    qCDebug(lcGameService) << "supportedGames() catalog loaded:" << m_supportedGamesCache.size() << "items";

    // Enrich each entry with local state using O(1) hash lookups
    for (int i = 0; i < m_supportedGamesCache.size(); ++i) {
        QVariantMap entry = m_supportedGamesCache[i].toMap();
        const QString steamAppId = entry[QStringLiteral("steamAppId")].toString();

        // Preserve catalog translation source before game source overwrites it
        if (entry.contains(QStringLiteral("source")))
            entry[QStringLiteral("translationSource")] = entry[QStringLiteral("source")];

        // Install status from local game list (O(1) via m_steamAppIdToIndex)
        auto gameIt = m_steamAppIdToIndex.constFind(steamAppId);
        if (gameIt != m_steamAppIdToIndex.constEnd()) {
            const auto& game = m_games[gameIt.value()];
            entry[QStringLiteral("isInstalled")] = game.isInstalled;
            entry[QStringLiteral("installPath")] = game.installPath;
            entry[QStringLiteral("id")] = game.id;
            entry[QStringLiteral("gameSource")] = game.source;
            entry[QStringLiteral("engine")] = game.engine;
        } else {
            entry[QStringLiteral("isInstalled")] = false;
            entry[QStringLiteral("id")] = steamAppId;
        }

        // Package installed status (cached)
        auto cacheIt = m_packageInstalledCache.constFind(steamAppId);
        if (cacheIt != m_packageInstalledCache.constEnd()) {
            entry[QStringLiteral("packageInstalled")] = *cacheIt;
        } else if (m_coreBridge) {
            bool installed = m_coreBridge->isPackageInstalled(steamAppId);
            m_packageInstalledCache[steamAppId] = installed;
            entry[QStringLiteral("packageInstalled")] = installed;
        }

        m_supportedGamesCache[i] = entry;
    }

    m_supportedCacheValid = true;

    // Populate the QAbstractListModel from the enriched cache
    m_supportedGamesModel->resetFromCatalog(m_supportedGamesCache);

    return m_supportedGamesCache;
}

int GameService::installedTranslationCount() const
{
    // Reuse the already-populated installed translations cache.
    // The old path re-iterated m_games calling isPackageInstalled() each time,
    // bypassing the cache. installedTranslations() populates the cache once.
    return installedTranslations().count();
}

int GameService::outdatedPatchCount() const
{
    if (!m_installedCacheValid)
        installedTranslations();  // populates cache + m_outdatedPatchCount
    return m_outdatedPatchCount;
}

QVariantList GameService::installedTranslations() const
{
    MAKINE_ZONE_NAMED("GameService::installedTranslations");
    if (m_installedCacheValid)
        return m_installedTranslationsCache;

    m_installedTranslationsCache.clear();
    m_outdatedPatchCount = 0;
    if (!m_coreBridge) {
        m_installedCacheValid = true;
        return m_installedTranslationsCache;
    }

    for (const auto& game : m_games) {
        if (!m_coreBridge->isPackageInstalled(game.id))
            continue;

        QVariantMap entry = game.toVariantMap();
        entry["packageInstalled"] = true;
        const bool hasUpdate = m_coreBridge->hasTranslationUpdate(game.id);
        entry["hasUpdate"] = hasUpdate;
        if (hasUpdate) ++m_outdatedPatchCount;

        auto pkg = m_coreBridge->getPackageForGame(game.id);
        if (pkg) {
            entry["version"] = pkg->version;
        }

        m_installedTranslationsCache.append(entry);
    }

    m_installedCacheValid = true;
    return m_installedTranslationsCache;
}

QVariantList GameService::library() const
{
    MAKINE_ZONE_NAMED("GameService::library");
    QHash<QString, QString> external;
    for (const QVariant& v : supportedGames()) {
        const QVariantMap m = v.toMap();
        const QString url = m.value(QStringLiteral("externalUrl")).toString();
        if (!url.isEmpty())
            external.insert(m.value(QStringLiteral("steamAppId")).toString(), url);
    }
    QHash<QString, QVariantMap> installed;
    for (const QVariant& v : installedTranslations()) {
        const QVariantMap m = v.toMap();
        installed.insert(m.value(QStringLiteral("id")).toString(), m);
    }

    QList<QVariantMap> rows;
    for (const QVariant& v : games()) {
        QVariantMap m = v.toMap();
        const QString id = m.value(QStringLiteral("id")).toString();
        const QString steamId = m.value(QStringLiteral("steamAppId")).toString();
        const QString appId = steamId.isEmpty() ? id : steamId;
        QString status = QStringLiteral("ready");
        if (const auto it = installed.constFind(id); it != installed.cend()) {
            status = it->value(QStringLiteral("hasUpdate")).toBool() ? QStringLiteral("update")
                                                                      : QStringLiteral("installed");
            m[QStringLiteral("version")] = it->value(QStringLiteral("version"));
        } else if (const auto ext = external.constFind(appId); ext != external.cend()) {
            status = QStringLiteral("external");
            m[QStringLiteral("externalUrl")] = *ext;
        }
        m[QStringLiteral("status")] = status;
        rows << m;
    }

    const auto rank = [](const QString& s) {
        return s == QLatin1String("update") ? 0 : s == QLatin1String("ready") ? 1
             : s == QLatin1String("installed") ? 2 : 3;
    };
    std::stable_sort(rows.begin(), rows.end(), [&rank](const QVariantMap& a, const QVariantMap& b) {
        const int ra = rank(a.value(QStringLiteral("status")).toString());
        const int rb = rank(b.value(QStringLiteral("status")).toString());
        if (ra != rb) return ra < rb;
        return QString::localeAwareCompare(a.value(QStringLiteral("name")).toString(),
                                           b.value(QStringLiteral("name")).toString()) < 0;
    });
    QVariantList out;
    out.reserve(rows.size());
    for (const QVariantMap& m : rows) out << m;
    return out;
}

void GameService::saveCachedGames()
{
    // Snapshot the game list (COW — cheap copy) and serialize in background
    QList<GameInfo> gamesCopy = m_games;
    const QString cacheDir = AppPaths::cacheDir();
    const QString cachePath = AppPaths::gamesCacheFile();

    (void)QtConcurrent::run([gamesCopy = std::move(gamesCopy), cacheDir, cachePath]() {
        MAKINE_THREAD_NAME("Worker-GamesCache");
        MAKINE_ZONE_NAMED("saveCachedGames (async)");

        try {
            QDir().mkpath(cacheDir);

            QJsonArray array;
            for (const auto& game : gamesCopy) {
                QJsonObject obj;
                obj["id"] = game.id;
                obj["name"] = game.name;
                obj["installPath"] = game.installPath;
                obj["steamAppId"] = game.steamAppId;
                obj["source"] = game.source;
                obj["engine"] = game.engine;
                obj["isVerified"] = game.isVerified;
                obj["isInstalled"] = game.isInstalled;
                obj["hasTranslation"] = game.hasTranslation;
                array.append(obj);
            }

            // Atomic write: QSaveFile writes to a sibling temp file and renames
            // on commit, so a crash mid-write leaves the previous cache intact
            // instead of producing a 0-byte file that fails to parse next launch.
            QSaveFile file(cachePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
                if (!file.commit())
                    qCWarning(lcGameService) << "Failed to commit games cache:" << file.errorString();
            } else {
                qCWarning(lcGameService) << "Failed to open games cache for write:" << file.errorString();
            }
        } catch (const std::exception& e) {
            qCWarning(lcGameService) << "Failed to save games cache:" << e.what();
        }
    });
}

void GameService::invalidateGameListCache()
{
    m_cacheValid = false;
    m_gamesCache.clear();
}

void GameService::invalidateTranslationCache()
{
    m_translationCacheValid = false;
    m_installedCacheValid = false;
    m_translationGamesCache.clear();
    m_installedTranslationsCache.clear();
}

void GameService::invalidateSupportedCache()
{
    m_supportedCacheValid = false;
    m_supportedGamesCache.clear();
}

void GameService::invalidateAllCaches()
{
    invalidateGameListCache();
    invalidateTranslationCache();
    invalidateSupportedCache();
}

void GameService::rebuildCache()
{
    MAKINE_ZONE_NAMED("GameService::rebuildCache");
    // Rebuild game ID and steamAppId indexes
    m_gameIdToIndex.clear();
    m_steamAppIdToIndex.clear();
    m_gameIdToIndex.reserve(m_games.count());
    m_steamAppIdToIndex.reserve(m_games.count());

    for (int i = 0; i < m_games.count(); ++i) {
        m_gameIdToIndex[m_games[i].id] = i;
        if (!m_games[i].steamAppId.isEmpty())
            m_steamAppIdToIndex[m_games[i].steamAppId] = i;
    }

    // Invalidate all QVariantList caches (preserves packageInstalledCache
    // which may have been pre-warmed in background or still be valid)
    invalidateAllCaches();
}

void GameService::ensureSupportedGamesCache()
{
    MAKINE_ZONE_NAMED("GameService::ensureSupportedGamesCache");
    if (!m_supportedGamesCache.isEmpty()) return;

    // Pre-warm by calling supportedGames() which populates m_supportedGamesCache
    supportedGames();
}

QVariantList GameService::getVariants(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getVariants");
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantsForGame(gameId);
}

QString GameService::getVariantType(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getVariantType");
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantTypeForGame(gameId);
}

QString GameService::getInstallNotes(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getInstallNotes");
    if (!m_coreBridge) return {};
    return m_coreBridge->getInstallNotesForGame(gameId);
}

QVariantMap GameService::getPostInstallGuidance(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getPostInstallGuidance");
    QVariantMap out{
        {QStringLiteral("notes"),          QString()},
        {QStringLiteral("languageSlot"),   QString()},
        {QStringLiteral("files"),          QStringList()},
        {QStringLiteral("actionRequired"), false},
    };
    if (!m_coreBridge) return out;

    const QString notes = m_coreBridge->getInstallNotesForGame(gameId);

    // Entries beginning with '_' are the installer's own bookkeeping, not game
    // files — checkPatchIntegrity() filters them the same way.
    QStringList written;
    if (const auto info = m_coreBridge->getInstalledInfo(gameId)) {
        written.reserve(info->installedFiles.size());
        for (const QString& rel : info->installedFiles) {
            if (!rel.startsWith(QLatin1Char('_'))) written.append(rel);
        }
    }

    const QString slot = makine::postinstall::languageSlotFromPaths(written);
    out[QStringLiteral("notes")]          = notes;
    out[QStringLiteral("languageSlot")]   = slot;
    out[QStringLiteral("files")]          = written;
    out[QStringLiteral("actionRequired")] =
        makine::postinstall::shouldShowAfterInstall(notes, slot);
    return out;
}

QVariantList GameService::getInstallOptions(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getInstallOptions");
    if (!m_coreBridge) return {};
    return m_coreBridge->getInstallOptionsForGame(gameId);
}

QString GameService::getSpecialDialog(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getSpecialDialog");
    if (!m_coreBridge) return {};
    return m_coreBridge->getSpecialDialogForGame(gameId);
}

QVariantList GameService::getVariantInstallOptions(const QString& gameId, const QString& variant)
{
    MAKINE_ZONE_NAMED("GameService::getVariantInstallOptions");
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantInstallOptionsForGame(gameId, variant);
}

QString GameService::getVariantSpecialDialog(const QString& gameId, const QString& variant)
{
    MAKINE_ZONE_NAMED("GameService::getVariantSpecialDialog");
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantSpecialDialogForGame(gameId, variant);
}

void GameService::cancelInstallation()
{
    if (m_installingGameId.isEmpty()) return;
    // The slot stays taken until the install reports back. Clearing it here
    // dropped the cancelled install's result — so its rollback never ran —
    // and a cancel during the backup let the install start anyway.
    m_installCancelRequested = true;
    if (m_coreBridge)
        m_coreBridge->cancelInstall();
}

bool GameService::isInstalling(const QString& gameId) const
{
    return !gameId.isEmpty() && gameId == m_installingGameId;
}

QString GameService::installConsole(const QString& gameId) const
{
    if (gameId.isEmpty() || gameId != m_installConsoleGameId)
        return {};
    // A trailing "\r" is a return still waiting for its next character.
    return m_installConsole.endsWith(QLatin1Char('\r')) ? m_installConsole.chopped(1)
                                                        : m_installConsole;
}

void GameService::sendInstallInput(const QString& text)
{
    if (m_installingGameId.isEmpty() || !m_coreBridge)
        return;
    // A pipe does not echo; show what was typed where a console would.
    processconsole::appendTo(m_installConsole, text + QLatin1Char('\n'), kInstallConsoleCap);
    emit translationInstallOutput(m_installingGameId);
    m_coreBridge->sendProcessInput(text);
}

void GameService::endCancelledInstall(const QString& gameId)
{
    m_installTimeoutTimer->stop();
    m_installingGameId.clear();
    m_installCancelRequested = false;
    emit translationInstallCompleted(gameId, false, tr("Kurulum iptal edildi"));
}

void GameService::installTranslation(const QString& gameId, const QString& variant,
                                      const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("GameService::installTranslation");
    installPackageCommon(gameId, variant, selectedOptions, InstallMode::Install);
}

void GameService::updateTranslation(const QString& gameId, const QString& variant,
                                     const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("GameService::updateTranslation");
    installPackageCommon(gameId, variant, selectedOptions, InstallMode::Update);
}

void GameService::installPackageCommon(const QString& gameId, const QString& variant,
                                        const QStringList& selectedOptions, InstallMode mode)
{
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Uygulama başlatılıyor, lütfen bekleyin"));
        return;
    }

    if (!m_installingGameId.isEmpty()) {
        emit translationInstallCompleted(gameId, false,
            tr("Zaten bir kurulum devam ediyor"));
        return;
    }

    // Same the other way: an uninstall, or the restore of a failed install,
    // still has the crash journal and may be writing into this game.
    if (const BackupManager* bm = BackupManager::instance();
        !m_uninstalling.isEmpty() || (bm && bm->isRestoring())) {
        emit translationInstallCompleted(gameId, false,
            tr("Bir yamanın kaldırılması ya da geri yüklemesi sürüyor. O bitince tekrar deneyin."));
        return;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationInstallCompleted(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    const GameInfo& game = m_games[*it];

    auto pkg = m_coreBridge->getPackageForGame(gameId);
    if (!pkg.has_value()) {
        if (mode == InstallMode::Install) {
            qCWarning(lcGameService) << "GameService::installPackageCommon: No package found for" << gameId
                       << "- hasTranslationPackage:" << m_coreBridge->hasTranslationPackage(gameId);
        }
        emit translationInstallCompleted(gameId, false,
            tr("Bu oyun için çeviri paketi bulunamadı"));
        return;
    }

    if (mode == InstallMode::Install) {
        qCInfo(lcGameService) << "GameService::installPackageCommon(Install): Starting for" << gameId
                << "pkg:" << pkg->packageId << "v" << pkg->version;
    }

    if (game.installPath.isEmpty() || !QDir(game.installPath).exists()) {
        emit translationInstallCompleted(gameId, false,
            tr("Oyun klasörü bulunamadı: %1").arg(game.installPath));
        return;
    }

    // Ghost-folder guard: Steam/Epic create the install directory the moment a
    // download is queued, so exists() above passes even for a game whose files
    // never arrived (download cancelled, disk full, game moved away). Patching an
    // empty folder only yields a cryptic engine error deep in the copy step, so
    // catch the partial install here with a cause the user can act on.
    if (QDir(game.installPath).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        emit translationInstallCompleted(gameId, false,
            tr("Oyun klasörü boş: %1\n\n"
               "Oyun mağazada hâlâ indiriliyor veya kaldırılmış olabilir. "
               "Oyunun tamamen kurulduğundan emin olup tekrar deneyin.").arg(game.installPath));
        return;
    }

    if (mode == InstallMode::Update) {
        // No backup to go back to: an in-place recipe cannot be restored and
        // re-applied, so it is installed over (its steps see the file already
        // patched). Anything else stays an update. The Install fallback took no
        // new backup either (the package is installed, so the backup step is
        // skipped) and lost what the update does: files the old version added
        // that the new one no longer ships stayed in the game, off the record.
        // Every package that only adds files has no backup.
        BackupManager* bm = BackupManager::instance();
        if (bm && !bm->hasBackup(gameId) && m_coreBridge->recipeWritesInPlace(gameId)) {
            qCWarning(lcGameService) << "No backup found for" << gameId
                << "— in-place recipe, installing over the current version";
            mode = InstallMode::Install;
        }
    }

    if (mode == InstallMode::Update && m_coreBridge->recipeWritesInPlace(gameId)) {
        // In-place steps (patchBytes, forgeReimport, xdelta, vpatch) edit files
        // the installed version already edited: a patch finds nothing left to
        // replace, and the original is only in the first install's backup, which
        // an update does not refresh. Go back to the originals, then install.
        reinstallForUpdate(gameId, variant, selectedOptions);
        return;
    }

    if (mode == InstallMode::Install) {
        // Install mode: consume anti-cheat acknowledgement.
        // InstallFlowController already handled the warning dialog and called
        // acknowledgeAntiCheat() before reaching here, so we just clear the flag.
        m_antiCheatAcknowledged.remove(gameId);
    }

    // Reserve install slot early to prevent double-install
    m_installingGameId = gameId;
    if (const auto over = m_coreBridge->getInstalledInfo(gameId))
        m_installOverRecord = over->installedFiles + over->addedFiles;
    else
        m_installOverRecord.reset();
    m_installCancelRequested = false;
    m_installConsole.clear();
    m_installConsoleGameId = gameId;
    m_installTimeoutTimer->start();
    emit translationInstallStarted(gameId);
    emit translationInstallProgress(gameId, 0.0, tr("Oyun durumu kontrol ediliyor..."));

    // Async game-running check (avoids blocking the main thread with tasklist)
    const QString installPath = game.installPath;
    const QString runningMsg = (mode == InstallMode::Install)
        ? tr("Bu oyun şu anda çalışıyor (%1). Çeviriyi kurmak için oyunu kapatın.")
        : tr("Bu oyun şu anda çalışıyor (%1). Güncelleme için oyunu kapatın.");

    // An update that writes over a game file for the first time (v1 left
    // data/b.pak alone, v2 ships it) records it as replaced, and uninstall and
    // a failed update's rollback leave it to the restore — of the backup the
    // first install took, which does not hold it: "Yama başarıyla kaldırıldı"
    // with the Turkish b.pak still in the game. Its original goes into that
    // backup first, in the preflight below.
    QString topUpDir;
    QStringList topUp;
    if (mode == InstallMode::Update) {
        BackupManager* bm = BackupManager::instance();
        const auto record = m_coreBridge->getInstalledInfo(gameId);
        if (bm && record)
            topUpDir = bm->getLatestBackup(gameId).value(QStringLiteral("backupPath")).toString();
        if (!topUpDir.isEmpty()) {
            topUp = steprules::notOnRecord(
                m_coreBridge->getPackageFileList(gameId, variant, installPath),
                record->installedFiles + record->addedFiles + record->replacedFiles);
        }
    }

    struct Preflight {
        QString runningExe;
        bool writable = true;
        bool declined = false;   // the user said no to the write-access prompt
        QString topUpFailed;     // the original that could not go into the backup
    };
    auto* watcher = new QFutureWatcher<Preflight>(this);
    connect(watcher, &QFutureWatcher<Preflight>::finished, this,
        [this, watcher, gameId, installPath, variant, selectedOptions, pkg, mode, runningMsg]() {
            const Preflight pre = watcher->result();
            const QString runningExe = pre.runningExe;
            watcher->deleteLater();
            if (m_installCancelRequested) {
                endCancelledInstall(gameId);
                return;
            }

            if (!runningExe.isEmpty()) {
                m_installTimeoutTimer->stop();
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false, runningMsg.arg(runningExe));
                return;
            }

            // A game under C:\Program Files (EA, Ubisoft, repacks) was asked for
            // write access in the preflight: one UAC prompt, after which the
            // copy, the in-place patch steps, a later restore and uninstall all
            // take the normal path. Before, access denied read as "file
            // locked", the elevated path never ran, and install, rollback and
            // uninstall all failed — 10 games in two days of 0.1.5.
            if (!pre.writable) {
                const bool helper = ElevatedOps::available();
                if (helper && !pre.declined) {
                    // The grant itself failed; the copy pass can still replay
                    // what it cannot write through the helper, file by file.
                    qCWarning(lcGameService)
                        << "write access not granted, falling back to elevated copies:"
                        << installPath;
                } else {
                    qCWarning(lcGameService) << "game directory not writable:" << installPath
                                             << (pre.declined ? "(prompt declined)" : "(no helper)");
                    m_installTimeoutTimer->stop();
                    m_installingGameId.clear();
                    const QString msg = pre.declined
                        ? tr("Bu oyun yönetici izni gereken bir klasörde kurulu:\n%1\n\n"
                             "Çeviriyi kurabilmek için açılan izin penceresinde \"Evet\" "
                             "demeniz gerekiyor; izin yalnız bu oyunun klasörü için "
                             "verilir. Tekrar deneyip onaylayın.").arg(installPath)
                        : tr("Oyun klasörüne yazma izni yok:\n%1\n\n"
                             "Bu klasör yönetici izni gerektiriyor "
                             "(örn. C:\\Program Files altındaki kurulumlar) ve "
                             "gerekli yardımcı bileşen bulunamadı.\n\n"
                             "Çözüm: uygulamayı Microsoft Store üzerinden "
                             "güncelleyin, ya da oyunu Program Files dışında bir "
                             "klasöre taşıyın.").arg(installPath);
                    reportOperationFailure("install", gameId,
                        QStringLiteral("game directory not writable (%1): %2")
                            .arg(pre.declined ? QStringLiteral("prompt declined")
                                              : QStringLiteral("no elevate helper"),
                                 installPath));
                    emit translationInstallCompleted(gameId, false, msg);
                    return;
                }
            }

            if (mode == InstallMode::Update) {
                if (!pre.topUpFailed.isEmpty()) {
                    qCWarning(lcGameService) << "Could not add" << pre.topUpFailed
                                             << "to the backup of" << gameId << "- update not started";
                    m_installTimeoutTimer->stop();
                    m_installingGameId.clear();
                    emit translationInstallCompleted(gameId, false,
                        tr("Güncelleme başlamadı: yeni sürümün ilk kez değiştireceği oyun "
                           "dosyası yedeğe eklenemedi (%1). Oyun dosyalarına dokunulmadı.\n"
                           "Oyunu ve mağaza uygulamasını kapatıp, diskte yer olduğundan "
                           "emin olup tekrar deneyin.").arg(pre.topUpFailed));
                    return;
                }
                // No backup step for updates — go straight to update
                qCDebug(lcGameService) << "Updating translation for" << gameId
                         << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                         << "options:" << selectedOptions
                         << "path:" << installPath;
                m_coreBridge->updatePackage(gameId, installPath, variant, selectedOptions);
                return;
            }

            // Install mode: create selective backup first, then install
            // Guard: if package is already installed, files are patched — skip backup
            // to protect the original backups from being overwritten with patched files
            const bool alreadyInstalled = m_coreBridge->isPackageInstalled(gameId);
            if (alreadyInstalled) {
                qCWarning(lcGameService) << "Package already installed for" << gameId
                                         << "- skipping backup to protect originals";
            }

            BackupManager* bm = BackupManager::instance();
            // installPath matters: it lets the wrapper-stripping check verify a
            // package's top-level folder against the real game install, so the
            // backup list matches the files the install will actually overwrite.
            QStringList filesToOverwrite =
                m_coreBridge->getPackageFileList(gameId, variant, installPath);

            // An empty list means "could not look", never "nothing to back up".
            // getPackageFileList enumerates what the install will WRITE, and a
            // package that writes nothing is not a package. The else-branch
            // below read the two the same way and installed anyway: the patch
            // overwrote originals with no backup behind it, and uninstall then
            // refused for good ("Yama kaldırılamadı … eski haline
            // döndürülemiyor"), leaving the user to verify game files by hand.
            //
            // Every such report came back with records=0 — Sentry, 30 days,
            // 2026-09-20: 376 events across Resident Evil 4, Starfield, Kenshi,
            // Red Dead Redemption 2, Sir Brante, Spider-Man 2, It Takes Two.
            // All plain overlay packages, all first installs.
            //
            // d401dd8 fixed one way the list came back empty (the backup list
            // and the install resolved the package differently). This closes
            // the shape itself: whatever the reason, a patch never goes on top
            // of originals we did not manage to record.
            // A recipe that is only the patch's own program writes where no list
            // can see in advance. It may go in without a backup of ours when it
            // also names the program that undoes it: uninstall starts that one
            // (startUninstallPrograms). Without it, nothing could take it back.
            const auto ownProgram = m_coreBridge->ownProgramRecipe(gameId);
            if (ownProgram == LocalPackageManager::OwnProgram::WithUndo && filesToOverwrite.isEmpty())
                qCInfo(lcGameService) << "Recipe of" << gameId
                                      << "is its own program with an undo - no backup of ours";
            if (bm && !alreadyInstalled && filesToOverwrite.isEmpty()
                && ownProgram != LocalPackageManager::OwnProgram::WithUndo) {
                // Warning, not critical: a critical log is a Sentry event of
                // its own, and the report below already is one.
                qCWarning(lcGameService) << "Backup file list empty for" << gameId
                    << "— refusing to install without a backup";
                m_installTimeoutTimer->stop();
                m_installingGameId.clear();

                // A recipe step this build does not know writes nothing it
                // could list, so the list comes back empty — and "download it
                // again" cannot help. 0.1.5 met D1's forgeReimport/xdelta steps
                // this way: ~800 Sentry events, every one a retry that could
                // never succeed. Say what is true; the wording files it as a
                // capability gap (counted, not sent).
                if (const QStringList unknown = m_coreBridge->unknownRecipeActions(gameId);
                    !unknown.isEmpty()) {
                    const QString msg = tr("Bu yama, uygulamanın şu an desteklemediği bir "
                                           "kurulum adımı içeriyor (%1).\n"
                                           "Uygulamayı güncelleyip tekrar deneyin.")
                                            .arg(unknown.join(QStringLiteral(", ")));
                    reportOperationFailure("install", gameId, msg);
                    emit translationInstallCompleted(gameId, false, msg);
                    return;
                }

                // Not "download it again": the recipe itself is incomplete, and
                // every retry would end here. 56 attempts in eight minutes on
                // Heavy Rain (2026-10-20) were this.
                if (ownProgram == LocalPackageManager::OwnProgram::WithoutUndo) {
                    reportOperationFailure("install", gameId,
                        QStringLiteral("own-program recipe names no uninstall program; "
                                       "refusing without a backup"));
                    emit translationInstallCompleted(gameId, false,
                        tr("Bu yama kendi programıyla kuruluyor, ama onu geri alacak "
                           "kaldırma programı tanımlanmamış. Oyun dosyalarına dokunulmadı: "
                           "değişiklikler yedeklenemez ve geri alınamazdı.\n"
                           "Bu bir katalog hatası ve bildirildi; düzeltildiğinde yama kurulabilecek."));
                    return;
                }

                reportOperationFailure("install", gameId,
                    QStringLiteral("backup file list empty; refusing to patch without a backup"));
                emit translationInstallCompleted(gameId, false,
                    tr("Yama kurulmadı: yedeklenecek dosyalar belirlenemedi, bu yüzden "
                       "oyun dosyalarının üzerine yedek almadan yazılmayacak.\n"
                       "Yamayı yeniden indirip tekrar deneyin."));
                return;
            }

            if (bm && !filesToOverwrite.isEmpty() && !alreadyInstalled) {
                emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));

                connect(bm, &BackupManager::selectiveBackupCompleted, this,
                    [this, gameId, installPath, variant, selectedOptions](const QString& backupGameId, bool success) {
                        if (backupGameId != gameId) return;
                        if (m_installCancelRequested) {
                            endCancelledInstall(gameId);
                            return;
                        }
                        if (!success) {
                            // Without a backup we cannot offer safe rollback, and the patch
                            // would silently destroy the originals on the next uninstall.
                            qCWarning(lcGameService) << "Selective backup failed for" << gameId
                                                      << "— aborting install to protect originals";
                            m_installTimeoutTimer->stop();
                            m_installingGameId.clear();
                            // BackupManager already worked out WHY; repeating its
                            // reason beats guessing at disk space for a file that
                            // was simply held open by a running game.
                            BackupManager* backup = BackupManager::instance();
                            const QString why = backup ? backup->lastError() : QString();
                            emit translationInstallCompleted(gameId, false,
                                why.isEmpty()
                                    ? tr("Yedek oluşturulamadı, kurulum iptal edildi. "
                                         "Disk alanı veya yazma iznini kontrol edin.")
                                    : tr("Kurulum iptal edildi. %1").arg(why));
                            return;
                        }
                        qCDebug(lcGameService) << "Installing translation for" << gameId
                                 << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                                 << "options:" << selectedOptions
                                 << "path:" << installPath;
                        m_coreBridge->installPackage(gameId, installPath, variant, selectedOptions);
                    }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

                const QString patchVer = pkg.has_value() ? pkg->version : QString();
                QString storeVer;
                bm->createSelectiveBackupAsync(gameId, m_games[m_gameIdToIndex.value(gameId)].name,
                                                installPath, filesToOverwrite, storeVer, patchVer);
            } else {
                qCDebug(lcGameService) << "Installing translation for" << gameId
                         << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                         << "options:" << selectedOptions
                         << "path:" << installPath;
                m_coreBridge->installPackage(gameId, installPath, variant, selectedOptions);
            }
        });

    watcher->setFuture(QtConcurrent::run([installPath, topUpDir, topUp]() -> Preflight {
        Preflight out;
        // By image path: the root-folder *.exe names missed every game whose
        // exe sits in bin/ (Watch_Dogs copied 4 of 6 files, then failed on the
        // DLL and archive the running game held).
        out.runningExe = ProcessScanner::runningExeUnder(installPath);
        if (!out.runningExe.isEmpty())
            return out;
        // Here, off the GUI thread: the UAC prompt blocks until answered.
        if (ElevatedOps::available()) {
            out.writable = ElevatedOps::ensureWritable(installPath);
            out.declined = !out.writable && ElevatedOps::lastRunDeclined();
        } else {
            out.writable = ElevatedOps::canWrite(installPath);
        }
        // The update's first overwrites into the backup (topUp above). Through
        // a file beside the backup folder, renamed in when complete: a copy cut
        // short by a crash must not sit in the backup as the original.
        const QString part = topUpDir + QStringLiteral(".part");
        for (const QString& rel : topUp) {
            const QString src = QDir::cleanPath(installPath + QLatin1Char('/') + rel);
            const QString dst = QDir::cleanPath(topUpDir + QLatin1Char('/') + rel);
            if (!security::isPathContained(installPath, src) || !rawfile::exists(src)
                || rawfile::exists(dst))
                continue;
            QFile::remove(part);
            if (!QDir().mkpath(QFileInfo(dst).absolutePath()) || !QFile::copy(src, part)
                || !QFile::rename(part, dst)) {
                QFile::remove(part);
                out.topUpFailed = rel;
                break;
            }
        }
        return out;
    }));
}

void GameService::uninstallTranslation(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::uninstallTranslation");
    if (!m_coreBridge) {
        emit translationUninstalled(gameId, false, tr("Uygulama başlatılıyor, lütfen bekleyin"));
        return;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationUninstalled(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    // One uninstall per game at a time. The button stays clickable through
    // the restore, and a second click ran a second uninstall next to the
    // first: its restore was refused as busy, the flow went on to delete
    // files the first restore was still writing, and the first one then
    // finished against a record that was already gone. Cleared when
    // translationUninstalled is emitted (see the constructor).
    if (m_uninstalling.contains(gameId)) {
        qCInfo(lcGameService) << "Uninstall already running for" << gameId << "- ignoring repeat";
        return;
    }
    // An install still writing — this game's update, or another game's — and
    // this restore and delete would run beside it: one crash journal holds
    // one operation, so each committed the other's away, and on the same
    // game the two wrote over each other. "Yamayı taşı" stays clickable
    // during an update.
    if (!m_installingGameId.isEmpty()) {
        emit translationUninstalled(gameId, false,
            tr("Şu anda bir yama kuruluyor. Kurulum bitince tekrar deneyin."));
        return;
    }
    m_uninstalling.insert(gameId);
    m_storeRepairStarted.remove(gameId);

    // A game under C:\Program Files: restore and delete both need write access.
    // Ask for it once, before anything is touched, and off the GUI thread —
    // the UAC prompt blocks until it is answered.
    const QString installPath = m_games[*it].installPath;
    if (ElevatedOps::available() && QDir(installPath).exists()
        && !ElevatedOps::canWrite(installPath)) {
        using Access = std::pair<bool, bool>;  // {writable, declined}
        auto* watcher = new QFutureWatcher<Access>(this);
        connect(watcher, &QFutureWatcher<Access>::finished, this, [this, watcher, gameId]() {
            const bool declined = watcher->result().second;
            watcher->deleteLater();
            if (!declined) {
                // Granted, or the helper failed: the normal path still knows
                // how to report what it could not restore or remove.
                uninstallTranslationNow(gameId);
                return;
            }
            reportOperationFailure("uninstall", gameId,
                QStringLiteral("game directory not writable (prompt declined)"));
            emit translationUninstalled(gameId, false,
                tr("Bu oyun yönetici izni gereken bir klasörde kurulu.\n\n"
                   "Çeviriyi kaldırabilmek için açılan izin penceresinde \"Evet\" demeniz "
                   "gerekiyor; izin yalnız bu oyunun klasörü için verilir. Tekrar deneyip onaylayın."));
        });
        watcher->setFuture(QtConcurrent::run([installPath]() {
            const bool ok = ElevatedOps::ensureWritable(installPath);
            return Access{ok, !ok && ElevatedOps::lastRunDeclined()};
        }));
        return;
    }
    uninstallTranslationNow(gameId);
}

void GameService::uninstallTranslationNow(const QString& gameId)
{
    // Looked up again: a library scan may have reloaded m_games while the
    // write-access prompt was open.
    auto it = m_gameIdToIndex.constFind(gameId);
    if (!m_coreBridge || it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationUninstalled(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    const GameInfo& game = m_games[*it];

    // Where the patch is. The scan's folder, unless it is gone and the folder
    // the install recorded still exists (a stale library entry). Restore and
    // delete must agree on it; they used to take one each.
    const auto installed = m_coreBridge->getInstalledInfo(gameId);
    const QString recorded = installed.has_value() ? installed->gamePath : QString();
    const QString gamePath = (QDir(game.installPath).exists() || recorded.isEmpty())
        ? game.installPath : recorded;

    // The game itself is gone: deleted, moved, or removed from its store.
    // Nothing to restore into and nothing to delete, so drop the record.
    // Asking the backup to restore into a missing folder failed every time,
    // and when the patch had replaced originals that refused the uninstall
    // for good (Mass Effect LE, "Hedef klasör bulunamadı").
    if (!QDir(gamePath).exists()) {
        qCWarning(lcGameService) << "Game folder gone for" << gameId
                                 << "- dropping the install record:" << gamePath;
        finalizeUninstall(gameId, gamePath);
        return;
    }

    // A running game holds its .dll/.pak/.dat files; the restore failed on
    // exactly those and the uninstall stopped half way (Watch_Dogs, Far Cry 3,
    // Starfield, RDR2 — ~19 users, Sentry 2026-10).
    if (const QString exe = ProcessScanner::runningExeUnder(gamePath); !exe.isEmpty()) {
        emit translationUninstalled(gameId, false,
            tr("Bu oyun şu anda çalışıyor (%1). Çeviriyi kaldırmak için oyunu kapatın.").arg(exe));
        return;
    }

    BackupManager* bm = BackupManager::instance();

    // Another restore still running, most often a failed install rolling
    // back. Ours would be refused as busy, and the flow below used to take
    // that refusal for "no restore needed".
    if (bm && bm->isRestoring()) {
        emit translationUninstalled(gameId, false,
            tr("Önceki geri yükleme işlemi henüz bitmedi. Birkaç saniye sonra tekrar deneyin."));
        return;
    }

    // Restore from backup before uninstall (wait for async restore to complete)
    if (bm && bm->hasBackup(gameId)) {
        auto latest = bm->getLatestBackup(gameId);
        if (!latest.isEmpty() && latest.contains("id")) {
            // Wait for the restore to finish, then proceed with uninstall.
            // Partial restore: refuse to run uninstallPackage afterwards. Removing
            // patched-but-not-restored files would leave the game with holes;
            // surface the reason and let the user run Steam's "Verify Integrity"
            // (or unlock the file manually) instead.
            const QString backupPath = latest.value(QStringLiteral("backupPath")).toString();
            const auto cancelWait = awaitRestore(bm, gameId,
                [this, gameId, gamePath, backupPath]() {
                    m_restoreFailures.remove(gameId);
                    finalizeUninstall(gameId, gamePath, backupPath);
                },
                [this, gameId, gamePath](const QString& reason) {
                    // Refusing forever was a dead end: people retried dozens of
                    // times (uninstall_aborted 105 events / 14 users — Alan Wake,
                    // Starfield, RDR2). The first failure still stops, since a
                    // closed game or a released lock usually lets the next try
                    // through; the second removes the patch and hands what could
                    // not be restored to the store's repair, like a missing backup.
                    if (++m_restoreFailures[gameId] >= 2) {
                        qCWarning(lcGameService) << "Backup restore failed again for" << gameId
                                                 << "— removing the patch, the store repairs the rest";
                        m_restoreFailures.remove(gameId);
                        uninstallWithStoreRepair(gameId, gamePath);
                        return;
                    }
                    qCWarning(lcGameService) << "Backup restore failed for" << gameId
                                              << "— skipping uninstall to avoid mixed state";
                    // Tagged "uninstall", not "uninstall/restore": operation is
                    // a fixed set (download, install, uninstall, backup,
                    // restore, sync, scan, integrity) and a value outside it
                    // splits the grouping. BackupManager already reports the
                    // same signal as "restore" — the reason it matters here is
                    // the consequence, so that is what this message carries.
                    reportOperationFailure("uninstall", gameId,
                        QStringLiteral("restore failed, uninstall aborted to avoid mixed state: %1")
                            .arg(reason));
                    emit translationUninstalled(gameId, false, reason + QStringLiteral("\n\n")
                        + tr("Oyunu ve mağaza uygulamasını kapatıp tekrar deneyin. Yine olmazsa yama "
                             "kaldırılır ve geri yüklenemeyen dosyalar için mağazanın dosya onarımı başlatılır."));
                });
            bool started = bm->restoreBackup(latest["id"].toString(), gamePath);
            if (!started) {
                cancelWait();
                // Restore never started (a record without files, an emptied
                // folder). LocalPackageManager::uninstallPackage skips
                // replacedFiles because it assumes the restore puts the
                // originals back, so the store has to put them back instead.
                if (translationReplacedOriginalFiles(gameId)) {
                    qCWarning(lcGameService) << "Backup restore could not start for" << gameId
                               << "- removing the patch, originals go to the store's repair";
                    uninstallWithStoreRepair(gameId, gamePath);
                    return;
                }
                qCWarning(lcGameService) << "Backup restore could not start for" << gameId
                           << "- install only added files, uninstall is still safe";
                finalizeUninstall(gameId, gamePath);
            }
            return;
        }
    }

    // No usable backup. With no restore pass the overwritten originals stay
    // patched, so the store's repair is what brings them back.
    if (translationReplacedOriginalFiles(gameId)) {
        // Which kind of "no backup": no record at all means the patch went in
        // before this game ever had a backup taken; records that are all
        // invalid mean a backup was made and then lost.
        const int records = bm ? bm->getBackupsForGame(gameId).size() : -1;
        qCWarning(lcGameService) << "No backup available for" << gameId
                   << "(backup records:" << records
                   << ") - removing the patch, originals go to the store's repair";
        uninstallWithStoreRepair(gameId, gamePath);
        return;
    }

    finalizeUninstall(gameId, gamePath);
}

void GameService::uninstallWithStoreRepair(const QString& gameId, const QString& gamePath)
{
    // Refusing here was a dead end. The message sent the user to the store's
    // file check, but the install record stayed, so "Kaldır" refused again
    // after the check too: 34 attempts on Thief, 17 on Resident Evil 7.
    // Remove what the patch added, drop the record, and start the store's
    // repair for the files it overwrote.
    m_storeRepairStarted.insert(gameId);
    finalizeUninstall(gameId, gamePath);
    if (!m_coreBridge->isPackageInstalled(gameId))
        repairGameFiles(gameId);
}

bool GameService::translationReplacedOriginalFiles(const QString& gameId)
{
    if (!m_coreBridge) return false;
    const auto info = m_coreBridge->getInstalledInfo(gameId);
    if (!info.has_value()) return false;
    if (!info->replacedFiles.isEmpty()) return true;
    // vpatch, xdelta, forgeReimport and patchBytes rewrite a game file in
    // place and record it under this marker, not in replacedFiles.
    for (const QString& rel : info->installedFiles)
        if (rel.startsWith(QLatin1String("_vpatch:"))) return true;
    return false;
}

void GameService::checkPatchIntegrity(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::checkPatchIntegrity");
    if (!m_coreBridge) return;

    const auto info = m_coreBridge->getInstalledInfo(gameId);
    if (!info.has_value()) return;          // not installed — nothing to verify

    // The game folder itself going missing is a different problem from the
    // patch being stripped, and needs different advice.
    if (info->gamePath.isEmpty() || !QDir(info->gamePath).exists()) {
        emit patchIntegrityChecked(gameId, false,
            tr("Oyun klasörü bulunamadı. Oyun taşınmış veya kaldırılmış "
               "olabilir. Kütüphaneyi yenileyin."));
        return;
    }

    // Snapshot the file list here, on the main thread. A library scan can reload
    // the catalog underneath us, so the worker must not reach back into it.
    const QString gamePath = info->gamePath;
    QStringList rels;
    rels.reserve(info->installedFiles.size());
    for (const QString& rel : info->installedFiles) {
        if (!rel.startsWith(QLatin1Char('_'))) rels.append(rel);
    }
    if (rels.isEmpty()) {
        emit patchIntegrityChecked(gameId, true, QString());
        return;
    }

    (void)QtConcurrent::run([this, gameId, gamePath, rels]() {
        MAKINE_THREAD_NAME("Worker-Integrity");
        int missing = 0;
        QStringList examples;   // which files: the count alone could not tell
                                // antivirus (.dll/.asi) from the game's own cleanup
        for (const QString& rel : rels) {
            if (!rawfile::exists(QDir::cleanPath(gamePath + QLatin1Char('/') + rel))) {
                ++missing;
                if (examples.size() < 3) examples << rel;
            }
        }
        const int total = rels.size();

        QMetaObject::invokeMethod(this, [this, gameId, missing, total, examples]() {
            if (missing == 0) {
                emit patchIntegrityChecked(gameId, true, QString());
                return;
            }

            // All gone versus some gone points at different causes, so the
            // advice differs: a wholesale removal is a store verification or a
            // game update, a partial one is almost always antivirus taking
            // individual payloads.
            const QString msg = (missing == total)
                ? tr("Yama dosyalarının tamamı oyun klasöründen silinmiş. Bu genellikle "
                     "mağazada \"dosya bütünlüğünü doğrula\" çalıştırıldığında veya oyun "
                     "güncellendiğinde olur. Yamayı yeniden kurun.")
                : tr("Yama dosyalarının %1 tanesi (toplam %2) eksik. Bir güvenlik yazılımı "
                     "dosyaları karantinaya almış olabilir. Yamayı kaldırıp yeniden kurun; "
                     "sorun sürerse Makine Launcher'ı antivirüs dışlamalarına ekleyin.")
                      .arg(missing).arg(total);

            qCWarning(lcGameService) << "Patch integrity failed for" << gameId
                                     << missing << "of" << total << "files missing";

            // System-side by definition: the user did nothing wrong, and this is
            // the signal that tells us how often patches quietly disappear.
            // Once per state and session: the check runs on every page visit,
            // and one install sent nine identical events.
            const QString key = QStringLiteral("%1:%2/%3").arg(gameId).arg(missing).arg(total);
            if (!m_integrityReported.contains(key)) {
                m_integrityReported.insert(key);
                // Through reportOperationFailure for the game tags: called
                // bare, 22 games shared one issue with game.id blank.
                reportOperationFailure(
                    "integrity", gameId,
                    QStringLiteral("installed patch files missing: %1/%2 (e.g. %3)")
                        .arg(missing).arg(total).arg(examples.join(QStringLiteral(", "))));
            }

            emit patchIntegrityChecked(gameId, false, msg);
        }, Qt::QueuedConnection);
    });
}

void GameService::repairGameFiles(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::repairGameFiles");
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit gameRepairStarted(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    const GameInfo& game = m_games[*it];
    const bool isSteam =
        game.source.compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0;

    if (isSteam && !game.steamAppId.isEmpty()) {
        const QUrl url(QStringLiteral("steam://validate/%1").arg(game.steamAppId));
        if (QDesktopServices::openUrl(url)) {
            qCInfo(lcGameService) << "Requested Steam file verification for" << gameId;
            emit gameRepairStarted(gameId, true,
                tr("Steam'de dosya doğrulama başlatıldı. Steam eksik veya değiştirilmiş "
                   "dosyaları indirip oyunu orijinal haline döndürecek. İşlem bitince "
                   "oyunu çalıştırıp kontrol edin."));
            return;
        }
        qCWarning(lcGameService) << "Failed to hand off steam://validate for" << gameId;
    }

    // GOG Galaxy and the Epic launcher expose no verification URL scheme, so
    // the best we can do is tell the user exactly where the button lives.
    emit gameRepairStarted(gameId, false,
        tr("Bu oyun için doğrulama otomatik başlatılamıyor. Oyunu mağaza "
           "uygulamasından onarın:\n"
           "• Steam: Kitaplık > oyuna sağ tık > Özellikler > Yüklü Dosyalar > "
           "Oyun dosyalarının bütünlüğünü doğrula\n"
           "• Epic: Kitaplık > oyunun yanındaki ... > Yönet > Doğrula\n"
           "• GOG Galaxy: oyun > Ayarlar > Yönet > Doğrula / onar"));
}

void GameService::reportOperationFailure(const char* operation, const QString& gameId,
                                          const QString& message)
{
    QString gameName;
    QString store;
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it != m_gameIdToIndex.constEnd() && *it >= 0 && *it < m_games.count()) {
        gameName = m_games[*it].name;
        store = m_games[*it].source;
    }

    CrashReporter::setGameContext(gameId, gameName);

    // The install method travels WITH the event. This comment used to say the
    // catalogue supplies it "when triaging", and that assumption is what let a
    // single defect in 67 packages read as 67 separate per-game issues for
    // months: nobody opens the catalogue per issue, and Sentry cannot group by
    // a field its events do not carry.
    QHash<QString, QString> tags;
    tags.insert(QStringLiteral("game.name"), gameName);
    // Where the copy came from (steam, epic, gog, manual, filesystem…): a
    // failure that only hits folder-detected copies is a different problem
    // from one that hits store installs.
    tags.insert(QStringLiteral("game.store"), store);
    if (m_coreBridge) {
        const QString method = m_coreBridge->getInstallMethodForGame(gameId);
        // An empty type is the catalogue's way of saying "plain overlay"; name
        // the bucket so it is visible rather than blank.
        tags.insert(QStringLiteral("install.method"),
                    method.isEmpty() ? QStringLiteral("overlay") : method);
    }

    // Delegates to the shared reporter so these share the user/system severity
    // split with download, backup, sync and scan failures — one classification
    // rule for the whole application.
    const QString subject = gameName.isEmpty()
        ? gameId
        : QStringLiteral("%1 (%2)").arg(gameId, gameName);
    CrashReporter::reportFailure(operation, subject, message, tags);
}

void GameService::performInstallRollback(const QString& gameId, const QString& originalError)
{
    MAKINE_ZONE_NAMED("GameService::performInstallRollback");
    auto idxIt = m_gameIdToIndex.constFind(gameId);
    QString gamePath;
    if (idxIt != m_gameIdToIndex.constEnd() && *idxIt >= 0 && *idxIt < m_games.count())
        gamePath = m_games[*idxIt].installPath;

    BackupManager* bm = BackupManager::instance();
    if ((!bm || !bm->hasBackup(gameId)) && m_installOverRecord && m_coreBridge
        && !gamePath.isEmpty()) {
        // No backup, over an earlier install: the record is that one's plus
        // what this attempt wrote. Only the attempt's own files go. Removing
        // the record took the working patch with it whenever the update of an
        // add-only patch (it has no backup) failed, timed out or was cancelled.
        const bool clean =
            m_coreBridge->removeAttemptFiles(gameId, gamePath, *m_installOverRecord);
        markInstalledIfRecorded(gameId);
        emit translationInstallCompleted(gameId, false, clean
            ? tr("Kurulum tamamlanamadı; önceki yama kurulu kalıyor.\n%1").arg(originalError)
            : tr("Kurulum tamamlanamadı; önceki yama kurulu kalıyor, ama bu denemenin "
                 "eklediği bazı dosyalar silinemedi. Oyunu kapatıp tekrar deneyin.\n%1")
                  .arg(originalError));
        return;
    }
    if (!bm || !bm->hasBackup(gameId)) {
        // No backup. A record of added files only (nothing the install wrote
        // was there before it) is undone by deleting them; one that replaced
        // originals stays for "Kaldır" and the store's repair. Either way
        // surface the original error and let the user decide (often a Steam
        // Verify is their best path forward).
        if (m_coreBridge && !gamePath.isEmpty() && m_coreBridge->isPackageInstalled(gameId)
            && !translationReplacedOriginalFiles(gameId)
            && m_coreBridge->uninstallPackage(gameId, gamePath))
            markUninstalled(gameId);
        markInstalledIfRecorded(gameId);
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    auto latest = bm->getLatestBackup(gameId);
    if (latest.isEmpty() || !latest.contains(QStringLiteral("id"))) {
        markInstalledIfRecorded(gameId);
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    if (gamePath.isEmpty()) {
        markInstalledIfRecorded(gameId);
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    qCWarning(lcGameService) << "Install failed for" << gameId
                              << "— rolling back via backup:" << originalError;

    // Restore success: also clean up any added files via uninstallPackage,
    // so the game directory is truly back to its pre-install layout.
    // Restore failure: surface both errors. The user will need to verify
    // the game files via Steam (or whichever store) — our own restore
    // cannot guarantee a clean state at this point.
    const auto cancelWait = awaitRestore(bm, gameId,
        [this, gameId, gamePath, originalError,
         backupPath = latest.value(QStringLiteral("backupPath")).toString()]() {
            // Still on record after this means files the failed install added
            // could not be deleted; "returned to how it was" was said anyway.
            if (!m_coreBridge->uninstallPackage(gameId, gamePath, backupPath)
                && m_coreBridge->isPackageInstalled(gameId)) {
                qCWarning(lcGameService) << "Install rollback left files behind for" << gameId;
                markInstalledIfRecorded(gameId);
                emit translationInstallCompleted(gameId, false,
                    tr("Kurulum başarısız oldu. Oyunun orijinal dosyaları geri yüklendi, ama "
                       "yamanın eklediği bazı dosyalar silinemedi. Oyunu kapatıp yamayı "
                       "kaldırmayı deneyin.\n%1").arg(originalError));
                return;
            }
            // A reinstall over an installed patch drops the old record too;
            // the page kept showing the patch as installed.
            markUninstalled(gameId);
            retireBackups(gameId);
            qCInfo(lcGameService) << "Install rollback complete for" << gameId;
            emit translationInstallCompleted(gameId, false,
                tr("Kurulum başarısız oldu, oyun kurulum öncesi haline döndürüldü.\n%1")
                    .arg(originalError));
        },
        [this, gameId, originalError](const QString& reason) {
            // The restore's own failure report is the Sentry event; a
            // critical log here filed the same failure a second time.
            qCWarning(lcGameService) << "Install rollback failed for" << gameId
                                      << "— restore reported errors:" << reason;
            markInstalledIfRecorded(gameId);
            emit translationInstallCompleted(gameId, false,
                tr("Kurulum başarısız oldu ve oyun tam olarak eski haline döndürülemedi.\n"
                   "Kurulum hatası: %1\n%2").arg(originalError, reason));
        });

    // A restore that never starts never reports back either; the install
    // would stay "in progress" for good.
    if (!bm->restoreBackup(latest[QStringLiteral("id")].toString(), gamePath)) {
        cancelWait();
        qCWarning(lcGameService) << "Install rollback could not start for" << gameId;
        markInstalledIfRecorded(gameId);
        emit translationInstallCompleted(gameId, false, originalError);
    }
}

// The originals are back in the game folder and the install record is gone:
// the game's backups belong to no install any more. Kept, the newest one was
// taken for the next install's backup — one that backs nothing up (a patch
// that only adds files, a moved patch) — and its uninstall wrote these old
// originals over the game, files a game update had changed since included.
void GameService::retireBackups(const QString& gameId)
{
    BackupManager* bm = BackupManager::instance();
    if (!bm || m_coreBridge->isPackageInstalled(gameId)) return;
    for (const QVariant& backup : bm->getBackupsForGame(gameId))
        bm->deleteBackup(backup.toMap().value(QStringLiteral("id")).toString());
}

std::function<void()> GameService::awaitRestore(BackupManager* bm, const QString& gameId,
                                                std::function<void()> onRestored,
                                                std::function<void(const QString&)> onFailed)
{
    // Both connections go as soon as either fires. Two single-shot
    // connections left the unfired one behind, and it went off on the
    // game's next restore with a stale callback. `settled` also stops an
    // event queued before the disconnect: restoreBackup() can report a
    // failure and return false in the same call.
    struct Wait {
        QMetaObject::Connection restored, failed;
        bool settled = false;
    };
    auto wait = std::make_shared<Wait>();
    auto settle = [wait]() {
        wait->settled = true;
        QObject::disconnect(wait->restored);
        QObject::disconnect(wait->failed);
    };
    wait->restored = connect(bm, &BackupManager::backupRestored, this,
        [gameId, wait, settle, onRestored](const QString& restoredGameId) {
            if (restoredGameId != gameId || wait->settled) return;
            settle();
            onRestored();
        }, Qt::QueuedConnection);
    wait->failed = connect(bm, &BackupManager::backupRestoreFailed, this,
        [gameId, wait, settle, onFailed](const QString& failedGameId, const QString& reason) {
            if (failedGameId != gameId || wait->settled) return;
            settle();
            onFailed(reason);
        }, Qt::QueuedConnection);
    return settle;
}

void GameService::markUninstalled(const QString& gameId)
{
    // Looked up now, not when the restore started: a scan in between
    // rebuilds the list and the old index points at another game.
    const int gameIndex = m_gameIdToIndex.value(gameId, -1);
    if (gameIndex < 0 || gameIndex >= m_games.count()) return;
    m_games[gameIndex].hasTranslation = false;
    m_packageInstalledCache[gameId] = false;
    // Granular model update
    m_supportedGamesModel->updatePackageStatus(m_games[gameIndex].steamAppId, false);
    invalidateAllCaches();
    emit translationStatusChanged();
}

void GameService::markInstalledIfRecorded(const QString& gameId)
{
    // A record the rollback could not undo (files it could not delete,
    // originals left to the store's repair, the version an update went over)
    // is an installed patch, and "Kaldır" is the way out the message points
    // to. The page took the restore for an uninstall and offered no such
    // button; after a restart the same record read as installed.
    if (!m_coreBridge || !m_coreBridge->isPackageInstalled(gameId)) return;
    const int gameIndex = m_gameIdToIndex.value(gameId, -1);
    if (gameIndex < 0 || gameIndex >= m_games.count()) return;
    m_games[gameIndex].hasTranslation = true;
    m_packageInstalledCache[gameId] = true;
    m_supportedGamesModel->updatePackageStatus(m_games[gameIndex].steamAppId, true);
    invalidateAllCaches();
    emit translationStatusChanged();
}

void GameService::finalizeUninstall(const QString& gameId, const QString& gamePath,
                                    const QString& restoredFrom)
{
    MAKINE_ZONE_NAMED("GameService::finalizeUninstall");
    bool success = m_coreBridge->uninstallPackage(gameId, gamePath, restoredFrom);
    if (success && !restoredFrom.isEmpty())
        retireBackups(gameId);
    if (success)
        markUninstalled(gameId);

    // Still on record after a failed uninstall means files could not be
    // deleted — most often because the game, or something holding its
    // files, is still running.
    const bool filesLeft = !success && m_coreBridge->isPackageInstalled(gameId);

    // Which of the two it was, so the event can tell them apart.
    if (!success)
        reportOperationFailure("uninstall", gameId,
            filesLeft ? QStringLiteral("uninstallPackage returned false: files left on disk")
                      : QStringLiteral("uninstallPackage returned false: no install record"));
    emit translationUninstalled(gameId, success,
        success     ? tr("Yama başarıyla kaldırıldı")
        : filesLeft ? tr("Yama tamamen kaldırılamadı: bazı dosyalar silinemedi. "
                         "Oyunu kapatıp tekrar deneyin.")
                    : tr("Yama kaldırılamadı"));
}

void GameService::recoverTranslation(const QString& gameId)
{
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Uygulama başlatılıyor, lütfen bekleyin"));
        return;
    }

    // The reinstall below picks no option and no game version. A recipe that
    // needs one (Elden Ring, Dark Souls II) was refused there as "kurulum
    // yöntemi: options" — after the uninstall had already taken the patch out.
    // The record does not keep what was picked, so ask before touching it.
    if (!getInstallOptions(gameId).isEmpty() || !getVariants(gameId).isEmpty()) {
        emit translationInstallCompleted(gameId, false,
            tr("Bu yama kurulurken seçim istiyor (kurulum seçeneği ya da oyun sürümü), "
               "bu yüzden kendiliğinden onarılamıyor. Yamaya dokunulmadı.\n"
               "Onarmak için yamayı kaldırıp yeniden kurun; seçimleri yeniden "
               "yapabilirsiniz."));
        return;
    }

    // Step 1: Uninstall current (broken) translation
    // Step 2: When uninstall completes, reinstall the same package
    auto conn = connect(this, &GameService::translationUninstalled, this,
        [this, gameId](const QString& uninstalledId, bool success, const QString& /*msg*/) {
            if (uninstalledId != gameId) return;
            if (!success) {
                emit translationInstallCompleted(gameId, false,
                    tr("Eski çeviri kaldırılamadı, onarım başarısız"));
                return;
            }
            // The store is re-downloading the originals. Installing now would
            // back up files mid-repair as the "originals".
            if (m_storeRepairStarted.contains(gameId)) {
                emit translationInstallCompleted(gameId, false,
                    tr("Eski çeviri kaldırıldı; oyun dosyaları mağaza üzerinden onarılıyor. "
                       "Onarım bitince yamayı yeniden kurun."));
                return;
            }
            // Reinstall with default variant/options
            installTranslation(gameId);
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

    uninstallTranslation(gameId);
}

void GameService::reinstallForUpdate(const QString& gameId, const QString& variant,
                                     const QStringList& selectedOptions)
{
    qCInfo(lcGameService) << "Update of" << gameId
                          << "rewrites files in place — restoring originals, then installing";
    connect(this, &GameService::translationUninstalled, this,
        [this, gameId, variant, selectedOptions](const QString& uninstalledId, bool success, const QString& /*msg*/) {
            if (uninstalledId != gameId) return;
            if (!success) {
                emit translationInstallCompleted(gameId, false,
                    tr("Güncelleme için oyunun orijinal dosyaları geri yüklenemedi; yeni sürüm kurulmadı."));
                return;
            }
            if (m_storeRepairStarted.contains(gameId)) {
                emit translationInstallCompleted(gameId, false,
                    tr("Eski sürüm kaldırıldı; oyun dosyaları mağaza üzerinden onarılıyor. "
                       "Onarım bitince yamayı yeniden kurun."));
                return;
            }
            installTranslation(gameId, variant, selectedOptions);
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
    uninstallTranslation(gameId);
}

bool GameService::hasTranslationUpdate(const QString& gameId) const
{
    if (!m_coreBridge) return false;
    return m_coreBridge->hasTranslationUpdate(gameId);
}

QVariantMap GameService::getRuntimeStatus(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::getRuntimeStatus");
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.size())
        return {{"isUnity", false}, {"needsRuntime", false}};

    const GameInfo& game = m_games[*it];
    bool isUnity = game.engine.toLower().contains("unity");

    if (!isUnity)
        return {{"isUnity", false}, {"needsRuntime", false}};

    // Anti-cheat detection is still relevant for Unity games
    auto antiCheat = checkAntiCheat(gameId);

    return {
        {"isUnity", true},
        {"needsRuntime", false},
        {"installed", false},
        {"upToDate", false},
        {"backend", "none"},
        {"unityVersion", ""},
        {"hasAntiCheat", antiCheat.value("hasAntiCheat").toBool()},
        {"antiCheatName", antiCheat.value("systems").toList().isEmpty() ? ""
            : antiCheat.value("systems").toList().first().toMap().value("name").toString()}
    };
}

void GameService::installRuntime(const QString& gameId)
{
    // Runtime installation is not yet implemented.
    emit runtimeInstallFinished(gameId, false, tr("Çalışma ortamı kurulumu henüz desteklenmiyor"));
}

void GameService::uninstallRuntime(const QString& gameId)
{
    // Runtime uninstallation is not yet implemented.
    emit runtimeInstallFinished(gameId, false, tr("Çalışma ortamı kaldırma henüz desteklenmiyor"));
}

void GameService::acknowledgeAntiCheat(const QString& gameId)
{
    m_antiCheatAcknowledged.insert(gameId);
}

QVariantMap GameService::checkAntiCheat(const QString& gameId)
{
    MAKINE_ZONE_NAMED("GameService::checkAntiCheat");
    // Look up game install path
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.size()) {
        return {{"hasAntiCheat", false}, {"systems", QVariantList()}};
    }

    const QString& gamePath = m_games[*it].installPath;
    if (gamePath.isEmpty() || !QDir(gamePath).exists()) {
        return {{"hasAntiCheat", false}, {"systems", QVariantList()}};
    }

    QVariantList systems;

    // Check for Easy Anti-Cheat
    if (QDir(gamePath + "/EasyAntiCheat").exists() ||
        QFile::exists(gamePath + "/EasyAntiCheat_EOS.sys") ||
        QFile::exists(gamePath + "/easyanticheat_x64.dll")) {
        systems.append(QVariantMap{
            {"name", "Easy Anti-Cheat"},
            {"shortName", "EAC"},
            {"severity", "high"},
            {"warning", tr("EAC aktif oyunlarda çeviri yaması sorunlara yol açabilir")}
        });
    }

    // Check for BattlEye
    if (QDir(gamePath + "/BattlEye").exists() ||
        QFile::exists(gamePath + "/BEService.exe") ||
        QFile::exists(gamePath + "/BEClient_x64.dll")) {
        systems.append(QVariantMap{
            {"name", "BattlEye"},
            {"shortName", "BE"},
            {"severity", "high"},
            {"warning", tr("BattlEye aktif oyunlarda dosya değişiklikleri engellenir")}
        });
    }

    // Check for Vanguard (Riot)
    if (QFile::exists(gamePath + "/vgc.exe") ||
        QFile::exists(gamePath + "/vanguard.exe")) {
        systems.append(QVariantMap{
            {"name", "Riot Vanguard"},
            {"shortName", "Vanguard"},
            {"severity", "critical"},
            {"warning", tr("Vanguard kernel-level anti-cheat, dosya değişikliği kesinlikle önerilmez")}
        });
    }

    return {
        {"hasAntiCheat", !systems.isEmpty()},
        {"systems", systems}
    };
}

bool GameService::hasLocalPackage(const QString& steamAppId) const
{
    if (!m_coreBridge || !m_coreBridge->hasTranslationPackage(steamAppId))
        return false;
    // Metadata exists in manifest — check if package files are actually downloaded
    QString dirName = m_coreBridge->getPackageDirName(steamAppId);
    if (dirName.isEmpty()) return false;

    QDir pkgDir(AppPaths::packagesDir() + QStringLiteral("/") + dirName);
    if (!pkgDir.exists()) return false;

    // A folder filled by another package than the one the catalogue lists now
    // (an update was published, or it predates stamps) is not reused: it would
    // install the old files under the new version. Download again instead.
    const QString wantChecksum = getCatalogEntry(steamAppId).value(QStringLiteral("checksum")).toString();
    if (!pkgstamp::matches(dirName, wantChecksum)) {
        qCInfo(lcGameService) << "Extracted package for" << steamAppId
                              << "is not the catalogue's current one — downloading again";
        pkgDir.removeRecursively();
        pkgstamp::clear(dirName);
        return false;
    }

    // Guard: a previous crash may have left an empty directory tree.
    // Check for actual FILES recursively — empty subdirs don't count.
    // Some packages legitimately have just 1 file, so any file = valid.
    QDirIterator it(pkgDir.absolutePath(), rawfile::kFiles, QDirIterator::Subdirectories);
    if (!it.hasNext()) {
        if (m_emptyExtractionRetried.contains(steamAppId)) {
            // Second time round. The download is not the problem: the package
            // itself extracts to nothing, which is a packaging defect and needs
            // to reach us rather than the user's connection. Keep the directory
            // and let the install fail for real instead of fetching again.
            qCWarning(lcGameService) << "Package extracts to no files on the second "
                                        "attempt — packaging defect:" << steamAppId;
            CrashReporter::reportFailure("package", steamAppId,
                QStringLiteral("package extracts to zero files after a re-download; "
                               "the archive itself carries no usable content"));
            return true;
        }
        m_emptyExtractionRetried.insert(steamAppId);
        qCWarning(lcGameService) << "Package directory has no files (broken extraction?):"
                   << pkgDir.absolutePath() << "— will re-download once";
        pkgDir.removeRecursively();
        return false;
    }
    return true;
}

QVariantMap GameService::getCatalogEntry(const QString& steamAppId) const
{
    const QVariantList catalog = supportedGames();
    for (const auto& entry : catalog) {
        QVariantMap m = entry.toMap();
        if (m.value("steamAppId").toString() == steamAppId) {
            // Enrich with dirName from local manifest if missing
            if (!m.contains("dirName") && m_coreBridge) {
                QString dirName = m_coreBridge->getPackageDirName(steamAppId);
                if (!dirName.isEmpty())
                    m["dirName"] = dirName;
            }
            return m;
        }
    }
    qCDebug(lcGameService) << "GameService::getCatalogEntry: not found for" << steamAppId;
    return {};
}

QVariantList GameService::recentlyUpdatedGames(int limit) const
{
    // Entries without a dated version (an external link, an old index) are
    // left out rather than sorted as the oldest.
    QList<std::pair<QString, QVariant>> dated;
    for (const QVariant& v : supportedGames()) {
        const QString version = v.toMap().value(QStringLiteral("version")).toString();
        if (QDate::fromString(version, Qt::ISODate).isValid())
            dated.append({version, v});
    }
    std::stable_sort(dated.begin(), dated.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    QVariantList out;
    for (const auto& [version, entry] : dated) {
        if (limit >= 0 && out.size() >= limit)
            break;
        out.append(entry);
    }
    return out;
}

QVariantMap GameService::resolveGameData(const QString& gameId,
                                          const QString& gameName,
                                          const QString& installPath,
                                          const QString& engine,
                                          bool forceAutoInstall) const
{
    const QVariantMap gameData = getGameById(gameId);
    const bool hasGame = !gameData.isEmpty();

    // Manual game detection
    const bool isManual = (hasGame && gameData.value("source").toString() == QLatin1String("manual"))
                          || gameId.startsWith(QLatin1String("manual_"));

    const bool isInstalled = (hasGame && gameData.value("isInstalled").toBool())
                             || !installPath.isEmpty();

    // Resolve steamAppId: prefer gameData, fall back to gameId if numeric
    QString resolvedSteamAppId = hasGame ? gameData.value("steamAppId").toString() : QString();
    if (resolvedSteamAppId.isEmpty()) {
        // Check if gameId is purely numeric (i.e. a Steam App ID itself)
        bool isNumeric = !gameId.isEmpty();
        for (const QChar& ch : gameId) {
            if (!ch.isDigit()) { isNumeric = false; break; }
        }
        if (isNumeric)
            resolvedSteamAppId = gameId;
    }

    // Image resolution via ImageCacheManager
    const QString imageKey = resolvedSteamAppId.isEmpty() ? gameId : resolvedSteamAppId;
    const QString resolvedImageUrl = m_imageCache ? m_imageCache->resolve(imageKey) : QString();

    const bool hasTranslation = hasGame && gameData.value("hasTranslation").toBool();
    const bool pkgInstalled = hasGame && gameData.value("packageInstalled").toBool();

    // Catalog lookup for external URL / Apex / Hangar flags
    const QVariantMap catalog = getCatalogEntry(imageKey);
    const bool hasCatalog = !catalog.isEmpty();
    const QString externalUrl = hasCatalog ? catalog.value("externalUrl").toString() : QString();
    const QString sourceField = hasCatalog ? catalog.value("source").toString() : QString();
    const QString translationSource = hasCatalog ? catalog.value("translationSource").toString() : QString();
    const bool isApex = hasCatalog
        && (translationSource == QLatin1String("apex")
            || sourceField == QLatin1String("apex")
            || sourceField == QLatin1String("hangar_apex"));
    const bool isHangar = hasCatalog
        && (translationSource == QLatin1String("hangar")
            || sourceField == QLatin1String("hangar")
            || sourceField == QLatin1String("hangar_apex"));
    const QString apexTier = hasCatalog ? catalog.value("apexTier").toString() : QString();

    // Contributors from catalog (index.json) — available immediately, no async
    const QVariantList catalogContribs = hasCatalog
        ? catalog.value(QStringLiteral("contributors")).toList() : QVariantList{};

    return {
        {"gameId",            gameId},
        {"gameName",          gameName},
        {"engine",            engine},
        {"imageUrl",          resolvedImageUrl},
        {"verified",          hasGame && gameData.value("isVerified").toBool()},
        {"steamAppId",        resolvedSteamAppId},
        {"hasTranslation",    hasTranslation},
        {"isManualGame",      isManual},
        {"isGameInstalled",   isInstalled},
        {"packageInstalled",  pkgInstalled},
        {"isApex",            isApex},
        {"isHangar",          isHangar},
        {"apexTier",          apexTier},
        {"autoInstall",       forceAutoInstall},
        {"externalUrl",       externalUrl},
        {"contributors",      catalogContribs}
    };
}

// ── URL construction helpers (moved from GameDetailViewModel.qml) ──

QString GameService::steamHeroUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/")
           + steamAppId + QStringLiteral("/library_hero.jpg");
}

QString GameService::steamCoverUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/")
           + steamAppId + QStringLiteral("/library_600x900_2x.jpg");
}

QString GameService::steamLogoUrl(const QString& steamAppId) const
{
    if (steamAppId.isEmpty())
        return {};
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/")
           + steamAppId + QStringLiteral("/logo.png");
}

QString GameService::formatDownloadProgress(qint64 received, qint64 total) const
{
    if (total <= 0)
        return {};
    const double receivedMB = static_cast<double>(received) / (1024.0 * 1024.0);
    const double totalMB = static_cast<double>(total) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB / %2 MB")
        .arg(receivedMB, 0, 'f', 1)
        .arg(totalMB, 0, 'f', 1);
}

bool GameService::shouldAutoInstall(const QString& gameId) const
{
    // A game should auto-install if it has a translation, is installed,
    // and the package is not yet installed.
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd())
        return false;
    int idx = it.value();
    if (idx < 0 || idx >= m_games.count())
        return false;
    const auto& game = m_games[idx];
    if (!game.hasTranslation || !game.isInstalled)
        return false;
    // Check if package is already installed
    const QString key = game.steamAppId.isEmpty() ? game.id : game.steamAppId;
    if (m_coreBridge && m_coreBridge->isPackageInstalled(key))
        return false;
    return true;
}

void GameService::checkForUpdates()
{
    qCInfo(lcGameService) << "GameService: checking for updates...";

    // Re-sync manifest from CDN
    if (m_manifestSync)
        m_manifestSync->syncCatalog();

    // Rescan game libraries
    scanAllLibraries();

}

} // namespace makine
