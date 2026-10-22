// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// Telemetry went blind for most of every month: Sentry's quota filled in 6-9
// days, so 37 of 61 days had no data. These rules are what keeps both
// channels seeing — the counting payload the Worker accepts, the queue that
// survives being offline, and the gate that stops one defect from spending
// Sentry's month in a day.

#include <gtest/gtest.h>
#include "telemetryrules.h"

using namespace makine::telemetryrules;

// ===== Counting channel ======================================================

TEST(TelemetryRules, PayloadMatchesTheWorkerContract)
{
    Event e;
    e.t = 1727000000000;
    e.op = QStringLiteral("install");
    e.out = QStringLiteral("fail");
    e.side = QStringLiteral("system");
    e.reason = QStringLiteral("step_failed");
    e.app = QStringLiteral("1245620");
    e.meta.insert(QStringLiteral("step"), QStringLiteral("run"));

    const Identity id{QStringLiteral("0123456789abcdef"), QString(32, QLatin1Char('a')),
                      QStringLiteral("0.1.5.0-beta"), QStringLiteral("msix"),
                      QStringLiteral("10.0.26200")};
    const QJsonObject body = buildPayload(id, {e});

    EXPECT_EQ(body.value(QStringLiteral("v")).toInt(), 2);
    EXPECT_EQ(body.value(QStringLiteral("install")).toString(), QStringLiteral("0123456789abcdef"));
    EXPECT_EQ(body.value(QStringLiteral("channel")).toString(), QStringLiteral("msix"));

    const QJsonObject ev = body.value(QStringLiteral("events")).toArray().at(0).toObject();
    EXPECT_EQ(ev.value(QStringLiteral("t")).toInteger(), 1727000000000);
    EXPECT_EQ(ev.value(QStringLiteral("op")).toString(), QStringLiteral("install"));
    EXPECT_EQ(ev.value(QStringLiteral("out")).toString(), QStringLiteral("fail"));
    EXPECT_EQ(ev.value(QStringLiteral("side")).toString(), QStringLiteral("system"));
    EXPECT_EQ(ev.value(QStringLiteral("reason")).toString(), QStringLiteral("step_failed"));
    EXPECT_EQ(ev.value(QStringLiteral("app")).toString(), QStringLiteral("1245620"));
    EXPECT_EQ(ev.value(QStringLiteral("meta")).toObject().value(QStringLiteral("step")).toString(),
              QStringLiteral("run"));
}

TEST(TelemetryRules, EmptyOptionalFieldsAreOmittedNotSentBlank)
{
    Event e;
    e.t = 1;
    e.op = QStringLiteral("session");
    e.out = QStringLiteral("ok");
    const QJsonObject ev = toJson(e);
    EXPECT_FALSE(ev.contains(QStringLiteral("side")));
    EXPECT_FALSE(ev.contains(QStringLiteral("reason")));
    EXPECT_FALSE(ev.contains(QStringLiteral("app")));
    EXPECT_FALSE(ev.contains(QStringLiteral("meta")));
}

TEST(TelemetryRules, QueuedEventSurvivesTheDiskRoundTrip)
{
    Event e;
    e.t = 1727000000123;
    e.op = QStringLiteral("scan");
    e.out = QStringLiteral("ok");
    e.meta.insert(QStringLiteral("games"), 12);

    const Event back = fromJson(toJson(e));
    EXPECT_EQ(back.t, e.t);
    EXPECT_EQ(back.op, e.op);
    EXPECT_EQ(back.out, e.out);
    EXPECT_EQ(back.meta.value(QStringLiteral("games")).toInt(), 12);
}

TEST(TelemetryRules, OfflineQueueKeepsTheNewest)
{
    QList<Event> q;
    for (int i = 0; i < kMaxQueued + 25; ++i) {
        Event e;
        e.t = i;
        q.append(e);
    }
    capQueue(q);
    ASSERT_EQ(q.size(), kMaxQueued);
    EXPECT_EQ(q.first().t, 25);
    EXPECT_EQ(q.last().t, kMaxQueued + 24);
}

