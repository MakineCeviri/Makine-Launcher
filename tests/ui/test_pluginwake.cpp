// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#include "pluginwake.h"

#include <gtest/gtest.h>

using pluginwake::Reason;
using pluginwake::startsHost;

TEST(PluginWake, NothingStartsAtLaunch)
{
    const QStringList events{"game.started", "game.stopped"};
    const QJsonObject autoStart{{"autoStart", true}};
    for (bool bundled : {true, false})
        EXPECT_FALSE(startsHost(Reason::Launch, bundled, events, {}, autoStart)) << bundled;
}

TEST(PluginWake, FirstUseStartsIt)
{
    EXPECT_TRUE(startsHost(Reason::Screen, true, {}));
    EXPECT_TRUE(startsHost(Reason::Action, true, {}));
    EXPECT_TRUE(startsHost(Reason::Action, false, {}));
}

TEST(PluginWake, BundledWakesOnGameStartOnlyWithAutoStart)
{
    const QStringList events{"game.started", "game.stopped"};
    EXPECT_FALSE(startsHost(Reason::Event, true, events, "game.started"));
    EXPECT_FALSE(startsHost(Reason::Event, true, events, "game.started", {{"autoStart", false}}));
    EXPECT_FALSE(startsHost(Reason::Event, true, events, "game.started", {{"autoStart", "true"}}));
    EXPECT_TRUE(startsHost(Reason::Event, true, events, "game.started", {{"autoStart", true}}));
    EXPECT_FALSE(startsHost(Reason::Event, true, events, "game.stopped", {{"autoStart", true}}));
    // Not declared: never, whatever the setting says.
    EXPECT_FALSE(startsHost(Reason::Event, true, {"game.stopped"}, "game.started", {{"autoStart", true}}));
}

TEST(PluginWake, OtherPluginsWakeOnEventsTheyDeclared)
{
    const QStringList events{"translation.installed"};
    EXPECT_TRUE(startsHost(Reason::Event, false, events, "translation.installed"));
    EXPECT_FALSE(startsHost(Reason::Event, false, events, "game.started"));
}
