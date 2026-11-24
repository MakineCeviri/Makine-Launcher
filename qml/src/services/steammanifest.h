// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri
#pragma once

// What an installed Steam game's appmanifest_<appid>.acf says about the build
// on disk: "buildid" (Steam's public-branch build number, the same number the
// catalogue's gameVersion.steamBuild and assets/game-builds.json carry) and
// "LastUpdated" (Unix seconds of the last update Steam applied).
//
// A patch made for build N and a game at build M > N is the compatibility
// warning on the game page (patchcompat.h). Plain std only: no Qt, testable
// next to the parser (test_vdfparser).

#include "vdfparser.h"

#include <cstdint>
#include <string>

namespace makine::steammanifest {

struct AppBuild {
    int64_t buildId{0};       // 0 = unknown
    int64_t lastUpdated{0};   // Unix seconds, 0 = unknown
};

namespace detail {
// Digits only, no sign, below 2^53; anything else is unknown (0).
inline int64_t positiveNumber(const std::string& s)
{
    if (s.empty() || s.size() > 15) return 0;
    int64_t v = 0;
    for (const char c : s) {
        if (c < '0' || c > '9') return 0;
        v = v * 10 + (c - '0');
    }
    return v;
}
} // namespace detail

/// Read buildid / LastUpdated from appmanifest text. Missing or malformed
/// values come back as 0, never an error.
inline AppBuild parseAppBuild(const std::string& acfContent)
{
    AppBuild out;
    const auto root = vdf::parse(acfContent);
    if (!root) return out;
    const vdf::Node* state = root->find("AppState");
    if (!state) state = &*root;
    out.buildId = detail::positiveNumber(state->getString("buildid"));
    out.lastUpdated = detail::positiveNumber(state->getString("LastUpdated"));
    return out;
}

} // namespace makine::steammanifest
