/**
 * @file manifestsyncservice.cpp
 * @brief Catalog sync orchestrator — facade implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "manifestsyncservice.h"
#include "catalogstore.h"
#include "detailfetcher.h"
#include "telemetryservice.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "networksecurity.h"
#include "profiler.h"
#include "crashreporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcManifestSync, "makine.manifest")

namespace makine {

static constexpr auto CDN_BASE = cdn::kAssetsBase;

ManifestSyncService::ManifestSyncService(QObject* parent)
    : QObject(parent)
    , m_store(new CatalogStore())
    , m_detailFetcher(new DetailFetcher(this))
    , m_telemetry(new TelemetryService(this))
{
    security::installTlsPinning(&m_nam);

    // Load cached data for instant offline catalog display
    m_store->loadCachedEtag();
    m_store->loadCachedIndex();

    // Retry timer: periodically re-attempt catalog sync when offline
    m_retryTimer.setInterval(15000);
    connect(&m_retryTimer, &QTimer::timeout, this, [this]() {
        const int next = qMin(m_retryTimer.interval() * 2, 300000);
        m_retryTimer.setInterval(next);
        syncCatalog();
    });

    // Safety timeout: reset m_syncing if sync hangs for 30s
    m_syncTimeoutTimer.setSingleShot(true);
    m_syncTimeoutTimer.setInterval(30000);
    connect(&m_syncTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_syncing) {
            qCWarning(lcManifestSync) << "ManifestSync: sync timeout (30s) — resetting state";
            m_syncing = false;
            emit syncStatusChanged();
        }
    });

    // Forward detail fetcher signal
    connect(m_detailFetcher, &DetailFetcher::detailReady,
            this, &ManifestSyncService::packageDetailReady);
}

ManifestSyncService::~ManifestSyncService()
{
    delete m_store;
}

// ========== QML API Delegates ==========

QVariantList ManifestSyncService::catalog() const { return m_store->catalog(); }
bool ManifestSyncService::hasCatalogEntry(const QString& appId) const { return m_store->hasCatalogEntry(appId); }
QString ManifestSyncService::catalogGameName(const QString& appId) const { return m_store->catalogGameName(appId); }
int ManifestSyncService::catalogCount() const { return m_store->catalogCount(); }
QString ManifestSyncService::lastSyncTime() const { return m_store->lastSyncTime(); }

void ManifestSyncService::fetchPackageDetail(const QString& appId) { m_detailFetcher->fetch(appId); }
QVariantMap ManifestSyncService::getPackageDetail(const QString& appId) const { return m_detailFetcher->get(appId); }
bool ManifestSyncService::hasPackageDetail(const QString& appId) const { return m_detailFetcher->has(appId); }

// ========== URL Helpers ==========

QString ManifestSyncService::indexUrl()
{
    return QLatin1String(CDN_BASE) + QStringLiteral("index.json");
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
    m_syncTimeoutTimer.start();

    fetchCatalogMeta();
}

// ========== Delta Sync Flow ==========

void ManifestSyncService::fetchCatalogMeta()
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchCatalogMeta");

    QNetworkRequest req{QUrl{QLatin1String(cdn::kCatalogMeta)}};
    req.setTransferTimeout(8000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(cdn::kUserAgent));

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
    const int localVersion = m_store->loadLocalCatalogVersion();

    qCDebug(lcManifestSync) << "ManifestSync: server v" << serverVersion << "local v" << localVersion;

    if (serverVersion <= localVersion && !m_store->isEmpty()) {
        qCDebug(lcManifestSync) << "ManifestSync: catalog is current";
        finishSync();
        return;
    }

    if (localVersion > 0 && (serverVersion - localVersion) <= 50) {
        fetchCatalogDelta(localVersion);
    } else {
        fetchFullCatalog();
    }
}

void ManifestSyncService::fetchCatalogDelta(int sinceVersion)
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchCatalogDelta");

    const QString url = QLatin1String(cdn::kCatalogDelta) + QStringLiteral("?since=%1").arg(sinceVersion);
    QNetworkRequest req{QUrl{url}};
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(cdn::kUserAgent));

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

    for (const QJsonValue& val : changes) {
        const QJsonObject change = val.toObject();
        const QString appId = change[QStringLiteral("appId")].toString();
        const QString changeType = change[QStringLiteral("changeType")].toString();

        if (!m_store->applyDelta(appId, changeType,
                                 change[QStringLiteral("data")].toObject())) {
            qCWarning(lcManifestSync) << "ManifestSync: invalid delta entry, falling back to full";
            fetchFullCatalog();
            return;
        }
    }

    m_store->saveLocalCatalogVersion(toVersion);
    invalidateChangedDetails();
    finishSync();

    qCDebug(lcManifestSync) << "ManifestSync: delta applied — now v" << toVersion
                            << "with" << m_store->catalogCount() << "games";
}

void ManifestSyncService::fetchFullCatalog()
{
    MAKINE_ZONE_NAMED("ManifestSync::fetchFullCatalog");

    QNetworkRequest req{QUrl{QLatin1String(cdn::kCatalogUrl)}};
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(cdn::kUserAgent));

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
    m_store->parseIndex(data);

    if (!m_store->isEmpty()) {
        m_store->saveCachedIndex(data, QString());
        if (m_store->catalogVersion() > 0)
            m_store->saveLocalCatalogVersion(m_store->catalogVersion());

        qCDebug(lcManifestSync) << "ManifestSync: full catalog synced —"
                                << m_store->catalogCount() << "packages, v"
                                << m_store->catalogVersion();
        invalidateChangedDetails();
        finishSync();
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
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(cdn::kUserAgent));

    if (!m_store->etag().isEmpty())
        req.setRawHeader("If-None-Match", m_store->etag().toUtf8());

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > 1 * 1024 * 1024) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_syncing = false;
        m_syncTimeoutTimer.stop();
        emit syncStatusChanged();

        if (reply->error() != QNetworkReply::NoError) {
            if (m_store->isEmpty()) {
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
            m_store->parseIndex(data);

            if (!m_store->isEmpty()) {
                m_store->saveCachedIndex(data, etag);
                m_store->setEtag(etag);
                qCDebug(lcManifestSync) << "ManifestSync: legacy catalog synced —"
                                        << m_store->catalogCount() << "packages";
                invalidateChangedDetails();
                setOffline(false);
                QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
            } else {
                emit syncError(tr("Katalog verisi boş veya geçersiz"));
            }
            return;
        }

        emit syncError(tr("Katalog alınamadı (HTTP %1)").arg(status));
    });
}

// ========== Sync Helpers ==========

void ManifestSyncService::finishSync()
{
    m_syncing = false;
    m_syncTimeoutTimer.stop();
    emit syncStatusChanged();
    setOffline(false);
    QTimer::singleShot(0, this, [this]() { emit catalogReady(); });
    m_telemetry->onSyncComplete(m_store->catalogVersion(), m_store->catalogCount());
}

void ManifestSyncService::invalidateChangedDetails()
{
    const QStringList changed = m_store->takeChangedAppIds();
    for (const QString& appId : changed) {
        m_detailFetcher->invalidate(appId);
    }
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
        m_retryTimer.setInterval(15000);
        qCDebug(lcManifestSync) << "ManifestSync: back online";
    }
}

} // namespace makine
