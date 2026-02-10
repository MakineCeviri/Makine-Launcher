/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "gameservice.h"
#include "backupmanager.h"
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

namespace makineai {

GameService::GameService(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    setupCoreBridge();
    loadCachedGames();
    loadSteamDetailsCache();

    // Auto-scan on first launch if no cached games
    if (m_games.isEmpty()) {
        qDebug() << "No cached games found, starting auto-scan...";
        QTimer::singleShot(500, this, &GameService::scanAllLibraries);
    }
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

QVariantList GameService::featuredGames() const
{
    if (m_cacheValid && !m_featuredGamesCache.isEmpty()) {
        return m_featuredGamesCache;
    }

    m_featuredGamesCache.clear();
    m_featuredGamesCache.reserve(m_featuredIds.size());

    for (const auto& game : m_games) {
        if (m_featuredIds.contains(game.id)) {
            m_featuredGamesCache.append(game.toSummary());
        }
    }
    return m_featuredGamesCache;
}

QVariantList GameService::recentGames() const
{
    if (m_cacheValid && !m_recentGamesCache.isEmpty()) {
        return m_recentGamesCache;
    }

    m_recentGamesCache.clear();
    m_recentGamesCache.reserve(m_recentIds.size());

    for (const auto& game : m_games) {
        if (m_recentIds.contains(game.id)) {
            m_recentGamesCache.append(game.toSummary());
        }
    }
    return m_recentGamesCache;
}

void GameService::scanAllLibraries()
{
    if (m_isScanning) return;

    m_isScanning = true;
    emit isScanningChanged();

    m_scanProgress = 0;
    m_scanStatus = tr("Oyun kütüphaneleri taranıyor...");
    emit scanStatusChanged();
    emit scanProgressChanged();

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
            this, [this](const QString& error) {
                m_lastError = error;
                emit lastErrorChanged();
                emit scanError(error);
            });

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
                    // Update game's hasTranslation flag
                    if (m_gameIdToIndex.contains(gameId)) {
                        int idx = m_gameIdToIndex[gameId];
                        if (idx >= 0 && idx < m_games.count()) {
                            m_games[idx].hasTranslation = true;
                            invalidateCache();
                            emit gamesChanged();
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
    m_scanProgress = progress;
    m_scanStatus = status;
    emit scanProgressChanged();
    emit scanStatusChanged();
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
}

void GameService::onGameDetected(const QString& gameId, const QString& gameName)
{
    emit gameDetected(gameId);
    qDebug() << "Game detected:" << gameName << "(" << gameId << ")";
}

void GameService::scanSteamLibrary()
{
    if (m_isScanning) return;

    m_isScanning = true;
    m_scanStatus = tr("Steam kütüphanesi taranıyor...");
    emit isScanningChanged();
    emit scanStatusChanged();

    if (m_coreBridge) {
        m_coreBridge->scanSteamLibrary();
    }
}

void GameService::scanEpicLibrary()
{
    if (m_isScanning) return;

    m_isScanning = true;
    m_scanStatus = tr("Epic Games taranıyor...");
    emit isScanningChanged();
    emit scanStatusChanged();

    if (m_coreBridge) {
        m_coreBridge->scanEpicLibrary();
    }
}

void GameService::scanGogLibrary()
{
    if (m_isScanning) return;

    m_isScanning = true;
    m_scanStatus = tr("GOG Galaxy taranıyor...");
    emit isScanningChanged();
    emit scanStatusChanged();

    if (m_coreBridge) {
        m_coreBridge->scanGogLibrary();
    }
}

void GameService::addManualGame(const QString& path)
{
    // Security: Validate path
    if (!isValidGamePath(path)) {
        emit scanError(tr("Geçersiz oyun klasörü: %1").arg(path));
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        emit scanError(tr("Belirtilen klasör bulunamadı: %1").arg(path));
        return;
    }

    // Check for duplicate (case-insensitive on Windows)
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    for (const auto& game : m_games) {
        if (QFileInfo(game.installPath).canonicalFilePath() == canonicalPath) {
            emit scanError(tr("Bu oyun zaten eklenmiş: %1").arg(path));
            return;
        }
    }

    GameInfo game;
    game.id = QString::number(m_games.count() + 1);
    game.name = dir.dirName();
    game.installPath = path;
    game.source = "manual";
    game.isInstalled = true;

    m_games.append(game);

    // Update index
    m_gameIdToIndex[game.id] = m_games.count() - 1;

    invalidateCache();
    emit gamesChanged();
    emit gameDetected(game.id);
}

QVariantMap GameService::getGameById(const QString& id)
{
    if (m_gameIdToIndex.contains(id)) {
        int index = m_gameIdToIndex[id];
        if (index >= 0 && index < m_games.count()) {
            return m_games[index].toVariantMap();
        }
    }
    return {};
}

bool GameService::hasRecipe(const QString& gameId)
{
    if (m_coreBridge) {
        return m_coreBridge->hasTranslationPackage(gameId);
    }
    return false;
}

void GameService::refreshGameMetadata(const QString& gameId)
{
    if (m_gameIdToIndex.contains(gameId)) {
        int idx = m_gameIdToIndex[gameId];
        if (idx >= 0 && idx < m_games.count()) {
            const QString& appId = m_games[idx].steamAppId;
            if (!appId.isEmpty()) {
                fetchSteamDetails(appId);
            }
        }
    }
}

void GameService::fetchSteamDetails(const QString& steamAppId)
{
    if (steamAppId.isEmpty()) return;

    // Prevent duplicate requests
    if (m_pendingFetches.contains(steamAppId)) return;

    // Check cache (not expired)
    if (m_steamDetailsCache.contains(steamAppId) && !m_steamDetailsCache[steamAppId].isExpired()) {
        emit steamDetailsFetched(steamAppId, steamDetailsToVariantMap(m_steamDetailsCache[steamAppId]));
        return;
    }

    m_pendingFetches.insert(steamAppId);
    emit isFetchingSteamDetailsChanged();

    QUrl url(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=turkish").arg(steamAppId));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "MakineAI/0.1");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, steamAppId]() {
        reply->deleteLater();
        m_pendingFetches.remove(steamAppId);
        emit isFetchingSteamDetailsChanged();

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

    if (m_steamDetailsCache.contains(steamAppId) && !m_steamDetailsCache[steamAppId].isExpired()) {
        return steamDetailsToVariantMap(m_steamDetailsCache[steamAppId]);
    }
    return {};
}

QVariantMap GameService::getRecipeInfo(const QString& gameId)
{
    if (!m_coreBridge) return {};

    auto pkg = m_coreBridge->getPackageForGame(gameId);
    if (!pkg.has_value()) return {};

    return {
        {"packageId", pkg->packageId},
        {"version", pkg->version},
        {"gameName", pkg->gameName},
        {"sizeBytes", pkg->sizeBytes},
        {"hasRecipe", true}
    };
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

QVariantList GameService::searchGames(const QString& query)
{
    QVariantList result;
    const QString lowerQuery = query.toLower();

    for (const auto& game : m_games) {
        if (game.name.toLower().contains(lowerQuery) ||
            game.id.contains(query)) {
            result.append(game.toSummary());
        }
    }
    return result;
}

int GameService::patchedGamesCount() const
{
    int count = 0;
    for (const auto& game : m_games) {
        if (game.hasTranslation) {
            ++count;
        }
    }
    return count;
}

QVariantList GameService::supportedGames() const
{
    if (m_cacheValid && !m_supportedGamesCache.isEmpty()) {
        return m_supportedGamesCache;
    }

    if (!m_coreBridge) return {};

    m_supportedGamesCache = m_coreBridge->allSupportedGames();

    // Enrich with packageInstalled status
    for (int i = 0; i < m_supportedGamesCache.size(); ++i) {
        QVariantMap entry = m_supportedGamesCache[i].toMap();
        const QString steamAppId = entry["steamAppId"].toString();
        entry["packageInstalled"] = m_coreBridge->isPackageInstalled(steamAppId);
        m_supportedGamesCache[i] = entry;
    }

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
            map["packageInstalled"] = m_coreBridge ? m_coreBridge->isPackageInstalled(game.id) : false;
            result.append(map);
        }
    }
    return result;
}

void GameService::loadCachedGames()
{
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/games_cache.json";
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
}

void GameService::saveCachedGames()
{
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(cachePath);

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

    QFile file(cachePath + "/games_cache.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
    }
}

void GameService::invalidateCache()
{
    m_cacheValid = false;
    m_gamesCache.clear();
    m_featuredGamesCache.clear();
    m_recentGamesCache.clear();
    m_supportedGamesCache.clear();
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

void GameService::loadSteamDetailsCache()
{
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/steam_details_cache.json";
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
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dirPath);

    QJsonObject root;
    for (auto it = m_steamDetailsCache.constBegin(); it != m_steamDetailsCache.constEnd(); ++it) {
        const SteamDetails& d = it.value();
        if (d.isExpired()) continue;

        QJsonObject obj;
        obj["description"] = d.description;
        obj["releaseDate"] = d.releaseDate;
        obj["metacriticScore"] = d.metacriticScore;
        obj["hasWindows"] = d.hasWindows;
        obj["hasMac"] = d.hasMac;
        obj["hasLinux"] = d.hasLinux;
        obj["price"] = d.price;
        obj["discountPercent"] = d.discountPercent;
        obj["backgroundUrl"] = d.backgroundUrl;
        obj["fetchedAt"] = d.fetchedAt.toString(Qt::ISODate);

        QJsonArray devArr, pubArr, genreArr, ssArr;
        for (const auto& v : d.developers) devArr.append(v);
        for (const auto& v : d.publishers) pubArr.append(v);
        for (const auto& v : d.genres) genreArr.append(v);
        for (const auto& v : d.screenshots) ssArr.append(v);

        obj["developers"] = devArr;
        obj["publishers"] = pubArr;
        obj["genres"] = genreArr;
        obj["screenshots"] = ssArr;

        root[it.key()] = obj;
    }

    QFile file(dirPath + "/steam_details_cache.json");
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

// =============================================================================
// Drop Handling (#60)
// =============================================================================

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

// =============================================================================
// Translation Package Installation (#primary-flow)
// =============================================================================

void GameService::installTranslation(const QString& gameId)
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

    // Backup before install
    BackupManager* bm = BackupManager::instance();
    if (bm) {
        emit translationInstallProgress(gameId, 0.0, tr("Yedek oluşturuluyor..."));
        bool backupOk = bm->createBackup(gameId, game.name, game.installPath);
        if (!backupOk) {
            emit translationInstallCompleted(gameId, false,
                tr("Yedek oluşturulamadı, kurulum iptal edildi"));
            return;
        }
    }

    m_installingGameId = gameId;
    emit translationInstallStarted(gameId);

    qDebug() << "Installing translation for" << game.name
             << "package:" << pkg->packageId
             << "path:" << game.installPath;

    // Delegate to CoreBridge (handles download + verify + install)
    m_coreBridge->installPackage(pkg->packageId, game.installPath);
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

    // Restore from backup before uninstall
    BackupManager* bm = BackupManager::instance();
    if (bm && bm->hasBackup(gameId)) {
        auto latest = bm->getLatestBackup(gameId);
        if (!latest.isEmpty()) {
            bm->restoreBackup(latest["id"].toString(), game.installPath);
        }
    }

    bool success = m_coreBridge->uninstallPackage(gameId, game.installPath);

    if (success) {
        // Update game's hasTranslation flag
        m_games[*it].hasTranslation = false;
        invalidateCache();
        emit gamesChanged();
    }

    emit translationUninstalled(gameId, success,
        success ? tr("Translation removed successfully")
                : tr("Failed to remove translation"));
}

bool GameService::isTranslationInstalled(const QString& gameId)
{
    if (!m_coreBridge) return false;
    return m_coreBridge->isPackageInstalled(gameId);
}

QVariantMap GameService::checkCompatibility(const QString& gameId)
{
    Q_UNUSED(gameId)
    return {{"level", "unknown"}, {"integrityPercent", 100},
            {"modifiedCount", 0}, {"addedCount", 0}, {"removedCount", 0},
            {"summary", tr("No translation snapshot available yet")}};
}

QVariantMap GameService::analyzeFonts(const QString& gameId)
{
    Q_UNUSED(gameId)
    return {{"hasFontAnalysis", false}, {"totalFonts", 0},
            {"turkishSupportCount", 0}, {"missingChars", QStringList()},
            {"summary", tr("Font analysis not available")}, {"fonts", QVariantList()}};
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
