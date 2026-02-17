/**
 * @file updatedetectionservice.h
 * @brief Two-tier game update detection service — thin Qt wrapper
 * @copyright (c) 2026 MakineAI Team
 *
 * Tier 1 (fast): Store metadata checks (ACF buildid, Epic manifest, GOG registry)
 * Tier 2 (detailed): File hash comparison with mtime pre-filtering
 *
 * When core is available, delegates file I/O and hashing to
 * makineai::update (pure C++ module). In UI-only mode, falls back to
 * the original QDir/QCryptographicHash-based implementation.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QList>
#include <QMutex>

#ifndef MAKINEAI_UI_ONLY
#include <makineai/update_detection.hpp>
#endif

// Note: No QML_ELEMENT/QML_SINGLETON -- only used from C++ (GameService)

namespace makineai {

class GameService;

// Store-level version record (Tier 1)
struct StoreVersionRecord {
    QString gameId;
    QString steamBuildId;       // ACF "buildid"
    QString epicVersionString;  // Epic manifest "AppVersionString"
    QString gogBuildId;         // GOG registry "ver"
    qint64 exeLastModified{0}; // Fallback: exe mtime
    qint64 recordedAt{0};
};

// File hash record (Tier 2 snapshot)
struct FileHashRecord {
    QString relativePath;
    QString sha256;
    qint64 fileSize{0};
    qint64 lastModified{0};    // mtime for pre-filtering
};

// Game file snapshot
struct GameSnapshot {
    QString gameId;
    QString patchVersion;
    qint64 takenAt{0};
    QList<FileHashRecord> files;
};

// Engine profile - directory/extension rules for file tracking
struct EngineProfile {
    // Each entry: { directory, extensionFilter }
    // directory="" means root, extensionFilter="*" means all files
    struct Rule {
        QString directory;      // "Content/Paks", "*_Data/Managed", "" for root
        QString nameFilter;     // "*.pak", "*.dll", "globalgamemanagers"
        bool recurse{false};    // Recurse into subdirectories
    };
    QList<Rule> rules;
    QStringList ignoredDirs;    // "Saved", "Logs", "saves"
    int maxFiles{100};
};

/**
 * @brief Two-tier game update detection service
 *
 * Tier 1: Fast store metadata check (~1ms/game)
 * Tier 2: Detailed file hash comparison (only for changed games)
 */
class UpdateDetectionService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isChecking READ isChecking NOTIFY isCheckingChanged)
    Q_PROPERTY(int gamesWithUpdates READ gamesWithUpdates NOTIFY gamesWithUpdatesChanged)
    Q_PROPERTY(bool monitoringActive READ monitoringActive NOTIFY monitoringActiveChanged)

public:
    explicit UpdateDetectionService(QObject *parent = nullptr);
    ~UpdateDetectionService() override;

    // Properties
    bool isChecking() const { return m_isChecking; }
    int gamesWithUpdates() const { return m_gamesWithUpdates; }
    bool monitoringActive() const { return m_monitoringActive; }

    // Tier 1: Quick store metadata check
    Q_INVOKABLE void checkAllGamesQuick();
    Q_INVOKABLE void checkGameQuick(const QString& gameId);

    // Tier 2: Detailed file hash comparison
    Q_INVOKABLE QVariantMap checkCompatibility(const QString& gameId);

    // Snapshot management
    Q_INVOKABLE void takeSnapshot(const QString& gameId, const QString& patchVersion,
                                   const QString& installPath, const QString& engine);
    Q_INVOKABLE bool hasSnapshot(const QString& gameId);

    // Store version recording
    Q_INVOKABLE void recordStoreVersion(const QString& gameId,
                                         const QString& installPath, const QString& source);

    // Update tracking
    Q_INVOKABLE bool hasUpdate(const QString& gameId) const;
    Q_INVOKABLE void clearUpdate(const QString& gameId);

    // Cleanup
    Q_INVOKABLE void removeSnapshot(const QString& gameId);
    Q_INVOKABLE void removeStoreVersion(const QString& gameId);

    // Background monitoring
    Q_INVOKABLE void startMonitoring();
    Q_INVOKABLE void stopMonitoring();

    // GameService access (set by GameService on construction)
    void setGameService(GameService* gs) { m_gameService = gs; }

signals:
    void isCheckingChanged();
    void gamesWithUpdatesChanged();
    void monitoringActiveChanged();
    void gameUpdateDetected(const QString& gameId, const QString& gameName,
                            const QString& summary);
    void compatibilityChecked(const QString& gameId, const QVariantMap& result);
    void snapshotTaken(const QString& gameId, int fileCount);

private:
    // Engine profiles
    static EngineProfile profileForEngine(const QString& engine);

    // Tier 1: Store metadata readers (thread-safe, no Qt parent access)
    static QString readSteamBuildId(const QString& installPath, const QString& steamAppId);
    static QString readEpicVersion(const QString& installPath);
    static QString readGogVersion(const QString& gameId);
    static qint64 readExeMtime(const QString& installPath);

    // Tier 2: File collection & hashing (thread-safe)
    static QStringList collectFiles(const QString& installPath, const EngineProfile& profile);
    static QList<FileHashRecord> hashGameFiles(const QString& installPath, const EngineProfile& profile);
    static QString computeFileHash(const QString& filePath);

    // Persistence
    void loadStoreVersions();
    void saveStoreVersions();
    static void loadSnapshot(const QString& dataDir, const QString& gameId, GameSnapshot& out);
    static void saveSnapshot(const QString& dataDir, const GameSnapshot& snapshot);
    QString dataDir() const;

    // Internal helpers
    static StoreVersionRecord readCurrentStoreVersion(const QString& gameId,
                                                       const QString& installPath,
                                                       const QString& source,
                                                       const QString& steamAppId);

    // State
    GameService* m_gameService{nullptr};
    bool m_isChecking{false};
    int m_gamesWithUpdates{0};
    bool m_monitoringActive{false};
    QTimer* m_monitorTimer{nullptr};
    QHash<QString, StoreVersionRecord> m_storeVersions;
    QSet<QString> m_updatedGameIds;
    mutable QMutex m_storeVersionsMutex;
};

} // namespace makineai
