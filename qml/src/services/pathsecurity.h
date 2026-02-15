/**
 * @file pathsecurity.h
 * @brief Path security utilities — traversal prevention, safe join, validation
 * @copyright (c) 2026 MakineAI Team
 */

#pragma once

#include <QString>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace makineai::security {

// Check that resolved fullPath stays within basePath directory
inline bool isPathContained(const QString& basePath, const QString& fullPath) {
    QString canonBase = QFileInfo(basePath).canonicalFilePath();
    QString canonFull = QFileInfo(fullPath).canonicalFilePath();

    // Fallback to cleanPath if canonical resolution fails (non-existent paths)
    if (canonBase.isEmpty() || canonFull.isEmpty()) {
        canonBase = QDir::cleanPath(basePath);
        canonFull = QDir::cleanPath(fullPath);
    }

    // Append separator to prevent /foo/bar matching /foo/barBaz
    if (!canonBase.endsWith('/') && !canonBase.endsWith('\\'))
        canonBase += '/';

    return canonFull.startsWith(canonBase) ||
           canonFull == canonBase.chopped(1);
}

// Safely join base + relative path, return empty if escape detected
inline QString safePathJoin(const QString& basePath, const QString& relativePath) {
    if (relativePath.contains("..") || relativePath.startsWith('/') ||
        relativePath.startsWith('\\') || relativePath.contains("://") ||
        relativePath.contains(QChar(0)))
        return {};

    QString joined = QDir::cleanPath(basePath + '/' + relativePath);
    if (!isPathContained(basePath, joined)) {
        qWarning() << "Path escape blocked:" << relativePath;
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

} // namespace makineai::security
