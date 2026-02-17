/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "gameservice.h"
#include "backupmanager.h"
#include "updatedetectionservice.h"
#include "apppaths.h"
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

namespace {
constexpr int kAutoScanDelayMs = 500;
constexpr qint64 kMaxSteamResponseBytes = 5 * 1024 * 1024; // 5 MB
}

namespace makineai {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_updateService(new UpdateDetectionService(this))
{
    m_updateService->setGameService(this);

    // Forward update detection signals
    connect(m_updateService, &UpdateDetectionService::gameUpdateDetected,
            this, &GameService::gameUpdateDetected);
    connect(m_updateService, &UpdateDetectionService::gamesWithUpdatesChanged,
            this, &GameService::gameUpdateCountChanged);

    // Start monitoring after first scan completes
    connect(this, &GameService::scanCompleted, this, [this](int) {
        m_updateService->startMonitoring();
    }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
}

void GameService::initialize()
{
    setupCoreBridge();
    loadCachedGames();
    loadSteamDetailsCache();

    // Auto-scan on first launch if no cached games
    if (m_games.isEmpty()) {
        qDebug() << "No cached games found, starting auto-scan...";
        QTimer::singleShot(kAutoScanDelayMs, this, &GameService::scanAllLibraries);
    }

    emit gamesChanged();
}

GameService::~GameService() = default;

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

    m_gamesCache.clear();
    m_gamesCache.reserve(m_games.count());

    for (const auto& game : m_games) {
        m_gamesCache.append(game.toVariantMap());
    }

    m_cacheValid = true;
    return m_gamesCache;
}

void GameService::scanAllLibraries()
{
    if (m_isScanning) return;

    m_isScanning = true;
    emit isScanningChanged();

    m_scanProgress = 0;
    m_scanStatus = tr("Oyun kütüphaneleri taranıyor...");
    emit scanStatusChanged();

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
    connect(m_coreBridge, &CoreBridge::scanError,
            this, &GameService::scanError);

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
                            invalidateCache();
                            emit gamesChanged();

                            // Record store version + take file snapshot
                            const auto& game = m_games[idx];
                            auto pkg = m_coreBridge->getPackageForGame(gameId);
                            QString patchVer = pkg.has_value() ? pkg->version : "unknown";
                            m_updateService->recordStoreVersion(gameId, game.installPath, game.source);
                            m_updateService->takeSnapshot(gameId, patchVer, game.installPath, game.engine);
                            m_updateService->clearUpdate(gameId);
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
    if (changed) {
        emit scanStatusChanged();
    }
}

void GameService::onScanCompleted(int count)
{
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
        game.headerImageUrl = detected.headerImageUrl;
        game.isVerified = detected.isVerified;
        game.isInstalled = true;

        // Check for translation package
        game.hasTranslation = m_coreBridge->hasTranslationPackage(detected.id);

        m_games.append(game);
    }

    // Rebuild lookup cache after loading games
    rebuildCache();

    m_isScanning = false;
    emit isScanningChanged();
    emit gamesChanged();
    emit scanCompleted(count);

    saveCachedGames();
    ensureSupportedGamesCache();
}

void GameService::onGameDetected(const QString& gameId, const QString& gameName)
{
    emit gameDetected(gameId);
    qDebug() << "Game detected:" << gameName << "(" << gameId << ")";
}

