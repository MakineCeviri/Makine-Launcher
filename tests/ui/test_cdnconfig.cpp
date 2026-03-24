#include <gtest/gtest.h>
#include "cdnconfig.h"

#include <string>
#include <string_view>

namespace makine::testing {

class CdnConfigTest : public ::testing::Test {
protected:
    // Helper to check if a URL starts with "https://"
    static bool usesHttps(std::string_view url) {
        return url.starts_with("https://");
    }

    // Helper to check if a URL contains the expected domain
    static bool usesDomain(std::string_view url) {
        return url.find(cdn::kDomain) != std::string_view::npos;
    }

    // Helper to check if a string starts with kBaseUrl
    static bool startsWithBaseUrl(std::string_view url) {
        return url.starts_with(cdn::kBaseUrl);
    }
};

// 1. All URLs use HTTPS
TEST_F(CdnConfigTest, AllUrlsUseHttps) {
    EXPECT_TRUE(usesHttps(cdn::kBaseUrl));
    EXPECT_TRUE(usesHttps(cdn::kAssetsBase));
    EXPECT_TRUE(usesHttps(cdn::kImagesBase));
    EXPECT_TRUE(usesHttps(cdn::kUpdateJson));
    EXPECT_TRUE(usesHttps(cdn::kBannersBase));
    EXPECT_TRUE(usesHttps(cdn::kDataBase));
}

// 2. All URLs use the correct domain
TEST_F(CdnConfigTest, AllUrlsUseCorrectDomain) {
    EXPECT_TRUE(usesDomain(cdn::kBaseUrl));
    EXPECT_TRUE(usesDomain(cdn::kAssetsBase));
    EXPECT_TRUE(usesDomain(cdn::kImagesBase));
    EXPECT_TRUE(usesDomain(cdn::kUpdateJson));
    EXPECT_TRUE(usesDomain(cdn::kBannersBase));
    EXPECT_TRUE(usesDomain(cdn::kDataBase));
}

// 3. kAssetsBase ends with "/"
TEST_F(CdnConfigTest, AssetsBaseEndsWithSlash) {
    std::string_view url(cdn::kAssetsBase);
    EXPECT_TRUE(url.ends_with("/"));
}

// 4. kImagesBase ends with "/"
TEST_F(CdnConfigTest, ImagesBaseEndsWithSlash) {
    std::string_view url(cdn::kImagesBase);
    EXPECT_TRUE(url.ends_with("/"));
}

// 5. kBannersBase ends with "/"
TEST_F(CdnConfigTest, BannersBaseEndsWithSlash) {
    std::string_view url(cdn::kBannersBase);
    EXPECT_TRUE(url.ends_with("/"));
}

// 6. kDataBase ends with "/"
TEST_F(CdnConfigTest, DataBaseEndsWithSlash) {
    std::string_view url(cdn::kDataBase);
    EXPECT_TRUE(url.ends_with("/"));
}

// 7. kUpdateJson ends with ".json"
TEST_F(CdnConfigTest, UpdateJsonEndsWithJsonExtension) {
    std::string_view url(cdn::kUpdateJson);
    EXPECT_TRUE(url.ends_with(".json"));
}

// 8. kBaseUrl does NOT end with "/"
TEST_F(CdnConfigTest, BaseUrlDoesNotEndWithSlash) {
    std::string_view url(cdn::kBaseUrl);
    EXPECT_FALSE(url.ends_with("/"));
}

// 9. All asset URLs start with kBaseUrl
TEST_F(CdnConfigTest, AllAssetUrlsStartWithBaseUrl) {
    EXPECT_TRUE(startsWithBaseUrl(cdn::kAssetsBase));
    EXPECT_TRUE(startsWithBaseUrl(cdn::kImagesBase));
    EXPECT_TRUE(startsWithBaseUrl(cdn::kUpdateJson));
    EXPECT_TRUE(startsWithBaseUrl(cdn::kBannersBase));
    EXPECT_TRUE(startsWithBaseUrl(cdn::kDataBase));
}

// 10. kDataBase is separate from kAssetsBase (data vs assets path)
TEST_F(CdnConfigTest, DataBaseIsSeparateFromAssetsBase) {
    std::string_view data(cdn::kDataBase);
    std::string_view assets(cdn::kAssetsBase);

    // Both use the same base URL but diverge at the path segment
    EXPECT_TRUE(data.starts_with(cdn::kBaseUrl));
    EXPECT_TRUE(assets.starts_with(cdn::kBaseUrl));

    // data/ path must not be under assets/
    EXPECT_NE(data.find("/data/"), std::string_view::npos);
    EXPECT_FALSE(data.starts_with(assets));
    EXPECT_FALSE(assets.starts_with(data));
}

} // namespace makine::testing
