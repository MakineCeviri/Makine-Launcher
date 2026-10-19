// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file imagecachemanager.h
 * @brief Disk-based image cache for game images from R2 CDN
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Downloads game card images from Cloudflare R2 CDN and stores them
 * locally in AppData cache for instant loading on subsequent launches.
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QSet>
#include <QQueue>
#include <QTimer>
#include <QVariantMap>
#include <QNetworkAccessManager>

namespace makine {

class ImageCacheManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qint64 cacheSizeBytes READ cacheSizeBytes NOTIFY cacheSizeChanged)
    Q_PROPERTY(QString cacheSizeFormatted READ cacheSizeFormatted NOTIFY cacheSizeChanged)
    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)

public:
    explicit ImageCacheManager(QObject* parent = nullptr);

    /**
     * @brief Resolve a game image to a local cached path.
     *
     * If the image is already cached on disk, returns a file:/// URL.
     * Otherwise starts a background download from R2 CDN
     * and returns an empty string. When the download completes,
     * imageReady() is emitted.
     *
     * @param appId  Steam App ID (used as cache key and R2 filename)
     * @return file:/// URL if cached, empty string if download pending
     */
    Q_INVOKABLE QString resolve(const QString& appId);

    /**
     * @brief Delete all cached images.
     * Recreates the cache directory after deletion.
     */
    Q_INVOKABLE void clearCache();

    QString cacheDir() const { return m_cacheDir; }
    qint64 cacheSizeBytes() const;
    QString cacheSizeFormatted() const;

    // Dev-tools accessors for MemoryProfiler
    int cachedImageCount() const;
    qint64 cachedImageBytes() const;

#ifdef MAKINE_DEV_TOOLS
    Q_INVOKABLE QVariantMap imageStats() const;
#endif

signals:
    /** Emitted when a single image download completes */
    void imageReady(const QString& appId);
    /** A cached image was replaced by a newer one from the CDN (shown from the next load on) */
    void imageUpdated(const QString& appId);

    /** Emitted when cache size changes (download complete or clear) */
    void cacheSizeChanged();

private:
    void ensureCacheDir();
    QString localPath(const QString& appId) const;
    QString remoteUrl(const QString& appId) const;
    QString steamCdnUrl(const QString& appId) const;
    void startDownload(const QString& appId, bool useSteamCdn = false);
    void processQueue();
    // A cached cover is kept for good unless asked about: once every few days
    // the CDN is asked whether ours changed (If-None-Match with the file's MD5,
    // which is what R2 uses as its ETag) — a cover replaced in the panel, or
    // one uploaded after Steam's was cached, then reaches the player.
    void maybeRevalidate(const QString& appId);
    void processRevalidations();
    void saveChecks();

    QString m_cacheDir;
    QNetworkAccessManager m_nam;
    QSet<QString> m_pending;          // appIds currently downloading
    QSet<QString> m_failed;           // appIds that failed ALL sources
    QSet<QString> m_r2Failed;         // appIds that failed R2 CDN (try Steam CDN)
    QQueue<QString> m_queue;          // appIds waiting to download
    QSet<QString> m_queued;           // O(1) lookup for queue membership
    QHash<QString, qint64> m_checkedAt;  // appId → last CDN revalidation (epoch seconds), persisted
    QSet<QString> m_revalidated;      // asked about this session already
    QQueue<QString> m_revalidateQueue;
    int m_revalidating{0};
    QTimer m_saveChecksTimer;
    mutable qint64 m_cachedSizeBytes{-1};     // Incremental cache size tracking
    mutable int m_cachedImageCount{-1};        // Incremental image count tracking
    static constexpr int MAX_CONCURRENT = 8;
    static constexpr int MAX_REVALIDATING = 2;
    static constexpr qint64 RECHECK_AFTER_SECS = 3 * 24 * 3600;

#ifdef MAKINE_DEV_TOOLS
    int m_downloadCount{0};
    int m_cacheHitCount{0};
    int m_queuePeakSize{0};
#endif
};

} // namespace makine
