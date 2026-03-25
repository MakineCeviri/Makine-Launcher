/**
 * @file catalogstore.cpp
 * @brief Catalog data persistence implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "catalogstore.h"
#include "apppaths.h"
#include "profiler.h"

#include <makine/path_utils.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCatalogStore, "makine.manifest")

namespace makine {

CatalogStore::CatalogStore() = default;

// ========== Catalog Access ==========

QVariantList CatalogStore::catalog() const
{
    if (m_catalogCacheValid)
        return m_catalogCache;

    MAKINE_ZONE_NAMED("CatalogStore::catalog (rebuild)");

    m_catalogCache.clear();
    m_catalogCache.reserve(m_catalog.size());

    for (auto it = m_catalog.constBegin(); it != m_catalog.constEnd(); ++it) {
        const auto& ce = it.value();
        QVariantMap entry{
            {QStringLiteral("steamAppId"), it.key()},
            {QStringLiteral("name"), ce.name},
            {QStringLiteral("gameName"), ce.name},
            {QStringLiteral("version"), ce.version},
            {QStringLiteral("sizeBytes"), ce.sizeBytes},
            {QStringLiteral("hasTranslation"), true},
            {QStringLiteral("isVerified"), true},
        };

        if (ce.downloadSize > 0)
            entry.insert(QStringLiteral("downloadSize"), ce.downloadSize);
        if (!ce.dataUrl.isEmpty())
            entry.insert(QStringLiteral("dataUrl"), ce.dataUrl);
        if (!ce.checksum.isEmpty())
            entry.insert(QStringLiteral("checksum"), ce.checksum);
        if (!ce.dirName.isEmpty())
            entry.insert(QStringLiteral("dirName"), ce.dirName);
        if (!ce.externalUrl.isEmpty())
            entry.insert(QStringLiteral("externalUrl"), ce.externalUrl);
        if (!ce.source.isEmpty())
            entry.insert(QStringLiteral("source"), ce.source);
        if (!ce.apexTier.isEmpty())
            entry.insert(QStringLiteral("apexTier"), ce.apexTier);

        m_catalogCache.append(entry);
    }

    m_catalogCacheValid = true;
    return m_catalogCache;
}

bool CatalogStore::hasCatalogEntry(const QString& appId) const
{
    return m_catalog.contains(appId);
}

QString CatalogStore::catalogGameName(const QString& appId) const
{
    auto it = m_catalog.constFind(appId);
    return (it != m_catalog.constEnd()) ? it->name : QString();
}

int CatalogStore::catalogCount() const { return m_catalog.size(); }
bool CatalogStore::isEmpty() const { return m_catalog.isEmpty(); }

// ========== Catalog Mutation ==========

void CatalogStore::replaceCatalog(QHash<QString, CatalogEntry>&& newCatalog)
{
    m_catalog = std::move(newCatalog);
    m_catalogCacheValid = false;
}

bool CatalogStore::applyDelta(const QString& appId, const QString& changeType,
                              const QJsonObject& data)
{
    if (appId.isEmpty()) {
        qCWarning(lcCatalogStore) << "CatalogStore: rejecting delta with empty appId";
        return false;
    }

    if (makine::path::containsTraversalPattern(appId.toStdString())) {
        qCWarning(lcCatalogStore) << "CatalogStore: rejecting traversal in appId:" << appId;
        return false;
    }

    if (changeType == QStringLiteral("delete")) {
        m_catalog.remove(appId);
    } else if (changeType == QStringLiteral("add") || changeType == QStringLiteral("update")) {
        m_catalog.insert(appId, parseCatalogEntry(data));
    } else {
        qCWarning(lcCatalogStore) << "CatalogStore: unknown changeType:" << changeType
                                  << "for appId:" << appId;
        return false;
    }

    m_changedAppIds.append(appId);
    m_catalogCacheValid = false;
    return true;
}

void CatalogStore::invalidateCatalogCache()
{
    m_catalogCacheValid = false;
}

// ========== Parsing ==========

CatalogStore::CatalogEntry CatalogStore::parseCatalogEntry(const QJsonObject& entry)
{
    CatalogEntry ce;
    ce.name = entry[QStringLiteral("name")].toString();
    ce.version = entry[QStringLiteral("v")].toString();
    ce.sizeBytes = static_cast<qint64>(entry[QStringLiteral("sizeBytes")].toDouble());
    ce.downloadSize = static_cast<qint64>(entry[QStringLiteral("size")].toDouble());
    ce.dataUrl = entry[QStringLiteral("dataUrl")].toString();
    ce.checksum = entry[QStringLiteral("checksum")].toString();
    ce.dirName = entry[QStringLiteral("dirName")].toString();
    ce.externalUrl = entry[QStringLiteral("externalUrl")].toString();
    ce.source = entry[QStringLiteral("source")].toString();
    ce.apexTier = entry[QStringLiteral("apexTier")].toString();
    return ce;
}

void CatalogStore::parseIndex(const QByteArray& data)
{
    MAKINE_ZONE_NAMED("CatalogStore::parseIndex");

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcCatalogStore) << "CatalogStore: index parse error:" << err.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    m_catalogVersion = root[QStringLiteral("version")].toInt();
    m_lastSync = root[QStringLiteral("generatedAt")].toString();

    const QJsonObject packages = root[QStringLiteral("packages")].toObject();

    QHash<QString, CatalogEntry> newCatalog;
    newCatalog.reserve(packages.size());

    for (auto it = packages.constBegin(); it != packages.constEnd(); ++it) {
        newCatalog.insert(it.key(), parseCatalogEntry(it.value().toObject()));
    }

    // Track version-changed packages for detail invalidation
    for (auto it = newCatalog.constBegin(); it != newCatalog.constEnd(); ++it) {
        auto old = m_catalog.constFind(it.key());
        if (old != m_catalog.constEnd() && old->version != it->version) {
            m_changedAppIds.append(it.key());
            qCDebug(lcCatalogStore) << "CatalogStore: version changed for" << it.key()
                                    << "(" << old->version << "->" << it->version << ")";
        }
    }

    m_catalog = std::move(newCatalog);
    m_catalogCacheValid = false;
}

// ========== Changed Tracking ==========

QStringList CatalogStore::takeChangedAppIds()
{
    QStringList result;
    result.swap(m_changedAppIds);
    return result;
}

// ========== Persistence ==========

void CatalogStore::loadCachedIndex()
{
    MAKINE_ZONE_NAMED("CatalogStore::loadCachedIndex");

    const QString path = AppPaths::manifestIndexFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    parseIndex(file.readAll());

    if (!m_catalog.isEmpty()) {
        qCDebug(lcCatalogStore) << "CatalogStore: loaded cached index —"
                                << m_catalog.size() << "packages";
    }
}

void CatalogStore::saveCachedIndex(const QByteArray& rawData, const QString& etag)
{
    QFile indexFile(AppPaths::manifestIndexFile());
    if (indexFile.open(QIODevice::WriteOnly))
        indexFile.write(rawData);

    if (!etag.isEmpty()) {
        QFile etagFile(AppPaths::manifestEtagFile());
        if (etagFile.open(QIODevice::WriteOnly))
            etagFile.write(etag.toUtf8());
    }
}

void CatalogStore::loadCachedEtag()
{
    QFile file(AppPaths::manifestEtagFile());
    if (file.open(QIODevice::ReadOnly))
        m_etag = QString::fromUtf8(file.readAll()).trimmed();
}

int CatalogStore::loadLocalCatalogVersion() const
{
    QFile f(AppPaths::cacheDir() + QStringLiteral("/catalog_version.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    bool ok = false;
    const int v = f.readAll().trimmed().toInt(&ok);
    return ok ? v : 0;
}

void CatalogStore::saveLocalCatalogVersion(int version)
{
    // Atomic write: temp file + rename to prevent corruption on crash
    const QString path = AppPaths::cacheDir() + QStringLiteral("/catalog_version.txt");
    const QString tmpPath = path + QStringLiteral(".tmp");

    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    f.write(QByteArray::number(version));
    f.close();

    QFile::remove(path);
    QFile::rename(tmpPath, path);
}

} // namespace makine
