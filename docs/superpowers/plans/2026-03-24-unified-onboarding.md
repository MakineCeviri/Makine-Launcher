# Unified Onboarding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Merge LoginScreen + OnboardingWizard into a single premium 4-step onboarding wizard with neon gradient background, accent color picker, and returning user support.

**Architecture:** OnboardingWizard becomes the single auth+onboarding gate in Main.qml. It embeds LoginScreen's neon gradient as a persistent background, with 4 StackLayout steps: WelcomeLoginStep (merged welcome+login), ThemeStep (new accent picker), ScanStep (reskinned), ReadyStep (reskinned). Returning users see only the login step without dots.

**Tech Stack:** Qt 6.10.1, QML, C++ (SettingsManager, AuthService), MinGW 13.1

**Spec:** `docs/superpowers/specs/2026-03-24-unified-onboarding-design.md`

---

## Performance Considerations

These apply throughout ALL tasks:

- **No dynamic Component.createObject()** — all steps are statically declared in StackLayout. Steps not visible have `visible: false` which prevents rendering.
- **Images use `asynchronous: true`** and explicit `sourceSize` to avoid main-thread stalls.
- **Animations use `enabled: false`** pattern when not visible to avoid idle GPU work.
- **StackLayout** only lays out the `currentIndex` child — others are effectively dormant. No Loader overhead needed per step.
- **ThemeStep color circles:** Use a Repeater with a simple model, not 10 individual Rectangles.
- **ScanStep timers:** Stop all timers on step exit (currentStep changes away). **Critical:** Do NOT start timers in `Component.onCompleted` — StackLayout instantiates all children at creation time. Guard timer start with `StackLayout.isCurrentItem` becoming true, or watch `currentStep === 2`.
- **Background gradient animation:** Single instance, always runs (same as current LoginScreen). Not duplicated per step.
- **Step initialization:** All 4 steps' `Component.onCompleted` fires simultaneously at wizard creation. Steps must NOT perform heavy work (network, scans, timers) in onCompleted — defer to when the step becomes active via `StackLayout.isCurrentItem`.

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `qml/qml/OnboardingWizard.qml` | **Rewrite** | Neon bg, window controls, StackLayout, dot indicator, mode switching |
| `qml/qml/screens/onboarding/WelcomeLoginStep.qml` | **New** | Merged welcome+login (from LoginScreen.qml content) |
| `qml/qml/screens/onboarding/ThemeStep.qml` | **New** | Accent color grid + mini preview |
| `qml/qml/screens/onboarding/ScanStep.qml` | **Modify** | Visual reskin to neon style, logic unchanged |
| `qml/qml/screens/onboarding/ReadyStep.qml` | **Modify** | Visual reskin to neon style, logic unchanged |
| `qml/qml/Main.qml` | **Modify** | Simplify gate: remove standalone LoginScreen, unify wizard condition |
| `qml/qml/screens/LoginScreen.qml` | **Delete** | Content moved to WelcomeLoginStep |
| `qml/qml/screens/onboarding/WelcomeStep.qml` | **Delete** | Replaced by WelcomeLoginStep |
| `qml/CMakeLists.txt` | **Modify** | Update QML_FILES list |

---

## Task 1: Create WelcomeLoginStep (merged welcome + login)

**Files:**
- Create: `qml/qml/screens/onboarding/WelcomeLoginStep.qml`
- Reference: `qml/qml/screens/LoginScreen.qml` (source of content to migrate)
- Reference: `qml/qml/screens/onboarding/WelcomeStep.qml` (being replaced)

This is the core merge — LoginScreen's premium content becomes a wizard step.

- [ ] **Step 1: Read current LoginScreen.qml and WelcomeStep.qml completely**

Understand all auth states, signals, layout, animations.

- [ ] **Step 2: Create WelcomeLoginStep.qml**

