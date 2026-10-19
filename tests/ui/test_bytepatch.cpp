// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The "patchBytes" install step: same-length find-and-replace in a game file.
//
// The fixture is shaped like the edit the legacy ERING_TR.exe made to
// eldenring.exe (decompiled 2026-10-08): two NUL-terminated UTF-16 mount
// strings, 928 bytes apart, inside an otherwise arbitrary binary. A wrong
// answer here writes a broken game executable, so every refusal case matters
// as much as the success.

#include <gtest/gtest.h>
#include "bytepatch.h"

using namespace makine::bytepatch;

namespace {

QByteArray u16(const QString& s) { return decode(QStringLiteral("utf16:") + s); }

// eldenring.exe in miniature: binary noise, then the font mount string,
// then 904 bytes, then the msg mount string, then more noise.
QByteArray fakeEldenRing()
{
    QByteArray exe(4096, '\x90');
    exe.replace(1000, 24, u16(QStringLiteral("data0:/font") + QChar(0)));
    exe.replace(1928, 22, u16(QStringLiteral("data0:/msg") + QChar(0)));
    return exe;
}

QList<Pair> eldenRingPairs()
{
    QList<Pair> pairs;
    const bool ok = decodePairs({
        {QStringLiteral("utf16:data0:/font") + QChar(0), QStringLiteral("utf16:dvdroot:/fn") + QChar(0)},
        {QStringLiteral("utf16:data0:/msg") + QChar(0),
         QStringLiteral("utf16:dvdroot:/") + QChar(0) + QChar(0)},
    }, &pairs);
    EXPECT_TRUE(ok);
    return pairs;
}

} // namespace

TEST(BytePatch, DecodesEachEncoding)
{
    EXPECT_EQ(decode(QStringLiteral("utf16:ab")), QByteArray("a\0b\0", 4));
    EXPECT_EQ(decode(QStringLiteral("ascii:data\\locale")), QByteArray("data\\locale"));
    EXPECT_EQ(decode(QStringLiteral("hex:64007600")), QByteArray("d\0v\0", 4));
    EXPECT_EQ(decode(QStringLiteral("hex:DEADbeef")), QByteArray::fromHex("deadbeef"));
}

TEST(BytePatch, RefusesWhatItCannotReadExactly)
{
    EXPECT_TRUE(decode(QStringLiteral("data0:/font")).isEmpty());        // no encoding
    EXPECT_TRUE(decode(QStringLiteral("latin1:x")).isEmpty());           // unknown encoding
    EXPECT_TRUE(decode(QStringLiteral("hex:abc")).isEmpty());            // odd length
    EXPECT_TRUE(decode(QStringLiteral("hex:zz")).isEmpty());             // not hex
    EXPECT_TRUE(decode(QStringLiteral("ascii:ğ")).isEmpty());            // not 7-bit
}

TEST(BytePatch, PairsMustKeepEveryOffsetInPlace)
{
    EXPECT_FALSE(decodePairs({}, nullptr));
    EXPECT_FALSE(decodePairs({{QStringLiteral("utf16:data0:/msg"),
                               QStringLiteral("utf16:dvdroot:/")}}, nullptr));   // shorter
    EXPECT_FALSE(decodePairs({{QStringLiteral("ascii:abc"),
                               QStringLiteral("ascii:abc")}}, nullptr));         // no-op
    EXPECT_FALSE(decodePairs({{QStringLiteral("ascii:abc"), QStringLiteral("abc")}}, nullptr));
    EXPECT_TRUE(decodePairs({{QStringLiteral("ascii:abc"), QStringLiteral("hex:414243")}}, nullptr));
}

TEST(BytePatch, PointsEldenRingsMountStringsAtTheGameFolder)
{
    const QByteArray original = fakeEldenRing();
    QByteArray exe = original;
    ASSERT_EQ(apply(exe, eldenRingPairs()), Result::Patched);

    EXPECT_EQ(exe.size(), original.size());
    EXPECT_EQ(exe.mid(1000, 24), u16(QStringLiteral("dvdroot:/fn") + QChar(0)));
    EXPECT_EQ(exe.mid(1928, 22), u16(QStringLiteral("dvdroot:/") + QChar(0) + QChar(0)));
    // Nothing else moved or changed.
    EXPECT_EQ(exe.left(1000), original.left(1000));
    EXPECT_EQ(exe.mid(1024, 904), original.mid(1024, 904));
    EXPECT_EQ(exe.mid(1950), original.mid(1950));
}

TEST(BytePatch, ASecondRunIsAlreadyPatchedNotAnError)
{
    QByteArray exe = fakeEldenRing();
    ASSERT_EQ(apply(exe, eldenRingPairs()), Result::Patched);
    const QByteArray once = exe;
    EXPECT_EQ(apply(exe, eldenRingPairs()), Result::AlreadyPatched);
    EXPECT_EQ(exe, once);
}

TEST(BytePatch, AnotherGameBuildChangesNothing)
{
    // The msg string is gone: a different executable. The font string that
    // IS there must not be edited on its own.
    QByteArray exe = fakeEldenRing();
    exe.replace(1928, 22, QByteArray(22, '\x90'));
    const QByteArray before = exe;
    EXPECT_EQ(apply(exe, eldenRingPairs()), Result::NoMatch);
    EXPECT_EQ(exe, before);
}

TEST(BytePatch, TwoCandidatesAreNeverGuessedBetween)
{
    QByteArray exe = fakeEldenRing();
    exe.replace(3000, 24, u16(QStringLiteral("data0:/font") + QChar(0)));
    const QByteArray before = exe;
    EXPECT_EQ(apply(exe, eldenRingPairs()), Result::Ambiguous);
    EXPECT_EQ(exe, before);
}

TEST(BytePatch, TheTerminatorKeepsALongerStringOutOfIt)
{
    // "data0:/msgbox" must not count as "data0:/msg".
    QByteArray exe = fakeEldenRing();
    exe.replace(3000, 28, u16(QStringLiteral("data0:/msgbox") + QChar(0)));
    ASSERT_EQ(apply(exe, eldenRingPairs()), Result::Patched);
    EXPECT_EQ(exe.mid(3000, 28), u16(QStringLiteral("data0:/msgbox") + QChar(0)));
}

TEST(BytePatch, InvalidPairsAreRefusedBeforeLooking)
{
    QByteArray exe = fakeEldenRing();
    EXPECT_EQ(apply(exe, {}), Result::Invalid);
    EXPECT_EQ(apply(exe, {Pair{QByteArray("ab"), QByteArray("abc")}}), Result::Invalid);
}
