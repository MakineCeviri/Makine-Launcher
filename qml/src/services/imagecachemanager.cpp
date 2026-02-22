/**
 * @file imagecachemanager.cpp
 * @brief Disk-based image cache implementation
 * @copyright (c) 2026 MakineAI Team
 */

#include "imagecachemanager.h"
#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QNetworkReply>
#include <QStandardPaths>
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
    QString safe = appId;
    safe.remove(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_-]")));
    if (safe.isEmpty())
        return {};
    return m_cacheDir + QLatin1Char('/') + safe + QStringLiteral(".jpg");
}

QString ImageCacheManager::resolve(const QString& appId, const QString& remoteUrl)
{
    if (appId.isEmpty())
        return {};

    // Embedded qrc image — instant, no network needed
    const QString qrcRes = QStringLiteral(":/qt/qml/MakineAI/resources/showcase/%1.jpg").arg(appId);
    if (QFile::exists(qrcRes))
        return QStringLiteral("qrc") + qrcRes;

    const QString path = localPath(appId);

    // Already cached on disk — return instant file URL
    if (QFile::exists(path))
        return QUrl::fromLocalFile(path).toString();

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

void ImageCacheManager::startDownload(const QString& appId, const QString& remoteUrl)
{
    m_pending.insert(appId);

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
                    ensureCacheDir();
                    QFile file(localPath(appId));
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(data);
                        file.close();
                        if (m_cachedSizeBytes >= 0)
                            m_cachedSizeBytes += data.size();
                        emit imageReady(appId);
                        emit cacheSizeChanged();
                        success = true;
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

} // namespace makineai
