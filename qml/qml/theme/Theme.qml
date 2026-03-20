pragma Singleton
import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Theme.qml - Application color definitions and theme system
 * Supports dark/light mode via SettingsManager.isDarkMode
 */
QtObject {
    // =========================================================================
    // THEME MODE
    // =========================================================================

    /// Dark mode active (default true when SettingsManager unavailable)
    property bool darkMode: typeof SettingsManager !== "undefined" ? SettingsManager.isDarkMode : true

    // =========================================================================
    // BACKGROUND COLORS
    // =========================================================================

    /// Ana arka plan — matches makineceviri.org
    readonly property color bgPrimary: darkMode ? "#07070a" : lightBackground

    /// İkincil arka plan
    readonly property color bgSecondary: darkMode ? "#0d0d11" : lightSurface

    /// Gradient başlangıç
    readonly property color bgGradientStart: darkMode ? "#07070a" : "#F8F8FA"

    /// Gradient bitiş
    readonly property color bgGradientEnd: darkMode ? "#0d0d14" : "#F0F0F5"

    // =========================================================================
    // SURFACE COLORS (Cards, Panels)
    // =========================================================================

    /// Normal surface — matches makineceviri.org
    readonly property color surface: darkMode ? "#121216" : lightSurface

    /// Light surface
    readonly property color surfaceLight: darkMode ? "#1b1b21" : lightSurfaceLight

    /// Hover state
    readonly property color surfaceHover: darkMode ? "#18181b" : "#E8E8EC"

    /// Active/selected state
    readonly property color surfaceActive: darkMode ? "#1b1b21" : "#E0E0E8"

    /// Yükseltilmiş yüzey (modal, dropdown)
    readonly property color surfaceElevated: darkMode ? "#222228" : "#FFFFFF"

    // =========================================================================
    // ANA RENKLER
    // =========================================================================

    /// Birincil renk (accent preset'e bağlı)
    readonly property color primary: accentBase

    /// Birincil hover
    readonly property color primaryHover: accentDark

    /// Birincil glow (alfa ile — uses pre-computed accentBase property binding)
    readonly property color primaryGlow: withAlpha(accentBase, 0.25)

    // =========================================================================
    // ACCENT COLOR SYSTEM (user-configurable via SettingsManager.accentPreset)
    // =========================================================================

    /// Active accent preset ID
    property string _accentPresetId: typeof SettingsManager !== "undefined" ? SettingsManager.accentPreset : "purple"

    /// Internal: load colors from preset
    property var _accentColors: _resolveAccentColors(_accentPresetId)

    function _resolveAccentColors(presetId) {
        var presets = {
            "purple":   ["#C4B5FD", "#A78BFA", "#8B5CF6", "#7C3AED", "#6D28D9"],
            "blue":     ["#93C5FD", "#60A5FA", "#3B82F6", "#2563EB", "#1D4ED8"],
            "teal":     ["#5EEAD4", "#2DD4BF", "#14B8A6", "#0D9488", "#0F766E"],
            "green":    ["#86EFAC", "#4ADE80", "#22C55E", "#16A34A", "#15803D"],
            "rose":     ["#F9A8D4", "#F472B6", "#EC4899", "#DB2777", "#BE185D"],
            "amber":    ["#FCD34D", "#FBBF24", "#F59E0B", "#D97706", "#B45309"],
            "red":      ["#FCA5A5", "#F87171", "#EF4444", "#DC2626", "#B91C1C"],
            "sky":      ["#7DD3FC", "#38BDF8", "#0EA5E9", "#0284C7", "#0369A1"],
            "indigo":   ["#C7D2FE", "#A5B4FC", "#818CF8", "#6366F1", "#4F46E5"],
            "black":    ["#D4D4D8", "#A1A1AA", "#71717A", "#52525B", "#3F3F46"]
        }
        return presets[presetId] || presets["purple"]
    }

    /// Accent tones (lightest → darkest)
    readonly property color accentLightest: _accentColors[0]
    readonly property color accentLight:    _accentColors[1]
    readonly property color accentBase:     _accentColors[2]
    readonly property color accentDark:     _accentColors[3]
    readonly property color accentDarkest:  _accentColors[4]

    /// Full gradient array for Canvas usage
    readonly property var accentGradient: [accentLightest, accentLight, accentBase, accentDark, accentDarkest]

    /// Backwards compatibility aliases
    readonly property color secondary: accentBase
    readonly property color secondaryHover: accentDark

    /// Vurgu cyan
    readonly property color accent: "#06B6D4"

    /// Vurgu hover
    readonly property color accentHover: "#0891B2"

    // =========================================================================
    // DURUM RENKLERİ
    // =========================================================================

    /// Başarı yeşil
    readonly property color success: "#10B981"

    /// Başarı arka plan (15% alpha)
    readonly property color successBg: Qt.rgba(success.r, success.g, success.b, darkMode ? 0.08 : 0.12)

    /// Uyarı turuncu
    readonly property color warning: "#F59E0B"

    /// Uyarı arka plan
    readonly property color warningBg: Qt.rgba(warning.r, warning.g, warning.b, darkMode ? 0.08 : 0.12)

    /// Hata kırmızı
    readonly property color error: "#EF4444"

    /// Hata arka plan
    readonly property color errorBg: Qt.rgba(error.r, error.g, error.b, darkMode ? 0.08 : 0.12)

    /// Bilgi cyan (tracks accent)
    readonly property color info: accent

    // =========================================================================
    // METİN RENKLERİ
    // =========================================================================

    /// Birincil metin — warm cream (matches makineceviri.org)
    readonly property color textPrimary: darkMode ? "#dedad4" : lightTextPrimary

    /// Renkli/koyu arka plan üzerindeki beyaz metin
    readonly property color textOnColor: "#FFFFFF"

    /// İkincil metin
    readonly property color textSecondary: darkMode ? "#A1A1AA" : lightTextSecondary

    /// Soluk metin — warm gray (matches makineceviri.org)
    readonly property color textMuted: darkMode ? "#7d7a73" : lightTextMuted

    /// Devre dışı metin
    readonly property color textDisabled: darkMode ? "#52525B" : "#A1A1AA"

    // =========================================================================
    // BORDER / SEPARATOR COLORS
    // =========================================================================

    /// Normal kenar — matches makineceviri.org
    readonly property color border: darkMode ? "#1b1b21" : lightBorder

    /// Vurgulu kenar
    readonly property color borderHover: darkMode ? "#262632" : "#D4D4D8"

    /// Odak kenarlığı (accent preset'e bağlı)
    readonly property color borderFocus: accentBase

    /// Outlined button border
    readonly property color outlinedBorder: darkMode ? "#334155" : "#CBD5E1"

    // =========================================================================
    // CAM EFEKT RENKLERİ (Glass Morphism)
    // =========================================================================

    /// Cam arka plan - rgba(255,255,255,0.05)
    readonly property color glassBackground: Qt.rgba(1, 1, 1, 0.05)

    /// Cam kenar - rgba(255,255,255,0.1)
    readonly property color glassBorder: Qt.rgba(1, 1, 1, 0.1)

    /// Cam parlaklık - rgba(255,255,255,0.15)
    readonly property color glassHighlight: Qt.rgba(1, 1, 1, 0.15)

    // =========================================================================
    // GÖLGE RENKLERİ
    // =========================================================================

    /// Yumuşak gölge
    readonly property color shadowSoft: Qt.rgba(0, 0, 0, 0.16)

    /// Orta gölge
    readonly property color shadowMedium: Qt.rgba(0, 0, 0, 0.31)

    /// Sert gölge
    readonly property color shadowHard: Qt.rgba(0, 0, 0, 0.47)

    // =========================================================================
    // GRADIENT COLORS (Makine brand)
    // =========================================================================

    readonly property color gold: "#DDC66A"
    readonly property color olive: "#759764"
    readonly property color brown: "#9B7649"
    readonly property color pastelBlue: "#A4C2C9"

    // Pink for hover effects
    readonly property color pink: "#FF69B4"

    // Brand gradient palette (official Makine colors)
    readonly property var brandGradient: [
        "#FCCD66", "#F7AE76", "#EE968F", "#CC9FD8",
        "#90C2E6", "#77DBC8", "#80E59D", "#C8EB7C", "#D4BE77"
    ]

    // Logo gradient (fallback when logo image not loaded)
    readonly property color logoGold: "#E8C547"
    readonly property color logoCoral: "#E8A090"
    readonly property color logoGreen: "#90D090"

    // Named brand gradient colors
    readonly property color brandGold: "#FCCD66"
    readonly property color brandOrange: "#F7AE76"
    readonly property color brandCoral: "#EE968F"
    readonly property color brandPurple: "#CC9FD8"
    readonly property color brandBlue: "#90C2E6"
    readonly property color brandTeal: "#77DBC8"
    readonly property color brandGreen: "#80E59D"
    readonly property color brandLime: "#C8EB7C"
    readonly property color brandOlive: "#D4BE77"

    // Logo gradient transition colors (MakineLogo.qml multi-stop gradient)
    readonly property color logoAmber: "#DEA54B"
    readonly property color logoLavender: "#B8A0C8"
    readonly property color logoSky: "#90B8D0"
    readonly property color logoTeal: "#80C8B8"

    // =========================================================================
    // SPLASH SCREEN COLORS
    // =========================================================================

    readonly property color splashBackground: "#0A0A0F"
    readonly property color splashGold: "#FFD700"
    readonly property color splashOrange: "#FF8C00"
    readonly property color splashPink: "#FF69B4"
    readonly property color splashOrchid: "#DA70D6"

    // =========================================================================
    // SPECIAL UI COLORS
    // =========================================================================

    /// Windows close button hover
    readonly property color closeButtonHover: "#E81123"

    /// Turkish flag red (translation mode indicator)
    readonly property color turkishRed: "#E30A17"

    /// Titlebar background
    readonly property color titlebarBg: "#151515"

    /// Steam orange (for warnings)
    readonly property color steamOrange: "#FF9800"

    /// Discord brand color
    readonly property color discordColor: "#5865F2"

    /// Destructive red
    readonly property color destructive: "#E53935"

    /// Status colors
    readonly property color statusOnline: "#4CAF50"
    readonly property color statusPurple: "#9C27B0"
    readonly property color statusCyan: "#00BCD4"

    // =========================================================================
    // NOTIFICATION COLORS
    // =========================================================================

    readonly property color notificationUpdate: "#4DA6FF"
    readonly property color notificationWarning: "#FFB347"
    readonly property color notificationError: "#FF6B6B"

    // =========================================================================
    // SEVERITY COLORS (AntiCheat warnings)
    // =========================================================================

    readonly property color severityLow: "#FFB74D"
    readonly property color severityMedium: "#FF9800"
    readonly property color severityCritical: "#B71C1C"

    // =========================================================================
    // CARD DARK GRADIENT (CedraInteractiveCard backgrounds)
    // =========================================================================

    readonly property color cardDarkStart: "#1A1A2E"
    readonly property color cardDarkMid: "#12121F"
    readonly property color cardDarkEnd: "#0A0A14"
    readonly property color cardDarkerStart: "#0D0D12"
    readonly property color cardDarkerMid: "#10101A"

    // =========================================================================
    // SCORE / QUALITY COLORS
    // =========================================================================

    readonly property color scoreExcellent: "#66CC33"
    readonly property color scoreGood: "#88BB44"
    readonly property color scoreFair: "#FFCC33"
    readonly property color scorePoor: "#FF9933"
    readonly property color scoreBad: "#FF0000"

    // =========================================================================
    // SCROLLBAR COLORS
    // =========================================================================

    readonly property color scrollbarThumb: darkMode ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color scrollbarThumbHover: darkMode ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.25)
    readonly property color scrollbarTrack: "transparent"

    // =========================================================================
    // LIGHT MODE
    // =========================================================================

    readonly property color lightBackground: "#F5F5F5"
    readonly property color lightSurface: "#FFFFFF"
    readonly property color lightSurfaceLight: "#EEEEEE"
    readonly property color lightPrimary: "#2563EB"
    readonly property color lightTextPrimary: "#18181B"
    readonly property color lightTextSecondary: "#52525B"
    readonly property color lightTextMuted: "#71717A"
    readonly property color lightBorder: "#E4E4E7"

    // =========================================================================
    // BADGE COLORS
    // =========================================================================

    readonly property color verifiedBg: Qt.rgba(0.063, 0.725, 0.506, 0.15)
    readonly property color verifiedText: "#10B981"

    readonly property color warningBadgeBg: Qt.rgba(0.961, 0.620, 0.043, 0.15)
    readonly property color warningBadgeText: "#F59E0B"

    readonly property color errorBadgeBg: Qt.rgba(0.937, 0.267, 0.267, 0.15)
    readonly property color errorBadgeText: "#EF4444"

    // =========================================================================
    // PRE-COMPUTED ALPHA COLORS (zero-rebinding optimization)
    // Naming: <colorName><alphaPercent> — e.g. textPrimary06 = textPrimary @ 6%
    // =========================================================================

    // textPrimary + alpha
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

    // primary + alpha
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
    readonly property color primary85: withAlpha(primary, 0.85)

    // accent + alpha
    readonly property color accent06: withAlpha(accent, 0.06)
    readonly property color accent10: withAlpha(accent, 0.10)
    readonly property color accent15: withAlpha(accent, 0.15)
    readonly property color accent18: withAlpha(accent, 0.18)
    readonly property color accent20: withAlpha(accent, 0.20)
    readonly property color accent25: withAlpha(accent, 0.25)
    readonly property color accent30: withAlpha(accent, 0.30)
    readonly property color accent40: withAlpha(accent, 0.40)
    readonly property color accent85: withAlpha(accent, 0.85)

    // error + alpha
    readonly property color error06: withAlpha(error, 0.06)
    readonly property color error08: withAlpha(error, 0.08)
    readonly property color error10: withAlpha(error, 0.10)
    readonly property color error12: withAlpha(error, 0.12)
    readonly property color error15: withAlpha(error, 0.15)
    readonly property color error20: withAlpha(error, 0.20)
    readonly property color error25: withAlpha(error, 0.25)
    readonly property color error30: withAlpha(error, 0.30)
    readonly property color error40: withAlpha(error, 0.40)

    // warning + alpha
    readonly property color warning06: withAlpha(warning, 0.06)
    readonly property color warning08: withAlpha(warning, 0.08)
    readonly property color warning10: withAlpha(warning, 0.10)
    readonly property color warning12: withAlpha(warning, 0.12)
    readonly property color warning15: withAlpha(warning, 0.15)
    readonly property color warning20: withAlpha(warning, 0.20)
    readonly property color warning25: withAlpha(warning, 0.25)
    readonly property color warning60: withAlpha(warning, 0.60)

    // success + alpha
    readonly property color success08: withAlpha(success, 0.08)
    readonly property color success10: withAlpha(success, 0.10)
    readonly property color success12: withAlpha(success, 0.12)
    readonly property color success20: withAlpha(success, 0.20)
    readonly property color success60: withAlpha(success, 0.60)
    readonly property color success85: withAlpha(success, 0.85)

    // bgPrimary + alpha
    readonly property color bgPrimary15: withAlpha(bgPrimary, 0.15)
    readonly property color bgPrimary40: withAlpha(bgPrimary, 0.40)
    readonly property color bgPrimary50: withAlpha(bgPrimary, 0.50)
    readonly property color bgPrimary60: withAlpha(bgPrimary, 0.60)
    readonly property color bgPrimary65: withAlpha(bgPrimary, 0.65)
    readonly property color bgPrimary70: withAlpha(bgPrimary, 0.70)
    readonly property color bgPrimary82: withAlpha(bgPrimary, 0.82)
    readonly property color bgPrimary85: withAlpha(bgPrimary, 0.85)
    readonly property color bgPrimary90: withAlpha(bgPrimary, 0.90)

    // surface + alpha
    readonly property color surface50: withAlpha(surface, 0.50)
    readonly property color surface60: withAlpha(surface, 0.60)
    readonly property color surface70: withAlpha(surface, 0.70)
    readonly property color surface92: withAlpha(surface, 0.92)

    // destructive + alpha
    readonly property color destructive08: withAlpha(destructive, 0.08)
    readonly property color destructive10: withAlpha(destructive, 0.10)
    readonly property color destructive12: withAlpha(destructive, 0.12)
    readonly property color destructive15: withAlpha(destructive, 0.15)
    readonly property color destructive20: withAlpha(destructive, 0.20)
    readonly property color destructive25: withAlpha(destructive, 0.25)

    // accentBase + alpha
    readonly property color accentBase30: withAlpha(accentBase, 0.30)
    readonly property color accentBase40: withAlpha(accentBase, 0.40)

    // textMuted + alpha
    readonly property color textMuted05: withAlpha(textMuted, 0.05)
    readonly property color textMuted08: withAlpha(textMuted, 0.08)
    readonly property color textMuted10: withAlpha(textMuted, 0.10)
    readonly property color textMuted15: withAlpha(textMuted, 0.15)
    readonly property color textMuted70: withAlpha(textMuted, 0.70)

    // surfaceActive + alpha
    readonly property color surfaceActive50: withAlpha(surfaceActive, 0.50)

    // Steam blue (HeroSection)
    readonly property color steamBlue: "#66c0f4"
    readonly property color steamBlue50: withAlpha(steamBlue, 0.50)
    readonly property color steamBlue20: withAlpha(steamBlue, 0.20)

    // =========================================================================
    // HELPER FUNCTIONS
    // =========================================================================

    /// Alpha color cache (invalidated on accent change)
    property var _alphaCache: ({})
    on_AccentColorsChanged: _alphaCache = ({})

    /// Rengi alfa ile döndür (cached for dynamic colors)
    function withAlpha(color, alpha) {
        if (!color) return Qt.rgba(0, 0, 0, alpha)
        var key = "" + color + alpha
        var cached = _alphaCache[key]
        if (cached !== undefined) return cached
        var result = Qt.rgba(color.r, color.g, color.b, alpha)
        _alphaCache[key] = result
        return result
    }

    /// Rengi koyulaştır
    function darken(color, factor) {
        factor = factor || 0.1
        return Qt.darker(color, 1 + factor)
    }

    /// Rengi açıklaştır
    function lighten(color, factor) {
        factor = factor || 0.1
        return Qt.lighter(color, 1 + factor)
    }

    /// Linear color interpolation (c1 → c2 by factor t)
    function lerpColor(c1, c2, t) {
        return Qt.rgba(
            c1.r + (c2.r - c1.r) * t,
            c1.g + (c2.g - c1.g) * t,
            c1.b + (c2.b - c1.b) * t,
            c1.a + (c2.a - c1.a) * t
        )
    }
}
