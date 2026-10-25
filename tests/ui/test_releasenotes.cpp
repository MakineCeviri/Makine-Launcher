// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The launcher's own versions, notes and plan: which version is newer, how
// the CDN's copy lays over the built-in one, and what the page lists.

#include <gtest/gtest.h>
#include <QFile>
#include <algorithm>
#include "releasenotes.h"

using namespace makine::releasenotes;

namespace {

QJsonObject release(const char* version, const char* summary = "")
{
    return QJsonObject{{QStringLiteral("version"), QString::fromLatin1(version)},
                       {QStringLiteral("summary"), QString::fromLatin1(summary)}};
}

QStringList versions(const QJsonArray& releases)
{
    QStringList out;
    for (const QJsonValue& v : releases)
        out << v.toObject().value(QStringLiteral("version")).toString();
    return out;
}

} // namespace

TEST(ReleaseNotes, NewerVersions)
{
    EXPECT_TRUE(isNewer(QStringLiteral("0.2.3-beta"), QStringLiteral("0.2.2-beta")));
    EXPECT_TRUE(isNewer(QStringLiteral("v0.10.0"), QStringLiteral("0.9.9-beta")));
    EXPECT_FALSE(isNewer(QStringLiteral("0.1.5-beta"), QStringLiteral("0.2.3-beta")));
    // A release over its own pre-release, never the other way
    EXPECT_TRUE(isNewer(QStringLiteral("0.2.3"), QStringLiteral("0.2.3-beta")));
    EXPECT_FALSE(isNewer(QStringLiteral("0.2.3-beta"), QStringLiteral("0.2.3")));
    EXPECT_FALSE(isNewer(QStringLiteral("0.2.3-beta"), QStringLiteral("0.2.3-beta")));
    // A manifest without the channel's field says nothing is newer
    EXPECT_FALSE(isNewer(QString(), QStringLiteral("0.2.3-beta")));
    EXPECT_FALSE(isNewer(QStringLiteral("0.2.3-beta"), QString()));
    EXPECT_TRUE(sameVersion(QStringLiteral("v0.2.3-beta"), QStringLiteral("0.2.3-beta")));
    EXPECT_FALSE(sameVersion(QString(), QString()));
}

TEST(ReleaseNotes, ParseDropsWhatThePageCannotShow)
{
    QString error;
    const Manifest m = parse(R"({
        "latest": "0.2.3-beta", "store": "0.1.5-beta", "page": "https://example.org/x",
        "releases": [
            {"version": "0.2.3-beta", "changes": [{"type": "new", "text": "a"}, {"type": "fixed"}]},
            {"title": "no version"}
        ],
        "roadmap": [{"status": "next", "title": "t", "text": "x"}, {"status": "later"}]
    })", &error);
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(m.latest, QStringLiteral("0.2.3-beta"));
    EXPECT_EQ(m.store, QStringLiteral("0.1.5-beta"));
    EXPECT_EQ(m.page, QStringLiteral("https://example.org/x"));
    ASSERT_EQ(m.releases.size(), 1);
    EXPECT_EQ(m.releases[0].toObject().value(QStringLiteral("changes")).toArray().size(), 1);
    EXPECT_EQ(m.roadmap.size(), 1);

    parse("<html>404</html>", &error);
    EXPECT_FALSE(error.isEmpty());
    error.clear();
    parse("[1, 2]", &error);
    EXPECT_FALSE(error.isEmpty());
}

TEST(ReleaseNotes, FreshCopyWinsAndNewestComesFirst)
{
    const QJsonArray bundled{release("0.2.3-beta", "built in"), release("0.2.2-beta")};
    const QJsonArray fresh{release("0.2.2-beta"), release("0.2.4-beta"), release("0.2.3-beta", "corrected")};
    const QJsonArray merged = mergeReleases(fresh, bundled);
    EXPECT_EQ(versions(merged),
              (QStringList{"0.2.4-beta", "0.2.3-beta", "0.2.2-beta"}));
    EXPECT_EQ(merged[1].toObject().value(QStringLiteral("summary")).toString(), QStringLiteral("corrected"));

    // Offline: the built-in notes alone
    EXPECT_EQ(versions(mergeReleases({}, bundled)), (QStringList{"0.2.3-beta", "0.2.2-beta"}));
}

