// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The "forgeReimport" install step, against the tool it replaces.
//
// The expected archives are not derived from forgereimport.h: each digest is
// what QuickBMS 0.11.0 itself wrote (`qb.exe -w -r -r acv.bms <forge> <dir>`,
// the legacy installer's command, measured 2026-10-08) for the same input
// archive and the same reimport files. A Valhalla or Mirage .forge is
// gigabytes the user cannot get back without a re-download; matching the
// legacy tool byte for byte is the bar.

#include <gtest/gtest.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>

#include "forgereimport.h"

using namespace makine::forgereimport;

namespace {

template <typename T>
QByteArray le(T v)
{
    QByteArray b(sizeof(T), '\0');
    qToLittleEndian(v, b.data());
    return b;
}

// The synthetic archive the QuickBMS runs used: header offset 0x40, file
// table at 0x100, data from 0x400, entry i filled with the byte '0'+i.
QByteArray makeForge(const QList<quint32>& sizes, const QByteArray& tail)
{
    QByteArray buf = QByteArray("scimitar\0\x1b\0\0\0", 13) + le<quint64>(0x40);
    buf += QByteArray(0x40 - buf.size(), '\xEE');
    buf += QByteArray(0x2C, '\xAB') + le<quint32>(quint32(sizes.size()))
         + QByteArray(4, '\xCD') + le<quint64>(0x100);
    buf += QByteArray(0x100 - buf.size(), '\0');
    quint64 offset = 0x400;
    QByteArray blobs;
    for (int i = 0; i < sizes.size(); ++i) {
        buf += le<quint64>(offset) + le<quint64>(0x1000 + i) + le<quint32>(sizes[i]);
        blobs += QByteArray(sizes[i], char(0x30 + i));
        offset += sizes[i];
    }
    buf += QByteArray(0x400 - buf.size(), '\0');
    return buf + blobs + tail;
}

struct Entry { quint64 offset; quint64 assetId; quint32 size; };

Entry entry(const QByteArray& forge, int i)
{
    const char* p = forge.constData() + 0x100 + 20 * i;
    return {qFromLittleEndian<quint64>(p), qFromLittleEndian<quint64>(p + 8),
            qFromLittleEndian<quint32>(p + 16)};
}

Result reimport(QByteArray& forge, const QMap<quint32, QByteArray>& files)
{
    QBuffer buffer(&forge);
    buffer.open(QIODevice::ReadWrite);
    return apply(buffer, files);
}

QByteArray sha256(const QByteArray& b)
{
    return QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex();
}

} // namespace

TEST(ForgeReimport, MatchesQuickBmsWhenFilesShrinkStayAndGrow)
{
    QByteArray forge = makeForge({64, 96, 128, 32}, QByteArray("TAILDATA").repeated(4));
    ASSERT_EQ(forge.size(), 1376);
    ASSERT_EQ(reimport(forge, {{0, QByteArray(40, 'S')},
                               {1, QByteArray(96, 'E')},
                               {2, QByteArray(200, 'B')}}),
              Result::Done);

    EXPECT_EQ(sha256(forge), "59f50e859bbfa78cf97f16e763b41801ea5665b4b96882d48d185b26cce8266a");
    EXPECT_EQ(forge.size(), 1576);
    const Entry e0 = entry(forge, 0), e1 = entry(forge, 1), e2 = entry(forge, 2);
    EXPECT_EQ(e0.offset, 1024u);  EXPECT_EQ(e0.size, 40u);    // shrunk in place
    EXPECT_EQ(e1.offset, 1088u);  EXPECT_EQ(e1.size, 96u);    // same size, in place
    EXPECT_EQ(e2.offset, 1376u);  EXPECT_EQ(e2.size, 200u);   // grew: appended at the old end
    EXPECT_EQ(e2.assetId, 0x1002u);
    EXPECT_EQ(forge.mid(1024 + 40, 24), QByteArray(24, '\0'));    // rest of the shrunk slot
    EXPECT_EQ(forge.mid(1184, 128), QByteArray(128, '\0'));       // the slot that grew out
}