TEST(TelemetryRules, OnlyAMalformedPayloadIsDropped)
{
    EXPECT_EQ(classifyReply(200), SendResult::Sent);
    EXPECT_EQ(classifyReply(400), SendResult::Drop);
    EXPECT_EQ(classifyReply(413), SendResult::Drop);
    // Offline (no HTTP status), rate limited, server trouble: try again later.
    EXPECT_EQ(classifyReply(0), SendResult::Retry);
    EXPECT_EQ(classifyReply(429), SendResult::Retry);
    EXPECT_EQ(classifyReply(503), SendResult::Retry);
}

TEST(TelemetryRules, InstallIdIsTheOneSentryAlwaysUsed)
{
    const QByteArray machine("machine-guid-example");
    const QString expected = QString::fromLatin1(
        QCryptographicHash::hash(machine, QCryptographicHash::Sha256).toHex().left(16));
    EXPECT_EQ(installId(machine), expected);
    EXPECT_EQ(installId(machine).size(), 16);
    EXPECT_TRUE(installId(QByteArray()).isEmpty());
}

TEST(TelemetryRules, ChannelSeparatesStoreZipAndDevBuilds)
{
    EXPECT_EQ(channel(true, false), QStringLiteral("msix"));
    EXPECT_EQ(channel(false, false), QStringLiteral("plain"));
    EXPECT_EQ(channel(true, true), QStringLiteral("dev"));
}

// ===== Sentry gate ===========================================================

TEST(TelemetryRules, SameFailureReachesSentryOncePerDay)
{
    const qint64 now = 1727000000000;
    EXPECT_TRUE(sentryMaySend(0, now));                                // never sent
    EXPECT_FALSE(sentryMaySend(now - 60'000, now));                     // a minute ago
    EXPECT_FALSE(sentryMaySend(now - kSentryRepeatWindowMs + 1, now));
    EXPECT_TRUE(sentryMaySend(now - kSentryRepeatWindowMs, now));
}

TEST(TelemetryRules, ClockMovedBackOpensTheGate)
{
    const qint64 now = 1727000000000;
    EXPECT_TRUE(sentryMaySend(now + 3'600'000, now));
}

TEST(TelemetryRules, KeySeparatesReasonAndSubject)
{
    EXPECT_NE(sentryKey(QStringLiteral("install"), QStringLiteral("step_failed"), QStringLiteral("1245620")),
              sentryKey(QStringLiteral("install"), QStringLiteral("step_failed"), QStringLiteral("812140")));
    EXPECT_NE(sentryKey(QStringLiteral("install"), QStringLiteral("step_failed"), QStringLiteral("1245620")),
              sentryKey(QStringLiteral("install"), QStringLiteral("backup_failed"), QStringLiteral("1245620")));
}

TEST(TelemetryRules, PruneForgetsOnlyExpiredKeys)
{
    const qint64 now = 1727000000000;
    QVariantMap sent;
    sent.insert(QStringLiteral("old"), now - kSentryRepeatWindowMs - 1);
    sent.insert(QStringLiteral("fresh"), now - 1000);
    pruneSent(sent, now);
    EXPECT_FALSE(sent.contains(QStringLiteral("old")));
    EXPECT_TRUE(sent.contains(QStringLiteral("fresh")));
}

TEST(TelemetryRules, FailureSubjectKeepsTheGameId)
{
    // GameService reports install/uninstall failures as "id (name)"; dropping
    // the whole subject would lose which game failed — the one fact that matters.
    EXPECT_EQ(appFromSubject(QStringLiteral("1245620 (Elden Ring)")), QStringLiteral("1245620"));
    EXPECT_EQ(appFromSubject(QStringLiteral("812140")), QStringLiteral("812140"));
    EXPECT_EQ(appFromSubject(QStringLiteral("catalog")), QStringLiteral("catalog"));
    EXPECT_TRUE(appFromSubject(QStringLiteral("Some Game")).isEmpty());
    EXPECT_TRUE(appFromSubject(QString(33, QLatin1Char('1'))).isEmpty());
    EXPECT_TRUE(appFromSubject(QString()).isEmpty());
}

// ===== QML script errors =====================================================

TEST(TelemetryRules, QmlScriptErrorIsKeyedByFileLineAndKind)
{
    EXPECT_EQ(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/components/GameCard.qml:88: TypeError: "
        "Cannot read property 'name' of null")),
        QStringLiteral("qml|components/GameCard.qml:88|TypeError"));
    // The column and the error's own text do not split the key.
    EXPECT_EQ(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/components/GameCard.qml:88:5: TypeError: "
        "Cannot read property 'cover' of undefined")),
        QStringLiteral("qml|components/GameCard.qml:88|TypeError"));
    EXPECT_EQ(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/screens/HomeScreen.qml:12: ReferenceError: "
        "gameService is not defined")),
        QStringLiteral("qml|screens/HomeScreen.qml:12|ReferenceError"));
    EXPECT_EQ(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/theme/Theme.js:3: SyntaxError: Unexpected token")),
        QStringLiteral("qml|theme/Theme.js:3|SyntaxError"));
}

