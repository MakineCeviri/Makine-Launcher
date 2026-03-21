# Game Detection v2 — Filesystem & Registry Scanner + Alias Support

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect games installed outside Steam/Epic/GOG by scanning common directories and Windows Uninstall registry, and improve catalog matching with alias support.

**Architecture:** Add two new scanner methods to CoreBridge (`doScanFilesystemReal`, `doScanRegistryReal`) following the existing `doScanSteamReal` pattern. Add alias support to `PackageCatalogEntry` and `findMatchingAppId()`. Add cross-scanner deduplication by normalized install path. All scanners run sequentially in the existing worker thread.

**Tech Stack:** C++23, Qt6, Windows Registry API (via QSettings), std::filesystem

**Motivation:** `C:\Games\` contains 10+ games (GTA SA DE, Ghost of Tsushima, RDR2, UNCHARTED, etc.) that are completely invisible to the launcher because only store-specific scanners exist. Windows Uninstall registry captures most non-store installs.

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `qml/src/services/corebridge.h` | Modify | Add `doScanFilesystemReal`, `doScanRegistryReal` declarations + `m_customGamePaths` |
| `qml/src/services/corebridge.cpp` | Modify | Implement both scanners, wire into `scanAllLibraries()`, add dedup |
| `core/include/makine/package_catalog.hpp` | Modify | Add `aliases` field to `PackageCatalogEntry` |
| `core/src/package_catalog/package_catalog.cpp` | Modify | Parse aliases, add Tier 0.5 alias matching in `findMatchingAppId()` |
| `qml/src/services/gameservice.h` | Modify | Add `Q_INVOKABLE addCustomScanPath()`, `Q_INVOKABLE customScanPaths()` |
| `qml/src/services/gameservice.cpp` | Modify | Persist custom paths to settings, forward to CoreBridge |

---

### Task 1: Filesystem Scanner

**Files:**
- Modify: `qml/src/services/corebridge.h:258-268`
- Modify: `qml/src/services/corebridge.cpp:517-525` (wire in scanAllLibraries)

Scan well-known game directories and user-configured paths. Skip folders already covered by Steam/Epic/GOG library paths.

- [ ] **Step 1: Add declarations to corebridge.h**

In the private section (around line 261), add:

```cpp
void doScanFilesystemReal(QList<DetectedGame>& outGames,
                          const QSet<QString>& knownPaths);
