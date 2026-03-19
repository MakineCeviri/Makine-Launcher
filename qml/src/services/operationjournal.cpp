/**
 * @file operationjournal.cpp
 * @brief Crash recovery journal — thin Qt wrapper implementation
 * @copyright (c) 2026 MakineCeviri Team
 *
 * When core is available, delegates to makine::recovery::CrashRecoveryJournal.
 * In UI-only mode, falls back to QJsonDocument-based implementation.
 */

#include "operationjournal.h"
#include "profiler.h"
#include "apppaths.h"

#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>
#include <memory>

#ifdef MAKINE_UI_ONLY
#include "pathsecurity.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDirIterator>
#include <QDateTime>
#endif

Q_LOGGING_CATEGORY(lcJournal, "makine.journal")

namespace makine {

// ============================================================================
// Qt ↔ Core conversion helpers
// ============================================================================

#ifndef MAKINE_UI_ONLY

static recovery::OperationType opTypeToCore(OpType t) {
    switch (t) {
    case OpType::Install:       return recovery::OperationType::Install;
    case OpType::Uninstall:     return recovery::OperationType::Uninstall;
    case OpType::BackupCreate:  return recovery::OperationType::BackupCreate;
    case OpType::BackupRestore: return recovery::OperationType::BackupRestore;
    }
    return recovery::OperationType::Install;
}

static OpType coreToOpType(recovery::OperationType t) {
    switch (t) {
    case recovery::OperationType::Install:       return OpType::Install;
    case recovery::OperationType::Uninstall:     return OpType::Uninstall;
    case recovery::OperationType::BackupCreate:  return OpType::BackupCreate;
    case recovery::OperationType::BackupRestore: return OpType::BackupRestore;
    }
    return OpType::Install;
}

static recovery::JournalEntry toCoreEntry(const JournalEntry& qe) {
    recovery::JournalEntry ce;
    ce.type = opTypeToCore(qe.type);
    ce.gameId = qe.gameId.toStdString();
    ce.gamePath = qe.gamePath.toStdString();
    ce.backupId = qe.backupId.toStdString();
    ce.backupPath = qe.backupPath.toStdString();
    ce.variant = qe.variant.toStdString();
    ce.startedAt = qe.startedAt;
    ce.modifiedFiles.reserve(qe.modifiedFiles.size());
    for (const QString& f : qe.modifiedFiles) {
        ce.modifiedFiles.push_back(f.toStdString());
    }
    return ce;
}

static JournalEntry fromCoreEntry(const recovery::JournalEntry& ce) {
    JournalEntry qe;
    qe.type = coreToOpType(ce.type);
    qe.gameId = QString::fromStdString(ce.gameId);
    qe.gamePath = QString::fromStdString(ce.gamePath);
    qe.backupId = QString::fromStdString(ce.backupId);
    qe.backupPath = QString::fromStdString(ce.backupPath);
    qe.variant = QString::fromStdString(ce.variant);
    qe.startedAt = ce.startedAt;
    for (const auto& f : ce.modifiedFiles) {
        qe.modifiedFiles.append(QString::fromStdString(f));
    }
    return qe;
}

#endif // !MAKINE_UI_ONLY

// ============================================================================
// Construction
// ============================================================================

OperationJournal::OperationJournal(QObject *parent)
    : QObject(parent)
{
#ifndef MAKINE_UI_ONLY
    QString dir = AppPaths::dataDir();
    QDir().mkpath(dir);
    m_journal = std::make_unique<recovery::CrashRecoveryJournal>(
        dir.toStdString());
#endif
}

QString OperationJournal::journalPath()
{
    return AppPaths::pendingOperationFile();
}

// ============================================================================
// Core-delegated implementation
// ============================================================================

#ifndef MAKINE_UI_ONLY

bool OperationJournal::beginOperation(const JournalEntry& entry)
{
    MAKINE_ZONE_NAMED("Journal::beginOperation");
    return m_journal->beginOperation(toCoreEntry(entry));
}

void OperationJournal::recordFileModified(const QString& relativePath)
{
    m_journal->recordFileModified(relativePath.toStdString());
}

void OperationJournal::commitOperation()
{
    m_journal->commitOperation();
}

void OperationJournal::abortOperation()
{
    m_journal->abortOperation();
}

bool OperationJournal::hasPendingOperation() const
{
    return m_journal->hasPendingOperation();
}

JournalEntry OperationJournal::readPendingOperation() const
{
    return fromCoreEntry(m_journal->readPendingOperation());
}

bool OperationJournal::recover()
{
    MAKINE_ZONE_NAMED("Journal::recover");
    if (!hasPendingOperation()) return true;

    QString statePath = AppPaths::installedPackagesFile();

    auto result = m_journal->recover(statePath.toStdString());

    QString msg = result.success
        ? tr("Yarım kalan işlem temizlendi")
        : tr("Kurtarma tamamlanamadı");

    if (!result.message.empty()) {
        msg += " (" + QString::fromStdString(result.message) + ")";
    }

    qCDebug(lcJournal) << "OperationJournal: recovery" << (result.success ? "succeeded" : "failed")
             << "-" << result.filesProcessed << "files processed";
    emit recoveryCompleted(result.success, msg);
    return result.success;
}

#else // MAKINE_UI_ONLY — full fallback implementation

// ============================================================================
// UI-only fallback (same as original QJsonDocument-based implementation)
// ============================================================================

static QString opTypeToString(OpType t) {
    switch (t) {
    case OpType::Install:       return "install";
    case OpType::Uninstall:     return "uninstall";
    case OpType::BackupCreate:  return "backup_create";
    case OpType::BackupRestore: return "backup_restore";
    }
    return "unknown";
}

static OpType stringToOpType(const QString& s) {
    if (s == "install")        return OpType::Install;
    if (s == "uninstall")      return OpType::Uninstall;
    if (s == "backup_create")  return OpType::BackupCreate;
    if (s == "backup_restore") return OpType::BackupRestore;
    return OpType::Install;
}

bool OperationJournal::beginOperation(const JournalEntry& entry)
{
    MAKINE_ZONE_NAMED("Journal::beginOperation");
    QMutexLocker lock(&m_mutex);

    if (m_active) {
        qCWarning(lcJournal) << "OperationJournal: operation already in progress";
        return false;
    }

    m_current = entry;
    m_current.startedAt = QDateTime::currentSecsSinceEpoch();
    m_current.modifiedFiles.clear();
    m_pendingCount = 0;
    m_active = true;

    flushJournal();
    return true;
}

void OperationJournal::recordFileModified(const QString& relativePath)
{
    QMutexLocker lock(&m_mutex);
    if (!m_active) return;

    m_current.modifiedFiles.append(relativePath);
    m_pendingCount++;

    if (m_pendingCount >= kFlushInterval) {
        flushJournal();
        m_pendingCount = 0;
    }
}

void OperationJournal::commitOperation()
{
    QMutexLocker lock(&m_mutex);
    m_active = false;
    m_current = {};
    m_pendingCount = 0;
    deleteJournal();
}

void OperationJournal::abortOperation()
{
    QMutexLocker lock(&m_mutex);
    m_active = false;
    m_current = {};
    m_pendingCount = 0;
    deleteJournal();
}

bool OperationJournal::hasPendingOperation() const
{
    return QFile::exists(journalPath());
}

JournalEntry OperationJournal::readPendingOperation() const
{
    JournalEntry entry;
    QFile file(journalPath());
    if (!file.open(QIODevice::ReadOnly)) return entry;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcJournal) << "OperationJournal: corrupted journal:" << err.errorString();
        return entry;
    }

    QJsonObject obj = doc.object();
    entry.type = stringToOpType(obj["type"].toString());
    entry.gameId = obj["gameId"].toString();
    entry.gamePath = obj["gamePath"].toString();
    entry.backupId = obj["backupId"].toString();
    entry.backupPath = obj["backupPath"].toString();
    entry.variant = obj["variant"].toString();
    entry.startedAt = obj["startedAt"].toInteger();

    for (const auto& f : obj["modifiedFiles"].toArray())
        entry.modifiedFiles.append(f.toString());

    return entry;
}

