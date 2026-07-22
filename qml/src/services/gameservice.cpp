// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
#include <QProcess>
#include <optional>

Q_LOGGING_CATEGORY(lcGameService, "makine.game")

namespace {
constexpr int kAutoScanDelayMs = 500;

// Normalize a game name for comparison: lowercase + drop non-alphanumeric.
QString normalizeGameName(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (QChar ch : s) {
        if (ch.isLetterOrNumber()) out.append(ch.toLower());
        else if (ch.isSpace() && !out.isEmpty() && !out.endsWith(QLatin1Char(' '))) out.append(QLatin1Char(' '));
    }
    return out.trimmed();
}

// Reject catalog matches where the locally detected game name has
// nothing to do with the catalog name. Crack/repack ACFs sometimes
// reuse a known appid (e.g. Little Nightmares' 424840) for a
// completely different game (e.g. The Genesis Order), which would
// otherwise offer the wrong translation.
bool gameNamesLikelyMatch(const QString& localName, const QString& catalogName)
{
    const QString a = normalizeGameName(localName);
    const QString b = normalizeGameName(catalogName);
    if (a.isEmpty() || b.isEmpty()) return true;       // can't decide → trust catalog
    if (a.contains(b) || b.contains(a)) return true;   // substring either direction

    const auto tokensA = a.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const auto tokensB = b.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokensA.isEmpty() || tokensB.isEmpty()) return true;

    QSet<QString> setB(tokensB.begin(), tokensB.end());
    int common = 0;
    for (const auto& t : tokensA)
        if (setB.contains(t)) ++common;

    const int minTokens = std::min(tokensA.size(), tokensB.size());
    return common >= std::max(1, minTokens / 2);
}

} // namespace

