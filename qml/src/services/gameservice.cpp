/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "gameservice.h"
#include "imagecachemanager.h"
#include "steamdetailsservice.h"
#include "backupmanager.h"
#include "apppaths.h"
#include "profiler.h"
#include "crashreporter.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QLoggingCategory>
#include <QtConcurrent>
#include <algorithm>
#include <QProcess>
#include <optional>

Q_LOGGING_CATEGORY(lcGameService, "makine.game")

namespace {
constexpr int kAutoScanDelayMs = 500;
} // namespace

namespace makine {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_steamDetails(new SteamDetailsService(this))
    , m_supportedGamesModel(new SupportedGamesModel(this))
{
    connect(m_steamDetails, &SteamDetailsService::detailsFetched,
            this, &GameService::steamDetailsFetched);
    connect(m_steamDetails, &SteamDetailsService::detailsFetchError,
            this, &GameService::steamDetailsFetchError);
}

void GameService::initialize()
{
    CrashReporter::addBreadcrumb("game", "GameService::initialize");
    setupCoreBridge();

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

        // Load Steam details cache via SteamDetailsService
        m_steamDetails->loadCache();

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
                                         p = std::move(pkgCache)]() mutable {
            MAKINE_ZONE_NAMED("GameService::initialize (main thread)");

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

void GameService::scanAllLibraries()
{
    MAKINE_ZONE_NAMED("GameService::scanAllLibraries");
    if (m_isScanning) return;

    m_isScanning = true;
    emit isScanningChanged();

    m_scanProgress = 0;
    m_scanStatus = tr("Oyun kütüphaneleri taranıyor...");

    // Use CoreBridge for actual scanning
    if (m_coreBridge) {
        m_coreBridge->scanAllLibraries();
    }
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

    connect(m_coreBridge, &CoreBridge::packageInstallCompleted,
            this, [this](bool success, const QString& message) {
                QString gameId = m_installingGameId;
                m_installingGameId.clear();
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
                }
                emit translationInstallCompleted(gameId, success, message);
            });

    connect(m_coreBridge, &CoreBridge::packageInstallError,
            this, [this](const QString& error) {
                QString gameId = m_installingGameId;
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false, error);
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

void GameService::addManualGame(const QString& path)
{
    MAKINE_ZONE_NAMED("GameService::addManualGame");
    // Security: Validate path (sync — fast)
    if (!isValidGamePath(path)) {
        qCWarning(lcGameService) << "addManualGame: invalid path" << path;
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        qCWarning(lcGameService) << "addManualGame: path not found" << path;
        return;
    }

    // Check for duplicate (sync — O(n) but fast string compare)
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    for (const auto& game : m_games) {
        if (QFileInfo(game.installPath).canonicalFilePath() == canonicalPath) {
            emit manualGameAdded(game.id);
            return;
        }
    }

    const QString folderName = dir.dirName();

    // Heavy work (disk I/O + linear scan) → background thread
    CoreBridge* cb = m_coreBridge;
    (void)QtConcurrent::run([this, path, folderName, cb]() {
        MAKINE_THREAD_NAME("Worker-ManualGame");
        MAKINE_ZONE_NAMED("addManualGame (async)");

        QString engine;
        if (cb) engine = cb->detectEngine(path);

        // Step 1: Try folder-name matching (fast, backward compatible)
        QString matchedAppId;
        if (cb) matchedAppId = cb->findMatchingAppId(folderName);

        // Step 2: If no match, try file-based fingerprint matching
        if (matchedAppId.isEmpty() && cb) {
            QVariantList candidates = cb->findMatchingGamesFromFiles(path);
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

        QMetaObject::invokeMethod(this, [this, path, folderName, engine, matchedAppId]() {
            finalizeManualGame(path, folderName, engine, matchedAppId);
        }, Qt::QueuedConnection);
    });
}

void GameService::finalizeManualGame(const QString& path, const QString& folderName,
                                      const QString& engine, const QString& matchedAppId)
{
    MAKINE_ZONE_NAMED("GameService::finalizeManualGame");

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

    // If still no match, try matching via ManifestSync catalog by name
    if (resolvedAppId.isEmpty() && m_manifestSync) {
        const QVariantList catalog = m_manifestSync->catalog();
        const QString lowerFolder = folderName.toLower();
        for (const auto& item : catalog) {
            const QVariantMap entry = item.toMap();
            const QString nameLower = entry.value(QStringLiteral("gameName")).toString().toLower();
            const QString dirLower = entry.value(QStringLiteral("dirName")).toString().toLower();
            if (nameLower.contains(lowerFolder) ||
                lowerFolder.contains(nameLower) ||
                dirLower == lowerFolder) {
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
        game.id = QStringLiteral("manual_%1").arg(m_games.count() + 1);
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

    // Contributors + install notes from package manifest
    QVariantList contributors;
    QString installNotes;
    if (m_coreBridge) {
        auto pkg = m_coreBridge->getPackageForGame(gameId);
        if (pkg)
            contributors = pkg->contributors;
        installNotes = m_coreBridge->getInstallNotesForGame(gameId);
    }

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
            entry[QStringLiteral("source")] = game.source;
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

            QFile file(cachePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
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

bool GameService::isValidGamePath(const QString& path) const
{
    // Security: Check for path traversal attacks
    if (path.contains("..") || path.contains("//") || path.contains("\\\\")) {
        qCWarning(lcGameService) << "Path traversal attempt detected:" << path;
        return false;
    }

    QFileInfo info(path);
    if (!info.isAbsolute()) {
        qCWarning(lcGameService) << "Relative path not allowed:" << path;
        return false;
    }

    if (!info.isDir()) {
        qCWarning(lcGameService) << "Path is not a directory:" << path;
        return false;
    }

    // Check for suspicious paths (system directories)
    const QString normalizedPath = info.absoluteFilePath().toLower();
    static const QStringList forbiddenPaths = {
        "c:/windows",
        "c:/program files/common files",
        "c:/programdata",
        "/etc",
        "/usr",
        "/bin",
        "/sbin"
    };

    for (const auto& forbidden : forbiddenPaths) {
        if (normalizedPath.startsWith(forbidden)) {
            qCWarning(lcGameService) << "Forbidden system path:" << path;
            return false;
        }
    }

    return true;
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
    if (m_coreBridge)
        m_coreBridge->cancelInstall();
    m_installingGameId.clear();
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

    if (mode == InstallMode::Update) {
        // Safety: backup must exist — if not, fall back to Install mode
        BackupManager* bm = BackupManager::instance();
        if (bm && !bm->hasBackup(gameId)) {
            qCWarning(lcGameService) << "No backup found for" << gameId
                << "— falling back to Install mode (will create new backup)";
            mode = InstallMode::Install;
        }
    }

    if (mode == InstallMode::Install) {
        // Install mode: consume anti-cheat acknowledgement.
        // InstallFlowController already handled the warning dialog and called
        // acknowledgeAntiCheat() before reaching here, so we just clear the flag.
        m_antiCheatAcknowledged.remove(gameId);
    }

    // Reserve install slot early to prevent double-install
    m_installingGameId = gameId;
    emit translationInstallStarted(gameId);
    emit translationInstallProgress(gameId, 0.0, tr("Oyun durumu kontrol ediliyor..."));

    // Async game-running check (avoids blocking the main thread with tasklist)
    const QString installPath = game.installPath;
    const QString runningMsg = (mode == InstallMode::Install)
        ? tr("Bu oyun şu anda çalışıyor (%1). Çeviriyi kurmak için oyunu kapatın.")
        : tr("Bu oyun şu anda çalışıyor (%1). Güncelleme için oyunu kapatın.");

    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
        [this, watcher, gameId, installPath, variant, selectedOptions, pkg, mode, runningMsg]() {
            const QString runningExe = watcher->result();
            watcher->deleteLater();

            if (!runningExe.isEmpty()) {
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false, runningMsg.arg(runningExe));
                return;
            }

            if (mode == InstallMode::Update) {
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
            QStringList filesToOverwrite = m_coreBridge->getPackageFileList(gameId, variant);

            if (bm && !filesToOverwrite.isEmpty() && !alreadyInstalled) {
                emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));

                connect(bm, &BackupManager::selectiveBackupCompleted, this,
                    [this, gameId, installPath, variant, selectedOptions](const QString& backupGameId, bool /*success*/) {
                        if (backupGameId != gameId) return;
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

    watcher->setFuture(QtConcurrent::run([installPath]() -> QString {
        const QDir gameDir(installPath);
        const QStringList exeFiles = gameDir.entryList({"*.exe"}, QDir::Files);
        for (const QString& exe : exeFiles) {
            QProcess tasklist;
            tasklist.start("tasklist", {"/FI", "IMAGENAME eq " + exe, "/FO", "CSV", "/NH"});
            tasklist.waitForFinished(2000);
            const QString output = QString::fromLocal8Bit(tasklist.readAllStandardOutput());
            if (output.contains(exe, Qt::CaseInsensitive) && !output.contains("INFO:")) {
                return exe;
            }
        }
        return {};
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

    const GameInfo& game = m_games[*it];

    // Restore from backup before uninstall (wait for async restore to complete)
    BackupManager* bm = BackupManager::instance();
    if (bm && bm->hasBackup(gameId)) {
        auto latest = bm->getLatestBackup(gameId);
        if (!latest.isEmpty() && latest.contains("id")) {
            // One-shot connection: wait for restore to finish, then proceed with uninstall
            auto restoreConn = connect(bm, &BackupManager::backupRestored, this,
                [this, gameId, gamePath = game.installPath, idx = *it](const QString& restoredGameId) {
                    if (restoredGameId != gameId) return;
                    finalizeUninstall(gameId, gamePath, idx);
                }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
            bool started = bm->restoreBackup(latest["id"].toString(), game.installPath);
            if (!started) {
                disconnect(restoreConn);
                qCWarning(lcGameService) << "Backup restoration failed for" << gameId
                           << "- proceeding with uninstall anyway";
                finalizeUninstall(gameId, game.installPath, *it);
            }
            return;
        }
    }

    finalizeUninstall(gameId, game.installPath, *it);
}

void GameService::finalizeUninstall(const QString& gameId, const QString& gamePath, int gameIndex)
{
    MAKINE_ZONE_NAMED("GameService::finalizeUninstall");
    bool success = m_coreBridge->uninstallPackage(gameId, gamePath);

    if (success && gameIndex >= 0 && gameIndex < m_games.count()) {
        m_games[gameIndex].hasTranslation = false;
        m_packageInstalledCache[gameId] = false;
        // Granular model update
        m_supportedGamesModel->updatePackageStatus(m_games[gameIndex].steamAppId, false);
        invalidateAllCaches();
        emit translationStatusChanged();

    }

    emit translationUninstalled(gameId, success,
        success ? tr("Yama başarıyla kaldırıldı")
                : tr("Yama kaldırılamadı"));
}

void GameService::recoverTranslation(const QString& gameId)
{
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Uygulama başlatılıyor, lütfen bekleyin"));
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
            // Reinstall with default variant/options
            installTranslation(gameId);
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

    // Guard: a previous crash may have left an empty directory tree.
    // Check for actual FILES recursively — empty subdirs don't count.
    // Some packages legitimately have just 1 file, so any file = valid.
    QDirIterator it(pkgDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
    if (!it.hasNext()) {
        qCWarning(lcGameService) << "Package directory has no files (broken extraction?):"
                   << pkgDir.absolutePath() << "— will re-download";
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

    // Catalog lookup for external URL / Apex flags
    const QVariantMap catalog = getCatalogEntry(imageKey);
    const bool hasCatalog = !catalog.isEmpty();
    const QString externalUrl = hasCatalog ? catalog.value("externalUrl").toString() : QString();
    const bool isApex = hasCatalog
        && (catalog.value("translationSource").toString() == QLatin1String("apex")
            || catalog.value("source").toString() == QLatin1String("apex"));
    const QString apexTier = hasCatalog ? catalog.value("apexTier").toString() : QString();

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
        {"apexTier",          apexTier},
        {"autoInstall",       forceAutoInstall},
        {"externalUrl",       externalUrl}
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
