// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file telemetryservice.cpp
 * @brief Operation-outcome counting implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "telemetryservice.h"
#include "cdnconfig.h"
#include "networksecurity.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMutex>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#include <appmodel.h>
#endif

Q_LOGGING_CATEGORY(lcTelemetry, "makine.manifest")

namespace makine {

namespace {

using telemetryrules::Event;

// Process-wide queue: record() is static so worker threads and CrashReporter
// can reach it without holding a pointer to the service.
QMutex s_mutex;
QList<Event> s_queue;

bool isPackaged()
{
#ifdef Q_OS_WIN
    UINT32 nameLength = 0;
    return ::GetCurrentPackageFullName(&nameLength, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
#else
    return false;
#endif
}

const telemetryrules::Identity& identity()
{
    static const telemetryrules::Identity id = [] {
#ifdef MAKINE_DEV_TOOLS
        constexpr bool devBuild = true;
#else
        constexpr bool devBuild = false;
#endif
        return telemetryrules::Identity{
            telemetryrules::installId(QSysInfo::machineUniqueId()),
            QUuid::createUuid().toString(QUuid::Id128),
            QCoreApplication::applicationVersion(),
            telemetryrules::channel(isPackaged(), devBuild),
            QSysInfo::kernelVersion(),
        };
    }();
    return id;
}

QString queuePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/telemetry-queue.json");
}

void enqueue(Event e)
{
    e.t = QDateTime::currentMSecsSinceEpoch();
    e.app = telemetryrules::appFromSubject(e.app);
    QMutexLocker lock(&s_mutex);
    s_queue.append(std::move(e));
    telemetryrules::capQueue(s_queue);
}

QNetworkRequest makeRequest()
{
    QNetworkRequest req{QUrl{QLatin1String(cdn::kTelemetry)}};
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return req;
}

} // namespace

TelemetryService::TelemetryService(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);

    // Records a previous run could not send (offline, or it quit first).
    QFile file(queuePath());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray saved = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
        QFile::remove(queuePath());
        QList<Event> restored;
        for (const QJsonValue& v : saved)
            restored.append(telemetryrules::fromJson(v.toObject()));
        QMutexLocker lock(&s_mutex);
        s_queue = restored + s_queue;
        telemetryrules::capQueue(s_queue);
    }

    m_flushTimer.setInterval(30000);
    connect(&m_flushTimer, &QTimer::timeout, this, &TelemetryService::flush);
    m_flushTimer.start();
    QTimer::singleShot(5000, this, &TelemetryService::flush);
}

void TelemetryService::record(const QString& op, const QString& out, const QString& app,
                              const QVariantMap& meta)
{
    Event e;
    e.op = op;
    e.out = out;
    e.app = app;
    e.meta = meta;
    enqueue(std::move(e));
}

void TelemetryService::recordFailure(const QString& op, const QString& app,
                                     const QString& side, const QString& reason)
{
    Event e;
    e.op = op;
    e.out = QStringLiteral("fail");
    e.app = app;
    e.side = side;
    e.reason = reason;
    enqueue(std::move(e));
}

void TelemetryService::flush()
{
    if (m_inFlight)
        return;

    QList<Event> batch;
    {
        QMutexLocker lock(&s_mutex);
        const int n = qMin(static_cast<int>(s_queue.size()), telemetryrules::kMaxBatch);
        batch = s_queue.mid(0, n);
        s_queue.remove(0, n);
    }
    if (batch.isEmpty())
        return;

    m_inFlight = true;
    const QByteArray body = QJsonDocument(telemetryrules::buildPayload(identity(), batch))
                                .toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_nam.post(makeRequest(), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, batch]() {
        reply->deleteLater();
        m_inFlight = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        switch (telemetryrules::classifyReply(status)) {
        case telemetryrules::SendResult::Sent: {
            QMutexLocker lock(&s_mutex);
            if (!s_queue.isEmpty())
                QTimer::singleShot(0, this, &TelemetryService::flush);
            break;
        }
        case telemetryrules::SendResult::Drop:
            qCWarning(lcTelemetry) << "Telemetry batch rejected, dropping" << batch.size()
                                   << "records: HTTP" << status;
            break;
        case telemetryrules::SendResult::Retry: {
            QMutexLocker lock(&s_mutex);
            s_queue = batch + s_queue;
            telemetryrules::capQueue(s_queue);
            break;
        }
        }
    });
}

void TelemetryService::persist()
{
    QJsonArray arr;
    {
        QMutexLocker lock(&s_mutex);
        for (const Event& e : std::as_const(s_queue))
            arr.append(telemetryrules::toJson(e));
    }
    if (arr.isEmpty())
        return;

    QDir().mkpath(QFileInfo(queuePath()).absolutePath());
    QSaveFile file(queuePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

int TelemetryService::selfTest()
{
    Event e;
    e.t = QDateTime::currentMSecsSinceEpoch();
    e.op = QStringLiteral("session");
    e.out = QStringLiteral("ok");
    e.meta.insert(QStringLiteral("selftest"), true);

    QNetworkAccessManager nam;
    security::installTlsPinning(&nam);
    QNetworkReply* reply = nam.post(
        makeRequest(),
        QJsonDocument(telemetryrules::buildPayload(identity(), {e})).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    return status;
}

} // namespace makine