QString GameService::addManualGame(const QString& path)
{
    // Security: Validate path
    if (!isValidGamePath(path)) {
        emit scanError(tr("Geçersiz oyun klasörü: %1").arg(path));
        return {};
    }

    QDir dir(path);
    if (!dir.exists()) {
        emit scanError(tr("Belirtilen klasör bulunamadı: %1").arg(path));
        return {};
    }

    // Check for duplicate (case-insensitive on Windows)
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    for (const auto& game : m_games) {
        if (QFileInfo(game.installPath).canonicalFilePath() == canonicalPath) {
            // Already exists — return existing game's ID
            return game.id;
        }
    }

    const QString folderName = dir.dirName();

    // Detect engine
    QString engine;
    if (m_coreBridge) {
        engine = m_coreBridge->detectEngine(path);
    }

    // Match folder name against translation catalog
    QString matchedAppId;
    if (m_coreBridge) {
        matchedAppId = m_coreBridge->findMatchingAppId(folderName);
    }

    GameInfo game;
    game.installPath = path;
    game.source = "manual";
    game.isInstalled = true;
    game.engine = engine.isEmpty() ? "Unknown" : engine;

    if (!matchedAppId.isEmpty()) {
        // Found a matching translation package
        game.id = matchedAppId;
        game.steamAppId = matchedAppId;
        game.hasTranslation = true;
        game.headerImageUrl = QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/library_600x900_2x.jpg").arg(matchedAppId);

        // Use catalog game name instead of folder name
        auto pkg = m_coreBridge->getPackageForGame(matchedAppId);
        game.name = pkg.has_value() ? pkg->gameName : folderName;
    } else {
        game.id = QStringLiteral("manual_%1").arg(m_games.count() + 1);
        game.name = folderName;
        game.hasTranslation = false;
    }

    // Avoid duplicate ID collision (e.g. steam scan already found this game)
    if (m_gameIdToIndex.contains(game.id)) {
        // Update existing entry's install path
        int idx = m_gameIdToIndex[game.id];
        if (idx >= 0 && idx < m_games.count()) {
            m_games[idx].installPath = path;
            m_games[idx].isInstalled = true;
            invalidateCache();
            emit gamesChanged();
            return game.id;
        }
    }

    m_games.append(game);
    m_gameIdToIndex[game.id] = m_games.count() - 1;

    invalidateCache();
    emit gamesChanged();
    emit gameDetected(game.id);

    qDebug() << "Manual game added:" << game.name << "id:" << game.id
             << "engine:" << game.engine << "hasTranslation:" << game.hasTranslation;

    return game.id;
}

