/**
 * @file test_package_catalog.cpp
 * @brief Unit tests for PackageCatalog module
 *
 * Copyright (c) 2026 MakineCeviri Team
 */

#include <gtest/gtest.h>
#include <makine/package_catalog.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>

namespace makine {
namespace testing {

using json = nlohmann::json;
namespace fs = std::filesystem;

class PackageCatalogTest : public ::testing::Test {
protected:
    fs::path testDir_;
    fs::path indexPath_;
    fs::path cachePath_;
    packages::PackageCatalog catalog_;

    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "makine_catalog_tests";
        cachePath_ = testDir_ / "cache";
        indexPath_ = testDir_ / "index.json";
        fs::create_directories(cachePath_);

        writeSyntheticIndex();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(testDir_, ec);
    }

    // Index uses "name" and "v" fields (matching parseIndex implementation)
    void writeSyntheticIndex() {
        json index;
        index["packages"] = {
            {"1245620", {{"name", "Elden Ring"}, {"v", "1.0.0"}, {"dirName", "elden-ring"}, {"sizeBytes", 1024}}},
            {"1716740", {{"name", "Disco Elysium"}, {"v", "2.0.0"}, {"dirName", "disco-elysium"}, {"sizeBytes", 2048}}},
            {"391540",  {{"name", "Undertale"}, {"v", "1.0.0"}, {"dirName", "undertale"}, {"sizeBytes", 512}}}
        };

        std::ofstream f(indexPath_);
        f << index.dump(2);
    }

    std::string makeEnrichmentJson() {
        json detail;
        detail["installMethod"] = {
            {"type", "script"},
            {"steps", json::array({
                {{"action", "copy"}, {"src", "patch.pak"}, {"dest", "Game/Content/Paks"}}
            })}
        };
        detail["storeIds"] = {{"epic", "epic_eldenring"}};
        detail["contributors"] = json::array({
            {{"name", "Test"}, {"role", "translator"}}
        });
        return detail.dump();
    }
};

// =========================================================================
// LOADING
// =========================================================================

TEST_F(PackageCatalogTest, LoadFromIndexSucceeds) {
    EXPECT_TRUE(catalog_.loadFromIndex(indexPath_, cachePath_));
}

TEST_F(PackageCatalogTest, LoadFromIndexNonexistentFile) {
    EXPECT_FALSE(catalog_.loadFromIndex(testDir_ / "missing.json", cachePath_));
}

TEST_F(PackageCatalogTest, LoadFromIndexInvalidJson) {
    auto badPath = testDir_ / "bad.json";
    std::ofstream(badPath) << "not valid json {{{";
    EXPECT_FALSE(catalog_.loadFromIndex(badPath, cachePath_));
}

TEST_F(PackageCatalogTest, LoadFromIndexEmptyPackages) {
    auto emptyPath = testDir_ / "empty_index.json";
    json emptyIndex;
    emptyIndex["packages"] = json::object();
    std::ofstream(emptyPath) << emptyIndex.dump();
    EXPECT_FALSE(catalog_.loadFromIndex(emptyPath, cachePath_));
}

// =========================================================================
// PACKAGE COUNT
// =========================================================================

TEST_F(PackageCatalogTest, PackageCountAfterLoad) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_EQ(catalog_.packageCount(), 3);
}

// =========================================================================
// HAS PACKAGE
// =========================================================================

TEST_F(PackageCatalogTest, HasPackageExisting) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_TRUE(catalog_.hasPackage("1245620"));
    EXPECT_TRUE(catalog_.hasPackage("1716740"));
    EXPECT_TRUE(catalog_.hasPackage("391540"));
}

TEST_F(PackageCatalogTest, HasPackageNonexistent) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_FALSE(catalog_.hasPackage("999999"));
    EXPECT_FALSE(catalog_.hasPackage(""));
}

// =========================================================================
// GET PACKAGE
// =========================================================================

