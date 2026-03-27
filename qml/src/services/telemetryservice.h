/**
 * @file telemetryservice.h
 * @brief Anonymous session telemetry — fire-and-forget
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Decoupled from sync logic. Sends anonymous, non-PII session data
 * (app version, OS name, locale, game count) after catalog sync.
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>

namespace makine {

class TelemetryService : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryService(QObject* parent = nullptr);

    void onSyncComplete(int catalogVersion, int gameCount);
    void onDownload(const QString& appId);
    void onInstall(const QString& appId);
    void onUpdate(const QString& appId);

private:
    void sendEvent(const QString& event, const QString& appId = {});
    QNetworkAccessManager m_nam;
};

} // namespace makine
