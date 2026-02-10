/**
 * @file corebridge.h
 * @brief Bridge between QML services and C++ Core library
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <optional>
#include <memory>

namespace makineai {

class LocalPackageManager;

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
    QString headerImageUrl;
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

    // ========== Game Detection ==========

    /**
     * @brief Scan all game libraries asynchronously
     */
    void scanAllLibraries();

    /**
     * @brief Scan Steam library only
     */
    void scanSteamLibrary();

    /**
     * @brief Scan Epic Games library only
     */
    void scanEpicLibrary();

    /**
     * @brief Scan GOG Galaxy library only
     */
    void scanGogLibrary();

    /**
     * @brief Detect engine for a specific game directory
     */
    QString detectEngine(const QString& gamePath);

    /**
     * @brief Get detected games list
     */
    QList<DetectedGame> detectedGames() const { return m_detectedGames; }

    /**
     * @brief Get all supported games from package catalog, enriched with install status
     */
    QVariantList allSupportedGames() const;

    /**
     * @brief Get count of all supported games in catalog
     */
    int supportedGameCount() const;

    // ========== Backup ==========

    /**
     * @brief Create backup of game files
     */
    QString createBackup(const QString& gamePath, const QString& engine);

    /**
     * @brief Restore game files from backup
     */
    bool restoreBackup(const QString& gamePath, const QString& engine,
                       const QString& backupId);

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
     * @brief Download and install translation package
     */
    void installPackage(const QString& packageId, const QString& gamePath);

    /**
     * @brief Check if package is installed for game
     */
    bool isPackageInstalled(const QString& gameId);

    /**
     * @brief Uninstall translation package
     */
    bool uninstallPackage(const QString& gameId, const QString& gamePath);

    /**
     * @brief Refresh package manifest from server
     */
    void refreshPackageManifest();

signals:
    // Scanning signals
    void scanStarted();
    void scanProgress(qreal progress, const QString& status);
    void scanCompleted(int gameCount);
    void scanError(const QString& error);
    void gameDetected(const QString& gameId, const QString& gameName);

    // Backup signals
    void backupCreated(const QString& backupId);
    void backupRestored();
    void backupError(const QString& error);

    // Package signals
    void packageManifestRefreshed(int packageCount);
    void packageDownloadProgress(qreal progress, const QString& status);
    void packageInstalled(const QString& packageId);
    void packageInstallError(const QString& error);
    void packageInstallProgress(double progress, const QString& status);
    void packageInstallCompleted(bool success, const QString& message);

private:
#ifndef MAKINEAI_UI_ONLY
    void doScanSteam();
    void doScanEpic();
    void doScanGog();

    // Package helpers
    TranslationPackageQt convertPackage(const struct TranslationPackage& pkg);
#endif

    static CoreBridge* s_instance;
    QList<DetectedGame> m_detectedGames;
#ifdef MAKINEAI_UI_ONLY
    // Real scanning helpers for UI_ONLY build (pure Qt, no vcpkg deps)
    void doScanSteamReal();
    void doScanEpicReal();
    void doScanGogReal();
    QString detectEngineReal(const QString& gamePath);
    LocalPackageManager* m_localPkgManager{nullptr};
#else
    std::unique_ptr<class PackageManager> m_packageManager;
    bool m_packageManagerInitialized{false};
#endif
};

} // namespace makineai
