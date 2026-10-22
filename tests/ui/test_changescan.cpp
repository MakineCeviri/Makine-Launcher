// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// What a patch's own program did to the game folder, on a real tree.

#include <gtest/gtest.h>
#include "changescan.h"

#include <QFile>
#include <QTemporaryDir>
#include <QThread>

using namespace makine::changescan;

namespace {

void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(data);
}

} // namespace

TEST(ChangeScan, NothingDoneIsNoDifference)
{
    QTemporaryDir game;
    write(game.filePath("HeavyRain.exe"), "MZ");
    write(game.filePath("Resources/BigFile_WIN.idx"), "idx");
    const Snapshot before = take(game.path());
    EXPECT_EQ(before.files.size(), 2);
    EXPECT_TRUE(compare(before, take(game.path())).isEmpty());
}

TEST(ChangeScan, CreatedModifiedRemoved)
{
    QTemporaryDir game;
    write(game.filePath("DataPC_boot_patch_02.forge"), "forge");
    write(game.filePath("old.txt"), "x");
    const Snapshot before = take(game.path());
    write(game.filePath("DataPC_boot_patch_02.forge"), "forge+translated");
    write(game.filePath("acs_text_yedek/DataPC_boot_patch_02.forge.tablo"), "backup");
    QFile::remove(game.filePath("old.txt"));
    const Diff d = compare(before, take(game.path()));
    EXPECT_EQ(d.modified, QStringList{"DataPC_boot_patch_02.forge"});
    EXPECT_EQ(d.created, QStringList{"acs_text_yedek/DataPC_boot_patch_02.forge.tablo"});
    EXPECT_EQ(d.removed, QStringList{"old.txt"});
    EXPECT_EQ(markers(d), (QStringList{"_program:DataPC_boot_patch_02.forge",
                                       "_program:acs_text_yedek/DataPC_boot_patch_02.forge.tablo"}));
}

TEST(ChangeScan, SameSizeRewriteIsSeenByItsTime)
{
    // Heavy Rain's tool writes its archives in place at the same size.
    QTemporaryDir game;
    write(game.filePath("Resources/BigFile_WIN.d05"), "aaaa");
    const Snapshot before = take(game.path());
    QThread::msleep(20);
    QFile f(game.filePath("Resources/BigFile_WIN.d05"));
    ASSERT_TRUE(f.open(QIODevice::ReadWrite));
    f.write("bbbb");
    f.close();
    f.setFileTime(QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime);
    EXPECT_EQ(compare(before, take(game.path())).modified, QStringList{"Resources/BigFile_WIN.d05"});
}

TEST(ChangeScan, LaunchersOwnFilesAreSkipped)
{
    QTemporaryDir game;
    const Snapshot before = take(game.path(), {".makine"});
    write(game.filePath(".makine"), "set \"OYUN=x\"");
    EXPECT_TRUE(compare(before, take(game.path(), {".makine"})).isEmpty());
}

TEST(ChangeScan, StopsAtTheLimit)
{
    QTemporaryDir game;
    for (int i = 0; i < 5; ++i) write(game.filePath(QStringLiteral("f%1").arg(i)), "x");
    const Snapshot s = take(game.path(), {}, 3);
    EXPECT_FALSE(s.complete);
    EXPECT_EQ(s.files.size(), 3);
}
