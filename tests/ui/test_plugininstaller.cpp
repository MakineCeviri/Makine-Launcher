// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Plugin folders: stage a real .mkplugin, then commit, update, roll back and remove.

#include "plugininstaller.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <zstd.h>

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace {

// Minimal USTAR writer: regular files only, names under 100 bytes.
QByteArray tar(const std::vector<std::pair<QByteArray, QByteArray>>& files)
{
    QByteArray out;
    for (const auto& [name, data] : files) {
        char h[512] = {};
        std::memcpy(h, name.constData(), name.size());
        std::memcpy(h + 100, "0000644", 8);
        std::memcpy(h + 108, "0000000", 8);
        std::memcpy(h + 116, "0000000", 8);
        std::snprintf(h + 124, 12, "%011llo", static_cast<unsigned long long>(data.size()));
        std::memcpy(h + 136, "00000000000", 12);
        h[156] = '0';
        std::memcpy(h + 257, "ustar", 6);
        std::memcpy(h + 263, "00", 2);
        std::memset(h + 148, ' ', 8);
        unsigned sum = 0;
        for (unsigned char c : h) sum += c;
        std::snprintf(h + 148, 8, "%06o", sum);
        out.append(h, 512);
        out.append(data);
        out.append(QByteArray((512 - data.size() % 512) % 512, '\0'));
    }
    out.append(QByteArray(1024, '\0'));
    return out;
}

QByteArray mkplugin(const QJsonObject& manifest, bool withDll = true)
{
    std::vector<std::pair<QByteArray, QByteArray>> files{
        {"manifest.json", QJsonDocument(manifest).toJson()}};
    if (withDll) files.push_back({"bin/test.dll", QByteArray("MZ fake dll")});
    const QByteArray raw = tar(files);
    QByteArray compressed(static_cast<qsizetype>(ZSTD_compressBound(raw.size())), '\0');
    const size_t n = ZSTD_compress(compressed.data(), compressed.size(), raw.constData(), raw.size(), 3);
    compressed.resize(static_cast<qsizetype>(n));
    return QByteArray("MKPK\x02", 5) + compressed;
}

QJsonObject manifest(const QString& version)
{
    return {{"schema", 2}, {"id", "org.example.test"}, {"name", "Test"}, {"version", version},
            {"publisher", "Example"}, {"abi", 2}, {"entry", "bin/test.dll"}};
}

QString versionAt(const QString& dir)
{
    QFile f(dir + "/manifest.json");
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object().value("version").toString();
}

} // namespace

TEST(PluginInstaller, InstallUpdateRollbackRemove)
{
    QTemporaryDir root;
    ASSERT_TRUE(root.isValid());
    const QString plugins = root.path() + "/plugins";
    const QString target = plugins + "/org.example.test";
    QString error;

    auto v1 = plugininstaller::stage(mkplugin(manifest("1.0.0")), plugins + "/.staging", &error);
    ASSERT_TRUE(v1) << error.toStdString();
    ASSERT_TRUE(plugininstaller::commit(*v1, plugins, &error)) << error.toStdString();
    EXPECT_EQ(versionAt(target), QStringLiteral("1.0.0"));
    EXPECT_TRUE(QFile::exists(target + "/bin/test.dll"));

    auto v2 = plugininstaller::stage(mkplugin(manifest("2.0.0")), plugins + "/.staging", &error);
    ASSERT_TRUE(v2);
    ASSERT_TRUE(plugininstaller::commit(*v2, plugins, &error));
    EXPECT_EQ(versionAt(target), QStringLiteral("2.0.0"));
    EXPECT_EQ(versionAt(plugins + "/.previous/org.example.test"), QStringLiteral("1.0.0"));

    const auto found = plugininstaller::scan(plugins);
    ASSERT_EQ(found.size(), 1);   // dot folders are not plugins
    ASSERT_TRUE(found[0].manifest);

    ASSERT_TRUE(plugininstaller::rollback(plugins, "org.example.test"));
    EXPECT_EQ(versionAt(target), QStringLiteral("1.0.0"));
    EXPECT_FALSE(QFile::exists(plugins + "/.previous/org.example.test"));

    ASSERT_TRUE(plugininstaller::remove(plugins, "org.example.test", &error));
    EXPECT_FALSE(QFile::exists(target));
    plugininstaller::cleanUp(plugins);
    EXPECT_TRUE(plugininstaller::scan(plugins).isEmpty());
}

TEST(PluginInstaller, RejectsBrokenPackagesAndLeavesNothingBehind)
{
    QTemporaryDir root;
    const QString staging = root.path() + "/plugins/.staging";
    QString error;

    EXPECT_FALSE(plugininstaller::stage(mkplugin(manifest("1.0.0"), false), staging, &error));
    EXPECT_TRUE(error.contains("entry")) << error.toStdString();

    QJsonObject v1 = manifest("1.0.0");
    v1.insert("schema", 1);
    EXPECT_FALSE(plugininstaller::stage(mkplugin(v1), staging, &error));

    QByteArray encrypted = mkplugin(manifest("1.0.0"));
    encrypted[4] = 1;   // MKPK v1 is the encrypted translation format, never a plugin
    EXPECT_FALSE(plugininstaller::stage(encrypted, staging, &error));
    EXPECT_FALSE(plugininstaller::stage("garbage", staging, &error));

    EXPECT_TRUE(QDir(staging).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

TEST(PluginInstaller, FolderNameMustMatchManifestId)
{
    QTemporaryDir root;
    const QString plugins = root.path() + "/plugins";
    QString error;
    auto staged = plugininstaller::stage(mkplugin(manifest("1.0.0")), plugins + "/.staging", &error);
    ASSERT_TRUE(staged);
    ASSERT_TRUE(QDir().rename(staged->dir, plugins + "/org.example.other"));

    const auto found = plugininstaller::scan(plugins);
    ASSERT_EQ(found.size(), 1);
    EXPECT_FALSE(found[0].manifest);
    EXPECT_FALSE(found[0].error.isEmpty());
}
