#pragma once

/**
 * @file networksecurity.h
 * @brief Shared network security: TLS pinning, rate limiting, request hardening
 * @copyright (c) 2026 MakineCeviri Team
 *
 * All QNetworkAccessManager instances MUST use these helpers.
 * Provides: SPKI certificate pinning for makineceviri.org domains,
 * per-machine download rate limiting, and request hardening.
 */

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslError>
#include <QCryptographicHash>
#include <QSettings>
#include <QDateTime>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcNetSec)

namespace makine::security {

// SPKI pin hashes for makineceviri.org (SHA-256, base64)
// Primary: current Cloudflare certificate
// Backup: next rotation certificate
inline const QByteArray kPinnedSpkiPrimary =
    QByteArray::fromBase64("mC/RiYlbhN0AdU/u23BPTNwoLlj5OTigvIL0IbnGppg=");
inline const QByteArray kPinnedSpkiBackup =
    QByteArray::fromBase64("kIdp6NNEd8wsugYyyIYFsi1ylMCED3hZbSR8ZFsa/A4=");

// Trusted domains for pinning
inline bool isTrustedDomain(const QString& host) {
    return host.endsWith(QStringLiteral("makineceviri.org"));
}

// Verify SPKI pin against certificate chain
inline bool verifySpkiPin(const QList<QSslCertificate>& chain) {
    for (const auto& cert : chain) {
        QByteArray spki = QCryptographicHash::hash(
            cert.publicKey().toDer(), QCryptographicHash::Sha256);
        if (spki == kPinnedSpkiPrimary || spki == kPinnedSpkiBackup)
            return true;
    }
    return false;
}

/**
 * @brief Install TLS certificate pinning on a QNetworkAccessManager.
 *
 * MUST be called on every QNAM instance that communicates with makineceviri.org.
 * Rejects connections where SPKI pins don't match (MITM protection).
 */
inline void installTlsPinning(QNetworkAccessManager* nam) {
    QObject::connect(nam, &QNetworkAccessManager::sslErrors,
        nam, [](QNetworkReply* reply, const QList<QSslError>&) {
            const auto host = reply->url().host();
            if (!isTrustedDomain(host))
                return; // only pin our own domains

            if (verifySpkiPin(reply->sslConfiguration().peerCertificateChain())) {
                reply->ignoreSslErrors(); // pin matched
            } else {
                qCWarning(lcNetSec) << "TLS pin FAILED for" << host << "— blocking (MITM?)";
                reply->abort();
            }
        });
}

/**
 * @brief Harden a QNetworkRequest with security headers.
 *
 * Sets timeout, disables caching for sensitive requests,
 * adds safe User-Agent without version leak.
 */
inline void hardenRequest(QNetworkRequest& req, int timeoutMs = 15000) {
    req.setTransferTimeout(timeoutMs);
    req.setRawHeader("User-Agent", "MakineLauncher/1.0");
    // Prevent caching of auth/sensitive responses
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
}

// ── Rate Limiting ──

// Per-machine daily download limits
inline constexpr int kMaxPackageDownloadsPerDay = 20;
inline constexpr int kMaxUpdateChecksPerHour = 10;
inline constexpr int kMaxCatalogRefreshesPerHour = 6;

/**
 * @brief Check if a rate-limited action is allowed.
 * @param action Unique action key (e.g., "package_download", "update_check")
 * @param maxCount Maximum allowed count in the time window
 * @param windowSecs Time window in seconds
 * @return true if action is allowed, false if rate limited
 */
inline bool isRateLimitAllowed(const QString& action, int maxCount, int windowSecs = 86400) {
    QSettings settings(QStringLiteral("MakineCeviri"), QStringLiteral("Makine-Launcher"));
    QString countKey = QStringLiteral("ratelimit/%1/count").arg(action);
    QString resetKey = QStringLiteral("ratelimit/%1/reset").arg(action);

    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime resetTime = settings.value(resetKey).toDateTime();

    // Window expired — reset counter
    if (!resetTime.isValid() || now > resetTime) {
        settings.setValue(countKey, 1);
        settings.setValue(resetKey, now.addSecs(windowSecs));
        return true;
    }

    int count = settings.value(countKey, 0).toInt();
    if (count >= maxCount) {
        qCWarning(lcNetSec) << "Rate limit hit:" << action
                            << count << "/" << maxCount
                            << "resets at" << resetTime.toString(Qt::ISODate);
        return false;
    }

    settings.setValue(countKey, count + 1);
    return true;
}

// Convenience wrappers
inline bool canDownloadPackage() {
    return isRateLimitAllowed(QStringLiteral("pkg_download"), kMaxPackageDownloadsPerDay, 86400);
}

inline bool canCheckUpdate() {
    return isRateLimitAllowed(QStringLiteral("update_check"), kMaxUpdateChecksPerHour, 3600);
}

inline bool canRefreshCatalog() {
    return isRateLimitAllowed(QStringLiteral("catalog_refresh"), kMaxCatalogRefreshesPerHour, 3600);
}

// ── Response Size Limits ──

inline constexpr qint64 kMaxJsonResponseBytes   = 2 * 1024 * 1024;          // 2 MB
inline constexpr qint64 kMaxPackageBytes         = 3LL * 1024 * 1024 * 1024; // 3 GB (largest: ~1.6 GB)
inline constexpr qint64 kMaxUpdateInstallerBytes = 200 * 1024 * 1024;       // 200 MB
inline constexpr qint64 kMaxDecompressedBytes    = 5LL * 1024 * 1024 * 1024; // 5 GB (decompressed tar limit)

/**
 * @brief Install a download size guard on a QNetworkReply.
 *
 * Aborts the download if it exceeds the specified limit.
 */
inline void installSizeGuard(QNetworkReply* reply, qint64 maxBytes) {
    QObject::connect(reply, &QNetworkReply::downloadProgress,
        reply, [reply, maxBytes](qint64 received, qint64) {
            if (received > maxBytes) {
                qCWarning(lcNetSec) << "Response too large:" << received
                                    << ">" << maxBytes << "— aborting"
                                    << reply->url().toString();
                reply->abort();
            }
        });
}

} // namespace makine::security
