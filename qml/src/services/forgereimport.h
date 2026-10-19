// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Reimport files into an Anvil ".forge" archive — what the legacy installer
// did with `qb.exe -w -r -r acv.bms <forge> <folder>` (QuickBMS 0.11.0,
// "reimport2" mode), done in-process.
//
// AC Valhalla and AC Mirage are translated this way (legacy installer
// decompiled 2026-10-08). Shipping QuickBMS meant a 20 MB third-party exe in
// the package that antivirus products quarantine, that waits for a key press
// on error, and that runs with whatever paths it is given; the launcher now
// writes the archive itself and QuickBMS is no longer needed.
//
// Layout, as acv.bms reads it (little-endian):
//
//   0x00                "scimitar"           magic
//   0x0D                u64 H                header offset
//   H + 0x2C            u32 N                file count
//   H + 0x34            u64 T                file table offset
//   T + 20*i            u64 offset, u64 asset id, u32 size
//
// Entry i is replaced by "<i>.acv" from the reimport folder. Reimport2 rules,
// measured against qb.exe on synthetic archives (test_forgereimport.cpp holds
// its outputs):
//
//   - entries go in ascending index; files with an index >= N are ignored
//   - new size <= old size: written at the old offset, the rest of the old
//     slot zero-filled, the size field updated
//   - larger: the old slot zero-filled, the data appended at the end of the
//     archive (no alignment), offset and size fields updated; appends follow
//     each other in index order
//   - every other byte of the archive is left as it was
//
// A second run over the result changes nothing: every replaced entry is then
// exactly the size of its file and is rewritten in place. That is what lets a
// re-install or an update run the step again.
//
// Nothing is written before every entry has been checked against the archive,
// so a wrong file is refused rather than half-written.

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QList>
#include <QMap>
#include <QString>
#include <QtEndian>

namespace makine::forgereimport {

enum class Result {
    Done,       // every file written
    NoFiles,    // the folder holds no "<index>.acv" this archive has
    NotAForge,  // the archive's header or table does not describe this file
    IoError,    // reading or writing the archive failed
};

inline const char* resultName(Result r)
{
    switch (r) {
    case Result::Done:      return "done";
    case Result::NoFiles:   return "no-files";
    case Result::NotAForge: return "not-a-forge";
    case Result::IoError:   return "io-error";
    }
    return "?";
}

// "<index>.acv" files of a reimport folder, by index. QuickBMS looks each
// entry up by its decimal name, so "0012.acv" is not entry 12.
inline bool loadFolder(const QString& dir, QMap<quint32, QByteArray>* files)
{
    const QFileInfoList entries =
        QDir(dir).entryInfoList({QStringLiteral("*.acv")}, QDir::Files);
    for (const QFileInfo& fi : entries) {
        bool ok = false;
        const quint32 index = fi.completeBaseName().toUInt(&ok);
        if (!ok || QString::number(index) != fi.completeBaseName()) continue;
        QFile f(fi.filePath());
        if (!f.open(QIODevice::ReadOnly)) return false;
        files->insert(index, f.readAll());
    }
    return true;
}

namespace detail {

inline bool readAt(QIODevice& dev, qint64 pos, char* out, qint64 n)
{
    return dev.seek(pos) && dev.read(out, n) == n;
}

inline bool writeAt(QIODevice& dev, qint64 pos, const QByteArray& bytes)
{
    return dev.seek(pos) && dev.write(bytes) == bytes.size();
}

template <typename T>
inline QByteArray le(T v)
{
    QByteArray b(sizeof(T), '\0');
    qToLittleEndian(v, b.data());
    return b;
}

} // namespace detail

inline Result apply(QIODevice& forge, const QMap<quint32, QByteArray>& files)
{
    using detail::readAt;
    const qint64 size = forge.size();

    char magic[8];
    if (!readAt(forge, 0, magic, 8) || QByteArray(magic, 8) != QByteArrayLiteral("scimitar"))
        return Result::NotAForge;

    char u64[8], u32[4];
    if (!readAt(forge, 0x0D, u64, 8)) return Result::NotAForge;
    const quint64 header = qFromLittleEndian<quint64>(u64);
    if (header > quint64(size) || quint64(size) - header < 0x3C) return Result::NotAForge;
    if (!readAt(forge, qint64(header) + 0x2C, u32, 4)) return Result::NotAForge;
    const quint32 count = qFromLittleEndian<quint32>(u32);
    if (!readAt(forge, qint64(header) + 0x34, u64, 8)) return Result::NotAForge;
    const quint64 table = qFromLittleEndian<quint64>(u64);
    if (table > quint64(size) || (quint64(size) - table) / 20 < count) return Result::NotAForge;

    // Plan every write first; touch the archive only once all of it holds.
    struct Write { qint64 at; QByteArray bytes; };
    QList<Write> appends, inPlace;
    qint64 end = size;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (it.key() >= count) continue;
        const QByteArray& data = it.value();
        if (quint64(data.size()) > 0xFFFFFFFFull) return Result::NotAForge;

        const qint64 entry = qint64(table) + 20 * qint64(it.key());
        char raw[20];
        if (!readAt(forge, entry, raw, 20)) return Result::IoError;
        const quint64 offset = qFromLittleEndian<quint64>(raw);
        const quint32 oldSize = qFromLittleEndian<quint32>(raw + 16);
        if (offset > quint64(size) || quint64(size) - offset < oldSize) return Result::NotAForge;

        quint64 newOffset = offset;
        if (quint64(data.size()) <= oldSize) {
            inPlace.append({qint64(offset), data + QByteArray(oldSize - data.size(), '\0')});
        } else {
            inPlace.append({qint64(offset), QByteArray(oldSize, '\0')});
            appends.append({end, data});
            newOffset = quint64(end);
            end += data.size();
        }
        inPlace.append({entry, detail::le<quint64>(newOffset)});
        inPlace.append({entry + 16, detail::le<quint32>(quint32(data.size()))});
    }
    if (inPlace.isEmpty()) return Result::NoFiles;

    // Appends first: until the table changes they are unreferenced bytes past
    // the old end, so an interruption there leaves a working archive.
    for (const Write& w : appends)
        if (!detail::writeAt(forge, w.at, w.bytes)) return Result::IoError;
    for (const Write& w : inPlace)
        if (!detail::writeAt(forge, w.at, w.bytes)) return Result::IoError;
    return Result::Done;
}

} // namespace makine::forgereimport