TEST(ForgeReimport, MatchesQuickBmsOnEmptyAndOutOfRangeFiles)
{
    QByteArray forge = makeForge({64, 96, 128, 32, 16}, QByteArray("XYZ"));
    ASSERT_EQ(reimport(forge, {{1, QByteArray(150, 'P')},
                               {3, QByteArray(33, 'Q')},
                               {4, QByteArray()},
                               {9, QByteArray(10, 'Z')}}),     // the archive has 5 entries
              Result::Done);

    EXPECT_EQ(sha256(forge), "e57802db60c223366152a57cfbd822d936546b32f4734f833d7d86d51df370c1");
    EXPECT_EQ(forge.size(), 1546);
    EXPECT_EQ(entry(forge, 1).offset, 1363u);   // appends follow index order
    EXPECT_EQ(entry(forge, 3).offset, 1513u);
    EXPECT_EQ(entry(forge, 4).offset, 1344u);
    EXPECT_EQ(entry(forge, 4).size, 0u);
    EXPECT_EQ(forge.mid(1344, 16), QByteArray(16, '\0'));
}

TEST(ForgeReimport, MatchesQuickBmsWhenTheLastSlotGrows)
{
    QByteArray forge = makeForge({64, 96, 128, 32}, QByteArray());
    ASSERT_EQ(reimport(forge, {{3, QByteArray(70, 'L')}, {0, QByteArray(80, 'Z')}}),
              Result::Done);
    EXPECT_EQ(sha256(forge), "c99b3daffa998a87f26ce4816a876446c3498c71f46cf29b69f9df13fe816266");
    EXPECT_EQ(entry(forge, 0).offset, 1344u);
    EXPECT_EQ(entry(forge, 3).offset, 1424u);
}

TEST(ForgeReimport, ASecondRunChangesNothing)
{
    // A re-install or an update runs the step over its own result.
    QByteArray forge = makeForge({64, 96, 128, 32}, QByteArray("TAILDATA").repeated(4));
    const QMap<quint32, QByteArray> files = {{0, QByteArray(40, 'S')},
                                             {2, QByteArray(200, 'B')}};
    ASSERT_EQ(reimport(forge, files), Result::Done);
    const QByteArray once = forge;
    ASSERT_EQ(reimport(forge, files), Result::Done);
    EXPECT_EQ(forge, once);
}

TEST(ForgeReimport, RefusesWhatIsNotThisArchiveWithoutWriting)
{
    const QByteArray good = makeForge({64, 96}, QByteArray());
    const QMap<quint32, QByteArray> files = {{0, QByteArray(8, 'S')}};

    QByteArray notForge = good;
    notForge[0] = 'S';
    EXPECT_EQ(reimport(notForge, files), Result::NotAForge);

    QByteArray hugeCount = good;   // a file count the table cannot hold
    hugeCount.replace(0x40 + 0x2C, 4, le<quint32>(0x10000000));
    const QByteArray before = hugeCount;
    EXPECT_EQ(reimport(hugeCount, files), Result::NotAForge);
    EXPECT_EQ(hugeCount, before);

    QByteArray slotOutside = good;  // entry 1 claims bytes past the end
    slotOutside.replace(0x100 + 20, 8, le<quint64>(0x10000));
    const QByteArray before2 = slotOutside;
    EXPECT_EQ(reimport(slotOutside, {{0, QByteArray(8, 'S')}, {1, QByteArray(8, 'T')}}),
              Result::NotAForge);
    EXPECT_EQ(slotOutside, before2);   // entry 0 was valid, and still untouched

    QByteArray noneOfOurs = good;
    EXPECT_EQ(reimport(noneOfOurs, {{7, QByteArray(8, 'S')}}), Result::NoFiles);
    EXPECT_EQ(noneOfOurs, good);
}

TEST(ForgeReimport, FolderFilesAreFoundByTheirDecimalIndex)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto put = [&dir](const QString& name, const QByteArray& data) {
        QFile f(dir.filePath(name));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(data);
    };
    put(QStringLiteral("5714.acv"), "a");
    put(QStringLiteral("12.acv"), "b");
    put(QStringLiteral("0013.acv"), "c");    // QuickBMS looks for "13.acv"
    put(QStringLiteral("x.acv"), "d");
    put(QStringLiteral("14.txt"), "e");

    QMap<quint32, QByteArray> files;
    ASSERT_TRUE(loadFolder(dir.path(), &files));
    EXPECT_EQ(files.keys(), (QList<quint32>{12, 5714}));
    EXPECT_EQ(files.value(5714), QByteArray("a"));
}
