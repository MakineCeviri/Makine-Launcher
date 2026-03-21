/**
 * @file corebridge.h
 * @brief Bridge between QML services and C++ Core library
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtConcurrent>

#include <optional>
#include <memory>

#include "localpackagemanager.h"

namespace makine {

class OperationJournal;

/**
 * @brief Detected game info from core scanners
 */
struct DetectedGame {
    QString id;
    QString name;
    QString installPath;
    QString source;        // steam, epic, gog, manual
    QString engine;        // Unity, Unreal, RenPy, RPGMaker, GameMaker
    QString steamAppId;
    bool isVerified{false};
    bool hasTranslation{false};
    QString translationStatus;
};

/**
 * @brief Translation package info from core
 */
struct TranslationPackageQt {
    QString packageId;
    QString gameId;
    QString gameName;
    QString version;
    QString downloadUrl;
    qint64 sizeBytes;
    bool requiresRuntime{false};
    QVariantList contributors;  // [{name, role}] from manifest
};

/**
 * @brief Core Bridge - Interface to C++ Core Library
 *
 * Provides async operations for:
 * - Game library scanning (Steam, Epic, GOG)
 * - Engine detection
 * - Translation package management
 * - Backup/restore operations
 */
class CoreBridge : public QObject
{
    Q_OBJECT

public:
    explicit CoreBridge(QObject *parent = nullptr);
    ~CoreBridge() override;

    // Singleton access
    static CoreBridge* instance();

    // Set crash recovery journal (forwarded to LocalPackageManager)
    void setJournal(OperationJournal* journal);

    // Access to package manager (for ProcessScanner dynamic map)
    LocalPackageManager* packageManager() const { return m_localPkgManager; }

    // ========== Game Detection ==========

    /**
     * @brief Scan all game libraries asynchronously
     */
    void scanAllLibraries();

    /**
     * @brief Detect engine for a specific game directory
     */
    QString detectEngine(const QString& gamePath);

    /**
     * @brief Get detected games list
     */
    QList<DetectedGame> detectedGames() const { return m_detectedGames; }

    void setCustomGamePaths(const QStringList& paths);
    QStringList customGamePaths() const { return m_customGamePaths; }

    /**
     * @brief Get all supported games from package catalog, enriched with install status
     */
    QVariantList allSupportedGames() const;

    // ========== Package Manager ==========

    /**
     * @brief Check if translation package exists for game
     */
    bool hasTranslationPackage(const QString& gameId);

    /**
     * @brief Get translation package info for game
     */
    std::optional<TranslationPackageQt> getPackageForGame(const QString& gameId);

    /**
     * @brief Get filesystem directory name for a package (from local manifest)
     * @return dirName string, or empty if not found
     */
    Q_INVOKABLE QString getPackageDirName(const QString& steamAppId) const;

    /**
     * @brief Download and install translation package
     */
    void installPackage(const QString& packageId, const QString& gamePath,
                        const QString& variant = {},
                        const QStringList& selectedOptions = {});

    /**
     * @brief Update an already-installed translation package
     * Skips backup — overwrites translation files and updates version.
     */
    void updatePackage(const QString& packageId, const QString& gamePath,
                       const QString& variant = {},
                       const QStringList& selectedOptions = {});

    /**
     * @brief Cancel an in-progress installation
     */
    void cancelInstall();

    /**
     * @brief Get available variants for a game
     */
    QVariantList getVariantsForGame(const QString& gameId);

    /**
     * @brief Get variant type label for a game ("version" or "platform")
     */
    QString getVariantTypeForGame(const QString& gameId);

    /**
     * @brief Get pre-install notes for a game package
     */
    QString getInstallNotesForGame(const QString& gameId);

    /**
     * @brief Get install options (checkbox-style) for a game
     */
    QVariantList getInstallOptionsForGame(const QString& gameId);

    /**
     * @brief Get special dialog mode for a game
     */
    QString getSpecialDialogForGame(const QString& gameId);

