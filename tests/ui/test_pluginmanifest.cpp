// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginmanifest.h"

#include <makine/plugin/abi.h>

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>

#include <optional>

namespace {

QJsonObject validManifest()
{
    return QJsonObject{
        {"schema", 2},
        {"id", "org.makineceviri.live"},
        {"name", "Makine Live"},
        {"version", "1.2.0"},
        {"publisher", "Makine Çeviri"},
        {"abi", 2},
        {"minLauncher", "0.2.0"},
        {"entry", "bin/makine-live.dll"},
        {"capabilities", QJsonArray{QJsonObject{{"id", "network"}, {"reason", "Çeviri servisine bağlanır"}}}},
        {"contributes", QJsonObject{
            {"settings", QJsonArray{
                QJsonObject{{"key", "provider"}, {"type", "select"}, {"label", "Servis"},
                            {"options", QJsonArray{"deepl", QJsonObject{{"value", "google"}, {"label", "Google"}}}},
                            {"default", "deepl"}},
                QJsonObject{{"key", "apiKey"}, {"type", "secret"}, {"label", "API anahtarı"}},
                QJsonObject{{"key", "interval"}, {"type", "number"}, {"label", "Aralık"},
                            {"min", 100}, {"max", 5000}, {"default", 500}},
            }},
            {"actions", QJsonArray{QJsonObject{{"id", "start"}, {"label", "Başlat"}}}},
            {"events", QJsonArray{"game.started", "settings.changed"}},
        }},
    };
}

std::optional<PluginManifest> parse(const QJsonObject& o, QString* error = nullptr)
{
    return PluginManifest::parse(QJsonDocument(o).toJson(), error);
}

// Parses `o` with `key` replaced by `value` and expects a rejection.
void expectRejected(QJsonObject o, const QString& key, const QJsonValue& value)
{
    o.insert(key, value);
    QString error;
    EXPECT_FALSE(parse(o, &error).has_value()) << key.toStdString();
    EXPECT_FALSE(error.isEmpty());
}

QJsonObject withSetting(const QJsonObject& setting)
{
    QJsonObject o = validManifest();
    o.insert("contributes", QJsonObject{{"settings", QJsonArray{setting}}});
    return o;
}

} // namespace

TEST(PluginManifest, ParsesAValidManifest)
{
    QString error;
    const auto m = parse(validManifest(), &error);
    ASSERT_TRUE(m.has_value()) << error.toStdString();
    EXPECT_EQ(m->id, QStringLiteral("org.makineceviri.live"));
    EXPECT_EQ(m->entry, QStringLiteral("bin/makine-live.dll"));
    EXPECT_TRUE(m->hasCapability("network"));
    EXPECT_FALSE(m->hasCapability("process-inject"));
    ASSERT_EQ(m->settings.size(), 3);
    // Plain string options are normalised to {value, label}.
    const QJsonArray options = m->setting("provider").value("options").toArray();
    EXPECT_EQ(options[0].toObject().value("value").toString(), QStringLiteral("deepl"));
    EXPECT_EQ(options[1].toObject().value("label").toString(), QStringLiteral("Google"));
    ASSERT_EQ(m->actions.size(), 1);
    EXPECT_EQ(m->actions[0].method, QStringLiteral("start"));   // defaults to the id
    EXPECT_EQ(m->events, (QStringList{"game.started", "settings.changed"}));
}

TEST(PluginManifest, OlderAbisStaySupported)
{
    for (unsigned abi = MAKINE_PLUGIN_ABI_MIN_VERSION; abi <= MAKINE_PLUGIN_ABI_VERSION; ++abi)
        EXPECT_TRUE(pluginmanifest::isSupportedAbi(static_cast<int>(abi))) << abi;
    EXPECT_FALSE(pluginmanifest::isSupportedAbi(static_cast<int>(MAKINE_PLUGIN_ABI_MIN_VERSION) - 1));
    EXPECT_FALSE(pluginmanifest::isSupportedAbi(static_cast<int>(MAKINE_PLUGIN_ABI_VERSION) + 1));
}

