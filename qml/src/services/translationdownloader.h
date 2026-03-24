/**
 * @file translationdownloader.h
 * @brief Download, decrypt, and extract translation packages from R2
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Handles the complete flow:
 *   1. HTTP GET from Cloudflare R2 (with progress + resume)
 *   2. AES-256-GCM decryption (MKPK format — includes auth tag tamper detection)
 *   3. Zstandard decompression
 *   4. Tar extraction to local data directory
 *
 * Security model:
 *   AES-256-GCM authentication tag is the sole integrity gate.
 *   Tampered or corrupted data fails decryption automatically.
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace makine {

class TranslationDownloader : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasActiveDownloads READ hasActiveDownloads NOTIFY activeDownloadsChanged)

public:
    explicit TranslationDownloader(QObject* parent = nullptr);

    void setDataPath(const QString& path) { m_dataPath = path; }

    /**
     * @brief Start downloading a translation package from R2.
     * @param appId     Steam App ID
     * @param dataUrl   R2 download URL (from manifest)
     * @param dirName   Target directory name under data path
     *
     * Flow: download → decrypt+decompress+extract → data/{dirName}/
     */
    Q_INVOKABLE void downloadPackage(const QString& appId,
                                     const QString& dataUrl,
                                     const QString& dirName);

    /**
     * @brief Cancel an active download.
     */
    Q_INVOKABLE void cancelDownload(const QString& appId);

    /**
     * @brief Check if a specific package is currently downloading.
     */
    Q_INVOKABLE bool isDownloading(const QString& appId) const;

    /**
     * @brief Check if any downloads are active.
     */
    bool hasActiveDownloads() const { return !m_activeDownloads.isEmpty(); }

signals:
    /// Download progress (bytes received / total expected)
    void downloadProgress(const QString& appId, qint64 received, qint64 total);

    /// Download complete, extraction starting
    void extractionStarted(const QString& appId);

    /// Package fully ready (downloaded + decrypted + extracted)
    void packageReady(const QString& appId, const QString& dirName);

    /// Error during download or extraction
    void downloadError(const QString& appId, const QString& error);

    /// Download was cancelled by user
    void downloadCancelled(const QString& appId);

    /// Retry attempt in progress
    void downloadRetrying(const QString& appId, int attempt, int maxAttempts);

    /// Active downloads list changed
    void activeDownloadsChanged();

private:
    void processDownloadedFile(const QString& appId, const QString& tempPath,
                               const QString& dirName);

    static constexpr int kMaxRetries = 2;  // total 3 attempts
    static constexpr int kRetryDelaysMs[kMaxRetries] = {2000, 5000};
    static bool shouldRetry(QNetworkReply::NetworkError err, int httpStatus);

    void startHttpRequest(const QString& appId);

    struct DownloadState {
        QNetworkReply* reply{nullptr};
        QString tempPath;       // UUID final temp
        QString partPath;       // {appId}.makine.part — persistent partial file
        QString dirName;
        QString dataUrl;        // Stored for resume/retry
        bool cancelled{false};
        bool stallAborted{false};
        int retryCount{0};
        qint64 resumeOffset{0};
    };

    QNetworkAccessManager m_nam;
    QHash<QString, DownloadState> m_activeDownloads;
    QString m_dataPath;
};

} // namespace makine
