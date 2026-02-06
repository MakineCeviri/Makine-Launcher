/**
 * @file gameservice.cpp
 * @brief Game Service Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "gameservice.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

namespace makineai {

GameService::GameService(QObject *parent)
    : QObject(parent)
{
    setupCoreBridge();
    loadCachedGames();

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
        QVariantMap map;
        map["id"] = game.id;
        map["name"] = game.name;
        map["headerImageUrl"] = game.headerImageUrl;
        map["logoImageUrl"] = game.logoImageUrl;
        map["installPath"] = game.installPath;
        map["steamAppId"] = game.steamAppId;
        map["source"] = game.source;
        map["engine"] = game.engine;
        map["isVerified"] = game.isVerified;
        map["isInstalled"] = game.isInstalled;
        map["hasTranslation"] = game.hasTranslation;
        m_gamesCache.append(map);
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

    // O(n) with O(1) contains check
    for (const auto& game : m_games) {
        if (m_featuredIds.contains(game.id)) {
            QVariantMap map;
            map["id"] = game.id;
            map["name"] = game.name;
            map["headerImageUrl"] = game.headerImageUrl;
            map["isVerified"] = game.isVerified;
            m_featuredGamesCache.append(map);
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

    // O(n) with O(1) contains check
    for (const auto& game : m_games) {
        if (m_recentIds.contains(game.id)) {
            QVariantMap map;
            map["id"] = game.id;
            map["name"] = game.name;
            map["headerImageUrl"] = game.headerImageUrl;
            map["isVerified"] = game.isVerified;
            m_recentGamesCache.append(map);
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
    m_scanStatus = "Oyun kütüphaneleri taranıyor...";
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
    m_scanStatus = "Steam kütüphanesi taranıyor...";
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
    m_scanStatus = "Epic Games taranıyor...";
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
    m_scanStatus = "GOG Galaxy taranıyor...";
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
        emit scanError("Geçersiz oyun klasörü: " + path);
        return;
    }

    QDir dir(path);
    if (!dir.exists()) {
        emit scanError("Belirtilen klasör bulunamadı: " + path);
        return;
    }

    // Check for duplicate
    for (const auto& game : m_games) {
        if (game.installPath == path) {
            emit scanError("Bu oyun zaten eklenmiş: " + path);
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
    // O(1) lookup using hash
    if (m_gameIdToIndex.contains(id)) {
        int index = m_gameIdToIndex[id];
        if (index >= 0 && index < m_games.count()) {
            const auto& game = m_games[index];
            QVariantMap map;
            map["id"] = game.id;
            map["name"] = game.name;
            map["headerImageUrl"] = game.headerImageUrl;
            map["logoImageUrl"] = game.logoImageUrl;
            map["installPath"] = game.installPath;
            map["steamAppId"] = game.steamAppId;
            map["source"] = game.source;
            map["engine"] = game.engine;
            map["isVerified"] = game.isVerified;
            map["isInstalled"] = game.isInstalled;
            map["hasTranslation"] = game.hasTranslation;
            return map;
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
    // TODO: Fetch metadata from Steam API
    Q_UNUSED(gameId)
}

QVariantList GameService::searchGames(const QString& query)
{
    QVariantList result;
    const QString lowerQuery = query.toLower();

    for (const auto& game : m_games) {
        if (game.name.toLower().contains(lowerQuery) ||
            game.id.contains(query)) {
            QVariantMap map;
            map["id"] = game.id;
            map["name"] = game.name;
            map["headerImageUrl"] = game.headerImageUrl;
            map["isVerified"] = game.isVerified;
            result.append(map);
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

} // namespace makineai
