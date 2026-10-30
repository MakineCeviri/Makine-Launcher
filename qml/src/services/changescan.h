// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// What a patch's own program did to the game folder.
//
// A "run" step used to be judged by its exit code alone. A window the user
// closed without pressing "Kur" exits 0; ACS.exe pointed at a folder without
// the game's archives patches nothing and exits 0. Both were "kuruldu". The
// folder itself tells the truth: its files are listed (size, modification
// time) before the program starts and again after it ends, and the difference
// is what the program did — nothing at all means it installed nothing.
//
// Header-only, Qt Core only — tests/ui/test_changescan.cpp runs it on real
// directory trees.

#include "rawfile.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace makine::changescan {

struct FileState {
    qint64 size = -1;
    qint64 modified = 0;   // ms since epoch
    bool operator==(const FileState&) const = default;
};

struct Snapshot {
    QHash<QString, FileState> files;   // path relative to the root, '/'-separated
    bool complete = true;              // false: stopped at the entry limit
};

// Every file under `root`, without following links. `skip` names files the
// launcher writes itself (relative paths); they are not the program's doing.
inline Snapshot take(const QString& root, const QStringList& skip = {}, int maxFiles = 250000)
{
    Snapshot s;
    const QDir base(root);
    QDirIterator it(root, rawfile::kFiles | QDir::Hidden | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (s.files.size() >= maxFiles) {
            s.complete = false;
            break;
        }
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString rel = base.relativeFilePath(fi.filePath());
        if (skip.contains(rel, Qt::CaseInsensitive)) continue;
        s.files.insert(rel, {fi.size(), fi.lastModified().toMSecsSinceEpoch()});
    }
    return s;
}

struct Diff {
    QStringList created;
    QStringList modified;
    QStringList removed;
    bool isEmpty() const { return created.isEmpty() && modified.isEmpty() && removed.isEmpty(); }
    int count() const { return int(created.size() + modified.size() + removed.size()); }
};

inline Diff compare(const Snapshot& before, const Snapshot& after)
{
    Diff d;
    for (auto it = after.files.cbegin(); it != after.files.cend(); ++it) {
        const auto was = before.files.constFind(it.key());
        if (was == before.files.cend()) d.created.append(it.key());
        else if (!(*was == *it)) d.modified.append(it.key());
    }
    for (auto it = before.files.cbegin(); it != before.files.cend(); ++it)
        if (!after.files.contains(it.key())) d.removed.append(it.key());
    d.created.sort(Qt::CaseInsensitive);
    d.modified.sort(Qt::CaseInsensitive);
    d.removed.sort(Qt::CaseInsensitive);
    return d;
}

// How the record keeps what a program did: one marker per file, so uninstall
// leaves those files to the program that undoes it (they may be its backups).
inline constexpr char kMarker[] = "_program:";

inline QStringList markers(const Diff& d, int cap = 2000)
{
    QStringList out;
    for (const QStringList* list : {&d.modified, &d.created}) {
        for (const QString& f : *list) {
            if (out.size() >= cap) return out;
            out.append(QLatin1String(kMarker) + f);
        }
    }
    return out;
}

// Does an install record leave anything to the patch's undo program? What the
// program changed is on record (markers). A record from before that was tracked
// (no launcher note, `noteFile`) cannot say, so the program runs — unless the
// record lists files the launcher copied itself: that patch came from an older
// recipe with no program. RDR2's was copied hook files (dinput8.dll …) until
// KUR.bat; its KALDIR.bat, run over such a record, changed nothing or stopped
// at its own prompt, and the update was refused (Sentry 2026-10-29: 30 players).
inline bool recordLeftToProgram(const std::vector<std::string>& installedFiles,
                                std::string_view noteFile)
{
    bool changes = false, tracked = false, copied = false;
    for (const std::string& f : installedFiles) {
        if (f.starts_with(kMarker)) changes = true;
        else if (f == noteFile) tracked = true;
        else if (!f.starts_with('_')) copied = true;
    }
    return changes || (!tracked && !copied);
}

} // namespace makine::changescan
