/**
 * @file operationjournal.h
 * @brief Crash recovery journal — thin Qt wrapper
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Thin Qt wrapper around makine::recovery::CrashRecoveryJournal.
 * Provides QObject signals for UI integration, delegates all business
 * logic to the pure C++ core module.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#ifndef MAKINE_UI_ONLY
#include <makine/crash_recovery.hpp>
#include <memory>
#endif

#ifdef MAKINE_UI_ONLY
#include <QMutex>
#endif

namespace makine {

enum class OpType { Install, Uninstall, BackupCreate, BackupRestore };

struct JournalEntry {
    OpType type{OpType::Install};
    QString gameId;
    QString gamePath;       // Target game directory
    QString backupId;       // For backup operations
    QString backupPath;     // For backup create: the dir being created
    QString variant;        // For install: selected variant
    QStringList modifiedFiles; // Files already copied/deleted (relative paths)
    qint64 startedAt{0};
};

class OperationJournal : public QObject
{
    Q_OBJECT

public:
    explicit OperationJournal(QObject *parent = nullptr);

    // Journal lifecycle (thread-safe)
    bool beginOperation(const JournalEntry& entry);
    void recordFileModified(const QString& relativePath);
    void commitOperation();
    void abortOperation();

    // Startup recovery
    bool hasPendingOperation() const;
    JournalEntry readPendingOperation() const;
    bool recover();

    static QString journalPath();

signals:
    void recoveryCompleted(bool success, const QString& message);

private:
#ifndef MAKINE_UI_ONLY
    std::unique_ptr<recovery::CrashRecoveryJournal> m_journal;
#else
    void flushJournal();
    void deleteJournal();

    bool recoverInstall(const JournalEntry& entry);
    bool recoverUninstall(const JournalEntry& entry);
    bool recoverBackupCreate(const JournalEntry& entry);
    bool recoverBackupRestore(const JournalEntry& entry);

    JournalEntry m_current;
    bool m_active{false};
    int m_pendingCount{0};
    static constexpr int kFlushInterval = 20;
    mutable QMutex m_mutex;
#endif
};

} // namespace makine
