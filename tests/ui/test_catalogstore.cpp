/**
 * @file test_catalogstore.cpp
 * @brief Unit tests for CatalogStore — catalog persistence and delta application
 */

#include <gtest/gtest.h>
#include "catalogstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace makine::testing {

// ========== Helpers ==========

static QJsonObject makeEntry(const QString& name, const QString& version,
                             qint64 size = 1024)
{
    QJsonObject obj;
    obj["name"] = name;
    obj["v"] = version;
    obj["sizeBytes"] = size;
    obj["size"] = size / 2;
    obj["dataUrl"] = "https://cdn.makineceviri.org/data/test.makine";
    obj["checksum"] = "abc123";
    obj["dirName"] = "test_game";
    obj["externalUrl"] = "";
    obj["source"] = "";
    obj["apexTier"] = "";
    return obj;
}

static QByteArray makeIndexJson(int version,
                                const QJsonObject& packages)
{
    QJsonObject root;
    root["version"] = version;
    root["generatedAt"] = "2026-03-25T10:00:00Z";
    root["packages"] = packages;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// ========== Tests ==========

class CatalogStoreTest : public ::testing::Test {
protected:
    CatalogStore store;
};

TEST_F(CatalogStoreTest, StartsEmpty)
{
    EXPECT_TRUE(store.isEmpty());
    EXPECT_EQ(store.catalogCount(), 0);
    EXPECT_EQ(store.catalogVersion(), 0);
}

TEST_F(CatalogStoreTest, ParseCatalogEntryAllFields)
{
    QJsonObject obj;
    obj["name"] = "Dark Souls II";
    obj["v"] = "2026-03-01";
    obj["sizeBytes"] = 5000000.0;
    obj["size"] = 2500000.0;
    obj["dataUrl"] = "https://cdn.example.com/ds2.makine";
    obj["checksum"] = "sha256abc";
    obj["dirName"] = "dark_souls_2";
    obj["externalUrl"] = "https://example.com";
    obj["source"] = "apex";
    obj["apexTier"] = "pro";

    auto ce = CatalogStore::parseCatalogEntry(obj);

    EXPECT_EQ(ce.name, "Dark Souls II");
    EXPECT_EQ(ce.version, "2026-03-01");
    EXPECT_EQ(ce.sizeBytes, 5000000);
    EXPECT_EQ(ce.downloadSize, 2500000);
    EXPECT_EQ(ce.dataUrl, "https://cdn.example.com/ds2.makine");
    EXPECT_EQ(ce.checksum, "sha256abc");
    EXPECT_EQ(ce.dirName, "dark_souls_2");
    EXPECT_EQ(ce.externalUrl, "https://example.com");
    EXPECT_EQ(ce.source, "apex");
    EXPECT_EQ(ce.apexTier, "pro");
}

TEST_F(CatalogStoreTest, ParseIndexPopulatesCatalog)
{
    QJsonObject packages;
    packages["236430"] = makeEntry("Dark Souls II", "2026-03-01");
    packages["570940"] = makeEntry("Dark Souls III", "2026-02-15");

    store.parseIndex(makeIndexJson(42, packages));

    EXPECT_EQ(store.catalogCount(), 2);
    EXPECT_EQ(store.catalogVersion(), 42);
    EXPECT_TRUE(store.hasCatalogEntry("236430"));
    EXPECT_TRUE(store.hasCatalogEntry("570940"));
    EXPECT_FALSE(store.hasCatalogEntry("999999"));
}

TEST_F(CatalogStoreTest, CatalogGameName)
{
    QJsonObject packages;
    packages["236430"] = makeEntry("Dark Souls II", "2026-03-01");

    store.parseIndex(makeIndexJson(1, packages));

    EXPECT_EQ(store.catalogGameName("236430"), "Dark Souls II");
    EXPECT_EQ(store.catalogGameName("999999"), "");
}

TEST_F(CatalogStoreTest, CatalogReturnsVariantList)
{
    QJsonObject packages;
    packages["236430"] = makeEntry("Dark Souls II", "2026-03-01");

    store.parseIndex(makeIndexJson(1, packages));

    QVariantList list = store.catalog();
    EXPECT_EQ(list.size(), 1);

    QVariantMap entry = list.first().toMap();
    EXPECT_EQ(entry["steamAppId"].toString(), "236430");
    EXPECT_EQ(entry["name"].toString(), "Dark Souls II");
    EXPECT_EQ(entry["hasTranslation"].toBool(), true);
}

TEST_F(CatalogStoreTest, CatalogCacheIsInvalidatedOnParse)
{
    QJsonObject packages;
    packages["236430"] = makeEntry("Dark Souls II", "v1");

    store.parseIndex(makeIndexJson(1, packages));
    QVariantList list1 = store.catalog();
    EXPECT_EQ(list1.size(), 1);

    // Parse again with different data
    packages["570940"] = makeEntry("Dark Souls III", "v1");
    store.parseIndex(makeIndexJson(2, packages));
    QVariantList list2 = store.catalog();
    EXPECT_EQ(list2.size(), 2);
}

// ========== Delta Tests ==========

TEST_F(CatalogStoreTest, ApplyDeltaAdd)
{
    QJsonObject data = makeEntry("Elden Ring", "2026-03-25");

    bool ok = store.applyDelta("1245620", "add", data);

    EXPECT_TRUE(ok);
    EXPECT_EQ(store.catalogCount(), 1);
    EXPECT_TRUE(store.hasCatalogEntry("1245620"));
    EXPECT_EQ(store.catalogGameName("1245620"), "Elden Ring");
}

TEST_F(CatalogStoreTest, ApplyDeltaUpdate)
{
    store.applyDelta("1245620", "add", makeEntry("Elden Ring", "v1"));
    bool ok = store.applyDelta("1245620", "update", makeEntry("Elden Ring", "v2"));

    EXPECT_TRUE(ok);
    EXPECT_EQ(store.catalogCount(), 1);
}

TEST_F(CatalogStoreTest, ApplyDeltaDelete)
{
    store.applyDelta("1245620", "add", makeEntry("Elden Ring", "v1"));
    EXPECT_EQ(store.catalogCount(), 1);

    bool ok = store.applyDelta("1245620", "delete", QJsonObject());

    EXPECT_TRUE(ok);
    EXPECT_EQ(store.catalogCount(), 0);
    EXPECT_FALSE(store.hasCatalogEntry("1245620"));
}

// ========== Hardening Tests ==========

TEST_F(CatalogStoreTest, ApplyDeltaRejectsEmptyAppId)
{
    bool ok = store.applyDelta("", "add", makeEntry("Test", "v1"));

    EXPECT_FALSE(ok);
    EXPECT_EQ(store.catalogCount(), 0);
}

TEST_F(CatalogStoreTest, ApplyDeltaRejectsTraversalPattern)
{
    bool ok = store.applyDelta("../../etc/passwd", "add", makeEntry("Evil", "v1"));

    EXPECT_FALSE(ok);
    EXPECT_EQ(store.catalogCount(), 0);
}

TEST_F(CatalogStoreTest, ApplyDeltaRejectsDotDotInAppId)
{
    bool ok = store.applyDelta("../sensitive_file", "delete", QJsonObject());

    EXPECT_FALSE(ok);
}

TEST_F(CatalogStoreTest, ApplyDeltaRejectsUnknownChangeType)
{
    bool ok = store.applyDelta("1245620", "destroy", QJsonObject());

    EXPECT_FALSE(ok);
    EXPECT_EQ(store.catalogCount(), 0);
}

// ========== Changed Tracking Tests ==========

TEST_F(CatalogStoreTest, TakeChangedAppIdsReturnsAndClears)
{
    store.applyDelta("100", "add", makeEntry("Game1", "v1"));
    store.applyDelta("200", "add", makeEntry("Game2", "v1"));

    QStringList changed = store.takeChangedAppIds();
    EXPECT_EQ(changed.size(), 2);
    EXPECT_TRUE(changed.contains("100"));
    EXPECT_TRUE(changed.contains("200"));

    // Second call returns empty
    QStringList empty = store.takeChangedAppIds();
    EXPECT_TRUE(empty.isEmpty());
}

TEST_F(CatalogStoreTest, ParseIndexTracksVersionChanges)
{
    // Initial load
    QJsonObject packages;
    packages["236430"] = makeEntry("Dark Souls II", "v1");
    store.parseIndex(makeIndexJson(1, packages));
    store.takeChangedAppIds(); // clear

    // Update with new version
    packages["236430"] = makeEntry("Dark Souls II", "v2");
    store.parseIndex(makeIndexJson(2, packages));

    QStringList changed = store.takeChangedAppIds();
    EXPECT_EQ(changed.size(), 1);
    EXPECT_EQ(changed.first(), "236430");
}

// ========== Parse Error Handling ==========

TEST_F(CatalogStoreTest, ParseIndexHandlesInvalidJson)
{
    store.parseIndex("not json at all");

    EXPECT_TRUE(store.isEmpty());
    EXPECT_EQ(store.catalogVersion(), 0);
}

TEST_F(CatalogStoreTest, ParseIndexHandlesEmptyPackages)
{
    QJsonObject packages; // empty
    store.parseIndex(makeIndexJson(5, packages));

    EXPECT_TRUE(store.isEmpty());
    EXPECT_EQ(store.catalogVersion(), 5);
}

} // namespace makine::testing

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