TEST(TelemetryRules, OnlyScriptErrorsInOurQmlAreReported)
{
    // The live breadcrumb that must stay a breadcrumb: a missing cover image.
    EXPECT_TRUE(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/screens/detail/HeroSection.qml:124:9: QML QQuickImage*: "
        "Error transferring https://cdn.makineceviri.org/assets/banners/2536520.jpg - server "
        "replied with status code 404")).isEmpty());
    EXPECT_TRUE(qmlErrorKey(QStringLiteral(
        "qrc:/qt/qml/MakineLauncher/qml/X.qml:5:3: QML Item: Binding loop detected for "
        "property \"width\"")).isEmpty());
    // Not a QML location at all, even with an error word in it.
    EXPECT_TRUE(qmlErrorKey(QStringLiteral("TypeError in plugin host")).isEmpty());
    EXPECT_TRUE(qmlErrorKey(QStringLiteral("qrc:/x/Main.qml: TypeError: no line")).isEmpty());
}

TEST(TelemetryRules, QmlKeysWaitAWeekOtherKeysADay)
{
    const QString qml = QStringLiteral("qml|components/GameCard.qml:88|TypeError");
    const QString install = sentryKey(QStringLiteral("install"), QStringLiteral("step_failed"),
                                      QStringLiteral("1245620"));
    EXPECT_EQ(sentryRepeatWindowFor(qml), kSentryQmlRepeatWindowMs);
    EXPECT_EQ(sentryRepeatWindowFor(install), kSentryRepeatWindowMs);

    const qint64 now = 1727000000000;
    const qint64 twoDaysAgo = now - 2 * kSentryRepeatWindowMs;
    EXPECT_FALSE(sentryMaySend(twoDaysAgo, now, sentryRepeatWindowFor(qml)));
    EXPECT_TRUE(sentryMaySend(twoDaysAgo, now, sentryRepeatWindowFor(install)));

    // Pruning keeps the QML key for its whole week.
    QVariantMap sent;
    sent.insert(qml, twoDaysAgo);
    sent.insert(install, twoDaysAgo);
    pruneSent(sent, now);
    EXPECT_TRUE(sent.contains(qml));
    EXPECT_FALSE(sent.contains(install));
}

// ===== Failure event tags ====================================================