```qml
// Key structure — NOT the full file, see LoginScreen.qml for complete content
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0

/**
 * WelcomeLoginStep.qml — Merged welcome + login (Step 1 of onboarding)
 * Preserves LoginScreen's premium neon design as a wizard step.
 */
Item {
    id: root

    // Mode: first launch shows "Hoş Geldin", returning shows "Tekrar Hoş Geldin"
    property bool returningUser: false

    signal loginSuccess()

    // Content — exact copy of LoginScreen's ColumnLayout content
    // with these changes:
    // 1. Remove background (wizard provides it)
    // 2. Remove window controls (wizard provides them)
    // 3. Remove MouseArea blocker (wizard handles it)
    // 4. Keep anchors.horizontalCenterOffset: -43
    // 5. Add Checking state: show spinner, hide login button
    // 6. Tagline text: returningUser ? "" : original tagline
    // 7. Card header: returningUser
    //      ? qsTr("Tekrar hoş geldin")
    //      : qsTr("Devam etmek için giriş yapın")

    // Auth state connections — same as LoginScreen
    Connections {
        target: AuthService
        function onLoginError(message) { errorText.text = message }
        function onStateChanged() {
            if (AuthService.isAuthenticated)
                root.loginSuccess()
        }
    }
}
```

Copy ALL visual content from LoginScreen.qml lines 137-401 (the ColumnLayout and its children) AND the Connections block (lines 403-410), removing only:
- The outer background Rectangle (lines 101-134)
- The window controls Row (lines 35-99)
- The top MouseArea blocker (lines 14-18)
- The window drag Item (lines 21-32)

Add:
- `property bool returningUser: false`
- `signal loginSuccess()`
- Checking state handling — concrete structure:

```qml
// Checking state: show spinner while checkStoredToken() runs
readonly property bool isChecking: typeof AuthService !== "undefined"
                                   && AuthService.state === AuthServiceType.Checking

// Inside the ColumnLayout, wrap the login card in:
// The card + register link visibility:
//   visible: !root.isChecking
//   opacity controlled by fade-in animation

// Centered spinner for Checking state:
BusyIndicator {
    Layout.alignment: Qt.AlignHCenter
    Layout.preferredWidth: 32
    Layout.preferredHeight: 32
    running: root.isChecking
    visible: root.isChecking
    palette.dark: "#22D3EE"
}
```

When `Checking` → show only logo + brand + spinner (card hidden).
When state transitions to `Unauthenticated` → card fades in.
When state transitions to `Authenticated` → emit `loginSuccess()`.

- Conditional text based on `returningUser` property:
  - Card header: `returningUser ? qsTr("Tekrar hoş geldin") : qsTr("Devam etmek için giriş yapın")`
  - Tagline: `visible: !returningUser` (hide on returning user)
- Include Connections block for `onLoginError` and `onStateChanged` (critical for auth error handling)

- [ ] **Step 3: Verify file structure**

Run: `ls qml/qml/screens/onboarding/`
Expected: WelcomeLoginStep.qml exists alongside ScanStep.qml, ReadyStep.qml, WelcomeStep.qml (still present, will be deleted later)

- [ ] **Step 4: Commit**

```bash
git add qml/qml/screens/onboarding/WelcomeLoginStep.qml
git commit -m "feat(ui): create WelcomeLoginStep merging welcome + login"
```

---

## Task 2: Create ThemeStep (accent color picker)

**Files:**
- Create: `qml/qml/screens/onboarding/ThemeStep.qml`
- Reference: `qml/qml/theme/Theme.qml` (accent preset colors)

- [ ] **Step 1: Read Theme.qml accent preset definitions**

Note the 10 presets and their accentBase (index [2]) colors:
```
purple: #8B5CF6, blue: #3B82F6, teal: #14B8A6, green: #22C55E, rose: #EC4899
amber: #F59E0B, red: #EF4444, sky: #0EA5E9, indigo: #818CF8, black: #71717A
```

- [ ] **Step 2: Create ThemeStep.qml**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ThemeStep.qml — Accent color picker (Step 2 of onboarding)
 */
