// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// The OyunBul fingerprint, judged here as OyunBul judges it.
//
// kGameBin below is a game.bin OyunBul.exe (OyunBul.cs, 2026-10-25) wrote for
// the tree makeGame() builds: same file names, sizes and bytes. A folder that
// tree reproduces must come out exactly as OyunBul's own "dogrula" says — the
// partial hash, the 60/80 % rules, the alternative exe, the guessed one. If
// this file and OyunBul ever disagree, a translator's game.bin would say "TAM"
// on their PC and "sürüm farklı" in the launcher.

#include "gameprint.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace makine::gameprint;

namespace {

// OyunBul.exe ornekle "<tmp>\Ölçü Oyunu" over makeGame()'s tree.
const char kGameBin[] =
    "T1lVTkJVTDENw5Zsw6fDvCBPeXVudQ3DlmzDp8O8IE95dW51g4b02Ccy3wgJAAAAXnQKAAAAAAAACE95dW4uZXhl"
    "4JMEAAAAAACtffKImGi9OwVoURZ8cSFTAQAAAA1PeXVuX3BsdXMuZXhl6AMAAAAAAABMkJG2aiY5yxcZ1GaaISgV"
    "BQAAABJsZXZlbHNcw5Zsw6fDvC5kYXRwEQEAAAAAAI7/e74pMI2Mjueqd221dP0MZGF0YVxtaWQucGFroIYBAAAA"
    "AAA+T2GbXRDCaE5U1i/sJ1SkDmRhdGFcc21hbGwuZGF0iBMAAAAAAADvQDtoLDzkS7luko7w3hAXDGRhdGFcYmln"
    "LnBha0ANAwAAAAAAgT07u5YzkyY11p0ClYe3wBFkYXRhXHN1Ylx0aW55LmJpbmQAAAAAAAAAPDu3Sr1zKLh+z0AG"
    "uNu9XryEhxCNA1eLlVuQC9rLSZPIpdzdfrHCt8WjDu39E/7L";

QByteArray gameBin() { return QByteArray::fromBase64(QByteArray(kGameBin)); }

QByteArray pattern(qint64 size, int seed)
{
    QByteArray b(size, Qt::Uninitialized);
    for (qint64 i = 0; i < size; ++i) b[i] = char((i * 31 + seed) & 0xFF);
    return b;
}

class GamePrintTest : public ::testing::Test {
protected:
    QTemporaryDir tmp;

    QString path(const QString& rel) const { return QDir::cleanPath(tmp.path() + '/' + rel); }

    void write(const QString& rel, const QByteArray& bytes) const
    {
        const QString full = path(rel);
        QDir().mkpath(QFileInfo(full).absolutePath());
        QFile f(full);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(bytes);
    }

    // The tree OyunBul fingerprinted, under `at`.
    QString makeGame(const QString& at) const
    {
        const struct { const char* rel; qint64 size; int seed; } files[] = {
            {"Oyun.exe", 300000, 1},      {"Oyun_plus.exe", 1000, 2},
            {"data/big.pak", 200000, 3},  {"data/mid.pak", 100000, 4},
            {"data/small.dat", 5000, 5},  {"data/sub/tiny.bin", 100, 6},
            {"levels/\xC3\x96l\xC3\xA7\xC3\xBC.dat", 70000, 7},
            {"config.ini", 50, 8},        {"steam_api64.dll", 9000, 9},
        };
        for (const auto& f : files)
            write(at + '/' + QString::fromUtf8(f.rel), pattern(f.size, f.seed));
        return path(at);
    }

    // Same size, other bytes: what a patch or an update does to an archive.
    void alter(const QString& rel, qint64 at = 0) const
    {
        QFile f(path(rel));
        ASSERT_TRUE(f.open(QIODevice::ReadWrite));
        f.seek(at);
        char c = 0;
        f.getChar(&c);
        f.seek(at);
        f.putChar(char(c ^ 0x5A));
    }

