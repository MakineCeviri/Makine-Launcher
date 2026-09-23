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
