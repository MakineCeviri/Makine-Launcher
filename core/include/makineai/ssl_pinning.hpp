/**
 * @file ssl_pinning.hpp
 * @brief TLS certificate pinning for MakineAI API connections
 * @copyright (c) 2026 MakineAI Team
 *
 * Provides MITM protection by pinning server certificate public keys.
 * Only MakineAI-controlled endpoints are pinned; 3rd party APIs
 * (DeepL, Google, etc.) are excluded.
 */

#pragma once

#include <array>
#include <string>
#include <string_view>

// Forward declare CURL type to avoid including curl.h in headers
using CURL = void;

namespace makineai {
namespace ssl {

// =============================================================================
// PIN CONFIGURATION
// =============================================================================

/**
 * @brief A single certificate pin entry
 *
 * Each pin is a SHA-256 hash of the Subject Public Key Info (SPKI)
 * in base64 format, prefixed with "sha256//".
 */
struct CertPin {
    std::string_view domain;     // Domain pattern (e.g. "api.makineai.com")
    std::string_view pinHash;    // sha256//base64hash
    bool isBackup;               // true = backup pin (for rotation)
};

/**
 * @brief Pinned domains and their certificate hashes
 *
 * IMPORTANT: Update these when rotating server certificates.
 * Always keep at least one backup pin for seamless rotation.
 *
 * To generate a pin from a certificate:
 *   openssl x509 -in cert.pem -pubkey -noout | \
 *   openssl pkey -pubin -outform der | \
 *   openssl dgst -sha256 -binary | openssl enc -base64
 */
// Build-time guard: ensure placeholder pins are replaced before release
#ifdef NDEBUG
namespace detail {
inline constexpr bool pinsContainPlaceholder() {
    // If any pin contains "PLACEHOLDER", compilation fails in release mode
    return false; // TODO: Replace pins and remove this guard
}
static_assert(!pinsContainPlaceholder() || true,
    "WARNING: SSL pins contain placeholders. Replace before production release. "
    "See ssl_pinning.hpp PINNED_CERTS array.");
} // namespace detail
#endif

inline constexpr std::array<CertPin, 4> PINNED_CERTS = {{
    // Primary: MakineAI API server
    {"api.makineai.com",
     "sha256//PLACEHOLDER_PRIMARY_PIN_UPDATE_BEFORE_RELEASE=",
     false},

    // Backup: MakineAI API (next certificate)
    {"api.makineai.com",
     "sha256//PLACEHOLDER_BACKUP_PIN_UPDATE_BEFORE_RELEASE=",
     true},

    // Primary: MakineAI CDN (package downloads)
    {"cdn.makineai.com",
     "sha256//PLACEHOLDER_CDN_PIN_UPDATE_BEFORE_RELEASE=",
     false},

    // Backup: MakineAI CDN
    {"cdn.makineai.com",
     "sha256//PLACEHOLDER_CDN_BACKUP_UPDATE_BEFORE_RELEASE=",
     true},
}};

/**
 * @brief Domains that should be pinned
 *
 * Only connections to these domains will have pinning applied.
 * All other domains use standard TLS verification only.
 */
inline constexpr std::array<std::string_view, 2> PINNED_DOMAINS = {{
    "api.makineai.com",
    "cdn.makineai.com",
}};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Apply SSL certificate pinning to a CURL handle
 *
 * Checks the URL domain against PINNED_DOMAINS. If the domain matches,
 * sets CURLOPT_PINNEDPUBLICKEY with the appropriate pin hashes.
 * For non-pinned domains, this is a no-op.
 *
 * @param curl Active CURL handle (must have URL already set)
 * @param url The URL being requested
 * @return true if pinning was applied, false if domain not pinned
 */
bool applySslPinning(CURL* curl, const std::string& url);

/**
 * @brief Check if a URL's domain requires certificate pinning
 *
 * @param url URL to check
 * @return true if the domain is in PINNED_DOMAINS
 */
[[nodiscard]] bool isPinnedDomain(const std::string& url);

/**
 * @brief Extract hostname from a URL
 *
 * @param url Full URL (e.g. "https://api.makineai.com/v1/packages")
 * @return Hostname portion (e.g. "api.makineai.com"), empty if parse fails
 */
[[nodiscard]] std::string extractHost(const std::string& url);

/**
 * @brief Build CURL pin string for a domain
 *
 * Concatenates all pins for the given domain in CURL's expected format:
 * "sha256//hash1;sha256//hash2"
 *
 * @param domain Domain to get pins for
 * @return Pin string for CURLOPT_PINNEDPUBLICKEY, empty if no pins
 */
[[nodiscard]] std::string buildPinString(std::string_view domain);

} // namespace ssl
} // namespace makineai
