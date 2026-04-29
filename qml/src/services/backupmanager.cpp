/**
 * @file backupmanager.cpp
 * @brief Backup Manager Implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "backupmanager.h"
#include "profiler.h"
#include "operationjournal.h"
#include "pathsecurity.h"
#include "appprotection.h"
#include "apppaths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QDirIterator>
#include <QSet>
#include <QLoggingCategory>
#include <QtConcurrent>
#include <algorithm>

Q_LOGGING_CATEGORY(lcBackup, "makine.backup")

namespace makine {

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
    MAKINE_ZONE_NAMED("BackupManager::backups");
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
    MAKINE_ZONE_NAMED("BackupManager::getBackupsForGame");
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
    MAKINE_ZONE_NAMED("BackupManager::getLatestBackup");
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

void BackupManager::createSelectiveBackupAsync(const QString& gameId, const QString& gameName,
                                                const QString& gamePath, const QStringList& filesToOverwrite,
                                                const QString& gameStoreVersion,
                                                const QString& patchVersion)
{
    MAKINE_ZONE_NAMED("BackupManager::createSelectiveBackupAsync");
    INTEGRITY_GATE();
    if (filesToOverwrite.isEmpty()) {
        emit selectiveBackupCompleted(gameId, true); // Nothing to backup
        return;
    }

    const QString backupId = generateBackupId();
    const QString backupDir = getBackupsDirectory() + "/" + gameId + "/" + backupId;

    (void)QtConcurrent::run([this, gameId, gameName, gamePath, filesToOverwrite, backupId, backupDir,
                              gameStoreVersion, patchVersion]() {
        QDir().mkpath(backupDir);

        // Begin crash recovery journal
        if (m_journal) {
            JournalEntry je;
            je.type = OpType::BackupCreate;
            je.gameId = gameId;
            je.gamePath = gamePath;
            je.backupId = backupId;
            je.backupPath = backupDir;
            m_journal->beginOperation(je);
        }

        const QString canonGamePath = QDir(gamePath).canonicalPath();
        qint64 totalSize = 0;
        int copiedFiles = 0;
        int failedFiles = 0;
        int total = filesToOverwrite.size();
        QSet<QString> createdDirs;

        for (int i = 0; i < total; ++i) {
            const QString& relPath = filesToOverwrite[i];
            const QString sourceFile = QDir::cleanPath(gamePath + "/" + relPath);

            // Path traversal check
            if (!sourceFile.startsWith(canonGamePath)) continue;

            // Only backup if file actually exists in game dir
            if (!QFile::exists(sourceFile)) continue;

            const QString destFile = backupDir + "/" + relPath;
            const QString destDir = QFileInfo(destFile).absolutePath();
            if (!createdDirs.contains(destDir)) {
                QDir().mkpath(destDir);
                createdDirs.insert(destDir);
            }

            if (QFile::copy(sourceFile, destFile)) {
                totalSize += QFileInfo(destFile).size();
                copiedFiles++;
            } else {
                failedFiles++;
                qCWarning(lcBackup) << "Backup copy failed:" << sourceFile
                                    << "->" << destFile;
            }

            // Throttle progress
            if ((i + 1) % 20 == 0 || i + 1 == total) {
                QMetaObject::invokeMethod(this, [this, i, total]() {
                    double progress = static_cast<double>(i + 1) / total;
                    emit backupProgress(progress, tr("Yedekleniyor: %1/%2").arg(i + 1).arg(total));
                }, Qt::QueuedConnection);
            }
        }

        if (failedFiles > 0) {
            qCWarning(lcBackup) << "Selective backup partial:" << gameId
                                << copiedFiles << "/" << total << "copied,"
                                << failedFiles << "failed";
        }

        // Save backup info on main thread
        QMetaObject::invokeMethod(this, [this, backupId, gameId, gameName, backupDir,
                                          gamePath, totalSize, copiedFiles,
                                          gameStoreVersion, patchVersion]() {
            if (copiedFiles > 0) {
                BackupInfo backup;
                backup.id = backupId;
                backup.gameId = gameId;
                backup.gameName = gameName;
                backup.backupPath = backupDir;
                backup.originalPath = gamePath;
                backup.createdAt = QDateTime::currentDateTime();
                backup.sizeBytes = totalSize;
                backup.fileCount = copiedFiles;
                backup.isValid = true;
                backup.gameStoreVersion = gameStoreVersion;
                backup.patchVersion = patchVersion;

                m_backups.append(backup);
                m_backupIdToIndex[backup.id] = m_backups.size() - 1;
                cleanupOldBackups(gameId);
                saveBackups();
                emit backupsChanged();
                emit backupCreated(gameId);
            } else {
                // No files needed backup — clean up empty dir
                QDir(backupDir).removeRecursively();
            }

            if (m_journal) m_journal->commitOperation();
            qCDebug(lcBackup) << "Selective backup done:" << gameId << copiedFiles << "files";
            emit selectiveBackupCompleted(gameId, true);
        }, Qt::QueuedConnection);
    });
}

bool BackupManager::restoreBackup(const QString& backupId, const QString& targetPath)
{
    MAKINE_ZONE_NAMED("BackupManager::restoreBackup");
    INTEGRITY_GATE();
    auto idxIt = m_backupIdToIndex.constFind(backupId);
    if (idxIt == m_backupIdToIndex.constEnd()) {
        emit backupError(tr("Yedek bulunamadı: %1").arg(backupId));
        return false;
    }

    const BackupInfo& info = m_backups[*idxIt];
    if (!info.isValid) {
        emit backupError(tr("Yedek dosyaları bulunamadı"));
        return false;
    }

    const QString backupDir = info.backupPath;
    const QString restoreDir = targetPath.isEmpty() ? info.originalPath : targetPath;
    const QString gameId = info.gameId;

    QDir sourceDir(backupDir);
    if (!sourceDir.exists()) {
        emit backupError(tr("Yedek klasörü bulunamadı: %1").arg(backupDir));
        return false;
    }

    // Validate restore target exists (game may have been uninstalled)
    if (!QDir(restoreDir).exists()) {
        emit backupError(tr("Hedef klasör bulunamadı: %1\n"
                            "Oyun kaldırılmış veya taşınmış olabilir.")
                         .arg(restoreDir));
        return false;
    }

    // Set restoring state
    m_isRestoring = true;
    m_restoreStatus = tr("Yedek geri yükleniyor...");
    emit isRestoringChanged();
    emit restoreStatusChanged();

    // Begin crash recovery journal
    if (m_journal) {
        JournalEntry je;
        je.type = OpType::BackupRestore;
        je.gameId = gameId;
        je.gamePath = restoreDir;
        je.backupPath = backupDir;
        m_journal->beginOperation(je);
    }

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
        QSet<QString> createdDirs;

        while (it2.hasNext()) {
            const QString backupFile = it2.next();
            const QString relativePath = sourceDir.relativeFilePath(backupFile);
            const QString destFile = QDir::cleanPath(restoreDir + "/" + relativePath);

            // Prevent path traversal: ensure destination stays within restore directory
            if (!security::isPathContained(canonRestoreDir, destFile)) {
                qCWarning(lcBackup) << "Path traversal blocked during restore:" << relativePath;
                continue;
            }

            // Ensure target directory exists (skip if already created)
            const QString destDir = QFileInfo(destFile).absolutePath();
            if (!createdDirs.contains(destDir)) {
                QDir().mkpath(destDir);
                createdDirs.insert(destDir);
            }

            // Remove existing file if present
            if (QFile::exists(destFile)) {
                QFile::remove(destFile);
            }

            // Copy backup file to target
            if (QFile::copy(backupFile, destFile)) {
                restoredCount++;
                if (m_journal) m_journal->recordFileModified(relativePath);

                // Update status periodically
                if (restoredCount % 10 == 0 || restoredCount == totalFiles) {
                    QMetaObject::invokeMethod(this, [this, restoredCount, totalFiles]() {
                        m_restoreStatus = tr("Geri yükleniyor: %1/%2").arg(restoredCount).arg(totalFiles);
                        emit restoreStatusChanged();
                    }, Qt::QueuedConnection);
                }
            } else {
                qCWarning(lcBackup) << "Failed to restore file:" << destFile;
            }
        }

        // Finish restore
        QMetaObject::invokeMethod(this, [this, restoredCount, gameId]() {
            m_isRestoring = false;
            m_restoreStatus = tr("%1 dosya geri yüklendi").arg(restoredCount);
            emit isRestoringChanged();
            emit restoreStatusChanged();
            if (m_journal) m_journal->commitOperation();
            emit backupRestored(gameId);
            qCDebug(lcBackup) << "Backup restored:" << gameId << "-" << restoredCount << "files";
        }, Qt::QueuedConnection);
    });

    return true;
}

bool BackupManager::deleteBackup(const QString& backupId)
{
    MAKINE_ZONE_NAMED("BackupManager::deleteBackup");
    auto idxIt = m_backupIdToIndex.constFind(backupId);
    if (idxIt == m_backupIdToIndex.constEnd()) {
        return false;
    }

    const int idx = *idxIt;

    // Delete backup directory
    QDir backupDir(m_backups[idx].backupPath);
    if (backupDir.exists()) {
        backupDir.removeRecursively();
    }

    m_backups.removeAt(idx);
    rebuildBackupIndex();
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

void BackupManager::updateOriginalPaths(const QString& gameId, const QString& newPath)
{
    if (newPath.isEmpty()) return;

    bool changed = false;
    for (auto& backup : m_backups) {
        if (backup.gameId == gameId && backup.originalPath != newPath) {
            qCDebug(lcBackup) << "Updating backup originalPath for" << gameId
                              << ":" << backup.originalPath << "→" << newPath;
            backup.originalPath = newPath;
            changed = true;
        }
    }

    if (changed)
        saveBackups();
}

void BackupManager::loadBackups()
{
    MAKINE_ZONE_NAMED("BackupManager::loadBackups");
    const QString metadataPath = getBackupsDirectory() + "/backups.json";
    QFile file(metadataPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    try {
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
            backup.gameStoreVersion = obj["gameStoreVersion"].toString();
            backup.patchVersion = obj["patchVersion"].toString();
            backup.isValid = QDir(backup.backupPath).exists();
            m_backups.append(backup);
        }
    } catch (const std::exception& e) {
        qCWarning(lcBackup) << "Failed to load backups metadata:" << e.what();
        return;
    }

    rebuildBackupIndex();
    emit backupsChanged();
}

void BackupManager::saveBackups()
{
    const QString backupsDir = getBackupsDirectory();
    QDir().mkpath(backupsDir);

    try {
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
            if (!backup.gameStoreVersion.isEmpty())
                obj["gameStoreVersion"] = backup.gameStoreVersion;
            if (!backup.patchVersion.isEmpty())
                obj["patchVersion"] = backup.patchVersion;
            array.append(obj);
        }

        QByteArray data = QJsonDocument(array).toJson();
        fileutils::atomicWriteJson(backupsDir + "/backups.json", data);
    } catch (const std::exception& e) {
        qCWarning(lcBackup) << "Failed to save backups metadata:" << e.what();
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

        qCDebug(lcBackup) << "Auto-cleanup: removed old backup" << backup.id << "for" << gameId;
    }

    // Remove from list (reverse order to keep indices valid)
    for (int i = toRemove - 1; i >= 0; --i) {
        m_backups.removeAt(indices[i]);
    }

    rebuildBackupIndex();
}

void BackupManager::rebuildBackupIndex()
{
    m_backupIdToIndex.clear();
    m_backupIdToIndex.reserve(m_backups.size());
    for (int i = 0; i < m_backups.size(); ++i) {
        m_backupIdToIndex[m_backups[i].id] = i;
    }
}

QString BackupManager::generateBackupId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString BackupManager::getBackupsDirectory()
{
    return AppPaths::backupsDir();
}

} // namespace makine
