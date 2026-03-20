/**
 * @file steamdetailsservice.cpp
 * @brief Steam store details fetching and caching service
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "steamdetailsservice.h"
#include "apppaths.h"
#include "profiler.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <optional>

namespace {

constexpr qint64 kMaxSteamResponseBytes = 5 * 1024 * 1024; // 5 MB
constexpr int kMaxSteamCache = 80;

// Pure JSON parsing — no side effects, safe for background thread
std::optional<makine::SteamDetails> parseSteamJson(const QString& steamAppId,
                                                      const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return std::nullopt;

    const QJsonObject root = doc.object();
    const QJsonObject appObj = root.value(steamAppId).toObject();
    if (!appObj.value("success").toBool())
        return std::nullopt;

    const QJsonObject appData = appObj.value("data").toObject();
    if (appData.isEmpty())
        return std::nullopt;

    makine::SteamDetails details;
    details.description = appData.value("short_description").toString();
    details.releaseDate  = appData.value("release_date").toObject().value("date").toString();

    const auto devArr = appData.value("developers").toArray();
    details.developers.reserve(devArr.size());
    for (const auto& dev : devArr)
        details.developers.append(dev.toString());

    const auto pubArr = appData.value("publishers").toArray();
    details.publishers.reserve(pubArr.size());
    for (const auto& pub : pubArr)
        details.publishers.append(pub.toString());

    const auto genreArr = appData.value("genres").toArray();
    details.genres.reserve(genreArr.size());
    for (const auto& genre : genreArr)
        details.genres.append(genre.toObject().value("description").toString());

    details.metacriticScore = appData.value("metacritic").toObject().value("score").toInt(0);

    const QJsonObject platforms = appData.value("platforms").toObject();
    details.hasWindows = platforms.value("windows").toBool(true);
    details.hasMac     = platforms.value("mac").toBool(false);
    details.hasLinux   = platforms.value("linux").toBool(false);

    if (appData.value("is_free").toBool()) {
        details.price = QObject::tr("\u00DCcretsiz");
    } else {
        const QJsonObject priceObj = appData.value("price_overview").toObject();
        details.price           = priceObj.value("final_formatted").toString();
        details.discountPercent = priceObj.value("discount_percent").toInt(0);
    }

    const auto ssArr = appData.value("screenshots").toArray();
    details.screenshots.reserve(ssArr.size());
    for (const auto& ss : ssArr) {
        const QString thumbUrl = ss.toObject().value("path_thumbnail").toString();
        if (!thumbUrl.isEmpty())
            details.screenshots.append(thumbUrl);
    }

    details.backgroundUrl = appData.value("background").toString();
    details.fetchedAt     = QDateTime::currentDateTime();

    return details;
}

} // namespace

Q_LOGGING_CATEGORY(lcSteamDetails, "makine.steam")

namespace makine {

SteamDetailsService::SteamDetailsService(QObject* parent)
    : QObject(parent)
{
}

void SteamDetailsService::fetchDetails(const QString& steamAppId)
{
    MAKINE_ZONE_NAMED("SteamDetailsService::fetchDetails");
    if (steamAppId.isEmpty()) return;

    // Validate: must be numeric, max 10 digits (prevents URL injection)
    static const QRegularExpression numericOnly(QStringLiteral("^\\d{1,10}$"));
    if (!numericOnly.match(steamAppId).hasMatch()) {
        qCWarning(lcSteamDetails) << "Invalid steamAppId format:" << steamAppId;
        return;
    }

    if (m_pendingFetches.contains(steamAppId)) return;

    auto cacheIt = m_steamDetailsCache.constFind(steamAppId);
    if (cacheIt != m_steamDetailsCache.constEnd() && !cacheIt->isExpired()) {
        emit detailsFetched(steamAppId, toVariantMap(*cacheIt));
        return;
    }

    m_pendingFetches.insert(steamAppId);

    QUrl url(QStringLiteral("https://store.steampowered.com/api/appdetails?appids=%1&l=turkish")
                 .arg(steamAppId));
    QNetworkRequest request(url);
    // Browser-compatible User-Agent to bypass CF Bot Fight Mode
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Makine-Launcher/0.1");

    QNetworkReply* reply = m_networkManager.get(request);

    // Abort if response exceeds size limit (prevents memory exhaustion)
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > kMaxSteamResponseBytes) {
            qCWarning(lcSteamDetails) << "Steam API response too large, aborting";
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, steamAppId]() {
        reply->deleteLater();
        m_pendingFetches.remove(steamAppId);

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcSteamDetails) << "Steam API error for" << steamAppId << ":" << reply->errorString();
            emit detailsFetchError(steamAppId, reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();

        // Parse JSON on background thread (~100ms main thread → ~0ms)
        auto* watcher = new QFutureWatcher<std::optional<SteamDetails>>(this);
        connect(watcher, &QFutureWatcher<std::optional<SteamDetails>>::finished, this,
                [this, watcher, steamAppId]() {
            watcher->deleteLater();
            auto result = watcher->result();
            if (!result) {
                emit detailsFetchError(steamAppId,
                    QStringLiteral("Failed to parse Steam API response"));
                return;
            }

            // Cache eviction — O(1) FIFO: remove oldest entry when full.
            // Previously O(n) linear scan for expired entries; now dequeue head.
            if (m_steamDetailsCache.size() >= kMaxSteamCache) {
                // Dequeue entries until we free a slot (skip already-removed keys)
                while (!m_insertionOrder.isEmpty()
                       && m_steamDetailsCache.size() >= kMaxSteamCache) {
                    const QString oldest = m_insertionOrder.dequeue();
                    m_steamDetailsCache.remove(oldest);
                }
            }

            m_steamDetailsCache[steamAppId] = *result;
            m_insertionOrder.enqueue(steamAppId);
            saveCache();
            emit detailsFetched(steamAppId, toVariantMap(*result));
        });

        watcher->setFuture(QtConcurrent::run([steamAppId, data]() {
            return parseSteamJson(steamAppId, data);
        }));
    });
}

QVariantMap SteamDetailsService::getDetails(const QString& steamAppId) const
{
    MAKINE_ZONE_NAMED("SteamDetailsService::getDetails");
    if (steamAppId.isEmpty()) return {};

    auto cacheIt = m_steamDetailsCache.constFind(steamAppId);
    if (cacheIt != m_steamDetailsCache.constEnd() && !cacheIt->isExpired())
        return toVariantMap(*cacheIt);

    return {};
}

QVariantMap SteamDetailsService::toVariantMap(const SteamDetails& details)
{
    QVariantList screenshotList;
    screenshotList.reserve(details.screenshots.size());
    for (const auto& url : details.screenshots)
        screenshotList.append(url);

    return {
        {"description",     details.description},
        {"developers",      QVariant::fromValue(details.developers)},
        {"publishers",      QVariant::fromValue(details.publishers)},
        {"releaseDate",     details.releaseDate},
        {"genres",          QVariant::fromValue(details.genres)},
        {"metacriticScore", details.metacriticScore},
        {"hasWindows",      details.hasWindows},
        {"hasMac",          details.hasMac},
        {"hasLinux",        details.hasLinux},
        {"price",           details.price},
        {"discountPercent", details.discountPercent},
        {"screenshots",     screenshotList},
        {"backgroundUrl",   details.backgroundUrl}
    };
}

void SteamDetailsService::loadCache()
{
    MAKINE_ZONE_NAMED("SteamDetailsService::loadCache");
    const QString cachePath = AppPaths::steamDetailsCacheFile();
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject obj = it.value().toObject();
        SteamDetails details;
        details.description     = obj["description"].toString();
        details.releaseDate     = obj["releaseDate"].toString();
        details.metacriticScore = obj["metacriticScore"].toInt(0);
        details.hasWindows      = obj["hasWindows"].toBool(true);
        details.hasMac          = obj["hasMac"].toBool(false);
        details.hasLinux        = obj["hasLinux"].toBool(false);
        details.price           = obj["price"].toString();
        details.discountPercent = obj["discountPercent"].toInt(0);
        details.backgroundUrl   = obj["backgroundUrl"].toString();
        details.fetchedAt       = QDateTime::fromString(obj["fetchedAt"].toString(), Qt::ISODate);

        for (const auto& v : obj["developers"].toArray())  details.developers.append(v.toString());
        for (const auto& v : obj["publishers"].toArray())  details.publishers.append(v.toString());
        for (const auto& v : obj["genres"].toArray())      details.genres.append(v.toString());
        for (const auto& v : obj["screenshots"].toArray()) details.screenshots.append(v.toString());

        // Skip expired entries; track insertion order for O(1) eviction
        if (!details.isExpired()) {
            m_steamDetailsCache[it.key()] = details;
            m_insertionOrder.enqueue(it.key());
        }
    }

    qCDebug(lcSteamDetails) << "Loaded" << m_steamDetailsCache.size() << "cached Steam details";
}

void SteamDetailsService::saveCache()
{
    // Snapshot the cache (COW — cheap copy) and serialize in background
    QHash<QString, SteamDetails> cacheCopy = m_steamDetailsCache;
    const QString cacheDir  = AppPaths::cacheDir();
    const QString cachePath = AppPaths::steamDetailsCacheFile();

    (void)QtConcurrent::run([cacheCopy = std::move(cacheCopy), cacheDir, cachePath]() {
        MAKINE_THREAD_NAME("Worker-SteamCache");
        MAKINE_ZONE_NAMED("SteamDetailsService::saveCache (async)");

        QDir().mkpath(cacheDir);

        QJsonObject root;
        for (auto it = cacheCopy.constBegin(); it != cacheCopy.constEnd(); ++it) {
            const SteamDetails& details = it.value();
            if (details.isExpired()) continue;

            QJsonObject obj;
            obj["description"]     = details.description;
            obj["releaseDate"]     = details.releaseDate;
            obj["metacriticScore"] = details.metacriticScore;
            obj["hasWindows"]      = details.hasWindows;
            obj["hasMac"]          = details.hasMac;
            obj["hasLinux"]        = details.hasLinux;
            obj["price"]           = details.price;
            obj["discountPercent"] = details.discountPercent;
            obj["backgroundUrl"]   = details.backgroundUrl;
            obj["fetchedAt"]       = details.fetchedAt.toString(Qt::ISODate);

            QJsonArray devArr, pubArr, genreArr, ssArr;
            for (const auto& v : details.developers)  devArr.append(v);
            for (const auto& v : details.publishers)  pubArr.append(v);
            for (const auto& v : details.genres)      genreArr.append(v);
            for (const auto& v : details.screenshots) ssArr.append(v);

            obj["developers"]  = devArr;
            obj["publishers"]  = pubArr;
            obj["genres"]      = genreArr;
            obj["screenshots"] = ssArr;

            root[it.key()] = obj;
        }

        QFile file(cachePath);
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    });
}

} // namespace makine