TEST(ReleaseNotes, PageListsUpToWhatThisCopyCanInstall)
{
    const QJsonArray all{release("0.2.5-beta"), release("0.2.4-beta"), release("0.2.3-beta"),
                         release("0.2.2-beta")};

    // GitHub has 0.2.4; 0.2.5 is in the notes before its release
    QVariantList shown = forDisplay(all, QStringLiteral("0.2.3-beta"), QStringLiteral("0.2.4-beta"));
    ASSERT_EQ(shown.size(), 3);
    EXPECT_EQ(shown[0].toMap().value(QStringLiteral("version")).toString(), QStringLiteral("0.2.4-beta"));
    EXPECT_TRUE(shown[0].toMap().value(QStringLiteral("newer")).toBool());
    EXPECT_FALSE(shown[0].toMap().value(QStringLiteral("running")).toBool());
    EXPECT_TRUE(shown[1].toMap().value(QStringLiteral("running")).toBool());
    EXPECT_FALSE(shown[1].toMap().value(QStringLiteral("newer")).toBool());

    // The Store still serves an older version: nothing above the running one
    shown = forDisplay(all, QStringLiteral("0.2.3-beta"), QStringLiteral("0.1.5-beta"));
    ASSERT_EQ(shown.size(), 2);
    EXPECT_TRUE(shown[0].toMap().value(QStringLiteral("running")).toBool());

    // Unknown (offline)
    EXPECT_EQ(forDisplay(all, QStringLiteral("0.2.3-beta"), QString()).size(), 2);
}

TEST(ReleaseNotes, DateInTurkish)
{
    QJsonObject r = release("0.2.3-beta");
    r.insert(QStringLiteral("date"), QStringLiteral("2026-10-25"));
    const QVariantList shown = forDisplay(QJsonArray{r}, QStringLiteral("0.2.3-beta"), QString());
    ASSERT_EQ(shown.size(), 1);
    EXPECT_EQ(shown[0].toMap().value(QStringLiteral("dateText")).toString(), QStringLiteral("25 Ekim 2026"));
}

#ifdef MAKINE_WHATSNEW_JSON
TEST(ReleaseNotes, BuiltInNotesStartWithThisVersion)
{
    // Bumping the version without its notes would open "Yenilikler" on the
    // previous version's — and mark that as the one running.
    QFile f(QStringLiteral(MAKINE_WHATSNEW_JSON));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    QString error;
    const Manifest m = parse(f.readAll(), &error);
    ASSERT_TRUE(error.isEmpty()) << error.toStdString();
    ASSERT_FALSE(m.releases.isEmpty());
    // The app says "0.2.3.0-beta" (dev: "0.2.3.0-pre-alpha-dev"), the notes "0.2.3-beta"
    const QString top = m.releases[0].toObject().value(QStringLiteral("version")).toString();
    EXPECT_TRUE(sameVersion(top, QStringLiteral(MAKINE_VERSION_FULL)))
        << top.toStdString() << " vs " << MAKINE_VERSION_FULL;
    for (const QJsonValue& v : m.releases)
        EXPECT_FALSE(v.toObject().value(QStringLiteral("changes")).toArray().isEmpty())
            << v.toObject().value(QStringLiteral("version")).toString().toStdString();
    for (const char* stage : {"doing", "next", "later"}) {
        const bool any = std::any_of(m.roadmap.begin(), m.roadmap.end(), [stage](const QJsonValue& v) {
            return v.toObject().value(QStringLiteral("status")).toString() == QLatin1String(stage);
        });
        EXPECT_TRUE(any) << stage;
    }
}
#endif
