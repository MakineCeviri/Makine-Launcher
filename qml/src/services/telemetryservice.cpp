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

void TelemetryService::onSyncComplete(int catalogVersion, int gameCount)
{
    QJsonObject body;
    body[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    body[QStringLiteral("os")] = QSysInfo::prettyProductName();
    body[QStringLiteral("locale")] = QLocale::system().name();
    body[QStringLiteral("catalogVersion")] = catalogVersion;
    body[QStringLiteral("gamesInstalled")] = gameCount;

    QNetworkRequest req{QUrl{QLatin1String(cdn::kTelemetry)}};
    req.setTransferTimeout(5000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

} // namespace makine
