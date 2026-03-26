/**
 * @file test_updateservice.cpp
 * @brief Unit tests for UpdateService::compareVersions algorithm
 *
 * Tests a standalone replica of the version comparison logic.
 * We cannot link against UpdateService directly (requires QML engine),
 * so the algorithm is replicated here to validate correctness.
 */

#include <gtest/gtest.h>
#include <QString>
#include <QStringList>
#include <QLatin1Char>

namespace makine::testing {

// Standalone replica of UpdateService::compareVersions (updateservice.cpp:510-526)
static int compareVersions(const QString& v1, const QString& v2)
{
    if (v1.isEmpty() || v2.isEmpty())
        return 0;

    const QStringList parts1 = v1.split(QLatin1Char('.'));
    const QStringList parts2 = v2.split(QLatin1Char('.'));

    const int maxLen = qMax(parts1.size(), parts2.size());
    for (int i = 0; i < maxLen; ++i) {
        const int p1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        const int p2 = (i < parts2.size()) ? parts2[i].toInt() : 0;
        if (p1 > p2) return 1;
        if (p1 < p2) return -1;
    }
    return 0;
}

// -- Equal versions --

TEST(CompareVersions, EqualVersions)
{
    EXPECT_EQ(compareVersions("1.0.0", "1.0.0"), 0);
}

TEST(CompareVersions, EqualTwoPart)
{
    EXPECT_EQ(compareVersions("2.5", "2.5"), 0);
}

// -- Greater / lesser major --

TEST(CompareVersions, GreaterMajor)
{
    EXPECT_EQ(compareVersions("2.0.0", "1.0.0"), 1);
}

TEST(CompareVersions, LesserMajor)
{
    EXPECT_EQ(compareVersions("1.0.0", "2.0.0"), -1);
}

// -- Greater / lesser minor --

TEST(CompareVersions, GreaterMinor)
{
    EXPECT_EQ(compareVersions("1.2.0", "1.1.0"), 1);
}

TEST(CompareVersions, LesserMinor)
{
    EXPECT_EQ(compareVersions("1.1.0", "1.2.0"), -1);
}

// -- Greater / lesser patch --

TEST(CompareVersions, GreaterPatch)
{
    EXPECT_EQ(compareVersions("1.0.2", "1.0.1"), 1);
}

TEST(CompareVersions, LesserPatch)
{
    EXPECT_EQ(compareVersions("1.0.1", "1.0.2"), -1);
}

// -- Different length versions --

TEST(CompareVersions, DifferentLengthEqual)
{
    // "1.0.0" vs "1.0" — trailing zero is implicit
    EXPECT_EQ(compareVersions("1.0.0", "1.0"), 0);
}

TEST(CompareVersions, DifferentLengthGreater)
{
    // "1.0.1" vs "1.0" — patch component makes it greater
    EXPECT_EQ(compareVersions("1.0.1", "1.0"), 1);
}

TEST(CompareVersions, DifferentLengthLesser)
{
    EXPECT_EQ(compareVersions("1.0", "1.0.1"), -1);
}

// -- Pre-alpha range --

TEST(CompareVersions, PreAlphaMinorBump)
{
    EXPECT_EQ(compareVersions("0.1.0", "0.0.9"), 1);
}

TEST(CompareVersions, PreAlphaEqual)
{
    EXPECT_EQ(compareVersions("0.0.1", "0.0.1"), 0);
}

// -- Empty string handling --

TEST(CompareVersions, BothEmpty)
{
    EXPECT_EQ(compareVersions("", ""), 0);
}

TEST(CompareVersions, FirstEmpty)
{
    // Per implementation: either empty -> return 0
    EXPECT_EQ(compareVersions("", "1.0.0"), 0);
}

TEST(CompareVersions, SecondEmpty)
{
    EXPECT_EQ(compareVersions("1.0.0", ""), 0);
}

// -- Edge cases --

TEST(CompareVersions, SingleComponent)
{
    EXPECT_EQ(compareVersions("2", "1"), 1);
    EXPECT_EQ(compareVersions("1", "2"), -1);
    EXPECT_EQ(compareVersions("1", "1"), 0);
}

TEST(CompareVersions, LargeVersionNumbers)
{
    EXPECT_EQ(compareVersions("1.0.100", "1.0.99"), 1);
    EXPECT_EQ(compareVersions("10.0.0", "9.9.9"), 1);
}

TEST(CompareVersions, FourPartVersion)
{
    EXPECT_EQ(compareVersions("1.0.0.1", "1.0.0.0"), 1);
    EXPECT_EQ(compareVersions("1.0.0.0", "1.0.0.1"), -1);
    EXPECT_EQ(compareVersions("1.0.0.0", "1.0.0"), 0);
}

} // namespace makine::testing
