// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

/**
 * @file packagestamp.h
 * @brief Which package an extracted folder under packages/ came from
 *
 * Installs and updates use packages/<dirName>/ as is when it holds files.
 * Without knowing which .makine filled it, "Güncelle" re-applied the old
 * files and recorded the new version as installed — a package republished
 * in the panel never reached anyone who had installed it before.
 *
 * The stamp is the catalogue SHA-256 of the package a folder was extracted
 * from, kept for all folders in one file next to them (a file inside the
 * folder would be copied into the game). A folder whose stamp does not match
 * the catalogue, or that has none (extracted before stamps existed), is not
 * reused.
 */

#include "apppaths.h"
#include "packagechecksum.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

namespace makine::pkgstamp {

inline QString filePath() { return AppPaths::dataDir() + QStringLiteral("/package_stamps.json"); }

inline QJsonObject load()
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

inline void save(const QJsonObject& stamps)
{
    QSaveFile f(filePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(stamps).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

/// The normalized checksum the folder was extracted from, or empty when unknown.
inline QString get(const QString& dirName)
{
    return load().value(dirName).toString();
}

/// Record that packages/<dirName>/ now holds the package with @p catalogChecksum.
inline void set(const QString& dirName, const QString& catalogChecksum)
{
    QJsonObject stamps = load();
    const QString hex = pkgchecksum::normalizeChecksum(catalogChecksum);
    if (hex.isEmpty()) stamps.remove(dirName);
    else stamps.insert(dirName, hex);
    save(stamps);
}

inline void clear(const QString& dirName)
{
    QJsonObject stamps = load();
    if (stamps.contains(dirName)) {
        stamps.remove(dirName);
        save(stamps);
    }
}

/// Can packages/<dirName>/ stand in for the package the catalogue lists now?
/// An unverifiable catalogue value keeps the old behaviour (reuse).
inline bool matches(const QString& dirName, const QString& catalogChecksum)
{
    const QString want = pkgchecksum::normalizeChecksum(catalogChecksum);
    return want.isEmpty() || get(dirName) == want;
}

} // namespace makine::pkgstamp
