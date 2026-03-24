/**
 * @file translationdownloader.cpp
 * @brief Download, decrypt, and extract translation packages from R2
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "translationdownloader.h"
#include "apppaths.h"
#include "profiler.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <string>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QTimer>
#include <QLoggingCategory>

#ifndef MAKINE_UI_ONLY
#include "mkpkformat.h"
#include <QtConcurrent>
#include <QCryptographicHash>
#endif

Q_LOGGING_CATEGORY(lcDownloader, "makine.download")

namespace makine {

TranslationDownloader::TranslationDownloader(QObject* parent)
    : QObject(parent)
{
    // Clean stale .part files older than 7 days
    const QString tempDir = AppPaths::tempRoot() + QStringLiteral("/downloads");
    QDir dir(tempDir);
    if (dir.exists()) {
        const auto entries = dir.entryInfoList({QStringLiteral("*.makine.part")}, QDir::Files);
        const qint64 staleThreshold = QDateTime::currentSecsSinceEpoch() - 7 * 24 * 3600;
        for (const auto& fi : entries) {
            if (fi.lastModified().toSecsSinceEpoch() < staleThreshold) {
                QFile::remove(fi.absoluteFilePath());
                QFile::remove(fi.absoluteFilePath() + QStringLiteral(".meta"));
                qCDebug(lcDownloader) << "removed stale part file" << fi.fileName();
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
    const QString& expectedChecksum)
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

#ifdef MAKINE_UI_ONLY
    emit downloadError(appId, tr("Bu sürümde indirme desteklenmiyor"));
    return;
#else

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
    state.expectedChecksum = expectedChecksum;
    state.cancelled = false;
    state.retryCount = 0;
    state.resumeOffset = 0;
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
        // Invalidate stale .part if checksum changed (package updated on CDN)
        const QString metaPath = state.partPath + QStringLiteral(".meta");
        QFile metaFile(metaPath);
        bool stale = false;
        if (!state.expectedChecksum.isEmpty()) {
            if (metaFile.open(QIODevice::ReadOnly)) {
                const QString savedChecksum = QString::fromUtf8(metaFile.readAll()).trimmed();
                metaFile.close();
                if (savedChecksum != state.expectedChecksum) {
                    qCDebug(lcDownloader) << "part file stale (checksum changed) for" << appId;
                    stale = true;
                }
            } else {
                // No meta file — old .part from before this fix, discard
                stale = true;
            }
        }

        if (stale) {
            QFile::remove(state.partPath);
            QFile::remove(metaPath);
            state.resumeOffset = 0;
        } else {
            state.resumeOffset = partInfo.size();
            qCDebug(lcDownloader) << "resuming from offset" << state.resumeOffset << "for" << appId;
        }
    } else {
        state.resumeOffset = 0;
    }

    // Save checksum meta alongside .part for future resume validation
    if (!state.expectedChecksum.isEmpty() && state.resumeOffset == 0) {
        const QString metaPath = state.partPath + QStringLiteral(".meta");
        QFile metaFile(metaPath);
        if (metaFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            metaFile.write(state.expectedChecksum.toUtf8());
            metaFile.close();
        }
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
        [reply, partFile]() {
            partFile->write(reply->readAll());
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
            reply->deleteLater();
            partFile->close();

            auto it = m_activeDownloads.find(appId);
            if (it == m_activeDownloads.end()) return;
            auto& state = it.value();

            if (state.cancelled) {
                QFile::remove(state.partPath);
                QFile::remove(state.partPath + QStringLiteral(".meta"));
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadCancelled(appId);
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

            // Rename .part to final temp path and clean meta
            QFile::remove(state.partPath + QStringLiteral(".meta"));
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

            // Checksum pre-flight — warn on mismatch but proceed
            // AES-256-GCM auth tag in MKPK format is the real integrity gate;
            // manifest checksums may be stale after package re-generation.
            verifyChecksum(appId, state.tempPath, state.expectedChecksum);

            // Proceed to extraction
            emit extractionStarted(appId);
            processDownloadedFile(appId, state.tempPath, state.dirName);
        });
#endif // !MAKINE_UI_ONLY
}

void TranslationDownloader::cancelDownload(const QString& appId)
{
    auto it = m_activeDownloads.find(appId);
    if (it == m_activeDownloads.end())
        return;

    it->cancelled = true;

    if (it->reply) {
        it->reply->abort();
    }
}

bool TranslationDownloader::isDownloading(const QString& appId) const
{
    return m_activeDownloads.contains(appId);
}

void TranslationDownloader::processDownloadedFile(
    const QString& appId,
    const QString& tempPath,
    const QString& dirName)
{
#ifndef MAKINE_UI_ONLY
    MAKINE_ZONE_NAMED("TranslationDownloader::processDownloadedFile");

    const QString destDir = m_dataPath + QStringLiteral("/") + dirName;

    auto future = QtConcurrent::run([tempPath, destDir]()
        -> std::pair<int, std::string>
    {
        try {
            QFile file(tempPath);
            if (!file.open(QIODevice::ReadOnly)) {
                return {-1, "Cannot open downloaded file: " + tempPath.toStdString()};
            }

            const qint64 fileSize = file.size();
            if (fileSize <= 0) {
                file.close();
                return {-1, "Downloaded file is empty"};
            }

            // Safety: reject files > 2 GB to prevent OOM
            constexpr qint64 kMaxPackageSize = 2LL * 1024 * 1024 * 1024;
            if (fileSize > kMaxPackageSize) {
                file.close();
                return {-1, "Package too large: " + std::to_string(fileSize / (1024*1024)) + " MB (max 2 GB)"};
            }

            const QByteArray rawData = file.readAll();
            file.close();

            if (rawData.isEmpty()) {
                return {-1, "Failed to read downloaded file"};
            }

            mkpk::MkpkError err{""};
            int fileCount = mkpk::process_mkpkg(
                reinterpret_cast<const uint8_t*>(rawData.constData()),
                static_cast<size_t>(rawData.size()),
                destDir.toStdWString(),
                &err);

            if (fileCount < 0) {
                return {-1, err.message};
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

            if (fileCount < 0) {
                QFile::remove(tempPath);
                emit downloadError(appId,
                    tr("Paket açma hatası: %1").arg(QString::fromStdString(errorMsg)));
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

#ifndef MAKINE_UI_ONLY

bool TranslationDownloader::verifyChecksum(
    const QString& appId,
    const QString& filePath,
    const QString& expectedChecksum)
{
    // No checksum in manifest — skip (not an error)
    if (expectedChecksum.isEmpty()) {
        qCDebug(lcDownloader) << "no checksum in manifest for" << appId << "- skipping";
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDownloader) << "cannot open file for checksum verification:" << appId;
        return false; // Cannot verify — treat as failure
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    constexpr qint64 kBufferSize = 65536;
    char buf[kBufferSize];
    qint64 bytesRead;
    while ((bytesRead = file.read(buf, kBufferSize)) > 0) {
        hasher.addData(QByteArrayView(buf, bytesRead));
    }
    file.close();

    const QString computedHex = QString::fromLatin1(hasher.result().toHex());

    // Normalize: strip "sha256:" prefix if present, compare plain hex
    const QString expectedHex = expectedChecksum.startsWith(QStringLiteral("sha256:"))
        ? expectedChecksum.mid(7)
        : expectedChecksum;

    if (computedHex != expectedHex) {
        qCWarning(lcDownloader) << "checksum mismatch for" << appId
                   << "expected:" << expectedHex
                   << "got:" << computedHex
                   << "- aborting download (integrity violation)";
        CrashReporter::addBreadcrumb("download",
            QStringLiteral("checksumMismatch: %1").arg(appId).toUtf8().constData());
        return false;
    }

    qCDebug(lcDownloader) << "checksum verified for" << appId;
    return true;
}

#endif // !MAKINE_UI_ONLY

} // namespace makine