TEST_F(PackageCatalogTest, GetPackageReturnsCorrectFields) {
    catalog_.loadFromIndex(indexPath_, cachePath_);

    auto pkg = catalog_.getPackage("1245620");
    ASSERT_TRUE(pkg.has_value());
    EXPECT_EQ(pkg->gameName, "Elden Ring");
    EXPECT_EQ(pkg->version, "1.0.0");
    EXPECT_EQ(pkg->dirName, "elden-ring");
    EXPECT_EQ(pkg->sizeBytes, 1024);
    EXPECT_EQ(pkg->steamAppId, "1245620");
}

TEST_F(PackageCatalogTest, GetPackageNonexistentReturnsNullopt) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_FALSE(catalog_.getPackage("999999").has_value());
}

// =========================================================================
// ALL PACKAGES
// =========================================================================

TEST_F(PackageCatalogTest, AllPackagesReturnsAllEntries) {
    catalog_.loadFromIndex(indexPath_, cachePath_);

    auto all = catalog_.allPackages();
    EXPECT_EQ(all.size(), 3u);

    // Collect all steamAppIds
    std::set<std::string> ids;
    for (const auto& pkg : all) {
        ids.insert(pkg.steamAppId);
    }
    EXPECT_TRUE(ids.count("1245620"));
    EXPECT_TRUE(ids.count("1716740"));
    EXPECT_TRUE(ids.count("391540"));
}

// =========================================================================
// ENRICH PACKAGE
// =========================================================================

TEST_F(PackageCatalogTest, IsDetailLoadedInitiallyFalse) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_FALSE(catalog_.isDetailLoaded("1245620"));
}

TEST_F(PackageCatalogTest, EnrichPackageSetsDetailLoaded) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_TRUE(catalog_.enrichPackage("1245620", makeEnrichmentJson()));
    EXPECT_TRUE(catalog_.isDetailLoaded("1245620"));

    // Verify enriched data was merged
    auto pkg = catalog_.getPackage("1245620");
    ASSERT_TRUE(pkg.has_value());
    EXPECT_EQ(pkg->installMethodType, "script");
    EXPECT_EQ(pkg->contributors.size(), 1u);
    EXPECT_EQ(pkg->contributors[0].name, "Test");
    EXPECT_EQ(pkg->contributors[0].role, "translator");
}

TEST_F(PackageCatalogTest, EnrichPackageNonexistentAppId) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_FALSE(catalog_.enrichPackage("999999", makeEnrichmentJson()));
}

TEST_F(PackageCatalogTest, EnrichPackageInvalidJson) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_FALSE(catalog_.enrichPackage("1245620", "not json {{{"));
}

// =========================================================================
// RESOLVE GAME ID
// =========================================================================

TEST_F(PackageCatalogTest, ResolveGameIdDirectSteamAppId) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_EQ(catalog_.resolveGameId("1245620"), "1245620");
}

TEST_F(PackageCatalogTest, ResolveGameIdUnknownReturnsEmpty) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_EQ(catalog_.resolveGameId("unknown_id"), "");
}

TEST_F(PackageCatalogTest, ResolveGameIdViaStoreId) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    catalog_.enrichPackage("1245620", makeEnrichmentJson());

    // After enrichment, epic_eldenring should resolve to 1245620
    EXPECT_EQ(catalog_.resolveGameId("epic_epic_eldenring"), "1245620");
}

// =========================================================================
// INSTALLED STATE
// =========================================================================

TEST_F(PackageCatalogTest, MarkInstalledMakesIsInstalledTrue) {
    packages::InstalledPackageState state;
    state.version = "1.0.0";
    state.gamePath = "C:/Games/EldenRing";
    state.installedAt = 1700000000;

    catalog_.markInstalled("1245620", state);
    EXPECT_TRUE(catalog_.isInstalled("1245620"));
}

TEST_F(PackageCatalogTest, MarkUninstalledMakesIsInstalledFalse) {
    packages::InstalledPackageState state;
    state.version = "1.0.0";
    catalog_.markInstalled("1245620", state);
    EXPECT_TRUE(catalog_.isInstalled("1245620"));

    catalog_.markUninstalled("1245620");
    EXPECT_FALSE(catalog_.isInstalled("1245620"));
}

