/**
 * @file telemetryservice.cpp
 * @brief Anonymous session telemetry implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "telemetryservice.h"
#include "cdnconfig.h"
#include "networksecurity.h"

#include <QCoreApplication>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QSysInfo>

namespace makine {

TelemetryService::TelemetryService(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);
}

void TelemetryService::sendEvent(const QString& event, const QString& appId)
{
    QJsonObject body;
    body[QStringLiteral("event")] = event;
    body[QStringLiteral("launcher_version")] = QCoreApplication::applicationVersion();
    body[QStringLiteral("os")] = QSysInfo::prettyProductName();
    body[QStringLiteral("locale")] = QLocale::system().name();
    if (!appId.isEmpty())
        body[QStringLiteral("app_id")] = appId;

    QNetworkRequest req{QUrl{QLatin1String(cdn::kTelemetry)}};
    req.setTransferTimeout(5000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void TelemetryService::onSyncComplete(int catalogVersion, int gameCount)
{
    Q_UNUSED(catalogVersion)
    Q_UNUSED(gameCount)
    sendEvent(QStringLiteral("sync"));
}

void TelemetryService::onDownload(const QString& appId)
{
    sendEvent(QStringLiteral("download"), appId);
}

void TelemetryService::onInstall(const QString& appId)
{
    sendEvent(QStringLiteral("install"), appId);
}

void TelemetryService::onUpdate(const QString& appId)
{
    sendEvent(QStringLiteral("update"), appId);
}

} // namespace makine
