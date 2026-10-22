// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

pragma Singleton
import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Theme.qml — colours of the launcher.
 *
 * Apple's system palette (Human Interface Guidelines, dark and light): one
 * accent the user picks, system status colours, labels in four strengths,
 * translucent fills for controls and hairline separators. docs/design.md
 * says which token goes where. Older names (textMuted, primary12, …) stay
 * as aliases so every screen reads the same palette.
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

    /// Window / page background
    readonly property color bgPrimary: _m("#141416", "#F5F5F7")
    /// Grouped content and cards (secondarySystemBackground)
    readonly property color bgSecondary: _m("#1C1C1E", "#FFFFFF")
    /// Sidebar panel — a step lighter than the page in dark, a step darker in light
    readonly property color sidebarBg: _m("#1C1C1F", "#E9E9EC")
    readonly property color sidebarBorder: _m(Qt.rgba(1, 1, 1, 0.07), Qt.rgba(0, 0, 0, 0.06))

    readonly property color bgGradientStart: bgPrimary
    readonly property color bgGradientEnd: bgPrimary

    // =========================================================================
    // SURFACES
    // =========================================================================

    /// Card / grouped list section
    readonly property color surface: _m("#1C1C1E", "#FFFFFF")
    /// Inset element on a card (tertiarySystemBackground)
    readonly property color surfaceLight: _m("#2C2C2E", "#F2F2F4")
    readonly property color surfaceHover: _m("#242427", "#F7F7F9")
    readonly property color surfaceActive: _m("#2C2C2E", "#EDEDF0")
    /// Sheets, popovers, menus, toasts
    readonly property color surfaceElevated: _m("#232326", "#FFFFFF")

    // =========================================================================
    // FILLS (controls on any background) — systemFill family
    // =========================================================================

    readonly property color fill: _m(Qt.rgba(120/255, 120/255, 128/255, 0.36), Qt.rgba(120/255, 120/255, 128/255, 0.20))
    readonly property color fillSecondary: _m(Qt.rgba(120/255, 120/255, 128/255, 0.32), Qt.rgba(120/255, 120/255, 128/255, 0.16))
    readonly property color fillTertiary: _m(Qt.rgba(118/255, 118/255, 128/255, 0.24), Qt.rgba(118/255, 118/255, 128/255, 0.12))
    readonly property color fillQuaternary: _m(Qt.rgba(116/255, 116/255, 128/255, 0.18), Qt.rgba(116/255, 116/255, 128/255, 0.08))

    /// Hairline between rows and around cards
    readonly property color separator: _m(Qt.rgba(1, 1, 1, 0.09), Qt.rgba(0, 0, 0, 0.09))
    readonly property color separatorStrong: _m(Qt.rgba(1, 1, 1, 0.16), Qt.rgba(0, 0, 0, 0.16))

    /// Selected segment of a segmented control
    readonly property color segmentSelected: _m("#5A5A5E", "#FFFFFF")

    /// Behind a sheet or alert
    readonly property color scrim: _m(Qt.rgba(0, 0, 0, 0.55), Qt.rgba(0, 0, 0, 0.28))

    // =========================================================================
    // ACCENT — user-picked (SettingsManager.accentPreset), Apple system colours
    // =========================================================================

    property string _accentPresetId: typeof SettingsManager !== "undefined" ? SettingsManager.accentPreset : "blue"

    // [dark, light] per preset — the system colour in each appearance
    readonly property var _accentPresets: ({
        "blue":   ["#0A84FF", "#007AFF"],
        "purple": ["#BF5AF2", "#AF52DE"],
        "rose":   ["#FF375F", "#FF2D55"],
        "red":    ["#FF453A", "#FF3B30"],
        "amber":  ["#FF9F0A", "#FF9500"],
        "green":  ["#30D158", "#28CD41"],
        "teal":   ["#40C8E0", "#30B0C7"],
        "sky":    ["#64D2FF", "#32ADE6"],
        "indigo": ["#5E5CE6", "#5856D6"],
        "black":  ["#98989D", "#8E8E93"]
    })
    readonly property color accentBase: {
        const p = _accentPresets[_accentPresetId] || _accentPresets["blue"]
        return darkMode ? p[0] : p[1]
    }
    readonly property color accentLightest: Qt.lighter(accentBase, 1.45)
    readonly property color accentLight: darkMode ? Qt.lighter(accentBase, 1.22) : accentBase
    readonly property color accentDark: Qt.darker(accentBase, 1.15)
    readonly property color accentDarkest: Qt.darker(accentBase, 1.4)
    readonly property var accentGradient: [accentLightest, accentLight, accentBase, accentDark, accentDarkest]

    /// The accent: buttons, links, selection, progress
    readonly property color primary: accentBase
    readonly property color primaryHover: darkMode ? Qt.lighter(accentBase, 1.1) : Qt.darker(accentBase, 1.08)
    readonly property color primaryGlow: withAlpha(accentBase, 0.25)
    readonly property color secondary: accentBase
    readonly property color secondaryHover: primaryHover

    /// A second, cool highlight (systemCyan) — rare: links on art, info marks
    readonly property color accent: _m("#64D2FF", "#0091D5")
    readonly property color accentHover: _m("#8ADDFF", "#0079B3")

    // =========================================================================
    // STATUS — system green / orange / red / yellow
    // =========================================================================

    readonly property color success: _m("#30D158", "#248A3D")
    readonly property color warning: _m("#FF9F0A", "#CC6E00")
    readonly property color error: _m("#FF453A", "#D70015")
    readonly property color yellow: _m("#FFD60A", "#B25000")
    readonly property color info: accent
    readonly property color destructive: error

    readonly property color successBg: withAlpha(success, darkMode ? 0.14 : 0.10)
    readonly property color warningBg: withAlpha(warning, darkMode ? 0.14 : 0.10)
    readonly property color errorBg: withAlpha(error, darkMode ? 0.14 : 0.10)

    // =========================================================================
    // LABELS
    // =========================================================================

    readonly property color textPrimary: _m("#F5F5F7", "#1D1D1F")
    readonly property color textSecondary: _m("#AEAEB2", "#515154")
    /// Descriptions, hints, metadata
    readonly property color textMuted: _m("#8E8E93", "#6E6E73")
    /// Placeholders, disabled
    readonly property color textTertiary: _m("#636366", "#A1A1A6")
    readonly property color textDisabled: _m("#48484A", "#C7C7CC")
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

    readonly property color glassBackground: Qt.rgba(1, 1, 1, darkMode ? 0.08 : 0.55)
    readonly property color glassBorder: Qt.rgba(1, 1, 1, darkMode ? 0.12 : 0.6)
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, 0.18)
    /// Dark glass for controls laid over cover art (any appearance)
    readonly property color glassOnArt: Qt.rgba(0.08, 0.08, 0.09, 0.62)
    readonly property color glassOnArtHover: Qt.rgba(0.14, 0.14, 0.15, 0.74)

    // =========================================================================
    // SHADOWS
    // =========================================================================

    readonly property color shadowSoft: Qt.rgba(0, 0, 0, darkMode ? 0.30 : 0.08)
    readonly property color shadowMedium: Qt.rgba(0, 0, 0, darkMode ? 0.45 : 0.14)
    readonly property color shadowHard: Qt.rgba(0, 0, 0, darkMode ? 0.60 : 0.22)

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

    readonly property color scrollbarThumb: _m(Qt.rgba(1, 1, 1, 0.22), Qt.rgba(0, 0, 0, 0.22))
    readonly property color scrollbarThumbHover: _m(Qt.rgba(1, 1, 1, 0.40), Qt.rgba(0, 0, 0, 0.40))
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
