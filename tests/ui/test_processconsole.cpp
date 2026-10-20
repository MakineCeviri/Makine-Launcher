// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// What a patch's own program prints, as the game page shows it.
//
// The output arrives in pipe reads of whatever size, so a Turkish character can
// be cut in two and a "\r\n" can straddle two reads. Both used to be invisible
// (the output was only logged on failure); now every read is on screen.

#include <gtest/gtest.h>
#include "processconsole.h"

using namespace makine::processconsole;

TEST(ProcessConsole, AUtf8CharacterCutByAReadWaitsForItsRest)
{
    const QByteArray all = QStringLiteral("Çeviri yüklendi: ş").toUtf8();
    QByteArray pending = all.first(all.size() - 1);  // "ş" is C5 9F; 9F not read yet

    EXPECT_EQ(takeText(pending), QStringLiteral("Çeviri yüklendi: "));
    EXPECT_EQ(pending.size(), 1);
    pending += all.back();
    EXPECT_EQ(takeText(pending), QStringLiteral("ş"));
    EXPECT_TRUE(pending.isEmpty());
}

TEST(ProcessConsole, ThreeAndFourByteCharactersAreHeldWhole)
{
    const QByteArray arrow = QStringLiteral("→").toUtf8();  // E2 86 92
    QByteArray pending = "ok " + arrow.first(2);
    EXPECT_EQ(takeText(pending), QStringLiteral("ok "));
    pending += arrow.mid(2);
    EXPECT_EQ(takeText(pending), QStringLiteral("→"));
}

TEST(ProcessConsole, TheLastReadIsTakenWhole)
{
    QByteArray pending = QByteArray("son") + char(0xC5);
    EXPECT_EQ(takeText(pending, true).size(), 4);
    EXPECT_TRUE(pending.isEmpty());
}

TEST(ProcessConsole, NonUtf8OutputIsStillShown)
{
    // cmd writes the OEM code page; whatever that page is, ASCII survives and
    // no byte is held back or lost.
    QByteArray pending = QByteArray("Devam etmek i") + char(0x87) + "in";
    const QString text = takeText(pending);
    EXPECT_TRUE(pending.isEmpty());
    EXPECT_TRUE(text.startsWith(QStringLiteral("Devam etmek i")));
    EXPECT_TRUE(text.endsWith(QStringLiteral("in")));
}

TEST(ProcessConsole, ACarriageReturnRewritesTheLine)
{
    QString log;
    appendTo(log, u"Yükleniyor %10\r", 1024);
    appendTo(log, u"Yükleniyor %50\r", 1024);
    appendTo(log, u"Yükleniyor %100\n", 1024);
    EXPECT_EQ(log, QStringLiteral("Yükleniyor %100\n"));
}

TEST(ProcessConsole, ACrLfSplitAcrossReadsKeepsTheLine)
{
    QString log;
    appendTo(log, u"satır bir\r", 1024);
    appendTo(log, u"\nsatır iki\r\n", 1024);
    EXPECT_EQ(log, QStringLiteral("satır bir\nsatır iki\n"));
}

TEST(ProcessConsole, OnlyTheNewestLinesAreKeptFromALineStart)
{
    QString log;
    for (int i = 0; i < 100; ++i)
        appendTo(log, QStringLiteral("line %1\n").arg(i, 3, 10, QLatin1Char('0')), 64);
    EXPECT_LE(log.size(), 64);
    EXPECT_TRUE(log.startsWith(QStringLiteral("line ")));
    EXPECT_TRUE(log.endsWith(QStringLiteral("line 099\n")));
}
