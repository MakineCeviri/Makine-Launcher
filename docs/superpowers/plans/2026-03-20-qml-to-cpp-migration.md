# QML → C++ Business Logic Migration Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move ~500 lines of business logic from QML JavaScript to native C++ for maximum performance, testability, and minimal resource consumption.

**Architecture:** Create 2 new C++ services (InstallFlowService, TranslationStateManager), extend 2 existing services (GameService, PluginManager), and slim QML controllers to pure signal/property forwarding.

**Tech Stack:** Qt6 C++23, QML, MinGW GCC 13.1

**Constraints:**
- `<regex>` broken on MinGW — use string operations
- Explicit `#include <set>/<map>` required
- Build: `cmake --build --preset dev`
- NO UI/visual changes

---

## File Structure

### New Files
| File | Responsibility |
|------|---------------|
| `qml/src/services/installflowservice.h` | Install/update state machine, download gate, external redirect |
| `qml/src/services/installflowservice.cpp` | Implementation |
| `qml/src/services/translationstatemanager.h` | 7-state button resolver, pre-computed state data |
| `qml/src/services/translationstatemanager.cpp` | Implementation |

### Modified Files
| File | Change |
|------|--------|
| `qml/src/services/gameservice.h/.cpp` | Add resolveGameData(), autoInstallCheck(), formatProgress() |
| `qml/src/services/pluginmanager.h/.cpp` | Add settingsCategories(), pluginsWithSettings() |
| `qml/src/main.cpp` | Register new services with QML engine |
| `qml/qml/controllers/InstallFlowController.qml` | Slim to signal forwarding → InstallFlowService |
| `qml/qml/controllers/GameDataResolver.qml` | Replace with GameService.resolveGameData() |
| `qml/qml/controllers/GameDetailViewModel.qml` | Remove JS logic, bind to C++ computed properties |
| `qml/qml/GameDetailScreen.qml` | Remove auto-install JS, use GameService signals |
| `qml/qml/screens/detail/TranslationActionButton.qml` | Bind to TranslationStateManager |
| `qml/qml/SettingsScreen.qml` | Use PluginManager.settingsCategories() |
| `qml/CMakeLists.txt` | Add new .cpp/.h files |

---

### Task 1: Create TranslationStateManager (C++)

**Files:**
- Create: `qml/src/services/translationstatemanager.h`
- Create: `qml/src/services/translationstatemanager.cpp`
- Modify: `qml/src/main.cpp` (register with QML)
- Modify: `qml/CMakeLists.txt` (add to sources)
- Modify: `qml/qml/screens/detail/TranslationActionButton.qml` (bind to C++)

The 7-state resolver in TranslationActionButton.qml (lines 31-45) runs on every property change. Move to C++ with pre-computed state enum.

- [ ] **Step 1: Create translationstatemanager.h**

```cpp
#pragma once
#include <QObject>
#include <QString>

namespace makine {

class TranslationStateManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString label READ label NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleText READ accessibleText NOTIFY stateChanged)

public:
    enum class State { Download, Installing, Completed, Installed, Update, Broken, External };
    Q_ENUM(State)

    explicit TranslationStateManager(QObject* parent = nullptr);

    Q_INVOKABLE void evaluate(bool hasUpdate, bool packageInstalled,
                               bool isInstalling, bool installCompleted,
                               const QString& impactLevel,
                               const QString& externalUrl);

    QString state() const;
    QString label() const;
    QString accessibleText() const;

signals:
    void stateChanged();

private:
    State m_state{State::Download};
};

} // namespace makine
```

- [ ] **Step 2: Create translationstatemanager.cpp**

Implement evaluate() with the same conditional cascade from QML, but as a single C++ function that emits stateChanged only when state actually changes.

- [ ] **Step 3: Register in main.cpp**

```cpp
qmlRegisterSingletonInstance("MakineLauncher", 1, 0, "TranslationState", translationStateManager);
```

- [ ] **Step 4: Add to CMakeLists.txt**

- [ ] **Step 5: Update TranslationActionButton.qml**

Replace the `_state` computed property with binding to `TranslationState.state`. Remove JS conditional cascade.

- [ ] **Step 6: Build and verify**

```bash
cmake --build --preset dev
```

- [ ] **Step 7: Commit**

```bash
git commit -m "refactor(core): move translation state resolver from QML to C++"
```

---

### Task 2: Merge GameDataResolver into GameService (C++)

**Files:**
- Modify: `qml/src/services/gameservice.h` (add resolveGameData)
- Modify: `qml/src/services/gameservice.cpp` (implement)
- Modify: `qml/qml/controllers/GameDataResolver.qml` (delegate to C++)

GameDataResolver.qml has 51 lines of pure business logic (regex, catalog matching, 8 conditionals). Move entirely to GameService.

- [ ] **Step 1: Add Q_INVOKABLE to gameservice.h**

```cpp
Q_INVOKABLE QVariantMap resolveGameData(const QString& gameId,
                                         const QString& gameName,
                                         const QString& installPath,
                                         const QString& engine,
                                         bool forceAutoInstall) const;
```

- [ ] **Step 2: Implement in gameservice.cpp**

Port the resolve() logic from GameDataResolver.qml: manual game check, steamAppId resolution, catalog lookup, externalUrl/isApex extraction, image resolve.

- [ ] **Step 3: Slim GameDataResolver.qml**

```qml
QtObject {
    function resolve(gameId, gameName, installPath, engine, forceAutoInstall) {
        return GameService.resolveGameData(gameId, gameName, installPath, engine, forceAutoInstall)
    }
}
```

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

