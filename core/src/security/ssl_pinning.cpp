/**
 * @file ssl_pinning.cpp
 * @brief TLS certificate pinning implementation
 * @copyright (c) 2026 MakineCeviri Team
 */

#include "makine/ssl_pinning.hpp"
#include "makine/logging.hpp"

#include <curl/curl.h>
#include <algorithm>
#include <string>

namespace makine {
namespace ssl {

std::string extractHost(const std::string& url) {
    // Find scheme end (://)
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return {};
    }

    auto hostStart = schemeEnd + 3;
    if (hostStart >= url.size()) {
        return {};
    }

    // Skip userinfo (user:pass@)
    auto atPos = url.find('@', hostStart);
    auto slashPos = url.find('/', hostStart);
    if (atPos != std::string::npos &&
        (slashPos == std::string::npos || atPos < slashPos)) {
        hostStart = atPos + 1;
    }

    // Find end of host (port, path, query, or end of string)
    auto hostEnd = url.find_first_of(":/?#", hostStart);
    if (hostEnd == std::string::npos) {
        hostEnd = url.size();
    }

    auto host = url.substr(hostStart, hostEnd - hostStart);

    // Lowercase for comparison
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return host;
}

bool isPinnedDomain(const std::string& url) {
    auto host = extractHost(url);
    if (host.empty()) {
        return false;
    }

    for (const auto& domain : PINNED_DOMAINS) {
        if (host == domain) {
            return true;
        }
    }
    return false;
}

std::string buildPinString(std::string_view domain) {
    std::string pins;

    for (const auto& pin : PINNED_CERTS) {
        if (pin.domain == domain) {
            if (!pins.empty()) {
                pins += ';';
            }
            pins += pin.pinHash;
        }
    }

    return pins;
}

bool applySslPinning(CURL* curl, const std::string& url) {
    if (!curl) {
        return false;
    }

    auto host = extractHost(url);
    if (host.empty()) {
        return false;
    }

    // Check if this domain requires pinning
    bool found = false;
    for (const auto& domain : PINNED_DOMAINS) {
        if (host == domain) {
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    auto pinString = buildPinString(host);
    if (pinString.empty()) {
        MAKINE_LOG_WARN(log::NETWORK,
            "Domain {} is in pinned list but has no pins configured", host);
        return false;
    }

    // Apply pinning via CURL's built-in mechanism
    // This works across SSL backends (OpenSSL, Schannel, etc.)
    CURLcode res = curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY,
                                    pinString.c_str());

    if (res == CURLE_NOT_BUILT_IN || res == CURLE_UNKNOWN_OPTION) {
#ifdef NDEBUG
        // Release builds MUST have pinning — refuse to connect without it
        MAKINE_LOG_ERROR(log::NETWORK,
            "CURL SSL pinning not supported in this build — "
            "refusing connection to {} (security requirement)", host);
        return false;
#else
        // Debug builds: warn but allow (for development flexibility)
        MAKINE_LOG_WARN(log::NETWORK,
            "CURL SSL pinning not supported in this build, "
            "falling back to standard TLS verification for {}", host);
        return false;
#endif
    }

    if (res != CURLE_OK) {
        MAKINE_LOG_WARN(log::NETWORK,
            "Failed to set SSL pin for {}: {}", host, curl_easy_strerror(res));
        return false;
    }

    MAKINE_LOG_DEBUG(log::NETWORK, "SSL pinning applied for {}", host);
    return true;
}

} // namespace ssl
} // namespace makine
