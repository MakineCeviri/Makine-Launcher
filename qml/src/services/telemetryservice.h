// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file telemetryservice.h
 * @brief Anonymous operation-outcome counting — the channel without a quota
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Every session, sync, scan, download, install and uninstall ends in one
 * outcome record (ok / fail / cancel, with the failure's reason code) sent in
 * batches to POST /api/v2/telemetry (schema v2, see telemetryrules.h).
 *
 * Sentry cannot do this job: its monthly quota ran out within the first week,
 * and it never saw successes, so no success rate could be computed. Records
 * carry no paths, names or file contents — an anonymous install id, the
 * launcher version and the operation's outcome only.
 */

#pragma once

#include "telemetryrules.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

namespace makine {

class TelemetryService : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryService(QObject* parent = nullptr);

    /// Queue one outcome. Thread-safe and usable before any instance exists:
    /// the library scan finishes on a worker thread, and CrashReporter is static.
    static void record(const QString& op, const QString& out, const QString& app = {},
                       const QVariantMap& meta = {});

    /// Queue a failure with the side and reason code Sentry would give it.
    static void recordFailure(const QString& op, const QString& app, const QString& side,
                              const QString& reason);

    /// Write unsent records to disk; they go out on the next launch.
    void persist();

    /// Dev self-test: send one record synchronously, return the HTTP status (0 = no reply).
    static int selfTest();

private:
    void flush();

    QNetworkAccessManager m_nam;
    QTimer m_flushTimer;
    bool m_inFlight = false;
};

} // namespace makine
