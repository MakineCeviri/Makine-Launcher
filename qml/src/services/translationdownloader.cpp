// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file translationdownloader.cpp
 * @brief Download, decrypt, and extract translation packages from R2
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "translationdownloader.h"
#include "apppaths.h"
#include "networksecurity.h"
#include "profiler.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <atomic>
#include <memory>
#include <string>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QTimer>
#include <QStorageInfo>
#include <QLoggingCategory>

#ifndef MAKINE_UI_ONLY
#include "mkpkformat.h"
#include <QtConcurrent>
#endif

Q_LOGGING_CATEGORY(lcDownloader, "makine.download")

namespace makine {

TranslationDownloader::TranslationDownloader(QObject* parent)
    : QObject(parent)
{
    security::installTlsPinning(&m_nam);

    // Clean stale temp files older than 7 days. Both .part (interrupted
    // download) and final .makine (extraction crashed before QFile::remove
    // ran) get orphaned; otherwise they accumulate forever and chew through
    // the user's drive over months of crashes/cancels (TD-08).
    const QString tempDir = AppPaths::tempRoot() + QStringLiteral("/downloads");
    QDir dir(tempDir);
    if (dir.exists()) {
        const auto entries = dir.entryInfoList(
            {QStringLiteral("*.makine.part"), QStringLiteral("*.makine")},
            QDir::Files);
        const qint64 staleThreshold = QDateTime::currentSecsSinceEpoch() - 7 * 24 * 3600;
        for (const auto& fi : entries) {
            if (fi.lastModified().toSecsSinceEpoch() < staleThreshold) {
                QFile::remove(fi.absoluteFilePath());
                qCDebug(lcDownloader) << "removed stale temp file" << fi.fileName();
            }
        }
    }
}

bool TranslationDownloader::shouldRetry(QNetworkReply::NetworkError err, int httpStatus)
{
    if (err == QNetworkReply::OperationCanceledError)
        return false;

    if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 408 && httpStatus != 429)
        return false;

    if (httpStatus >= 500)
        return true;

    switch (err) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        return true;
    default:
        break;
    }

    return false;
}