TEST_F(PackageCatalogTest, GetInstalledStateReturnsCorrectData) {
    packages::InstalledPackageState state;
    state.version = "2.0.0";
    state.gamePath = "D:/Games/Disco";
    state.installedAt = 1700000001;
    state.gameSource = "steam";
    state.installedFiles = {"file1.txt", "file2.pak"};
    state.addedFiles = {"file1.txt"};
    state.replacedFiles = {"file2.pak"};

    catalog_.markInstalled("1716740", state);

    auto retrieved = catalog_.getInstalledState("1716740");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->version, "2.0.0");
    EXPECT_EQ(retrieved->gamePath, "D:/Games/Disco");
    EXPECT_EQ(retrieved->installedAt, 1700000001);
    EXPECT_EQ(retrieved->gameSource, "steam");
    EXPECT_EQ(retrieved->installedFiles.size(), 2u);
    EXPECT_EQ(retrieved->addedFiles.size(), 1u);
    EXPECT_EQ(retrieved->replacedFiles.size(), 1u);
}

TEST_F(PackageCatalogTest, GetInstalledStateNonexistentReturnsNullopt) {
    EXPECT_FALSE(catalog_.getInstalledState("999999").has_value());
}

// =========================================================================
// PERSISTENCE — SAVE/LOAD ROUND-TRIP
// =========================================================================

TEST_F(PackageCatalogTest, SaveAndLoadInstalledStateRoundTrip) {
    auto statePath = testDir_ / "installed_packages.json";

    // Mark two packages as installed
    packages::InstalledPackageState state1;
    state1.version = "1.0.0";
    state1.gamePath = "C:/Games/EldenRing";
    state1.installedAt = 1700000000;
    state1.gameSource = "steam";
    state1.addedFiles = {"patch.pak"};

    packages::InstalledPackageState state2;
    state2.version = "2.0.0";
    state2.gamePath = "D:/Games/Disco";
    state2.installedAt = 1700000001;
    state2.replacedFiles = {"loc.txt"};

    catalog_.markInstalled("1245620", state1);
    catalog_.markInstalled("1716740", state2);

    // Save
    catalog_.saveInstalledState(statePath);
    EXPECT_TRUE(fs::exists(statePath));

    // Load into a fresh catalog
    packages::PackageCatalog freshCatalog;
    freshCatalog.loadInstalledState(statePath);

    EXPECT_TRUE(freshCatalog.isInstalled("1245620"));
    EXPECT_TRUE(freshCatalog.isInstalled("1716740"));
    EXPECT_FALSE(freshCatalog.isInstalled("391540"));

    auto loaded1 = freshCatalog.getInstalledState("1245620");
    ASSERT_TRUE(loaded1.has_value());
    EXPECT_EQ(loaded1->version, "1.0.0");
    EXPECT_EQ(loaded1->gamePath, "C:/Games/EldenRing");
    EXPECT_EQ(loaded1->installedAt, 1700000000);
    EXPECT_EQ(loaded1->gameSource, "steam");
    EXPECT_EQ(loaded1->addedFiles.size(), 1u);

    auto loaded2 = freshCatalog.getInstalledState("1716740");
    ASSERT_TRUE(loaded2.has_value());
    EXPECT_EQ(loaded2->version, "2.0.0");
    EXPECT_EQ(loaded2->replacedFiles.size(), 1u);
}

TEST_F(PackageCatalogTest, LoadInstalledStateNonexistentFileNoOp) {
    // Should not crash, just no-op
    catalog_.loadInstalledState(testDir_ / "does_not_exist.json");
    EXPECT_FALSE(catalog_.isInstalled("1245620"));
}

// =========================================================================
// FOLDER MATCHING
// =========================================================================

