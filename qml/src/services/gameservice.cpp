/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "gameservice.h"
#include "backupmanager.h"
#include "apppaths.h"
#include "profiler.h"
#include "crashreporter.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QProcess>
#include <optional>

namespace {
constexpr int kAutoScanDelayMs = 500;
constexpr qint64 kMaxSteamResponseBytes = 5 * 1024 * 1024; // 5 MB

// Pure JSON parsing — no side effects, safe for background thread
std::optional<makineai::SteamDetails> parseSteamJson(const QString& steamAppId, const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return std::nullopt;

    const QJsonObject root = doc.object();
    const QJsonObject appObj = root.value(steamAppId).toObject();
    if (!appObj.value("success").toBool())
        return std::nullopt;

    const QJsonObject appData = appObj.value("data").toObject();
    if (appData.isEmpty())
        return std::nullopt;

    makineai::SteamDetails details;
    details.description = appData.value("short_description").toString();
    details.releaseDate = appData.value("release_date").toObject().value("date").toString();

    const auto devArr = appData.value("developers").toArray();
    details.developers.reserve(devArr.size());
    for (const auto& dev : devArr)
        details.developers.append(dev.toString());

    const auto pubArr = appData.value("publishers").toArray();
    details.publishers.reserve(pubArr.size());
    for (const auto& pub : pubArr)
        details.publishers.append(pub.toString());

    const auto genreArr = appData.value("genres").toArray();
    details.genres.reserve(genreArr.size());
    for (const auto& genre : genreArr)
        details.genres.append(genre.toObject().value("description").toString());

    details.metacriticScore = appData.value("metacritic").toObject().value("score").toInt(0);

    const QJsonObject platforms = appData.value("platforms").toObject();
    details.hasWindows = platforms.value("windows").toBool(true);
    details.hasMac = platforms.value("mac").toBool(false);
    details.hasLinux = platforms.value("linux").toBool(false);

    if (appData.value("is_free").toBool()) {
        details.price = QObject::tr("\u00DCcretsiz");
    } else {
        const QJsonObject priceObj = appData.value("price_overview").toObject();
        details.price = priceObj.value("final_formatted").toString();
        details.discountPercent = priceObj.value("discount_percent").toInt(0);
    }

    const auto ssArr = appData.value("screenshots").toArray();
    details.screenshots.reserve(ssArr.size());
    for (const auto& ss : ssArr) {
        const QString thumbUrl = ss.toObject().value("path_thumbnail").toString();
        if (!thumbUrl.isEmpty())
            details.screenshots.append(thumbUrl);
    }

    details.backgroundUrl = appData.value("background").toString();
    details.fetchedAt = QDateTime::currentDateTime();

    return details;
}
} // namespace

