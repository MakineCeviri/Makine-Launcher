// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include <gtest/gtest.h>
#include "pathsecurity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

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

// =========================================================================
// CONTAINMENT WITH REAL DIRECTORIES
// =========================================================================
//
// A restore writes files that are NOT on disk yet. canonicalFilePath() answers
// with nothing for those, so the check used to compare a resolved base against
// an unresolved destination — and every difference the resolution removes read
// as an escape. Live effect: a backup restore refused 16 of 20 files, all of
// them ordinary subdirectory paths like "DataPC_13_dlc/21596.aco", and left the
// game half-patched with no way back. 155 events across 19 users.

namespace {

// A real directory, so the base side canonicalises and the destination side
// (a file that does not exist yet) has to agree with it.
class RealDir {
public:
    RealDir() {
        path_ = QDir::cleanPath(QDir::tempPath()
            + QStringLiteral("/makine_pathsec_%1").arg(QCoreApplication::applicationPid()));
        QDir().mkpath(path_ + QStringLiteral("/DataPC_13_dlc"));
    }
    ~RealDir() { QDir(path_).removeRecursively(); }
    const QString& get() const { return path_; }
private:
    QString path_;
};

} // namespace

// backupmanager.cpp passes a canonicalised base (QDir::canonicalPath, which
// resolves junctions and symlinks) together with a destination built from the
// raw recorded path. Two ways that pairing used to fail, both fixed here: the
// destination not existing yet (nothing to canonicalise, so one side stayed
// resolved and the other did not), and the two spellings differing in case.

TEST(PathSecurityReal, DestinationThatDoesNotExistYetIsStillContained) {
    RealDir base;
    const QString dest = base.get() + QStringLiteral("/DataPC_13_dlc/21596.aco");
    ASSERT_FALSE(QFileInfo::exists(dest));
    EXPECT_TRUE(makine::security::isPathContained(base.get(), dest));
}

TEST(PathSecurityReal, MissingIntermediateDirectoryIsStillContained) {
    RealDir base;
    const QString dest = base.get() + QStringLiteral("/DataPC_99_dlc/deep/new.aco");
    EXPECT_TRUE(makine::security::isPathContained(base.get(), dest));
}

TEST(PathSecurityReal, EscapeFromARealDirectoryIsStillBlocked) {
    RealDir base;
    EXPECT_FALSE(makine::security::isPathContained(base.get(),
        QDir::cleanPath(base.get() + QStringLiteral("/../escaped.txt"))));
    EXPECT_FALSE(makine::security::isPathContained(base.get(),
        QDir::cleanPath(base.get() + QStringLiteral("/DataPC_13_dlc/../../escaped.txt"))));
}

TEST(PathSecurityReal, SiblingWithASharedPrefixIsStillBlocked) {
    RealDir base;
    EXPECT_FALSE(makine::security::isPathContained(base.get(), base.get() + QStringLiteral("Extra/f.txt")));
}

TEST(PathSecurityReal, DifferentCaseIsTheSameDirectoryOnWindows) {
    RealDir base;
    const QString dest = base.get().toUpper() + QStringLiteral("/DataPC_13_dlc/21596.aco");
#ifdef Q_OS_WIN
    EXPECT_TRUE(makine::security::isPathContained(base.get(), dest));
#else
    EXPECT_FALSE(makine::security::isPathContained(base.get(), dest));
#endif
}

TEST(PathSecurityReal, EmptyArgumentsAreNotContained) {
    EXPECT_FALSE(makine::security::isPathContained("", "C:/Games/x"));
    EXPECT_FALSE(makine::security::isPathContained("C:/Games", ""));
}
