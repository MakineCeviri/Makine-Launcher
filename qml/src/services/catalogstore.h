/**
 * @file catalogstore.h
 * @brief Catalog data persistence — stores game catalog, version, ETag
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Pure C++ data store (not QObject) for maximum testability.
 * Owns the catalog hash map, version tracking, and disk persistence.
 * Called by ManifestSyncService (orchestrator) — no network code here.
 */

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace makine {

class CatalogStore
{
public:
    struct CatalogEntry {
        QString name;
        QString version;
        qint64 sizeBytes{0};
        qint64 downloadSize{0};
        QString dataUrl;
        QString checksum;
        QString dirName;
        QString externalUrl;
        QString source;
        QString apexTier;
    };

    CatalogStore();

    // --- Catalog access ---
    QVariantList catalog() const;
    bool hasCatalogEntry(const QString& appId) const;
    QString catalogGameName(const QString& appId) const;
    int catalogCount() const;
    bool isEmpty() const;

    // --- Catalog mutation ---
    void replaceCatalog(QHash<QString, CatalogEntry>&& newCatalog);
    bool applyDelta(const QString& appId, const QString& changeType,
                    const QJsonObject& data);
    void invalidateCatalogCache();

    // --- Version / ETag ---
    int catalogVersion() const { return m_catalogVersion; }
    void setCatalogVersion(int version) { m_catalogVersion = version; }
    QString etag() const { return m_etag; }
    void setEtag(const QString& etag) { m_etag = etag; }
    QString lastSyncTime() const { return m_lastSync; }

    // --- Persistence ---
    void loadCachedIndex();
    void saveCachedIndex(const QByteArray& rawData, const QString& etag);
    void loadCachedEtag();
    int loadLocalCatalogVersion() const;
    void saveLocalCatalogVersion(int version);

    // --- Parsing ---
    void parseIndex(const QByteArray& data);
    static CatalogEntry parseCatalogEntry(const QJsonObject& obj);

    // --- Changed tracking (consumed by orchestrator for detail invalidation) ---
    QStringList takeChangedAppIds();

private:
    QHash<QString, CatalogEntry> m_catalog;
    int m_catalogVersion{0};
    QString m_etag;
    QString m_lastSync;
    mutable QVariantList m_catalogCache;
    mutable bool m_catalogCacheValid{false};
    QStringList m_changedAppIds;
};

} // namespace makine
