/**
 * @file test_ssl_pinning.cpp
 * @brief Unit tests for SSL certificate pinning
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/ssl_pinning.hpp>

#include <string>

namespace makine {
namespace testing {

using namespace makine::ssl;

class SslPinningTest : public ::testing::Test {};

// =========================================================================
// extractHost
// =========================================================================

TEST_F(SslPinningTest, ExtractHost_SimpleHttps) {
    EXPECT_EQ(extractHost("https://example.com"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_WithPath) {
    EXPECT_EQ(extractHost("https://example.com/path/to/resource"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_WithPort) {
    EXPECT_EQ(extractHost("https://example.com:8443/path"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_WithUserInfo) {
    EXPECT_EQ(extractHost("https://user:pass@example.com/path"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_HttpOnly) {
    EXPECT_EQ(extractHost("http://example.com"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_NoScheme) {
    EXPECT_EQ(extractHost("example.com/path"), "");
}

TEST_F(SslPinningTest, ExtractHost_EmptyUrl) {
    EXPECT_EQ(extractHost(""), "");
}

TEST_F(SslPinningTest, ExtractHost_MalformedUrl) {
    EXPECT_EQ(extractHost("://broken"), "broken");
}

TEST_F(SslPinningTest, ExtractHost_IPv4Address) {
    EXPECT_EQ(extractHost("https://192.168.1.1:443/api"), "192.168.1.1");
}

TEST_F(SslPinningTest, ExtractHost_LowercasesHost) {
    EXPECT_EQ(extractHost("https://Example.COM/path"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_WithQueryString) {
    EXPECT_EQ(extractHost("https://api.example.com?key=value"), "api.example.com");
}

TEST_F(SslPinningTest, ExtractHost_WithFragment) {
    EXPECT_EQ(extractHost("https://example.com#section"), "example.com");
}

TEST_F(SslPinningTest, ExtractHost_SchemeEndAtStringEnd) {
    EXPECT_EQ(extractHost("ftp://"), "");
}

// =========================================================================
// isPinnedDomain
// =========================================================================

TEST_F(SslPinningTest, IsPinnedDomain_KnownDomain) {
    EXPECT_TRUE(isPinnedDomain("https://makineceviri.org/api"));
}

TEST_F(SslPinningTest, IsPinnedDomain_CdnDomain) {
    EXPECT_TRUE(isPinnedDomain("https://cdn.makineceviri.org/data/pkg.makine"));
}

TEST_F(SslPinningTest, IsPinnedDomain_UnknownDomain) {
    EXPECT_FALSE(isPinnedDomain("https://google.com/translate"));
}

TEST_F(SslPinningTest, IsPinnedDomain_EmptyUrl) {
    EXPECT_FALSE(isPinnedDomain(""));
}

TEST_F(SslPinningTest, IsPinnedDomain_MalformedUrl) {
    EXPECT_FALSE(isPinnedDomain("not-a-url"));
}

TEST_F(SslPinningTest, IsPinnedDomain_SubdomainNotPinned) {
    EXPECT_FALSE(isPinnedDomain("https://sub.makineceviri.org/api"));
}

// =========================================================================
// buildPinString
// =========================================================================

TEST_F(SslPinningTest, BuildPinString_ValidDomain) {
    auto pins = buildPinString("makineceviri.org");
    EXPECT_FALSE(pins.empty());
    EXPECT_NE(pins.find("sha256//"), std::string::npos);
    // Two pins (primary + backup) separated by ';'
    EXPECT_NE(pins.find(';'), std::string::npos);
}

TEST_F(SslPinningTest, BuildPinString_CdnDomain) {
    auto pins = buildPinString("cdn.makineceviri.org");
    EXPECT_FALSE(pins.empty());
    EXPECT_NE(pins.find("sha256//"), std::string::npos);
}

TEST_F(SslPinningTest, BuildPinString_UnknownDomain) {
    auto pins = buildPinString("unknown.example.com");
    EXPECT_TRUE(pins.empty());
}

TEST_F(SslPinningTest, BuildPinString_EmptyDomain) {
    auto pins = buildPinString("");
    EXPECT_TRUE(pins.empty());
}

// =========================================================================
// applySslPinning
// =========================================================================

TEST_F(SslPinningTest, ApplySslPinning_NullCurlHandle) {
    EXPECT_FALSE(applySslPinning(nullptr, "https://makineceviri.org/api"));
}

TEST_F(SslPinningTest, ApplySslPinning_EmptyUrl) {
    int fakeCurl = 0;
    EXPECT_FALSE(applySslPinning(reinterpret_cast<CURL*>(&fakeCurl), ""));
}

TEST_F(SslPinningTest, ApplySslPinning_UnpinnedDomain) {
    // Non-pinned domain: function returns false before calling curl_easy_setopt
    int fakeCurl = 0;
    EXPECT_FALSE(applySslPinning(reinterpret_cast<CURL*>(&fakeCurl), "https://google.com"));
}

TEST_F(SslPinningTest, ApplySslPinning_NullHandleWithUnpinnedDomain) {
    EXPECT_FALSE(applySslPinning(nullptr, "https://google.com"));
}

// =========================================================================
// PINNED_CERTS array
// =========================================================================

TEST_F(SslPinningTest, PinnedCertsHasExpectedCount) {
    EXPECT_EQ(PINNED_CERTS.size(), 4u);
}

TEST_F(SslPinningTest, PinnedCertsHasPrimaryAndBackup) {
    int primaryCount = 0;
    int backupCount = 0;
    for (const auto& pin : PINNED_CERTS) {
        if (pin.isBackup) ++backupCount;
        else ++primaryCount;
    }
    EXPECT_EQ(primaryCount, 2);
    EXPECT_EQ(backupCount, 2);
}

TEST_F(SslPinningTest, PinnedCertsAllHaveSha256Prefix) {
    for (const auto& pin : PINNED_CERTS) {
        EXPECT_TRUE(pin.pinHash.starts_with("sha256//"))
            << "Pin for " << pin.domain << " missing sha256// prefix";
    }
}

TEST_F(SslPinningTest, PinnedDomainsHasExpectedCount) {
    EXPECT_EQ(PINNED_DOMAINS.size(), 2u);
}

TEST_F(SslPinningTest, PlaceholderDetectionReturnsFalse) {
    // Current pins should not contain placeholders
    EXPECT_FALSE(detail::pinsContainPlaceholder());
}

} // namespace testing
} // namespace makine
