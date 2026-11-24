// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Which running process counts as a game for plugins (Makine Live), beyond the
// catalogue: an installed game the library knows, or — as a fallback — the
// process in front whose window covers its monitor and that renders with
// D3D/Vulkan. Header-only so test_gameprocessrules checks it without Windows.

#include <QSet>
#include <QString>

namespace gameprocessrules {

/// Stable id for a game that has no catalogue id: "exe:<exe name, lower case>".
inline QString otherGameId(const QString& exeName)
{
    return QStringLiteral("exe:") + exeName.toLower();
}

/// Never a game, however it renders: browsers, chat, store launchers, streaming
/// and capture tools, the shell, and Makine itself.
inline bool isExcludedExe(const QString& exeName)
{
    static const QSet<QString> kExcluded = {
        // browsers
        "chrome.exe", "msedge.exe", "firefox.exe", "opera.exe", "brave.exe", "vivaldi.exe",
        "msedgewebview2.exe", "arc.exe",
        // chat, calls, media
        "discord.exe", "discordptb.exe", "discordcanary.exe", "teams.exe", "ms-teams.exe", "zoom.exe",
        "slack.exe", "telegram.exe", "whatsapp.exe", "spotify.exe", "vlc.exe", "mpv.exe",
        "mpc-hc64.exe", "potplayermini64.exe", "powerpnt.exe",
        // store launchers
        "steam.exe", "steamwebhelper.exe", "epicgameslauncher.exe", "galaxyclient.exe", "upc.exe",
        "eadesktop.exe", "battle.net.exe", "riotclientservices.exe",
        // console streaming and capture (Live's console mode reads those)
        "remoteplay.exe", "chiaki.exe", "chiaki-ng.exe", "xboxpcapp.exe", "greenlight.exe",
        "obs64.exe", "obs32.exe", "4kcaptureutility.exe", "gamecapture.exe",
        // the shell
        "explorer.exe", "dwm.exe", "applicationframehost.exe", "searchhost.exe",
        "shellexperiencehost.exe", "startmenuexperiencehost.exe", "textinputhost.exe", "lockapp.exe",
        // Makine
        "makine-launcher.exe", "makine-plugin-host.exe", "makine-elevate.exe",
    };
    return kExcluded.contains(exeName.toLower());
}

/// The fallback: the foreground process counts as a game when its window covers
/// its monitor (fullscreen or borderless), it has D3D/Vulkan loaded and it is not
/// on the exclusion list.
inline bool looksLikeForegroundGame(const QString& exeName, bool coversMonitor, bool rendersWithGpu)
{
    return coversMonitor && rendersWithGpu && exeName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)
        && !isExcludedExe(exeName);
}

} // namespace gameprocessrules
