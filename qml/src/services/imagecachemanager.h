/**
 * @file imagecachemanager.h
 * @brief Disk-based image cache for Steam header images
 * @copyright (c) 2026 MakineAI Team
 *
 * Downloads game header images from Steam CDN and stores them locally
 * in AppData/Local/Temp/MakineAI/images/ for instant loading on
 * subsequent launches.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QQueue>
#include <QPair>
#include <QVariantMap>
#include <QImage>
#include <QNetworkAccessManager>

namespace makineai {

class ImageCacheManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qint64 cacheSizeBytes READ cacheSizeBytes NOTIFY cacheSizeChanged)
    Q_PROPERTY(QString cacheSizeFormatted READ cacheSizeFormatted NOTIFY cacheSizeChanged)
    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)

public:
    explicit ImageCacheManager(QObject* parent = nullptr);

    /**
     * @brief Resolve an image URL to a local cached path.
     *
     * If the image is already cached on disk, returns a file:/// URL.
     * Otherwise starts a background download and returns an empty string.
     * When the download completes, imageReady() is emitted.
     *
     * @param appId  Steam App ID (used as cache key)
     * @param remoteUrl  Original Steam CDN URL
     * @return file:/// URL if cached, empty string if download pending
     */
    Q_INVOKABLE QString resolve(const QString& appId, const QString& remoteUrl);

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

#ifdef MAKINEAI_DEV_TOOLS
    Q_INVOKABLE QVariantMap imageStats() const;
#endif

signals:
    /** Emitted when a single image download completes */
    void imageReady(const QString& appId);

    /** Emitted when cache size changes (download complete or clear) */
    void cacheSizeChanged();

private:
    void ensureCacheDir();
    QString localPath(const QString& appId) const;
    void startDownload(const QString& appId, const QString& remoteUrl);
    void processQueue();
    QString fallbackUrl(const QString& appId, const QString& originalUrl) const;

    QImage processForCard(const QByteArray& data) const;

    QString m_cacheDir;
    QNetworkAccessManager m_nam;
    QSet<QString> m_pending;          // appIds currently downloading
    QSet<QString> m_failed;           // appIds that exhausted all fallbacks
    QQueue<QPair<QString, QString>> m_queue;  // waiting: {appId, remoteUrl}
    mutable qint64 m_cachedSizeBytes{-1};     // Incremental cache size tracking
    static constexpr int MAX_CONCURRENT = 8;

    // Pre-baked card dimensions — matches QML sourceSize and Dimensions.cardBorderRadius * 2
    static constexpr int CARD_WIDTH  = 260;
    static constexpr int CARD_HEIGHT = 370;
    static constexpr int CARD_RADIUS = 32;  // 16px display × 2x source scale

#ifdef MAKINEAI_DEV_TOOLS
    int m_downloadCount{0};
    int m_cacheHitCount{0};
    int m_queuePeakSize{0};
#endif
};

} // namespace makineai
