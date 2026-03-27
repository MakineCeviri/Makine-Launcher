/**
 * @file manifestsyncservice.h
 * @brief Catalog sync orchestrator — facade over CatalogStore, DetailFetcher, TelemetryService
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Sync flow: meta check → delta/full/legacy fallback
 * Preserves QML API: ManifestSync context property with Q_PROPERTY + Q_INVOKABLE.
 *
 * Delegates:
 *   - Catalog data & persistence → CatalogStore
 *   - Per-game detail fetch → DetailFetcher
 *   - Session telemetry → TelemetryService
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QTimer>

namespace makine {

class CatalogStore;
class DetailFetcher;
class TelemetryService;

class ManifestSyncService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY syncStatusChanged)
    Q_PROPERTY(bool isOffline READ isOffline NOTIFY offlineChanged)
    Q_PROPERTY(int catalogCount READ catalogCount NOTIFY catalogReady)
    Q_PROPERTY(QString lastSyncTime READ lastSyncTime NOTIFY catalogReady)

public:
    explicit ManifestSyncService(QObject* parent = nullptr);
    ~ManifestSyncService() override;

    // --- QML API (unchanged) ---
    Q_INVOKABLE void syncCatalog();
    QVariantList catalog() const;
    Q_INVOKABLE void fetchPackageDetail(const QString& appId);
    Q_INVOKABLE QVariantMap getPackageDetail(const QString& appId) const;
    Q_INVOKABLE bool hasPackageDetail(const QString& appId) const;
    bool hasCatalogEntry(const QString& appId) const;
    QString catalogGameName(const QString& appId) const;

    TelemetryService* telemetry() const { return m_telemetry; }

    bool isSyncing() const { return m_syncing; }
    bool isOffline() const { return m_offline; }
    int catalogCount() const;
    QString lastSyncTime() const;

signals:
    void syncStatusChanged();
    void catalogReady();
    void packageDetailReady(const QString& appId);
    void syncError(const QString& error);
    void offlineChanged();

private:
    void setOffline(bool offline);

    // Sync paths
    void fetchCatalogMeta();
    void handleMetaResponse(const QByteArray& data);
    void fetchCatalogDelta(int sinceVersion);
    void handleDeltaResponse(const QByteArray& data);
    void fetchFullCatalog();
    void handleFullCatalogResponse(const QByteArray& data);
    void fallbackToLegacySync();
    void finishSync();
    void invalidateChangedDetails();

    static QString indexUrl();

    // Owned components
    CatalogStore* m_store;
    DetailFetcher* m_detailFetcher;
    TelemetryService* m_telemetry;

    // Sync state
    QNetworkAccessManager m_nam;
    bool m_syncing{false};
    bool m_offline{false};
    QTimer m_retryTimer;
    QTimer m_syncTimeoutTimer;
};

} // namespace makine