QStringList knownGameDirectories() const;
QStringList m_customGamePaths;  // User-configured additional scan paths
```

- [ ] **Step 2: Implement `knownGameDirectories()`**

Returns a list of common game installation directories to scan. Add after `doScanGogReal()` in corebridge.cpp:

```cpp
QStringList CoreBridge::knownGameDirectories() const
{
    QStringList dirs;

    // Enumerate all mounted drives dynamically
    QStringList drives;
    for (const auto& vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isReady() || vol.isReadOnly()) continue;
        QString root = vol.rootPath();
        if (root.size() >= 2) drives.append(root.left(2)); // "C:", "D:", etc.
    }
    const QStringList knownFolders = {"Games", "Oyunlar", "Program Files/Rockstar Games"};

    for (const auto& drive : drives) {
        for (const auto& folder : knownFolders) {
            QString path = QDir::cleanPath(drive + "/" + folder);
            if (QDir(path).exists())
                dirs.append(path);
        }
    }

    // User-configured paths
    dirs.append(m_customGamePaths);

    return dirs;
}
```

- [ ] **Step 3: Implement `doScanFilesystemReal()`**

Add after `knownGameDirectories()`:

```cpp
void CoreBridge::doScanFilesystemReal(QList<DetectedGame>& outGames,
                                       const QSet<QString>& knownPaths)
{
    const QStringList gameDirs = knownGameDirectories();
    if (gameDirs.isEmpty()) return;

    qCDebug(lcCoreBridge) << "Filesystem scan: checking" << gameDirs.size() << "directories";

    for (const QString& baseDir : gameDirs) {
        QDir dir(baseDir);
        if (!dir.exists()) continue;

        const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folderName : entries) {
            QString fullPath = QDir::cleanPath(dir.absoluteFilePath(folderName));

            // Skip if already detected by store scanners
            QString normalizedPath = QDir::cleanPath(fullPath).toLower();
            if (knownPaths.contains(normalizedPath))
                continue;

            // Must contain at least one .exe to qualify as a game
            if (!hasExecutable(fullPath))
                continue;

            DetectedGame game;
            game.id = QString();  // Will be resolved via catalog matching
            game.name = folderName;
            game.installPath = fullPath;
            game.source = QStringLiteral("filesystem");

            outGames.append(game);
            qCDebug(lcCoreBridge) << "Filesystem: found" << folderName << "at" << fullPath;
        }
    }
}
```

- [ ] **Step 4: Add `hasExecutable()` helper**

Add as a static helper in corebridge.cpp (in anonymous namespace or before the scanner):

```cpp
namespace {
// Check if a directory contains at least one .exe within maxDepth levels.
// GTA SA DE has exe at Gameface/Binaries/Win64/SanAndreas.exe (depth 3).
bool hasExecutable(const QString& dirPath, int maxDepth = 3)
{
    QDirIterator it(dirPath, {"*.exe"}, QDir::Files,
                    QDirIterator::Subdirectories);
    QDir base(dirPath);
    while (it.hasNext()) {
        it.next();
        // Count depth by counting separators in relative path
        QString rel = base.relativeFilePath(it.filePath());
        if (rel.count('/') + rel.count('\\') <= maxDepth)
            return true;
    }
    return false;
}
} // namespace
```

Note: Add `#include <QDirIterator>` to corebridge.cpp includes if not already present.

- [ ] **Step 5: Wire into `scanAllLibraries()`**

In `CoreBridge::scanAllLibraries()` (corebridge.cpp:517-525), after GOG scan and before engine detection, add:

```cpp
// Collect known install paths from store scanners (for dedup)
QSet<QString> knownPaths;
for (const auto& g : games)
    knownPaths.insert(QDir::cleanPath(g.installPath).toLower());

emit scanProgress(0.80, tr("Dosya sistemi taranıyor..."));
doScanFilesystemReal(games, knownPaths);
```

Adjust the existing progress values (final values after all tasks):
- Steam: 0.10
- Epic: 0.45
- GOG: 0.60
- Filesystem: 0.75
- Registry: 0.82 (Task 2)
- Dedup: 0.87 (Task 3)
- Engine detection: 0.90

- [ ] **Step 6: Build and verify**

```bash
just dev-ui
```

- [ ] **Step 7: Commit**

```bash
git add qml/src/services/corebridge.h qml/src/services/corebridge.cpp
git commit -m "feat(ui): add filesystem scanner for non-store game detection"
```

---

### Task 2: Windows Uninstall Registry Scanner

**Files:**
- Modify: `qml/src/services/corebridge.h:258-268`
- Modify: `qml/src/services/corebridge.cpp` (after doScanFilesystemReal)

Scan `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` and the WOW6432Node variant for installed programs that look like games.

- [ ] **Step 1: Add declaration to corebridge.h**

In private section:

```cpp
void doScanRegistryReal(QList<DetectedGame>& outGames,
                        const QSet<QString>& knownPaths);
```

- [ ] **Step 2: Implement `doScanRegistryReal()`**