    Print print() const
    {
        const auto p = fromGameBin(gameBin());
        EXPECT_TRUE(p.has_value());
        return p.value_or(Print{});
    }
};

const QString kGame = QString::fromUtf8("\xC3\x96l\xC3\xA7\xC3\xBC Oyunu");   // Ölçü Oyunu

} // namespace

TEST_F(GamePrintTest, ReadsWhatOyunBulWrote)
{
    const Print p = print();
    EXPECT_EQ(p.name, kGame);
    EXPECT_EQ(p.rootName, kGame);
    EXPECT_EQ(p.exe.rel, QStringLiteral("Oyun.exe"));
    EXPECT_EQ(p.exe.size, 300000);
    ASSERT_EQ(p.altExes.size(), 1);
    EXPECT_EQ(p.altExes[0].rel, QStringLiteral("Oyun_plus.exe"));
    // Volatile (.ini) and store files (steam_api) are never samples; paths come "/"-separated.
    ASSERT_EQ(p.samples.size(), 5);
    QStringList rels;
    for (const Entry& e : p.samples) rels << e.rel;
    EXPECT_TRUE(rels.contains(QString::fromUtf8("levels/\xC3\x96l\xC3\xA7\xC3\xBC.dat")));
    EXPECT_TRUE(rels.contains(QStringLiteral("data/sub/tiny.bin")));
}

TEST_F(GamePrintTest, DamagedOrForeignFileIsNoPrint)
{
    QByteArray bin = gameBin();
    bin[40] = char(bin[40] ^ 1);
    EXPECT_FALSE(fromGameBin(bin).has_value());
    EXPECT_FALSE(fromGameBin(QByteArray("OYUNBUL1")).has_value());
    EXPECT_FALSE(fromGameBin(QByteArray(400, 'x')).has_value());
}

// The partial hash over files below, inside and above 128 KB: identical to the
// patch maker's copy means identical here.
TEST_F(GamePrintTest, SameCopyIsExact)
{
    const QString root = makeGame(kGame);
    const Verdict v = verify(print(), root);
    EXPECT_TRUE(v.match);
    EXPECT_TRUE(v.exact);
    EXPECT_TRUE(v.exeSame);
    EXPECT_EQ(v.hashHits, 5);
    EXPECT_EQ(v.exeRel, QStringLiteral("Oyun.exe"));
    EXPECT_EQ(v.exeDir(), root);
}

TEST_F(GamePrintTest, ChangedFilesMakeAnotherVersion)
{
    const QString root = makeGame(kGame);
    // One of five changed: 4/5 still reaches 80 %.
    alter(kGame + QStringLiteral("/data/big.pak"), 199999);
    EXPECT_TRUE(verify(print(), root).exact);
    // Two of five: the game, but not the version the patch was made for.
    alter(kGame + QStringLiteral("/data/mid.pak"), 70000);
    const Verdict v = verify(print(), root);
    EXPECT_TRUE(v.match);
    EXPECT_FALSE(v.exact);
    EXPECT_EQ(v.pathHits, 5);
    EXPECT_EQ(v.hashHits, 3);
}

TEST_F(GamePrintTest, ChangedExeIsAnotherVersion)
{
    const QString root = makeGame(kGame);
    // Only the first and last 64 KB are read: a change in between goes unseen.
    alter(kGame + QStringLiteral("/Oyun.exe"), 150000);
    EXPECT_TRUE(verify(print(), root).exact);
    alter(kGame + QStringLiteral("/Oyun.exe"), 299000);
    const Verdict v = verify(print(), root);
    EXPECT_TRUE(v.match);
    EXPECT_FALSE(v.exact);
    EXPECT_TRUE(v.exeFound);
    EXPECT_FALSE(v.exeSame);
}

// Another store's edition: the main exe is not there, the one beside it is.
TEST_F(GamePrintTest, AlternativeExeStandsIn)
{
    const QString root = makeGame(kGame);
    ASSERT_TRUE(QFile::remove(root + QStringLiteral("/Oyun.exe")));
    const Verdict v = verify(print(), root);
    EXPECT_TRUE(v.exeAlt);
    EXPECT_TRUE(v.exact);
    EXPECT_EQ(v.exeRel, QStringLiteral("Oyun_plus.exe"));
}

