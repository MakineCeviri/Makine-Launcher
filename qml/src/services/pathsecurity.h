// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file pathsecurity.h
 * @brief Path security utilities — traversal prevention, safe join, validation
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

static inline const QLoggingCategory &lcPathSecurity() {
    static const QLoggingCategory category("makine.security");
    return category;
}

namespace makine::security {

// Resolve as much of a path as actually exists, keeping the rest verbatim.
//
// canonicalFilePath() answers with nothing for a path that is not on disk, and
// a file about to be CREATED is exactly that. Resolving only the side that
// happens to exist then compares a resolved path against an unresolved one, and
// anything the resolution would have removed — a junction in a Steam library
// path, a symlinked game folder — reads as an escape.
inline QString resolveThroughExisting(const QString& path) {
    const QString cleaned = QDir::cleanPath(path);
    QString head = cleaned;
    QString tail;

    // Bounded: every pass drops one segment, and a path has finitely many.
    for (int guard = 0; guard < 256; ++guard) {
        const QString canon = QFileInfo(head).canonicalFilePath();
        if (!canon.isEmpty()) {
            return tail.isEmpty() ? QDir::cleanPath(canon)
                                  : QDir::cleanPath(canon + QLatin1Char('/') + tail);
        }

        const int slash = head.lastIndexOf(QLatin1Char('/'));
        if (slash <= 0)
            break;                       // no parent left to try
        const QString segment = head.mid(slash + 1);
        if (segment.isEmpty())
            break;                       // "C:/" — already at a root
        tail = tail.isEmpty() ? segment : segment + QLatin1Char('/') + tail;
        head.truncate(slash);
        if (head.endsWith(QLatin1Char(':')))
            head += QLatin1Char('/');    // "C:" names the drive, "C:/" its root
    }

    // Nothing along this path exists. Both sides then get identical treatment,
    // which is all containment needs.
    return cleaned;
}

// Check that resolved fullPath stays within basePath directory
//
// Traversal stays blocked: cleanPath() collapses ".." before anything here
// looks at the result, so an escaping path is a different prefix rather than a
// disguised one.
inline bool isPathContained(const QString& basePath, const QString& fullPath) {
    if (basePath.isEmpty() || fullPath.isEmpty())
        return false;

    QString canonBase = resolveThroughExisting(basePath);
    const QString canonFull = resolveThroughExisting(fullPath);
    if (canonBase.isEmpty() || canonFull.isEmpty())
        return false;

    // Windows paths are case-insensitive: the same directory spelled with
    // different case IS the same directory. Comparing case-sensitively refused
    // 16 of 20 files in a backup restore and left the game half-patched.
#ifdef Q_OS_WIN
    constexpr auto kCase = Qt::CaseInsensitive;
#else
    constexpr auto kCase = Qt::CaseSensitive;
#endif

    // Append separator to prevent /foo/bar matching /foo/barBaz
    if (!canonBase.endsWith(QLatin1Char('/')) && !canonBase.endsWith(QLatin1Char('\\')))
        canonBase += QLatin1Char('/');

    return canonFull.startsWith(canonBase, kCase) ||
           canonFull.compare(canonBase.chopped(1), kCase) == 0;
}

// Safely join base + relative path, return empty if escape detected
inline QString safePathJoin(const QString& basePath, const QString& relativePath) {
    if (relativePath.contains("..") || relativePath.startsWith('/') ||
        relativePath.startsWith('\\') || relativePath.contains("://") ||
        relativePath.contains(QChar(0)))
        return {};

    QString joined = QDir::cleanPath(basePath + '/' + relativePath);
    if (!isPathContained(basePath, joined)) {
        qCWarning(lcPathSecurity()) << "Path escape blocked:" << relativePath;
        return {};
    }
    return joined;
}

// Validate that a user-provided path is safe
inline bool isPathSafe(const QString& path) {
    if (path.isEmpty()) return false;
    if (path.contains("..")) return false;
    if (path.contains(QChar(0))) return false;
    if (path.startsWith("\\\\") || path.startsWith("//")) return false;
    return true;
}

} // namespace makine::security

namespace makine::fileutils {

// Write data to path atomically: write .tmp then rename
inline bool atomicWriteJson(const QString& path, const QByteArray& data) {
    const QString tmpPath = path + ".tmp";
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly)) return false;
    if (tmpFile.write(data) != data.size()) {
        tmpFile.close();
        QFile::remove(tmpPath);
        return false;
    }
    tmpFile.close();
    // Windows: rename fails if target exists
    if (QFile::exists(path)) QFile::remove(path);
    return QFile::rename(tmpPath, path);
}

} // namespace makine::fileutils
