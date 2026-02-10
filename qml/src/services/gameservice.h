/**
 * @file gameservice.h
 * @brief Game management backend service
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
#include <QDateTime>

class QNetworkAccessManager;

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

    QVariantMap toVariantMap() const {
        return {
            {"id", id}, {"name", name},
            {"headerImageUrl", headerImageUrl}, {"logoImageUrl", logoImageUrl},
            {"installPath", installPath}, {"steamAppId", steamAppId},
            {"source", source}, {"engine", engine},
            {"isVerified", isVerified}, {"isInstalled", isInstalled},
            {"hasTranslation", hasTranslation}
        };
    }

    QVariantMap toSummary() const {
        return {
            {"id", id}, {"name", name},
            {"headerImageUrl", headerImageUrl}, {"isVerified", isVerified}
        };
    }
};

/**
 * @brief Steam store details for a game
 */
struct SteamDetails {
    QString description;
    QStringList developers;
    QStringList publishers;
    QString releaseDate;
    QStringList genres;
    int metacriticScore{0};
    bool hasWindows{true};
    bool hasMac{false};
    bool hasLinux{false};
    QString price;
    int discountPercent{0};
    QStringList screenshots;
    QString backgroundUrl;
    QDateTime fetchedAt;

    static constexpr int TTL_HOURS = 24;

    bool isExpired() const {
        return fetchedAt.isNull() || fetchedAt.secsTo(QDateTime::currentDateTime()) > TTL_HOURS * 3600;
    }
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
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool isFetchingSteamDetails READ isFetchingSteamDetails NOTIFY isFetchingSteamDetailsChanged)
    Q_PROPERTY(QVariantList gamesWithTranslation READ gamesWithTranslation NOTIFY gamesChanged)

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
    QString lastError() const { return m_lastError; }
    bool isFetchingSteamDetails() const { return !m_pendingFetches.isEmpty(); }
    QVariantList gamesWithTranslation() const;

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void scanAllLibraries();
    Q_INVOKABLE void scanSteamLibrary();
    Q_INVOKABLE void scanEpicLibrary();
    Q_INVOKABLE void scanGogLibrary();
    Q_INVOKABLE void addManualGame(const QString& path);
    Q_INVOKABLE QVariantMap getGameById(const QString& id);
    Q_INVOKABLE bool hasRecipe(const QString& gameId);
    Q_INVOKABLE void refreshGameMetadata(const QString& gameId);
    Q_INVOKABLE void fetchSteamDetails(const QString& steamAppId);
    Q_INVOKABLE QVariantMap getSteamDetails(const QString& steamAppId);
    Q_INVOKABLE QVariantMap getRecipeInfo(const QString& gameId);
    Q_INVOKABLE QVariantList searchGames(const QString& query);

    /**
     * @brief Handle files dropped onto the application window
     * Dispatches to appropriate handler based on file type
     */
    Q_INVOKABLE void handleDroppedFiles(const QVariantList& urls);

    /**
     * @brief Install a local .mkpkg translation package
     */
    Q_INVOKABLE void installLocalPackage(const QString& filePath);

    /**
     * @brief Install translation package for a supported game
     * Finds the package, downloads and installs it via CoreBridge
     */
    Q_INVOKABLE void installTranslation(const QString& gameId);

    /**
     * @brief Uninstall translation package from a game
     */
    Q_INVOKABLE void uninstallTranslation(const QString& gameId);

    /**
     * @brief Check if a translation is currently installed for a game
     */
    Q_INVOKABLE bool isTranslationInstalled(const QString& gameId);

    /**
     * @brief Check translation compatibility after game update
     * @return Map with: level (compatible/partial/incompatible/unknown),
     *         integrityPercent, modifiedCount, addedCount, removedCount, summary
     */
    Q_INVOKABLE QVariantMap checkCompatibility(const QString& gameId);

    /**
     * @brief Analyze fonts in a game directory for Turkish character support
     * @return Map with: hasFontAnalysis, totalFonts, turkishSupportCount,
     *         missingChars, summary, fonts (list of font details)
     */
    Q_INVOKABLE QVariantMap analyzeFonts(const QString& gameId);

    /**
     * @brief Check if a game has anti-cheat protection
     * @return Map with: hasAntiCheat, systems (list of {name, shortName, severity, warning})
     */
    Q_INVOKABLE QVariantMap checkAntiCheat(const QString& gameId);

    /**
     * @brief Get BepInEx/XUnity runtime status for a Unity game
     * @return Map with: isUnity, needsRuntime, installed, upToDate,
     *         bepinexVersion, xunityVersion, backend (mono/il2cpp),
     *         unityVersion, hasAntiCheat, antiCheatName
     */
    Q_INVOKABLE QVariantMap getRuntimeStatus(const QString& gameId);

    /**
     * @brief Install BepInEx + XUnity.AutoTranslator for a Unity game
     * Emits runtimeInstallFinished when complete
     */
    Q_INVOKABLE void installRuntime(const QString& gameId);

    /**
     * @brief Uninstall BepInEx runtime from a Unity game
     * Emits runtimeInstallFinished when complete
     */
    Q_INVOKABLE void uninstallRuntime(const QString& gameId);

signals:
    void gamesChanged();
    void isScanningChanged();
    void scanStatusChanged();
    void scanProgressChanged();
    void lastErrorChanged();
    void gameDetected(const QString& gameId);
    void scanCompleted(int count);
    void scanError(const QString& error);
    void steamDetailsFetched(const QString& steamAppId, const QVariantMap& details);
    void steamDetailsFetchError(const QString& steamAppId, const QString& error);
    void isFetchingSteamDetailsChanged();
    void localPackageReady(const QString& packageName, const QString& gameName, const QString& filePath);
    void localPackageError(const QString& filePath, const QString& error);
    void folderDropped(const QString& path, bool isGame);
    void runtimeInstallFinished(const QString& gameId, bool success, const QString& error);
    void translationInstallStarted(const QString& gameId);
    void translationInstallProgress(const QString& gameId, double progress, const QString& status);
    void translationInstallCompleted(const QString& gameId, bool success, const QString& message);
    void translationUninstalled(const QString& gameId, bool success, const QString& message);

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

    void parseSteamApiResponse(const QString& steamAppId, const QByteArray& data);
    QVariantMap steamDetailsToVariantMap(const SteamDetails& details) const;
    void loadSteamDetailsCache();
    void saveSteamDetailsCache();

    CoreBridge* m_coreBridge{nullptr};
    QNetworkAccessManager* m_networkManager{nullptr};
    QList<GameInfo> m_games;
    QHash<QString, int> m_gameIdToIndex;  // O(1) lookup by ID
    QSet<QString> m_featuredIds;          // O(1) contains check
    QSet<QString> m_recentIds;            // O(1) contains check
    QHash<QString, SteamDetails> m_steamDetailsCache;
    QSet<QString> m_pendingFetches;
    bool m_isScanning{false};
    QString m_scanStatus;
    qreal m_scanProgress{0};
    QString m_lastError;
    QString m_installingGameId;  // Track which game is being installed

    // Cache for QVariantList conversions
    mutable QVariantList m_gamesCache;
    mutable QVariantList m_featuredGamesCache;
    mutable QVariantList m_recentGamesCache;
    mutable bool m_cacheValid{false};
};

} // namespace makineai

Q_DECLARE_METATYPE(makineai::GameInfo)