void TranslationDownloader::downloadPackage(
    const QString& appId,
    const QString& dataUrl,
    const QString& dirName,
    qint64 expectedSize)
{
    MAKINE_ZONE_NAMED("TranslationDownloader::downloadPackage");
    CrashReporter::addBreadcrumb("download",
        QStringLiteral("downloadPackage: %1").arg(appId).toUtf8().constData());

    if (appId.isEmpty() || dataUrl.isEmpty() || dirName.isEmpty()) {
        emit downloadError(appId, tr("İndirme bilgileri eksik"));
        return;
    }

    if (m_activeDownloads.contains(appId)) {
        qCDebug(lcDownloader) << "already downloading" << appId;
        return;
    }

    // Already-queued duplicate (user double-clicked Install).
    for (const auto& q : m_pendingDownloads) {
        if (q.appId == appId) {
            qCDebug(lcDownloader) << "already queued" << appId;
            return;
        }
    }

    // Serialise concurrent installs — defer to the queue if a slot isn't free.
    if (m_activeDownloads.size() >= kMaxConcurrentDownloads) {
        qCDebug(lcDownloader) << "queueing download" << appId
                              << "— active:" << m_activeDownloads.size();
        m_pendingDownloads.enqueue({appId, dataUrl, dirName, expectedSize});
        return;
    }

#ifdef MAKINE_UI_ONLY
    emit downloadError(appId, tr("Bu sürümde indirme desteklenmiyor"));
    return;
#else

    // Check available disk space before downloading. The flat 500 MB minimum
    // misses large packages (>250 MB compressed needs >750 MB end-to-end:
    // .part + decrypt buffer + extracted dir). When the catalog gives us
    // expectedSize, scale to ~3x + 200 MB safety; otherwise keep the
    // 500 MB floor for unknown-size paths (TD-02).
    const auto storageInfo = QStorageInfo(AppPaths::dataDir());
    const qint64 availableBytes = storageInfo.bytesAvailable();
    constexpr qint64 kMinFreeSpace = 500LL * 1024 * 1024;
    qint64 requiredBytes = kMinFreeSpace;
    if (expectedSize > 0) {
        const qint64 scaled = expectedSize * 3 + 200LL * 1024 * 1024;
        if (scaled > requiredBytes)
            requiredBytes = scaled;
    }
    if (availableBytes > 0 && availableBytes < requiredBytes) {
        emit downloadError(appId, tr("Yetersiz disk alanı — yaklaşık %1 MB boş alan gerekli (%2 MB mevcut)")
            .arg(requiredBytes / (1024 * 1024))
            .arg(availableBytes / (1024 * 1024)));
        return;
    }

    const QString tempDir = AppPaths::tempRoot() + QStringLiteral("/downloads");
    QDir().mkpath(tempDir);

    const QString tempPath = tempDir + QStringLiteral("/%1_%2.makine")
        .arg(appId, QUuid::createUuid().toString(QUuid::Id128).left(8));
    const QString partPath = tempDir + QStringLiteral("/%1.makine.part").arg(appId);

    // Normalize legacy .mkpkg URLs to .makine (R2 uses .makine extension)
    QString normalizedUrl = dataUrl;
    if (normalizedUrl.endsWith(QStringLiteral(".mkpkg"))) {
        normalizedUrl.chop(6);
        normalizedUrl.append(QStringLiteral(".makine"));
        qCDebug(lcDownloader) << "normalized .mkpkg URL to .makine for" << appId;
    }

    DownloadState state;
    state.tempPath = tempPath;
    state.partPath = partPath;
    state.dirName = dirName;
    state.dataUrl = normalizedUrl;
    state.cancelled = false;
    state.retryCount = 0;
    state.resumeOffset = 0;
    state.cancelFlag = std::make_shared<std::atomic_bool>(false);
    m_activeDownloads.insert(appId, state);
    emit activeDownloadsChanged();

    startHttpRequest(appId);

#endif // !MAKINE_UI_ONLY
}