bool OperationJournal::recover()
{
    MAKINE_ZONE_NAMED("Journal::recover");
    if (!hasPendingOperation()) return true;

    JournalEntry entry = readPendingOperation();

    if (entry.gameId.isEmpty() && entry.backupPath.isEmpty()) {
        qCWarning(lcJournal) << "OperationJournal: empty/corrupt journal, deleting";
        deleteJournal();
        emit recoveryCompleted(true, tr("Bozuk işlem günlüğü temizlendi"));
        return true;
    }

    qCDebug(lcJournal) << "OperationJournal: recovering" << opTypeToString(entry.type)
             << "for game" << entry.gameId;

    bool ok = false;
    switch (entry.type) {
    case OpType::Install:       ok = recoverInstall(entry); break;
    case OpType::Uninstall:     ok = recoverUninstall(entry); break;
    case OpType::BackupCreate:  ok = recoverBackupCreate(entry); break;
    case OpType::BackupRestore: ok = recoverBackupRestore(entry); break;
    }

    deleteJournal();

    QString msg = ok
        ? tr("Yarım kalan işlem temizlendi (oyun: %1)").arg(entry.gameId)
        : tr("Kurtarma tamamlanamadı (oyun: %1)").arg(entry.gameId);

    qCDebug(lcJournal) << "OperationJournal: recovery" << (ok ? "succeeded" : "failed");
    emit recoveryCompleted(ok, msg);
    return ok;
}

