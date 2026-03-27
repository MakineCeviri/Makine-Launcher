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
 *   assets/banners/*.png        - Announcement banners
 *   assets/update.json          - Self-update metadata
 *   data/{id}.makine            - Encrypted translation packages
 */

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
inline constexpr auto kCatalogUrl   = "https://makineceviri.org/api/v2/catalog.json";
inline constexpr auto kCatalogMeta  = "https://makineceviri.org/api/v2/catalog/meta";
inline constexpr auto kCatalogDelta = "https://makineceviri.org/api/v2/catalog/delta";
inline constexpr auto kGameDetail   = "https://makineceviri.org/api/v2/games/";
inline constexpr auto kTelemetry    = "https://makineceviri.org/api/v2/telemetry";

// Shared User-Agent for all API requests
inline constexpr auto kUserAgent    = "Makine-Launcher/0.1";

} // namespace makine::cdn
