// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Pure decision rules shared by the install path and its tests.
//
// These live outside localpackagemanager.cpp so they can be exercised directly:
// both rules decide whether we touch a user's game directory, and a wrong
// answer either half-patches a game or copies the wrong game version's assets
// over a working install. That is not something to verify by reading.
//
// Deliberately free of InstallStep/QObject so the test needs nothing but
// Qt6::Core — no MOC, no service construction, no filesystem.

#include <QString>
#include <QStringList>
#include <QStringView>

namespace makine::steprules {

// Actions executeStep() actually dispatches on. Keep in lockstep with it.
inline const QStringList& knownActions()
{
    static const QStringList kActions = {
        QStringLiteral("copy"),          QStringLiteral("copyFile"),
        QStringLiteral("copyDir"),       QStringLiteral("delete"),
        QStringLiteral("installFont"),   QStringLiteral("run"),
        QStringLiteral("copyToDesktop"), QStringLiteral("rename"),
        QStringLiteral("setSteamLanguage")
    };
    return kActions;
}

inline bool isKnownAction(const QString& action)
{
    return knownActions().contains(action);
}

// Which required parameter of a KNOWN action is empty, if any.
//
// An unknown action name is not the only way a recipe can be unrunnable. The
// catalog parser reads exactly action/src/dest/exe/fallback/workDir/language/
// args; a recipe authored against other key spellings ("cmd" instead of
// exe+args, "pattern"/"to" instead of src/dest, "patch"/"target") parses into a
// well-formed step carrying a KNOWN action and empty parameters. Such a step
// passes an action-name check and then does nothing — after the steps before it
// have already modified the game.
//
// Returns the offending field name, or an empty string when executable.
inline QString missingField(const QString& action, const QString& src = {},
                            const QString& dest = {}, const QString& exe = {},
                            const QString& language = {})
{
    if (action == QLatin1String("copy") || action == QLatin1String("copyFile")
        || action == QLatin1String("copyDir") || action == QLatin1String("rename")
        || action == QLatin1String("copyToDesktop")) {
        if (src.isEmpty())      return QStringLiteral("src");
        if (dest.isEmpty())     return QStringLiteral("dest");
    } else if (action == QLatin1String("delete")) {
        if (dest.isEmpty())     return QStringLiteral("dest");
    } else if (action == QLatin1String("installFont")) {
        if (src.isEmpty())      return QStringLiteral("src");
    } else if (action == QLatin1String("run")) {
        if (exe.isEmpty())      return QStringLiteral("exe");
    } else if (action == QLatin1String("setSteamLanguage")) {
        if (language.isEmpty()) return QStringLiteral("language");
    }
    return {};
}

// Does an archive folder name denote the declared variant?
//
// Variant folders inside a .makine archive are not named exactly like the
// declared variant string: Hollow Knight declares "1.5.78"/"1.5.80" but ships
// "v1.5.78.11833"/"1.5.80". Tolerate a leading "v" and a longer build suffix.
// Callers must still require a UNIQUE match — copying the wrong game version's
// assets breaks the game, so an ambiguous result may never be guessed at.
inline bool variantFolderMatches(const QString& variant, const QString& folder)
{
    const auto norm = [](QString s) {
        if (s.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) s.remove(0, 1);
        return s;
    };
    if (variant.isEmpty() || folder.isEmpty()) return false;
    const QString want = norm(variant);
    const QString have = norm(folder);
    return have == want || have.startsWith(want + QLatin1Char('.'));
}

} // namespace makine::steprules
