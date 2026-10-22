// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

// Rules for the launcher's two telemetry channels, free of network and Sentry
// so they can be tested.
//
// Why two channels. Measured 2026-09-23: Sentry's 5,000-event monthly quota
// filled 6-9 days after each reset. In 30 days 6,592 of 11,617 events were
// dropped and 37 of the last 61 days had no data at all — every field defect
// in those weeks was invisible. 69% of what Sentry did accept were refusals
// the launcher makes on purpose ("this install method is not supported"),
// and the rest repeated: Elden Ring alone sent 744 events from 122 users.
//
// So counting moved to our own endpoint (no quota, retention we control), and
// Sentry keeps what only it can do — the context of a genuine defect — behind
// a gate: one event per (operation, reason, subject) per day, a hard cap per
// session, capability refusals never. Crashes are not gated.

#include <QByteArray>
#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace makine::telemetryrules {

// ---- counting channel (POST /api/v2/telemetry, schema v2) ------------------

struct Event {
    qint64 t = 0;          // ms since epoch
    QString op;            // session, sync, scan, download, install, update, uninstall, restore, backup, integrity, package
    QString out;           // ok, fail, cancel
    QString side;          // failures only: unsupported, user, system
    QString reason;        // failurereasons.h code
    QString app;           // appId when the operation concerns one game
    QVariantMap meta;      // a few small facts; the Worker drops it above 512 bytes
};

inline constexpr int kMaxBatch  = 50;    // Worker contract: 1..50 events per request
inline constexpr int kMaxQueued = 500;   // a machine offline for weeks keeps the newest

/// The anonymous id both channels use, so a Sentry event and the counts from
/// the same install can be joined. Same derivation Sentry's user id always had.
inline QString installId(const QByteArray& machineId)
{
    if (machineId.isEmpty())
        return {};
    return QString::fromLatin1(
        QCryptographicHash::hash(machineId, QCryptographicHash::Sha256).toHex().left(16));
}

inline QJsonObject toJson(const Event& e)
{
    QJsonObject o;
    o[QStringLiteral("t")] = e.t;
    o[QStringLiteral("op")] = e.op;
    o[QStringLiteral("out")] = e.out;
    if (!e.side.isEmpty())   o[QStringLiteral("side")] = e.side;
    if (!e.reason.isEmpty()) o[QStringLiteral("reason")] = e.reason;
    if (!e.app.isEmpty())    o[QStringLiteral("app")] = e.app;
    if (!e.meta.isEmpty())   o[QStringLiteral("meta")] = QJsonObject::fromVariantMap(e.meta);
    return o;
}

inline Event fromJson(const QJsonObject& o)
{
    Event e;
    e.t = o.value(QStringLiteral("t")).toInteger();
    e.op = o.value(QStringLiteral("op")).toString();
    e.out = o.value(QStringLiteral("out")).toString();
    e.side = o.value(QStringLiteral("side")).toString();
    e.reason = o.value(QStringLiteral("reason")).toString();
    e.app = o.value(QStringLiteral("app")).toString();
    e.meta = o.value(QStringLiteral("meta")).toObject().toVariantMap();
    return e;
}

struct Identity {
    QString install;       // installId()
    QString session;       // 32 hex, new per process
    QString version;       // launcher version
    QString channel;       // msix, plain, dev
    QString os;            // kernel version
};

inline QJsonObject buildPayload(const Identity& id, const QList<Event>& events)
{
    QJsonArray arr;
    for (const Event& e : events)
        arr.append(toJson(e));

    QJsonObject body;
    body[QStringLiteral("v")] = 2;
    body[QStringLiteral("install")] = id.install;
    body[QStringLiteral("session")] = id.session;
    body[QStringLiteral("version")] = id.version;
    body[QStringLiteral("channel")] = id.channel;
    body[QStringLiteral("os")] = id.os;
    body[QStringLiteral("events")] = arr;
    return body;
}