```cpp
void CoreBridge::doScanRegistryReal(QList<DetectedGame>& outGames,
                                     const QSet<QString>& knownPaths)
{
    // Scan both 64-bit and 32-bit uninstall registry paths
    const QStringList regPaths = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };

    // Publisher patterns that indicate a game (case-insensitive substring match)
    static const QStringList gamePublishers = {
        "rockstar", "2k games", "take-two", "capcom", "square enix",
        "bandai namco", "sega", "ubisoft", "electronic arts",
        "bethesda", "cd projekt", "techland", "fromsoftware",
        "devolver", "team17", "paradox", "koei tecmo",
        "naughty dog", "insomniac", "sucker punch", "guerrilla",
        "remedy", "playdead", "annapurna", "thq nordic", "deep silver",
        "focus entertainment", "focus home", "warner bros", "wb games",
        "sony", "playstation pc", "xbox game studios",
        "microsoft game studios", "rare ltd",
    };

    // Non-game patterns to skip (only applied when publisher is NOT a known game publisher)
    static const QStringList skipPatterns = {
        "visual c++", "redistributable",
        ".net framework", "directx", "driver", "nvidia driver", "amd driver",
        "intel driver", "update for", "hotfix", "service pack", "sdk", "runtime",
        "chrome", "firefox", "edge browser", "antivirus",
        "office", "adobe", "java runtime", "python", "node.js",
    };

    int found = 0;

    for (const QString& regPath : regPaths) {
        QSettings reg(regPath, QSettings::NativeFormat);

        for (const QString& group : reg.childGroups()) {
            reg.beginGroup(group);

            QString displayName = reg.value("DisplayName").toString().trimmed();
            QString installLocation = reg.value("InstallLocation").toString().trimmed();
            QString publisher = reg.value("Publisher").toString().trimmed();

            reg.endGroup();

            // Must have both name and install path
            if (displayName.isEmpty() || installLocation.isEmpty())
                continue;

            // Install location must exist
            installLocation = QDir::cleanPath(installLocation);
            if (!QDir(installLocation).exists())
                continue;

            // Skip if already known
            if (knownPaths.contains(installLocation.toLower()))
                continue;

            // Filter: must be from a known game publisher
            QString lowerPublisher = publisher.toLower();
            QString lowerName = displayName.toLower();

            bool isGamePublisher = false;
            for (const auto& gp : gamePublishers) {
                if (lowerPublisher.contains(gp)) {
                    isGamePublisher = true;
                    break;
                }
            }

            // Also check if name contains game-like keywords
            bool hasGameKeyword = lowerName.contains("game") ||
                                  lowerName.contains("edition") ||
                                  lowerName.contains("remastered") ||
                                  lowerName.contains("definitive");

            if (!isGamePublisher && !hasGameKeyword)
                continue;

            // Skip known non-games — but only if publisher is NOT a known game publisher
            // (prevents Xbox Game Studios / Microsoft Game Studios titles from being skipped)
            if (!isGamePublisher) {
                bool isSkipped = false;
                for (const auto& sp : skipPatterns) {
                    if (lowerName.contains(sp) || lowerPublisher.contains(sp)) {
                        isSkipped = true;
                        break;
                    }
                }
                if (isSkipped) continue;
            }

            // Must contain an executable to be a game
            if (!hasExecutable(installLocation))
                continue;

            DetectedGame game;
            game.name = displayName;
            game.installPath = installLocation;
            game.source = QStringLiteral("registry");

            outGames.append(game);
            ++found;
            qCDebug(lcCoreBridge) << "Registry: found" << displayName
                     << "by" << publisher << "at" << installLocation;
        }
    }

    qCDebug(lcCoreBridge) << "Registry scan: found" << found << "games";
}
```

- [ ] **Step 3: Wire into `scanAllLibraries()`**

After filesystem scan, before engine detection:

```cpp
// Update knownPaths with filesystem results before registry scan
for (const auto& g : games)
    knownPaths.insert(QDir::cleanPath(g.installPath).toLower());

emit scanProgress(0.85, tr("Kayıt defteri taranıyor..."));
doScanRegistryReal(games, knownPaths);
```

- [ ] **Step 4: Build and verify**

```bash
just dev-ui
```

- [ ] **Step 5: Commit**

```bash
git add qml/src/services/corebridge.h qml/src/services/corebridge.cpp
git commit -m "feat(ui): add Windows registry scanner for non-store games"
```

