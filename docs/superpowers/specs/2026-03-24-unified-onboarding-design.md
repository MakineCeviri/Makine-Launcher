# Unified Onboarding Design

> Merge LoginScreen + OnboardingWizard into a single premium onboarding experience.

## Problem

Three separate layers handle first-launch UX, each with different visual design:

1. **LoginScreen** — neon gradient Vice City aesthetic, z:100 overlay
2. **OnboardingWizard** — standard Theme.bgPrimary, separate overlay
3. **Main content** — only visible after both complete

This creates a disjointed experience: neon login → abrupt theme switch → onboarding wizard → app. The user sees three different visual contexts before reaching the actual app.

## Solution

Merge everything into a single **OnboardingWizard** that uses LoginScreen's premium neon design throughout. LoginScreen becomes Step 1 of the wizard instead of a standalone overlay.

## Flow

### First Launch (new user)

```
Step 1: Welcome + Login  →  Step 2: Theme Select  →  Step 3: Scan Games  →  Step 4: Ready!
```

4 steps, dot indicator at bottom, neon gradient background persists across all steps.

### Returning User (token expired)

```
Login Screen ("Tekrar Hoş Geldin")  →  Main App
```

No wizard, no scan, no theme step. Just the login screen with a personalized greeting, then straight to main content.

### Returning User (valid token)

```
Direct to Main App
```

`checkStoredToken()` succeeds → `_authReady = true` → skip everything.

## Visual Design

### Background (ALL steps)

Preserve LoginScreen's existing neon gradient background exactly as-is:

```qml
// Outer container
Rectangle {
    color: "#0d1117"
    clip: true

    // Moving neon gradient — 6x width, ping-pong animation
    Rectangle {
        width: root.width * 6
        height: root.height
        gradient: Gradient {
            orientation: Gradient.Horizontal
            // 13 stops: deep blue → purple → teal → back
            GradientStop { position: 0.000; color: "#0a1628" }
            GradientStop { position: 0.080; color: "#0e1a30" }
            GradientStop { position: 0.160; color: "#150f2a" }
            GradientStop { position: 0.240; color: "#1a0f2e" }
            GradientStop { position: 0.320; color: "#1e0e30" }
            GradientStop { position: 0.400; color: "#170d2a" }
            GradientStop { position: 0.480; color: "#0f2a2e" }
            GradientStop { position: 0.560; color: "#0a2428" }
            GradientStop { position: 0.640; color: "#0d1820" }
            GradientStop { position: 0.720; color: "#0d1117" }
            GradientStop { position: 0.800; color: "#10131c" }
            GradientStop { position: 0.880; color: "#0c1522" }
            GradientStop { position: 1.000; color: "#0a1628" }
        }
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { to: -root.width * 5; duration: 25000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0; duration: 25000; easing.type: Easing.InOutSine }
        }
    }
}
```

This background is the **single visual constant** across all wizard steps. Content crossfades on top of it.

### Window Controls

Single set of window controls (tray, minimize, close) in top-right corner — same as current LoginScreen. These persist across all steps.

### Step Indicator

Bottom-center dot indicator (from current OnboardingWizard):
- Active step: wider pill (24px), accent color
- Completed: success color
- Upcoming: muted color
- 4 dots total for first launch

### Step Transitions

Crossfade + subtle slide (reuse existing `contentStackContainer` animation pattern):
- Outgoing: fade out + slide up (-12px)
- Incoming: fade in + slide up from below (+16px)
- Duration: match `Dimensions.transitionDuration`

## Step Details

### Step 1: Welcome + Login

Single screen combining current LoginScreen content and WelcomeStep purpose.

