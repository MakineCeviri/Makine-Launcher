/**
 * @file backupmanager.cpp
 * @brief Backup Manager Implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "backupmanager.h"
#include "pathsecurity.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QDirIterator>
#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

namespace makineai {

BackupManager* BackupManager::s_instance = nullptr;

BackupManager::BackupManager(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    loadBackups();
}

BackupManager::~BackupManager()
{
    saveBackups();
}

BackupManager* BackupManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return new BackupManager();
}

BackupManager* BackupManager::instance()
{
    return s_instance;
}

QVariantList BackupManager::backups() const
{
    QVariantList result;
    for (const auto& backup : m_backups) {
        result.append(backup.toVariantMap());
    }
    return result;
}

void BackupManager::setMaxBackupsPerGame(int max)
{
    if (max < 1) max = 1;
    if (m_maxBackupsPerGame == max) return;
    m_maxBackupsPerGame = max;
    emit maxBackupsPerGameChanged();
}

QString BackupManager::totalSizeFormatted() const
{
    qint64 total = 0;
    for (const auto& b : m_backups)
        total += b.sizeBytes;
    return BackupInfo::formatSize(total);
}

QVariantList BackupManager::getBackupsForGame(const QString& gameId)
{
    QVariantList result;
    for (const auto& backup : m_backups) {
        if (backup.gameId == gameId) {
            result.append(backup.toVariantMap(
                "Yedek - " + backup.createdAt.toString("dd.MM.yyyy HH:mm")));
        }
    }
    return result;
}

QVariantMap BackupManager::getLatestBackup(const QString& gameId)
{
    const BackupInfo* latest = nullptr;
    for (const auto& b : m_backups) {
        if (b.gameId == gameId && b.isValid) {
            if (!latest || b.createdAt > latest->createdAt)
                latest = &b;
        }
    }
    if (latest)
        return latest->toVariantMap();
    return {};
}

bool BackupManager::createBackup(const QString& gameId, const QString& gameName, const QString& sourcePath)
{
    QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        emit backupError(tr("Kaynak klasör bulunamadı: %1").arg(sourcePath));
        return false;
    }

    const QString backupId = generateBackupId();
    const QString backupDir = getBackupsDirectory() + "/" + gameId + "/" + backupId;

    QDir().mkpath(backupDir);

    // Copy files recursively
    qint64 totalSize = 0;
    int copiedFiles = 0;
    int failedFiles = 0;

    QDirIterator it(sourcePath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString sourceFile = it.next();
        const QString relativePath = sourceDir.relativeFilePath(sourceFile);
        const QString destFile = backupDir + "/" + relativePath;

        QDir().mkpath(QFileInfo(destFile).absolutePath());

        if (QFile::copy(sourceFile, destFile)) {
            totalSize += QFileInfo(destFile).size();
            copiedFiles++;
        } else {
            failedFiles++;
            qWarning() << "Failed to copy:" << sourceFile << "->" << destFile;
        }
    }

    if (copiedFiles == 0) {
        // Nothing was copied — remove empty backup dir and report error
        QDir(backupDir).removeRecursively();
        emit backupError(tr("Yedekleme başarısız: hiçbir dosya kopyalanamadı (%1)").arg(gameName));
        return false;
    }

    BackupInfo backup;
    backup.id = backupId;
    backup.gameId = gameId;
    backup.gameName = gameName;
    backup.backupPath = backupDir;
    backup.originalPath = sourcePath;
    backup.createdAt = QDateTime::currentDateTime();
    backup.sizeBytes = totalSize;
    backup.fileCount = copiedFiles;
    backup.isValid = true;

    m_backups.append(backup);

    // Auto-cleanup: remove oldest backups if limit exceeded
    cleanupOldBackups(gameId);

    saveBackups();

    emit backupsChanged();
    emit backupCreated(gameId);

    if (failedFiles > 0) {
        qWarning() << "Backup created with" << failedFiles << "failed copies for game:" << gameId;
        emit backupError(tr("Yedek oluşturuldu ancak %1 dosya kopyalanamadı").arg(failedFiles));
    }

    qDebug() << "Backup created:" << backupId << "for game:" << gameId
             << "(" << copiedFiles << "files," << failedFiles << "failed)";
    return true;
}

bool BackupManager::restoreBackup(const QString& backupId, const QString& targetPath)
{
    auto it = std::find_if(m_backups.begin(), m_backups.end(),
        [&backupId](const BackupInfo& b) { return b.id == backupId; });

    if (it == m_backups.end()) {
        emit backupError(tr("Yedek bulunamadı: %1").arg(backupId));
        return false;
    }

    if (!it->isValid) {
        emit backupError(tr("Yedek dosyaları bulunamadı"));
        return false;
    }

    const QString backupDir = it->backupPath;
    const QString restoreDir = targetPath.isEmpty() ? it->originalPath : targetPath;
    const QString gameId = it->gameId;

    QDir sourceDir(backupDir);
    if (!sourceDir.exists()) {
        emit backupError(tr("Yedek klasörü bulunamadı: %1").arg(backupDir));
        return false;
    }

    // Set restoring state
    m_isRestoring = true;
    m_restoreStatus = tr("Yedek geri yükleniyor...");
    emit isRestoringChanged();
    emit restoreStatusChanged();

    // Run restore operation async
    (void)QtConcurrent::run([this, backupDir, restoreDir, backupId, gameId]() {
        QDir sourceDir(backupDir);

        // Count total files first
        int totalFiles = 0;
        QDirIterator countIt(backupDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (countIt.hasNext()) {
            countIt.next();
            totalFiles++;
        }

        // Restore files from backup to target directory
        int restoredCount = 0;
        const QString canonRestoreDir = QDir(restoreDir).canonicalPath();
        QDirIterator it2(backupDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

        while (it2.hasNext()) {
            const QString backupFile = it2.next();
            const QString relativePath = sourceDir.relativeFilePath(backupFile);
            const QString destFile = QDir::cleanPath(restoreDir + "/" + relativePath);

            // Prevent path traversal: ensure destination stays within restore directory
            if (!security::isPathContained(canonRestoreDir, destFile)) {
                qWarning() << "Path traversal blocked during restore:" << relativePath;
                continue;
            }

            // Ensure target directory exists
            QDir().mkpath(QFileInfo(destFile).absolutePath());

            // Remove existing file if present
            if (QFile::exists(destFile)) {
                QFile::remove(destFile);
            }

            // Copy backup file to target
            if (QFile::copy(backupFile, destFile)) {
                restoredCount++;

                // Update status periodically
                if (restoredCount % 10 == 0 || restoredCount == totalFiles) {
                    QMetaObject::invokeMethod(this, [this, restoredCount, totalFiles]() {
                        m_restoreStatus = tr("Geri yükleniyor: %1/%2").arg(restoredCount).arg(totalFiles);
                        emit restoreStatusChanged();
                    }, Qt::QueuedConnection);
                }
            } else {
                qWarning() << "Failed to restore file:" << destFile;
            }
        }

        // Finish restore
        QMetaObject::invokeMethod(this, [this, restoredCount, gameId]() {
            m_isRestoring = false;
            m_restoreStatus = tr("%1 dosya geri yüklendi").arg(restoredCount);
            emit isRestoringChanged();
            emit restoreStatusChanged();
            emit backupRestored(gameId);
            qDebug() << "Backup restored:" << gameId << "-" << restoredCount << "files";
        }, Qt::QueuedConnection);
    });

    return true;
}

bool BackupManager::deleteBackup(const QString& backupId)
{
    auto it = std::find_if(m_backups.begin(), m_backups.end(),
        [&backupId](const BackupInfo& b) { return b.id == backupId; });

    if (it == m_backups.end()) {
        return false;
    }

    // Delete backup directory
    QDir backupDir(it->backupPath);
    if (backupDir.exists()) {
        backupDir.removeRecursively();
    }

    m_backups.erase(it);
    saveBackups();

    emit backupsChanged();
    emit backupDeleted(backupId);

    return true;
}

bool BackupManager::hasBackup(const QString& gameId)
{
    return std::any_of(m_backups.begin(), m_backups.end(),
        [&gameId](const BackupInfo& b) { return b.gameId == gameId && b.isValid; });
}

void BackupManager::loadBackups()
{
    const QString metadataPath = getBackupsDirectory() + "/backups.json";
    QFile file(metadataPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isArray()) return;

    for (const auto& value : doc.array()) {
        const QJsonObject obj = value.toObject();
        BackupInfo backup;
        backup.id = obj["id"].toString();
        backup.gameId = obj["gameId"].toString();
        backup.gameName = obj["gameName"].toString();
        backup.backupPath = obj["backupPath"].toString();
        backup.originalPath = obj["originalPath"].toString();
        backup.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        backup.sizeBytes = obj["sizeBytes"].toVariant().toLongLong();
        backup.fileCount = obj["fileCount"].toInt();
        backup.isValid = QDir(backup.backupPath).exists();
        m_backups.append(backup);
    }

    emit backupsChanged();
}

void BackupManager::saveBackups()
{
    const QString backupsDir = getBackupsDirectory();
    QDir().mkpath(backupsDir);

    QJsonArray array;
    for (const auto& backup : m_backups) {
        QJsonObject obj;
        obj["id"] = backup.id;
        obj["gameId"] = backup.gameId;
        obj["gameName"] = backup.gameName;
        obj["backupPath"] = backup.backupPath;
        obj["originalPath"] = backup.originalPath;
        obj["createdAt"] = backup.createdAt.toString(Qt::ISODate);
        obj["sizeBytes"] = backup.sizeBytes;
        obj["fileCount"] = backup.fileCount;
        array.append(obj);
    }

    QFile file(backupsDir + "/backups.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
    }
}

void BackupManager::cleanupOldBackups(const QString& gameId)
{
    // Collect backups for this game, sorted oldest first
    QList<int> indices;
    for (int i = 0; i < m_backups.size(); ++i) {
        if (m_backups[i].gameId == gameId)
            indices.append(i);
    }

    if (indices.size() <= m_maxBackupsPerGame)
        return;

    // Sort by createdAt ascending (oldest first)
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return m_backups[a].createdAt < m_backups[b].createdAt;
    });

    // Remove oldest until we're at the limit
    int toRemove = indices.size() - m_maxBackupsPerGame;
    for (int i = 0; i < toRemove; ++i) {
        const auto& backup = m_backups[indices[i]];
        QDir dir(backup.backupPath);
        if (dir.exists())
            dir.removeRecursively();

        qDebug() << "Auto-cleanup: removed old backup" << backup.id << "for" << gameId;
    }

    // Remove from list (reverse order to keep indices valid)
    for (int i = toRemove - 1; i >= 0; --i) {
        m_backups.removeAt(indices[i]);
    }
}

QString BackupManager::generateBackupId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString BackupManager::getBackupsDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
}

} // namespace makineai
