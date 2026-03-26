/**
 * @file detailfetcher.cpp
 * @brief Per-game package detail fetching implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "detailfetcher.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "networksecurity.h"
#include "profiler.h"

#include <makine/path_utils.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

Q_LOGGING_CATEGORY(lcDetailFetcher, "makine.manifest")

namespace makine {

DetailFetcher::DetailFetcher(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);
}

// ========== Public API ==========

void DetailFetcher::fetch(const QString& appId)
{
    MAKINE_ZONE_NAMED("DetailFetcher::fetch");

    if (appId.isEmpty() || m_pending.contains(appId))
        return;

    // Security: validate appId before any file path construction
    if (makine::path::containsTraversalPattern(appId.toStdString())) {
        qCWarning(lcDetailFetcher) << "DetailFetcher: rejecting traversal in appId:" << appId;
        return;
    }

    // Check in-memory cache
    if (m_details.contains(appId)) {
        emit detailReady(appId);
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
                m_details.insert(appId, doc.object().toVariantMap());
                emit detailReady(appId);
                return;
            }
        }
    }

    // Fetch from API
    m_pending.insert(appId);

    QNetworkRequest req{QUrl{detailUrl(appId)}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    security::hardenRequest(req, 10000);

    QNetworkReply* reply = m_nam.get(req);
    security::installSizeGuard(reply, 1 * 1024 * 1024);

    connect(reply, &QNetworkReply::finished, this, [this, reply, appId]() {
        reply->deleteLater();
        m_pending.remove(appId);

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDetailFetcher) << "DetailFetcher: network error for" << appId
                                       << reply->errorString();
            emit detailReady(appId);
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            onFetched(appId, reply->readAll());
        } else {
            qCWarning(lcDetailFetcher) << "DetailFetcher: HTTP" << status << "for" << appId;
            emit detailReady(appId);
        }
    });
}

QVariantMap DetailFetcher::get(const QString& appId) const
{
    return m_details.value(appId);
}

bool DetailFetcher::has(const QString& appId) const
{
    if (m_details.contains(appId))
        return true;

    if (m_diskCache.contains(appId))
        return true;

    // Cold path: disk check
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    const bool exists = QFile::exists(cachePath);
    if (exists)
        m_diskCache.insert(appId);
    return exists;
}

void DetailFetcher::invalidate(const QString& appId)
{
    m_details.remove(appId);
    m_diskCache.remove(appId);

    // Deferred deletion — cache is already invalidated in memory
    const QString path = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    QTimer::singleShot(0, this, [path]() { QFile::remove(path); });
}

// ========== Internal ==========

void DetailFetcher::onFetched(const QString& appId, const QByteArray& data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcDetailFetcher) << "DetailFetcher: invalid JSON for" << appId;
        emit detailReady(appId);
        return;
    }

    m_details.insert(appId, doc.object().toVariantMap());

    // Cache to disk
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(appId);
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        m_diskCache.insert(appId);
    }

    emit detailReady(appId);
}

QString DetailFetcher::detailUrl(const QString& appId)
{
    return QLatin1String(cdn::kGameDetail) + appId;
}

} // namespace makine