---

### Task 3: Cross-Scanner Deduplication

**Files:**
- Modify: `qml/src/services/corebridge.cpp:558-569`

After all scanners run (including new ones), deduplicate by normalized install path. Store scanners (Steam) get priority over filesystem/registry.

- [ ] **Step 1: Add deduplication after all scans, before engine detection**

In `scanAllLibraries()`, replace the engine-detection block with:

```cpp
// Deduplicate across all scanners by normalized install path
// Priority: steam > epic > gog > registry > filesystem
{
    QHash<QString, int> pathIndex;  // normalized path -> index in games list
    QList<DetectedGame> unique;
    unique.reserve(games.size());

    // Priority order for sources
    auto sourcePriority = [](const QString& src) -> int {
        if (src == "steam") return 0;
        if (src == "epic") return 1;
        if (src == "gog") return 2;
        if (src == "registry") return 3;
        if (src == "filesystem") return 4;
        return 5;
    };

    for (auto& game : games) {
        QString normPath = QDir::cleanPath(game.installPath).toLower();
        auto it = pathIndex.find(normPath);

        if (it != pathIndex.end()) {
            // Duplicate — keep higher priority source
            auto& existing = unique[it.value()];
            if (sourcePriority(game.source) < sourcePriority(existing.source)) {
                existing = std::move(game);
            }
            continue;
        }

        pathIndex[normPath] = unique.size();
        unique.append(std::move(game));
    }

    int removed = games.size() - unique.size();
    games = std::move(unique);
    if (removed > 0) {
        qCDebug(lcCoreBridge) << "Dedup: removed" << removed << "duplicate entries";
    }
}

// Detect engines for all found games
emit scanProgress(0.90, tr("Oyun motorları tespit ediliyor..."));
```

- [ ] **Step 2: Build and verify**

```bash
just dev-ui
```

- [ ] **Step 3: Commit**

```bash
git add qml/src/services/corebridge.cpp
git commit -m "fix(ui): deduplicate games across all scanner sources"
```

---

### Task 4: Catalog Alias Support

**Files:**
- Modify: `core/include/makine/package_catalog.hpp:111-137`
- Modify: `core/src/package_catalog/package_catalog.cpp:544-634`

Add an `aliases` field to catalog entries. In `findMatchingAppId()`, check aliases before substring matching (new Tier 1.5).

- [ ] **Step 1: Add `aliases` field to PackageCatalogEntry**

In `core/include/makine/package_catalog.hpp`, add to the struct (after `dirName` on line 123):

```cpp
std::vector<std::string> aliases;   // Alternative names for matching
```

- [ ] **Step 2: Parse aliases from index.json**

In `package_catalog.cpp`, inside `parseIndex()`, after parsing `dirName`:

```cpp
// Parse aliases (alternative names for matching)
if (pkg.contains("aliases") && pkg["aliases"].is_array()) {
    for (const auto& alias : pkg["aliases"]) {
        if (alias.is_string()) {
            entry.aliases.push_back(alias.get<std::string>());
        }
    }
}
```

- [ ] **Step 3: Parse aliases from enrichPackage() detail JSON**

In `enrichPackage()`, after other field parsing:

```cpp
// Merge aliases from detail JSON (additive — don't replace index aliases)
if (detail.contains("aliases") && detail["aliases"].is_array()) {
    for (const auto& alias : detail["aliases"]) {
        if (alias.is_string()) {
            std::string a = alias.get<std::string>();
            if (std::find(entry.aliases.begin(), entry.aliases.end(), a) == entry.aliases.end()) {
                entry.aliases.push_back(std::move(a));
            }
        }
    }
}
```

- [ ] **Step 4: Add Tier 1.5 alias matching in `findMatchingAppId()`**

After Tier 1 (exact match, line 554) and before Tier 2 (alphanumeric):

