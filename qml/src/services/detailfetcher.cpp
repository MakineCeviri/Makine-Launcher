// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
#include "installsteprules.h"
#include "patchcompat.h"

#include <makine/package_catalog.hpp>
#include <makine/path_utils.hpp>

#include <QDir>
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

bool DetailFetcher::validPkg(const QString& pkg)
{
    return pkg.isEmpty() || packages::isValidAlternativeId(pkg.toStdString());
}

void DetailFetcher::fetch(const QString& appId, const QString& pkg)
{
    MAKINE_ZONE_NAMED("DetailFetcher::fetch");

    const QString key = patchcompat::detailKey(appId, pkg);
    if (appId.isEmpty() || m_pending.contains(key))
        return;

    // Security: validate appId before any file path construction
    if (makine::path::containsTraversalPattern(appId.toStdString()) || !validPkg(pkg)) {
        qCWarning(lcDetailFetcher) << "DetailFetcher: rejecting traversal in appId:" << appId << pkg;
        return;
    }

    // Check in-memory cache
    if (m_details.contains(key)) {
        emit detailReady(appId);
        return;
    }

    // Check disk cache
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(key);
    if (QFile::exists(cachePath)) {
        QFile file(cachePath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                m_details.insert(key, doc.object().toVariantMap());
                emit detailReady(appId);
                return;
            }
        }
    }

    // Fetch from API
    m_pending.insert(key);

    QNetworkRequest req{QUrl{detailUrl(appId, pkg)}};
    // Which recipe steps this build runs; without it the API serves the
    // recipe kept for 0.1.5 (installsteprules.h).
    req.setRawHeader("X-Makine-Recipe", QByteArray::number(steprules::kRecipeLevel));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    // HTTP/1.1 like the catalogue requests (manifestsyncservice.cpp): Qt's
    // HTTP/2 to Cloudflare drops streams, and a failed detail fetch stops the
    // install ("Kurulum bilgileri alınamadı").
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    security::hardenRequest(req, 10000);

    QNetworkReply* reply = m_nam.get(req);
    security::installSizeGuard(reply, 1 * 1024 * 1024);

    connect(reply, &QNetworkReply::finished, this, [this, reply, appId, key]() {
        reply->deleteLater();
        m_pending.remove(key);

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcDetailFetcher) << "DetailFetcher: network error for" << key
                                       << reply->errorString();
            emit detailReady(appId);
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            onFetched(appId, key, reply->readAll());
        } else {
            qCWarning(lcDetailFetcher) << "DetailFetcher: HTTP" << status << "for" << key;
            emit detailReady(appId);
        }
    });
}

QVariantMap DetailFetcher::get(const QString& appId, const QString& pkg) const
{
    return m_details.value(patchcompat::detailKey(appId, pkg));
}

bool DetailFetcher::has(const QString& appId, const QString& pkg) const
{
    const QString key = patchcompat::detailKey(appId, pkg);
    if (m_details.contains(key))
        return true;

    if (m_diskCache.contains(key))
        return true;

    if (!validPkg(pkg))
        return false;

    // Cold path: disk check
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(key);
    const bool exists = QFile::exists(cachePath);
    if (exists)
        m_diskCache.insert(key);
    return exists;
}

void DetailFetcher::invalidate(const QString& appId)
{
    // The default's detail and every alternative's: "<appId>" and "<appId>--*"
    const QString altPrefix = appId + QStringLiteral("--");
    const auto drop = [&](const QString& key) {
        return key == appId || key.startsWith(altPrefix);
    };
    m_details.removeIf([&](const auto& it) { return drop(it.key()); });
    m_diskCache.removeIf([&](const QString& key) { return drop(key); });

    // Deferred deletion — cache is already invalidated in memory
    const QString dir = AppPaths::packageDetailDir();
    QTimer::singleShot(0, this, [dir, appId, altPrefix]() {
        QFile::remove(dir + QStringLiteral("/%1.json").arg(appId));
        const QStringList alts = QDir(dir).entryList({altPrefix + QStringLiteral("*.json")},
                                                     QDir::Files);
        for (const QString& f : alts)
            QFile::remove(dir + QLatin1Char('/') + f);
    });
}

void DetailFetcher::invalidateAll()
{
    m_details.clear();
    m_diskCache.clear();
}

// ========== Internal ==========

void DetailFetcher::onFetched(const QString& appId, const QString& key, const QByteArray& data)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcDetailFetcher) << "DetailFetcher: invalid JSON for" << key;
        emit detailReady(appId);
        return;
    }

    m_details.insert(key, doc.object().toVariantMap());

    // Cache to disk
    const QString cachePath = AppPaths::packageDetailDir() + QStringLiteral("/%1.json").arg(key);
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        m_diskCache.insert(key);
    }

    emit detailReady(appId);
}

QString DetailFetcher::detailUrl(const QString& appId, const QString& pkg)
{
    QString url = QLatin1String(cdn::kGameDetail) + appId;
    if (!pkg.isEmpty())
        url += QStringLiteral("?pkg=") + pkg;   // [a-z0-9-] only (validPkg)
    return url;
}

} // namespace makine