TEST(TelemetryRules, GameIdOnlyForSubjectsThatNameAGame)
{
    EXPECT_EQ(gameIdFromSubject(QStringLiteral("2536520 (Diablo II: Resurrected)")),
              QStringLiteral("2536520"));
    EXPECT_EQ(gameIdFromSubject(QStringLiteral("manual_3f9a01")), QStringLiteral("manual_3f9a01"));
    // Category words used as subjects by scan/sync/selftest are not games.
    EXPECT_TRUE(gameIdFromSubject(QStringLiteral("empty")).isEmpty());
    EXPECT_TRUE(gameIdFromSubject(QStringLiteral("catalog")).isEmpty());
    EXPECT_TRUE(gameIdFromSubject(QStringLiteral("telemetry")).isEmpty());
    EXPECT_TRUE(gameIdFromSubject(QString()).isEmpty());
}

TEST(TelemetryRules, OneDefectAcrossFolderNamesSharesTheGameTag)
{
    // Eight Diablo II issues in 30 days, one per folder name the users' copies
    // had. The game tag is what server-side grouping keys on, so it must not
    // depend on the name.
    const auto a = failureTags(QStringLiteral("install"),
                               QStringLiteral("2536520 (Diablo II: Resurrected)"),
                               QStringLiteral("system"), QStringLiteral("backup_unknowable"), {});
    const auto b = failureTags(QStringLiteral("install"),
                               QStringLiteral("2536520 (Diablo II Resurrected [FitGirl Repack])"),
                               QStringLiteral("system"), QStringLiteral("backup_unknowable"), {});
    EXPECT_EQ(a.value(QStringLiteral("game.id")), QStringLiteral("2536520"));
    EXPECT_EQ(a.value(QStringLiteral("game.id")), b.value(QStringLiteral("game.id")));
    EXPECT_EQ(a.value(QStringLiteral("fail.reason")), QStringLiteral("backup_unknowable"));
    EXPECT_EQ(a.value(QStringLiteral("operation")), QStringLiteral("install"));
    EXPECT_EQ(a.value(QStringLiteral("failure.side")), QStringLiteral("system"));
}

TEST(TelemetryRules, FailureTagsCarryEverythingTheEventNeeds)
{
    QHash<QString, QString> extra;
    extra.insert(QStringLiteral("install.method"), QStringLiteral("script"));
    extra.insert(QStringLiteral("game.name"), QStringLiteral("Mad Max"));
    const auto t = failureTags(QStringLiteral("install"), QStringLiteral("234140 (Mad Max)"),
                               QStringLiteral("system"), QStringLiteral("step_failed"),
                               QStringLiteral("setSteamLanguage"), extra);
    EXPECT_EQ(t.value(QStringLiteral("subject")), QStringLiteral("234140 (Mad Max)"));
    EXPECT_EQ(t.value(QStringLiteral("fail.step")), QStringLiteral("setSteamLanguage"));
    EXPECT_EQ(t.value(QStringLiteral("install.method")), QStringLiteral("script"));
    EXPECT_EQ(t.value(QStringLiteral("game.name")), QStringLiteral("Mad Max"));
    EXPECT_EQ(t.value(QStringLiteral("game.id")), QStringLiteral("234140"));
}

TEST(TelemetryRules, FailuresWithoutAGameCarryNoGameTag)
{
    // A sync or scan failure must not inherit a game: the scope-wide tag it
    // used to get from the previous install report named the wrong one.
    const auto sync = failureTags(QStringLiteral("sync"), QString(), QStringLiteral("system"),
                                  QStringLiteral("catalog_download"), {});
    EXPECT_FALSE(sync.contains(QStringLiteral("game.id")));
    EXPECT_FALSE(sync.contains(QStringLiteral("subject")));
    EXPECT_FALSE(sync.contains(QStringLiteral("fail.step")));
    const auto scan = failureTags(QStringLiteral("scan"), QStringLiteral("empty"),
                                  QStringLiteral("system"), QStringLiteral("scan_empty"), {});
    EXPECT_FALSE(scan.contains(QStringLiteral("game.id")));
    EXPECT_EQ(scan.value(QStringLiteral("subject")), QStringLiteral("empty"));
}