namespace makineai {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_supportedGamesModel(new SupportedGamesModel(this))
{
}

void GameService::initialize()
{
    CrashReporter::addBreadcrumb("game", "GameService::initialize");
    setupCoreBridge();

    // Load caches in background thread to avoid blocking the UI
    const QString gamesCachePath = AppPaths::gamesCacheFile();
    const QString steamCachePath = AppPaths::steamDetailsCacheFile();

    (void)QtConcurrent::run([this, gamesCachePath, steamCachePath]() {
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
                        if (!g.id.isEmpty() && !g.name.isEmpty())
                            games.append(g);
                    }
                }
            }
        }

        // Parse Steam details cache
        QHash<QString, SteamDetails> steamDetails;
        {
            QFile file(steamCachePath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
                file.close();

                if (err.error == QJsonParseError::NoError && doc.isObject()) {
                    const QJsonObject root = doc.object();
                    for (auto it = root.begin(); it != root.end(); ++it) {
                        const QJsonObject obj = it.value().toObject();
                        SteamDetails d;
                        d.description = obj["description"].toString();
                        d.releaseDate = obj["releaseDate"].toString();
                        d.metacriticScore = obj["metacriticScore"].toInt(0);
                        d.hasWindows = obj["hasWindows"].toBool(true);
                        d.hasMac = obj["hasMac"].toBool(false);
                        d.hasLinux = obj["hasLinux"].toBool(false);
                        d.price = obj["price"].toString();
                        d.discountPercent = obj["discountPercent"].toInt(0);
                        d.backgroundUrl = obj["backgroundUrl"].toString();
                        d.fetchedAt = QDateTime::fromString(obj["fetchedAt"].toString(), Qt::ISODate);
                        for (const auto& v : obj["developers"].toArray()) d.developers.append(v.toString());
                        for (const auto& v : obj["publishers"].toArray()) d.publishers.append(v.toString());
                        for (const auto& v : obj["genres"].toArray()) d.genres.append(v.toString());
                        for (const auto& v : obj["screenshots"].toArray()) d.screenshots.append(v.toString());
                        if (!d.isExpired())
                            steamDetails[it.key()] = d;
                    }
                }
            }
        }

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

        qDebug() << "Parsed" << games.count() << "games and"
                 << steamDetails.size() << "Steam details,"
                 << pkgCache.size() << "package statuses (background)";

        // Deliver results to main thread
        QMetaObject::invokeMethod(this, [this,
                                         g = std::move(games),
                                         s = std::move(steamDetails),
                                         p = std::move(pkgCache)]() mutable {
            MAKINE_ZONE_NAMED("GameService::initialize (main thread)");

            m_games = std::move(g);
            m_steamDetailsCache = std::move(s);
            m_packageInstalledCache = std::move(p);
            rebuildCache();

            if (m_games.isEmpty()) {
                qDebug() << "No cached games, auto-scan scheduled";
                QTimer::singleShot(kAutoScanDelayMs, this, &GameService::scanAllLibraries);
            }

            emit gameListChanged();
            emit translationStatusChanged();
            emit supportedGamesChanged();
            ensureSupportedGamesCache();
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
            qDebug() << "catalogReady received — rebuilding model";
            // Defer to next event loop so refreshPackageManifest completes first
            QTimer::singleShot(0, this, [this]{
                // Trigger cache rebuild + model population via supportedGames()
                supportedGames();
                emit supportedGamesChanged();
            });
        });
    }
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

    // Preserve manually added games across re-scan
    QList<GameInfo> manualGames;
    for (const auto& g : m_games) {
        if (g.source == QLatin1String("manual"))
            manualGames.append(g);
    }

    // Convert detected games to GameInfo
    m_games.clear();
    for (const auto& detected : m_coreBridge->detectedGames()) {
        GameInfo game;
        game.id = detected.id;
        game.name = detected.name;
        game.installPath = detected.installPath;
        game.source = detected.source;
        game.engine = detected.engine;
        game.steamAppId = detected.steamAppId;
        game.isInstalled = true;
        game.hasTranslation = detected.hasTranslation;  // Already set by worker thread
        game.isVerified = game.hasTranslation;  // Verified if translation is installed

        m_games.append(game);
    }

    // Re-add manual games that weren't found by scan (avoid duplicates by ID)
    for (const auto& manual : manualGames) {
        bool duplicate = false;
        for (const auto& g : m_games) {
            if (g.id == manual.id || g.installPath == manual.installPath) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            m_games.append(manual);
    }

    rebuildCache();

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
    qDebug() << "Game detected:" << gameName << "(" << gameId << ")";
}

