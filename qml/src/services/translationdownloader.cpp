/**
 * @file translationdownloader.cpp
 * @brief Download, decrypt, and extract translation packages from R2
 * @copyright (c) 2026 MakineAI Team
 */

#include "translationdownloader.h"
#include "apppaths.h"
#include "profiler.h"
#include "crashreporter.h"

#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QUrl>
#include <QUuid>

#ifndef MAKINEAI_UI_ONLY
#include "mkpkformat.h"
#include <QtConcurrent>
#endif

namespace makineai {

TranslationDownloader::TranslationDownloader(QObject* parent)
    : QObject(parent)
{
}

void TranslationDownloader::downloadPackage(
    const QString& appId,
    const QString& dataUrl,
    const QString& dirName)
{
    MAKINE_ZONE_NAMED("TranslationDownloader::downloadPackage");
    CrashReporter::addBreadcrumb("download",
        QStringLiteral("downloadPackage: %1").arg(appId).toUtf8().constData());

    if (appId.isEmpty() || dataUrl.isEmpty() || dirName.isEmpty()) {
        emit downloadError(appId, tr("Missing download parameters"));
        return;
    }

    // Already downloading?
    if (m_activeDownloads.contains(appId)) {
        qDebug() << "TranslationDownloader: already downloading" << appId;
        return;
    }

#ifdef MAKINEAI_UI_ONLY
    emit downloadError(appId, tr("Downloads are not available in this build"));
    return;
#else

    // Prepare temp file path with random suffix to prevent symlink attacks
    const QString tempDir = AppPaths::tempRoot() + QStringLiteral("/downloads");
    QDir().mkpath(tempDir);
    const QString tempPath = tempDir + QStringLiteral("/%1_%2.mkpkg")
        .arg(appId, QUuid::createUuid().toString(QUuid::Id128).left(8));

    // Start HTTP download
    QNetworkRequest req{QUrl{dataUrl}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::SameOriginRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MakineAI/0.1"));

    QNetworkReply* reply = m_nam.get(req);

    DownloadState state;
    state.reply = reply;
    state.tempPath = tempPath;
    state.dirName = dirName;
    state.cancelled = false;
    m_activeDownloads.insert(appId, state);
    emit activeDownloadsChanged();

    qDebug() << "TranslationDownloader: starting download" << appId << "from" << dataUrl;

    // Progress tracking
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this, appId](qint64 received, qint64 total) {
            emit downloadProgress(appId, received, total);
        });

    // Save data incrementally to temp file
    QFile* tempFile = new QFile(tempPath, reply); // parent = reply for auto cleanup
    if (!tempFile->open(QIODevice::WriteOnly)) {
        reply->abort();
        m_activeDownloads.remove(appId);
        emit activeDownloadsChanged();
        emit downloadError(appId, tr("Cannot create temp file: %1").arg(tempPath));
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this,
        [reply, tempFile]() {
            tempFile->write(reply->readAll());
        });

    // Download complete handler
    connect(reply, &QNetworkReply::finished, this,
        [this, reply, tempFile, appId, tempPath, dirName]() {
            reply->deleteLater();
            tempFile->close();

            // Check if cancelled
            auto it = m_activeDownloads.find(appId);
            if (it != m_activeDownloads.end() && it->cancelled) {
                QFile::remove(tempPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadCancelled(appId);
                return;
            }

            // Check for errors
            if (reply->error() != QNetworkReply::NoError) {
                QFile::remove(tempPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();

                const int status = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();

                QString errorMsg;
                if (status == 404) {
                    errorMsg = tr("Translation package not found on server");
                } else if (status >= 500) {
                    errorMsg = tr("Server error (%1). Try again later.").arg(status);
                } else {
                    errorMsg = tr("Download failed: %1").arg(reply->errorString());
                }

                emit downloadError(appId, errorMsg);
                return;
            }

            const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (status < 200 || status >= 300) {
                QFile::remove(tempPath);
                m_activeDownloads.remove(appId);
                emit activeDownloadsChanged();
                emit downloadError(appId, tr("Unexpected HTTP status: %1").arg(status));
                return;
            }

            qDebug() << "TranslationDownloader: download complete" << appId
                     << "- processing...";

            emit extractionStarted(appId);

            // Process in background thread (decrypt + decompress + extract)
            processDownloadedFile(appId, tempPath, dirName);
        });
#endif // !MAKINEAI_UI_ONLY
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
#ifndef MAKINEAI_UI_ONLY
    MAKINE_ZONE_NAMED("TranslationDownloader::processDownloadedFile");

    const QString destDir = m_dataPath + QStringLiteral("/") + dirName;

    // Run decrypt+decompress+extract in worker thread
    auto future = QtConcurrent::run([tempPath, destDir]()
        -> std::pair<int, std::string>
    {
        // Read entire .mkpkg file
        QFile file(tempPath);
        if (!file.open(QIODevice::ReadOnly)) {
            return {-1, "Cannot open downloaded file"};
        }

        const QByteArray rawData = file.readAll();
        file.close();

        if (rawData.isEmpty()) {
            return {-1, "Downloaded file is empty"};
        }

        // Full pipeline: decrypt → decompress → extract
        mkpk::MkpkError err{""};
        int fileCount = mkpk::process_mkpkg(
            reinterpret_cast<const uint8_t*>(rawData.constData()),
            static_cast<size_t>(rawData.size()),
            destDir.toStdWString(),
            &err);

        if (fileCount < 0) {
            return {-1, err.message};
        }

        // Clean up temp file
        QFile::remove(tempPath);

        return {fileCount, ""};
    });

    // Watch for completion
    auto* watcher = new QFutureWatcher<std::pair<int, std::string>>(this);
    connect(watcher, &QFutureWatcher<std::pair<int, std::string>>::finished, this,
        [this, watcher, appId, dirName, tempPath]() {
            watcher->deleteLater();

            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();

            auto [fileCount, errorMsg] = watcher->result();

            if (fileCount < 0) {
                QFile::remove(tempPath); // Clean up on error too
                emit downloadError(appId,
                    tr("Extraction failed: %1").arg(QString::fromStdString(errorMsg)));
                return;
            }

            qDebug() << "TranslationDownloader: package ready" << appId
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

} // namespace makineai
