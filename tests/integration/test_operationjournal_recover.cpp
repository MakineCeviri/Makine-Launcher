/**
 * @file test_operationjournal_recover.cpp
 * @brief Integration test for OperationJournal — crash + recover cycle
 *
 * Drives a real journal file under a per-test QTemporaryDir, exercises
 * begin / record / abrupt-loss / read-back / recover() the same way the
 * live install flow does (B4-01).
 */

#include <gtest/gtest.h>
#include "operationjournal.h"
#include "apppaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace makine::testing {

class OperationJournalRecoverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Re-route AppDataLocation to a per-test temp dir so writes don't
        // touch the developer's real launcher data folder.
        ASSERT_TRUE(m_tempDir.isValid());
        QStandardPaths::setTestModeEnabled(true);
        // Test mode rewrites GenericDataLocation under HOME/.qttest, so the
        // journal file lives at <test>/.../OperationJournal.json — isolated
        // from a real install.
        QString journalPath = OperationJournal::journalPath();
        QFile::remove(journalPath);  // start from scratch each test
    }

    void TearDown() override {
        QFile::remove(OperationJournal::journalPath());
        QStandardPaths::setTestModeEnabled(false);
    }

    QTemporaryDir m_tempDir;
};

TEST_F(OperationJournalRecoverTest, NoPendingWhenJournalAbsent)
{
    OperationJournal journal;
    EXPECT_FALSE(journal.hasPendingOperation());
}

TEST_F(OperationJournalRecoverTest, BeginAndCommitClearsJournal)
{
    OperationJournal journal;

    JournalEntry entry;
    entry.type = OpType::Install;
    entry.gameId = "424840";
    entry.gamePath = m_tempDir.path() + "/Game";
    entry.variant = "default";
    QDir().mkpath(entry.gamePath);

    ASSERT_TRUE(journal.beginOperation(entry));
    EXPECT_TRUE(journal.hasPendingOperation());

    journal.recordFileModified("data/locale/tr/strings.txt");
    journal.recordFileModified("data/locale/tr/menu.txt");
    journal.commitOperation();

    EXPECT_FALSE(journal.hasPendingOperation());
}

TEST_F(OperationJournalRecoverTest, AbortClearsJournal)
{
    OperationJournal journal;

    JournalEntry entry;
    entry.type = OpType::BackupCreate;
    entry.gameId = "424840";
    entry.gamePath = m_tempDir.path() + "/Game";
    entry.backupId = "20260502_141000";
    entry.backupPath = m_tempDir.path() + "/Backups/424840/20260502_141000";

    ASSERT_TRUE(journal.beginOperation(entry));
    journal.recordFileModified("data/locale/en/strings.txt");
    journal.abortOperation();

    EXPECT_FALSE(journal.hasPendingOperation());
}

TEST_F(OperationJournalRecoverTest, SimulatedCrashLeavesJournalForReplay)
{
    // First lifetime: begin operation, record enough files to cross the
    // periodic flush threshold (kFlushInterval = 20), then "die" without
    // commit/abort by destroying the journal object mid-flight.
    {
        OperationJournal journal;

        JournalEntry entry;
        entry.type = OpType::Install;
        entry.gameId = "1245620";
        entry.gamePath = m_tempDir.path() + "/Elden Ring";
        entry.variant = "default";
        QDir().mkpath(entry.gamePath);

        ASSERT_TRUE(journal.beginOperation(entry));
        for (int i = 0; i < 25; ++i) {
            journal.recordFileModified(QStringLiteral("translation/file_%1.txt").arg(i));
        }
        // Simulate crash: no commitOperation()/abortOperation() before
        // the journal goes out of scope.
    }

    // Second lifetime: a fresh journal sees the pending operation.
    OperationJournal recovered;
    EXPECT_TRUE(recovered.hasPendingOperation());

    JournalEntry pending = recovered.readPendingOperation();
    EXPECT_EQ(pending.gameId, QString("1245620"));
    EXPECT_EQ(pending.type, OpType::Install);
    // After 25 records and a kFlushInterval of 20, at least the first
    // batch of 20 should have made it to disk.
    EXPECT_GE(pending.modifiedFiles.size(), 20);
}

TEST_F(OperationJournalRecoverTest, RecoverHandlesMissingTargetGracefully)
{
    // Begin against a path that we then delete before recover() runs.
    {
        OperationJournal journal;

        JournalEntry entry;
        entry.type = OpType::BackupCreate;
        entry.gameId = "228980";
        entry.gamePath = m_tempDir.path() + "/Stub";
        entry.backupId = "test_id";
        entry.backupPath = m_tempDir.path() + "/Backups/228980/test_id";
        QDir().mkpath(entry.gamePath);
        QDir().mkpath(entry.backupPath);

        ASSERT_TRUE(journal.beginOperation(entry));
    }

    // Pull the rug — both paths gone before recovery sees them.
    QDir(m_tempDir.path() + "/Stub").removeRecursively();
    QDir(m_tempDir.path() + "/Backups").removeRecursively();

    OperationJournal recovered;
    ASSERT_TRUE(recovered.hasPendingOperation());
    // recover() must not crash on missing targets; result may be true
    // (nothing to clean up) or false (logged), but the journal MUST be
    // cleared either way so the next launch is fresh.
    (void)recovered.recover();
    EXPECT_FALSE(recovered.hasPendingOperation());
}

}  // namespace makine::testing

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