/// The app id to record for a failure subject. Install and uninstall failures
/// arrive as "1245620 (Elden Ring)"; others as a bare id or a category word
/// ("catalog"). The Worker rejects an event whose app is not
/// [0-9A-Za-z_.-]{0,32}, so anything else is left out rather than costing the
/// whole event.
inline QString appFromSubject(const QString& subject)
{
    const qsizetype paren = subject.indexOf(QStringLiteral(" ("));
    const QString id = paren >= 0 ? subject.left(paren) : subject;
    if (id.size() > 32)
        return {};
    for (const QChar c : id) {
        const bool ok = (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                     || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                     || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
                     || c == QLatin1Char('_') || c == QLatin1Char('.') || c == QLatin1Char('-');
        if (!ok)
            return {};
    }
    return id;
}

/// Keeps the newest kMaxQueued events.
inline void capQueue(QList<Event>& queue)
{
    if (queue.size() > kMaxQueued)
        queue.erase(queue.begin(), queue.begin() + (queue.size() - kMaxQueued));
}

enum class SendResult { Sent, Retry, Drop };

/// What to do with a batch after the POST finished. A 400 or 413 means the
/// payload itself is wrong, so resending it can never succeed and would only
/// block the batches behind it; anything else (offline, timeout, 5xx, 429)
/// is worth another try.
inline SendResult classifyReply(int httpStatus)
{
    if (httpStatus >= 200 && httpStatus < 300)
        return SendResult::Sent;
    if (httpStatus == 400 || httpStatus == 413)
        return SendResult::Drop;
    return SendResult::Retry;
}

inline QString channel(bool packaged, bool debugBuild)
{
    if (debugBuild)
        return QStringLiteral("dev");
    return packaged ? QStringLiteral("msix") : QStringLiteral("plain");
}

// ---- Sentry gate ------------------------------------------------------------

inline constexpr qint64 kSentryRepeatWindowMs = 24LL * 60 * 60 * 1000;
inline constexpr int    kSentryEventsPerSession = 30;

inline QString sentryKey(const QString& operation, const QString& reason,
                         const QString& subject)
{
    return operation + QLatin1Char('|') + reason + QLatin1Char('|') + subject;
}

/// True when an event whose key was last sent at `lastSentMs` may go out at
/// `nowMs`. A clock that moved backwards opens the gate rather than closing it
/// for however long the skew is.
inline bool sentryMaySend(qint64 lastSentMs, qint64 nowMs)
{
    return lastSentMs <= 0 || nowMs < lastSentMs
        || nowMs - lastSentMs >= kSentryRepeatWindowMs;
}

/// Forgets keys whose window has passed so the persisted map stays small.
inline void pruneSent(QVariantMap& sent, qint64 nowMs)
{
    for (auto it = sent.begin(); it != sent.end();) {
        if (sentryMaySend(it.value().toLongLong(), nowMs))
            it = sent.erase(it);
        else
            ++it;
    }
}

/// The game a failure subject names, or empty. Subjects are "1245620 (Elden
/// Ring)", a bare id, or a category word ("empty", "catalog", "telemetry");
/// a game id always carries a digit (Steam/GOG ids, Epic ids, manual_<hash>),
/// a category word never does.
inline QString gameIdFromSubject(const QString& subject)
{
    const QString id = appFromSubject(subject);
    for (const QChar c : id) {
        if (c.isDigit())
            return id;
    }
    return {};
}

/// The tags one failure event carries — on THAT event only.
///
/// They used to be written into the global Sentry scope, where they stayed
/// for the rest of the session: an uninstall refusal logged later arrived
/// tagged operation=integrity (the previous report), and a crash minutes
/// after a failed install arrived tagged operation=install,
/// failure.side=system and the game — which also fed it to the "Widespread
/// Failure" alert. `extra` (install.method, game.name, …) wins over the
/// derived values.
inline QHash<QString, QString> failureTags(const QString& operation, const QString& subject,
                                           const QString& side, const QString& reason,
                                           const QString& stepAction,
                                           const QHash<QString, QString>& extra = {})
{
    QHash<QString, QString> tags;
    tags.insert(QStringLiteral("operation"), operation);
    if (!subject.isEmpty())
        tags.insert(QStringLiteral("subject"), subject);
    tags.insert(QStringLiteral("failure.side"), side);
    tags.insert(QStringLiteral("fail.reason"), reason);
    if (!stepAction.isEmpty())
        tags.insert(QStringLiteral("fail.step"), stepAction);
    // The bare id, separate from the subject: the subject quotes the folder
    // name the user's copy has, which split one Diablo II defect into eight
    // issues ("Diablo II: Resurrected", "... [FitGirl Repack]", "Diablo"…).
    // Server-side grouping keys on this tag instead.
    if (const QString game = gameIdFromSubject(subject); !game.isEmpty())
        tags.insert(QStringLiteral("game.id"), game);
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        tags.insert(it.key(), it.value());
    return tags;
}

} // namespace makine::telemetryrules
