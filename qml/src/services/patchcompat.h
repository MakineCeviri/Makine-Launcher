// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// Patch alternatives and game-version compatibility — the decisions the game
// page shows, kept out of the services so they can be tested on their own.
//
//  * Which build the player's game is at: the installed copy's Steam buildid
//    when we know it, else the latest public build Steam reports
//    (assets/game-builds.json, written by the Worker cron).
//  * Whether a patch may not fit it: the patch names the build it was made
//    for (gameVersion.steamBuild) and the game is past it.
//  * Which action a patch option offers, given what is installed.
//
// Shared contract: Makine-WEB ⇄ Makine-Launcher "patch alternatives".

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace makine::patchcompat {

// ---- assets/game-builds.json ----------------------------------------------

struct LatestBuild {
    qint64 steamBuild{0};
    QString date;   // YYYY-MM-DD
};

/// {generatedAt, builds: {appId: {steamBuild, date}}}. Read defensively:
/// anything malformed is left out; a broken file is an empty map.
inline QHash<QString, LatestBuild> parseGameBuilds(const QByteArray& data)
{
    QHash<QString, LatestBuild> out;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return out;
    const QJsonValue builds = doc.object().value(QStringLiteral("builds"));
    if (!builds.isObject()) return out;
    const QJsonObject obj = builds.toObject();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (it.key().isEmpty() || !it.value().isObject()) continue;
        const QJsonObject b = it.value().toObject();
        const QJsonValue build = b.value(QStringLiteral("steamBuild"));
        if (!build.isDouble()) continue;
        const double d = build.toDouble();
        if (!(d > 0 && d < 9.0e15)) continue;
        out.insert(it.key(), LatestBuild{static_cast<qint64>(d),
                                         b.value(QStringLiteral("date")).toString()});
    }
    return out;
}

// ---- The warning ------------------------------------------------------------

enum class BuildSource { None, Installed, Latest };

struct Compat {
    bool warn{false};             // the game is newer than the patch
    BuildSource source{BuildSource::None};
    qint64 currentBuild{0};       // the build we compared with (0 = none known)
};

/// patchBuild: the patch's gameVersion.steamBuild (0 = unknown → never warn).
/// installedBuild: the player's appmanifest buildid (0 = not known).
/// latestBuild: Steam's latest public build (0 = not known).
inline Compat decide(qint64 patchBuild, qint64 installedBuild, qint64 latestBuild)
{
    Compat c;
    if (installedBuild > 0) {
        c.source = BuildSource::Installed;
        c.currentBuild = installedBuild;
    } else if (latestBuild > 0) {
        c.source = BuildSource::Latest;
        c.currentBuild = latestBuild;
    }
    c.warn = patchBuild > 0 && c.currentBuild > patchBuild;
    return c;
}

inline QString sourceName(BuildSource s)
{
    switch (s) {
    case BuildSource::Installed: return QStringLiteral("installed");
    case BuildSource::Latest:    return QStringLiteral("latest");
    case BuildSource::None:      break;
    }
    return {};
}

// ---- What a patch option offers ---------------------------------------------

enum class Action {
    Install,    // nothing installed: "Kur"
    Switch,     // another patch is installed: "Bu yamaya geç"
    Update,     // this patch is installed, the catalogue has a newer one: "Güncelle"
    Installed,  // this patch is installed and current
};

/// optionPkg / installedPkg: "" = the default patch. An install record
/// without "pkg" (every record before alternatives) is the default.
inline Action actionFor(const QString& optionPkg, bool anyInstalled,
                        const QString& installedPkg, bool installedHasUpdate)
{
    if (!anyInstalled) return Action::Install;
    if (optionPkg != installedPkg) return Action::Switch;
    return installedHasUpdate ? Action::Update : Action::Installed;
}

inline QString actionName(Action a)
{
    switch (a) {
    case Action::Install:   return QStringLiteral("install");
    case Action::Switch:    return QStringLiteral("switch");
    case Action::Update:    return QStringLiteral("update");
    case Action::Installed: return QStringLiteral("installed");
    }
    return {};
}

/// The detail cache key and file stem for a patch: "<appId>" for the
/// default, "<appId>--<pkg>" for an alternative (cache/packages/<key>.json).
inline QString detailKey(const QString& appId, const QString& pkg)
{
    return pkg.isEmpty() ? appId : appId + QStringLiteral("--") + pkg;
}

} // namespace makine::patchcompat
