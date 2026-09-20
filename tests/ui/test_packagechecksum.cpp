// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The gate that decides whether a 600 MB download is the package we published.
//
// There was none. translationdownloader.cpp read no checksum,
// config.verifyChecksums (default true) was read nowhere but config.cpp, and
// compressedChecksum never reached C++. The values below are the two spellings
// actually present in the catalogue on 2026-09-20 — package_pipeline.py writes
// "sha256:<hex>", the entries already in index.json carry a bare digest — and
// a reader that handled only one would silently verify nothing for half of it.

#include <gtest/gtest.h>
#include "packagechecksum.h"

using namespace makine::pkgchecksum;

// Half-Life's real digest, measured from the R2 object on 2026-09-20.
static const QString kHalfLife =
    QStringLiteral("b15fc02af10620b6a3b069705f45e0f6b9b262fa2056c89a627a93034ac84d86");

TEST(PackageChecksum, BothSpellingsInTheCatalogueParse)
{
    EXPECT_EQ(normalizeChecksum(QStringLiteral("sha256:") + kHalfLife), kHalfLife);
    EXPECT_EQ(normalizeChecksum(kHalfLife), kHalfLife);
    EXPECT_EQ(normalizeChecksum(QStringLiteral("SHA256:") + kHalfLife.toUpper()),
              kHalfLife);
    EXPECT_EQ(normalizeChecksum(QStringLiteral("  sha256:") + kHalfLife + QStringLiteral("  ")),
              kHalfLife);
}

TEST(PackageChecksum, NothingToVerifyAgainstIsNotAMismatch)
{
    // 54 of 239 catalogue entries carried no checksum at all. Treating that as
    // a failure would refuse installs the launcher has always allowed.
    EXPECT_FALSE(isVerifiable(QString()));
    EXPECT_FALSE(isVerifiable(QStringLiteral("")));
    EXPECT_FALSE(isVerifiable(QStringLiteral("sha256:")));
    EXPECT_TRUE(checksumMatches(QString(), kHalfLife));
    EXPECT_TRUE(checksumMatches(QStringLiteral("sha256:"), kHalfLife));
}

TEST(PackageChecksum, AnAlgorithmThisBuildCannotComputeIsNotAMismatch)
{
    // Refusing over an algorithm we simply do not read would break every
    // package published with it, on builds that are already in the field.
    EXPECT_FALSE(isVerifiable(QStringLiteral("blake3:") + kHalfLife));
    EXPECT_TRUE(checksumMatches(QStringLiteral("blake3:") + kHalfLife, kHalfLife));
    // A malformed digest is equally unusable, and equally not a verdict.
    EXPECT_FALSE(isVerifiable(QStringLiteral("sha256:deadbeef")));
    EXPECT_FALSE(isVerifiable(QStringLiteral("sha256:") + kHalfLife + QStringLiteral("00")));
    EXPECT_FALSE(isVerifiable(QStringLiteral("sha256:zzzz02af10620b6a3b069705f45e0f6"
                                             "b9b262fa2056c89a627a93034ac84d86")));
}

TEST(PackageChecksum, ARealMismatchIsCaught)
{
    QString other = kHalfLife;
    other[0] = QLatin1Char('c');   // b15f… -> c15f…
    EXPECT_FALSE(checksumMatches(QStringLiteral("sha256:") + kHalfLife, other));
    EXPECT_FALSE(checksumMatches(kHalfLife, other));
    // Matching is case-insensitive on both sides.
    EXPECT_TRUE(checksumMatches(QStringLiteral("sha256:") + kHalfLife.toUpper(),
                                kHalfLife));
    // A truncated download cannot produce a digest of the right length, so the
    // computed side being unusable must still fail.
    EXPECT_FALSE(checksumMatches(QStringLiteral("sha256:") + kHalfLife,
                                 QStringLiteral("deadbeef")));
    EXPECT_FALSE(checksumMatches(QStringLiteral("sha256:") + kHalfLife, QString()));
}
