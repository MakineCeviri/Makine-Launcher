#include <gtest/gtest.h>
#include "pathsecurity.h"

namespace makine::testing {

// --- isPathSafe ---

TEST(PathSecurity, SafeNormalPath) {
    EXPECT_TRUE(security::isPathSafe("C:/Games/Skyrim"));
}

TEST(PathSecurity, SafeWindowsPath) {
    EXPECT_TRUE(security::isPathSafe("C:/Games/Skyrim"));
}

TEST(PathSecurity, UnsafeEmpty) {
    EXPECT_FALSE(security::isPathSafe(""));
}

TEST(PathSecurity, UnsafeTraversal) {
    EXPECT_FALSE(security::isPathSafe("C:/Games/../etc/passwd"));
}

TEST(PathSecurity, UnsafeNullByte) {
    QString withNull = QString("C:/Games/test") + QChar(0) + ".txt";
    EXPECT_FALSE(security::isPathSafe(withNull));
}

TEST(PathSecurity, UnsafeUncBackslash) {
    EXPECT_FALSE(security::isPathSafe("\\\\server\\share"));
}

TEST(PathSecurity, UnsafeUncForwardSlash) {
    EXPECT_FALSE(security::isPathSafe("//server/share"));
}

// --- safePathJoin ---

TEST(PathSecurity, JoinNormal) {
    QString result = security::safePathJoin("C:/Games", "Skyrim/data");
    EXPECT_FALSE(result.isEmpty());
    EXPECT_TRUE(result.contains("Skyrim"));
}

TEST(PathSecurity, JoinTraversalBlocked) {
    EXPECT_TRUE(security::safePathJoin("C:/Games", "../etc/passwd").isEmpty());
}

TEST(PathSecurity, JoinAbsoluteRelativeBlocked) {
    EXPECT_TRUE(security::safePathJoin("C:/Games", "/etc/passwd").isEmpty());
}

TEST(PathSecurity, JoinProtocolBlocked) {
    EXPECT_TRUE(security::safePathJoin("C:/Games", "http://evil.com").isEmpty());
}

TEST(PathSecurity, JoinNullByteBlocked) {
    QString rel = QString("test") + QChar(0) + ".txt";
    EXPECT_TRUE(security::safePathJoin("C:/Games", rel).isEmpty());
}

// --- isPathContained ---

TEST(PathSecurity, ContainedInside) {
    EXPECT_TRUE(security::isPathContained("C:/Games", "C:/Games/Skyrim/data"));
}

TEST(PathSecurity, ContainedEscape) {
    EXPECT_FALSE(security::isPathContained("C:/Games", "C:/Other/file.txt"));
}

TEST(PathSecurity, ContainedExactBase) {
    EXPECT_TRUE(security::isPathContained("C:/Games", "C:/Games"));
}

TEST(PathSecurity, ContainedSimilarPrefix) {
    EXPECT_FALSE(security::isPathContained("C:/foo/bar", "C:/foo/barBaz"));
}

} // namespace makine::testing
