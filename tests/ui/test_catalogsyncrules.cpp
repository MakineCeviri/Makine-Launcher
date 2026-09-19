// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
//
// The decision that says whether a catalog correction ever reaches a client.
//
// The numbers below are the live ones read on 2026-09-19: the catalog API
// (makineceviri.org/api/v2/catalog/meta) reported version 12 with 221 games,
// while the CDN index (cdn.makineceviri.org/assets/index.json) carried its own
// version 19 with 239 games and had not been regenerated since 2026-05-18.
// Under the old "serverVersion <= localVersion" test that pairing meant every
// installed client decided it was already current on every launch — for four
// months. Synthetic numbers would not have caught it, so the real ones stay.

#include <gtest/gtest.h>
#include "catalogsyncrules.h"

using namespace makine::catalogsync;

namespace {

constexpr int kApiVersion   = 12;  // /api/v2/catalog/meta, 2026-09-19
constexpr int kCdnIndexVersion = 19;  // assets/index.json "version", 2026-05-18

} // namespace

// ===== The regression itself =====

TEST(CatalogSyncRules, CdnIndexCounterPersistedLocallyDoesNotCountAsCurrent)
{
    // The exact wedge: a local number issued by the CDN publish step, sitting
    // above the API's. "Higher" must not read as "newer".
    EXPECT_FALSE(catalogIsCurrent(kApiVersion, kCdnIndexVersion, /*storeEmpty=*/false));
}

TEST(CatalogSyncRules, WedgedClientTakesTheFullCatalogRatherThanABackwardsDelta)
{
    // since=19 against a server at 12 describes no range the API can answer,
    // so the gap must not be attempted as a delta.
    EXPECT_FALSE(canUseDelta(kApiVersion, kCdnIndexVersion));
}

TEST(CatalogSyncRules, ResyncHealsTheWedgeInOnePass)
{
    // After the full fetch persists the API's number, the next launch is quiet.
    EXPECT_TRUE(catalogIsCurrent(kApiVersion, kApiVersion, /*storeEmpty=*/false));
}

// ===== Ordinary operation must be unchanged =====

TEST(CatalogSyncRules, MatchingVersionsAreCurrent)
{
    EXPECT_TRUE(catalogIsCurrent(12, 12, false));
    EXPECT_TRUE(catalogIsCurrent(1, 1, false));
}

TEST(CatalogSyncRules, ServerAheadIsNotCurrent)
{
    EXPECT_FALSE(catalogIsCurrent(13, 12, false));
}

TEST(CatalogSyncRules, EmptyStoreIsNeverCurrentEvenAtMatchingVersions)
{
    // A version file left behind by a cleared cache must not suppress the fetch
    // that would refill it.
    EXPECT_FALSE(catalogIsCurrent(12, 12, /*storeEmpty=*/true));
}

TEST(CatalogSyncRules, FreshInstallIsNotCurrent)
{
    EXPECT_FALSE(catalogIsCurrent(12, 0, true));
    EXPECT_FALSE(catalogIsCurrent(12, 0, false));
}

TEST(CatalogSyncRules, UnreadableServerVersionIsNotCurrent)
{
    // A meta response whose "version" is missing parses as 0. Treating that as
    // a match would suppress the sync on a malformed answer.
    EXPECT_FALSE(catalogIsCurrent(0, 0, false));
}

// ===== Delta window =====

TEST(CatalogSyncRules, SmallForwardGapUsesDelta)
{
    EXPECT_TRUE(canUseDelta(13, 12));
    EXPECT_TRUE(canUseDelta(62, 12));  // exactly the 50-version window
}

TEST(CatalogSyncRules, GapBeyondTheWindowTakesTheFullCatalog)
{
    EXPECT_FALSE(canUseDelta(63, 12));
}

TEST(CatalogSyncRules, FreshInstallTakesTheFullCatalog)
{
    EXPECT_FALSE(canUseDelta(12, 0));
}

TEST(CatalogSyncRules, EqualVersionsNeedNoDelta)
{
    EXPECT_FALSE(canUseDelta(12, 12));
}