TEST_F(PackageCatalogTest, FindMatchingAppIdByDirNameCaseInsensitive) {
    catalog_.loadFromIndex(indexPath_, cachePath_);

    // Exact dirName match (case-insensitive)
    EXPECT_EQ(catalog_.findMatchingAppId("Elden-Ring"), "1245620");
    EXPECT_EQ(catalog_.findMatchingAppId("UNDERTALE"), "391540");
}

TEST_F(PackageCatalogTest, FindMatchingAppIdByGameNameSubstring) {
    catalog_.loadFromIndex(indexPath_, cachePath_);

    // gameName substring match
    EXPECT_NE(catalog_.findMatchingAppId("Disco"), "");
    EXPECT_EQ(catalog_.findMatchingAppId("Disco"), "1716740");
}

TEST_F(PackageCatalogTest, FindMatchingAppIdNoMatch) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_EQ(catalog_.findMatchingAppId("Cyberpunk 2077"), "");
}

// =========================================================================
// EMPTY CATALOG
// =========================================================================

TEST_F(PackageCatalogTest, EmptyCatalogPackageCountZero) {
    EXPECT_EQ(catalog_.packageCount(), 0);
}

TEST_F(PackageCatalogTest, EmptyCatalogHasPackageFalse) {
    EXPECT_FALSE(catalog_.hasPackage("1245620"));
}

TEST_F(PackageCatalogTest, EmptyCatalogGetPackageNullopt) {
    EXPECT_FALSE(catalog_.getPackage("1245620").has_value());
}

TEST_F(PackageCatalogTest, EmptyCatalogAllPackagesEmpty) {
    EXPECT_TRUE(catalog_.allPackages().empty());
}

// =========================================================================
// VARIANT QUERIES (empty before enrichment)
// =========================================================================

TEST_F(PackageCatalogTest, GetVariantsEmptyBeforeEnrichment) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_TRUE(catalog_.getVariants("1245620").empty());
}

TEST_F(PackageCatalogTest, GetVariantTypeEmptyBeforeEnrichment) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    EXPECT_TRUE(catalog_.getVariantType("1245620").empty());
}

// =========================================================================
// ADDITIONAL EDGE CASES
// =========================================================================

TEST_F(PackageCatalogTest, IndexWithMissingNameField) {
    auto badPath = testDir_ / "bad_index.json";
    json badIndex;
    badIndex["packages"] = {
        {"12345", {{"v", "1.0.0"}, {"dirName", "test"}}}
    };
    std::ofstream(badPath) << badIndex.dump();

    packages::PackageCatalog catalog;
    // May or may not load — name could default to empty
    auto loaded = catalog.loadFromIndex(badPath, cachePath_);
    if (loaded) {
        auto pkg = catalog.getPackage("12345");
        if (pkg.has_value()) {
            // gameName should be empty or default
            SUCCEED();
        }
    }
    SUCCEED();
}

TEST_F(PackageCatalogTest, EmptyCatalogResolveGameId) {
    EXPECT_EQ(catalog_.resolveGameId("1245620"), "");
    EXPECT_EQ(catalog_.resolveGameId(""), "");
}

TEST_F(PackageCatalogTest, InstalledStateWithUnknownAppId) {
    EXPECT_FALSE(catalog_.isInstalled("unknown_app_id"));
    EXPECT_FALSE(catalog_.getInstalledState("unknown_app_id").has_value());
}

TEST_F(PackageCatalogTest, MarkUninstalledNonexistent) {
    // Marking uninstalled when never installed should be safe
    catalog_.markUninstalled("never_installed");
    EXPECT_FALSE(catalog_.isInstalled("never_installed"));
}

TEST_F(PackageCatalogTest, EnrichPackageWithEmptyJson) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    // Empty JSON object should not crash
    EXPECT_TRUE(catalog_.enrichPackage("1245620", "{}"));
}

TEST_F(PackageCatalogTest, FindMatchingAppIdEmptyString) {
    catalog_.loadFromIndex(indexPath_, cachePath_);
    // Empty string may match entries — just verify no crash
    (void)catalog_.findMatchingAppId("");
    SUCCEED();
}

} // namespace testing
} // namespace makine
