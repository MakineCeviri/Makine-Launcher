/**
 * @file gameservice.h
 * @brief Game Service - Oyun yönetimi backend servisi
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QList>
#include <QSet>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QQmlEngine>
#include <QFileInfo>

#include "corebridge.h"

namespace makineai {

/**
 * @brief Game data model
 */
struct GameInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString headerImageUrl MEMBER headerImageUrl)
    Q_PROPERTY(QString logoImageUrl MEMBER logoImageUrl)
    Q_PROPERTY(QString installPath MEMBER installPath)
    Q_PROPERTY(QString steamAppId MEMBER steamAppId)
    Q_PROPERTY(QString source MEMBER source)
    Q_PROPERTY(QString engine MEMBER engine)
    Q_PROPERTY(bool isVerified MEMBER isVerified)
    Q_PROPERTY(bool isInstalled MEMBER isInstalled)
    Q_PROPERTY(bool hasTranslation MEMBER hasTranslation)

public:
    QString id;
    QString name;
    QString headerImageUrl;
    QString logoImageUrl;
    QString installPath;
    QString steamAppId;
    QString source{"steam"};
    QString engine;
    bool isVerified{false};
    bool isInstalled{false};
    bool hasTranslation{false};
};

/**
 * @brief Game Service - Manages game data and detection
 *
 * Provides:
 * - Game library scanning (Steam, Epic, GOG)
 * - Game metadata fetching
 * - Recipe availability checking
 * - Process monitoring for running games
 */
class GameService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList featuredGames READ featuredGames NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList recentGames READ recentGames NOTIFY gamesChanged)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY gamesChanged)
    Q_PROPERTY(int patchedGamesCount READ patchedGamesCount NOTIFY gamesChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(qreal scanProgress READ scanProgress NOTIFY scanProgressChanged)

public:
    explicit GameService(QObject *parent = nullptr);
    ~GameService() override;

    static GameService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    QVariantList games() const;
    QVariantList featuredGames() const;
    QVariantList recentGames() const;
    int gameCount() const { return m_games.count(); }
    int patchedGamesCount() const;
    bool isScanning() const { return m_isScanning; }
    QString scanStatus() const { return m_scanStatus; }
    qreal scanProgress() const { return m_scanProgress; }

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void scanAllLibraries();
    Q_INVOKABLE void scanSteamLibrary();
    Q_INVOKABLE void scanEpicLibrary();
    Q_INVOKABLE void scanGogLibrary();
    Q_INVOKABLE void addManualGame(const QString& path);
    Q_INVOKABLE QVariantMap getGameById(const QString& id);
    Q_INVOKABLE bool hasRecipe(const QString& gameId);
    Q_INVOKABLE void refreshGameMetadata(const QString& gameId);
    Q_INVOKABLE QVariantList searchGames(const QString& query);

signals:
    void gamesChanged();
    void isScanningChanged();
    void scanStatusChanged();
    void scanProgressChanged();
    void gameDetected(const QString& gameId);
    void scanCompleted(int count);
    void scanError(const QString& error);

private:
    void loadCachedGames();
    void saveCachedGames();
    void setupCoreBridge();
    void onScanProgress(qreal progress, const QString& status);
    void onScanCompleted(int count);
    void onGameDetected(const QString& gameId, const QString& gameName);

    void invalidateCache();
    void rebuildCache();
    bool isValidGamePath(const QString& path) const;

    CoreBridge* m_coreBridge{nullptr};
    QList<GameInfo> m_games;
    QHash<QString, int> m_gameIdToIndex;  // O(1) lookup by ID
    QSet<QString> m_featuredIds;          // O(1) contains check
    QSet<QString> m_recentIds;            // O(1) contains check
    bool m_isScanning{false};
    QString m_scanStatus;
    qreal m_scanProgress{0};

    // Cache for QVariantList conversions
    mutable QVariantList m_gamesCache;
    mutable QVariantList m_featuredGamesCache;
    mutable QVariantList m_recentGamesCache;
    mutable bool m_cacheValid{false};
};

} // namespace makineai

Q_DECLARE_METATYPE(makineai::GameInfo)