```bash
git commit -m "refactor(core): move GameDataResolver logic to GameService C++"
```

---

### Task 3: Create InstallFlowService (C++)

**Files:**
- Create: `qml/src/services/installflowservice.h`
- Create: `qml/src/services/installflowservice.cpp`
- Modify: `qml/src/main.cpp`
- Modify: `qml/CMakeLists.txt`
- Modify: `qml/qml/controllers/InstallFlowController.qml`

The install state machine (325 lines JS) is the largest business logic block in QML. Move to C++ with proper state management.

- [ ] **Step 1: Create installflowservice.h**

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace makine {

class GameService;
class TranslationDownloader;
class ManifestSyncService;
class CoreBridge;

class InstallFlowService : public QObject {
    Q_OBJECT

public:
    explicit InstallFlowService(GameService* gameService,
                                 TranslationDownloader* downloader,
                                 ManifestSyncService* manifestSync,
                                 CoreBridge* coreBridge,
                                 QObject* parent = nullptr);

    Q_INVOKABLE void startInstall(const QString& gameId, const QString& gameName,
                                   const QString& externalUrl);
    Q_INVOKABLE void startUpdate(const QString& gameId, const QString& gameName);
    Q_INVOKABLE void onAntiCheatContinue();
    Q_INVOKABLE void onOptionsConfirmed(const QStringList& selectedIds);
    Q_INVOKABLE void onDownloadReady(const QString& appId);
    Q_INVOKABLE void onDownloadError(const QString& appId, const QString& error);
    Q_INVOKABLE bool isExternalSource(const QString& gameId) const;
    Q_INVOKABLE QString externalUrl(const QString& gameId) const;

signals:
    void showAntiCheatWarning(const QVariantMap& data);
    void showInstallOptions(const QVariantMap& data);
    void externalRedirect(const QString& url);
    void installStarted(const QString& gameId);
    void installError(const QString& gameId, const QString& error);

private:
    void doInstall(const QString& gameId, const QString& variant, const QStringList& options);
    void doUpdate(const QString& gameId, const QString& variant, const QStringList& options);
    bool validateCdnUrl(const QString& url) const;

    GameService* m_gameService;
    TranslationDownloader* m_downloader;
    ManifestSyncService* m_manifestSync;
    CoreBridge* m_coreBridge;

    QString m_pendingGameId;
    QString m_pendingGameName;
    QString m_pendingVariant;
    bool m_pendingUpdate{false};
};

} // namespace makine
```

- [ ] **Step 2: Implement installflowservice.cpp**

Port all logic from InstallFlowController.qml:
- startInstall: external check → anti-cheat → detail loading → options → download
- doInstall: CDN URL validation, download gate
- Download callbacks: state cleanup, package install trigger

- [ ] **Step 3: Register in main.cpp and CMakeLists.txt**

- [ ] **Step 4: Slim InstallFlowController.qml**

Reduce to signal routing — all JS logic delegates to InstallFlowService C++ methods.

- [ ] **Step 5: Build and verify**

- [ ] **Step 6: Commit**

```bash
git commit -m "refactor(core): move install flow state machine from QML to C++"
```

---

### Task 4: Move GameDetailViewModel computed properties to C++

**Files:**
- Modify: `qml/src/services/gameservice.h/.cpp`
- Modify: `qml/qml/controllers/GameDetailViewModel.qml`

URL construction, .join() calls, and data assembly belong in C++.

- [ ] **Step 1: Add helper methods to GameService**

```cpp
Q_INVOKABLE QString steamHeroUrl(const QString& steamAppId) const;
Q_INVOKABLE QString steamCoverUrl(const QString& steamAppId) const;
Q_INVOKABLE QString steamLogoUrl(const QString& steamAppId) const;
Q_INVOKABLE QString formatProgress(qint64 received, qint64 total) const;
Q_INVOKABLE bool shouldAutoInstall(const QString& gameId) const;
```

- [ ] **Step 2: Implement — URL construction + progress formatting in C++**

- [ ] **Step 3: Update GameDetailViewModel.qml bindings**

Replace JS expressions with C++ method calls.

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

```bash
git commit -m "refactor(core): move URL construction and formatting to GameService C++"
```

---

### Task 5: Extend PluginManager for Settings categories

**Files:**
- Modify: `qml/src/services/pluginmanager.h/.cpp`
- Modify: `qml/qml/SettingsScreen.qml`

Plugin discovery loop and category filtering (40 lines JS) → C++ PluginManager.

- [ ] **Step 1: Add to PluginManager**

```cpp
Q_INVOKABLE QVariantList settingsCategories() const;
Q_INVOKABLE QVariantList pluginsWithSettings() const;
```

- [ ] **Step 2: Implement — filter loaded plugins with settings, build category list**

- [ ] **Step 3: Slim SettingsScreen.qml — remove JS loops, bind to PluginManager**

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

```bash
git commit -m "refactor(core): move plugin settings discovery to PluginManager C++"
```

---

## Summary

| Task | QML Lines Removed | C++ Lines Added | Impact |
|------|-------------------|-----------------|--------|
| 1. TranslationStateManager | ~60 | ~80 | 7-state eval: JS → native |
| 2. GameDataResolver → GameService | ~40 | ~50 | Regex + catalog match: native |
| 3. InstallFlowService | ~250 | ~200 | State machine: native + testable |
| 4. ViewModel computed props | ~30 | ~40 | URL concat + formatting: native |
| 5. PluginManager settings | ~40 | ~30 | Loop + filter: native |
| **Total** | **~420** | **~400** | **QML = pure UI, C++ = all logic** |
