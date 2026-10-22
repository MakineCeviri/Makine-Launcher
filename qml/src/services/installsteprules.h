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

#include "bytepatch.h"

#include <QDir>
#include <QList>
#include <QPair>
#include <QSet>
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
        QStringLiteral("setSteamLanguage"), QStringLiteral("vpatch"),
        QStringLiteral("patchBytes"),    QStringLiteral("forgeReimport"),
        QStringLiteral("xdelta")
    };
    return kActions;
}

// The step set above, as a number the catalog API understands (header
// X-Makine-Recipe). Below a recipe's level the API serves the older recipe
// kept for that game in install_methods_compat — a D1 change reaches every
// launcher at once, and 0.1.5 met 0.1.6-only steps that way (Elden Ring
// refused for 294 users). Bump it with every action added to knownActions().
//   1  0.1.5 and older (sent nothing)
//   2  patchBytes, forgeReimport, xdelta; copyFile in the backup list
inline constexpr int kRecipeLevel = 2;

inline bool isKnownAction(const QString& action)
{
    return knownActions().contains(action);
}

// Does this action rewrite a file of the game in place (rather than copy a
// package file over it)? Such a step cannot simply run again on an updated
// install: the file it edits is already edited, and its original exists only
// in the backup taken at the first install.
inline bool isInPlaceAction(const QString& action)
{
    return action == QLatin1String("patchBytes") || action == QLatin1String("forgeReimport")
        || action == QLatin1String("xdelta") || action == QLatin1String("vpatch");
}