```cpp
// Tier 1.5: Check aliases (exact case-insensitive)
for (const auto& [appId, pkg] : packages_) {
    for (const auto& alias : pkg.aliases) {
        if (toLower(alias) == normalized) {
            return appId;
        }
        // Also alphanumeric-normalized alias match
        if (alphaOnly(alias) == alphaInput) {
            return appId;
        }
    }
}

// Tier 1.5b: Alias substring match (bidirectional, min 5 chars)
if (normalized.size() >= 5) {
    for (const auto& [appId, pkg] : packages_) {
        for (const auto& alias : pkg.aliases) {
            std::string aliasLower = toLower(alias);
            if (aliasLower.size() >= 5 &&
                (aliasLower.find(normalized) != std::string::npos ||
                 normalized.find(aliasLower) != std::string::npos)) {
                return appId;
            }
        }
    }
}
```

- [ ] **Step 5: Build and verify**

```bash
just dev
```

Note: This task modifies core — needs `dev` preset (with vcpkg).

- [ ] **Step 6: Commit**

```bash
git add core/include/makine/package_catalog.hpp core/src/package_catalog/package_catalog.cpp
git commit -m "feat(core): add alias support to package catalog matching"
```

---

### Task 5: Custom Scan Path API

**Files:**
- Modify: `qml/src/services/gameservice.h:121-128`
- Modify: `qml/src/services/gameservice.cpp`
- Modify: `qml/src/services/corebridge.h`
- Modify: `qml/src/services/corebridge.cpp`

Expose API for users to add custom game directories (persisted to QSettings).

- [ ] **Step 1: Add CoreBridge methods**

In `corebridge.h`, public section:

```cpp
void setCustomGamePaths(const QStringList& paths);
QStringList customGamePaths() const { return m_customGamePaths; }
```

In `corebridge.cpp`:

```cpp
void CoreBridge::setCustomGamePaths(const QStringList& paths)
{
    m_customGamePaths = paths;
}
```

- [ ] **Step 2: Add GameService Q_INVOKABLE methods**

In `gameservice.h`:

```cpp
Q_INVOKABLE void addCustomScanPath(const QString& path);
Q_INVOKABLE void removeCustomScanPath(const QString& path);
Q_INVOKABLE QStringList customScanPaths() const;
```

- [ ] **Step 3: Implement with QSettings persistence**

In `gameservice.cpp`:

```cpp
void GameService::addCustomScanPath(const QString& path)
{
    QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty() || !QDir(cleanPath).exists()) return;

    QSettings settings;
    QStringList paths = settings.value("scan/customPaths").toStringList();
    // Case-insensitive check on Windows (C:\Games == c:\games)
    bool exists = std::any_of(paths.begin(), paths.end(), [&](const QString& p) {
        return p.compare(cleanPath, Qt::CaseInsensitive) == 0;
    });
    if (!exists) {
        paths.append(cleanPath);
        settings.setValue("scan/customPaths", paths);
        if (m_coreBridge) m_coreBridge->setCustomGamePaths(paths);
        qCDebug(lcGameService) << "Added custom scan path:" << cleanPath;
    }
}

void GameService::removeCustomScanPath(const QString& path)
{
    QSettings settings;
    QStringList paths = settings.value("scan/customPaths").toStringList();
    paths.removeAll(QDir::cleanPath(path));
    settings.setValue("scan/customPaths", paths);
    if (m_coreBridge) m_coreBridge->setCustomGamePaths(paths);
}

QStringList GameService::customScanPaths() const
{
    return QSettings().value("scan/customPaths").toStringList();
}
```

- [ ] **Step 4: Load custom paths on initialize**

In `GameService::setupCoreBridge()`, after connecting signals:

```cpp
// Load user-configured scan paths
QStringList customPaths = QSettings().value("scan/customPaths").toStringList();
if (!customPaths.isEmpty()) {
    m_coreBridge->setCustomGamePaths(customPaths);
    qCDebug(lcGameService) << "Loaded" << customPaths.size() << "custom scan paths";
}
```