Item {
    id: root

    signal nextStep()

    // Preset model — id + display color (accentBase from Theme.qml)
    readonly property var presets: [
        { presetId: "purple", color: "#8B5CF6" },
        { presetId: "blue",   color: "#3B82F6" },
        { presetId: "teal",   color: "#14B8A6" },
        { presetId: "green",  color: "#22C55E" },
        { presetId: "rose",   color: "#EC4899" },
        { presetId: "amber",  color: "#F59E0B" },
        { presetId: "red",    color: "#EF4444" },
        { presetId: "sky",    color: "#0EA5E9" },
        { presetId: "indigo", color: "#818CF8" },
        { presetId: "black",  color: "#71717A" }
    ]

    property string selectedPreset: SettingsManager.accentPreset

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 0
        width: Math.min(parent.width, 460)

        // Title
        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Tarzını Seç")
            font.pixelSize: 24
            font.weight: Font.Bold
            font.letterSpacing: -0.3
            color: "#FFFFFF"
        }

        Item { Layout.preferredHeight: 8 }

        // Subtitle
        Text {
            textFormat: Text.PlainText
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Sonradan ayarlardan değiştirebilirsin")
            font.pixelSize: 13
            color: Qt.rgba(1, 1, 1, 0.4)
        }

        Item { Layout.preferredHeight: 32 }

        // Color grid — Flow wraps 5 per row
        Flow {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 5 * 46  // 5 circles per row
            spacing: 10

            Repeater {
                model: root.presets

                Rectangle {
                    required property var modelData
                    required property int index

                    width: 36; height: 36; radius: 18
                    color: modelData.color
                    border.width: root.selectedPreset === modelData.presetId ? 2 : 0
                    border.color: "#FFFFFF"

                    // Glow for selected
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        radius: parent.radius + 4
                        color: "transparent"
                        border.width: root.selectedPreset === modelData.presetId ? 1 : 0
                        border.color: Qt.rgba(
                            parent.color.r, parent.color.g, parent.color.b, 0.4)
                        visible: root.selectedPreset === modelData.presetId
                    }

                    // Hover effect
                    scale: colorMa.containsMouse ? 1.1 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }

                    MouseArea {
                        id: colorMa
                        anchors.fill: parent
                        anchors.margins: -4  // larger hit area
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedPreset = modelData.presetId
                            SettingsManager.accentPreset = modelData.presetId
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 24 }

        // Mini preview card
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 240
            Layout.preferredHeight: 64
            radius: 10
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.06)

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 8

                // Progress bar preview
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.06)

                    Rectangle {
                        width: parent.width * 0.6
                        height: parent.height
                        radius: parent.radius
                        color: Theme.accentBase
                    }
                }

                // Button preview
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    radius: 6
                    color: Qt.rgba(Theme.accentBase.r, Theme.accentBase.g, Theme.accentBase.b, 0.15)
                    border.width: 1
                    border.color: Qt.rgba(Theme.accentBase.r, Theme.accentBase.g, Theme.accentBase.b, 0.3)

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: qsTr("Türkçe Yap")
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: Theme.accentBase
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 32 }

        // Continue button
        Button {
            id: continueBtn
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 44

            contentItem: Text {
                textFormat: Text.PlainText
                text: qsTr("Devam Et")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 8
                color: continueBtn.hovered
                    ? Qt.darker(Theme.accentBase, 1.15)
                    : Theme.accentBase
            }

            scale: pressed ? 0.97 : 1.0
            Behavior on scale { NumberAnimation { duration: 80 } }

            onClicked: root.nextStep()
        }
    }
}
```

- [ ] **Step 3: Verify file exists**

Run: `ls qml/qml/screens/onboarding/ThemeStep.qml`

- [ ] **Step 4: Commit**

```bash
git add qml/qml/screens/onboarding/ThemeStep.qml
git commit -m "feat(ui): create ThemeStep accent color picker for onboarding"
```

---

## Task 3: Reskin ScanStep and ReadyStep for neon background

**Files:**
- Modify: `qml/qml/screens/onboarding/ScanStep.qml`
- Modify: `qml/qml/screens/onboarding/ReadyStep.qml`

These steps keep their logic but get visual updates to match the neon dark background (white text on dark, neon-tinted cards).

- [ ] **Step 1: Read current ScanStep.qml and ReadyStep.qml**

Note all Theme.xxx color references that need changing.

- [ ] **Step 2: Reskin ScanStep.qml**

Changes needed (colors only — do NOT change logic, timers, signals, or GameService connections):

| Current | Change To | Reason |
|---------|-----------|--------|
| `Theme.textPrimary` | `"#FFFFFF"` | White text on neon bg |
| `Theme.textSecondary` | `Qt.rgba(1, 1, 1, 0.5)` | Light secondary on neon bg |
| `Theme.surface50` | `Qt.rgba(1, 1, 1, 0.03)` | Glass card on neon bg |
| `Theme.glassBorder` | `Qt.rgba(1, 0.42, 0.62, 0.12)` | Pink-tinted border (from LoginScreen) |
| `Theme.primary` (buttons) | `"#D63D8C"` | Neon pink button |
| `Theme.primaryHover` (buttons) | `"#E04898"` | Neon pink hover |
| `Theme.textOnColor` | `"#FFFFFF"` | Same |
| `Theme.success` | Keep as-is | Green is universal |
| `Theme.surfaceActive50` | `Qt.rgba(1, 1, 1, 0.06)` | Muted on neon bg |
| `Theme.surface60` (back btn hover) | `Qt.rgba(1, 1, 1, 0.06)` | Glass hover |

- [ ] **Step 3: Reskin ReadyStep.qml**

Same color mapping as ScanStep. Additionally:
- Checkmark circle: `color: Qt.rgba(0.06, 0.73, 0.51, 0.1)` (success green with low alpha), border unchanged
- Finish button: use accent gradient (same as current — `Theme.primary` to `Theme.accent`)

- [ ] **Step 4: Verify both files saved correctly**

Run: `head -5 qml/qml/screens/onboarding/ScanStep.qml qml/qml/screens/onboarding/ReadyStep.qml`

- [ ] **Step 5: Commit**

```bash
git add qml/qml/screens/onboarding/ScanStep.qml qml/qml/screens/onboarding/ReadyStep.qml
git commit -m "feat(ui): reskin ScanStep and ReadyStep for neon onboarding background"
```

---

## Task 4: Rewrite OnboardingWizard with neon background and new flow

**Files:**
- Rewrite: `qml/qml/OnboardingWizard.qml`

This is the container — neon gradient bg, window controls, StackLayout with 4 steps, dot indicator.

- [ ] **Step 1: Read current OnboardingWizard.qml and LoginScreen.qml background/controls sections**

Note:
- LoginScreen background: lines 101-134 (gradient + animation)
- LoginScreen window controls: lines 35-99 (tray, minimize, close)
- OnboardingWizard step dots: lines 123-141

- [ ] **Step 2: Rewrite OnboardingWizard.qml**

Structure:
```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
import "screens/onboarding"
pragma ComponentBehavior: Bound

