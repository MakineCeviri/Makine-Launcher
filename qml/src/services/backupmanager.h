/**
 * @file backupmanager.h
 * @brief Game backup management
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantList>
#include <QQmlEngine>

namespace makineai {

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
    bool isValid{true};
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
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList backups READ backups NOTIFY backupsChanged)
    Q_PROPERTY(bool isRestoring READ isRestoring NOTIFY isRestoringChanged)
    Q_PROPERTY(QString restoreStatus READ restoreStatus NOTIFY restoreStatusChanged)

public:
    explicit BackupManager(QObject *parent = nullptr);
    ~BackupManager() override;

    static BackupManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Properties
    QVariantList backups() const;
    bool isRestoring() const { return m_isRestoring; }
    QString restoreStatus() const { return m_restoreStatus; }

    // Q_INVOKABLE methods
    Q_INVOKABLE QVariantList getBackupsForGame(const QString& gameId);
    Q_INVOKABLE bool createBackup(const QString& gameId, const QString& gameName, const QString& sourcePath);
    Q_INVOKABLE bool restoreBackup(const QString& backupId, const QString& targetPath = QString());
    Q_INVOKABLE bool deleteBackup(const QString& backupId);
    Q_INVOKABLE bool hasBackup(const QString& gameId);
    Q_INVOKABLE QString getBackupPath(const QString& gameId);

signals:
    void backupsChanged();
    void isRestoringChanged();
    void restoreStatusChanged();
    void backupCreated(const QString& gameId);
    void backupRestored(const QString& gameId);
    void backupDeleted(const QString& backupId);
    void backupError(const QString& error);

private:
    void loadBackups();
    void saveBackups();
    QString generateBackupId();
    QString getBackupsDirectory();

    QList<BackupInfo> m_backups;
    bool m_isRestoring{false};
    QString m_restoreStatus;
};

} // namespace makineai
