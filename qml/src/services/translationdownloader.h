/**
 * @file translationdownloader.h
 * @brief Download, verify, decrypt, and extract translation packages from R2
 * @copyright (c) 2026 MakineAI Team
 *
 * Handles the complete flow:
 *   1. HTTP GET from Cloudflare R2 (with progress)
 *   2. Download .sig file and verify Ed25519 signature
 *   3. AES-256-GCM decryption (MKPK format)
 *   4. Zstandard decompression
 *   5. Tar extraction to local data directory
 *
 * After extraction, the normal LocalPackageManager install flow takes over.
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace makineai {

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
     * Flow: download → sig verify → decrypt+decompress+extract → data/{dirName}/
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

    /**
     * @brief Download .sig file and verify package signature.
     *
     * Chains async download of {dataUrl}.sig, then verifies:
     *   1. SHA-256 hash of the .mkpkg matches the hash in .sig
     *   2. Ed25519 signature over the hash is valid
     *
     * On success, continues to processDownloadedFile.
     * On failure, deletes the .mkpkg and emits downloadError.
     */
    void verifyAndProcess(const QString& appId, const QString& dataUrl,
                          const QString& tempPath, const QString& dirName);

    /**
     * @brief Verify Ed25519 signature using embedded public key.
     * @param data      Raw data that was signed (the hash hex string)
     * @param sigBase64 Base64-encoded Ed25519 signature
     * @return true if signature is valid
     */
    static bool verifyEd25519Signature(const QByteArray& data,
                                       const QByteArray& sigBase64);

    static constexpr int kMaxRetries = 2;  // total 3 attempts
    static constexpr int kRetryDelaysMs[kMaxRetries] = {2000, 5000};
    static bool shouldRetry(QNetworkReply::NetworkError err, int httpStatus);

    void startHttpRequest(const QString& appId);

    struct DownloadState {
        QNetworkReply* reply{nullptr};
        QString tempPath;       // UUID final temp (for verify)
        QString partPath;       // {appId}.mkpkg.part — persistent partial file
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

} // namespace makineai
