# Game Detection Improvements — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix Epic DLC false positives and improve overall game detection reliability.

**Architecture:** Three targeted fixes — Epic DLC filtering, cross-store deduplication, and improved catalog matching accuracy.

**Tech Stack:** C++23, simdjson, Qt6

---

### Task 1: Epic DLC/Content Filtering

**Files:**
- Modify: `core/src/game_detector/epic_scanner.cpp:50-95`
- Test: `core/tests/test_epic_scanner.cpp` (create if not exists)

Epic `.item` manifests contain fields like `bIsIncludedItem`, `MainGameCatalogItemId`, and `bIsExecutable` that distinguish base games from DLC/content. Currently none are checked.

- [ ] **Step 1: Add DLC filtering to EpicScanner::scan()**

After parsing the manifest (line 65), add:

```cpp
// Skip DLC, add-ons, and non-executable content
bool isIncludedItem = manifest.getBool("bIsIncludedItem", false);
std::string mainGameId = manifest.getString("MainGameCatalogItemId");
bool isExecutable = manifest.getBool("bIsExecutable", true);

if (isIncludedItem || !mainGameId.empty() || !isExecutable) {
    MAKINE_LOG_DEBUG(log::SCANNER, "Epic: Skipping DLC/content '{}' (included={}, mainGame={}, exe={})",
                     manifest.getString("DisplayName"), isIncludedItem, mainGameId, isExecutable);
    metrics().increment("epic_dlc_skipped");
    continue;
}
```

Insert between lines 65 and 67 (after `const auto& manifest = *docResult;`, before `GameInfo game;`).

- [ ] **Step 2: Extract additional useful fields**

After existing field extraction (line 89), add:

```cpp
// Extract Epic-specific metadata for better matching
game.version = manifest.getString("AppVersionString");
std::string appName = manifest.getString("AppName");
if (!appName.empty() && game.name.empty())
    game.name = appName;
```

- [ ] **Step 3: Build and verify**

```bash
just dev
```

- [ ] **Step 4: Commit**

```bash
git add core/src/game_detector/epic_scanner.cpp
git commit -m "fix(core): filter DLC/content from Epic Games scanner"
```

---

### Task 2: Cross-Store Deduplication

**Files:**
- Modify: `core/src/game_detector/game_detector.cpp:127-138`

Same game installed via both Steam and Epic shows as two entries. GameDetector::scanAll() does simple concat without deduplication.

- [ ] **Step 1: Add deduplication after merge**

After the parallel scan results are merged (line 138), add deduplication logic:

```cpp
// Deduplicate cross-store entries by install path
auto dedup = [](std::vector<GameInfo>& games) {
    std::unordered_map<std::string, size_t> pathMap;
    std::vector<GameInfo> unique;
    unique.reserve(games.size());

    for (auto& g : games) {
        std::string normPath = g.installPath.string();
        std::transform(normPath.begin(), normPath.end(), normPath.begin(), ::tolower);

        auto it = pathMap.find(normPath);
        if (it != pathMap.end()) {
            // Prefer Steam over Epic (Steam has AppID for catalog matching)
            auto& existing = unique[it->second];
            if (g.id.store == GameStore::Steam && existing.id.store != GameStore::Steam) {
                existing = std::move(g);
            }
            MAKINE_LOG_DEBUG(log::SCANNER, "Dedup: Skipping duplicate '{}'", g.name);
            continue;
        }
        pathMap[normPath] = unique.size();
        unique.push_back(std::move(g));
    }
    games = std::move(unique);
};
dedup(allGames);
MAKINE_LOG_INFO(log::SCANNER, "After dedup: {} unique games", allGames.size());
```

- [ ] **Step 2: Build and verify**

```bash
just dev
```

- [ ] **Step 3: Commit**

```bash
git add core/src/game_detector/game_detector.cpp
git commit -m "fix(core): deduplicate cross-store game entries"
```

---

### Task 3: Improve Catalog Matching Accuracy