QVariantMap GameService::getGameById(const QString& id) const
{
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

QVariantList GameService::filterGames(const QString& query) const
{
    if (query.isEmpty())
        return supportedGames();

    const QVariantList allGames = supportedGames();
    QVariantList result;
    result.reserve(allGames.size());
    for (const auto& entry : allGames) {
        const QVariantMap map = entry.toMap();
        const QString name = map.value("name").toString();
        if (name.contains(query, Qt::CaseInsensitive))
            result.append(entry);
    }
    result.squeeze();
    return result;
}

QVariantList GameService::filteredGamesWithTranslation(const QString& filter) const
{
    const QVariantList allGames = supportedGames();
    QVariantList result;
    result.reserve(allGames.size());
    for (const auto& entry : allGames) {
        const QVariantMap map = entry.toMap();
        if (!map.value("hasTranslation").toBool())
            continue;
        if (!filter.isEmpty()) {
            const QString name = map.value("name").toString();
            if (!name.contains(filter, Qt::CaseInsensitive))
                continue;
        }
        result.append(entry);
    }
    result.squeeze();
    return result;
}

QString GameService::classifyDroppedUrls(const QVariantList& urls) const
{
    if (urls.isEmpty())
        return QStringLiteral("unknown");

    for (const auto& urlVar : urls) {
        const QString urlStr = urlVar.toString().toLower();
        if (urlStr.endsWith(QLatin1String(".mkpkg")))
            return QStringLiteral("package");
        if (urlStr.endsWith(QLatin1String(".zip")) ||
            urlStr.endsWith(QLatin1String(".rar")) ||
            urlStr.endsWith(QLatin1String(".7z")))
            return QStringLiteral("archive");
    }

    // Check if first URL looks like a folder (no extension)
    const QString first = urls.first().toString();
    if (!first.contains(QLatin1Char('.')) ||
        first.endsWith(QLatin1Char('/')) ||
        first.endsWith(QLatin1Char('\\')))
        return QStringLiteral("folder");

    return QStringLiteral("unknown");
}

QVariantMap GameService::getGameDetails(const QString& gameId)
{
    QVariantMap result;

    // Contributors from package manifest
    QVariantList contributors;
    if (m_coreBridge) {
        auto pkg = m_coreBridge->getPackageForGame(gameId);
        if (pkg)
            contributors = pkg->contributors;
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
    if (steamAppId.isEmpty()) return;

    // Validate: must be numeric, max 10 digits (prevents URL injection)
    static const QRegularExpression numericOnly(QStringLiteral("^\\d{1,10}$"));
    if (!numericOnly.match(steamAppId).hasMatch()) {
        qWarning() << "Invalid steamAppId format:" << steamAppId;
        return;
    }

    // Prevent duplicate requests
    if (m_pendingFetches.contains(steamAppId)) return;

    // Check cache (not expired)
    auto cacheIt = m_steamDetailsCache.constFind(steamAppId);
    if (cacheIt != m_steamDetailsCache.constEnd() && !cacheIt->isExpired()) {
        emit steamDetailsFetched(steamAppId, steamDetailsToVariantMap(*cacheIt));
        return;
    }

    m_pendingFetches.insert(steamAppId);

    QUrl url(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=turkish").arg(steamAppId));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "MakineAI/0.1");

    QNetworkReply* reply = m_networkManager->get(request);

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

        parseSteamApiResponse(steamAppId, reply->readAll());
    });
}

void GameService::parseSteamApiResponse(const QString& steamAppId, const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Steam API JSON parse error:" << parseError.errorString();
        emit steamDetailsFetchError(steamAppId, parseError.errorString());
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject appObj = root.value(steamAppId).toObject();

    if (!appObj.value("success").toBool()) {
        emit steamDetailsFetchError(steamAppId, QStringLiteral("Steam API returned success=false"));
        return;
    }

    const QJsonObject appData = appObj.value("data").toObject();
    if (appData.isEmpty()) {
        emit steamDetailsFetchError(steamAppId, QStringLiteral("Steam API returned empty data"));
        return;
    }

    SteamDetails details;
    details.description = appData.value("short_description").toString();
    details.releaseDate = appData.value("release_date").toObject().value("date").toString();

    // Developers & publishers
    for (const auto& dev : appData.value("developers").toArray())
        details.developers.append(dev.toString());
    for (const auto& pub : appData.value("publishers").toArray())
        details.publishers.append(pub.toString());

    // Genres
    for (const auto& genre : appData.value("genres").toArray())
        details.genres.append(genre.toObject().value("description").toString());

    // Metacritic
    const QJsonObject metacritic = appData.value("metacritic").toObject();
    details.metacriticScore = metacritic.value("score").toInt(0);

    // Platforms
    const QJsonObject platforms = appData.value("platforms").toObject();
    details.hasWindows = platforms.value("windows").toBool(true);
    details.hasMac = platforms.value("mac").toBool(false);
    details.hasLinux = platforms.value("linux").toBool(false);

    // Price
    if (appData.value("is_free").toBool()) {
        details.price = tr("Ücretsiz");
    } else {
        const QJsonObject priceObj = appData.value("price_overview").toObject();
        details.price = priceObj.value("final_formatted").toString();
        details.discountPercent = priceObj.value("discount_percent").toInt(0);
    }

    // Screenshots
    for (const auto& ss : appData.value("screenshots").toArray()) {
        const QString thumbUrl = ss.toObject().value("path_thumbnail").toString();
        if (!thumbUrl.isEmpty())
            details.screenshots.append(thumbUrl);
    }

    // Background
    details.backgroundUrl = appData.value("background").toString();
    details.fetchedAt = QDateTime::currentDateTime();

    // Cache and persist
    m_steamDetailsCache[steamAppId] = details;
    saveSteamDetailsCache();

    emit steamDetailsFetched(steamAppId, steamDetailsToVariantMap(details));
}

QVariantMap GameService::getSteamDetails(const QString& steamAppId)
{
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
    if (m_supportedCacheValid && !m_supportedGamesCache.isEmpty()) {
        return m_supportedGamesCache;
    }

    if (!m_coreBridge) return {};

    m_supportedGamesCache = m_coreBridge->allSupportedGames();

    // Enrich with cached packageInstalled status
    for (int i = 0; i < m_supportedGamesCache.size(); ++i) {
        QVariantMap entry = m_supportedGamesCache[i].toMap();
        const QString steamAppId = entry["steamAppId"].toString();
        auto cacheIt = m_packageInstalledCache.constFind(steamAppId);
        if (cacheIt != m_packageInstalledCache.constEnd()) {
            entry["packageInstalled"] = *cacheIt;
        } else {
            bool installed = m_coreBridge->isPackageInstalled(steamAppId);
            m_packageInstalledCache[steamAppId] = installed;
            entry["packageInstalled"] = installed;
        }
        m_supportedGamesCache[i] = entry;
    }

    m_supportedCacheValid = true;

    return m_supportedGamesCache;
}

int GameService::supportedGameCount() const
{
    if (!m_coreBridge) return 0;
    return m_coreBridge->supportedGameCount();
}

QVariantList GameService::gamesWithTranslation() const
{
    QVariantList result;
    for (const auto& game : m_games) {
        bool hasPackage = m_coreBridge && m_coreBridge->hasTranslationPackage(game.id);
        if (game.hasTranslation || hasPackage) {
            QVariantMap map = game.toVariantMap();
            auto cacheIt = m_packageInstalledCache.constFind(game.id);
            if (cacheIt != m_packageInstalledCache.constEnd()) {
                map["packageInstalled"] = *cacheIt;
            } else if (m_coreBridge) {
                bool installed = m_coreBridge->isPackageInstalled(game.id);
                m_packageInstalledCache[game.id] = installed;
                map["packageInstalled"] = installed;
            } else {
                map["packageInstalled"] = false;
            }
            result.append(map);
        }
    }
    return result;
}

void GameService::loadCachedGames()
{
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
        game.headerImageUrl = obj["headerImageUrl"].toString();
        game.logoImageUrl = obj["logoImageUrl"].toString();
        game.installPath = obj["installPath"].toString();
        game.steamAppId = obj["steamAppId"].toString();
        game.source = obj["source"].toString();
        game.engine = obj["engine"].toString();
        game.isVerified = obj["isVerified"].toBool();
        game.isInstalled = obj["isInstalled"].toBool();
        game.hasTranslation = obj["hasTranslation"].toBool();

        // Validate required fields
        if (!game.id.isEmpty() && !game.name.isEmpty()) {
            m_games.append(game);
        }
    }

    // Build lookup index
    rebuildCache();

    qDebug() << "Loaded" << m_games.count() << "games from cache";
    emit gamesChanged();
    ensureSupportedGamesCache();
}