bool OperationJournal::recoverInstall(const JournalEntry& entry)
{
    MAKINE_ZONE_NAMED("Journal::recoverInstall");
    if (entry.gamePath.isEmpty() || !QDir(entry.gamePath).exists()) {
        qCWarning(lcJournal) << "recoverInstall: game path missing:" << entry.gamePath;
        return false;
    }

    int deleted = 0;
    for (const QString& relPath : entry.modifiedFiles) {
        if (relPath.startsWith("_font:")) {
#ifdef Q_OS_WIN
            QString fontName = relPath.mid(6);
            QString fontPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts/" + fontName;
            if (QFile::exists(fontPath)) {
                QFile::remove(fontPath);
                deleted++;
            }
#endif
            continue;
        }

        QString fullPath = QDir::cleanPath(entry.gamePath + "/" + relPath);
        if (QFile::exists(fullPath)) {
            if (QFile::remove(fullPath))
                deleted++;
        }
    }

    qCDebug(lcJournal) << "recoverInstall:" << deleted << "orphaned files removed";
    return true;
}

bool OperationJournal::recoverUninstall(const JournalEntry& entry)
{
    MAKINE_ZONE_NAMED("Journal::recoverUninstall");
    QString statePath = AppPaths::installedPackagesFile();
    QFile stateFile(statePath);
    if (!stateFile.open(QIODevice::ReadOnly)) return true;

    QJsonDocument doc = QJsonDocument::fromJson(stateFile.readAll());
    stateFile.close();

    QJsonObject root = doc.object();
    if (!root.contains(entry.gameId)) return true;

    QJsonObject pkgObj = root[entry.gameId].toObject();
    QJsonArray filesArr = pkgObj["files"].toArray();
    QString gamePath = entry.gamePath.isEmpty()
        ? pkgObj["gamePath"].toString() : entry.gamePath;

    QSet<QString> alreadyDeleted(entry.modifiedFiles.begin(), entry.modifiedFiles.end());
    int deleted = 0;

    for (const auto& f : filesArr) {
        QString relPath = f.toString();
        if (alreadyDeleted.contains(relPath)) continue;

        if (relPath.startsWith("_font:")) {
#ifdef Q_OS_WIN
            QString fontPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                               + "/AppData/Local/Microsoft/Windows/Fonts/" + relPath.mid(6);
            if (QFile::exists(fontPath)) QFile::remove(fontPath);
            deleted++;
#endif
            continue;
        }

        QString fullPath = QDir::cleanPath(gamePath + "/" + relPath);
        if (QFile::exists(fullPath)) {
            QFile::remove(fullPath);
            deleted++;
        }
    }

    root.remove(entry.gameId);
    QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    fileutils::atomicWriteJson(statePath, data);

    qCDebug(lcJournal) << "recoverUninstall:" << deleted << "remaining files deleted, state updated";
    return true;
}

bool OperationJournal::recoverBackupCreate(const JournalEntry& entry)
{
    if (entry.backupPath.isEmpty()) return true;

    QDir backupDir(entry.backupPath);
    if (backupDir.exists()) {
        backupDir.removeRecursively();
        qCDebug(lcJournal) << "recoverBackupCreate: orphan backup dir removed:" << entry.backupPath;
    }
    return true;
}

bool OperationJournal::recoverBackupRestore(const JournalEntry& entry)
{
    if (entry.backupPath.isEmpty() || entry.gamePath.isEmpty()) return false;

    QDir backupDir(entry.backupPath);
    if (!backupDir.exists()) {
        qCWarning(lcJournal) << "recoverBackupRestore: backup dir missing:" << entry.backupPath;
        return false;
    }

    QSet<QString> alreadyRestored(entry.modifiedFiles.begin(), entry.modifiedFiles.end());
    int restored = 0;

    QDirIterator it(entry.backupPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString relPath = backupDir.relativeFilePath(it.filePath());
        if (alreadyRestored.contains(relPath)) continue;

        QString destPath = QDir::cleanPath(entry.gamePath + "/" + relPath);
        QDir().mkpath(QFileInfo(destPath).absolutePath());
        if (QFile::exists(destPath)) QFile::remove(destPath);
        if (QFile::copy(it.filePath(), destPath))
            restored++;
    }

    qCDebug(lcJournal) << "recoverBackupRestore:" << restored << "remaining files restored";
    return true;
}

void OperationJournal::flushJournal()
{
    QJsonObject obj;
    obj["version"] = 1;
    obj["type"] = opTypeToString(m_current.type);
    obj["gameId"] = m_current.gameId;
    obj["gamePath"] = m_current.gamePath;
    obj["backupId"] = m_current.backupId;
    obj["backupPath"] = m_current.backupPath;
    obj["variant"] = m_current.variant;
    obj["startedAt"] = m_current.startedAt;

    QJsonArray files;
    for (const QString& f : m_current.modifiedFiles)
        files.append(f);
    obj["modifiedFiles"] = files;

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QString path = journalPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    fileutils::atomicWriteJson(path, data);
}

void OperationJournal::deleteJournal()
{
    QFile::remove(journalPath());
    QFile::remove(journalPath() + ".tmp");
}

#endif // MAKINE_UI_ONLY

} // namespace makine