void GameService::addManualGame(const QString& path)
{
    MAKINE_ZONE_NAMED("GameService::addManualGame");
    // Security: Validate path (sync — fast)
    if (!isValidGamePath(path)) {
        qWarning() << "addManualGame: invalid path" << path;
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "addManualGame: path not found" << path;
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
                    qDebug() << "Fingerprint match:" << matchedAppId
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

    GameInfo game;
    game.installPath = path;
    game.source = "manual";
    game.isInstalled = true;
    game.engine = engine.isEmpty() ? "Unknown" : engine;

    if (!matchedAppId.isEmpty()) {
        game.id = matchedAppId;
        game.steamAppId = matchedAppId;
        game.hasTranslation = true;

        auto pkg = m_coreBridge ? m_coreBridge->getPackageForGame(matchedAppId) : std::nullopt;
        game.name = (pkg.has_value()) ? pkg->gameName : folderName;
    } else {
        game.id = QStringLiteral("manual_%1").arg(m_games.count() + 1);
        game.name = folderName;
        game.hasTranslation = false;
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

    qDebug() << "Manual game added:" << game.name << "id:" << game.id
             << "engine:" << game.engine << "hasTranslation:" << game.hasTranslation;

    // Persist to disk so manual games survive app restart
    saveCachedGames();
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
    QVariantMap result;

    // Contributors + install notes from package manifest
    QVariantList contributors;
    if (m_coreBridge) {
        auto pkg = m_coreBridge->getPackageForGame(gameId);
        if (pkg)
            contributors = pkg->contributors;
        result["installNotes"] = m_coreBridge->getInstallNotesForGame(gameId);
    }
    result["contributors"] = contributors;

    // Runtime status
    QVariantMap runtime = getRuntimeStatus(gameId);
    if (runtime.value("isUnity").toBool()) {
        result["isUnityGame"] = true;
        result["runtimeNeeded"] = runtime.value("needsRuntime");
        result["runtimeInstalled"] = runtime.value("installed");
        result["runtimeUpToDate"] = runtime.value("upToDate");
        result["bepinexVersion"] = runtime.value("bepinexVersion");
        result["xunityVersion"] = runtime.value("xunityVersion");
        result["unityBackend"] = runtime.value("backend", "unknown");
        result["unityVersion"] = runtime.value("unityVersion");
        result["hasAntiCheat"] = runtime.value("hasAntiCheat");
        result["antiCheatName"] = runtime.value("antiCheatName");
    }

    return result;
}

void GameService::fetchSteamDetails(const QString& steamAppId)
{
    MAKINE_ZONE_NAMED("GameService::fetchSteamDetails");
    if (steamAppId.isEmpty()) return;

    // Validate: must be numeric, max 10 digits (prevents URL injection)
    static const QRegularExpression numericOnly(QStringLiteral("^\\d{1,10}$"));
    if (!numericOnly.match(steamAppId).hasMatch()) {
        qWarning() << "Invalid steamAppId format:" << steamAppId;
        return;
    }

    if (m_pendingFetches.contains(steamAppId)) return;

    auto cacheIt = m_steamDetailsCache.constFind(steamAppId);
    if (cacheIt != m_steamDetailsCache.constEnd() && !cacheIt->isExpired()) {
        emit steamDetailsFetched(steamAppId, steamDetailsToVariantMap(*cacheIt));
        return;
    }

    m_pendingFetches.insert(steamAppId);

    QUrl url(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=turkish").arg(steamAppId));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "MakineAI/0.1");

    QNetworkReply* reply = m_networkManager.get(request);

    // Abort if response exceeds size limit (prevents memory exhaustion)
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > kMaxSteamResponseBytes) {
            qWarning() << "Steam API response too large, aborting";
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, steamAppId]() {
        reply->deleteLater();
        m_pendingFetches.remove(steamAppId);

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Steam API error for" << steamAppId << ":" << reply->errorString();
            emit steamDetailsFetchError(steamAppId, reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();

        // Parse JSON on background thread (~100ms main thread → ~0ms)
        auto *watcher = new QFutureWatcher<std::optional<SteamDetails>>(this);
        connect(watcher, &QFutureWatcher<std::optional<SteamDetails>>::finished, this,
                [this, watcher, steamAppId]() {
            watcher->deleteLater();
            auto result = watcher->result();
            if (!result) {
                emit steamDetailsFetchError(steamAppId,
                    QStringLiteral("Failed to parse Steam API response"));
                return;
            }

            // Cache eviction (main thread — accesses m_steamDetailsCache)
            static constexpr int MAX_STEAM_CACHE = 80;
            if (m_steamDetailsCache.size() >= MAX_STEAM_CACHE) {
                QStringList expired;
                for (auto it = m_steamDetailsCache.constBegin();
                     it != m_steamDetailsCache.constEnd(); ++it) {
                    if (it->isExpired())
                        expired.append(it.key());
                }
                for (const auto& key : expired)
                    m_steamDetailsCache.remove(key);
                if (m_steamDetailsCache.size() >= MAX_STEAM_CACHE)
                    m_steamDetailsCache.clear();
            }

            m_steamDetailsCache[steamAppId] = *result;
            saveSteamDetailsCache();
            emit steamDetailsFetched(steamAppId, steamDetailsToVariantMap(*result));
        });

        watcher->setFuture(QtConcurrent::run([steamAppId, data]() {
            return parseSteamJson(steamAppId, data);
        }));
    });
}


QVariantMap GameService::getSteamDetails(const QString& steamAppId)
{
    MAKINE_ZONE_NAMED("GameService::getSteamDetails");
    if (steamAppId.isEmpty()) return {};

    auto cacheIt = m_steamDetailsCache.constFind(steamAppId);
    if (cacheIt != m_steamDetailsCache.constEnd() && !cacheIt->isExpired()) {
        return steamDetailsToVariantMap(*cacheIt);
    }
    return {};
}

QVariantMap GameService::steamDetailsToVariantMap(const SteamDetails& details) const
{
    QVariantList screenshotList;
    screenshotList.reserve(details.screenshots.size());
    for (const auto& url : details.screenshots)
        screenshotList.append(url);

    return {
        {"description", details.description},
        {"developers", QVariant::fromValue(details.developers)},
        {"publishers", QVariant::fromValue(details.publishers)},
        {"releaseDate", details.releaseDate},
        {"genres", QVariant::fromValue(details.genres)},
        {"metacriticScore", details.metacriticScore},
        {"hasWindows", details.hasWindows},
        {"hasMac", details.hasMac},
        {"hasLinux", details.hasLinux},
        {"price", details.price},
        {"discountPercent", details.discountPercent},
        {"screenshots", screenshotList},
        {"backgroundUrl", details.backgroundUrl}
    };
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
    qDebug() << "supportedGames() catalog loaded:" << m_supportedGamesCache.size() << "items";

    // Enrich each entry with local state using O(1) hash lookups
    for (int i = 0; i < m_supportedGamesCache.size(); ++i) {
        QVariantMap entry = m_supportedGamesCache[i].toMap();
        const QString steamAppId = entry[QStringLiteral("steamAppId")].toString();

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
    if (m_installedCacheValid)
        return m_installedTranslationsCache.count();

    if (!m_coreBridge) return 0;
    int count = 0;
    for (const auto& game : m_games) {
        if (m_coreBridge->isPackageInstalled(game.id))
            ++count;
    }
    return count;
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

void GameService::loadCachedGames()
{
    MAKINE_ZONE_NAMED("GameService::loadCachedGames");
    const QString cachePath = AppPaths::gamesCacheFile();
    QFile file(cachePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No cached games found at:" << cachePath;
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse games cache:" << parseError.errorString();
        return;
    }

    if (!doc.isArray()) {
        qWarning() << "Invalid games cache format - expected array";
        return;
    }

    m_games.clear();
    m_games.reserve(doc.array().count());

    for (const auto& value : doc.array()) {
        if (!value.isObject()) continue;

        const QJsonObject obj = value.toObject();
        GameInfo game;
        game.id = obj["id"].toString();
        game.name = obj["name"].toString();
        game.installPath = obj["installPath"].toString();
        game.steamAppId = obj["steamAppId"].toString();
        game.source = obj["source"].toString();
        game.engine = obj["engine"].toString();
        game.isVerified = obj["isVerified"].toBool();
        game.isInstalled = obj["isInstalled"].toBool();
        game.hasTranslation = obj["hasTranslation"].toBool();

        if (!game.id.isEmpty() && !game.name.isEmpty()) {
            m_games.append(game);
        }
    }

    rebuildCache();

    qDebug() << "Loaded" << m_games.count() << "games from cache";
    emit gameListChanged();
    emit translationStatusChanged();
    emit supportedGamesChanged();
    ensureSupportedGamesCache();
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

void GameService::loadSteamDetailsCache()
{
    const QString cachePath = AppPaths::steamDetailsCacheFile();
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject obj = it.value().toObject();
        SteamDetails details;
        details.description = obj["description"].toString();
        details.releaseDate = obj["releaseDate"].toString();
        details.metacriticScore = obj["metacriticScore"].toInt(0);
        details.hasWindows = obj["hasWindows"].toBool(true);
        details.hasMac = obj["hasMac"].toBool(false);
        details.hasLinux = obj["hasLinux"].toBool(false);
        details.price = obj["price"].toString();
        details.discountPercent = obj["discountPercent"].toInt(0);
        details.backgroundUrl = obj["backgroundUrl"].toString();
        details.fetchedAt = QDateTime::fromString(obj["fetchedAt"].toString(), Qt::ISODate);

        for (const auto& v : obj["developers"].toArray()) details.developers.append(v.toString());
        for (const auto& v : obj["publishers"].toArray()) details.publishers.append(v.toString());
        for (const auto& v : obj["genres"].toArray()) details.genres.append(v.toString());
        for (const auto& v : obj["screenshots"].toArray()) details.screenshots.append(v.toString());

        // Skip expired entries
        if (!details.isExpired()) {
            m_steamDetailsCache[it.key()] = details;
        }
    }

    qDebug() << "Loaded" << m_steamDetailsCache.size() << "cached Steam details";
}

void GameService::saveSteamDetailsCache()
{
    // Snapshot the cache (COW — cheap copy) and serialize in background
    QHash<QString, SteamDetails> cacheCopy = m_steamDetailsCache;
    const QString cacheDir = AppPaths::cacheDir();
    const QString cachePath = AppPaths::steamDetailsCacheFile();

    (void)QtConcurrent::run([cacheCopy = std::move(cacheCopy), cacheDir, cachePath]() {
        MAKINE_THREAD_NAME("Worker-SteamCache");
        MAKINE_ZONE_NAMED("saveSteamDetailsCache (async)");

        QDir().mkpath(cacheDir);

        QJsonObject root;
        for (auto it = cacheCopy.constBegin(); it != cacheCopy.constEnd(); ++it) {
            const SteamDetails& details = it.value();
            if (details.isExpired()) continue;

            QJsonObject obj;
            obj["description"] = details.description;
            obj["releaseDate"] = details.releaseDate;
            obj["metacriticScore"] = details.metacriticScore;
            obj["hasWindows"] = details.hasWindows;
            obj["hasMac"] = details.hasMac;
            obj["hasLinux"] = details.hasLinux;
            obj["price"] = details.price;
            obj["discountPercent"] = details.discountPercent;
            obj["backgroundUrl"] = details.backgroundUrl;
            obj["fetchedAt"] = details.fetchedAt.toString(Qt::ISODate);

            QJsonArray devArr, pubArr, genreArr, ssArr;
            for (const auto& v : details.developers) devArr.append(v);
            for (const auto& v : details.publishers) pubArr.append(v);
            for (const auto& v : details.genres) genreArr.append(v);
            for (const auto& v : details.screenshots) ssArr.append(v);

            obj["developers"] = devArr;
            obj["publishers"] = pubArr;
            obj["genres"] = genreArr;
            obj["screenshots"] = ssArr;

            root[it.key()] = obj;
        }

        QFile file(cachePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        }
    });
}

bool GameService::isValidGamePath(const QString& path) const
{
    // Security: Check for path traversal attacks
    if (path.contains("..") || path.contains("//") || path.contains("\\\\")) {
        qWarning() << "Path traversal attempt detected:" << path;
        return false;
    }

    QFileInfo info(path);
    if (!info.isAbsolute()) {
        qWarning() << "Relative path not allowed:" << path;
        return false;
    }

    if (!info.isDir()) {
        qWarning() << "Path is not a directory:" << path;
        return false;
    }

    // Check for suspicious paths (system directories)
    QString normalizedPath = info.absoluteFilePath().toLower();
    QStringList forbiddenPaths = {
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
            qWarning() << "Forbidden system path:" << path;
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
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Core bridge not available"));
        return;
    }

    if (!m_installingGameId.isEmpty()) {
        emit translationInstallCompleted(gameId, false,
            tr("Another installation is in progress"));
        return;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationInstallCompleted(gameId, false, tr("Game not found: %1").arg(gameId));
        return;
    }

    const GameInfo& game = m_games[*it];

    auto pkg = m_coreBridge->getPackageForGame(gameId);
    if (!pkg.has_value()) {
        qWarning() << "GameService::installTranslation: No package found for" << gameId
                   << "- hasTranslationPackage:" << m_coreBridge->hasTranslationPackage(gameId);
        emit translationInstallCompleted(gameId, false,
            tr("No translation package available for this game"));
        return;
    }

    qInfo() << "GameService::installTranslation: Starting for" << gameId
            << "pkg:" << pkg->packageId << "v" << pkg->version;

    if (game.installPath.isEmpty() || !QDir(game.installPath).exists()) {
        emit translationInstallCompleted(gameId, false,
            tr("Game install path not found: %1").arg(game.installPath));
        return;
    }

    // Anti-cheat: skip duplicate check here — InstallFlowController already
    // handles the warning dialog and calls acknowledgeAntiCheat() before
    // reaching installTranslation(). The old code emitted antiCheatWarningNeeded
    // which re-triggered the dialog in a loop.
    // Just consume the acknowledgement flag if present.
    m_antiCheatAcknowledged.remove(gameId);

    // Reserve install slot early to prevent double-install
    m_installingGameId = gameId;
    emit translationInstallStarted(gameId);
    emit translationInstallProgress(gameId, 0.0, tr("Oyun durumu kontrol ediliyor..."));

    // Async game running check (avoids blocking main thread with tasklist)
    QString installPath = game.installPath;
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
        [this, watcher, gameId, installPath, variant, selectedOptions, pkg]() {
            QString runningExe = watcher->result();
            watcher->deleteLater();

            if (!runningExe.isEmpty()) {
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false,
                    tr("Bu oyun şu anda çalışıyor (%1). Çeviriyi kurmak için oyunu kapatın.").arg(runningExe));
                return;
            }

            // Game not running — proceed with backup + install
            BackupManager* bm = BackupManager::instance();
            QStringList filesToOverwrite = m_coreBridge->getPackageFileList(gameId, variant);

            if (bm && !filesToOverwrite.isEmpty()) {
                emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));

                connect(bm, &BackupManager::selectiveBackupCompleted, this,
                    [this, gameId, installPath, variant, selectedOptions](const QString& backupGameId, bool /*success*/) {
                        if (backupGameId != gameId) return;

                        qDebug() << "Installing translation for" << gameId
                                 << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                                 << "options:" << selectedOptions
                                 << "path:" << installPath;

                        m_coreBridge->installPackage(gameId, installPath, variant, selectedOptions);
                    }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

                QString storeVer;
                QString patchVer = pkg.has_value() ? pkg->version : QString();
                bm->createSelectiveBackupAsync(gameId, m_games[m_gameIdToIndex.value(gameId)].name,
                                                installPath, filesToOverwrite, storeVer, patchVer);
            } else {
                qDebug() << "Installing translation for" << gameId
                         << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                         << "options:" << selectedOptions
                         << "path:" << installPath;

                m_coreBridge->installPackage(gameId, installPath, variant, selectedOptions);
            }
        });

    watcher->setFuture(QtConcurrent::run([installPath]() -> QString {
        QDir gameDir(installPath);
        QStringList exeFiles = gameDir.entryList({"*.exe"}, QDir::Files);
        for (const QString& exe : exeFiles) {
            QProcess tasklist;
            tasklist.start("tasklist", {"/FI", "IMAGENAME eq " + exe, "/FO", "CSV", "/NH"});
            tasklist.waitForFinished(2000);
            QString output = QString::fromLocal8Bit(tasklist.readAllStandardOutput());
            if (output.contains(exe, Qt::CaseInsensitive) && !output.contains("INFO:")) {
                return exe;
            }
        }
        return {};
    }));
}

void GameService::updateTranslation(const QString& gameId, const QString& variant,
                                     const QStringList& selectedOptions)
{
    MAKINE_ZONE_NAMED("GameService::updateTranslation");
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Core bridge not available"));
        return;
    }

    if (!m_installingGameId.isEmpty()) {
        emit translationInstallCompleted(gameId, false,
            tr("Another installation is in progress"));
        return;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationInstallCompleted(gameId, false, tr("Game not found: %1").arg(gameId));
        return;
    }

    const GameInfo& game = m_games[*it];

    auto pkg = m_coreBridge->getPackageForGame(gameId);
    if (!pkg.has_value()) {
        emit translationInstallCompleted(gameId, false,
            tr("No translation package available for this game"));
        return;
    }

    if (game.installPath.isEmpty() || !QDir(game.installPath).exists()) {
        emit translationInstallCompleted(gameId, false,
            tr("Game install path not found: %1").arg(game.installPath));
        return;
    }

    // Safety: backup must exist — if not, suggest repair instead
    BackupManager* bm = BackupManager::instance();
    if (bm && !bm->hasBackup(gameId)) {
        emit translationInstallCompleted(gameId, false,
            tr("Yedek bulunamadı. Güncelleme yerine Onarma yapın."));
        return;
    }

    m_installingGameId = gameId;
    emit translationInstallStarted(gameId);
    emit translationInstallProgress(gameId, 0.0, tr("Oyun durumu kontrol ediliyor..."));

    // Async game running check
    QString installPath = game.installPath;
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
        [this, watcher, gameId, installPath, variant, selectedOptions]() {
            QString runningExe = watcher->result();
            watcher->deleteLater();

            if (!runningExe.isEmpty()) {
                m_installingGameId.clear();
                emit translationInstallCompleted(gameId, false,
                    tr("Bu oyun şu anda çalışıyor (%1). Güncelleme için oyunu kapatın.").arg(runningExe));
                return;
            }

            // NO BACKUP STEP — go straight to update
            qDebug() << "Updating translation for" << gameId
                     << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                     << "options:" << selectedOptions
                     << "path:" << installPath;

            m_coreBridge->updatePackage(gameId, installPath, variant, selectedOptions);
        });

    watcher->setFuture(QtConcurrent::run([installPath]() -> QString {
        QDir gameDir(installPath);
        QStringList exeFiles = gameDir.entryList({"*.exe"}, QDir::Files);
        for (const QString& exe : exeFiles) {
            QProcess tasklist;
            tasklist.start("tasklist", {"/FI", "IMAGENAME eq " + exe, "/FO", "CSV", "/NH"});
            tasklist.waitForFinished(2000);
            QString output = QString::fromLocal8Bit(tasklist.readAllStandardOutput());
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
        emit translationUninstalled(gameId, false, tr("Core bridge not available"));
        return;
    }

    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationUninstalled(gameId, false, tr("Game not found"));
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
                qWarning() << "Backup restoration failed for" << gameId
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
        success ? tr("Translation removed successfully")
                : tr("Failed to remove translation"));
}

void GameService::recoverTranslation(const QString& gameId)
{
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Core bridge not available"));
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

    const QString& gamePath = game.installPath;
    if (gamePath.isEmpty())
        return {{"isUnity", true}, {"needsRuntime", true}, {"installed", false},
                {"upToDate", false}, {"bepinexVersion", ""}, {"xunityVersion", ""},
                {"backend", "unknown"}, {"unityVersion", ""},
                {"hasAntiCheat", false}, {"antiCheatName", ""}};

    // Detect Unity backend (Mono vs IL2CPP)
    QString backend = "unknown";
    if (QFile::exists(gamePath + "/GameAssembly.dll"))
        backend = "il2cpp";
    else if (QDir(gamePath).entryList({"*_Data"}, QDir::Dirs).size() > 0)
        backend = "mono";

    // Check if BepInEx is installed (look for BepInEx/core/BepInEx.dll)
    bool bepinexInstalled = QFile::exists(gamePath + "/BepInEx/core/BepInEx.dll")
                         || QFile::exists(gamePath + "/BepInEx/core/BepInEx.Preloader.dll");

    // Check XUnity.AutoTranslator
    bool xunityInstalled = false;
    QString xunityVersion;
    if (bepinexInstalled) {
        QDir pluginsDir(gamePath + "/BepInEx/plugins");
        if (pluginsDir.exists()) {
            auto xunityFiles = pluginsDir.entryList({"XUnity.AutoTranslator*.dll"}, QDir::Files);
            xunityInstalled = !xunityFiles.isEmpty();
            if (!xunityFiles.isEmpty())
                xunityVersion = xunityFiles.first().section('.', 0, -2); // strip .dll
        }
    }

    // Read BepInEx version from changelog or dll name
    QString bepinexVersion;
    if (bepinexInstalled) {
        QDir coreDir(gamePath + "/BepInEx/core");
        auto bepFiles = coreDir.entryList({"BepInEx.dll", "BepInEx.Core.dll"}, QDir::Files);
        if (!bepFiles.isEmpty())
            bepinexVersion = "installed";
    }

    // Anti-cheat from dedicated method
    auto antiCheat = checkAntiCheat(gameId);

    return {
        {"isUnity", true},
        {"needsRuntime", true},
        {"installed", bepinexInstalled},
        {"upToDate", bepinexInstalled && xunityInstalled},
        {"bepinexVersion", bepinexVersion},
        {"xunityVersion", xunityVersion},
        {"backend", backend},
        {"unityVersion", ""},
        {"hasAntiCheat", antiCheat.value("hasAntiCheat").toBool()},
        {"antiCheatName", antiCheat.value("systems").toList().isEmpty() ? ""
            : antiCheat.value("systems").toList().first().toMap().value("name").toString()}
    };
}

void GameService::installRuntime(const QString& gameId)
{
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.size()) {
        emit runtimeInstallFinished(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    const GameInfo& game = m_games[*it];
    if (!game.engine.toLower().contains("unity")) {
        emit runtimeInstallFinished(gameId, false, tr("Bu oyun Unity tabanlı değil"));
        return;
    }

    // Check if already installed
    if (QFile::exists(game.installPath + "/BepInEx/core/BepInEx.dll")) {
        emit runtimeInstallFinished(gameId, true, tr("BepInEx zaten kurulu"));
        return;
    }

    emit runtimeInstallFinished(gameId, false,
        tr("BepInEx paketi henüz mevcut değil. Manuel kurulum için: https://github.com/BepInEx/BepInEx/releases"));
}

void GameService::uninstallRuntime(const QString& gameId)
{
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.size()) {
        emit runtimeInstallFinished(gameId, false, tr("Oyun bulunamadı"));
        return;
    }

    const GameInfo& game = m_games[*it];
    const QString bepinexDir = game.installPath + "/BepInEx";

    if (!QDir(bepinexDir).exists()) {
        emit runtimeInstallFinished(gameId, false, tr("BepInEx kurulu değil"));
        return;
    }

    // Remove BepInEx directory and related files
    bool removed = QDir(bepinexDir).removeRecursively();
    // Also remove doorstop files
    QFile::remove(game.installPath + "/winhttp.dll");
    QFile::remove(game.installPath + "/doorstop_config.ini");

    emit runtimeInstallFinished(gameId, removed,
        removed ? tr("BepInEx kaldırıldı") : tr("BepInEx kaldırılırken hata oluştu"));
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
    return QDir(AppPaths::packagesDir() + QStringLiteral("/") + dirName).exists();
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
    qDebug() << "GameService::getCatalogEntry: not found for" << steamAppId;
    return {};
}

void GameService::checkForUpdates()
{
    qInfo() << "GameService: checking for updates...";

    // Re-sync manifest from CDN
    if (m_manifestSync)
        m_manifestSync->syncCatalog();

    // Rescan game libraries
    scanAllLibraries();

}

} // namespace makineai
