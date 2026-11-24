// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include "pluginmanifest.h"

#include <QString>
#include <QStringList>

/**
 * Trust and update rules for verified plugins (PluginManager).
 *
 * Verified = the copy that runs is the one shipped with the launcher, or it was
 * installed from the signed registry (index signature + package SHA-256). The
 * signature is the trust: a verified plugin runs with every capability its
 * manifest declares, including one a later version adds, and never asks for
 * consent. A plugin installed from a file (developer mode) is unverified and
 * runs only with what the user granted.
 *
 * Automatic updates: a verified plugin whose registry entry is newer and
 * compatible is downloaded and checked in the background exactly like a manual
 * update, then put in place when its host is not in use (canReplaceNow).
 *
 * Header-only, no QObject: tests/ui/test_pluginupdate.cpp.
 */
namespace pluginupdate {

/// The capabilities a plugin runs with. `declared`: its manifest's; `stored`: what the user granted.
inline QStringList granted(bool verified, const QStringList& declared, const QStringList& stored)
{
    return verified ? declared : stored;
}

struct Candidate {
    QString installedVersion;   // the copy that runs; empty when its manifest is broken
    bool verified = false;      // runs the bundled copy or came from the signed registry
    QString registryVersion;    // empty: the registry does not list it
    bool compatible = false;    // ABI supported and the launcher is at least minLauncher
    QString failedVersion;      // a version an earlier update could not start (rolled back)
};

/// Whether the launcher installs the registry version by itself.
inline bool shouldAutoUpdate(const Candidate& c)
{
    return c.verified && c.compatible && !c.installedVersion.isEmpty() && !c.registryVersion.isEmpty()
        && c.registryVersion != c.failedVersion
        && pluginmanifest::compareVersions(c.registryVersion, c.installedVersion) > 0;
}

/// What a plugin is doing right now, as the launcher sees it.
struct Activity {
    bool hostRunning = false;   // its host process is up (or starting)
    bool gameRunning = false;   // a game is running (game.started without game.stopped)
    bool overlayShown = false;  // it owns the visible overlay or overlay prompt
    bool userWaiting = false;   // an attach/region request or a call waits on it
    bool reportsBusy = false;   // its last ui.state says `running` or `busy` true
};

/// A host that is not running can be replaced at once; a running one only when
/// nothing above says it is in use (the caller also wants that to hold for a while).
inline bool canReplaceNow(const Activity& a)
{
    if (!a.hostRunning) return true;
    return !a.gameRunning && !a.overlayShown && !a.userWaiting && !a.reportsBusy;
}

} // namespace pluginupdate
