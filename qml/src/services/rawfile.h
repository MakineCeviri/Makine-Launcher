// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file rawfile.h
 * @brief File presence and size that do not follow Windows shortcuts
 */

#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <filesystem>
#include <system_error>

namespace makine::rawfile {

// Qt reads every *.lnk as a shell shortcut and answers for its target: a game
// data file that merely ends in .lnk (Nioh's archive/archive_19.lnk) "does not
// exist", has size 0 and is skipped by QDir::Files. The original then went
// unbacked-up and a copy over it destroyed it. Other names keep Qt's answer,
// which also copes with paths past MAX_PATH.
inline bool isLnk(const QString& path)
{
    return path.endsWith(QLatin1String(".lnk"), Qt::CaseInsensitive);
}

inline bool exists(const QString& path)
{
    if (!isLnk(path))
        return QFileInfo::exists(path);
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path.toStdWString()), ec);
}

inline qint64 size(const QString& path)
{
    if (!isLnk(path))
        return QFileInfo(path).size();
    std::error_code ec;
    const auto n = std::filesystem::file_size(std::filesystem::path(path.toStdWString()), ec);
    return ec ? 0 : static_cast<qint64>(n);
}

// QDirIterator filter for "every file": Qt lists a .lnk only under System.
inline constexpr QDir::Filters kFiles = QDir::Files | QDir::System;

} // namespace makine::rawfile