void TranslationDownloader::startHttpRequest(const QString& appId)
{
#ifndef MAKINE_UI_ONLY
    auto it = m_activeDownloads.find(appId);
    if (it == m_activeDownloads.end()) return;
    auto& state = it.value();

    QFileInfo partInfo(state.partPath);
    if (partInfo.exists() && partInfo.size() > 0) {
        // Cross-session resume is risky: if the CDN updated the package
        // after the user's prior partial download, appending fresh bytes
        // to stale prefix produces a corrupt file that fails AES-GCM auth
        // tag verification. The user then sees "Paket bozuk" and can be
        // stuck retrying until the constructor's 7-day cleanup kicks in.
        // 1 hour comfortably covers network-blip retries (the legitimate
        // resume use case) without exposing the staleness window.
        const qint64 ageSec = partInfo.lastModified()
                              .secsTo(QDateTime::currentDateTime());
        constexpr qint64 kMaxResumeAgeSec = 3600;  // 1 hour
        if (ageSec > kMaxResumeAgeSec) {
            qCDebug(lcDownloader) << "stale .part for" << appId
                                  << "(" << ageSec << "s old) — discarding";
            QFile::remove(state.partPath);
            state.resumeOffset = 0;
        } else {
            state.resumeOffset = partInfo.size();
            qCDebug(lcDownloader) << "resuming from offset"
                                  << state.resumeOffset << "for" << appId;
        }
    } else {
        state.resumeOffset = 0;
    }

    QNetworkRequest req{QUrl{state.dataUrl}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1"));

    if (state.resumeOffset > 0) {
        req.setRawHeader("Range",
            QStringLiteral("bytes=%1-").arg(state.resumeOffset).toUtf8());
    }

    QNetworkReply* reply = m_nam.get(req);
    state.reply = reply;

    qCDebug(lcDownloader) << "HTTP request" << appId
             << "from" << state.dataUrl
             << "offset:" << state.resumeOffset
             << "attempt:" << (state.retryCount + 1);

    QFile* partFile = new QFile(state.partPath, reply);
    QIODevice::OpenMode mode = (state.resumeOffset > 0)
        ? QIODevice::Append
        : QIODevice::WriteOnly;

    if (!partFile->open(mode)) {
        reply->abort();
        m_activeDownloads.remove(appId);
        emit activeDownloadsChanged();
        startNextQueuedDownload();
        emit downloadError(appId,
            tr("Geçici dosya oluşturulamadı: %1").arg(state.partPath));
        return;
    }

    const qint64 resumeOffset = state.resumeOffset;

    connect(reply, &QNetworkReply::downloadProgress, this,
        [this, appId, resumeOffset](qint64 received, qint64 total) {
            const qint64 actualReceived = received + resumeOffset;
            const qint64 actualTotal = (total > 0) ? total + resumeOffset : -1;
            emit downloadProgress(appId, actualReceived, actualTotal);
        });

    connect(reply, &QNetworkReply::readyRead, this,
        [this, reply, partFile, appId]() {
            const QByteArray chunk = reply->readAll();
            if (chunk.isEmpty()) return;
            const qint64 written = partFile->write(chunk);
            if (written != chunk.size()) {
                // Disk full or filesystem error — mark fatal so the finished
                // slot reports the real cause instead of letting the partial
                // .part bubble up later as a confusing decrypt failure.
                qCWarning(lcDownloader)
                    << "partFile write short:" << written << "of" << chunk.size()
                    << "for" << appId << "—" << partFile->errorString();
                auto it = m_activeDownloads.find(appId);
                if (it != m_activeDownloads.end()) it->writeError = true;
                reply->abort();
            }
        });

    // Stall timeout: abort if no data for 60 seconds
    QTimer* stallTimer = new QTimer(reply);
    stallTimer->setSingleShot(true);
    stallTimer->setInterval(60000);
    connect(reply, &QNetworkReply::readyRead, stallTimer, [stallTimer]() {
        stallTimer->start();
    });
    connect(stallTimer, &QTimer::timeout, this, [this, appId, reply]() {
        auto it = m_activeDownloads.find(appId);
        if (it != m_activeDownloads.end()) it->stallAborted = true;
        reply->abort();
    });
    stallTimer->start();

    connect(reply, &QNetworkReply::finished, this,
        [this, reply, partFile, appId]() {
            // B2-08: flush before close so the tail of the last
            // buffered chunk persists even if NAM/reply is torn down at
            // shutdown — the implicit QFile destructor close() does not
            // guarantee the bytes the resume logic later trusts.
            partFile->flush();
            partFile->close();
            reply->deleteLater();

            auto it = m_activeDownloads.find(appId);
            if (it == m_activeDownloads.end()) return;
            auto& state = it.value();

            if (state.cancelled) {
                QFile::remove(state.partPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadCancelled(appId);
                return;
            }

            // Surface disk-full / write errors as themselves rather than
            // letting the truncated .part advance to decrypt and confuse
            // the user with "Paket açma hatası".
            if (state.writeError) {
                QFile::remove(state.partPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadError(appId,
                    tr("Disk alanı doldu veya yazma hatası — boş alanı kontrol edip tekrar deneyin"));
                return;
            }

            const int httpStatus = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();

            // Server ignored Range header — restart
            if (httpStatus == 200 && state.resumeOffset > 0) {
                qCDebug(lcDownloader) << "server ignored Range header, restarting" << appId;
                QFile::remove(state.partPath);
                state.resumeOffset = 0;
                QMetaObject::invokeMethod(this, [this, appId]() {
                    startHttpRequest(appId);
                }, Qt::QueuedConnection);
                return;
            }

            if (reply->error() != QNetworkReply::NoError) {
                const bool retryable = state.stallAborted
                    || shouldRetry(reply->error(), httpStatus);
                state.stallAborted = false;

                if (retryable && state.retryCount < kMaxRetries) {
                    const int delay = kRetryDelaysMs[state.retryCount];
                    state.retryCount++;
                    qCDebug(lcDownloader) << "retrying" << appId
                             << "attempt" << state.retryCount
                             << "after" << delay << "ms";
                    emit downloadRetrying(appId, state.retryCount, kMaxRetries);
                    QTimer::singleShot(delay, this, [this, appId]() {
                        startHttpRequest(appId);
                    });
                    return;
                }

                QFile::remove(state.partPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();

                QString errorMsg;
                if (httpStatus == 404) {
                    errorMsg = tr("Çeviri paketi sunucuda bulunamadı");
                } else if (httpStatus >= 500) {
                    errorMsg = tr("Sunucu hatası (%1). Lütfen tekrar deneyin.").arg(httpStatus);
                } else {
                    errorMsg = tr("İndirme başarısız oldu. İnternet bağlantınızı kontrol edin.");
                }

                emit downloadError(appId, errorMsg);
                return;
            }

            if (httpStatus < 200 || httpStatus >= 300) {
                QFile::remove(state.partPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadError(appId, tr("Beklenmeyen sunucu yanıtı: %1").arg(httpStatus));
                return;
            }

            qCDebug(lcDownloader) << "download complete" << appId;

            // Rename .part to final temp path
            if (!QFile::rename(state.partPath, state.tempPath)) {
                if (QFile::copy(state.partPath, state.tempPath)) {
                    QFile::remove(state.partPath);
                } else {
                    m_activeDownloads.remove(appId);
                    emit activeDownloadsChanged();
                    emit downloadError(appId, tr("Geçici dosya oluşturulamadı"));
                    return;
                }
            }

            // Proceed to extraction (AES-256-GCM auth tag is the integrity gate)
            emit extractionStarted(appId);
            processDownloadedFile(appId, state.tempPath, state.dirName);
        });
#endif // !MAKINE_UI_ONLY
}

void TranslationDownloader::startNextQueuedDownload()
{
    if (m_pendingDownloads.isEmpty())
        return;
    if (m_activeDownloads.size() >= kMaxConcurrentDownloads)
        return;

    const QueuedDownload next = m_pendingDownloads.dequeue();
    qCDebug(lcDownloader) << "promoting queued download" << next.appId
                          << "— remaining queue:" << m_pendingDownloads.size();
    downloadPackage(next.appId, next.dataUrl, next.dirName, next.expectedSize);
}

void TranslationDownloader::cancelDownload(const QString& appId)
{
    // Drop from the queue if it hasn't been promoted yet.
    for (auto qIt = m_pendingDownloads.begin(); qIt != m_pendingDownloads.end(); ++qIt) {
        if (qIt->appId == appId) {
            m_pendingDownloads.erase(qIt);
            emit downloadCancelled(appId);
            return;
        }
    }

    auto it = m_activeDownloads.find(appId);
    if (it == m_activeDownloads.end())
        return;

    it->cancelled = true;
    // Trip the atomic flag too — the extraction worker (which already
    // released the network reply by the time it runs) only sees this (TD-10).
    if (it->cancelFlag)
        it->cancelFlag->store(true);

    if (it->reply) {
        it->reply->abort();
    }
}

bool TranslationDownloader::isDownloading(const QString& appId) const
{
    return m_activeDownloads.contains(appId);
}

// Map raw extraction error messages to user-friendly Turkish.
// Keeps technical detail (paths, byte counts) out of the dialog.
static QString categorizeExtractError(const QString& raw)
{
    if (raw.contains(QLatin1String("authentication tag mismatch")) ||
        raw.startsWith(QLatin1String("Decryption failed")))
        return TranslationDownloader::tr(
            "Paket bozuk veya kurcalanmış (kimlik doğrulama başarısız). "
            "Tekrar indirmeyi deneyin.");

    if (raw.contains(QLatin1String("Invalid MKPK")) ||
        raw.contains(QLatin1String("File too small")) ||
        raw.contains(QLatin1String("Tar truncated")) ||
        raw.contains(QLatin1String("Tar data too small")))
        return TranslationDownloader::tr(
            "Paket dosyası bozuk veya eksik indirildi. Tekrar deneyin.");

    if (raw.contains(QLatin1String("zstd")) ||
        raw.contains(QLatin1String("Not valid zstd")))
        return TranslationDownloader::tr(
            "Paket çözümleme hatası — dosya bozuk olabilir. Tekrar indirmeyi deneyin.");

    if (raw.contains(QLatin1String("Unsupported MKPK version")))
        return TranslationDownloader::tr(
            "Paket sürümü desteklenmiyor — uygulamayı güncellemeniz gerekiyor.");

    if (raw.contains(QLatin1String("Path traversal")))
        return TranslationDownloader::tr(
            "Güvenlik hatası: paket geçersiz dosya yolu içeriyor.");

    if (raw.contains(QLatin1String("locked")))
        return TranslationDownloader::tr(
            "Dosya kilitli — oyun açıksa kapatıp tekrar deneyin.");

    if (raw.contains(QLatin1String("disk full")) ||
        raw.contains(QLatin1String("Write failed")))
        return TranslationDownloader::tr(
            "Disk alanı yetersiz — boş alanı kontrol edip tekrar deneyin.");

    if (raw.contains(QLatin1String("Out of memory")) ||
        raw.contains(QLatin1String("memory-map")))
        return TranslationDownloader::tr(
            "Bellek yetersiz — diğer uygulamaları kapatıp tekrar deneyin.");

    if (raw.contains(QLatin1String("Package too large")))
        return TranslationDownloader::tr(
            "Paket boyutu güvenlik sınırını aşıyor.");

    if (raw.contains(QLatin1String("Cannot clean stale")))
        return TranslationDownloader::tr(
            "Önceki kurulum kalıntıları temizlenemedi — uygulamayı yeniden başlatın.");

    if (raw.contains(QLatin1String("Cannot open")) ||
        raw.contains(QLatin1String("empty")) ||
        raw.contains(QLatin1String("read")))
        return TranslationDownloader::tr(
            "İndirilen dosya okunamadı. Tekrar deneyin.");

    return TranslationDownloader::tr("Paket açma hatası: %1").arg(raw);
}

void TranslationDownloader::processDownloadedFile(
    const QString& appId,
    const QString& tempPath,
    const QString& dirName)
{
#ifndef MAKINE_UI_ONLY
    MAKINE_ZONE_NAMED("TranslationDownloader::processDownloadedFile");

    const QString destDir = m_dataPath + QStringLiteral("/") + dirName;

    // Snapshot the cancel flag now (still under the GUI thread); the lambda
    // captures the shared_ptr by value so it stays valid even if cancelDownload
    // removes the DownloadState before the worker finishes.
    std::shared_ptr<std::atomic_bool> cancelFlag =
        m_activeDownloads.value(appId).cancelFlag;

    auto future = QtConcurrent::run([tempPath, destDir, cancelFlag]()
        -> std::pair<int, std::string>
    {
        try {
            // Clean stale extraction from a previous crashed install.
            // If the dir has any entries, the prior run died mid-extract and
            // tar would either refuse to overwrite existing files or leave a
            // mixed-version directory. Either way we must start fresh.
            QDir destDirObj(destDir);
            if (destDirObj.exists()) {
                const auto staleEntries = destDirObj.entryList(
                    QDir::NoDotAndDotDot | QDir::AllEntries);
                if (!staleEntries.isEmpty()) {
                    qCWarning(lcDownloader)
                        << "stale extraction found at" << destDir
                        << "with" << staleEntries.size() << "entries — cleaning";
                    if (!destDirObj.removeRecursively()) {
                        return {-1, "Cannot clean stale extraction directory: "
                                + destDir.toStdString()};
                    }
                }
            }

            QFile file(tempPath);
            if (!file.open(QIODevice::ReadOnly)) {
                return {-1, "Cannot open downloaded file: " + tempPath.toStdString()};
            }

            const qint64 fileSize = file.size();
            if (fileSize <= 0) {
                file.close();
                return {-1, "Downloaded file is empty"};
            }

            // Safety: reject unreasonably large files
            if (fileSize > makine::security::kMaxPackageBytes) {
                file.close();
                return {-1, "Package too large: " + std::to_string(fileSize / (1024*1024)) + " MB"};
            }

            // Memory-map the file instead of readAll() to avoid OOM on large packages.
            // OS manages paging — no RAM allocation for the file contents.
            uchar* mapped = file.map(0, fileSize);
            if (!mapped) {
                // Fallback: small files can still use readAll
                if (fileSize > 256 * 1024 * 1024) {
                    file.close();
                    return {-1, "Failed to memory-map large package (" + std::to_string(fileSize / (1024*1024)) + " MB)"};
                }
                const QByteArray rawData = file.readAll();
                file.close();
                if (rawData.isEmpty())
                    return {-1, "Failed to read downloaded file"};

                mkpk::MkpkError err{""};
                auto cancelCheck = [cancelFlag]() {
                    return cancelFlag && cancelFlag->load();
                };
                int fileCount = mkpk::process_mkpkg(
                    reinterpret_cast<const uint8_t*>(rawData.constData()),
                    static_cast<size_t>(rawData.size()),
                    destDir.toStdWString(),
                    &err,
                    nullptr,
                    cancelCheck);
                if (fileCount < 0)
                    return {fileCount, err.message};
                QFile::remove(tempPath);
                return {fileCount, ""};
            }

            mkpk::MkpkError err{""};
            auto cancelCheck = [cancelFlag]() {
                return cancelFlag && cancelFlag->load();
            };
            int fileCount = mkpk::process_mkpkg(
                reinterpret_cast<const uint8_t*>(mapped),
                static_cast<size_t>(fileSize),
                destDir.toStdWString(),
                &err,
                nullptr,
                cancelCheck);
            file.unmap(mapped);
            file.close();

            if (fileCount < 0) {
                return {fileCount, err.message};
            }

            QFile::remove(tempPath);
            return {fileCount, ""};
        } catch (const std::bad_alloc&) {
            return {-1, "Out of memory during package extraction"};
        } catch (const std::exception& e) {
            return {-1, std::string("Extraction failed: ") + e.what()};
        } catch (...) {
            return {-1, "Unknown error during package extraction"};
        }
    });

    auto* watcher = new QFutureWatcher<std::pair<int, std::string>>(this);
    connect(watcher, &QFutureWatcher<std::pair<int, std::string>>::finished, this,
        [this, watcher, appId, dirName, tempPath]() {
            watcher->deleteLater();

            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();

            auto [fileCount, errorMsg] = watcher->result();

            if (fileCount == -2) {
                // Cancelled mid-extract — drop the .makine temp and any
                // partial destDir, then notify the UI via the cancel
                // signal rather than the error path (TD-10).
                QFile::remove(tempPath);
                QDir(m_dataPath + QStringLiteral("/") + dirName).removeRecursively();
                qCDebug(lcDownloader) << "extraction cancelled for" << appId;
                emit downloadCancelled(appId);
                return;
            }

            if (fileCount < 0) {
                QFile::remove(tempPath);
                const QString rawErr = QString::fromStdString(errorMsg);
                qCWarning(lcDownloader) << "extraction failed for" << appId
                                         << "raw:" << rawErr;
                emit downloadError(appId, categorizeExtractError(rawErr));
                return;
            }

            qCDebug(lcDownloader) << "package ready" << appId
                     << "-" << fileCount << "files extracted to" << dirName;

            CrashReporter::addBreadcrumb("download",
                QStringLiteral("packageReady: %1 (%2 files)").arg(appId).arg(fileCount).toUtf8().constData());
            emit packageReady(appId, dirName);
        });

    watcher->setFuture(future);
#else
    Q_UNUSED(appId)
    Q_UNUSED(tempPath)
    Q_UNUSED(dirName)
#endif
}

} // namespace makine
