#pragma once

/**
 * @file cdnconfig.h
 * @brief Centralized CDN URL configuration
 *
 * All Cloudflare R2 CDN endpoints in one place.
 * Domain: cdn.makineceviri.net (R2 custom domain via Cloudflare)
 *
 * R2 bucket structure:
 *   assets/index.json           - Package catalog (258 entries)
 *   assets/packages/{id}.json   - Per-game detail
 *   assets/images/{id}.png      - Game cover images (260x370)
 *   assets/banners/*.png        - Announcement banners
 *   assets/update.json          - Self-update metadata
 *   data/{id}.makine            - Encrypted translation packages
 */

namespace makineai::cdn {

// Base domain — change this single line to migrate all endpoints
inline constexpr auto kDomain     = "cdn.makineceviri.net";
inline constexpr auto kBaseUrl    = "https://cdn.makineceviri.net";

// Asset endpoints
inline constexpr auto kAssetsBase = "https://cdn.makineceviri.net/assets/";
inline constexpr auto kImagesBase = "https://cdn.makineceviri.net/assets/images/";
inline constexpr auto kUpdateJson = "https://cdn.makineceviri.net/assets/update.json";
inline constexpr auto kBannersBase= "https://cdn.makineceviri.net/assets/banners/";

// Data endpoint (encrypted .makine packages)
inline constexpr auto kDataBase   = "https://cdn.makineceviri.net/data/";

} // namespace makineai::cdn