namespace makine {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_steamDetails(new SteamDetailsService(this))
    , m_supportedGamesModel(new SupportedGamesModel(this))
    , m_installTimeoutTimer(new QTimer(this))
{
    connect(m_steamDetails, &SteamDetailsService::detailsFetched,
            this, &GameService::steamDetailsFetched);
    connect(m_steamDetails, &SteamDetailsService::detailsFetchError,
            this, &GameService::steamDetailsFetchError);

    // 30 minutes is the run-step ceiling in LocalPackageManager + a safety
    // margin. If the core never reports completion (hang, crash without
    // signal, deadlock) the slot would otherwise stay stuck and every
    // subsequent install would hit "Zaten bir kurulum devam ediyor".
    m_installTimeoutTimer->setSingleShot(true);
    m_installTimeoutTimer->setInterval(30 * 60 * 1000);
    connect(m_installTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_installingGameId.isEmpty()) return;
        const QString stuck = m_installingGameId;
        m_installingGameId.clear();
        qCWarning(lcGameService) << "Install timed out — clearing slot for" << stuck;
        emit translationInstallCompleted(stuck, false,
            tr("Kurulum yanıt vermiyor — uygulamayı yeniden başlatıp tekrar deneyin"));
    });
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
                m_installTimeoutTimer->stop();
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
    m_installTimeoutTimer->start();
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
                m_installTimeoutTimer->stop();
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false, runningMsg.arg(runningExe));
                return;
            }

            // Fail fast when the game directory cannot be written to. The
            // launcher runs asInvoker on purpose (elevating raises antivirus
            // false positives), so installs under C:\Program Files — where the
            // Epic launcher puts games by default — cannot be written without
            // elevation. Discovering that only at the copy step means the user
            // waits through the whole download, extraction and backup first,
            // then gets a permission error with nothing to show for it.
            if (!isGameDirWritable(installPath)) {
                qCWarning(lcGameService) << "game directory not writable:" << installPath;
                m_installTimeoutTimer->stop();
                m_installingGameId.clear();
                const QString msg =
                    tr("Oyun klasörüne yazma izni yok:\n%1\n\n"
                       "Bu klasör genelde yönetici izni gerektirir "
                       "(örn. C:\\Program Files altındaki kurulumlar).\n\n"
                       "Çözüm:\n"
                       "1) Makine Launcher'ı kapatın\n"
                       "2) Kısayola sağ tıklayıp \"Yönetici olarak çalıştır\" deyin\n"
                       "3) Yamayı tekrar kurun\n\n"
                       "Alternatif: oyunu Program Files dışında bir klasöre taşıyın.")
                        .arg(installPath);
                reportOperationFailure("install", gameId,
                    QStringLiteral("game directory not writable: %1").arg(installPath));
                emit translationInstallCompleted(gameId, false, msg);
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
            // installPath matters: it lets the wrapper-stripping check verify a
            // package's top-level folder against the real game install, so the
            // backup list matches the files the install will actually overwrite.
            QStringList filesToOverwrite =
                m_coreBridge->getPackageFileList(gameId, variant, installPath);

            if (bm && !filesToOverwrite.isEmpty() && !alreadyInstalled) {
                emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));

                connect(bm, &BackupManager::selectiveBackupCompleted, this,
                    [this, gameId, installPath, variant, selectedOptions](const QString& backupGameId, bool success) {
                        if (backupGameId != gameId) return;
                        if (!success) {
                            // Without a backup we cannot offer safe rollback, and the patch
                            // would silently destroy the originals on the next uninstall.
                            qCWarning(lcGameService) << "Selective backup failed for" << gameId
                                                      << "— aborting install to protect originals";
                            m_installTimeoutTimer->stop();
                            m_installingGameId.clear();
                            emit translationInstallCompleted(gameId, false,
                                tr("Yedek oluşturulamadı, kurulum iptal edildi. "
                                   "Disk alanı veya yazma iznini kontrol edin."));
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
            // Partial restore: refuse to run uninstallPackage afterwards. Removing
            // patched-but-not-restored files would leave the game with holes;
            // surface the reason and let the user run Steam's "Verify Integrity"
            // (or unlock the file manually) instead.
            connect(bm, &BackupManager::backupRestoreFailed, this,
                [this, gameId](const QString& failedGameId, const QString& reason) {
                    if (failedGameId != gameId) return;
                    qCWarning(lcGameService) << "Backup restore failed for" << gameId
                                              << "— skipping uninstall to avoid mixed state";
                    reportOperationFailure("uninstall/restore", gameId, reason);
                    emit translationUninstalled(gameId, false, reason);
                }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
            bool started = bm->restoreBackup(latest["id"].toString(), game.installPath);
            if (!started) {
                disconnect(restoreConn);
                // Restore never started. LocalPackageManager::uninstallPackage skips
                // replacedFiles because it assumes restore puts the originals back,
                // so running it now would leave the patched files in place while we
                // report success. Only safe when the install added files only.
                if (translationReplacedOriginalFiles(gameId)) {
                    qCCritical(lcGameService) << "Backup restore could not start for" << gameId
                               << "- refusing uninstall, originals would stay patched";
                    reportOperationFailure("uninstall", gameId,
                        QStringLiteral("restore could not start; originals would stay patched"));
                    emit translationUninstalled(gameId, false,
                        tr("Yama kaldırılamadı: yedek geri yüklenemediği için oyunun "
                           "değiştirilen dosyaları eski haline döndürülemiyor.\n"
                           "Oyun dosyalarını mağaza üzerinden doğrulayın (Steam: oyuna sağ tık > "
                           "Özellikler > Yüklü Dosyalar > Oyun dosyalarının bütünlüğünü doğrula)."));
                    return;
                }
                qCWarning(lcGameService) << "Backup restore could not start for" << gameId
                           << "- install only added files, uninstall is still safe";
                finalizeUninstall(gameId, game.installPath, *it);
            }
            return;
        }
    }

    // No usable backup. Same reasoning as above: with no restore pass, the
    // overwritten originals stay patched and the game keeps running the
    // translation the user was told had been removed.
    if (translationReplacedOriginalFiles(gameId)) {
        qCCritical(lcGameService) << "No backup available for" << gameId
                   << "- refusing uninstall, originals would stay patched";
        reportOperationFailure("uninstall", gameId,
            QStringLiteral("no backup available; originals would stay patched"));
        emit translationUninstalled(gameId, false,
            tr("Yama kaldırılamadı: bu yama oyunun orijinal dosyalarının üzerine yazmış "
               "ve geri yüklenecek bir yedek bulunamadı.\n"
               "Oyun dosyalarını mağaza üzerinden doğrulayın (Steam: oyuna sağ tık > "
               "Özellikler > Yüklü Dosyalar > Oyun dosyalarının bütünlüğünü doğrula)."));
        return;
    }

    finalizeUninstall(gameId, game.installPath, *it);
}

bool GameService::isGameDirWritable(const QString& gamePath)
{
    if (gamePath.isEmpty())
        return false;
    QDir dir(gamePath);
    if (!dir.exists())
        return false;

    // Actually create a file rather than trusting QFileInfo::isWritable().
    // On Windows that flag reflects the read-only attribute, not the effective
    // ACL, so a directory under C:\Program Files reports "writable" for a
    // non-elevated process right up until the write fails. Probing is the only
    // answer that matches what the install will experience.
    QTemporaryFile probe(dir.filePath(QStringLiteral("makine-write-probe-XXXXXX.tmp")));
    probe.setAutoRemove(true);
    if (!probe.open())
        return false;
    const bool wrote = probe.write("makine", 6) == 6;
    probe.close();
    return wrote;
}

bool GameService::translationReplacedOriginalFiles(const QString& gameId)
{
    if (!m_coreBridge) return false;
    const auto info = m_coreBridge->getInstalledInfo(gameId);
    return info.has_value() && !info->replacedFiles.isEmpty();
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
        for (const QString& rel : rels) {
            if (!QFileInfo::exists(QDir::cleanPath(gamePath + QLatin1Char('/') + rel)))
                ++missing;
        }
        const int total = rels.size();

        QMetaObject::invokeMethod(this, [this, gameId, missing, total]() {
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
            CrashReporter::reportFailure(
                "integrity", gameId,
                QStringLiteral("installed patch files missing: %1/%2")
                    .arg(missing).arg(total));

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
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it != m_gameIdToIndex.constEnd() && *it >= 0 && *it < m_games.count())
        gameName = m_games[*it].name;

    // Tag with the game so failures group per title in Sentry; the catalog
    // supplies the install method for any appId when triaging.
    CrashReporter::setGameContext(gameId, gameName);

    // Delegates to the shared reporter so these share the user/system severity
    // split with download, backup, sync and scan failures — one classification
    // rule for the whole application.
    const QString subject = gameName.isEmpty()
        ? gameId
        : QStringLiteral("%1 (%2)").arg(gameId, gameName);
    CrashReporter::reportFailure(operation, subject, message);
}

void GameService::performInstallRollback(const QString& gameId, const QString& originalError)
{
    MAKINE_ZONE_NAMED("GameService::performInstallRollback");
    BackupManager* bm = BackupManager::instance();
    if (!bm || !bm->hasBackup(gameId)) {
        // No backup exists — nothing safe to roll back to. Surface the
        // original error and let the user decide (often a Steam Verify is
        // their best path forward).
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    auto latest = bm->getLatestBackup(gameId);
    if (latest.isEmpty() || !latest.contains(QStringLiteral("id"))) {
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    auto idxIt = m_gameIdToIndex.constFind(gameId);
    QString gamePath;
    if (idxIt != m_gameIdToIndex.constEnd() && *idxIt >= 0 && *idxIt < m_games.count())
        gamePath = m_games[*idxIt].installPath;
    if (gamePath.isEmpty()) {
        emit translationInstallCompleted(gameId, false, originalError);
        return;
    }

    qCWarning(lcGameService) << "Install failed for" << gameId
                              << "— rolling back via backup:" << originalError;

    // Restore success: also clean up any added files via uninstallPackage,
    // so the game directory is truly back to its pre-install layout.
    connect(bm, &BackupManager::backupRestored, this,
        [this, gameId, gamePath, originalError](const QString& restoredGameId) {
            if (restoredGameId != gameId) return;
            if (m_coreBridge) m_coreBridge->uninstallPackage(gameId, gamePath);
            qCInfo(lcGameService) << "Install rollback complete for" << gameId;
            emit translationInstallCompleted(gameId, false,
                tr("Kurulum başarısız oldu, oyun kurulum öncesi haline döndürüldü.\n%1")
                    .arg(originalError));
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

    // Restore failure: surface both errors. The user will need to verify
    // the game files via Steam (or whichever store) — our own restore
    // cannot guarantee a clean state at this point.
    connect(bm, &BackupManager::backupRestoreFailed, this,
        [this, gameId, originalError](const QString& failedGameId, const QString& reason) {
            if (failedGameId != gameId) return;
            qCCritical(lcGameService) << "Install rollback failed for" << gameId
                                       << "— restore reported errors:" << reason;
            emit translationInstallCompleted(gameId, false,
                tr("Kurulum başarısız oldu ve oyun tam olarak eski haline döndürülemedi.\n"
                   "Kurulum hatası: %1\n%2").arg(originalError, reason));
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

    bm->restoreBackup(latest[QStringLiteral("id")].toString(), gamePath);
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

    if (!success)
        reportOperationFailure("uninstall", gameId,
            QStringLiteral("uninstallPackage returned false"));

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
