// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

/**
 * When an enabled plugin's host process starts (PluginManager). Never at
 * launch: a host costs a process, and an antivirus watches every one a fresh
 * launcher spawns. It starts on first use (its screen opens, the user presses
 * one of its actions) or on an event it declared. A plugin that ships with
 * Makine wakes only on `game.started`, and only when its stored `autoStart`
 * setting is true. Once up, a host stays up until exit or disable.
 *
 * Header-only, no QObject: tests/ui/test_pluginwake.cpp.
 */
namespace pluginwake {

enum class Reason {
    Launch,   // the launcher came up (window shown)
    Screen,   // the plugin's own screen asked it for something
    Action,   // the user pressed one of its actions
    Event,    // a launcher event (game.started, …) the plugin may have declared
};

/// `storedValues`: the plugin's settings.json "values" as stored, read without a host.
inline bool startsHost(Reason reason, bool bundled, const QStringList& declaredEvents,
                       const QString& event = {}, const QJsonObject& storedValues = {})
{
    switch (reason) {
    case Reason::Launch:
        return false;
    case Reason::Screen:
    case Reason::Action:
        return true;
    case Reason::Event:
        if (!declaredEvents.contains(event)) return false;
        if (bundled)
            return event == QLatin1String("game.started")
                && storedValues.value(QLatin1String("autoStart")).toBool();
        return true;
    }
    return false;
}

} // namespace pluginwake
