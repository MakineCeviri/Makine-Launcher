/**
 * @file detailfetcher.h
 * @brief Per-game package detail fetching with disk + memory cache
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Manages per-game JSON detail fetch from API, with three-tier cache:
 *   1. In-memory (QHash) — hot path
 *   2. Disk existence set (QSet) — avoids repeated QFile::exists syscalls
 *   3. Disk files (packages/{appId}.json) — cold path
 *
 * Owns its own QNetworkAccessManager with TLS pinning.
 */

#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

namespace makine {

class DetailFetcher : public QObject
{
    Q_OBJECT

public:
    explicit DetailFetcher(QObject* parent = nullptr);

    void fetch(const QString& appId);
    QVariantMap get(const QString& appId) const;
    bool has(const QString& appId) const;
    void invalidate(const QString& appId);

signals:
    void detailReady(const QString& appId);

private:
    void onFetched(const QString& appId, const QByteArray& data);
    static QString detailUrl(const QString& appId);

    QNetworkAccessManager m_nam;
    QHash<QString, QVariantMap> m_details;
    mutable QSet<QString> m_diskCache;
    QSet<QString> m_pending;
};

} // namespace makine