**Layout (centered ColumnLayout, width: 380):**
1. Logo — `logo_white.png`, 72x72, fade-in + scale animation
2. Brand title — "MAKİNE ÇEVİRİ" split text (bold + light weight, letter-spacing: 6)
3. Tagline — "Kâr Amacı Gütmeyen Türkçe Oyun Yerelleştirme Platformu"
4. Login card — glass card with "Giriş Yap" button (neon pink #D63D8C)
5. Register link — "Hesabınız yok mu? Kayıt olun"
6. Version — "v0.1.0-alpha"

**Returning user mode:**
- Title changes to "Tekrar Hoş Geldin" or similar personalized greeting
- No step dots shown (single screen, not a wizard)
- After successful login → go directly to main app (skip steps 2-4)

**Auth states (all 6 from AuthService):**
- `Checking` → initial state on app launch while `checkStoredToken()` runs. Show logo + spinner, hide login button. If check succeeds → auto-advance. If fails → transition to Unauthenticated.
- `Unauthenticated` → button enabled "Giriş Yap"
- `WaitingForBrowser` → "Tarayıcıdan yanıt bekleniyor..." + spinner
- `Exchanging` → "Doğrulanıyor..." + spinner
- `Refreshing` → treated as authenticated (`isAuthenticated()` returns true for both `Authenticated` and `Refreshing`)
- `Authenticated` → login success, advance or close wizard
- Error → red error box with retry
- Callback timeout (2 min) → show error "Zaman aşımı" with retry button (same as error state)

**On successful auth:**
- If first launch (`!onboardingCompleted`) → advance to Step 2
- If returning user → close wizard, show main app

**Layout note:** Preserve the `anchors.horizontalCenterOffset: -43` from current LoginScreen for visual balance with the neon gradient background.

### Step 2: Theme Select

**Layout (centered, width ~460):**
1. Title — "Tarzını Seç"
2. Subtitle — "Sonradan ayarlardan değiştirebilirsin"
3. Accent color grid — 10 circles in a wrapped flex row:
   - purple (#8B5CF6), blue (#3B82F6), teal (#14B8A6), green (#22C55E), rose (#EC4899)
   - amber (#F59E0B), red (#EF4444), sky (#0EA5E9), indigo (#818CF8), black (#71717A)
   - Selected: white border + glow shadow matching the color
   - Unselected: transparent border
4. Mini preview — small card showing a progress bar + button in the selected accent color
5. "Devam Et" button

**Behavior:**
- Tapping a color immediately calls `SettingsManager.accentPreset = presetId`
- The mini preview updates in real-time
- Default: "purple" (current default)
- No dark/light mode toggle — dark mode is the default, users can change later in settings

### Step 3: Scan Games

Existing `ScanStep.qml` logic preserved, visual reskin to match neon background:
- Card background: `Qt.rgba(1, 1, 1, 0.03)` with pink-tinted border (like LoginScreen's card)
- Button style: match neon pink from Step 1
- Scan stages: Steam → Epic → GOG (same timer logic)
- Results display: same layout

### Step 4: Ready

Existing `ReadyStep.qml` logic preserved, visual reskin:
- Checkmark circle with neon-compatible colors
- "Başlayalım" button with accent gradient
- On finish → `wizardFinished()` signal → same handler as now

## Architecture Changes

### Main.qml

**Before:**
```
LoginScreen (z:100, visible: !_authReady)
OnboardingWizard (Loader, active: _onboardingActive)
mainContent (visible: !_onboardingActive && _authReady)
```

**After:**
```
OnboardingWizard (Loader, active: !_authReady || _onboardingActive)
mainContent (visible: _authReady && !_onboardingActive)
```

The wizard is the single gate. It handles both auth and onboarding internally.

### OnboardingWizard.qml

Complete rewrite:
- Neon gradient background (from LoginScreen)
- Window controls: tray, minimize, close (from LoginScreen). **Tray button disabled during first-launch mode** — system tray is not yet initialized, hiding would make the app unreachable. Only minimize + close active. Returning user mode: all three buttons enabled.
- StackLayout with 4 steps (first launch) or 1 step (returning user)
- Bottom dot indicator (hidden in returning user mode)
- Two modes: `firstLaunch` (4 steps) vs `returningUser` (login only)
- On `Component.onCompleted`: check `AuthService.isAuthenticated` — if already true, skip to Step 2 (ThemeStep). This handles the crash-between-login-and-wizard-completion case.
- Partial wizard progress is intentionally NOT persisted — if the app is killed mid-wizard, it restarts from Step 2 (after auto-auth) on next launch.
- Emits `wizardFinished()` when done — Main.qml hides wizard reactively via `_authReady && !_onboardingActive`
- No separate `loginCompleted()` signal needed — for returning users, `AuthService.isAuthenticated` becoming true makes `_authReady = true`, which reactively hides the wizard since `_onboardingActive` is already false.

### New: screens/onboarding/WelcomeLoginStep.qml

Merges WelcomeStep + LoginScreen:
- LoginScreen's content layout (logo, brand, card)
- Auth flow logic (AuthService signals)
- Emits `loginSuccess()` on auth
- Shows "Hoş Geldin" or "Tekrar Hoş Geldin" based on mode

### Modified: screens/onboarding/ThemeStep.qml (NEW)

New file:
- 10 accent color circles
- Mini preview card
- `SettingsManager.accentPreset` binding
- "Devam Et" button

### Modified: screens/onboarding/ScanStep.qml

Visual reskin only:
- Neon-compatible card styling
- Pink button colors
- Logic unchanged

### Modified: screens/onboarding/ReadyStep.qml

Visual reskin only:
- Neon-compatible colors
- Logic unchanged

### Deleted: LoginScreen.qml

No longer needed as a standalone screen. Its content merges into WelcomeLoginStep.

### Deleted: screens/onboarding/WelcomeStep.qml

Replaced by WelcomeLoginStep.

## State Management

```
SettingsManager.onboardingCompleted  →  controls wizard mode
AuthService.isAuthenticated          →  controls auth gate
AuthService.state                    →  controls login UI states
SettingsManager.accentPreset         →  set during theme step
```

### Decision Matrix (Main.qml)

| onboardingCompleted | isAuthenticated | Result |
|---------------------|-----------------|--------|
| false | false | Wizard: Step 1 (Welcome+Login) |
| false | true | Wizard: Step 2 (Theme) — skip login |
| true | false | Wizard: Login only (returning mode) |
| true | true | Main app directly |

## Skip Behavior

- **Step 1 (Welcome+Login):** No skip — auth is required. The app cannot function without login.
- **Steps 2-4 (Theme, Scan, Ready):** A small "Atla" link at bottom-right allows skipping the remaining wizard entirely. Calls `wizardFinished()` with defaults (purple theme, no scan). Same pattern as current OnboardingWizard's skip on Step 0.
- **Returning user mode:** No skip button — there's only one screen (login).

## Files Changed

| File | Action |
|------|--------|
| `qml/qml/Main.qml` | Simplify auth/onboarding gate |
| `qml/qml/OnboardingWizard.qml` | Complete rewrite with neon bg |
| `qml/qml/screens/LoginScreen.qml` | DELETE |
| `qml/qml/screens/onboarding/WelcomeStep.qml` | DELETE → replaced by WelcomeLoginStep |
| `qml/qml/screens/onboarding/WelcomeLoginStep.qml` | NEW — merged welcome + login |
| `qml/qml/screens/onboarding/ThemeStep.qml` | NEW — accent color picker |
| `qml/qml/screens/onboarding/ScanStep.qml` | Visual reskin |
| `qml/qml/screens/onboarding/ReadyStep.qml` | Visual reskin |
| `qml/CMakeLists.txt` | Update QML file list |
