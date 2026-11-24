// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
    obj["source"] = "";
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
    obj["source"] = "oyunbul";

    auto ce = CatalogStore::parseCatalogEntry(obj);

    EXPECT_EQ(ce.name, "Dark Souls II");
    EXPECT_EQ(ce.version, "2026-03-01");
    EXPECT_EQ(ce.sizeBytes, 5000000);
    EXPECT_EQ(ce.downloadSize, 2500000);
    EXPECT_EQ(ce.dataUrl, "https://cdn.example.com/ds2.makine");
    EXPECT_EQ(ce.checksum, "sha256abc");
    EXPECT_EQ(ce.dirName, "dark_souls_2");
    EXPECT_EQ(ce.source, "oyunbul");
}

// "Yakında": a package of ours on the CDN is the only thing that makes a game installable.
TEST_F(CatalogStoreTest, ComingSoonWhenNoCdnPackage)
{
    QJsonObject packages;
    packages["1"] = makeEntry("Hosted", "1");
    QJsonObject none = makeEntry("NoUrl", "1");
    none.remove("dataUrl");
    packages["2"] = none;
    QJsonObject elsewhere = makeEntry("Elsewhere", "1");
    elsewhere["dataUrl"] = "https://example.com/some-page";
    elsewhere["externalUrl"] = "https://example.com/some-page";
    packages["3"] = elsewhere;
    store.parseIndex(makeIndexJson(1, packages));

    QHash<QString, bool> soon;
    for (const QVariant& v : store.catalog()) {
        const QVariantMap m = v.toMap();
        soon.insert(m.value("steamAppId").toString(), m.value("comingSoon").toBool());
        EXPECT_FALSE(m.contains("externalUrl"));
    }
    EXPECT_FALSE(soon.value("1"));
    EXPECT_TRUE(soon.value("2"));
    EXPECT_TRUE(soon.value("3"));
}

