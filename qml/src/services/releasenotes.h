// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file releasenotes.h
 * @brief The launcher's own release notes and plan, and which version is newest
 *
 * One JSON shape, two copies. qml/resources/whatsnew.json is built into the
 * launcher: the notes up to the version in hand and the plan, readable
 * offline. The same file, with the newest version of each channel added
 * (`just launcher-json`), is published as cdn.makineceviri.org/assets/
 * launcher.json; the launcher reads it to say a newer version exists and
 * lays its notes and plan over the built-in ones.
 *
 *   {
 *     "latest": "0.2.3-beta",   // newest GitHub release (ZIP)       — CDN copy only
 *     "store":  "0.2.3-beta",   // what the Microsoft Store serves    — CDN copy only
 *     "page":   "https://…",    // where the ZIP is downloaded        — CDN copy only
 *     "releases": [ { "version", "date": "YYYY-MM-DD", "title", "summary",
 *                     "changes": [ { "type": "new|improved|fixed", "text" } ] } ],
 *     "roadmap":  [ { "status": "doing|next|later", "title", "text" } ]
 *   }
 *
 * Header-only, no QObject: tests/ui/test_releasenotes.cpp.
 */

#pragma once

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace makine::releasenotes {

/// Dotted numbers: 1 when `a` is higher, -1 when lower, 0 when equal.
/// Missing parts count as 0 ("1.0" == "1.0.0"); an empty side compares equal.
inline int compareNumbers(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty())
        return 0;
    const QStringList pa = a.split(QLatin1Char('.'));
    const QStringList pb = b.split(QLatin1Char('.'));
    for (qsizetype i = 0; i < std::max(pa.size(), pb.size()); ++i) {
        const int x = i < pa.size() ? pa[i].toInt() : 0;
        const int y = i < pb.size() ? pb[i].toInt() : 0;
        if (x != y)
            return x > y ? 1 : -1;
    }
    return 0;
}

/// "v0.2.3-beta" → {"0.2.3", "beta"}
inline std::pair<QString, QString> split(QString version)
{
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        version.remove(0, 1);
    const qsizetype dash = version.indexOf(QLatin1Char('-'));
    if (dash > 0)
        return {version.left(dash), version.mid(dash + 1)};
    return {version, {}};
}

/// True when `version` is a later release than `than`. A release outranks
/// its own pre-release ("0.2.3" over "0.2.3-beta"); two pre-releases of one
/// number are the same release. An empty side is never newer.
inline bool isNewer(const QString& version, const QString& than)
{
    const auto [vn, vp] = split(version);
    const auto [tn, tp] = split(than);
    if (vn.isEmpty() || tn.isEmpty())
        return false;
    const int cmp = compareNumbers(vn, tn);
    return cmp > 0 || (cmp == 0 && !tp.isEmpty() && vp.isEmpty());
}

inline bool sameVersion(const QString& a, const QString& b)
{
    return !a.isEmpty() && !b.isEmpty() && !isNewer(a, b) && !isNewer(b, a);
}

struct Manifest {
    QString latest;
    QString store;
    QString page;
    QJsonArray releases;
    QJsonArray roadmap;
};

/// Reads either copy. Releases without a version, changes without text and
/// plan items without a title are dropped; `error` says why nothing was read.
inline Manifest parse(const QByteArray& json, QString* error = nullptr)
{
    Manifest m;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = pe.error != QJsonParseError::NoError ? pe.errorString()
                                                          : QStringLiteral("not a JSON object");
        return m;
    }
    const QJsonObject o = doc.object();
    m.latest = o.value(QStringLiteral("latest")).toString();
    m.store = o.value(QStringLiteral("store")).toString();
    m.page = o.value(QStringLiteral("page")).toString();

    for (const QJsonValue& v : o.value(QStringLiteral("releases")).toArray()) {
        QJsonObject r = v.toObject();
        if (r.value(QStringLiteral("version")).toString().isEmpty())
            continue;
        QJsonArray changes;
        for (const QJsonValue& c : r.value(QStringLiteral("changes")).toArray())
            if (!c.toObject().value(QStringLiteral("text")).toString().isEmpty())
                changes.append(c);
        r.insert(QStringLiteral("changes"), changes);
        m.releases.append(r);
    }
    for (const QJsonValue& v : o.value(QStringLiteral("roadmap")).toArray())
        if (!v.toObject().value(QStringLiteral("title")).toString().isEmpty())
            m.roadmap.append(v);
    return m;
}

/// `fresh` over `bundled`, one entry per version, newest first.
inline QJsonArray mergeReleases(const QJsonArray& fresh, const QJsonArray& bundled)
{
    const QString key = QStringLiteral("version");
    QList<QJsonObject> all;
    for (const QJsonArray* list : {&fresh, &bundled}) {
        for (const QJsonValue& v : *list) {
            const QJsonObject r = v.toObject();
            const QString version = r.value(key).toString();
            const bool known = std::any_of(all.cbegin(), all.cend(), [&](const QJsonObject& o) {
                return sameVersion(o.value(key).toString(), version);
            });
            if (!known)
                all.append(r);
        }
    }
    std::stable_sort(all.begin(), all.end(), [&key](const QJsonObject& a, const QJsonObject& b) {
        return isNewer(a.value(key).toString(), b.value(key).toString());
    });
    QJsonArray out;
    for (const QJsonObject& r : all)
        out.append(r);
    return out;
}

/// What the page lists: every release up to the one this copy can install
/// (`available`, empty when unknown) or the one running, whichever is later.
/// Each entry gains `running`, `newer` and a Turkish `dateText`.
inline QVariantList forDisplay(const QJsonArray& releases, const QString& running,
                               const QString& available)
{
    const QString ceiling = isNewer(available, running) ? available : running;
    const QLocale turkish(QLocale::Turkish, QLocale::Turkey);
    QVariantList out;
    for (const QJsonValue& v : releases) {
        QVariantMap r = v.toObject().toVariantMap();
        const QString version = r.value(QStringLiteral("version")).toString();
        if (isNewer(version, ceiling))
            continue;
        r.insert(QStringLiteral("running"), sameVersion(version, running));
        r.insert(QStringLiteral("newer"), isNewer(version, running));
        const QDate date = QDate::fromString(r.value(QStringLiteral("date")).toString(), Qt::ISODate);
        r.insert(QStringLiteral("dateText"),
                 date.isValid() ? turkish.toString(date, QStringLiteral("d MMMM yyyy")) : QString());
        out.append(r);
    }
    return out;
}

} // namespace makine::releasenotes
