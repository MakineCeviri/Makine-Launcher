// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

/**
 * @file networksecurity.h
 * @brief Shared network security: TLS error policy, rate limiting, request hardening
 * @copyright (c) 2026 MakineCeviri Team
 *
 * All QNetworkAccessManager instances MUST use these helpers.
 * Provides: the TLS error policy for makineceviri.org domains,
 * per-machine download rate limiting, and request hardening.
 */

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QSettings>
#include <QDateTime>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcNetSec)

namespace makine::security {

// Our own hosts: the catalogue, the recipes, the packages, the updates.
inline bool isTrustedDomain(const QString& host) {
    return host.endsWith(QStringLiteral("makineceviri.org"));
}

/**
 * @brief Install the TLS error policy on a QNetworkAccessManager.
 *
 * MUST be called on every QNAM instance that communicates with makineceviri.org.
 *
 * A certificate the system does not trust is refused, always. This used to
 * ignore the errors whenever a certificate in the chain carried a "pinned"
 * key — but the backup pin was the key of Google Trust Services' WE1
 * intermediate, a public CA. Any certificate WE1 signed passed, for another
 * host name or long expired; with the OpenSSL backend, which reports the chain
 * as the server sent it, a self-signed certificate with the public WE1
 * certificate appended passed too. Whoever sat on the user's network then
 * served the catalogue, the recipes and the packages. The errors are only
 * logged now, so a refused connection still says why.
 */
inline void installTlsPinning(QNetworkAccessManager* nam) {
    QObject::connect(nam, &QNetworkAccessManager::sslErrors,
        nam, [](QNetworkReply* reply, const QList<QSslError>& errors) {
            const auto host = reply->url().host();
            if (!isTrustedDomain(host))
                return;
            QStringList reasons;
            for (const QSslError& e : errors)
                reasons << e.errorString();
            qCWarning(lcNetSec) << "TLS refused for" << host << "—" << reasons.join(QStringLiteral("; "));
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
