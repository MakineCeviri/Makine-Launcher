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

// QML script errors repeat on every binding evaluation and on every machine
// that opens the screen, so their key waits a week, not a day: a broken home
// screen then costs about one event per active install per week (~3K a month
// at 0.1.5's 2.9K monthly users, of a 50K budget) instead of one per install
// per day. The per-session cap keeps one bad delegate from using the
// session's whole allowance.
inline constexpr qint64 kSentryQmlRepeatWindowMs = 7LL * 24 * 60 * 60 * 1000;
inline constexpr int    kSentryQmlErrorsPerSession = 3;
inline constexpr QLatin1StringView kQmlKeyPrefix{"qml|"};

inline qint64 sentryRepeatWindowFor(const QString& key)
{
    return key.startsWith(kQmlKeyPrefix) ? kSentryQmlRepeatWindowMs : kSentryRepeatWindowMs;
}

/// True when an event whose key was last sent at `lastSentMs` may go out at
/// `nowMs`. A clock that moved backwards opens the gate rather than closing it
/// for however long the skew is.
inline bool sentryMaySend(qint64 lastSentMs, qint64 nowMs,
                          qint64 windowMs = kSentryRepeatWindowMs)
{
    return lastSentMs <= 0 || nowMs < lastSentMs
        || nowMs - lastSentMs >= windowMs;
}

/// Forgets keys whose window has passed so the persisted map stays small —
/// each by its own window: a week-long QML key pruned after a day would
/// reopen daily.
inline void pruneSent(QVariantMap& sent, qint64 nowMs)
{
    for (auto it = sent.begin(); it != sent.end();) {
        if (sentryMaySend(it.value().toLongLong(), nowMs, sentryRepeatWindowFor(it.key())))
            it = sent.erase(it);
        else
            ++it;
    }
}

/// The gate key for a QML/JS script error, or empty when `message` is not one.
///
/// Script errors in shipped QML ("qrc:/…/GameCard.qml:88: TypeError: Cannot
/// read property 'name' of null") are UI defects that only ever reached a
/// breadcrumb: the screen half-works and nobody hears about it. Only errors
/// in our own QML count — a URL, a line, then a JS error or a failed
/// assignment. Image and network warnings ("Error transferring …") and
/// binding loops (common, rarely visible) are left to the breadcrumbs. The key is file:line plus the error kind;
/// the column and the error's own text are not, since both vary per object.
inline QString qmlErrorKey(const QString& message)
{
    if (!message.startsWith(QLatin1String("qrc:/")) && !message.startsWith(QLatin1String("file:///")))
        return {};
    static const QLatin1StringView kKinds[] = {
        QLatin1StringView("TypeError"),      QLatin1StringView("ReferenceError"),
        QLatin1StringView("RangeError"),     QLatin1StringView("SyntaxError"),
        QLatin1StringView("URIError"),       QLatin1StringView("Unable to assign"),
        QLatin1StringView("Cannot assign"),  QLatin1StringView("is not a type"),
        QLatin1StringView("Cannot call method"),
    };
    QLatin1StringView kind;
    for (const QLatin1StringView k : kKinds) {
        if (message.contains(k)) {
            kind = k;
            break;
        }
    }
    if (kind.isEmpty())
        return {};

    // "<url>.qml:<line>[:<col>]: …" — keep "<path>.qml:<line>".
    qsizetype ext = message.indexOf(QLatin1String(".qml:"));
    qsizetype extLen = 5;
    if (ext < 0) {
        ext = message.indexOf(QLatin1String(".js:"));
        extLen = 4;
    }
    if (ext < 0)
        return {};
    qsizetype end = ext + extLen;
    while (end < message.size() && message.at(end).isDigit())
        ++end;
    if (end == ext + extLen)
        return {};                                   // no line number: not a location
    const qsizetype schemeEnd = message.indexOf(QLatin1String(":/")) + 2;
    QString location = message.mid(schemeEnd, end - schemeEnd);
    // Long qrc prefixes ("qt/qml/MakineLauncher/qml/") carry nothing.
    if (const qsizetype slash = location.lastIndexOf(QLatin1Char('/'),
                                                     location.lastIndexOf(QLatin1Char('/')) - 1);
        slash >= 0)
        location = location.mid(slash + 1);
    return QString(kQmlKeyPrefix) + location + QLatin1Char('|') + QString(kind);
}

// ---- UI hang watchdog -------------------------------------------------------
//
// A frozen window was invisible to both channels: nothing crashes, the user
// waits or kills the process, and the only trace is a breadcrumb nobody sends.
// The GUI thread beats every half poll; a watchdog thread reports once per
// session when the beat is older than the threshold — Windows' own point for
// "Yanıt vermiyor". Reported while still frozen, so a user who kills the
// process does not take the report with them.

inline constexpr qint64 kHangThresholdMs = 5000;
inline constexpr qint64 kHangPollMs = 1000;

enum class HangCheck { Responsive, Suspended, Hung };

/// `lastPollMs` is the watchdog's own previous wake-up. When the watchdog
/// itself slept far past its period the machine was suspended (sleep, lid,
/// debugger) and the UI has simply not run yet: that is not a hang.
inline HangCheck checkHang(qint64 nowMs, qint64 lastBeatMs, qint64 lastPollMs)
{
    if (nowMs - lastPollMs > 3 * kHangPollMs)
        return HangCheck::Suspended;
    return nowMs - lastBeatMs >= kHangThresholdMs ? HangCheck::Hung : HangCheck::Responsive;
}

/// What the launcher was doing, from its last own breadcrumb: "package:
/// installPackage", "startup: Phase 10". Cut before the ids and counts
/// ("installPackage: 1174180", "Game context: Mad Max (234140)") so one kind
/// of hang is one issue, not one per game.
inline QString hangContext(const QString& category, const QString& message)
{
    qsizetype cut = message.size();
    for (const QChar c : {QLatin1Char(':'), QLatin1Char('='), QLatin1Char('(')}) {
        const qsizetype at = message.indexOf(c);
        if (at >= 0 && at < cut)
            cut = at;
    }
    return category + QLatin1String(": ") + message.left(qMin<qsizetype>(cut, 40)).trimmed();
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
