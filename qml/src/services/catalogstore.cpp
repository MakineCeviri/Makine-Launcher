// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file catalogstore.cpp
 * @brief Catalog data persistence implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "catalogstore.h"
#include "apppaths.h"
#include "cdnconfig.h"
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
        // No package of ours on the CDN (empty dataUrl, or one that points at
        // another site): the card says "Yakında" and nothing is downloaded.
        entry.insert(QStringLiteral("comingSoon"), !cdn::isCdnUrl(ce.dataUrl));
        if (!ce.checksum.isEmpty())
            entry.insert(QStringLiteral("checksum"), ce.checksum);
        if (!ce.dirName.isEmpty())
            entry.insert(QStringLiteral("dirName"), ce.dirName);
        if (!ce.source.isEmpty())
            entry.insert(QStringLiteral("source"), ce.source);
        if (!ce.contributors.isEmpty())
            entry.insert(QStringLiteral("contributors"), ce.contributors);
        if (!ce.label.isEmpty())
            entry.insert(QStringLiteral("label"), ce.label);
        if (!ce.gameVersion.isEmpty())
            entry.insert(QStringLiteral("gameVersion"), ce.gameVersion);
        if (!ce.alternatives.isEmpty()) {
            QVariantList alts;
            alts.reserve(ce.alternatives.size());
            for (const QVariant& a : ce.alternatives) {
                QVariantMap m = a.toMap();
                m.insert(QStringLiteral("comingSoon"),
                         !cdn::isCdnUrl(m.value(QStringLiteral("dataUrl")).toString()));
                alts.append(m);
            }
            entry.insert(QStringLiteral("alternatives"), alts);
        }

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
    ce.source = entry[QStringLiteral("source")].toString();

    // Parse inline contributors from index.json
    const QJsonArray contribs = entry[QStringLiteral("contributors")].toArray();
    for (const auto& c : contribs) {
        if (!c.isObject()) continue;
        const QJsonObject co = c.toObject();
        ce.contributors.append(QVariantMap{
            {QStringLiteral("name"), co[QStringLiteral("name")].toString()},
            {QStringLiteral("role"), co[QStringLiteral("role")].toString()}
        });
    }

    ce.label = entry[QStringLiteral("label")].toString();
    ce.gameVersion = parseGameVersion(entry[QStringLiteral("gameVersion")]);
    ce.alternatives = parseAlternatives(entry[QStringLiteral("alternatives")]);

    return ce;
}

namespace {

// ^[a-z0-9][a-z0-9-]{0,31}$ and never "default" (no <regex> on MinGW)
bool validAlternativeId(const QString& id)
{
    if (id.isEmpty() || id.size() > 32 || id == QLatin1String("default"))
        return false;
    for (qsizetype i = 0; i < id.size(); ++i) {
        const QChar c = id.at(i);
        const bool alnum = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                        || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        if (!alnum && !(i > 0 && c == QLatin1Char('-')))
            return false;
    }
    return true;
}

qint64 positiveInt(const QJsonValue& v)
{
    if (!v.isDouble()) return 0;
    const double d = v.toDouble();
    return (d > 0 && d < 9.0e15) ? static_cast<qint64>(d) : 0;
}

} // namespace

QVariantMap CatalogStore::parseGameVersion(const QJsonValue& value)
{
    if (!value.isObject())
        return {};
    const QJsonObject o = value.toObject();
    const QString label = o[QStringLiteral("label")].toString();
    const qint64 build = positiveInt(o[QStringLiteral("steamBuild")]);
    const QString date = o[QStringLiteral("date")].toString();
    if (label.isEmpty() && build == 0 && date.isEmpty())
        return {};
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("steamBuild"), build},
        {QStringLiteral("date"), date},
    };
}

QVariantList CatalogStore::parseAlternatives(const QJsonValue& value)
{
    QVariantList out;
    if (!value.isArray())
        return out;
    QStringList seen;
    for (const QJsonValue& v : value.toArray()) {
        if (!v.isObject()) continue;
        const QJsonObject a = v.toObject();
        const QString id = a[QStringLiteral("id")].toString();
        const QString label = a[QStringLiteral("label")].toString();
        if (!validAlternativeId(id) || label.isEmpty() || seen.contains(id))
            continue;
        seen.append(id);

        QVariantList contributors;
        for (const QJsonValue& c : a[QStringLiteral("contributors")].toArray()) {
            if (!c.isObject()) continue;
            const QJsonObject co = c.toObject();
            contributors.append(QVariantMap{
                {QStringLiteral("name"), co[QStringLiteral("name")].toString()},
                {QStringLiteral("role"), co[QStringLiteral("role")].toString()}});
        }
        out.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("description"), a[QStringLiteral("description")].toString()},
            {QStringLiteral("version"), a[QStringLiteral("v")].toString()},
            {QStringLiteral("sizeBytes"), positiveInt(a[QStringLiteral("sizeBytes")])},
            {QStringLiteral("downloadSize"), positiveInt(a[QStringLiteral("size")])},
            {QStringLiteral("dataUrl"), a[QStringLiteral("dataUrl")].toString()},
            {QStringLiteral("checksum"), a[QStringLiteral("checksum")].toString()},
            {QStringLiteral("dirName"), a[QStringLiteral("dirName")].toString()},
            {QStringLiteral("contributors"), contributors},
            {QStringLiteral("gameVersion"), parseGameVersion(a[QStringLiteral("gameVersion")])},
        });
        if (out.size() >= 20) break;
    }
    return out;
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

    // "soon": no package of ours yet. Never carries a dataUrl, so comingSoon;
    // an appId also in "packages" keeps the package.
    const QJsonObject soon = root[QStringLiteral("soon")].toObject();
    for (auto it = soon.constBegin(); it != soon.constEnd(); ++it) {
        if (newCatalog.contains(it.key()))
            continue;
        CatalogEntry ce = parseCatalogEntry(it.value().toObject());
        ce.dataUrl.clear();
        ce.checksum.clear();
        newCatalog.insert(it.key(), std::move(ce));
    }

    // Track version-changed packages for detail invalidation
    for (auto it = newCatalog.constBegin(); it != newCatalog.constEnd(); ++it) {
        auto old = m_catalog.constFind(it.key());
        // An alternative's new version invalidates its detail too
        const auto altVersions = [](const QVariantList& alts) {
            QStringList out;
            for (const QVariant& a : alts) {
                const QVariantMap m = a.toMap();
                out << m.value(QStringLiteral("id")).toString() + QLatin1Char('=')
                           + m.value(QStringLiteral("version")).toString();
            }
            return out;
        };
        if (old != m_catalog.constEnd()
            && (old->version != it->version
                || altVersions(old->alternatives) != altVersions(it->alternatives))) {
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
