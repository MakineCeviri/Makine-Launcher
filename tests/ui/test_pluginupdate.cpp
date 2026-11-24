// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

// Verified plugins: no consent, capabilities an update adds, which ones update by
// themselves, and when a running host may be restarted for it.

#include "pluginupdate.h"

#include <gtest/gtest.h>

using pluginupdate::Activity;
using pluginupdate::Candidate;
using pluginupdate::canReplaceNow;
using pluginupdate::granted;
using pluginupdate::shouldAutoUpdate;

TEST(PluginUpdate, VerifiedPluginsNeedNoConsent)
{
    const QStringList declared{"screen-capture", "network", "overlay"};
    // Installed from the catalogue (or bundled): granted by origin, nothing stored needed.
    EXPECT_EQ(granted(true, declared, {}), declared);
    // Installed from a file: only what the user agreed to.
    EXPECT_TRUE(granted(false, declared, {}).isEmpty());
    EXPECT_EQ(granted(false, declared, {"network"}), QStringList{"network"});
}

TEST(PluginUpdate, ACapabilityAnUpdateAddsIsGrantedOnlyWhenVerified)
{
    const QStringList stored{"screen-capture", "network", "overlay"};   // granted for 0.4.0
    const QStringList now{"screen-capture", "network", "overlay", "game-files-read"};   // 0.5.0
    EXPECT_TRUE(granted(true, now, stored).contains("game-files-read"));
    EXPECT_FALSE(granted(false, now, stored).contains("game-files-read"));   // asks again
}

TEST(PluginUpdate, OnlyVerifiedCompatibleNewerVersionsAutoUpdate)
{
    Candidate c{"0.4.0", true, "0.4.1", true, {}};
    EXPECT_TRUE(shouldAutoUpdate(c));

    Candidate file = c;
    file.verified = false;   // installed from a file: the developer's copy stays
    EXPECT_FALSE(shouldAutoUpdate(file));

    Candidate same = c;
    same.registryVersion = "0.4.0";
    EXPECT_FALSE(shouldAutoUpdate(same));
    Candidate older = c;
    older.registryVersion = "0.3.9";   // the launcher ships a newer bundled copy
    EXPECT_FALSE(shouldAutoUpdate(older));
    Candidate beta = c;
    beta.installedVersion = "0.4.1-beta";   // pre-release -> release is an update
    EXPECT_TRUE(shouldAutoUpdate(beta));

    Candidate incompatible = c;
    incompatible.compatible = false;   // ABI or minLauncher
    EXPECT_FALSE(shouldAutoUpdate(incompatible));

    Candidate notListed = c;
    notListed.registryVersion.clear();
    EXPECT_FALSE(shouldAutoUpdate(notListed));

    Candidate broken = c;
    broken.installedVersion.clear();   // broken manifest: the user reinstalls
    EXPECT_FALSE(shouldAutoUpdate(broken));
}

TEST(PluginUpdate, AVersionThatFailedToStartIsNotRetried)
{
    Candidate c{"0.4.0", true, "0.4.1", true, "0.4.1"};
    EXPECT_FALSE(shouldAutoUpdate(c));
    c.registryVersion = "0.4.2";   // the fix that follows is
    EXPECT_TRUE(shouldAutoUpdate(c));
}

TEST(PluginUpdate, ARunningHostIsReplacedOnlyWhenIdle)
{
    EXPECT_TRUE(canReplaceNow(Activity{}));   // no host: at once
    // No host, whatever else is going on (a game without the plugin, say).
    EXPECT_TRUE(canReplaceNow(Activity{false, true, false, false, false}));

    const Activity idle{true, false, false, false, false};
    EXPECT_TRUE(canReplaceNow(idle));

    Activity game = idle;
    game.gameRunning = true;
    EXPECT_FALSE(canReplaceNow(game));
    Activity overlay = idle;
    overlay.overlayShown = true;
    EXPECT_FALSE(canReplaceNow(overlay));
    Activity waiting = idle;
    waiting.userWaiting = true;
    EXPECT_FALSE(canReplaceNow(waiting));
    Activity busy = idle;
    busy.reportsBusy = true;   // Live's ui.state running: true (reading a console)
    EXPECT_FALSE(canReplaceNow(busy));
}