TEST(PluginManifest, RejectsWrongSchemaAbiAndIdentity)
{
    const QJsonObject o = validManifest();
    expectRejected(o, "schema", 1);
    expectRejected(o, "abi", 1);
    expectRejected(o, "abi", static_cast<int>(MAKINE_PLUGIN_ABI_VERSION) + 1);   // built for a newer launcher
    expectRejected(o, "id", "Live");
    expectRejected(o, "id", "org.makine/../evil");
    expectRejected(o, "version", "1.2");
    expectRejected(o, "minLauncher", "latest");
    expectRejected(o, "publisher", "");
    expectRejected(o, "name", "");
}

TEST(PluginManifest, EntryMustStayInsideThePluginFolder)
{
    const QJsonObject o = validManifest();
    expectRejected(o, "entry", "../outside.dll");
    expectRejected(o, "entry", "bin/../../outside.dll");
    expectRejected(o, "entry", "C:/Windows/evil.dll");
    expectRejected(o, "entry", "/abs.dll");
    expectRejected(o, "entry", "plugin.exe");
    expectRejected(o, "entry", "");
}

TEST(PluginManifest, CapabilitiesNeedKnownIdsAndReasons)
{
    const QJsonObject o = validManifest();
    expectRejected(o, "capabilities", QJsonArray{QJsonObject{{"id", "kernel"}, {"reason", "x"}}});
    expectRejected(o, "capabilities", QJsonArray{QJsonObject{{"id", "network"}}});
    expectRejected(o, "capabilities", QJsonArray{QJsonObject{{"id", "network"}, {"reason", "a"}},
                                                 QJsonObject{{"id", "network"}, {"reason", "b"}}});
}

TEST(PluginManifest, SettingsAreTypeChecked)
{
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "color"}, {"label", "L"}})));
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "text"}})));                       // no label
    EXPECT_FALSE(parse(withSetting({{"key", "9k"}, {"type", "text"}, {"label", "L"}})));      // bad key
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "secret"}, {"label", "L"}, {"default", "x"}})));
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "select"}, {"label", "L"}})));     // no options
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "select"}, {"label", "L"},
                                    {"options", QJsonArray{"a"}}, {"default", "b"}})));
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "number"}, {"label", "L"},
                                    {"min", 5}, {"max", 1}})));
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "toggle"}, {"label", "L"}, {"default", "yes"}})));
    EXPECT_FALSE(parse(withSetting({{"key", "k"}, {"type", "path"}, {"label", "L"}, {"mode", "drive"}})));
    EXPECT_TRUE(parse(withSetting({{"key", "k"}, {"type", "path"}, {"label", "L"}})));

    QJsonObject twice = validManifest();
    twice.insert("contributes", QJsonObject{{"settings", QJsonArray{
        QJsonObject{{"key", "k"}, {"type", "text"}, {"label", "A"}},
        QJsonObject{{"key", "k"}, {"type", "text"}, {"label", "B"}}}}});
    EXPECT_FALSE(parse(twice));
}

TEST(PluginManifest, UnknownEventsAreRejected)
{
    QJsonObject o = validManifest();
    o.insert("contributes", QJsonObject{{"events", QJsonArray{"keyboard.any"}}});
    EXPECT_FALSE(parse(o));
}

TEST(PluginManifest, SettingValues)
{
    const auto m = parse(validManifest());
    ASSERT_TRUE(m);
    EXPECT_TRUE(pluginmanifest::isValidSettingValue(m->setting("provider"), QJsonValue("google")));
    EXPECT_FALSE(pluginmanifest::isValidSettingValue(m->setting("provider"), QJsonValue("bing")));
    EXPECT_TRUE(pluginmanifest::isValidSettingValue(m->setting("interval"), QJsonValue(100)));
    EXPECT_FALSE(pluginmanifest::isValidSettingValue(m->setting("interval"), QJsonValue(99)));
    EXPECT_FALSE(pluginmanifest::isValidSettingValue(m->setting("interval"), QJsonValue("500")));
    EXPECT_TRUE(pluginmanifest::isValidSettingValue(m->setting("apiKey"), QJsonValue("sk-123")));
}

TEST(PluginManifest, VersionOrder)
{
    using pluginmanifest::compareVersions;
    EXPECT_GT(compareVersions("0.2.0", "0.1.5-beta"), 0);
    EXPECT_LT(compareVersions("0.2.0-beta", "0.2.0"), 0);
    EXPECT_GT(compareVersions("1.10.0", "1.9.9"), 0);
    EXPECT_EQ(compareVersions("1.0.0", "1.0.0"), 0);
    EXPECT_EQ(compareVersions("1.0", "1.0.0"), 0);
}
