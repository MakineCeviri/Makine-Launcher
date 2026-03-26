/**
 * @file steamdetailsservice.h
 * @brief Steam store details fetching and caching service
 * @copyright (c) 2026 MakineCeviri Team
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QQueue>
#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <QStringList>
#include <QNetworkAccessManager>

namespace makine {

/**
 * @brief Steam store details for a game
 */
struct SteamDetails {
    QString description;
    QStringList developers;
    QStringList publishers;
    QString releaseDate;
    QStringList genres;
    int metacriticScore{0};
    bool hasWindows{true};
    bool hasMac{false};
    bool hasLinux{false};
    QString price;
    int discountPercent{0};
    QStringList screenshots;
    QString backgroundUrl;
    QDateTime fetchedAt;

    static constexpr int TTL_HOURS = 24;
    static constexpr int kSecondsPerHour = 3600;

    bool isExpired() const {
        return fetchedAt.isNull()
            || fetchedAt.secsTo(QDateTime::currentDateTime()) > TTL_HOURS * kSecondsPerHour;
    }
};

/**
 * @brief Fetches and caches Steam store details for games.
 *
 * Responsibilities:
 * - One-at-a-time fetch deduplication via m_pendingFetches
 * - In-memory TTL cache (SteamDetails::TTL_HOURS)
 * - Disk persistence via AppPaths::steamDetailsCacheFile()
 * - JSON parsing offloaded to QtConcurrent background thread
 */
class SteamDetailsService : public QObject
{
    Q_OBJECT

public:
    explicit SteamDetailsService(QObject* parent = nullptr);

    /**
     * @brief Trigger a network fetch for the given Steam App ID.
     * No-op if a fetch is already in-flight or a valid cached entry exists.
     * Emits detailsFetched() on success, detailsFetchError() on failure.
     */
    void fetchDetails(const QString& steamAppId);

    /**
     * @brief Synchronous cache lookup — no network call.
     * @return Cached QVariantMap if not expired, empty map otherwise.
     */
    QVariantMap getDetails(const QString& steamAppId) const;

    /**
     * @brief Load the on-disk cache into memory (call at startup).
     * Expired entries are silently discarded.
     */
    void loadCache();

    /**
     * @brief Persist the in-memory cache to disk asynchronously.
     */
    void saveCache();

signals:
    void detailsFetched(const QString& steamAppId, const QVariantMap& details);
    void detailsFetchError(const QString& steamAppId, const QString& error);

private:
    static QVariantMap toVariantMap(const SteamDetails& details);

    QNetworkAccessManager m_networkManager;
    QHash<QString, SteamDetails> m_steamDetailsCache;
    QQueue<QString> m_insertionOrder;   // FIFO eviction order for O(1) cache trim
    QSet<QString> m_pendingFetches;
};

} // namespace makine
