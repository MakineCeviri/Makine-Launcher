/**
 * @file imagecachemanager.cpp
 * @brief Disk-based image cache implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "imagecachemanager.h"
#include "apppaths.h"
#include "profiler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QNetworkReply>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QUrl>

namespace makineai {

ImageCacheManager::ImageCacheManager(QObject* parent)
    : QObject(parent)
{
    m_cacheDir = AppPaths::imageCacheDir();
    ensureCacheDir();
}

void ImageCacheManager::ensureCacheDir()
{
    QDir dir(m_cacheDir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
}

QString ImageCacheManager::localPath(const QString& appId) const
{
    // M-8: Sanitize appId to prevent path traversal (strip anything except alphanumeric/underscore)
    // Static regex: compiled once, reused across all 260+ calls per session.
    static const QRegularExpression kSanitizeRx(QStringLiteral("[^a-zA-Z0-9_-]"));
    QString safe = appId;
    safe.remove(kSanitizeRx);
    if (safe.isEmpty())
        return {};
    return m_cacheDir + QLatin1Char('/') + safe + QStringLiteral(".png");
}

QString ImageCacheManager::resolve(const QString& appId, const QString& remoteUrl)
{
    MAKINE_ZONE_NAMED("ImageCacheManager::resolve");
    if (appId.isEmpty())
        return {};

    const QString path = localPath(appId);

    // Already processed and cached on disk — instant file URL
    if (QFile::exists(path)) {
#ifdef MAKINEAI_DEV_TOOLS
        ++m_cacheHitCount;
#endif
        return QUrl::fromLocalFile(path).toString();
    }

    // Embedded qrc image — process once (bake rounded corners), cache to disk
    const QString qrcRes = QStringLiteral(":/qt/qml/MakineAI/resources/showcase/%1.jpg").arg(appId);
    if (QFile::exists(qrcRes)) {
        QFile f(qrcRes);
        if (f.open(QIODevice::ReadOnly)) {
            const QImage processed = processForCard(f.readAll());
            if (!processed.isNull()) {
                ensureCacheDir();
                if (processed.save(path, "PNG")) {
                    if (m_cachedSizeBytes >= 0)
                        m_cachedSizeBytes += QFileInfo(path).size();
#ifdef MAKINEAI_DEV_TOOLS
                    ++m_cacheHitCount;
#endif
                    return QUrl::fromLocalFile(path).toString();
                }
            }
        }
        // Fallback: return raw qrc if processing failed
#ifdef MAKINEAI_DEV_TOOLS
        ++m_cacheHitCount;
#endif
        return QStringLiteral("qrc") + qrcRes;
    }

    // Already failed all attempts — don't retry
    if (m_failed.contains(appId))
        return {};

    // Not cached — enqueue download if not already pending/queued
    if (!remoteUrl.isEmpty() && !m_pending.contains(appId)) {
        // Check if already in queue
        bool inQueue = false;
        for (const auto& item : m_queue) {
            if (item.first == appId) { inQueue = true; break; }
        }
        if (!inQueue) {
            m_queue.enqueue({appId, remoteUrl});
#ifdef MAKINEAI_DEV_TOOLS
            if (m_queue.size() > m_queuePeakSize)
                m_queuePeakSize = m_queue.size();
#endif
            processQueue();
        }
    }

    return {};
}

void ImageCacheManager::processQueue()
{
    while (m_pending.size() < MAX_CONCURRENT && !m_queue.isEmpty()) {
        auto [appId, url] = m_queue.dequeue();
        // Skip if already resolved while waiting in queue
        if (QFile::exists(localPath(appId)) || m_pending.contains(appId))
            continue;
        startDownload(appId, url);
    }
}

QString ImageCacheManager::fallbackUrl(const QString& appId, const QString& originalUrl) const
{
    // Steam CDN fallback chain:
    // 1. library_600x900_2x.jpg (vertical capsule, high-res)
    // 2. header.jpg (horizontal header, 460x215)
    // 3. capsule_616x353.jpg (wide capsule)
    const QString base = QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/%1/").arg(appId);

    if (originalUrl.contains(QStringLiteral("library_600x900")))
        return base + QStringLiteral("header.jpg");

    if (originalUrl.contains(QStringLiteral("header.jpg")))
        return base + QStringLiteral("capsule_616x353.jpg");

    return {};  // No more fallbacks
}

QImage ImageCacheManager::processForCard(const QByteArray& data) const
{
    QImage src;
    if (!src.loadFromData(data))
        return {};

    // Scale to fill target dimensions (same logic as QML Image.PreserveAspectCrop)
    QImage scaled = src.scaled(CARD_WIDTH, CARD_HEIGHT,
                               Qt::KeepAspectRatioByExpanding,
                               Qt::SmoothTransformation);

    // Center crop to exact card size
    const int cx = (scaled.width() - CARD_WIDTH) / 2;
    const int cy = (scaled.height() - CARD_HEIGHT) / 2;

    // Create result with alpha for rounded corners
    QImage result(CARD_WIDTH, CARD_HEIGHT, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Clip to rounded rect path
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, CARD_WIDTH, CARD_HEIGHT), CARD_RADIUS, CARD_RADIUS);
    painter.setClipPath(path);

    // Draw the cropped region of the scaled image
    painter.drawImage(0, 0, scaled, cx, cy, CARD_WIDTH, CARD_HEIGHT);
    painter.end();

    return result;
}

void ImageCacheManager::startDownload(const QString& appId, const QString& remoteUrl)
{
    m_pending.insert(appId);
#ifdef MAKINEAI_DEV_TOOLS
    ++m_downloadCount;
#endif

    QNetworkRequest req{QUrl{remoteUrl}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MakineAI/0.1"));

    QNetworkReply* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, appId, remoteUrl]() {
        reply->deleteLater();
        m_pending.remove(appId);

        bool success = false;

        if (reply->error() == QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 200 && status < 300) {
                const QByteArray data = reply->readAll();
                if (!data.isEmpty()) {
                    // Pre-process: crop to card aspect ratio + bake rounded corners
                    const QImage processed = processForCard(data);
                    if (!processed.isNull()) {
                        ensureCacheDir();
                        const QString path = localPath(appId);
                        if (processed.save(path, "PNG")) {
                            if (m_cachedSizeBytes >= 0)
                                m_cachedSizeBytes += QFileInfo(path).size();
                            emit imageReady(appId);
                            emit cacheSizeChanged();
                            success = true;
                        }
                    }
                }
            }
        }

        if (!success) {
            // Try fallback URL
            const QString fb = fallbackUrl(appId, remoteUrl);
            if (!fb.isEmpty()) {
                m_queue.prepend({appId, fb});
            } else {
                m_failed.insert(appId);
            }
        }

        // Process next items in queue
        processQueue();
    });
}

void ImageCacheManager::clearCache()
{
    QDir dir(m_cacheDir);
    if (dir.exists()) {
        dir.removeRecursively();
        dir.mkpath(QStringLiteral("."));
    }
    m_failed.clear();
    m_cachedSizeBytes = 0;
    emit cacheSizeChanged();
}

qint64 ImageCacheManager::cacheSizeBytes() const
{
    MAKINE_ZONE_NAMED("ImageCacheManager::cacheSizeBytes");
    if (m_cachedSizeBytes >= 0)
        return m_cachedSizeBytes;

    // First call: scan directory once, then track incrementally
    qint64 total = 0;
    QDirIterator it(m_cacheDir, QDir::Files, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    m_cachedSizeBytes = total;
    return total;
}

QString ImageCacheManager::cacheSizeFormatted() const
{
    const qint64 bytes = cacheSizeBytes();
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
}

int ImageCacheManager::cachedImageCount() const
{
    QDir dir(m_cacheDir);
    return dir.entryList(QDir::Files).count();
}

qint64 ImageCacheManager::cachedImageBytes() const
{
    return cacheSizeBytes();
}

#ifdef MAKINEAI_DEV_TOOLS
QVariantMap ImageCacheManager::imageStats() const
{
    QVariantMap map;
    map[QStringLiteral("downloads")] = m_downloadCount;
    map[QStringLiteral("cacheHits")] = m_cacheHitCount;
    int total = m_downloadCount + m_cacheHitCount;
    map[QStringLiteral("hitRate")] = total > 0
        ? static_cast<double>(m_cacheHitCount) / total : 0.0;
    map[QStringLiteral("queuePeak")] = m_queuePeakSize;
    return map;
}
#endif

} // namespace makineai