void GameService::saveCachedGames()
{
    QDir().mkpath(AppPaths::cacheDir());

    QJsonArray array;
    for (const auto& game : m_games) {
        QJsonObject obj;
        obj["id"] = game.id;
        obj["name"] = game.name;
        obj["headerImageUrl"] = game.headerImageUrl;
        obj["logoImageUrl"] = game.logoImageUrl;
        obj["installPath"] = game.installPath;
        obj["steamAppId"] = game.steamAppId;
        obj["source"] = game.source;
        obj["engine"] = game.engine;
        obj["isVerified"] = game.isVerified;
        obj["isInstalled"] = game.isInstalled;
        obj["hasTranslation"] = game.hasTranslation;
        array.append(obj);
    }

    QFile file(AppPaths::gamesCacheFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
    }
}

void GameService::invalidateCache()
{
    m_cacheValid = false;
    m_supportedCacheValid = false;
    m_gamesCache.clear();
    m_supportedGamesCache.clear();
    m_packageInstalledCache.clear();
}

void GameService::rebuildCache()
{
    // Rebuild game ID index
    m_gameIdToIndex.clear();
    m_gameIdToIndex.reserve(m_games.count());

    for (int i = 0; i < m_games.count(); ++i) {
        m_gameIdToIndex[m_games[i].id] = i;
    }

    invalidateCache();
}

