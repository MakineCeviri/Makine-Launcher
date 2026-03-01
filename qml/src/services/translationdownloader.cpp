/**
 * @file translationdownloader.cpp
 * @brief Download, verify, decrypt, and extract translation packages from R2
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
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
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
        [this, reply, tempFile, appId, dataUrl, tempPath, dirName]() {
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
                     << "- verifying signature...";

            // Verify signature before processing
            verifyAndProcess(appId, dataUrl, tempPath, dirName);
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

#ifndef MAKINEAI_UI_ONLY

// Embedded Ed25519 public key — must match core/src/security/security_manager.cpp.
// Private key: scripts/certs/signing_private.pem (NEVER commit this)
static constexpr const char* kSigningPublicKeyPEM = R"(
-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEAenbLqZcQ4eoWsVvjpg3FQrkd0V1Q8b3P/OJSMkudvWo=
-----END PUBLIC KEY-----
)";

void TranslationDownloader::verifyAndProcess(
    const QString& appId,
    const QString& dataUrl,
    const QString& tempPath,
    const QString& dirName)
{
    MAKINE_ZONE_NAMED("TranslationDownloader::verifyAndProcess");

    // Build .sig URL: replace .mkpkg extension with .sig
    QString sigUrl = dataUrl;
    if (sigUrl.endsWith(QStringLiteral(".mkpkg"))) {
        sigUrl.chop(6);  // Remove ".mkpkg"
        sigUrl += QStringLiteral(".sig");
    } else {
        sigUrl += QStringLiteral(".sig");
    }

    qDebug() << "TranslationDownloader: downloading signature" << appId
             << "from" << sigUrl;

    QNetworkRequest sigReq{QUrl{sigUrl}};
    sigReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::SameOriginRedirectPolicy);
    sigReq.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MakineAI/0.1"));

    QNetworkReply* sigReply = m_nam.get(sigReq);

    connect(sigReply, &QNetworkReply::finished, this,
        [this, sigReply, appId, tempPath, dirName]()
    {
        sigReply->deleteLater();

        // Check if download was cancelled while fetching .sig
        auto it = m_activeDownloads.find(appId);
        if (it != m_activeDownloads.end() && it->cancelled) {
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadCancelled(appId);
            return;
        }

        // --- .sig download error handling ---

        if (sigReply->error() != QNetworkReply::NoError) {
            qWarning() << "TranslationDownloader: SECURITY - signature download failed for"
                       << appId << ":" << sigReply->errorString();
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            CrashReporter::addBreadcrumb("security",
                QStringLiteral("sigDownloadFailed: %1").arg(appId).toUtf8().constData());
            emit downloadError(appId,
                tr("Signature verification failed: cannot download signature file"));
            return;
        }

        const QByteArray sigData = sigReply->readAll();
        if (sigData.isEmpty()) {
            qWarning() << "TranslationDownloader: SECURITY - empty signature file for" << appId;
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadError(appId,
                tr("Signature verification failed: empty signature file"));
            return;
        }

        // --- Parse .sig JSON ---

        QJsonParseError parseError;
        QJsonDocument sigDoc = QJsonDocument::fromJson(sigData, &parseError);

        if (parseError.error != QJsonParseError::NoError || !sigDoc.isObject()) {
            qWarning() << "TranslationDownloader: SECURITY - malformed signature JSON for"
                       << appId << ":" << parseError.errorString();
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadError(appId,
                tr("Signature verification failed: malformed signature file"));
            return;
        }

        QJsonObject sigObj = sigDoc.object();
        const QString sigAlgorithm = sigObj.value(QStringLiteral("algorithm")).toString();
        const QString sigHash      = sigObj.value(QStringLiteral("hash")).toString();
        const QString sigSignature = sigObj.value(QStringLiteral("signature")).toString();
        const QString sigKeyId     = sigObj.value(QStringLiteral("keyId")).toString();

        // Validate required fields
        if (sigHash.isEmpty() || sigSignature.isEmpty()) {
            qWarning() << "TranslationDownloader: SECURITY - incomplete signature data for"
                       << appId;
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadError(appId,
                tr("Signature verification failed: missing hash or signature"));
            return;
        }

        // Validate algorithm (must be ed25519)
        if (!sigAlgorithm.isEmpty() &&
            sigAlgorithm.compare(QStringLiteral("ed25519"), Qt::CaseInsensitive) != 0) {
            qWarning() << "TranslationDownloader: SECURITY - unsupported signature algorithm"
                       << sigAlgorithm << "for" << appId;
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadError(appId,
                tr("Signature verification failed: unsupported algorithm '%1'").arg(sigAlgorithm));
            return;
        }

        // --- Compute SHA-256 hash of the downloaded .mkpkg ---

        QFile pkgFile(tempPath);
        if (!pkgFile.open(QIODevice::ReadOnly)) {
            qWarning() << "TranslationDownloader: SECURITY - cannot open package for hashing"
                       << appId;
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            emit downloadError(appId,
                tr("Signature verification failed: cannot read downloaded package"));
            return;
        }

        QCryptographicHash hasher(QCryptographicHash::Sha256);
        // Stream the file through the hasher to avoid loading entire file into memory
        constexpr qint64 kHashBufferSize = 65536;
        char hashBuf[kHashBufferSize];
        qint64 bytesRead;
        while ((bytesRead = pkgFile.read(hashBuf, kHashBufferSize)) > 0) {
            hasher.addData(QByteArrayView(hashBuf, bytesRead));
        }
        pkgFile.close();

        const QByteArray computedHashRaw = hasher.result();
        const QString computedHash = QStringLiteral("sha256:")
            + QString::fromLatin1(computedHashRaw.toHex());

        // --- Compare hash ---

        // The .sig hash field is "sha256:<hex>". Compare directly.
        if (computedHash != sigHash) {
            qWarning() << "TranslationDownloader: SECURITY - hash mismatch for" << appId
                       << "expected:" << sigHash << "got:" << computedHash;
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            CrashReporter::addBreadcrumb("security",
                QStringLiteral("hashMismatch: %1").arg(appId).toUtf8().constData());
            emit downloadError(appId,
                tr("Signature verification failed: package hash mismatch (possible tampering)"));
            return;
        }

        qDebug() << "TranslationDownloader: hash verified for" << appId;

        // --- Verify Ed25519 signature ---

        // The signature is computed over the hash string (the value of the "hash" field)
        const QByteArray hashData = sigHash.toUtf8();
        const QByteArray sigBytes = sigSignature.toUtf8();

        if (!verifyEd25519Signature(hashData, sigBytes)) {
            qWarning() << "TranslationDownloader: SECURITY - Ed25519 signature INVALID for"
                       << appId << "(keyId:" << sigKeyId << ")";
            QFile::remove(tempPath);
            m_activeDownloads.remove(appId);
            emit activeDownloadsChanged();
            CrashReporter::addBreadcrumb("security",
                QStringLiteral("sigInvalid: %1 keyId=%2").arg(appId, sigKeyId).toUtf8().constData());
            emit downloadError(appId,
                tr("Signature verification failed: invalid signature (possible tampering)"));
            return;
        }

        qDebug() << "TranslationDownloader: signature verified for" << appId
                 << "(keyId:" << sigKeyId << ")";
        CrashReporter::addBreadcrumb("security",
            QStringLiteral("sigVerified: %1").arg(appId).toUtf8().constData());

        // --- Signature valid, continue to extraction ---
        emit extractionStarted(appId);
        processDownloadedFile(appId, tempPath, dirName);
    });
}

bool TranslationDownloader::verifyEd25519Signature(
    const QByteArray& data,
    const QByteArray& sigBase64)
{
    // Decode the base64 signature
    QByteArray sigRaw = QByteArray::fromBase64(sigBase64);
    if (sigRaw.isEmpty()) {
        qWarning() << "TranslationDownloader: failed to decode base64 signature";
        return false;
    }

    // Ed25519 signatures are exactly 64 bytes
    if (sigRaw.size() != 64) {
        qWarning() << "TranslationDownloader: invalid Ed25519 signature size:"
                   << sigRaw.size() << "(expected 64)";
        return false;
    }

    // Load the embedded Ed25519 public key
    BIO* bio = BIO_new_mem_buf(kSigningPublicKeyPEM,
                               static_cast<int>(strlen(kSigningPublicKeyPEM)));
    if (!bio) {
        qWarning() << "TranslationDownloader: failed to create BIO for public key";
        return false;
    }

    EVP_PKEY* pubKey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!pubKey) {
        qWarning() << "TranslationDownloader: failed to parse embedded Ed25519 public key";
        return false;
    }

    // Verify the signature using EVP_DigestVerify (Ed25519 is a single-shot algorithm).
    // For Ed25519, the digest parameter must be nullptr — Ed25519 does its own hashing
    // internally (SHA-512), unlike RSA/ECDSA which require an external digest.
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pubKey);
        qWarning() << "TranslationDownloader: failed to create EVP_MD_CTX";
        return false;
    }

    bool verified = false;

    // Ed25519: pass nullptr for digest type (it uses its own SHA-512 internally)
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pubKey) == 1) {
        // Ed25519 uses one-shot verify — EVP_DigestVerify instead of Update+Final
        int result = EVP_DigestVerify(
            ctx,
            reinterpret_cast<const unsigned char*>(sigRaw.constData()),
            static_cast<size_t>(sigRaw.size()),
            reinterpret_cast<const unsigned char*>(data.constData()),
            static_cast<size_t>(data.size()));
        verified = (result == 1);
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubKey);

    return verified;
}

#endif // !MAKINEAI_UI_ONLY

} // namespace makineai