// The game file a step changes without copying a package file over it: one it
// rewrites in place, or one it deletes. The package does not contain it, so the
// pre-install backup must list it separately; empty for every other step.
//
// "delete" belongs here: AC Unity's recipe (289650) deletes the game's own
// DataPC_patch_01.forge. Left out of the backup, uninstall had nothing to put
// back and the game stayed without that archive.
inline QString backupTarget(const QString& action, const QString& src, const QString& dest)
{
    if (action == QLatin1String("vpatch") || action == QLatin1String("forgeReimport")
        || action == QLatin1String("xdelta") || action == QLatin1String("delete"))
        return dest;
    if (action == QLatin1String("patchBytes"))   // dest, or src in place when empty
        return dest.isEmpty() ? src : dest;
    return {};
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
                            const QString& language = {},
                            const QList<QPair<QString, QString>>& patches = {})
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
    } else if (action == QLatin1String("vpatch")) {
        // src = the .pat delta inside the package, dest = the game file it
        // rewrites. Both are mandatory: a vpatch step with either missing
        // would rewrite the wrong file or nothing at all.
        if (src.isEmpty())      return QStringLiteral("src");
        if (dest.isEmpty())     return QStringLiteral("dest");
    } else if (action == QLatin1String("patchBytes")) {
        // src = the game file read; dest (optional) = where the edited copy
        // goes, in place when empty. A malformed find/replace pair is refused
        // here, before any step has touched the game.
        if (src.isEmpty())      return QStringLiteral("src");
        if (!bytepatch::decodePairs(patches, nullptr)) return QStringLiteral("patches");
    } else if (action == QLatin1String("forgeReimport") || action == QLatin1String("xdelta")) {
        // src = the reimport folder / VCDIFF delta inside the package, dest =
        // the game archive it rewrites.
        if (src.isEmpty())      return QStringLiteral("src");
        if (dest.isEmpty())     return QStringLiteral("dest");
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

// Is this "run" step launching a tool the recipe ALSO hands to the user?
//
// Elden Ring's recipe copied engus/ and fn/ into the game, then ran
// ERING_TR.exe, then copied that same ERING_TR.exe to the desktop as
// "Elden Ring.exe". The run step failed in the field — 915 events across 160
// users in 14 days — and was read at the time as an interactive GUI tool.
// That reading was wrong: decompiled (2026-10-08), ERING_TR.exe has no window
// and no prompt; the GUIGetMsg it seemed to wait on is part of the AutoIt
// runtime every compiled script carries. It is an exe patcher that only works
// with Game/ as its working directory, which neither the run step (package
// folder) nor the desktop copy ever gave it. The recipe now edits the exe
// itself through "patchBytes" (bytepatch.h).
//
// The rule still holds on its own terms. The tell is structural: a recipe that
// installs an executable FOR the user has already said the user is the one who
// runs it. Running it ourselves as well is the leftover of a manual procedure.
// Measured over the whole catalogue 2026-09-20, Elden Ring is the only package
// with this shape; the other four run steps (moesow.exe, patch.exe) are
// argument-taking command-line patchers and keep running.
inline bool runStepIsUserLaunchedTool(QStringView exe,
                                      const QStringList& userInstalledTools)
{
    // Compare leaf names: a step may say "tools/x.exe" where the desktop copy
    // says "x.exe". Paths use either separator depending on who authored them.
    const auto leaf = [](QStringView v) {
        const QString s = v.toString();
        const int cut = qMax(s.lastIndexOf(QLatin1Char('/')),
                             s.lastIndexOf(QLatin1Char('\\')));
        return (cut < 0 ? s : s.mid(cut + 1)).trimmed().toLower();
    };
    const QString want = leaf(exe);
    if (want.isEmpty()) return false;
    for (const QString& tool : userInstalledTools) {
        if (leaf(tool) == want) return true;
    }
    return false;
}

// A run step's arguments with ${gamePath} and ${packageDir} filled in. The two
// paths go in with native separators: Qt holds them as "D:/SteamLibrary/…",
// and a patch's own .bat that hands that to xcopy or copy fails, because cmd
// reads "/SteamLibrary" as a switch. The rest of each argument is the recipe
// author's and stays as written.
inline QStringList resolveRunArgs(const QStringList& args, const QString& gamePath,
                                  const QString& packageDir)
{
    const QString game = QDir::toNativeSeparators(gamePath);
    const QString pkg  = QDir::toNativeSeparators(packageDir);
    QStringList resolved;
    resolved.reserve(args.size());
    for (QString arg : args) {
        arg.replace(QLatin1String("${gamePath}"), game);
        arg.replace(QLatin1String("${packageDir}"), pkg);
        resolved.append(arg);
    }
    return resolved;
}

// Does an update's copy write over a file of the game's own?
//
// The update takes no backup and used to answer from the first install's
// replaced list alone. A file the new version overwrites for the first time
// (v1 left data/b.pak alone, v2 ships it) was then recorded as ADDED: uninstall
// restored the first backup, which does not hold it, and deleted the game's
// original. A file that was there and that we did not add is the game's.
inline bool updateOverwritesOriginal(bool destExisted, bool replacedBefore, bool addedBefore)
{
    return replacedBefore || (destExisted && !addedBefore);
}

// A re-install over the patch already in the game finds the earlier install's
// files on disk and counts them as the game's own ("replaced"). That is the
// path an update takes when there is no backup to go back to — every package
// that only adds files has none. Recorded so, uninstall left the patch files
// in place and started a store repair for "changed originals" that were ours.
// A file the earlier record lists as added stays added.
inline void keepEarlierAdded(QStringList& added, QStringList& replaced,
                             const QStringList& earlierAdded)
{
    const QSet<QString> ours(earlierAdded.cbegin(), earlierAdded.cend());
    for (auto it = replaced.begin(); it != replaced.end();) {
        if (ours.contains(*it)) {
            added.append(*it);
            it = replaced.erase(it);
        } else {
            ++it;
        }
    }
}

// What an install that stopped part-way leaves on record.
//
// A failed or cancelled install has still written files. Its rollback
// (GameService::performInstallRollback) restores the backup, then removes the
// files the install added through uninstallPackage — which reads them from the
// install record. A failed install wrote no record, so nothing was removed: a
// half-copied patch stayed in the game behind "oyun kurulum öncesi haline
// döndürüldü", and the next attempt backed those leftovers up as originals.
//
// The record is the one already there (a re-install's) plus every file this
// attempt wrote that it does not list yet. A file already on record keeps its
// classification: the earlier install knew whether it was the game's.
struct FileRecord {
    QStringList installed;   // everything written, "_marker:" entries included
    QStringList added;       // files the game did not have
    QStringList replaced;    // the game's own files written over
};

inline FileRecord mergeWrittenFiles(FileRecord record, const QStringList& written,
                                    const QStringList& writtenReplaced)
{
    QSet<QString> known(record.installed.cbegin(), record.installed.cend());
    const QSet<QString> originals(writtenReplaced.cbegin(), writtenReplaced.cend());
    for (const QString& f : written) {
        if (known.contains(f)) continue;
        known.insert(f);
        record.installed.append(f);
        if (f.startsWith(QLatin1Char('_'))) continue;   // a marker, not a file of ours
        (originals.contains(f) ? record.replaced : record.added).append(f);
    }
    return record;
}

// Which of `files` an install record does not list. A "_vpatch:" entry names
// the game file it edited in place.
//
// A failed install over an earlier one fails into that one's record
// (mergeWrittenFiles): its own files are what the earlier record lacks. With
// no backup, the rollback removed the whole record — a failed, timed-out or
// cancelled update of an add-only patch deleted the working earlier version.
inline QStringList notOnRecord(const QStringList& files, const QStringList& record)
{
    QSet<QString> listed;
    for (const QString& f : record)
        listed.insert(f.startsWith(QLatin1String("_vpatch:")) ? f.mid(8) : f);
    QStringList out;
    for (const QString& f : files)
        if (!listed.contains(f)) out.append(f);
    return out;
}

} // namespace makine::steprules
