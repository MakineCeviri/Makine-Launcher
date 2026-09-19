// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// The one decision that says whether a client ever sees a catalog correction.
//
// Two independent counters both call themselves "version": the one the catalog
// API reports in /api/v2/catalog/meta, and the one the CDN publish step writes
// into assets/index.json. They are maintained by different systems and have no
// relation to each other. Comparing them froze the catalog for the whole user
// base: index.json was numbered 19 and its number got persisted as the local
// version, the API reported 12, and "12 <= 19" read as already-current on every
// launch from then on. No delta, no full fetch — and because the per-game
// detail cache is only purged when index.json itself changes, no purge either.
// Recipes corrected on the CDN reached nobody with the launcher installed.
//
// Lives outside manifestsyncservice.cpp so it can be exercised directly: the
// failure mode is silence (a sync that never runs logs nothing alarming), so
// reading the condition is not a way to know it is right.
//
// Plain ints and bools only: no Qt type, no include, nothing to construct.

namespace makine::catalogsync {

// Is the cached catalog already the one the server is offering?
//
// Equality, never <=: the stored number is only meaningful if it came from the
// same authority we are now asking. A stored number ABOVE the server's does not
// mean "newer", it means it was issued by something else — the honest response
// is to resync, which is also what heals a client already poisoned by the CDN
// index's counter.
inline bool catalogIsCurrent(int serverVersion, int localVersion, bool storeEmpty)
{
    return !storeEmpty && serverVersion > 0 && serverVersion == localVersion;
}

// Can the gap be closed with a delta, or does it need the full catalog?
//
// A delta is only meaningful forwards and only for a client that already holds
// a version the server issued. "since=19" against a server at 12 describes no
// range the server can answer.
inline bool canUseDelta(int serverVersion, int localVersion, int maxGap = 50)
{
    return localVersion > 0
        && serverVersion > localVersion
        && (serverVersion - localVersion) <= maxGap;
}

} // namespace makine::catalogsync
