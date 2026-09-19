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

// Does an EMPTY recipe under this method type mean "just overlay the payload"?
//
// It matters because two catalogue sources describe the same package
// differently. The published CDN entry (assets/packages/<id>.json, written by
// scripts/package_pipeline.py) omits installMethod entirely for a package that
// installs by a plain structure-preserving copy. The API the launcher actually
// reads its detail from — api/v2/games/<id> — fills that hole in with
//
//     "installMethod": {"type": "script", "steps": [], "options": []}
//
// for 67 of the 237 packages, measured 2026-09-19. "script" is not overlay-safe
// and has no handler, so the honesty gate below refused every one of them:
// Watch Dogs, Thief, DOOM (2016), Skyrim Special Edition, Dark Souls
// Remastered, Control, Cuphead, Celeste, Devil May Cry 5, A Plague Tale —
// 28% of the catalogue, reported from the field as "Bu yama otomatik
// kurulamıyor (kurulum yöntemi: script)" across fourteen separate Sentry
// issues.
//
// The distinction the gate needs is not "which type" but "does this type name
// a PROCESS or a COPY". A copy-shaped type with nothing to copy is the absence
// of a recipe, and the absence of a recipe already means overlay. A type that
// names something else entirely — an installer to run, a Workshop item to
// subscribe to, a forge archive to inject — still has no recipe and still must
// refuse, because overlaying those really would be the silent
// "kuruldu ama çalışmıyor" lie the gate was built to stop.
inline bool emptyRecipeIsPlainOverlay(QStringView type)
{
    static const QStringList kCopyShaped = {
        QStringLiteral("script"),        // the API's stand-in for "no method"
        QStringLiteral("copy"),          QStringLiteral("copyFile"),
        QStringLiteral("copyDir"),       QStringLiteral("overlay"),
        QStringLiteral("direct"),        QStringLiteral("file-replace"),
    };
    return kCopyShaped.contains(type.toString());
}

// Is this top-level folder name one of a package's alternative variants?
//
// Needed because the launcher cannot ask: api/v2/games/<id> drops the
// `variantType` and `variants` fields the published CDN entry carries, for all
// 15 variant packages in the catalogue (verified 2026-09-20). So `pkg.variants`
// arrives empty, nothing offers the user a choice, and an empty recipe walks
// straight into emptyRecipeIsPlainOverlay above — which would copy EVERY
// variant folder into the game root at once: <game>/v0.88.0/BepInEx/… , a patch
// that reports success and loads nothing.
//
// The names themselves are the signal, and they separate cleanly from ordinary
// overlay roots. Every variant string in the catalogue is a version
// ("1.5.78", "1.00", "v0.88.0", "1.202.0.0", "1.0.8 Steam") or a store
// ("Steam", "Gamepass"); every plain overlay root is an engine or content
// directory ("base", "Mods", "font", "menu", "msg", "data_win32", "dropzone").
inline bool looksLikeVariantFolderName(QStringView name)
{
    const QString trimmed = name.trimmed().toString();
    if (trimmed.isEmpty()) return false;

    static const QStringList kStores = {
        QStringLiteral("steam"),    QStringLiteral("gamepass"),
        QStringLiteral("game pass"),QStringLiteral("epic"),
        QStringLiteral("gog"),      QStringLiteral("xbox"),
        QStringLiteral("microsoft store"), QStringLiteral("ms store"),
    };
    if (kStores.contains(trimmed.toLower())) return true;

    // A version: optional leading "v", a digit, then digits and dots only.
    // A trailing store word is allowed ("1.0.8 Steam"), so measure the head.
    const QString head = trimmed.section(QLatin1Char(' '), 0, 0);
    int i = 0;
    if (head.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) i = 1;
    if (i >= head.size() || !head[i].isDigit()) return false;
    bool sawDot = false;
    for (; i < head.size(); ++i) {
        if (head[i].isDigit()) continue;
        if (head[i] == QLatin1Char('.')) { sawDot = true; continue; }
        return false;
    }
    return sawDot;   // "1.0" yes, a bare "2015" no — that is a year or a name
}

// Does a package's root hold alternatives rather than content to copy?
//
// Two or more variant-shaped entries means the archive ships one complete tree
// per version or store, and overlaying it wholesale is wrong. DOOM's "base" +
// "Mods" and Dark Souls' "font" + "menu" + "msg" are complementary, score zero,
// and keep working.
inline bool rootLooksVariantFoldered(const QStringList& topLevelNames)
{
    if (topLevelNames.size() < 2) return false;
    int variantLike = 0;
    for (const QString& n : topLevelNames) {
        if (looksLikeVariantFolderName(n)) ++variantLike;
    }
    return variantLike >= 2;
}

} // namespace makine::steprules
