/**
 * @file imagecachemanager.cpp
 * @brief Disk-based image cache — downloads from Cloudflare R2 CDN
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "imagecachemanager.h"
#include "apppaths.h"
#include "cdnconfig.h"
#include "networksecurity.h"
#include "profiler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>

namespace makine {

static constexpr auto CDN_IMAGE_BASE = cdn::kImagesBase;

ImageCacheManager::ImageCacheManager(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);
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
    // M-8: Sanitize appId to prevent path traversal.
    // Fast path: Steam appIds are purely numeric (e.g. "730", "570").
    // Check all chars — if any non-[a-zA-Z0-9_-] found, fall back to copy+filter.
    bool allSafe = true;
    for (const QChar ch : appId) {
        const char16_t c = ch.unicode();
        if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z') ||
              (c >= u'A' && c <= u'Z') || c == u'_' || c == u'-')) {
            allSafe = false;
            break;
        }
    }

    if (allSafe) {
        // Zero-allocation hot path — no copy, no regex engine
        if (appId.isEmpty()) return {};
        return m_cacheDir + QLatin1Char('/') + appId + QStringLiteral(".png");
    }

    // Slow path: filter unsafe characters (rare — non-Steam appIds)
    static const QRegularExpression kSanitizeRx(QStringLiteral("[^a-zA-Z0-9_-]"));
    QString safe = appId;
    safe.remove(kSanitizeRx);
    if (safe.isEmpty())
        return {};
    return m_cacheDir + QLatin1Char('/') + safe + QStringLiteral(".png");
}

QString ImageCacheManager::remoteUrl(const QString& appId) const
{
    return QLatin1String(CDN_IMAGE_BASE) + appId + QStringLiteral(".png");
}

QString ImageCacheManager::steamCdnUrl(const QString& appId) const
{
    return QStringLiteral("https://cdn.akamai.steamstatic.com/steam/apps/")
           + appId + QStringLiteral("/library_600x900_2x.jpg");
}

QString ImageCacheManager::resolve(const QString& appId)
{
    MAKINE_ZONE_NAMED("ImageCacheManager::resolve");
    if (appId.isEmpty())
        return {};

    const QString path = localPath(appId);

    // Already cached on disk — instant file URL
    if (QFile::exists(path)) {
#ifdef MAKINE_DEV_TOOLS
        ++m_cacheHitCount;
#endif
        return QUrl::fromLocalFile(path).toString();
    }

    // Already failed — don't retry
    if (m_failed.contains(appId))
        return {};

    // Not cached — enqueue download if not already pending/queued
    if (!m_pending.contains(appId) && !m_queued.contains(appId)) {
        m_queue.enqueue(appId);
        m_queued.insert(appId);
#ifdef MAKINE_DEV_TOOLS
        if (m_queue.size() > m_queuePeakSize)
            m_queuePeakSize = m_queue.size();
#endif
        processQueue();
    }

    return {};
}

void ImageCacheManager::processQueue()
{
    while (m_pending.size() < MAX_CONCURRENT && !m_queue.isEmpty()) {
        auto appId = m_queue.dequeue();
        m_queued.remove(appId);
        // Skip if already resolved while waiting in queue
        if (QFile::exists(localPath(appId)) || m_pending.contains(appId))
            continue;
        startDownload(appId);
    }
}

void ImageCacheManager::startDownload(const QString& appId, bool useSteamCdn)
{
    m_pending.insert(appId);
#ifdef MAKINE_DEV_TOOLS
    ++m_downloadCount;
#endif

    const QString url = useSteamCdn ? steamCdnUrl(appId) : remoteUrl(appId);
    QNetworkRequest req{QUrl{url}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1"));

    QNetworkReply* reply = m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, appId, useSteamCdn]() {
        reply->deleteLater();
        m_pending.remove(appId);

        bool success = false;

        if (reply->error() == QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 200 && status < 300) {
                const QByteArray data = reply->readAll();
                if (!data.isEmpty()) {
                    ensureCacheDir();
                    const QString path = localPath(appId);
                    QFile file(path);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(data);
                        file.close();
                        if (m_cachedSizeBytes >= 0)
                            m_cachedSizeBytes += data.size();
                        if (m_cachedImageCount >= 0)
                            ++m_cachedImageCount;
                        emit imageReady(appId);
                        emit cacheSizeChanged();
                        success = true;
                    }
                }
            }
        }

        if (!success) {
            if (!useSteamCdn && !m_r2Failed.contains(appId)) {
                // R2 CDN failed — try Steam CDN as fallback
                m_r2Failed.insert(appId);
                startDownload(appId, true);
                return;
            }
            // Both sources failed (or Steam CDN failed)
            m_failed.insert(appId);
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
    m_r2Failed.clear();
    m_queued.clear();
    m_cachedSizeBytes = 0;
    m_cachedImageCount = 0;
    emit cacheSizeChanged();
}

qint64 ImageCacheManager::cacheSizeBytes() const
{
    if (m_cachedSizeBytes >= 0)
        return m_cachedSizeBytes;

    // First call: scan directory once, then track incrementally
    MAKINE_ZONE_NAMED("ImageCacheManager::cacheSizeBytes (scan)");
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
    if (m_cachedImageCount >= 0)
        return m_cachedImageCount;

    // First call: count files once, then track incrementally
    QDir dir(m_cacheDir);
    m_cachedImageCount = dir.entryList(QDir::Files).count();
    return m_cachedImageCount;
}

qint64 ImageCacheManager::cachedImageBytes() const
{
    return cacheSizeBytes();
}

#ifdef MAKINE_DEV_TOOLS
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

} // namespace makine