- [ ] **Step 5: Build and verify**

```bash
just dev-ui
```

- [ ] **Step 6: Commit**

```bash
git add qml/src/services/corebridge.h qml/src/services/corebridge.cpp \
        qml/src/services/gameservice.h qml/src/services/gameservice.cpp
git commit -m "feat(ui): add custom scan path API for game detection"
```

---

### Task 6: Name Resolution for Filesystem/Registry Games

**Files:**
- Modify: `qml/src/services/corebridge.cpp:526-558`

Filesystem and registry games don't have Steam AppIDs. After detection, the existing engine detection + catalog matching loop already handles non-Steam games via `findMatchingAppId(folderName)` and `findMatchingAppId(game.name)`. But we need to also try fingerprint matching for better accuracy.

- [ ] **Step 1: Extend the matching logic for filesystem/registry games**

In the existing resolution loop (corebridge.cpp, around line 538), add fingerprint matching as fallback:

```cpp
// For non-Steam games (Epic/GOG/filesystem/registry), storeId reverse index may
// not be populated yet. Fall back to name-based matching.
if (resolved.isEmpty() && game.source != QLatin1String("steam")) {
    QDir gameDir(game.installPath);
    resolved = pkgMgr->findMatchingAppId(gameDir.dirName());

    // Also try the display name
    if (resolved.isEmpty() && !game.name.isEmpty()) {
        resolved = pkgMgr->findMatchingAppId(game.name);
    }

    // Last resort: fingerprint-based file matching (filesystem/registry games)
    // findMatchingGamesFromFiles() is safe to call from this worker thread —
    // it reads m_localPkgManager (set before thread launch) and calls
    // detectEngineReal (already used in this thread).
    if (resolved.isEmpty() &&
        (game.source == QLatin1String("filesystem") ||
         game.source == QLatin1String("registry"))) {
        QVariantList candidates = findMatchingGamesFromFiles(game.installPath);
        if (!candidates.isEmpty()) {
            QVariantMap best = candidates.first().toMap();
            if (best.value("confidence").toInt() >= 60) {
                resolved = best.value("steamAppId").toString();
                qCDebug(lcCoreBridge) << "Fingerprint match for"
                         << game.name << "->" << resolved
                         << "confidence:" << best.value("confidence").toInt();
            }
        }
    }
}

- [ ] **Step 2: Build and verify**

```bash
just dev-ui
```

- [ ] **Step 3: Run the launcher and verify GTA SA DE is detected**

1. Launch `./build/dev-ui/Makine-Launcher.exe`
2. Wait for scan to complete
3. Check if "Grand Theft Auto San Andreas Definitive Edition" appears in the detected games list
4. Check debug log output: `QT_LOGGING_RULES="makine.*=true"`

- [ ] **Step 4: Commit**

```bash
git add qml/src/services/corebridge.cpp
git commit -m "feat(ui): add fingerprint matching for filesystem/registry games"
```

---

## Summary

| Task | Impact | Files Modified |
|------|--------|----------------|
| 1. Filesystem Scanner | **Critical** — detects C:\Games\ etc. | corebridge.h/cpp |
| 2. Registry Scanner | **High** — catches publisher installs | corebridge.h/cpp |
| 3. Cross-Scanner Dedup | **Medium** — prevents duplicates | corebridge.cpp |
| 4. Catalog Aliases | **High** — better name matching | package_catalog.hpp/cpp |
| 5. Custom Scan Paths | **Medium** — user configuration | gameservice.h/cpp, corebridge.h/cpp |
| 6. Name Resolution | **High** — fingerprint fallback | corebridge.cpp |

**Build requirements:**
- Tasks 1-3, 5-6: `just dev-ui` (no vcpkg needed)
- Task 4: `just dev` (modifies core, needs vcpkg)

**Testing strategy:** Manual verification — launch the app, check that `C:\Games\` directory games are detected in the library. Debug via `QT_LOGGING_RULES="makine.*=true"`.
