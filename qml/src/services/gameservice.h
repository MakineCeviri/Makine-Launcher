/**
 * @file gameservice.h
 * @brief Game management backend service
 * @copyright (c) 2026 MakineCeviri Team
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
#include "corebridge.h"
#include "manifestsyncservice.h"
#include "steamdetailsservice.h"
#include "supportedgamesmodel.h"

namespace makine { class ImageCacheManager; }

namespace makine {

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

    /// Connect to ImageCacheManager for image resolution.
    void setImageCache(ImageCacheManager* cache);

    static GameService* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    QVariantList games() const;
    int gameCount() const {
        // Avoid calling games() which may trigger a full QVariantList rebuild.
        // Count directly from the source list — installed + has-translation filter.
        int n = 0;
        for (const auto& g : m_games)
            if (g.isInstalled && g.hasTranslation) ++n;
        return n;
    }
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

    /**
     * @brief Add a custom directory to scan for games (persisted to settings)
     */
    Q_INVOKABLE void addCustomScanPath(const QString& path);

    /**
     * @brief Remove a custom scan directory
     */
    Q_INVOKABLE void removeCustomScanPath(const QString& path);

    /**
     * @brief Get all custom scan directories
     */
    Q_INVOKABLE QStringList customScanPaths() const;

    /**
     * @brief Remove a game from the library (does NOT delete game files)
     * If a translation is installed, it must be uninstalled first.
     * Emits gameListChanged() and gameRemoved(gameId) on success.
     */
    Q_INVOKABLE void forgetGame(const QString& gameId);

    /**
     * @brief Change the install path for an existing game
     * Used when the original path is wrong or the game was moved.
     * Validates the new path before applying.
     */
    Q_INVOKABLE bool changeGamePath(const QString& gameId, const QString& newPath);

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
     * @brief Get runtime status for a game (Unity detection + anti-cheat)
     * @return Map with: isUnity, needsRuntime, installed, upToDate,
     *         backend, unityVersion, hasAntiCheat, antiCheatName
     * Note: installed/upToDate fields are always empty — runtime
     *       installation is not yet implemented.
     */
    Q_INVOKABLE QVariantMap getRuntimeStatus(const QString& gameId);

    /**
     * @brief Install translation runtime for a game (not yet implemented)
     * Emits runtimeInstallFinished with success=false
     */
    Q_INVOKABLE void installRuntime(const QString& gameId);

    /**
     * @brief Uninstall translation runtime from a game (not yet implemented)
     * Emits runtimeInstallFinished with success=false
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
     * @brief Resolve all game metadata needed for the detail screen.
     *
     * Consolidates: manual-game check, steamAppId resolution, catalog lookup,
     * image resolve, translation status, external URL / Apex flags.
     * Replaces the business logic previously in GameDataResolver.qml.
     */
    Q_INVOKABLE QVariantMap resolveGameData(const QString& gameId,
                                             const QString& gameName,
                                             const QString& installPath,
                                             const QString& engine,
                                             bool forceAutoInstall) const;

    // ── URL construction helpers (moved from GameDetailViewModel.qml) ──

    /**
     * @brief Build Steam CDN hero image URL for a given Steam App ID
     * @return Full URL to library_hero.jpg, or empty if steamAppId is empty
     */
    Q_INVOKABLE QString steamHeroUrl(const QString& steamAppId) const;

    /**
     * @brief Build Steam CDN cover image URL for a given Steam App ID
     * @return Full URL to library_600x900_2x.jpg, or empty if steamAppId is empty
     */
    Q_INVOKABLE QString steamCoverUrl(const QString& steamAppId) const;

    /**
     * @brief Build Steam CDN logo URL for a given Steam App ID
     * @return Full URL to logo.png, or empty if steamAppId is empty
     */
    Q_INVOKABLE QString steamLogoUrl(const QString& steamAppId) const;

    /**
     * @brief Format download progress as "X.X MB / Y.Y MB"
     * @return Formatted progress string, or empty if total <= 0
     */
    Q_INVOKABLE QString formatDownloadProgress(qint64 received, qint64 total) const;

    /**
     * @brief Check whether a game should auto-install its translation
     * Currently delegates to the forceAutoInstall flag from resolveGameData.
     */
    Q_INVOKABLE bool shouldAutoInstall(const QString& gameId) const;

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
    void gameRemoved(const QString& gameId);
    void runtimeInstallFinished(const QString& gameId, bool success, const QString& error);
    void translationInstallStarted(const QString& gameId);
    void translationInstallProgress(const QString& gameId, double progress, const QString& status);
    void translationInstallCompleted(const QString& gameId, bool success, const QString& message);
    void translationUninstalled(const QString& gameId, bool success, const QString& message);
    void antiCheatWarningNeeded(const QString& gameId, const QVariantMap& antiCheatData);

private:
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

    enum class InstallMode { Install, Update };

    /**
     * @brief Common implementation for install and update flows.
     *
     * Performs: guard checks → async running-exe detection → backup (Install only)
     * → coreBridge install/update call. The three public entry points are thin
     * wrappers that set the mode and call this method.
     */
    void installPackageCommon(const QString& gameId, const QString& variant,
                              const QStringList& selectedOptions, InstallMode mode);

    CoreBridge* m_coreBridge{nullptr};  // Non-owning. Singleton, set by setupCoreBridge().
    ManifestSyncService* m_manifestSync{nullptr};  // Non-owning. Set by setManifestSync().
    ImageCacheManager* m_imageCache{nullptr};  // Non-owning. Set by setImageCache().
    SteamDetailsService* m_steamDetails{nullptr};
    QList<GameInfo> m_games;
    QHash<QString, int> m_gameIdToIndex;       // O(1) lookup by ID
    QHash<QString, int> m_steamAppIdToIndex;   // O(1) lookup by steamAppId
    mutable QHash<QString, bool> m_packageInstalledCache;  // Cached isPackageInstalled results
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

} // namespace makine

Q_DECLARE_METATYPE(makine::GameInfo)