TEST_F(GamePrintTest, UnknownExeIsGuessedFromTheData)
{
    const QString root = makeGame(kGame);
    ASSERT_TRUE(QFile::remove(root + QStringLiteral("/Oyun.exe")));
    ASSERT_TRUE(QFile::remove(root + QStringLiteral("/Oyun_plus.exe")));
    write(kGame + QStringLiteral("/CrashReporter.exe"), pattern(500000, 10));
    // Only a helper exe left: like the game, but nothing to run.
    Verdict v = verify(print(), root);
    EXPECT_FALSE(v.match);
    EXPECT_TRUE(v.rootLike);

    write(kGame + QStringLiteral("/OyunGOG.exe"), pattern(2000, 11));
    v = verify(print(), root);
    EXPECT_TRUE(v.match);
    EXPECT_TRUE(v.exeGuessed);
    EXPECT_FALSE(v.exact);
    EXPECT_EQ(v.exeRel, QStringLiteral("OyunGOG.exe"));
}

TEST_F(GamePrintTest, OtherFolderDoesNotMatch)
{
    makeGame(kGame);
    write(QStringLiteral("Hades/Hades.exe"), pattern(1000, 1));
    write(QStringLiteral("Hades/data/big.pak"), pattern(1000, 3));
    const Verdict v = verify(print(), path(QStringLiteral("Hades")));
    EXPECT_FALSE(v.match);
    EXPECT_FALSE(v.exact);
}

// A repack unpacked into "<Game>/<Game>", and a folder opened inside the game:
// the root the print's paths start from is found around either.
TEST_F(GamePrintTest, FindsTheRootAround)
{
    const QString inner = makeGame(kGame + '/' + kGame);
    const QList<Print> prints{print()};
    EXPECT_FALSE(verify(prints[0], path(kGame)).match);
    EXPECT_EQ(findRoot(prints, path(kGame)), inner);
    EXPECT_EQ(findRoot(prints, inner + QStringLiteral("/data")), inner);
    EXPECT_TRUE(findRoot(prints, path(QStringLiteral("nowhere"))).isEmpty());
}

TEST_F(GamePrintTest, FindsGameBinsInAPackage)
{
    write(QStringLiteral("pkg/game.bin"), gameBin());
    write(QStringLiteral("pkg/v1.0/game.bin"), gameBin());
    write(QStringLiteral("pkg/a/b/c/d/game.bin"), gameBin());
    const QStringList found = findGameBins(path(QStringLiteral("pkg")));
    EXPECT_EQ(found.size(), 2);
    EXPECT_TRUE(fromGameBinFile(found.value(0)).has_value());
}

// game.path in OyunBul's three lines, for a .bat that reads it.
TEST(GamePrintPathFile, ThreeLines)
{
    EXPECT_EQ(QString::fromUtf8(pathFile(QStringLiteral("D:/Oyunlar/ELDEN RING"),
                                         QStringLiteral("Game/eldenring.exe"))),
              QStringLiteral("D:\\Oyunlar\\ELDEN RING\\Game\r\nD:\\Oyunlar\\ELDEN RING\r\n"
                             "D:\\Oyunlar\\ELDEN RING\\Game\\eldenring.exe\r\n"));
}

TEST(GamePrintRules, HelperAndLauncherExes)
{
    EXPECT_EQ(exePenalty(QStringLiteral("CrashReportClient.exe")), 2);
    EXPECT_EQ(exePenalty(QStringLiteral("unins000.exe")), 2);
    EXPECT_EQ(exePenalty(QStringLiteral("PlayRDR2Launcher.exe")), 1);
    EXPECT_EQ(exePenalty(QStringLiteral("RDR2.exe")), 0);
    EXPECT_EQ(need(10, 0.6), 6);
    EXPECT_EQ(need(10, 0.8), 8);
    EXPECT_EQ(need(5, 0.8), 4);
}
