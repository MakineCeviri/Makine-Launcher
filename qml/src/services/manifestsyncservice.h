/**
 * @file manifestsyncservice.h
 * @brief Remote manifest sync from GitHub Assets repository
 * @copyright (c) 2026 MakineAI Team
 *
 * Downloads and caches the translation catalog from GitHub:
 *   - index.json at startup (ETag conditional, ~15KB)
 *   - packages/{appId}.json on-demand (user clicks a game)
 *
 * Provides catalog data for display and delta detection for updates.
 * Works alongside LocalPackageManager which handles install/uninstall.
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QNetworkAccessManager>

namespace makineai {

class ManifestSyncService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isSyncing READ isSyncing NOTIFY syncStatusChanged)
    Q_PROPERTY(int catalogCount READ catalogCount NOTIFY catalogReady)
    Q_PROPERTY(QString lastSyncTime READ lastSyncTime NOTIFY catalogReady)

public:
    explicit ManifestSyncService(QObject* parent = nullptr);

    /**
     * @brief Sync the catalog index from GitHub (ETag conditional).
     * Called at startup and can be called on-demand to refresh.
     * Emits catalogReady() on success, syncError() on failure.
     */
    Q_INVOKABLE void syncCatalog();

    /**
     * @brief Get lightweight catalog entries for display.
     * Each entry: { steamAppId, name, version, sizeBytes }
     */
    QVariantList catalog() const;

    /**
     * @brief Fetch per-game package detail from GitHub (on-demand).
     * Caches locally. Emits packageDetailReady(appId) on success.
     */
    Q_INVOKABLE void fetchPackageDetail(const QString& appId);

    /**
     * @brief Get cached package detail (full JSON from packages/{id}.json).
     * Returns empty map if not yet fetched.
     */
    Q_INVOKABLE QVariantMap getPackageDetail(const QString& appId) const;

    /**
     * @brief Check if per-game detail is already cached locally.
     */
    Q_INVOKABLE bool hasPackageDetail(const QString& appId) const;

    /**
     * @brief Check if a specific appId exists in the remote catalog.
     */
    bool hasCatalogEntry(const QString& appId) const;

    /**
     * @brief Get catalog entry name for a given appId.
     */
    QString catalogGameName(const QString& appId) const;

    bool isSyncing() const { return m_syncing; }
    int catalogCount() const { return m_catalog.size(); }
    QString lastSyncTime() const { return m_lastSync; }

signals:
    void syncStatusChanged();

    /** Emitted when index.json is fetched and parsed successfully */
    void catalogReady();

    /** Emitted when a per-game detail is fetched and cached */
    void packageDetailReady(const QString& appId);

    /** Emitted on network or parse errors */
    void syncError(const QString& error);

private:
    void parseIndex(const QByteArray& data);
    void loadCachedIndex();
    void saveCachedIndex(const QByteArray& rawData, const QString& etag);
    void loadCachedEtag();
    void onIndexFetched(const QByteArray& data, const QString& etag);
    void onDetailFetched(const QString& appId, const QByteArray& data);

    static QString indexUrl();
    static QString packageUrl(const QString& appId);

    QNetworkAccessManager m_nam;
    QString m_etag;
    QString m_lastSync;
    int m_catalogVersion{0};

    struct CatalogEntry {
        QString name;
        QString version;    // "2026-02-21"
        qint64 sizeBytes{0};
    };

    QHash<QString, CatalogEntry> m_catalog;  // appId -> entry
    QHash<QString, QVariantMap> m_packageDetails;  // appId -> full detail (in-memory)

    bool m_syncing{false};
    QSet<QString> m_pendingDetails;  // appIds currently being fetched
};

} // namespace makineai
