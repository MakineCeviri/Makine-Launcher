// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

/**
 * @file test_catalogproxymodel.cpp
 * @brief Characterization tests for CatalogProxyModel's fuzzy search engine.
 *
 * These lock down the search behaviour (Turkish folding, fuzzy ranking,
 * acronym match, early-return on identical filter, offset/limit/wrap slicing)
 * so the 0.1.2.1 search-crash fix and any future refactor cannot silently
 * regress the engine. The crash itself is a GUI/timing heap race in the
 * ListView delegate cycle and is NOT reproducible at this layer — it is
 * validated post-release via Sentry.
 *
 * The `SameFilterNoRebuild` case documents the C++ mechanism the QML Layer-1
 * fix relies on: setting an identical searchFilter must not trigger a rebuild.
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "catalogproxymodel.h"
#include "supportedgamesmodel.h"

using namespace makine;

namespace {

QVariantList makeCatalog(const QStringList &names)
{
    QVariantList list;
    int appId = 1000;
    for (const QString &n : names) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), n);
        m.insert(QStringLiteral("steamAppId"), QString::number(appId++));
        list.append(m);
    }
    return list;
}

// scheduleRebuild() defers work via QTimer::singleShot(0). Spin the event
// loop enough to flush the zero-timer before asserting.
void flushRebuild()
{
    for (int i = 0; i < 5; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

QStringList proxyNames(CatalogProxyModel &p)
{
    QStringList out;
    for (int i = 0; i < p.rowCount(); ++i)
        out << p.data(p.index(i, 0), SupportedGamesModel::NameRole).toString();
    return out;
}

} // namespace

class CatalogProxyTest : public ::testing::Test
{
protected:
    SupportedGamesModel source;
    CatalogProxyModel proxy;

    void SetUp() override
    {
        source.resetFromCatalog(makeCatalog({
            QStringLiteral("The Witcher 3: Wild Hunt"),
            QStringLiteral("Witch It"),
            QStringLiteral("Portal 2"),
            QStringLiteral("Grand Theft Auto V"),
            QStringLiteral("Red Dead Redemption 2"),
            QString::fromUtf8("Türkçe Test Oyunu"),
        }));
        proxy.setSourceModel(&source);
        flushRebuild();
    }
};

// --- No filter: all rows pass through (rowLimit = -1 means "all") ---
TEST_F(CatalogProxyTest, NoFilterPassesAllRows)
{
    proxy.setRowLimit(-1);
    flushRebuild();
    EXPECT_EQ(proxy.rowCount(), 6);
}

// --- Search narrows to substring matches ---
TEST_F(CatalogProxyTest, SearchNarrowsToMatches)
{
    proxy.setRowLimit(-1);
    proxy.setSearchFilter(QStringLiteral("witch"));
    flushRebuild();
    const QStringList names = proxyNames(proxy);
    EXPECT_EQ(names.size(), 2); // "The Witcher 3" + "Witch It"
    EXPECT_TRUE(names.contains(QStringLiteral("Witch It")));
}

// --- Exact prefix substring ranks first ---
TEST_F(CatalogProxyTest, ExactSubstringRanksFirst)
{
    proxy.setRowLimit(-1);
    proxy.setSearchFilter(QStringLiteral("portal"));
    flushRebuild();
    ASSERT_GE(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.data(proxy.index(0, 0), SupportedGamesModel::NameRole).toString(),
              QStringLiteral("Portal 2"));
}

// --- Turkish folding: ASCII query matches Turkish-accented name ---
TEST_F(CatalogProxyTest, TurkishFoldingMatches)
{
    proxy.setRowLimit(-1);
    proxy.setSearchFilter(QStringLiteral("turkce")); // Türkçe → turkce
    flushRebuild();
    const QStringList names = proxyNames(proxy);
    EXPECT_TRUE(names.contains(QString::fromUtf8("Türkçe Test Oyunu")));
}

// --- Acronym match: word-boundary initials ---
TEST_F(CatalogProxyTest, AcronymMatchRanksFirst)
{
    proxy.setRowLimit(-1);
    proxy.setSearchFilter(QStringLiteral("gta")); // Grand Theft Auto
    flushRebuild();
    ASSERT_GE(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.data(proxy.index(0, 0), SupportedGamesModel::NameRole).toString(),
              QStringLiteral("Grand Theft Auto V"));
}

// --- Identical filter value must NOT trigger a rebuild (early-return) ---
// This is the C++ guarantee the QML Layer-1 crash fix depends on: freezing the
// hidden strip's filter to a constant means no rebuild while searching.
TEST_F(CatalogProxyTest, SameFilterNoRebuild)
{
    proxy.setSearchFilter(QStringLiteral("witch"));
    flushRebuild();

    int resetCount = 0;
    QObject::connect(&proxy, &QAbstractItemModel::modelReset,
                     [&resetCount]() { ++resetCount; });
    proxy.setSearchFilter(QStringLiteral("witch")); // identical → early return
    flushRebuild();
    EXPECT_EQ(resetCount, 0);
}

// --- offset + limit slice the result window ---
TEST_F(CatalogProxyTest, OffsetLimitSlicing)
{
    proxy.setRowOffset(2);
    proxy.setRowLimit(2);
    flushRebuild();
    EXPECT_EQ(proxy.rowCount(), 2);
}

// --- wrapAround doubles the exposed row count ---
TEST_F(CatalogProxyTest, WrapAroundDoublesCount)
{
    proxy.setRowLimit(3);
    proxy.setWrapAround(true);
    flushRebuild();
    EXPECT_EQ(proxy.rowCount(), 6); // 3 visible * 2 (wrap mirror)
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