**Files:**
- Modify: `core/src/package_catalog/package_catalog.cpp:544-613`

Current issues:
- Tier 3 substring match is too aggressive (short names match unrelated games)
- Tier 4 token similarity doesn't handle Roman numerals (II, III, IV)
- No Steam AppID direct matching for Epic games

- [ ] **Step 1: Add minimum length guard for substring matching**

In `findMatchingAppId()`, Tier 3 (around line 580), add minimum length:

```cpp
// Tier 3: Substring — require minimum 5 chars to avoid false positives
if (normalizedFolder.size() >= 5) {
    for (const auto& [appId, pkg] : m_packages) {
        std::string normName = toLower(pkg.name);
        std::string normDir = toLower(pkg.dirName);
        if ((normName.size() >= 5 && normName.find(normalizedFolder) != std::string::npos) ||
            (normDir.size() >= 5 && normDir.find(normalizedFolder) != std::string::npos) ||
            normalizedFolder.find(normName) != std::string::npos ||
            normalizedFolder.find(normDir) != std::string::npos) {
            return appId;
        }
    }
}
```

- [ ] **Step 2: Add Roman numeral normalization to token matching**

Before Tier 4 token comparison (around line 590), add numeral normalization:

```cpp
// Normalize Roman numerals for token comparison
static const std::vector<std::pair<std::string, std::string>> numerals = {
    {"ii", "2"}, {"iii", "3"}, {"iv", "4"}, {"v", "5"},
    {"vi", "6"}, {"vii", "7"}, {"viii", "8"}, {"ix", "9"}, {"x", "10"}
};

auto normalizeTokens = [&](std::vector<std::string>& tokens) {
    for (auto& t : tokens) {
        for (const auto& [roman, arabic] : numerals) {
            if (t == roman) { t = arabic; break; }
        }
    }
};
```

Apply to both folder tokens and package name tokens before Jaccard comparison.

- [ ] **Step 3: Reduce engine mismatch penalty**

In `findMatchingGames()` (around line 822), reduce the engine contradiction penalty:

```cpp
// Engine contradiction: reduce penalty (was -20, now -10)
// Some games report different engines depending on launcher version
score -= 10;
```

- [ ] **Step 4: Build and verify**

```bash
just dev
```

- [ ] **Step 5: Commit**

```bash
git add core/src/package_catalog/package_catalog.cpp
git commit -m "fix(core): improve catalog matching accuracy — substring guard, numeral normalization"
```

---

### Task 4: Epic-to-Steam AppID Resolution

**Files:**
- Modify: `qml/src/services/gameservice.cpp:165-197`

Epic games don't have Steam AppIDs, making catalog matching harder. After detecting an Epic game, try harder to resolve its Steam AppID.

- [ ] **Step 1: Add name-based catalog lookup for Epic games**

In `detectAndMatchGame()`, after `findMatchingAppId(dirName)` fails for Epic games, also try matching by display name:

```cpp
// For Epic games: also try matching by display name (not just folder name)
if (resolvedAppId.isEmpty() && game.store == "epic") {
    resolvedAppId = m_coreBridge->findMatchingAppId(game.name);
    if (!resolvedAppId.isEmpty()) {
        MAKINE_LOG_INFO(lcGameService, "Resolved Epic game '{}' via display name: {}",
                        game.name, resolvedAppId);
    }
}
```

- [ ] **Step 2: Build and verify**

```bash
just dev
```

- [ ] **Step 3: Commit**

```bash
git add qml/src/services/gameservice.cpp
git commit -m "fix(ui): resolve Epic games via display name when folder match fails"
```

---

## Summary

| Task | Impact | Risk |
|------|--------|------|
| Epic DLC filter | Fixes Dave the Diver content bug | Low — additive filter |
| Cross-store dedup | Removes duplicate entries | Low — path-based comparison |
| Matching accuracy | Fewer false matches/misses | Medium — matching logic change |
| Epic AppID resolution | Better Epic game recognition | Low — fallback only |
