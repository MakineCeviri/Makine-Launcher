// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
 * A game's other patches (alternatives) have details of their own:
 * `?pkg=<id>` on the API, cached as packages/{appId}--{id}.json. An empty
 * pkg is the default patch, exactly as before.
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

    void fetch(const QString& appId, const QString& pkg = {});
    QVariantMap get(const QString& appId, const QString& pkg = {}) const;
    bool has(const QString& appId, const QString& pkg = {}) const;
    /// Drops the default's detail and every alternative's of this game
    void invalidate(const QString& appId);
    /// Forget every detail held in memory; the files are CoreBridge's to drop.
    void invalidateAll();

    /// The API URL for a patch's detail (default: no query)
    static QString detailUrl(const QString& appId, const QString& pkg = {});

signals:
    /// Always the appId, whichever patch was asked for
    void detailReady(const QString& appId);

private:
    void onFetched(const QString& appId, const QString& key, const QByteArray& data);
    static bool validPkg(const QString& pkg);

    QNetworkAccessManager m_nam;
    QHash<QString, QVariantMap> m_details;   // keyed by patchcompat::detailKey
    mutable QSet<QString> m_diskCache;
    QSet<QString> m_pending;
};

} // namespace makine
