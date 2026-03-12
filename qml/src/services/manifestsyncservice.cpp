/**
 * @file manifestsyncservice.cpp
 * @brief Remote manifest sync from Cloudflare R2 CDN
 * @copyright (c) 2026 MakineAI Team
 */

#include "manifestsyncservice.h"
#include "apppaths.h"
#include "cdnconfig.h"
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
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcManifestSync, "makineai.manifest")

namespace makineai {

static constexpr auto CDN_BASE = cdn::kAssetsBase;

ManifestSyncService::ManifestSyncService(QObject* parent)
    : QObject(parent)
{
    // Load cached index for instant offline catalog display
    loadCachedEtag();
    loadCachedIndex();

    // Retry timer: periodically re-attempt catalog sync when offline
    m_retryTimer.setInterval(15000);  // 15 seconds
    connect(&m_retryTimer, &QTimer::timeout, this, &ManifestSyncService::syncCatalog);
}

// ========== URL Helpers ==========

QString ManifestSyncService::indexUrl()
{
    return QLatin1String(CDN_BASE) + QStringLiteral("index.json");
}

QString ManifestSyncService::packageUrl(const QString& appId)
{
    return QLatin1String(CDN_BASE) + QStringLiteral("packages/%1.json").arg(appId);
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

    QNetworkRequest req{QUrl{indexUrl()}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) MakineAI/0.1"));

    // ETag conditional request — if unchanged, server returns 304
    if (!m_etag.isEmpty())
        req.setRawHeader("If-None-Match", m_etag.toUtf8());

    QNetworkReply* reply = m_nam.get(req);

    // Abort if response exceeds 1 MB (index.json is ~93 KB normally)
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 1 * 1024 * 1024) {
            qCWarning(lcManifestSync) << "ManifestSync: index.json response too large, aborting";
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_syncing = false;
        emit syncStatusChanged();

        if (reply->error() != QNetworkReply::NoError) {
            // Network error — catalog stays as-is (from cache)
            if (m_catalog.isEmpty()) {
                setOffline(true);
                emit syncError(tr("Katalog indirilemedi: %1").arg(reply->errorString()));
            }
            // If we have cached data, silently continue with it
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 304) {
            // Not modified — cached version is current
            qCDebug(lcManifestSync) << "ManifestSync: 304 Not Modified — catalog is current";
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
        newCatalog.insert(it.key(), ce);
    }

    // Invalidate detail cache for packages whose version changed
    const QString detailDir = AppPaths::packageDetailDir();
    for (auto it = newCatalog.constBegin(); it != newCatalog.constEnd(); ++it) {
        auto old = m_catalog.constFind(it.key());
        if (old != m_catalog.constEnd() && old->version != it->version) {
            m_packageDetails.remove(it.key());
            QFile::remove(detailDir + QStringLiteral("/%1.json").arg(it.key()));
            qCDebug(lcManifestSync) << "ManifestSync: invalidated detail cache for" << it.key()
                     << "(version" << old->version << "->" << it->version << ")";
        }
    }

    m_catalog = std::move(newCatalog);
}

// ========== Catalog Query ==========

QVariantList ManifestSyncService::catalog() const
{
    MAKINE_ZONE_NAMED("ManifestSync::catalog");

    QVariantList result;
    result.reserve(m_catalog.size());

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

        result.append(entry);
    }

    return result;
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
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) MakineAI/0.1"));

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

    // Cache to disk
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
    }

    emit packageDetailReady(appId);
}

QVariantMap ManifestSyncService::getPackageDetail(const QString& appId) const
{
    return m_packageDetails.value(appId);
}

bool ManifestSyncService::hasPackageDetail(const QString& appId) const
{
    if (m_packageDetails.contains(appId))
        return true;

    // Check disk cache
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    return QFile::exists(cachePath);
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
        qCDebug(lcManifestSync) << "ManifestSync: back online";
    }
}

} // namespace makineai
