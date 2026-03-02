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
#include <QNetworkAccessManager>

#include "corebridge.h"
#include "manifestsyncservice.h"
#include "supportedgamesmodel.h"

namespace makineai {

/**
 * @brief Game data model
 */
struct GameInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
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
            {"installPath", installPath}, {"steamAppId", steamAppId},
            {"source", source}, {"engine", engine},
            {"isVerified", isVerified}, {"isInstalled", isInstalled},
            {"hasTranslation", hasTranslation}
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
    static constexpr int kSecondsPerHour = 3600;

    bool isExpired() const {
        return fetchedAt.isNull() || fetchedAt.secsTo(QDateTime::currentDateTime()) > TTL_HOURS * kSecondsPerHour;
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

    Q_PROPERTY(QVariantList games READ games NOTIFY gameListChanged)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY gameListChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(QVariantList installedTranslations READ installedTranslations NOTIFY translationStatusChanged)
    Q_PROPERTY(int installedTranslationCount READ installedTranslationCount NOTIFY translationStatusChanged)
    Q_PROPERTY(int outdatedPatchCount READ outdatedPatchCount NOTIFY translationStatusChanged)
    Q_PROPERTY(SupportedGamesModel* supportedGamesModel READ supportedGamesModel CONSTANT)

public:
    explicit GameService(QObject *parent = nullptr);
    ~GameService() override;

    /// Deferred initialization — call after construction to load caches.
    /// Separated from constructor so splash screen stays responsive.
    void initialize();

    /// Connect to ManifestSyncService for remote catalog data.
    void setManifestSync(ManifestSyncService* sync);

    static GameService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    QVariantList games() const;
    int gameCount() const { return m_games.count(); }
    bool isScanning() const { return m_isScanning; }
    QVariantList supportedGames() const;
    QVariantList installedTranslations() const;
    SupportedGamesModel* supportedGamesModel() const { return m_supportedGamesModel; }
    int installedTranslationCount() const;

    int outdatedPatchCount() const;
    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void scanAllLibraries();
    /**
     * @brief Add a manually selected game folder to the library (async)
     * Engine detection + catalog matching run in background thread.
     * Emits manualGameAdded(gameId) on completion.
     */
    Q_INVOKABLE void addManualGame(const QString& path);
    Q_INVOKABLE QVariantMap getGameById(const QString& id) const;
    Q_INVOKABLE void fetchSteamDetails(const QString& steamAppId);
    Q_INVOKABLE QVariantMap getSteamDetails(const QString& steamAppId);

    /**
     * @brief Get all game details in a single call
     * Combines recipe info, font analysis, compatibility, and runtime status
     */
    Q_INVOKABLE QVariantMap getGameDetails(const QString& gameId);

    /**
     * @brief Get available variants for a game (versions or platforms)
     * @return List of variant strings, empty if no variants
     */
    Q_INVOKABLE QVariantList getVariants(const QString& gameId);

    /**
     * @brief Get variant type for a game ("version" or "platform")
     */
    Q_INVOKABLE QString getVariantType(const QString& gameId);

    /**
     * @brief Get pre-install notes for a game package
     * @return Notes string, empty if none
     */
    Q_INVOKABLE QString getInstallNotes(const QString& gameId);

    /**
     * @brief Get install options (checkbox-style, multiple selectable)
     * @return List of {id, label, description, icon, defaultSelected, subDir}
     */
    Q_INVOKABLE QVariantList getInstallOptions(const QString& gameId);

    /**
     * @brief Get special dialog mode for a game (e.g. "eldenRing")
     */
    Q_INVOKABLE QString getSpecialDialog(const QString& gameId);

    /**
     * @brief Get variant-specific install options (e.g. GTA III patch/dubbing within trilogy)
     * @return List of {id, label, description, icon, defaultSelected, subDir}, empty if none
     */
    Q_INVOKABLE QVariantList getVariantInstallOptions(const QString& gameId, const QString& variant);

    /**
     * @brief Get variant-specific special dialog mode
     */
    Q_INVOKABLE QString getVariantSpecialDialog(const QString& gameId, const QString& variant);

    /**
     * @brief Install translation package for a supported game
     * Finds the package, downloads and installs it via CoreBridge
     */
    Q_INVOKABLE void installTranslation(const QString& gameId,
                                         const QString& variant = {},
                                         const QStringList& selectedOptions = {});

    /**
     * @brief Cancel an in-progress translation installation
     */
    Q_INVOKABLE void cancelInstallation();

    /**
     * @brief Update an installed translation package (no backup step)
     * Downloads new files, overwrites old ones, updates version.
     */
    Q_INVOKABLE void updateTranslation(const QString& gameId,
                                        const QString& variant = {},
                                        const QStringList& selectedOptions = {});

    /**
     * @brief Uninstall translation package from a game
     */
    Q_INVOKABLE void uninstallTranslation(const QString& gameId);

    /**
     * @brief Recover a broken translation: uninstall + reinstall
     */
    Q_INVOKABLE void recoverTranslation(const QString& gameId);

    /**
     * @brief Check if a translation package update is available
     * Compares installed version vs catalog version
     */
    Q_INVOKABLE bool hasTranslationUpdate(const QString& gameId) const;

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

    /**
     * @brief Acknowledge anti-cheat warning and proceed with install
     */
    Q_INVOKABLE void acknowledgeAntiCheat(const QString& gameId);

    /**
     * @brief Check if a local translation package exists for a game
     * Used by InstallFlowController to decide whether to download from R2
     */
    Q_INVOKABLE bool hasLocalPackage(const QString& steamAppId) const;

    /**
     * @brief Get catalog entry with dataUrl/downloadSize for a game
     * @return Map from supportedGames() cache, or empty if not found
     */
    Q_INVOKABLE QVariantMap getCatalogEntry(const QString& steamAppId) const;

    /**
     * @brief Check for all updates (re-sync manifest + rescan libraries + check translations)
     */
    Q_INVOKABLE void checkForUpdates();

signals:
    void gameListChanged();
    void translationStatusChanged();
    void supportedGamesChanged();
    void isScanningChanged();
    void gameDetected(const QString& gameId);
    void scanCompleted(int count);
    void steamDetailsFetched(const QString& steamAppId, const QVariantMap& details);
    void steamDetailsFetchError(const QString& steamAppId, const QString& error);
    void manualGameAdded(const QString& gameId);
    void runtimeInstallFinished(const QString& gameId, bool success, const QString& error);
    void translationInstallStarted(const QString& gameId);
    void translationInstallProgress(const QString& gameId, double progress, const QString& status);
    void translationInstallCompleted(const QString& gameId, bool success, const QString& message);
    void translationUninstalled(const QString& gameId, bool success, const QString& message);
    void antiCheatWarningNeeded(const QString& gameId, const QVariantMap& antiCheatData);

private:
    void loadCachedGames();
    void saveCachedGames();
    void setupCoreBridge();
    void onScanProgress(qreal progress, const QString& status);
    void onScanCompleted(int count);
    void onGameDetected(const QString& gameId, const QString& gameName);

    void finalizeManualGame(const QString& path, const QString& folderName,
                            const QString& engine, const QString& matchedAppId);
    void finalizeUninstall(const QString& gameId, const QString& gamePath, int gameIndex);
    void invalidateGameListCache();
    void invalidateTranslationCache();
    void invalidateSupportedCache();
    void invalidateAllCaches();
    void rebuildCache();
    void ensureSupportedGamesCache();
    bool isValidGamePath(const QString& path) const;

    QVariantMap steamDetailsToVariantMap(const SteamDetails& details) const;
    void loadSteamDetailsCache();
    void saveSteamDetailsCache();

    CoreBridge* m_coreBridge{nullptr};  // Non-owning. Singleton, set by setupCoreBridge().
    ManifestSyncService* m_manifestSync{nullptr};  // Non-owning. Set by setManifestSync().
    QNetworkAccessManager m_networkManager;
    QList<GameInfo> m_games;
    QHash<QString, int> m_gameIdToIndex;       // O(1) lookup by ID
    QHash<QString, int> m_steamAppIdToIndex;   // O(1) lookup by steamAppId
    QHash<QString, SteamDetails> m_steamDetailsCache;
    mutable QHash<QString, bool> m_packageInstalledCache;  // Cached isPackageInstalled results
    QSet<QString> m_pendingFetches;
    bool m_isScanning{false};
    QString m_scanStatus;
    qreal m_scanProgress{0};
    QString m_installingGameId;  // Track which game is being installed

    // Cache for QVariantList conversions
    mutable QVariantList m_gamesCache;
    mutable QVariantList m_supportedGamesCache;
    mutable QVariantList m_translationGamesCache;
    mutable QVariantList m_installedTranslationsCache;
    mutable bool m_cacheValid{false};
    mutable bool m_supportedCacheValid{false};
    mutable bool m_translationCacheValid{false};
    mutable bool m_installedCacheValid{false};
    mutable int m_outdatedPatchCount{0};
    QSet<QString> m_antiCheatAcknowledged;
    SupportedGamesModel* m_supportedGamesModel{nullptr};
};

} // namespace makineai

Q_DECLARE_METATYPE(makineai::GameInfo)
