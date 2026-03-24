/**
 * @file manifestsyncservice.cpp
 * @brief Remote manifest sync from Cloudflare R2 CDN
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "manifestsyncservice.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "networksecurity.h"
#include "profiler.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QLocale>
#include <QLoggingCategory>
#include <QSysInfo>

Q_LOGGING_CATEGORY(lcManifestSync, "makine.manifest")

namespace makine {

static constexpr auto CDN_BASE = cdn::kAssetsBase;

ManifestSyncService::ManifestSyncService(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);

    // Load cached index for instant offline catalog display
    loadCachedEtag();
    loadCachedIndex();

    // Retry timer: periodically re-attempt catalog sync when offline
    m_retryTimer.setInterval(15000);  // starts at 15s, exponential backoff
    connect(&m_retryTimer, &QTimer::timeout, this, [this]() {
        // Exponential backoff: 15s → 30s → 60s → 120s → max 300s
        const int next = qMin(m_retryTimer.interval() * 2, 300000);
        m_retryTimer.setInterval(next);
        syncCatalog();
    });
}

// ========== URL Helpers ==========

QString ManifestSyncService::indexUrl()
{
    return QLatin1String(CDN_BASE) + QStringLiteral("index.json");
}

QString ManifestSyncService::packageUrl(const QString& appId)
{
    // Use new API endpoint (faster, KV cached) with CDN fallback
    return QLatin1String(cdn::kGameDetail) + appId;
}

// ========== Catalog Sync ==========

void ManifestSyncService::syncCatalog()
{
    MAKINE_ZONE_NAMED("ManifestSync::syncCatalog");
    CrashReporter::addBreadcrumb("manifest", "ManifestSync::syncCatalog");

    if (m_syncing)
        return;

    m_syncing = true;
    emit syncStatusChanged();

    // Delta sync: first check meta endpoint (~100 bytes, <5ms)
    fetchCatalogMeta();
}

// ========== Delta Sync Flow ==========

void ManifestSyncService::fetchCatalogMeta()
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchCatalogMeta");

    QNetworkRequest req{QUrl{QLatin1String(cdn::kCatalogMeta)}};
    req.setTransferTimeout(8000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Makine-Launcher/0.1"));

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcManifestSync) << "ManifestSync: API meta failed, falling back to legacy";
            fallbackToLegacySync();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            handleMetaResponse(reply->readAll());
        } else {
            fallbackToLegacySync();
        }
    });
}

void ManifestSyncService::handleMetaResponse(const QByteArray& data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) { fallbackToLegacySync(); return; }

    const QJsonObject root = doc.object();
    if (!root[QStringLiteral("success")].toBool()) { fallbackToLegacySync(); return; }

    const QJsonObject meta = root[QStringLiteral("data")].toObject();
    const int serverVersion = meta[QStringLiteral("version")].toInt();
    const int localVersion = loadLocalCatalogVersion();

    qCDebug(lcManifestSync) << "ManifestSync: server v" << serverVersion << "local v" << localVersion;

    if (serverVersion <= localVersion && !m_catalog.isEmpty()) {
        // Up to date — no download needed
        qCDebug(lcManifestSync) << "ManifestSync: catalog is current";
        m_syncing = false;
        emit syncStatusChanged();
        setOffline(false);
        QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
        sendTelemetry();
        return;
    }

    if (localVersion > 0 && (serverVersion - localVersion) <= 50) {
        // Delta sync — only changed games
        fetchCatalogDelta(localVersion);
    } else {
        // Full catalog fetch
        fetchFullCatalog();
    }
}

void ManifestSyncService::fetchCatalogDelta(int sinceVersion)
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchCatalogDelta");

    const QString url = QLatin1String(cdn::kCatalogDelta) + QStringLiteral("?since=%1").arg(sinceVersion);
    QNetworkRequest req{QUrl{url}};
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Makine-Launcher/0.1"));

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcManifestSync) << "ManifestSync: delta failed, fetching full catalog";
            fetchFullCatalog();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 410) {
            // Too far behind — need full fetch
            fetchFullCatalog();
            return;
        }
        if (status >= 200 && status < 300) {
            handleDeltaResponse(reply->readAll());
        } else {
            fetchFullCatalog();
        }
    });
}

void ManifestSyncService::handleDeltaResponse(const QByteArray& data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) { fetchFullCatalog(); return; }

    const QJsonObject root = doc.object();
    if (!root[QStringLiteral("success")].toBool()) { fetchFullCatalog(); return; }

    const QJsonObject deltaData = root[QStringLiteral("data")].toObject();
    const int toVersion = deltaData[QStringLiteral("toVersion")].toInt();
    const QJsonArray changes = deltaData[QStringLiteral("changes")].toArray();

    qCDebug(lcManifestSync) << "ManifestSync: delta —" << changes.size() << "changes";

    const QString detailDir = AppPaths::packageDetailDir();

    for (const QJsonValue& val : changes) {
        const QJsonObject change = val.toObject();
        const QString appId = change[QStringLiteral("appId")].toString();
        const QString changeType = change[QStringLiteral("changeType")].toString();

        if (changeType == QStringLiteral("delete")) {
            m_catalog.remove(appId);
            m_packageDetails.remove(appId);
            m_diskDetailCache.remove(appId);
            QFile::remove(detailDir + QStringLiteral("/%1.json").arg(appId));
        } else {
            // add or update
            const QJsonObject entry = change[QStringLiteral("data")].toObject();
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
            m_catalog.insert(appId, ce);

            // Invalidate detail cache for updated games
            m_packageDetails.remove(appId);
            m_diskDetailCache.remove(appId);
            QFile::remove(detailDir + QStringLiteral("/%1.json").arg(appId));
        }
    }

    m_catalogCacheValid = false;
    saveLocalCatalogVersion(toVersion);

    m_syncing = false;
    emit syncStatusChanged();
    setOffline(false);
    QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
    sendTelemetry();

    qCDebug(lcManifestSync) << "ManifestSync: delta applied — now v" << toVersion << "with" << m_catalog.size() << "games";
}

void ManifestSyncService::fetchFullCatalog()
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchFullCatalog");

    QNetworkRequest req{QUrl{QLatin1String(cdn::kCatalogUrl)}};
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Makine-Launcher/0.1"));

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 2 * 1024 * 1024) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcManifestSync) << "ManifestSync: full catalog API failed, falling back to legacy";
            fallbackToLegacySync();
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            handleFullCatalogResponse(reply->readAll());
        } else {
            fallbackToLegacySync();
        }
    });
}

void ManifestSyncService::handleFullCatalogResponse(const QByteArray& data)
{
    // API response wraps catalog directly (same format as legacy index.json)
    parseIndex(data);

    if (!m_catalog.isEmpty()) {
        saveCachedIndex(data, QString());
        const int version = m_catalogVersion;
        if (version > 0)
            saveLocalCatalogVersion(version);

        qCDebug(lcManifestSync) << "ManifestSync: full catalog synced —" << m_catalog.size() << "packages, v" << version;
        setOffline(false);
        QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
        sendTelemetry();
    } else {
        fallbackToLegacySync();
    }
}

void ManifestSyncService::fallbackToLegacySync()
{
    qCDebug(lcManifestSync) << "ManifestSync: falling back to legacy CDN sync";

    QNetworkRequest req{QUrl{indexUrl()}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1"));

    if (!m_etag.isEmpty())
        req.setRawHeader("If-None-Match", m_etag.toUtf8());

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 1 * 1024 * 1024) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_syncing = false;
        emit syncStatusChanged();

        if (reply->error() != QNetworkReply::NoError) {
            if (m_catalog.isEmpty()) {
                setOffline(true);
                emit syncError(tr("Katalog indirilemedi: %1").arg(reply->errorString()));
            }
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 304) {
            setOffline(false);
            QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
            return;
        }

        if (status >= 200 && status < 300) {
            const QByteArray data = reply->readAll();
            const QString etag = reply->rawHeader("ETag");
            onIndexFetched(data, etag);
            return;
        }

        emit syncError(tr("Katalog alınamadı (HTTP %1)").arg(status));
    });
}

// ========== Telemetry ==========

void ManifestSyncService::sendTelemetry()
{
    // Anonymous session telemetry — fire and forget
    QJsonObject body;
    body[QStringLiteral("version")] = QStringLiteral("0.1.0");
    body[QStringLiteral("os")] = QSysInfo::prettyProductName();
    body[QStringLiteral("locale")] = QLocale::system().name();
    body[QStringLiteral("gamesInstalled")] = 0; // TODO: get from LocalPackageManager

    QNetworkRequest req{QUrl{QLatin1String(cdn::kTelemetry)}};
    req.setTransferTimeout(5000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

// ========== Catalog Version Persistence ==========

int ManifestSyncService::loadLocalCatalogVersion() const
{
    QFile f(AppPaths::cacheDir() + QStringLiteral("/catalog_version.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    bool ok = false;
    const int v = f.readAll().trimmed().toInt(&ok);
    return ok ? v : 0;
}

void ManifestSyncService::saveLocalCatalogVersion(int version)
{
    QFile f(AppPaths::cacheDir() + QStringLiteral("/catalog_version.txt"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    f.write(QByteArray::number(version));
}

void ManifestSyncService::onIndexFetched(const QByteArray& data, const QString& etag)
{
    MAKINE_ZONE_NAMED("ManifestSync::onIndexFetched");

    parseIndex(data);

    if (!m_catalog.isEmpty()) {
        saveCachedIndex(data, etag);
        m_etag = etag;
        qCDebug(lcManifestSync) << "ManifestSync: catalog synced —" << m_catalog.size() << "packages";
        setOffline(false);
        // Defer signal to next event loop iteration — breaks synchronous chain
        // that otherwise blocks main thread ~4s (refreshPackageManifest + QML rebind)
        QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
    } else {
        emit syncError(tr("Katalog verisi boş veya geçersiz"));
    }
}

void ManifestSyncService::parseIndex(const QByteArray& data)
{
    MAKINE_ZONE_NAMED("ManifestSync::parseIndex");

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcManifestSync) << "ManifestSync: index.json parse error:" << err.errorString();
        return;
    }

    const QJsonObject root = doc.object();
    m_catalogVersion = root[QStringLiteral("version")].toInt();
    m_lastSync = root[QStringLiteral("generatedAt")].toString();

    const QJsonObject packages = root[QStringLiteral("packages")].toObject();

    QHash<QString, CatalogEntry> newCatalog;
    newCatalog.reserve(packages.size());

    for (auto it = packages.constBegin(); it != packages.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
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
        newCatalog.insert(it.key(), ce);
    }

    // Invalidate detail cache for packages whose version changed
    const QString detailDir = AppPaths::packageDetailDir();
    for (auto it = newCatalog.constBegin(); it != newCatalog.constEnd(); ++it) {
        auto old = m_catalog.constFind(it.key());
        if (old != m_catalog.constEnd() && old->version != it->version) {
            m_packageDetails.remove(it.key());
            m_diskDetailCache.remove(it.key());
            QFile::remove(detailDir + QStringLiteral("/%1.json").arg(it.key()));
            qCDebug(lcManifestSync) << "ManifestSync: invalidated detail cache for" << it.key()
                     << "(version" << old->version << "->" << it->version << ")";
        }
    }

    m_catalog = std::move(newCatalog);
    m_catalogCacheValid = false;  // Invalidate catalog() cache on every parse
}

// ========== Catalog Query ==========

QVariantList ManifestSyncService::catalog() const
{
    // Return cached result if catalog hasn't changed since last call.
    // parseIndex() sets m_catalogCacheValid = false on every catalog update.
    // Avoids: 260+ QVariantMap + QString key allocations per call.
    if (m_catalogCacheValid)
        return m_catalogCache;

    MAKINE_ZONE_NAMED("ManifestSync::catalog (rebuild)");

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

        // Download metadata (available after pipeline processing)
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

bool ManifestSyncService::hasCatalogEntry(const QString& appId) const
{
    return m_catalog.contains(appId);
}

QString ManifestSyncService::catalogGameName(const QString& appId) const
{
    auto it = m_catalog.constFind(appId);
    return (it != m_catalog.constEnd()) ? it->name : QString();
}

// ========== Per-Game Detail ==========

void ManifestSyncService::fetchPackageDetail(const QString& appId)
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchPackageDetail");

    if (appId.isEmpty() || m_pendingDetails.contains(appId))
        return;

    // Check in-memory cache first
    if (m_packageDetails.contains(appId)) {
        emit packageDetailReady(appId);
        return;
    }

    // Check disk cache
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    if (QFile::exists(cachePath)) {
        QFile file(cachePath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                m_packageDetails.insert(appId, doc.object().toVariantMap());
                emit packageDetailReady(appId);
                return;
            }
        }
    }

    // Fetch from CDN
    m_pendingDetails.insert(appId);

    QNetworkRequest req{QUrl{packageUrl(appId)}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1"));

    QNetworkReply* reply = m_nam.get(req);

    // Abort if response exceeds 1 MB (package detail is ~700 B normally)
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 1 * 1024 * 1024) {
            qCWarning(lcManifestSync) << "ManifestSync: package detail response too large, aborting";
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, appId]() {
        reply->deleteLater();
        m_pendingDetails.remove(appId);

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcManifestSync) << "ManifestSync: package detail network error for" << appId
                       << reply->errorString();
            emit packageDetailReady(appId);  // Unblock waiting QML
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            onDetailFetched(appId, reply->readAll());
        } else {
            qCWarning(lcManifestSync) << "ManifestSync: package detail HTTP" << status << "for" << appId;
            emit packageDetailReady(appId);  // Unblock waiting QML
        }
    });
}

void ManifestSyncService::onDetailFetched(const QString& appId, const QByteArray& data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcManifestSync) << "ManifestSync: invalid JSON for package detail" << appId;
        emit packageDetailReady(appId);  // Unblock waiting QML
        return;
    }

    // Cache in memory
    m_packageDetails.insert(appId, doc.object().toVariantMap());

    // Cache to disk + populate disk-existence set (avoids future QFile::exists calls)
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        m_diskDetailCache.insert(appId);
    }

    emit packageDetailReady(appId);
}

QVariantMap ManifestSyncService::getPackageDetail(const QString& appId) const
{
    return m_packageDetails.value(appId);
}

bool ManifestSyncService::hasPackageDetail(const QString& appId) const
{
    // Hot path: in-memory cache hit (O(1), no disk I/O)
    if (m_packageDetails.contains(appId))
        return true;

    // Second hot path: previously confirmed disk hit (O(1), no syscall)
    if (m_diskDetailCache.contains(appId))
        return true;

    // Cold path: actual disk check — populate m_diskDetailCache to avoid repeat syscalls
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    const bool exists = QFile::exists(cachePath);
    if (exists)
        const_cast<ManifestSyncService*>(this)->m_diskDetailCache.insert(appId);
    return exists;
}

// ========== Local Cache Persistence ==========

void ManifestSyncService::loadCachedIndex()
{
    MAKINE_ZONE_NAMED("ManifestSync::loadCachedIndex");

    const QString path = AppPaths::manifestIndexFile();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    parseIndex(file.readAll());

    if (!m_catalog.isEmpty()) {
        qCDebug(lcManifestSync) << "ManifestSync: loaded cached index —" << m_catalog.size() << "packages";
    }
}

void ManifestSyncService::saveCachedIndex(const QByteArray& rawData, const QString& etag)
{
    // Save raw index.json
    QFile indexFile(AppPaths::manifestIndexFile());
    if (indexFile.open(QIODevice::WriteOnly))
        indexFile.write(rawData);

    // Save ETag separately
    if (!etag.isEmpty()) {
        QFile etagFile(AppPaths::manifestEtagFile());
        if (etagFile.open(QIODevice::WriteOnly))
            etagFile.write(etag.toUtf8());
    }
}

void ManifestSyncService::loadCachedEtag()
{
    QFile file(AppPaths::manifestEtagFile());
    if (file.open(QIODevice::ReadOnly))
        m_etag = QString::fromUtf8(file.readAll()).trimmed();
}

void ManifestSyncService::setOffline(bool offline)
{
    if (m_offline == offline)
        return;
    m_offline = offline;
    emit offlineChanged();

    if (offline) {
        if (!m_retryTimer.isActive())
            m_retryTimer.start();
        qCDebug(lcManifestSync) << "ManifestSync: offline mode — retrying every 15s";
    } else {
        m_retryTimer.stop();
        m_retryTimer.setInterval(15000);  // reset backoff on success
        qCDebug(lcManifestSync) << "ManifestSync: back online";
    }
}

} // namespace makine