// The index's "soon" object: listed as comingSoon, a clash with "packages" keeps the package.
TEST_F(CatalogStoreTest, SoonSectionIsComingSoon)
{
    QJsonObject packages;
    packages["1"] = makeEntry("Hosted", "1");
    QJsonObject root;
    root["version"] = 1;
    root["packages"] = packages;
    QJsonObject soon;
    QJsonObject s = makeEntry("Later", "1");
    s["dataUrl"] = "https://cdn.makineceviri.org/data/ignored.makine";
    soon["2"] = s;
    soon["1"] = makeEntry("Clash", "9");
    root["soon"] = soon;
    store.parseIndex(QJsonDocument(root).toJson());

    EXPECT_EQ(store.catalogCount(), 2);
    QHash<QString, QVariantMap> by;
    for (const QVariant& v : store.catalog()) {
        const QVariantMap m = v.toMap();
        by.insert(m.value("steamAppId").toString(), m);
    }
    EXPECT_FALSE(by["1"].value("comingSoon").toBool());
    EXPECT_EQ(by["1"].value("name").toString(), "Hosted");
    EXPECT_TRUE(by["2"].value("comingSoon").toBool());
    EXPECT_FALSE(by["2"].contains("dataUrl"));
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

// ========== Patch alternatives + game version (optional keys) ==========

static QJsonObject makeAlt(const QString& id, const QString& label, const QString& v)
{
    QJsonObject a;
    a["id"] = id;
    a["label"] = label;
    a["description"] = "Ses de Türkçe.";
    a["v"] = v;
    a["sizeBytes"] = 123;
    a["size"] = 100;
    a["dataUrl"] = "https://cdn.makineceviri.org/data/1245620--" + id + "-0123456789ab.makine";
    a["checksum"] = "sha256:ab";
    a["dirName"] = "ELDEN RING--" + id;
    a["contributors"] = QJsonArray{QJsonObject{{"name", "B"}, {"role", "Seslendirme"}}};
    a["gameVersion"] = QJsonObject{{"label", ""}, {"steamBuild", 16412345}, {"date", "2026-10-28"}};
    return a;
}

static QVariantMap entryFor(const CatalogStore& store, const QString& appId)
{
    for (const QVariant& v : store.catalog())
        if (v.toMap().value("steamAppId").toString() == appId)
            return v.toMap();
    return {};
}

TEST_F(CatalogStoreTest, EntryWithoutNewKeysHasNoNewFields)
{
    QJsonObject packages;
    packages["111"] = makeEntry("Old", "1.0");
    store.parseIndex(makeIndexJson(1, packages));

    const QVariantMap e = entryFor(store, "111");
    EXPECT_FALSE(e.contains("label"));
    EXPECT_FALSE(e.contains("gameVersion"));
    EXPECT_FALSE(e.contains("alternatives"));
    EXPECT_EQ(e.value("version").toString(), "1.0");
}

TEST_F(CatalogStoreTest, ParsesLabelGameVersionAndAlternatives)
{
    QJsonObject entry = makeEntry("ELDEN RING", "2026-10-30");
    entry["label"] = "Topluluk çevirisi";
    entry["gameVersion"] = QJsonObject{{"label", "1.16.1"}, {"steamBuild", 16412345},
                                       {"date", "2026-10-28"}};
    QJsonObject offsite = makeAlt("eski", "Eski sürüm", "2026-01-01");
    offsite["dataUrl"] = "https://example.com/x.makine";
    entry["alternatives"] = QJsonArray{
        makeAlt("dublaj", "Türkçe dublaj", "2026-11-02"),
        QJsonObject{{"id", "Bad Id"}, {"label", "x"}},
        QJsonObject{{"id", "dublaj"}, {"label", "duplicate"}},
        QJsonObject{{"id", "nolabel"}},
        QJsonObject{{"id", "default"}, {"label", "reserved"}},
        offsite,
    };
    QJsonObject packages;
    packages["1245620"] = entry;
    store.parseIndex(makeIndexJson(2, packages));

    const QVariantMap e = entryFor(store, "1245620");
    EXPECT_EQ(e.value("label").toString(), QString::fromUtf8("Topluluk çevirisi"));
    const QVariantMap gv = e.value("gameVersion").toMap();
    EXPECT_EQ(gv.value("label").toString(), "1.16.1");
    EXPECT_EQ(gv.value("steamBuild").toLongLong(), 16412345);
    EXPECT_EQ(gv.value("date").toString(), "2026-10-28");

    const QVariantList alts = e.value("alternatives").toList();
    ASSERT_EQ(alts.size(), 2);
    const QVariantMap d = alts[0].toMap();
    EXPECT_EQ(d.value("id").toString(), "dublaj");
    EXPECT_EQ(d.value("version").toString(), "2026-11-02");
    EXPECT_EQ(d.value("downloadSize").toLongLong(), 100);
    EXPECT_EQ(d.value("dirName").toString(), "ELDEN RING--dublaj");
    EXPECT_EQ(d.value("contributors").toList().size(), 1);
    EXPECT_EQ(d.value("gameVersion").toMap().value("steamBuild").toLongLong(), 16412345);
    EXPECT_FALSE(d.value("comingSoon").toBool());
    EXPECT_TRUE(alts[1].toMap().value("comingSoon").toBool());   // not on our CDN
    // The default patch's own keys are untouched
    EXPECT_EQ(e.value("version").toString(), "2026-10-30");
    EXPECT_EQ(e.value("dirName").toString(), "test_game");
}

TEST_F(CatalogStoreTest, MalformedNewKeysReadAsAbsent)
{
    QJsonObject entry = makeEntry("Game", "1.0");
    entry["label"] = 42;
    entry["gameVersion"] = "1.16.1";
    entry["alternatives"] = QJsonObject{{"id", "x"}};
    QJsonObject other = makeEntry("Other", "2.0");
    other["gameVersion"] = QJsonObject{{"steamBuild", "123"}, {"label", 7}};
    other["alternatives"] = QJsonArray{42, "x", QJsonValue(),
        QJsonObject{{"id", "ok"}, {"label", "Tamam"}, {"sizeBytes", "big"},
                    {"contributors", "A"}, {"gameVersion", QJsonArray{}}}};
    QJsonObject packages;
    packages["1"] = entry;
    packages["2"] = other;
    store.parseIndex(makeIndexJson(3, packages));

    EXPECT_EQ(store.catalogCount(), 2);
    const QVariantMap e1 = entryFor(store, "1");
    EXPECT_FALSE(e1.contains("label"));
    EXPECT_FALSE(e1.contains("gameVersion"));
    EXPECT_FALSE(e1.contains("alternatives"));
    const QVariantMap e2 = entryFor(store, "2");
    EXPECT_FALSE(e2.contains("gameVersion"));
    const QVariantList alts = e2.value("alternatives").toList();
    ASSERT_EQ(alts.size(), 1);
    EXPECT_EQ(alts[0].toMap().value("sizeBytes").toLongLong(), 0);
    EXPECT_TRUE(alts[0].toMap().value("gameVersion").toMap().isEmpty());
}

TEST_F(CatalogStoreTest, AlternativeVersionChangeMarksTheGameChanged)
{
    QJsonObject entry = makeEntry("ER", "1.0");
    entry["alternatives"] = QJsonArray{makeAlt("dublaj", "Dublaj", "a")};
    QJsonObject packages;
    packages["7"] = entry;
    store.parseIndex(makeIndexJson(1, packages));
    store.takeChangedAppIds();

    entry["alternatives"] = QJsonArray{makeAlt("dublaj", "Dublaj", "b")};
    packages["7"] = entry;
    store.parseIndex(makeIndexJson(2, packages));
    EXPECT_EQ(store.takeChangedAppIds(), QStringList{"7"});
}

} // namespace makine::testing

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
