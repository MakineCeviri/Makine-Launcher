pragma Singleton
import QtQuick
import MakineAI 1.0

/**
 * Theme.qml - Native Qt colors.h birebir port
 * Kaynak: ui/src/theme/colors.h
 * Supports dark/light mode via SettingsManager.isDarkMode
 */
QtObject {
    // =========================================================================
    // TEMA MODU
    // =========================================================================

    /// Karanlık mod aktif mi (SettingsManager'dan okunur)
    property bool darkMode: SettingsManager.isDarkMode

    // =========================================================================
    // ARKA PLAN RENKLERİ (colors.h)
    // =========================================================================

    /// Ana arka plan - en koyu
    readonly property color bgPrimary: darkMode ? "#0A0A0B" : lightBackground

    /// İkincil arka plan - hafif mavi ton
    readonly property color bgSecondary: darkMode ? "#12121A" : lightSurface

    /// Gradient başlangıç
    readonly property color bgGradientStart: darkMode ? "#0A0A0B" : "#F8F8FA"

    /// Gradient bitiş
    readonly property color bgGradientEnd: darkMode ? "#0F0F19" : "#F0F0F5"

    // =========================================================================
    // YÜZEY RENKLERİ (Kartlar, Paneller) - Flutter AppColors birebir
    // =========================================================================

    /// Normal yüzey - Flutter: darkSurface
    readonly property color surface: darkMode ? "#18181B" : lightSurface

    /// Açık yüzey - Flutter: darkSurfaceLight
    readonly property color surfaceLight: darkMode ? "#27272A" : lightSurfaceLight

    /// Hover durumu
    readonly property color surfaceHover: darkMode ? "#1F1F23" : "#E8E8EC"

    /// Aktif/seçili durum - Flutter: darkSurfaceLight ile aynı
    readonly property color surfaceActive: darkMode ? "#27272A" : "#E0E0E8"

    /// Yükseltilmiş yüzey (modal, dropdown)
    readonly property color surfaceElevated: darkMode ? "#2A2A2E" : "#FFFFFF"

    // =========================================================================
    // ANA RENKLER
    // =========================================================================

    /// Birincil mavi
    readonly property color primary: darkMode ? "#3B82F6" : lightPrimary

    /// Birincil hover
    readonly property color primaryHover: "#2563EB"

    /// Birincil glow (alfa ile)
    readonly property color primaryGlow: Qt.rgba(0.231, 0.510, 0.965, 0.25) // #3B82F640

    /// İkincil mor
    readonly property color secondary: "#8B5CF6"

    /// İkincil hover
    readonly property color secondaryHover: "#7C3AED"

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
    readonly property color successBg: Qt.rgba(0.063, 0.725, 0.506, darkMode ? 0.08 : 0.12)

    /// Uyarı turuncu
    readonly property color warning: "#F59E0B"

    /// Uyarı arka plan
    readonly property color warningBg: Qt.rgba(0.961, 0.620, 0.043, darkMode ? 0.08 : 0.12)

    /// Hata kırmızı
    readonly property color error: "#EF4444"

    /// Hata arka plan
    readonly property color errorBg: Qt.rgba(0.937, 0.267, 0.267, darkMode ? 0.08 : 0.12)

    /// Bilgi cyan
    readonly property color info: "#06B6D4"

    // =========================================================================
    // METİN RENKLERİ
    // =========================================================================

    /// Birincil metin - en parlak
    readonly property color textPrimary: darkMode ? "#FAFAFA" : lightTextPrimary

    /// İkincil metin
    readonly property color textSecondary: darkMode ? "#A1A1AA" : lightTextSecondary

    /// Soluk metin
    readonly property color textMuted: darkMode ? "#71717A" : lightTextMuted

    /// Devre dışı metin
    readonly property color textDisabled: darkMode ? "#52525B" : "#A1A1AA"

    // =========================================================================
    // KENAR / AYIRICI RENKLERİ - Flutter AppTheme birebir
    // =========================================================================

    /// Normal kenar
    readonly property color border: darkMode ? "#27272A" : lightBorder

    /// Vurgulu kenar
    readonly property color borderHover: darkMode ? "#3F3F46" : "#D4D4D8"

    /// Odak kenarlığı
    readonly property color borderFocus: "#3B82F6"

    /// Outlined button border - Flutter: OutlinedButton side color
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
    // GRADIENT COLORS (MakineAI brand)
    // =========================================================================

    readonly property color gold: "#DDC66A"
    readonly property color olive: "#759764"
    readonly property color brown: "#9B7649"
    readonly property color pastelBlue: "#A4C2C9"

    // Pink for hover effects
    readonly property color pink: "#FF69B4"

    // Brand gradient palette (official MakineAI colors)
    readonly property var brandGradient: [
        "#FCCD66", "#F7AE76", "#EE968F", "#CC9FD8",
        "#90C2E6", "#77DBC8", "#80E59D", "#C8EB7C", "#D4BE77"
    ]

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

    // =========================================================================
    // SPLASH SCREEN COLORS
    // =========================================================================

    readonly property color splashGold: "#FFD700"
    readonly property color splashOrange: "#FF8C00"
    readonly property color splashPink: "#FF69B4"
    readonly property color splashOrchid: "#DA70D6"

    // =========================================================================
    // SPECIAL UI COLORS
    // =========================================================================

    /// Windows close button hover
    readonly property color closeButtonHover: "#E81123"

    /// Titlebar background
    readonly property color titlebarBg: "#151515"

    /// Steam orange (for warnings)
    readonly property color steamOrange: "#FF9800"

    /// Destructive red
    readonly property color destructive: "#E53935"

    /// Status colors
    readonly property color statusOnline: "#4CAF50"
    readonly property color statusPurple: "#9C27B0"
    readonly property color statusCyan: "#00BCD4"

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
    // HELPER FUNCTIONS
    // =========================================================================

    /// Rengi alfa ile döndür
    function withAlpha(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
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
