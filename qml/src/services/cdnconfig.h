// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

#pragma once

/**
 * @file cdnconfig.h
 * @brief Centralized CDN URL configuration
 *
 * All Cloudflare R2 CDN endpoints in one place.
 * Domain: cdn.makineceviri.org (R2 custom domain via Cloudflare)
 *
 * R2 bucket structure:
 *   assets/index.json           - Package catalog (258 entries)
 *   assets/packages/{id}.json   - Per-game detail
 *   assets/images/{id}.png      - Game cover images (260x370)
 *   assets/banners/{name}.png   - Announcement banners
 *   assets/update.json          - Self-update metadata
 *   data/{id}.makine            - Encrypted translation packages
 */

#include <QString>
#include <QUrl>

namespace makine::cdn {

// Base domain — change this single line to migrate all endpoints
inline constexpr auto kDomain     = "cdn.makineceviri.org";
inline constexpr auto kBaseUrl    = "https://cdn.makineceviri.org";

// Asset endpoints
inline constexpr auto kAssetsBase = "https://cdn.makineceviri.org/assets/";
inline constexpr auto kImagesBase = "https://cdn.makineceviri.org/assets/images/";
inline constexpr auto kUpdateJson = "https://cdn.makineceviri.org/assets/update.json";
inline constexpr auto kBannersBase= "https://cdn.makineceviri.org/assets/banners/";

// Data endpoint (encrypted .makine packages)
inline constexpr auto kDataBase   = "https://cdn.makineceviri.org/data/";

// Catalog API (static JSON — served via Cloudflare Workers + Assets)
inline constexpr auto kCatalogUrl   = "https://cdn.makineceviri.org/assets/index.json";
inline constexpr auto kCatalogMeta  = "https://makineceviri.org/api/v2/catalog/meta";
inline constexpr auto kCatalogDelta = "https://makineceviri.org/api/v2/catalog/delta";
inline constexpr auto kGameDetail   = "https://makineceviri.org/api/v2/games/";
inline constexpr auto kTelemetry    = "https://makineceviri.org/api/v2/telemetry";

// Shared User-Agent for all API requests
inline constexpr auto kUserAgent    = "Makine-Launcher/0.1";

// Whether a catalogue URL points at our CDN. Compares the parsed host: a
// prefix check on the string also accepted
// "https://cdn.makineceviri.org.example.com/..." and
// "https://cdn.makineceviri.org@example.com/...".
inline bool isCdnUrl(const QString& url)
{
    const QUrl parsed(url, QUrl::StrictMode);
    return parsed.isValid()
        && parsed.scheme() == QLatin1String("https")
        && parsed.host() == QLatin1String(kDomain)
        && parsed.userInfo().isEmpty()
        && parsed.port(443) == 443;
}

} // namespace makine::cdn
