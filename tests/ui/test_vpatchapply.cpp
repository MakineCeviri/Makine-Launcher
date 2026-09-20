// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The VPatch applier, against the reference project's own test vectors.
//
// Two catalogue packages are built on this format and neither could install:
// Fahrenheit (312840) ships .pat files and an NSIS plugin DLL that has no
// command line, and Assassin's Creed III (911400) ships a patch.exe that
// Windows refuses to start without elevation. Getting the format wrong would
// corrupt a multi-gigabyte game archive, so the bytes below are not invented:
// oldfile.txt, newfile.txt and patch.pat are lifted verbatim from NSIS
// Contrib/VPatch, and applying the patch to the old file must reproduce the
// new file exactly — the same assertion the reference implementation passes.

#include <gtest/gtest.h>

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>

#include "vpatchapply.h"

using namespace makine::vpatch;

namespace {

QByteArray fromHex(const char* hex) { return QByteArray::fromHex(hex); }

// NSIS Contrib/VPatch/patch.pat — 1 patch, MD5 mode, 3 blocks:
// copy 91 bytes from source offset 0, then 34 literal bytes, then a FILETIME.
const char* kPatchHex =
    "565041540100008003000000dcfb2bd10556fc8dd280afa67d6a146ade333d4b"
    "27b86c4cdf8e7ffa0405c36833000000015b0000000005226e657766696c6520"
    "2d207670617463680d0a0d0a3637383930202d204748494a4b4cff008317828b"
    "1fc301";

// NSIS Contrib/VPatch/oldfile.txt
const char* kOldHex =
    "2a2a2a2054484953204953204120544553542046494c4520464f522054484520"
    "565041544348204558414d504c45202a2a2a0d0a2a2a2a20434f4d50494c4520"
    "4558414d504c452e4e534920544f2054455354202a2a2a0d0a0d0a6f6c646669"
    "6c65202d207670617463680d0a0d0a3132333435202d20414243444546";

// NSIS Contrib/VPatch/newfile.txt
const char* kNewHex =
    "2a2a2a2054484953204953204120544553542046494c4520464f522054484520"
    "565041544348204558414d504c45202a2a2a0d0a2a2a2a20434f4d50494c4520"
    "4558414d504c452e4e534920544f2054455354202a2a2a0d0a0d0a6e65776669"
    "6c65202d207670617463680d0a0d0a3637383930202d204748494a4b4c";

struct Applied {
    Result result;
    QByteArray produced;
};

Applied runPatch(const QByteArray& patchBytes, const QByteArray& sourceBytes)
{
    QByteArray patchData = patchBytes;
    QByteArray sourceData = sourceBytes;
    QByteArray destData;

    QBuffer patch(&patchData);
    QBuffer source(&sourceData);
    QBuffer dest(&destData);
    patch.open(QIODevice::ReadOnly);
    source.open(QIODevice::ReadOnly);
    dest.open(QIODevice::ReadWrite);

    const Result r = apply(patch, source, dest);
    return {r, destData};
}

} // namespace

TEST(VPatchApply, TheReferenceVectorReproducesTheTargetExactly)
{
    const Applied run = runPatch(fromHex(kPatchHex), fromHex(kOldHex));
    EXPECT_EQ(run.result, Result::Success);
    EXPECT_EQ(run.produced, fromHex(kNewHex));
    // 91 copied from the source + 34 literal.
    EXPECT_EQ(run.produced.size(), 125);
}

TEST(VPatchApply, PatchingTheTargetAgainSaysUpToDateAndWritesNothing)
{
    // Re-running an install must not corrupt a game that is already patched.
    const Applied run = runPatch(fromHex(kPatchHex), fromHex(kNewHex));
    EXPECT_EQ(run.result, Result::UpToDate);
    EXPECT_TRUE(run.produced.isEmpty());
}

TEST(VPatchApply, AnUnrelatedSourceIsRefusedWithoutWriting)
{
    // A different game version, or a file the user already modified. Nothing
    // may be produced: a half-written archive is worse than a refusal.
    const Applied run = runPatch(fromHex(kPatchHex), QByteArray("some other file entirely"));
    EXPECT_EQ(run.result, Result::NoMatch);
    EXPECT_TRUE(run.produced.isEmpty());
}

TEST(VPatchApply, AWrongTargetChecksumIsAnErrorNotASuccess)
{
    // The last line of defence. Flip one byte of the literal block so the
    // produced bytes are wrong while the patch still parses and its source
    // digest still matches: the applier must NOT report success.
    QByteArray tampered = fromHex(kPatchHex);
    const int literal = tampered.indexOf(QByteArray("newfile"));
    ASSERT_GT(literal, 0);
    tampered[literal] = 'N';

    const Applied run = runPatch(tampered, fromHex(kOldHex));
    EXPECT_EQ(run.result, Result::Error);
    EXPECT_NE(run.produced, fromHex(kNewHex));
}

TEST(VPatchApply, HeaderDamageIsReportedAsCorrupt)
{
    // Not "error": a patch file that is not a patch file is a packaging fault,
    // and the two are different things to chase.
    QByteArray notVpat = fromHex(kPatchHex);
    notVpat[0] = 'X';
    // With no "VPAT" at the start the applier reads the last four bytes as an
    // offset; here that lands nowhere useful, which is exactly the corrupt case.
    EXPECT_EQ(runPatch(notVpat, fromHex(kOldHex)).result, Result::Corrupt);

    EXPECT_EQ(runPatch(QByteArray(), fromHex(kOldHex)).result, Result::Corrupt);
    EXPECT_EQ(runPatch(QByteArray("VPAT"), fromHex(kOldHex)).result, Result::Corrupt);

    // Truncated mid-block: the declared length runs past the end of the file.
    QByteArray cut = fromHex(kPatchHex);
    cut.truncate(cut.size() - 20);
    const Result r = runPatch(cut, fromHex(kOldHex)).result;
    EXPECT_TRUE(r == Result::Corrupt || r == Result::Error) << resultName(r);
}

TEST(VPatchApply, TheHeaderMayLiveAtAnOffsetNamedByTheLastFourBytes)
{
    // Older generators append the header offset instead of putting the header
    // first; the reference applier handles both and so must this one.
    const QByteArray original = fromHex(kPatchHex);
    QByteArray wrapped;
    wrapped.append(QByteArray(7, '\0'));          // junk prefix
    const quint32 offset = 7;
    wrapped.append(original);
    wrapped.append(reinterpret_cast<const char*>(&offset), 4);

    const Applied run = runPatch(wrapped, fromHex(kOldHex));
    EXPECT_EQ(run.result, Result::Success);
    EXPECT_EQ(run.produced, fromHex(kNewHex));
}

TEST(VPatchApply, ResultNamesAreStableForTelemetry)
{
    EXPECT_STREQ(resultName(Result::Success),  "success");
    EXPECT_STREQ(resultName(Result::UpToDate), "uptodate");
    EXPECT_STREQ(resultName(Result::NoMatch),  "nomatch");
    EXPECT_STREQ(resultName(Result::Corrupt),  "corrupt");
    EXPECT_STREQ(resultName(Result::Error),    "error");
}