/**
 * OnboardingWizard.qml — Unified auth + onboarding experience
 *
 * Modes:
 *   firstLaunch (!onboardingCompleted): 4 steps with dots
 *   returningUser (onboardingCompleted): login only, no dots
 */
Rectangle {
    id: root
    color: "#0d1117"
    clip: true

    signal wizardFinished()

    // Mode detection
    readonly property bool returningUser: typeof SettingsManager !== "undefined"
                                          && SettingsManager.onboardingCompleted

    property int currentStep: 0
    readonly property int totalSteps: 4
    readonly property bool isLastStep: currentStep === totalSteps - 1

    Component.onCompleted: {
        // If already authenticated (crash recovery), skip to step 1 (ThemeStep)
        if (!returningUser && typeof AuthService !== "undefined"
                && AuthService.isAuthenticated) {
            currentStep = 1
        }
    }

    // === NEON GRADIENT BACKGROUND (from LoginScreen — DO NOT MODIFY) ===
    // ... exact copy of LoginScreen lines 107-133 ...

    // === WINDOW CONTROLS (tray, minimize, close) ===
    // Copy from LoginScreen lines 35-99
    // Change: tray button visible only if returningUser
    //   visible: root.returningUser

    // === WINDOW DRAG ===
    // Copy from LoginScreen lines 21-32

    // === BLOCK MOUSE EVENTS ===
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    // === STEP TRANSITION ANIMATIONS ===
    // Triggered by currentStep changes — crossfade + subtle slide
    property int _previousStep: 0
    onCurrentStepChanged: {
        var outgoing = stepStack.children[_previousStep]
        var incoming = stepStack.children[currentStep]
        if (outgoing && incoming && _previousStep !== currentStep) {
            // Outgoing: fade out + slide up
            outgoingAnim.target = outgoing
            outgoingAnim.start()
            // Incoming: fade in + slide from below
            incoming.opacity = 0
            incoming.y = 16
            incomingAnim.target = incoming
            incomingAnim.start()
        }
        _previousStep = currentStep
    }

    ParallelAnimation {
        id: outgoingAnim
        property var target: null
        NumberAnimation { target: outgoingAnim.target; property: "opacity"; to: 0; duration: 180; easing.type: Easing.OutCubic }
        NumberAnimation { target: outgoingAnim.target; property: "y"; to: -12; duration: 180; easing.type: Easing.InCubic }
    }
    ParallelAnimation {
        id: incomingAnim
        property var target: null
        NumberAnimation { target: incomingAnim.target; property: "opacity"; to: 1; duration: 250; easing.type: Easing.OutCubic }
        NumberAnimation { target: incomingAnim.target; property: "y"; to: 0; duration: 250; easing.type: Easing.OutCubic }
    }

    // === STEP CONTENT ===
    StackLayout {
        id: stepStack
        anchors.fill: parent
        anchors.topMargin: 40
        anchors.bottomMargin: returningUser ? 0 : 56  // no bottom space if no dots
        currentIndex: root.currentStep

        WelcomeLoginStep {
            returningUser: root.returningUser
            onLoginSuccess: {
                if (root.returningUser) {
                    // Returning user — reactive hide via _authReady becoming true
                    // Do NOT call wizardFinished() — it would trigger scanAllLibraries
                } else {
                    root.currentStep = 1
                }
            }
        }

        ThemeStep {
            onNextStep: root.currentStep = 2
        }

        ScanStep {
            onNextStep: root.currentStep = 3
            onPreviousStep: root.currentStep = 1
        }

        ReadyStep {
            onFinished: root.wizardFinished()
        }
    }

    // === BOTTOM: Step dots + Skip (first launch only) ===
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 60
        anchors.rightMargin: 60
        height: 32
        visible: !root.returningUser

        Item { Layout.fillWidth: true }

        // Step dots
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            Repeater {
                model: root.totalSteps
                Rectangle {
                    required property int index
                    width: index === root.currentStep ? 24 : 8
                    height: 6
                    radius: 3
                    color: index === root.currentStep
                        ? Theme.accentBase
                        : index < root.currentStep
                            ? Theme.success60
                            : Qt.rgba(1, 1, 1, 0.15)

                    Behavior on width {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Skip link (steps 1-3 only, not login step)
        Text {
            textFormat: Text.PlainText
            visible: root.currentStep > 0 && !root.isLastStep
            text: qsTr("Atla")
            font.pixelSize: 13
            color: skipMa.containsMouse
                ? Qt.rgba(1, 1, 1, 0.6)
                : Qt.rgba(1, 1, 1, 0.3)

            MouseArea {
                id: skipMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.wizardFinished()
            }
        }
    }
}
```

Key points:
- Background gradient is the EXACT copy from LoginScreen (13 gradient stops + 25s animation)
- Window controls: tray button `visible: root.returningUser` (disabled on first launch)
- StackLayout replaces previous 3-step layout with 4 steps
- Dot indicator: 4 dots, accent color for current, success for completed, muted for upcoming
- Skip: visible on steps 1-3 (after login), calls wizardFinished() with defaults
- `Component.onCompleted`: crash recovery — if already authed, jump to step 1

- [ ] **Step 3: Commit**

```bash
git add qml/qml/OnboardingWizard.qml
git commit -m "feat(ui): rewrite OnboardingWizard with neon bg and unified auth flow"
```

---

## Task 5: Update Main.qml gate logic

**Files:**
- Modify: `qml/qml/Main.qml`

Remove standalone LoginScreen, simplify to single wizard gate.

- [ ] **Step 1: Read Main.qml lines 50-70 and 274-280**

These are the auth/onboarding properties and LoginScreen usage.

- [ ] **Step 2: Remove LoginScreen from Main.qml**

Delete lines 274-279 (the standalone LoginScreen block):
```qml
// DELETE THIS:
LoginScreen {
    anchors.fill: parent
    visible: !window._authReady
    z: 100
}
```

- [ ] **Step 3: Update OnboardingWizard Loader condition**

Change the onboarding loader (around line 812-830) from:
```qml
Loader {
    id: onboardingLoader
    anchors.fill: parent
    active: window._onboardingActive
    sourceComponent: Component {
        OnboardingWizard {
            z: Dimensions.zOverlay
            onWizardFinished: {
                window._onboardingActive = false
                // ...
            }
        }
    }
}
```

To:
```qml
Loader {
    id: onboardingLoader
    anchors.fill: parent
    active: !window._authReady || window._onboardingActive
    z: Dimensions.zOverlay
    sourceComponent: Component {
        OnboardingWizard {
            onWizardFinished: {
                // Only scan + persist for first-launch (not returning users)
                if (window._onboardingActive) {
                    if (typeof GameService !== "undefined" && GameService.gameCount === 0) {
                        GameService.scanAllLibraries()
                    }
                    if (typeof SettingsManager !== "undefined") {
                        SettingsManager.onboardingCompleted = true
                    }
                }
                window._onboardingActive = false
            }
        }
    }
}
```

Key change: `active: !window._authReady || window._onboardingActive` — wizard shows if not authed OR onboarding pending.

- [ ] **Step 4: Update mainContent visibility**

Current (line 285):
```qml
visible: !window._onboardingActive && window._authReady
```

This stays the same — already correct for the new flow.

- [ ] **Step 5: Update window background color**

Current (line 26):
```qml
color: window._authReady ? Theme.bgPrimary : "#0d1117"
```

Change to:
```qml
color: (window._authReady && !window._onboardingActive) ? Theme.bgPrimary : "#0d1117"
```

This keeps the dark base color while the wizard is showing (wizard has its own neon bg on top).

- [ ] **Step 6: Commit**

```bash
git add qml/qml/Main.qml
git commit -m "feat(ui): unify auth+onboarding gate in Main.qml"
```

---

## Task 6: Update CMakeLists.txt and delete old files

**Files:**
- Modify: `qml/CMakeLists.txt`
- Delete: `qml/qml/screens/LoginScreen.qml`
- Delete: `qml/qml/screens/onboarding/WelcomeStep.qml`

- [ ] **Step 1: Read qml/CMakeLists.txt QML_FILES section**

Find the exact lines for LoginScreen.qml, WelcomeStep.qml registration and where to add new files.

- [ ] **Step 2: Update QML_FILES in CMakeLists.txt**

Remove:
```cmake
qml/screens/LoginScreen.qml
qml/screens/onboarding/WelcomeStep.qml
```

Add (in the onboarding section):
```cmake
qml/screens/onboarding/WelcomeLoginStep.qml
qml/screens/onboarding/ThemeStep.qml
```

- [ ] **Step 3: Delete old files**

```bash
git rm qml/qml/screens/LoginScreen.qml
git rm qml/qml/screens/onboarding/WelcomeStep.qml
```

- [ ] **Step 4: Build to verify**

Run: `just dev-ui`
Expected: Clean build with no missing QML file errors.

- [ ] **Step 5: Commit**

```bash
git add qml/CMakeLists.txt
git commit -m "build(ui): update QML file list for unified onboarding"
```

---

## Task 7: Smoke test the full flow

- [ ] **Step 1: Run the app**

```bash
just run
```

- [ ] **Step 2: Test first-launch flow**

Reset onboarding state for testing:
1. Close app
2. Delete `general/onboardingCompleted` from QSettings (or use fresh config)
3. Relaunch

Verify:
- Step 1: Neon gradient bg, logo, brand, login button visible
- Step 1: "Hoş Geldin" text shown (not "Tekrar")
- Step 1: Auth flow works (login → browser → callback → success)
- Step 2: Theme circles shown, default purple selected
- Step 2: Tapping a color updates preview instantly
- Step 3: Scan works (or shows empty result)
- Step 4: Ready screen, "Başlayalım" button → main app
- Dot indicator: shows 4 dots, progresses correctly
- Skip: visible on steps 1-3, works

- [ ] **Step 3: Test returning user flow**

1. Close and reopen app (onboarding completed, token expired/cleared)
2. Verify: Login screen shows with "Tekrar Hoş Geldin"
3. Verify: No dot indicator
4. Verify: Login → goes directly to main app (no theme/scan steps)

- [ ] **Step 4: Test auto-auth flow**

1. Close and reopen app (onboarding completed, token valid)
2. Verify: Goes directly to main app, no wizard shown

- [ ] **Step 5: Final commit if any fixes needed**

```bash
git add -A
git commit -m "fix(ui): polish unified onboarding flow"
```
