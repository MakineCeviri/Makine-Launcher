/**
 * @file ssl_pinning.hpp
 * @brief TLS certificate pinning for Makine API connections
 * @copyright (c) 2026 MakineCeviri Team
 *
 * Provides MITM protection by pinning server certificate public keys.
 * Only Makine-controlled endpoints are pinned; 3rd party APIs
 * (DeepL, Google, etc.) are excluded.
 */

#pragma once

#include <array>
#include <string>
#include <string_view>

// Forward declare CURL type to avoid including curl.h in headers
using CURL = void;

namespace makine {
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
    std::string_view domain;     // Domain pattern (e.g. "api.makineceviri.org")
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
inline constexpr std::array<CertPin, 4> PINNED_CERTS = {{
    // Primary: makineceviri.org (website + update endpoint)
    {"makineceviri.org",
     "sha256//mC/RiYlbhN0AdU/u23BPTNwoLlj5OTigvIL0IbnGppg=",
     false},

    // Backup: Cloudflare intermediate CA (survives leaf cert rotation)
    {"makineceviri.org",
     "sha256//kIdp6NNEd8wsugYyyIYFsi1ylMCED3hZbSR8ZFsa/A4=",
     true},

    // Primary: cdn.makineceviri.org (package downloads + assets)
    {"cdn.makineceviri.org",
     "sha256//MNGoZIDCbt1ZzepaJUqaasJVrbMUfnnEl6FyLjMClrE=",
     false},

    // Backup: Cloudflare intermediate CA (survives leaf cert rotation)
    {"cdn.makineceviri.org",
     "sha256//kIdp6NNEd8wsugYyyIYFsi1ylMCED3hZbSR8ZFsa/A4=",
     true},
}};

// Build-time guard: ensure placeholder pins are replaced before release
namespace detail {
inline constexpr bool pinsContainPlaceholder() {
    constexpr std::string_view marker = "PLACEHOLDER";
    for (const auto& pin : PINNED_CERTS) {
        if (pin.pinHash.find(marker) != std::string_view::npos)
            return true;
    }
    return false;
}
} // namespace detail

#ifdef MAKINE_RELEASE_VERIFIED
static_assert(!detail::pinsContainPlaceholder(),
    "SSL certificate pins contain placeholders — replace before release build. "
    "See ssl_pinning.hpp PINNED_CERTS array.");
#endif

/**
 * @brief Domains that should be pinned
 *
 * Only connections to these domains will have pinning applied.
 * All other domains use standard TLS verification only.
 */
inline constexpr std::array<std::string_view, 2> PINNED_DOMAINS = {{
    "makineceviri.org",
    "cdn.makineceviri.org",
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
 * @param url Full URL (e.g. "https://api.makineceviri.org/v1/packages")
 * @return Hostname portion (e.g. "api.makineceviri.org"), empty if parse fails
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
} // namespace makine
