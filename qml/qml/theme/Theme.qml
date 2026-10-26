// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

pragma Singleton
import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Theme.qml — colours of the launcher.
 *
 * Makine's own night: a near-black page lit by two soft orbs of the accent
 * (AppBackground), content on translucent containers with a hairline and a
 * lit top edge (Card), warm off-white labels in four strengths, white-alpha
 * fills for controls. The structure stays Apple's (docs/design.md says which
 * token goes where); the tone is 0.1.5's, which users preferred. Older names
 * (textMuted, primary12, …) stay as aliases so every screen reads one palette.
 */
QtObject {
    id: theme

    // =========================================================================
    // MODE
    // =========================================================================

    property bool darkMode: typeof SettingsManager !== "undefined" ? SettingsManager.isDarkMode : true

    function _m(dark, light) { return darkMode ? dark : light }

    // =========================================================================
    // BACKGROUNDS
    // =========================================================================

    // Dark surfaces take a little of the accent, so the whole window carries
    // the colour the user picked, not only its buttons
    function _night(r, g, b, k, alpha) {
        return Qt.rgba(r / 255 * (1 - k) + accentBase.r * k, g / 255 * (1 - k) + accentBase.g * k,
                       b / 255 * (1 - k) + accentBase.b * k, alpha)
    }

    /// The window's frame: the sidebar stands on it, the page panel sits in it
    readonly property color chrome: _m(_night(3, 3, 5, 0.03, 1), "#E7E7EC")
    /// Window / page background (AppBackground lights it)
    readonly property color bgPrimary: _m(_night(7, 7, 10, 0.035, 1), "#F4F4F6")
    /// A step up from the page, opaque
    readonly property color bgSecondary: _m("#0E0E13", "#FFFFFF")

    /// Containers: sidebar, sections, lists, cards — see Card.qml. Translucent
    /// in dark so the page's glow shows faintly through.
    readonly property color container: _m(_night(13, 13, 18, 0.05, 0.82), "#FFFFFF")
    /// Its hairline, and the lit top edge drawn over it
    readonly property color containerBorder: _m(Qt.rgba(1, 1, 1, 0.065), Qt.rgba(0, 0, 0, 0.07))
    readonly property color containerHighlight: _m(Qt.rgba(1, 1, 1, 0.16), Qt.rgba(1, 1, 1, 0.9))
    /// What content fades into at a container's edge (EdgeFade): the
    /// container over the page, as near as one colour gets
    readonly property color containerSolid: _m(_night(12, 12, 17, 0.05, 1), "#FFFFFF")

    readonly property color sidebarBg: container
    readonly property color sidebarBorder: containerBorder

    readonly property color bgGradientStart: bgPrimary
    readonly property color bgGradientEnd: bgSecondary

    // =========================================================================
    // SURFACES
    // =========================================================================

    /// Opaque content surface (where translucency would not do)
    readonly property color surface: _m("#121217", "#FFFFFF")
    /// Inset element on a container
    readonly property color surfaceLight: _m("#1B1B22", "#F2F2F4")
    readonly property color surfaceHover: _m("#17171D", "#F7F7F9")
    readonly property color surfaceActive: _m("#1E1E26", "#EDEDF0")
    /// Sheets, popovers, menus, toasts
    readonly property color surfaceElevated: _m("#18181F", "#FFFFFF")

    // =========================================================================
    // FILLS (controls on any background) — systemFill family
    // =========================================================================

    // White light on near-black: grey fills read muddy there.
    readonly property color fill: _m(Qt.rgba(1, 1, 1, 0.12), Qt.rgba(120/255, 120/255, 128/255, 0.20))
    readonly property color fillSecondary: _m(Qt.rgba(1, 1, 1, 0.095), Qt.rgba(120/255, 120/255, 128/255, 0.16))
    readonly property color fillTertiary: _m(Qt.rgba(1, 1, 1, 0.07), Qt.rgba(118/255, 118/255, 128/255, 0.12))
    readonly property color fillQuaternary: _m(Qt.rgba(1, 1, 1, 0.045), Qt.rgba(116/255, 116/255, 128/255, 0.08))

    /// Hairline between rows and around cards
    readonly property color separator: _m(Qt.rgba(1, 1, 1, 0.065), Qt.rgba(0, 0, 0, 0.09))
    readonly property color separatorStrong: _m(Qt.rgba(1, 1, 1, 0.12), Qt.rgba(0, 0, 0, 0.16))

    /// Behind a sheet or alert
    readonly property color scrim: _m(Qt.rgba(0, 0, 0, 0.55), Qt.rgba(0, 0, 0, 0.28))

    // =========================================================================
    // ACCENT — user-picked (SettingsManager.accentPreset), Apple system colours
    // =========================================================================

    property string _accentPresetId: typeof SettingsManager !== "undefined" ? SettingsManager.accentPreset : "purple"

    // [fill on dark, text on dark, fill on light] per preset — Makine's
    // presets from 0.1.5: a fill strong enough for white text, a lighter
    // tone for labels and icons on near-black.
    readonly property var _accentPresets: ({
        "purple": ["#8B5CF6", "#A78BFA", "#7C3AED"],
        "blue":   ["#3B82F6", "#60A5FA", "#2563EB"],
        "indigo": ["#6366F1", "#818CF8", "#4F46E5"],
        "sky":    ["#0EA5E9", "#38BDF8", "#0284C7"],
        "teal":   ["#14B8A6", "#2DD4BF", "#0D9488"],
        "green":  ["#22C55E", "#4ADE80", "#16A34A"],
        "amber":  ["#F59E0B", "#FBBF24", "#D97706"],
        "rose":   ["#EC4899", "#F472B6", "#DB2777"],
        "red":    ["#EF4444", "#F87171", "#DC2626"],
        "black":  ["#71717A", "#A1A1AA", "#52525B"]
    })
    readonly property var _accent: _accentPresets[_accentPresetId] || _accentPresets["purple"]
    readonly property color accentBase: darkMode ? _accent[0] : _accent[2]
    readonly property color accentLightest: Qt.lighter(_accent[1], 1.2)
    readonly property color accentLight: darkMode ? _accent[1] : _accent[2]
    readonly property color accentDark: Qt.darker(accentBase, 1.15)
    readonly property color accentDarkest: Qt.darker(accentBase, 1.4)
    readonly property var accentGradient: [accentLightest, accentLight, accentBase, accentDark, accentDarkest]

    /// The accent: filled buttons, selection, progress
    readonly property color primary: accentBase
    /// The accent as a label or icon on the page (links, selected items)
    readonly property color primaryText: accentLight
    readonly property color primaryHover: darkMode ? Qt.lighter(accentBase, 1.1) : Qt.darker(accentBase, 1.08)
    readonly property color primaryGlow: withAlpha(accentBase, 0.25)
    readonly property color secondary: accentBase
    readonly property color secondaryHover: primaryHover

    /// A second, cool highlight (systemCyan) — rare: links on art, info marks
    readonly property color accent: _m("#67E8F9", "#0891B2")
    readonly property color accentHover: _m("#A5F3FC", "#0E7490")

    // =========================================================================
    // STATUS — system green / orange / red / yellow
    // =========================================================================

    readonly property color success: _m("#34D399", "#059669")
    readonly property color warning: _m("#FBBF24", "#B45309")
    readonly property color error: _m("#F87171", "#DC2626")
    readonly property color yellow: _m("#FDE047", "#A16207")
    readonly property color info: accent
    readonly property color destructive: error

    readonly property color successBg: withAlpha(success, darkMode ? 0.14 : 0.10)
    readonly property color warningBg: withAlpha(warning, darkMode ? 0.14 : 0.10)
    readonly property color errorBg: withAlpha(error, darkMode ? 0.14 : 0.10)

    // =========================================================================
    // LABELS
    // =========================================================================

    // Warm off-white on the cool page, as 0.1.5 had it
    readonly property color textPrimary: _m("#ECE9E3", "#1D1D1F")
    readonly property color textSecondary: _m("#AFACA6", "#515154")
    /// Descriptions, hints, metadata
    readonly property color textMuted: _m("#88857F", "#6E6E73")
    /// Placeholders, disabled
    readonly property color textTertiary: _m("#5F5D59", "#A1A1A6")
    readonly property color textDisabled: _m("#46454A", "#C7C7CC")
    /// On the accent, on art, on status fills
    readonly property color textOnColor: "#FFFFFF"

    // =========================================================================
    // BORDERS
    // =========================================================================

    readonly property color border: separator
    readonly property color borderHover: separatorStrong
    readonly property color borderFocus: accentBase
    readonly property color outlinedBorder: separatorStrong

    // =========================================================================
    // MATERIALS — "glass" on art (hero, covers, toasts)
    // =========================================================================

    readonly property color glassBackground: Qt.rgba(1, 1, 1, darkMode ? 0.06 : 0.55)
    readonly property color glassBorder: Qt.rgba(1, 1, 1, darkMode ? 0.10 : 0.6)
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, 0.18)
    /// Dark glass for controls laid over cover art (any appearance)
    readonly property color glassOnArt: Qt.rgba(0.04, 0.04, 0.06, 0.66)
    readonly property color glassOnArtHover: Qt.rgba(0.10, 0.10, 0.13, 0.78)

    // =========================================================================
    // SHADOWS
    // =========================================================================

    readonly property color shadowSoft: Qt.rgba(0, 0, 0, darkMode ? 0.35 : 0.08)
    readonly property color shadowMedium: Qt.rgba(0, 0, 0, darkMode ? 0.50 : 0.14)
    readonly property color shadowHard: Qt.rgba(0, 0, 0, darkMode ? 0.65 : 0.22)

    // =========================================================================
    // BRAND (logo, onboarding) — Makine's own palette
    // =========================================================================

    readonly property color gold: "#DDC66A"
    readonly property color olive: "#759764"
    readonly property color pastelBlue: "#A4C2C9"
    readonly property var brandGradient: [
        "#FCCD66", "#F7AE76", "#EE968F", "#CC9FD8",
        "#90C2E6", "#77DBC8", "#80E59D", "#C8EB7C", "#D4BE77"
    ]

    // =========================================================================
    // SPECIAL
    // =========================================================================

    readonly property color closeButtonHover: "#E81123"
    readonly property color turkishRed: "#E30A17"
    readonly property color discordColor: "#5865F2"
    readonly property color youtubeColor: "#FF0033"
    readonly property color steamBlue: "#66C0F4"
    readonly property color statusOnline: success

    readonly property color severityLow: _m("#FFD60A", "#B25000")
    readonly property color severityMedium: warning
    readonly property color severityCritical: error

    readonly property color verifiedBg: successBg
    readonly property color verifiedText: success
    readonly property color warningBadgeBg: warningBg
    readonly property color warningBadgeText: warning
    readonly property color errorBadgeBg: errorBg
    readonly property color errorBadgeText: error

    readonly property color scrollbarThumb: _m(Qt.rgba(1, 1, 1, 0.16), Qt.rgba(0, 0, 0, 0.22))
    readonly property color scrollbarThumbHover: _m(Qt.rgba(1, 1, 1, 0.32), Qt.rgba(0, 0, 0, 0.40))
    readonly property color scrollbarTrack: "transparent"

    // =========================================================================
    // ALPHA VARIANTS (names: <colour><percent>)
    // =========================================================================

    readonly property color textPrimary03: withAlpha(textPrimary, 0.03)
    readonly property color textPrimary04: withAlpha(textPrimary, 0.04)
    readonly property color textPrimary05: withAlpha(textPrimary, 0.05)
    readonly property color textPrimary06: withAlpha(textPrimary, 0.06)
    readonly property color textPrimary08: withAlpha(textPrimary, 0.08)
    readonly property color textPrimary10: withAlpha(textPrimary, 0.10)
    readonly property color textPrimary12: withAlpha(textPrimary, 0.12)
    readonly property color textPrimary15: withAlpha(textPrimary, 0.15)
    readonly property color textPrimary20: withAlpha(textPrimary, 0.20)
    readonly property color textPrimary25: withAlpha(textPrimary, 0.25)

    readonly property color primary03: withAlpha(primary, 0.03)
    readonly property color primary04: withAlpha(primary, 0.04)
    readonly property color primary05: withAlpha(primary, 0.05)
    readonly property color primary06: withAlpha(primary, 0.06)
    readonly property color primary08: withAlpha(primary, 0.08)
    readonly property color primary10: withAlpha(primary, 0.10)
    readonly property color primary12: withAlpha(primary, 0.12)
    readonly property color primary15: withAlpha(primary, 0.15)
    readonly property color primary20: withAlpha(primary, 0.20)
    readonly property color primary25: withAlpha(primary, 0.25)
    readonly property color primary30: withAlpha(primary, 0.30)
    readonly property color primary35: withAlpha(primary, 0.35)
    readonly property color primary40: withAlpha(primary, 0.40)
    readonly property color primary50: withAlpha(primary, 0.50)
    readonly property color primary60: withAlpha(primary, 0.60)

    readonly property color accent10: withAlpha(accent, 0.10)
    readonly property color accent15: withAlpha(accent, 0.15)
    readonly property color accent20: withAlpha(accent, 0.20)
    readonly property color accent25: withAlpha(accent, 0.25)
    readonly property color accent30: withAlpha(accent, 0.30)
    readonly property color accent40: withAlpha(accent, 0.40)
    readonly property color accent85: withAlpha(accent, 0.85)

    readonly property color error06: withAlpha(error, 0.06)
    readonly property color error08: withAlpha(error, 0.08)
    readonly property color error10: withAlpha(error, 0.10)
    readonly property color error12: withAlpha(error, 0.12)
    readonly property color error15: withAlpha(error, 0.15)
    readonly property color error20: withAlpha(error, 0.20)
    readonly property color error25: withAlpha(error, 0.25)
    readonly property color error40: withAlpha(error, 0.40)

    readonly property color warning06: withAlpha(warning, 0.06)
    readonly property color warning08: withAlpha(warning, 0.08)
    readonly property color warning10: withAlpha(warning, 0.10)
    readonly property color warning12: withAlpha(warning, 0.12)
    readonly property color warning15: withAlpha(warning, 0.15)
    readonly property color warning20: withAlpha(warning, 0.20)
    readonly property color warning25: withAlpha(warning, 0.25)
    readonly property color warning60: withAlpha(warning, 0.60)

    readonly property color success08: withAlpha(success, 0.08)
    readonly property color success10: withAlpha(success, 0.10)
    readonly property color success12: withAlpha(success, 0.12)
    readonly property color success85: withAlpha(success, 0.85)

    readonly property color bgPrimary15: withAlpha(bgPrimary, 0.15)
    readonly property color bgPrimary60: withAlpha(bgPrimary, 0.60)
    readonly property color bgPrimary85: withAlpha(bgPrimary, 0.85)
    readonly property color bgPrimary90: withAlpha(bgPrimary, 0.90)

    readonly property color surface50: withAlpha(surface, 0.50)
    readonly property color surface92: withAlpha(surface, 0.92)

    readonly property color destructive08: withAlpha(destructive, 0.08)
    readonly property color destructive10: withAlpha(destructive, 0.10)
    readonly property color destructive12: withAlpha(destructive, 0.12)
    readonly property color destructive15: withAlpha(destructive, 0.15)
    readonly property color destructive20: withAlpha(destructive, 0.20)

    readonly property color accentBase30: withAlpha(accentBase, 0.30)
    readonly property color accentBase40: withAlpha(accentBase, 0.40)

    readonly property color textMuted08: withAlpha(textMuted, 0.08)
    readonly property color textMuted15: withAlpha(textMuted, 0.15)

    // =========================================================================
    // HELPERS
    // =========================================================================

    function withAlpha(color, alpha) {
        if (!color) return Qt.rgba(0, 0, 0, alpha)
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function darken(color, factor) {
        return Qt.darker(color, 1 + (factor || 0.1))
    }

    function lighten(color, factor) {
        return Qt.lighter(color, 1 + (factor || 0.1))
    }

    function lerpColor(c1, c2, t) {
        return Qt.rgba(c1.r + (c2.r - c1.r) * t, c1.g + (c2.g - c1.g) * t,
                       c1.b + (c2.b - c1.b) * t, c1.a + (c2.a - c1.a) * t)
    }
}