    /**
     * @brief Get variant-specific install options
     */
    QVariantList getVariantInstallOptionsForGame(const QString& gameId, const QString& variant);

    /**
     * @brief Get variant-specific special dialog mode
     */
    QString getVariantSpecialDialogForGame(const QString& gameId, const QString& variant);

    /**
     * @brief Get list of files in the translation package (relative paths)
     */
    QStringList getPackageFileList(const QString& gameId, const QString& variant = {});

    /**
     * @brief Match a folder name against the package catalog
     * @return steamAppId if matched, empty string otherwise
     */
    QString findMatchingAppId(const QString& folderName);

    /**
     * @brief Match a game folder against catalog fingerprints using file-based signals
     * @param gamePath Full path to the game folder
     * @return List of {steamAppId, confidence, matchedBy} sorted by confidence
     */
    Q_INVOKABLE QVariantList findMatchingGamesFromFiles(const QString& gamePath);

    /**
     * @brief Check if a translation update is available
     * Compares installed version vs catalog version
     */
    bool hasTranslationUpdate(const QString& gameId);

    /**
     * @brief Check if package is installed for game
     */
    bool isPackageInstalled(const QString& gameId);

    /**
     * @brief Get installed package info (file classification, store version)
     */
    std::optional<InstalledPackageInfo> getInstalledInfo(const QString& gameId);

    /**
     * @brief Update stored game version for installed package
     */
    void updateInstalledStoreVersion(const QString& gameId, const QString& storeVersion,
                                      const QString& source);

    /**
     * @brief Uninstall translation package
     */
    bool uninstallPackage(const QString& gameId, const QString& gamePath);

    /**
     * @brief Refresh package catalog from cached index.json
     */
    Q_INVOKABLE void refreshPackageManifest();

    /**
     * @brief Ensure per-game detail is loaded (from disk cache or needs fetch)
     * @return true if detail was loaded from cache, false if fetch is needed
     */
    Q_INVOKABLE bool ensurePackageDetail(const QString& steamAppId);

    /**
     * @brief Check if per-game detail has been loaded into catalog
     */
    Q_INVOKABLE bool isPackageDetailLoaded(const QString& steamAppId);

    /**
     * @brief Enrich catalog entry with per-game detail JSON data
     */
    void enrichPackageFromJson(const QString& steamAppId, const QByteArray& jsonData);

signals:
    // Scanning signals
    void scanStarted();
    void scanProgress(qreal progress, const QString& status);
    void scanCompleted(int gameCount);
    void scanError(const QString& error);
    void gameDetected(const QString& gameId, const QString& gameName);

    // Package signals
    void packageManifestRefreshed(int packageCount);
    void packageDetailEnriched(const QString& steamAppId);
    void packageInstallError(const QString& error);
    void packageInstallProgress(double progress, const QString& status);
    void packageInstallCompleted(bool success, const QString& message);

private:
    static CoreBridge* s_instance;
    QList<DetectedGame> m_detectedGames;
    // Scanning helpers (pure Qt, no vcpkg deps)
    void doScanSteamReal(QList<DetectedGame>& outGames);
    void doScanEpicReal(QList<DetectedGame>& outGames);
    void doScanGogReal(QList<DetectedGame>& outGames);
    void doScanFilesystemReal(QList<DetectedGame>& outGames,
                              const QSet<QString>& knownPaths);
    void doScanRegistryReal(QList<DetectedGame>& outGames,
                            const QSet<QString>& knownPaths);
    QStringList knownGameDirectories() const;
    QString detectEngineReal(const QString& gamePath);
    QString resolveToSteamAppId(const QString& gameId);
    void buildDetectedGameIndex();
    LocalPackageManager* m_localPkgManager{nullptr};  // Owned (lazy-init, Qt parent). Created in scanAllLibraries().
    OperationJournal* m_journal{nullptr};  // Non-owning. Set by setJournal().
    QHash<QString, int> m_steamAppIdToDetectedIndex;
    QStringList m_customGamePaths;
};

} // namespace makine