void GameService::ensureSupportedGamesCache()
{
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
    QDir().mkpath(AppPaths::cacheDir());

    QJsonObject root;
    for (auto it = m_steamDetailsCache.constBegin(); it != m_steamDetailsCache.constEnd(); ++it) {
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

    QFile file(AppPaths::steamDetailsCacheFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

bool GameService::isValidGamePath(const QString& path) const
{
    // Security: Check for path traversal attacks
    if (path.contains("..") || path.contains("//") || path.contains("\\\\")) {
        qWarning() << "Path traversal attempt detected:" << path;
        return false;
    }

    // Check absolute path
    QFileInfo info(path);
    if (!info.isAbsolute()) {
        qWarning() << "Relative path not allowed:" << path;
        return false;
    }

    // Verify it's a directory
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

void GameService::handleDroppedFiles(const QVariantList& urls) {
    for (const auto& urlVar : urls) {
        QString urlStr = urlVar.toString();

        // Convert file:// URL to local path
        QUrl url(urlStr);
        QString filePath = url.isLocalFile() ? url.toLocalFile() : urlStr;

        QFileInfo info(filePath);
        if (!info.exists()) {
            qWarning() << "Dropped file does not exist:" << filePath;
            continue;
        }

        if (info.isDir()) {
            // Folder dropped — try to detect as game directory
            qDebug() << "Folder dropped:" << filePath;
            addManualGame(filePath);
            emit folderDropped(filePath, true);
        } else {
            QString ext = info.suffix().toLower();

            if (ext == "mkpkg") {
                // Translation package
                qDebug() << "Package dropped:" << filePath;
                installLocalPackage(filePath);
            } else if (ext == "zip" || ext == "rar" || ext == "7z") {
                // Archive — for now, emit as package attempt
                // Full archive extraction would need libarchive integration
                qDebug() << "Archive dropped:" << filePath;
                emit localPackageError(filePath,
                    tr("Archive format not yet supported. Please use .mkpkg packages."));
            } else {
                qDebug() << "Unknown file type dropped:" << filePath;
                emit localPackageError(filePath,
                    tr("Unsupported file type: .%1").arg(ext));
            }
        }
    }
}

void GameService::installLocalPackage(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        emit localPackageError(filePath, tr("File not found"));
        return;
    }

    if (info.suffix().toLower() != "mkpkg") {
        emit localPackageError(filePath, tr("Not a valid .mkpkg package"));
        return;
    }

    // Read the package manifest to get game info
    // In a full implementation, this would use PackageBuilder::inspect()
    // For now, extract basic info from filename convention: gamename-version.mkpkg
    QString baseName = info.completeBaseName(); // e.g. "hollow-knight-tr-2.1.0"

    // Try to parse name-version pattern
    QString packageName = baseName;
    QString gameName = baseName;

    // Look for last hyphen followed by version-like string
    int lastHyphen = baseName.lastIndexOf('-');
    if (lastHyphen > 0) {
        QString possibleVersion = baseName.mid(lastHyphen + 1);
        // Simple version check: starts with digit
        if (!possibleVersion.isEmpty() && possibleVersion[0].isDigit()) {
            gameName = baseName.left(lastHyphen);
        }
    }

    // Replace hyphens with spaces for display
    gameName.replace('-', ' ');

    qDebug() << "Installing local package:" << packageName << "for game:" << gameName;

    emit localPackageReady(packageName, gameName, filePath);

    // Delegate to CoreBridge for actual installation
    if (m_coreBridge) {
        m_coreBridge->installPackage(baseName, info.absoluteFilePath());
    }
}

QVariantList GameService::getVariants(const QString& gameId)
{
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantsForGame(gameId);
}

QString GameService::getVariantType(const QString& gameId)
{
    if (!m_coreBridge) return {};
    return m_coreBridge->getVariantTypeForGame(gameId);
}

QString GameService::getInstallNotes(const QString& gameId)
{
    if (!m_coreBridge) return {};
    return m_coreBridge->getInstallNotesForGame(gameId);
}

void GameService::installTranslation(const QString& gameId, const QString& variant)
{
    if (!m_coreBridge) {
        emit translationInstallCompleted(gameId, false, tr("Core bridge not available"));
        return;
    }

    // Prevent concurrent installs
    if (!m_installingGameId.isEmpty()) {
        emit translationInstallCompleted(gameId, false,
            tr("Another installation is in progress"));
        return;
    }

    // Look up game info
    auto it = m_gameIdToIndex.constFind(gameId);
    if (it == m_gameIdToIndex.constEnd() || *it < 0 || *it >= m_games.count()) {
        emit translationInstallCompleted(gameId, false, tr("Game not found: %1").arg(gameId));
        return;
    }

    const GameInfo& game = m_games[*it];

    // Check if package exists for this game
    auto pkg = m_coreBridge->getPackageForGame(gameId);
    if (!pkg.has_value()) {
        emit translationInstallCompleted(gameId, false,
            tr("No translation package available for this game"));
        return;
    }

    // Verify install path exists
    if (game.installPath.isEmpty() || !QDir(game.installPath).exists()) {
        emit translationInstallCompleted(gameId, false,
            tr("Game install path not found: %1").arg(game.installPath));
        return;
    }

    m_installingGameId = gameId;
    emit translationInstallStarted(gameId);

    // Selective async backup: only backup files that translation will overwrite
    BackupManager* bm = BackupManager::instance();
    QStringList filesToOverwrite = m_coreBridge->getPackageFileList(gameId, variant);

    if (bm && !filesToOverwrite.isEmpty()) {
        emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));

        // One-shot connection: when backup done, proceed with install
        connect(bm, &BackupManager::selectiveBackupCompleted, this,
            [this, gameId, gamePath = game.installPath, variant](const QString& backupGameId, bool /*success*/) {
                if (backupGameId != gameId) return;

                qDebug() << "Installing translation for" << gameId
                         << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                         << "path:" << gamePath;

                m_coreBridge->installPackage(gameId, gamePath, variant);
            }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

        bm->createSelectiveBackupAsync(gameId, game.name, game.installPath, filesToOverwrite);
    } else {
        // No backup needed — proceed directly
        qDebug() << "Installing translation for" << game.name
                 << "gameId:" << gameId
                 << "variant:" << (variant.isEmpty() ? "(none)" : variant)
                 << "path:" << game.installPath;

        m_coreBridge->installPackage(gameId, game.installPath, variant);
    }
}

void GameService::uninstallTranslation(const QString& gameId)
{
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
    bool success = m_coreBridge->uninstallPackage(gameId, gamePath);

    if (success && gameIndex >= 0 && gameIndex < m_games.count()) {
        m_games[gameIndex].hasTranslation = false;
        m_packageInstalledCache[gameId] = false;
        invalidateCache();
        emit gamesChanged();

        m_updateService->removeSnapshot(gameId);
        m_updateService->removeStoreVersion(gameId);
    }

    emit translationUninstalled(gameId, success,
        success ? tr("Translation removed successfully")
                : tr("Failed to remove translation"));
}

void GameService::setUpdateMonitoringEnabled(bool enabled)
{
    if (enabled)
        m_updateService->startMonitoring();
    else
        m_updateService->stopMonitoring();
}

int GameService::gameUpdateCount() const
{
    return m_updateService->gamesWithUpdates();
}

bool GameService::hasGameUpdate(const QString& gameId) const
{
    return m_updateService->hasUpdate(gameId);
}

QVariantMap GameService::checkCompatibility(const QString& gameId)
{
    return m_updateService->checkCompatibility(gameId);
}

QVariantMap GameService::getRuntimeStatus(const QString& gameId)
{
    auto it = m_gameIdToIndex.constFind(gameId);
    bool isUnity = false;
    if (it != m_gameIdToIndex.constEnd() && *it >= 0 && *it < m_games.size())
        isUnity = m_games[*it].engine.toLower().contains("unity");

    if (!isUnity)
        return {{"isUnity", false}, {"needsRuntime", false}};

    return {{"isUnity", true}, {"needsRuntime", true}, {"installed", false},
            {"upToDate", false}, {"bepinexVersion", ""}, {"xunityVersion", ""},
            {"backend", "unknown"}, {"unityVersion", ""},
            {"hasAntiCheat", false}, {"antiCheatName", ""}};
}

void GameService::installRuntime(const QString& gameId)
{
    Q_UNUSED(gameId)
    QTimer::singleShot(100, this, [this, gameId]() {
        emit runtimeInstallFinished(gameId, false, tr("Runtime not available yet"));
    });
}

void GameService::uninstallRuntime(const QString& gameId)
{
    Q_UNUSED(gameId)
    QTimer::singleShot(100, this, [this, gameId]() {
        emit runtimeInstallFinished(gameId, false, tr("Runtime not available yet"));
    });
}

QVariantMap GameService::checkAntiCheat(const QString& gameId)
{
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

} // namespace makineai
