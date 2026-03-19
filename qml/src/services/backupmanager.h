/**
 * @file backupmanager.h
 * @brief Game backup management
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>
#include <QQmlEngine>

namespace makine {

class OperationJournal;

/**
 * @brief Backup info structure
 */
struct BackupInfo {
    QString id;
    QString gameId;
    QString gameName;
    QString backupPath;
    QString originalPath;  // Original game path for restore
    QDateTime createdAt;
    qint64 sizeBytes{0};
    int fileCount{0};
    bool isValid{true};
    QString gameStoreVersion;   // Game version when backup was created
    QString patchVersion;       // Translation patch version

    static QString formatSize(qint64 bytes) {
        if (bytes < 1024) return QString::number(bytes) + " B";
        if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }

    QVariantMap toVariantMap(const QString& displayName = {},
                             const QString& currentGameVersion = {}) const {
        bool isStale = !gameStoreVersion.isEmpty() && !currentGameVersion.isEmpty()
                       && gameStoreVersion != currentGameVersion;
        return {
            {"id", id}, {"gameId", gameId}, {"gameName", gameName},
            {"backupPath", backupPath}, {"originalPath", originalPath},
            {"createdAt", createdAt.toString("dd MMM yyyy HH:mm")},
            {"date", createdAt.toString("dd MMM yyyy HH:mm")},
            {"sizeBytes", sizeBytes},
            {"sizeFormatted", formatSize(sizeBytes)},
            {"fileCount", fileCount},
            {"name", displayName.isEmpty()
                ? gameName + " - " + createdAt.toString("dd.MM.yyyy")
                : displayName},
            {"isValid", isValid},
            {"gameStoreVersion", gameStoreVersion},
            {"patchVersion", patchVersion},
            {"isStale", isStale}
        };
    }
};

/**
 * @brief Backup Manager - Manages game file backups
 *
 * Provides:
 * - Automatic backup before patching
 * - Manual backup creation
 * - Backup restoration
 * - Backup deletion
 */
class BackupManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList backups READ backups NOTIFY backupsChanged)
    Q_PROPERTY(bool isRestoring READ isRestoring NOTIFY isRestoringChanged)
    Q_PROPERTY(QString restoreStatus READ restoreStatus NOTIFY restoreStatusChanged)
    Q_PROPERTY(int maxBackupsPerGame READ maxBackupsPerGame WRITE setMaxBackupsPerGame NOTIFY maxBackupsPerGameChanged)
    Q_PROPERTY(QString totalSizeFormatted READ totalSizeFormatted NOTIFY backupsChanged)

public:
    explicit BackupManager(QObject *parent = nullptr);
    ~BackupManager() override;

    static BackupManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static BackupManager* instance();

    // Properties
    QVariantList backups() const;
    bool isRestoring() const { return m_isRestoring; }
    QString restoreStatus() const { return m_restoreStatus; }
    int maxBackupsPerGame() const { return m_maxBackupsPerGame; }
    void setMaxBackupsPerGame(int max);
    QString totalSizeFormatted() const;

    // Q_INVOKABLE methods
    Q_INVOKABLE QVariantList getBackupsForGame(const QString& gameId);
    Q_INVOKABLE QVariantMap getLatestBackup(const QString& gameId);
    Q_INVOKABLE bool restoreBackup(const QString& backupId, const QString& targetPath = QString());
    Q_INVOKABLE bool deleteBackup(const QString& backupId);
    Q_INVOKABLE bool hasBackup(const QString& gameId);

    /**
     * @brief Update originalPath for all backups of a game (e.g. after game moved)
     * Prevents backup restore from targeting a stale path.
     */
    void updateOriginalPaths(const QString& gameId, const QString& newPath);

    void setJournal(OperationJournal* journal) { m_journal = journal; }

    /**
     * @brief Selective async backup — only backs up files that will be overwritten
     * @param gameId Steam app ID
     * @param gameName Display name
     * @param gamePath Game install directory
     * @param filesToOverwrite Relative paths of files the translation will overwrite
     */
    void createSelectiveBackupAsync(const QString& gameId, const QString& gameName,
                                     const QString& gamePath, const QStringList& filesToOverwrite,
                                     const QString& gameStoreVersion = {},
                                     const QString& patchVersion = {});

signals:
    void backupProgress(double progress, const QString& status);
    void selectiveBackupCompleted(const QString& gameId, bool success);
    void backupsChanged();
    void isRestoringChanged();
    void restoreStatusChanged();
    void maxBackupsPerGameChanged();
    void backupCreated(const QString& gameId);
    void backupRestored(const QString& gameId);
    void backupDeleted(const QString& backupId);
    void backupError(const QString& error);

private:
    void loadBackups();
    void saveBackups();
    void cleanupOldBackups(const QString& gameId);
    QString generateBackupId();
    QString getBackupsDirectory();

    static BackupManager* s_instance;

    void rebuildBackupIndex();

    QList<BackupInfo> m_backups;
    QHash<QString, int> m_backupIdToIndex;  // O(1) lookup by backup ID
    bool m_isRestoring{false};
    QString m_restoreStatus;
    int m_maxBackupsPerGame{3};
    OperationJournal* m_journal{nullptr};
};

} // namespace makine
