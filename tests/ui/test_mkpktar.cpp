// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// mkpk::tar::extract against the name records Python's tarfile writes: PAX
// ('x', the default) and GNU long names ('L'). Before these were read, a
// non-ASCII or long name came out of the ustar field as an ASCII stand-in —
// "T?rk?e Yama" on disk as "T_rk_e Yama", and Sekiro's Japanese .fmg names
// collapsing into one another.

#include "mkpkformat.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace makine::mkpk;

namespace {

void putOctal(char* field, size_t len, size_t value)
{
    std::snprintf(field, len, "%0*zo", static_cast<int>(len - 1), value);
}

// One header block plus its data, padded to 512 bytes.
void addEntry(std::vector<uint8_t>& tar, const std::string& ustarName, char type,
              const std::string& data)
{
    char h[512] = {};
    std::memcpy(h, ustarName.data(), std::min<size_t>(ustarName.size(), 100));
    putOctal(h + 100, 8, 0644);
    putOctal(h + 124, 12, data.size());
    h[156] = type;
    std::memcpy(h + 257, "ustar", 5);
    tar.insert(tar.end(), h, h + 512);
    tar.insert(tar.end(), data.begin(), data.end());
    tar.resize(tar.size() + (512 - data.size() % 512) % 512, 0);
}

std::string paxRecord(const std::string& key, const std::string& value)
{
    // "<len> <key>=<value>\n", where len counts the whole record.
    const std::string body = " " + key + "=" + value + "\n";
    size_t len = body.size() + 1;
    while (std::to_string(len).size() + body.size() != len) ++len;
    return std::to_string(len) + body;
}

// A UTF-8 relative path, without the deprecated fs::u8path.
fs::path u8(const std::string& s)
{
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string readFile(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

class MkpkTar : public ::testing::Test {
protected:
    void SetUp() override
    {
        static std::atomic<int> n{0};
        dir = fs::temp_directory_path() / ("mkpktar-" + std::to_string(++n));
        fs::remove_all(dir);
    }
    void TearDown() override { fs::remove_all(dir); }

    int extract(std::vector<uint8_t> tar)
    {
        tar.resize(tar.size() + 1024, 0);  // end-of-archive marker
        MkpkError err("");
        return tar::extract(tar.data(), tar.size(), dir, &err);
    }

    fs::path dir;
};

} // namespace

TEST_F(MkpkTar, PaxPathReplacesTheAsciiStandIn)
{
    std::vector<uint8_t> tar;
    addEntry(tar, "././@PaxHeader", 'x', paxRecord("path", "Türkçe Yama/a.txt"));
    addEntry(tar, "T?rk?e Yama/a.txt", '0', "tr");

    EXPECT_EQ(extract(tar), 1);
    EXPECT_EQ(readFile(dir / u8("Türkçe Yama/a.txt")), "tr");
    EXPECT_FALSE(fs::exists(dir / "T_rk_e Yama"));
}

TEST_F(MkpkTar, DistinctNamesWithTheSameStandInStayDistinct)
{
    std::vector<uint8_t> tar;
    addEntry(tar, "././@PaxHeader", 'x', paxRecord("path", "msg/会話.fmg"));
    addEntry(tar, "msg/??.fmg", '0', "one");
    addEntry(tar, "././@PaxHeader", 'x', paxRecord("path", "msg/武器.fmg"));
    addEntry(tar, "msg/??.fmg", '0', "two");

    EXPECT_EQ(extract(tar), 2);
    EXPECT_EQ(readFile(dir / u8("msg/会話.fmg")), "one");
    EXPECT_EQ(readFile(dir / u8("msg/武器.fmg")), "two");
}

TEST_F(MkpkTar, GnuLongNameIsNotTruncated)
{
    const std::string longName = "deep/" + std::string(40, 'x') + "/" + std::string(40, 'y')
                               + "/" + std::string(20, 'z') + ".txt";
    ASSERT_GT(longName.size(), 100u);

    std::vector<uint8_t> tar;
    addEntry(tar, "././@LongLink", 'L', longName + '\0');
    addEntry(tar, longName.substr(0, 100), '0', "long");

    EXPECT_EQ(extract(tar), 1);
    EXPECT_EQ(readFile(dir / longName), "long");
}

TEST_F(MkpkTar, SiblingFolderWithTheSamePrefixIsOutside)
{
    // dir is ".../mkpktar-N"; "../mkpktar-NX/a.dll" starts with the same string
    // but is a different folder. A string-prefix check let it through.
    const std::string sibling = dir.filename().string() + "X";
    std::vector<uint8_t> tar;
    addEntry(tar, "../" + sibling + "/a.dll", '0', "evil");

    EXPECT_EQ(extract(tar), -1);
    EXPECT_FALSE(fs::exists(dir.parent_path() / sibling));
}

TEST_F(MkpkTar, NamesThatClimbOrAreAbsoluteAreRefused)
{ // checked by the name alone (no per-entry canonicalisation): every way out is still shut
    for (const std::string& name : {std::string("a/../../escaped.dll"), std::string("/rooted.dll"),
                                    std::string("C:/drive.dll"), std::string("C:drive-relative.dll"),
                                    std::string("..\\backslash.dll")}) {
        std::vector<uint8_t> tar;
        addEntry(tar, name, '0', "evil");
        EXPECT_EQ(extract(tar), -1) << name;
    }
    std::vector<uint8_t> ok;
    addEntry(ok, "Game/./data/../data/file.bin", '0', "fine");
    EXPECT_EQ(extract(ok), 1);
    EXPECT_EQ(readFile(dir / "Game" / "data" / "file.bin"), "fine");
}

TEST_F(MkpkTar, RecordAppliesToTheNextEntryOnly)
{
    std::vector<uint8_t> tar;
    addEntry(tar, "././@PaxHeader", 'x', paxRecord("mtime", "1") + paxRecord("path", "ç/a.txt"));
    addEntry(tar, "?/a.txt", '0', "a");
    addEntry(tar, "plain/b.txt", '0', "b");

    EXPECT_EQ(extract(tar), 2);
    EXPECT_EQ(readFile(dir / u8("ç/a.txt")), "a");
    EXPECT_EQ(readFile(dir / "plain/b.txt"), "b");
}

#ifdef _WIN32
TEST_F(MkpkTar, PathPastMaxPathIsWritten)
{
    // Pillars of Eternity II ships 158-character names; under the packages
    // folder they pass MAX_PATH, which needs the extended path form.
    const std::string longName = std::string(120, 'a') + "/" + std::string(120, 'b') + "/c.txt";

    std::vector<uint8_t> tar;
    addEntry(tar, "././@LongLink", 'L', longName + '\0');
    addEntry(tar, longName.substr(0, 100), '0', "long");

    const fs::path root(LR"(\\?\)" + fs::absolute(dir).make_preferred().native());
    EXPECT_EQ(extract(tar), 1);
    const fs::path file = root / fs::path(longName).make_preferred();
    EXPECT_EQ(readFile(file), "long");

    // Not remove_all: libstdc++'s never returns once a path is past MAX_PATH.
    std::error_code ec;
    fs::remove(file, ec);
    fs::remove(file.parent_path(), ec);
    fs::remove(file.parent_path().parent_path(), ec);
}
#endif
