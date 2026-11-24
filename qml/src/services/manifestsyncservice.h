// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QHash>

#include <functional>

#include "patchcompat.h"

class QNetworkReply;

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
    /**
     * @brief Sync again if the last sync started at least @p minAgeMs ago.
     * Quiet: when the catalog turns out unchanged, no catalogReady (which
     * would re-match every game). Run by a timer and when the window
     * regains focus, so a game published in the panel shows up without a
     * restart.
     */
    Q_INVOKABLE void syncIfStale(int minAgeMs);
    QVariantList catalog() const;
    Q_INVOKABLE void fetchPackageDetail(const QString& appId);
    Q_INVOKABLE QVariantMap getPackageDetail(const QString& appId) const;
    Q_INVOKABLE bool hasPackageDetail(const QString& appId) const;
    /// Which patch of a game the detail calls above mean ("" = the default):
    /// the one the catalogue entry describes now (CoreBridge::activePackage).
    /// Unset, every call is about the default.
    void setActivePackageResolver(std::function<QString(const QString&)> resolver);
    QString activePackage(const QString& appId) const;

    /// Steam's latest public build of a game from assets/game-builds.json:
    /// {steamBuild, date}, {} when unknown (file missing, not deployed, no entry).
    Q_INVOKABLE QVariantMap latestGameBuild(const QString& appId) const;
    /// Fetch game-builds.json again (If-None-Match). Not more often than every
    /// 10 minutes; unforced, not more often than every 6 hours.
    void fetchGameBuilds(bool force = false);
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
    /**
     * @brief Stale-While-Revalidate refresh notification.
     * Emitted after a sync that actually changed cached entries (added or
     * version-bumped). Lets the UI surface a 3 sn corner badge without
     * intruding on the user's flow. See docs/specs/swr-cache.md.
     */
    void catalogRefreshed(int totalCount, int changedCount);
    void packageDetailReady(const QString& appId);
    void syncError(const QString& error);
    void offlineChanged();
    /// game-builds.json brought something new
    void gameBuildsChanged();

private:
    void loadCachedGameBuilds();
    void setOffline(bool offline);

    // Sync paths
    void fetchCatalogMeta();
    void handleMetaResponse(const QByteArray& data);
    void fetchCatalogDelta(int sinceVersion);
    void handleDeltaResponse(const QByteArray& data, int sinceVersion);
    void fetchFullCatalog();
    void handleFullCatalogResponse(const QByteArray& data);
    void fallbackToLegacySync();
    /// CatalogStore::parseIndex, plus dropping the in-memory details when the
    /// index itself was replaced (its version or generatedAt moved).
    void parseIndex(const QByteArray& data);
    void startSync(bool quiet);
    /// @p catalogChanged false = the server confirmed the cached catalog.
    void finishSync(bool catalogChanged = true);
    void invalidateChangedDetails();
    /// SWR helper: peek changedCount, drain via invalidateChangedDetails,
    /// then emit catalogRefreshed when count > 0.
    void invalidateAndNotifyRefresh();

    static QString indexUrl();

    // Owned components
    CatalogStore* m_store;
    DetailFetcher* m_detailFetcher;
    TelemetryService* m_telemetry;

    // Sync state
    QNetworkAccessManager m_nam;
    bool m_syncing{false};
    bool m_offline{false};

    // Version reported by the catalog API for the sync in flight; 0 when this
    // sync never got a meta answer. Only this number may be persisted as the
    // local catalog version — see catalogsyncrules.h.
    int  m_serverCatalogVersion{0};
    QTimer m_retryTimer;
    // Periodic re-check while the launcher stays open (see syncIfStale).
    QTimer m_pollTimer;
    QElapsedTimer m_sinceSyncStart;
    bool m_quietSync{false};
    QTimer m_syncTimeoutTimer;
    // Tracks the in-flight sync reply so the timeout can abort it instead
    // of just clearing m_syncing — that previously left a zombie reply
    // racing the next sync attempt (B2-10).
    QPointer<QNetworkReply> m_currentSyncReply;

    std::function<QString(const QString&)> m_activePkg;

    // assets/game-builds.json: latest Steam build per appId (patchcompat.h)
    QHash<QString, patchcompat::LatestBuild> m_gameBuilds;
    QString m_gameBuildsEtag;
    QElapsedTimer m_sinceGameBuildsFetch;
    bool m_gameBuildsInFlight{false};
    QTimer m_gameBuildsTimer;
};

} // namespace makine
