pragma Singleton
import QtQuick

/**
 * Dimensions.qml - Design system constants for MakineAI
 */
QtObject {
    // =========================================================================
    // APP INFO
    // =========================================================================

    readonly property string appName: "MakineAI"
    readonly property string appVersion: "0.1.0alpha"
    readonly property string appVersionFull: "v0.1.0alpha"
    readonly property string appBuildNumber: "1"
    readonly property string appDescription: qsTr("Türkçe Yama Launcher")
    readonly property string appCopyright: "© 2026 MakineAI"

    // =========================================================================
    // EXTERNAL LINKS
    // =========================================================================

    readonly property string discordUrl: "https://discord.com/invite/QDezpy4QtV"
    readonly property string websiteUrl: "https://makineçeviri.com/"
    readonly property string donateUrl: "https://www.patreon.com/makineceviri"
    readonly property string logoUrl: "https://makineceviri.com/assets/logo.png"
    readonly property string feedbackUrl: "https://makineai.com/feedback"
    readonly property string cedraDeveloperUrl: "https://cedra.dev"
    readonly property string donatePageUrl: "https://makineai.com/destekci-ol"

    // GitHub
    readonly property string githubOwner: "jlceaser"
    readonly property string githubRepo: "MakineAI"
    readonly property string githubReleasesUrl: "https://api.github.com/repos/" + githubOwner + "/" + githubRepo + "/releases/latest"

    // =========================================================================
    // MARGINS
    // =========================================================================

    readonly property int marginXXS: 2
    readonly property int marginXS: 4
    readonly property int marginSM: 8
    readonly property int marginBase: 10
    readonly property int marginMS: 12
    readonly property int marginMD: 16
    readonly property int marginML: 20
    readonly property int marginLG: 24
    readonly property int marginXL: 32
    readonly property int marginXXL: 48

    // =========================================================================
    // PADDING
    // =========================================================================

    readonly property int paddingXS: 4
    readonly property int paddingSM: 8
    readonly property int paddingMD: 12
    readonly property int paddingLG: 16
    readonly property int paddingXL: 24
    readonly property int paddingXXL: 48

    // =========================================================================
    // SPACING (Row/Column/Layout gaps)
    // =========================================================================

    readonly property int spacingXXS: 2
    readonly property int spacingXS: 4
    readonly property int spacingSM: 6
    readonly property int spacingMD: 8
    readonly property int spacingBase: 10
    readonly property int spacingLG: 12
    readonly property int spacingXL: 16
    readonly property int spacingXXL: 20
    readonly property int spacingSection: 24
    readonly property int spacingPage: 32

    // =========================================================================
    // BORDER RADIUS
    // =========================================================================

    // Standard radius for all cards, buttons, panels (4px = minimal, clean look)
    readonly property int radiusStandard: 4

    // Aliases (all point to standard for consistency)
    readonly property int radiusTiny: radiusStandard
    readonly property int radiusXS: radiusStandard
    readonly property int radiusSM: radiusStandard
    readonly property int radiusMD: radiusStandard
    readonly property int radiusLG: radiusStandard
    readonly property int radiusXL: radiusStandard
    readonly property int radiusFull: 9999

    // =========================================================================
    // NAVBAR
    // =========================================================================

    readonly property int navbarHeight: 56
    readonly property int navbarIconSize: 16
    readonly property int navbarIconSizeLogo: 32

    // =========================================================================
    // TITLEBAR
    // =========================================================================

    readonly property int titlebarHeight: 32
    readonly property int windowButtonSize: 28

    // =========================================================================
    // GAME CARD
    // =========================================================================

    readonly property int cardWidth: 140
    readonly property int cardHeight: 200
    readonly property int cardImageHeight: 160
    readonly property int cardGap: 16
    readonly property int cardBorderRadius: radiusStandard

    // =========================================================================
    // ANIMATION DURATIONS (ms)
    // =========================================================================

    readonly property int animInstant: 50
    readonly property int animVeryFast: 100
    readonly property int animFast: 150
    readonly property int animNormal: 250
    readonly property int animSlow: 400
    readonly property int animVerySlow: 800
    readonly property int animGradient: 2000

    // =========================================================================
    // BLUR
    // =========================================================================

    readonly property int blurLight: 10
    readonly property int blurMedium: 20
    readonly property int blurHeavy: 80

    // =========================================================================
    // FONT SIZES
    // =========================================================================

    // Semantic font scale
    readonly property int fontMicro: 8
    readonly property int fontMini: 9
    readonly property int fontCaption: 10
    readonly property int fontBody: 13
    readonly property int fontSubtitle: 15
    readonly property int fontTitle: 18
    readonly property int fontHeadline: 22
    readonly property int fontHero: 28
    readonly property int fontBanner: 32
    readonly property int fontSplash: 42

    // TextTheme sizes
    readonly property int displayXL: 56
    readonly property int displayLarge: 48
    readonly property int displaySmall: 40
    readonly property int displayMedium: 36
    readonly property int headlineXL: 26
    readonly property int headlineLarge: 24
    readonly property int headlineMedium: 20
    readonly property int titleLarge: 16
    readonly property int bodyLarge: 14
    readonly property int bodyMedium: 14
    readonly property int bodySmall: 12
    readonly property int labelSmall: 11

    // Legacy aliases
    readonly property int fontXS: 11
    readonly property int fontSM: 12
    readonly property int fontMD: 14
    readonly property int fontLG: 16
    readonly property int fontXL: 20
    readonly property int fontXXL: 24
    readonly property int fontDisplay: 48

    // Font weights
    readonly property int weightNormal: 50
    readonly property int weightMedium: 57
    readonly property int weightSemiBold: 63
    readonly property int weightBold: 75

    // Letter spacing
    readonly property real letterSpacingDisplay: -1.5
    readonly property real letterSpacingHeadline: -0.5
    readonly property real letterSpacingBody: 0.0
    readonly property real letterSpacingWide: 0.3
    readonly property real letterSpacingLoose: 1.5

    // =========================================================================
    // ICON SIZES
    // =========================================================================

    readonly property int iconXS: 14
    readonly property int iconSM: 16
    readonly property int iconMD: 20
    readonly property int iconLG: 24
    readonly property int iconXL: 32

    // =========================================================================
    // WINDOW SIZES
    // =========================================================================

    readonly property int minWindowWidth: 900
    readonly property int minWindowHeight: 620
    readonly property int defaultWindowWidth: 1024
    readonly property int defaultWindowHeight: 640

    // =========================================================================
    // INTERACTION DURATIONS
    // =========================================================================

    readonly property int hoverDuration: 150
    readonly property int transitionDuration: 200
    readonly property int fadeTransitionDuration: 300
    readonly property int splashDuration: 2500

    // =========================================================================
    // LIMITS
    // =========================================================================

    readonly property int maxManualGames: 100
    readonly property int maxRecentGames: 10
    readonly property int maxSearchResults: 50

    // =========================================================================
    // BUTTON DIMENSIONS
    // =========================================================================

    readonly property int buttonPaddingH: 28
    readonly property int buttonPaddingV: 14
    readonly property int buttonFontSize: 15

    // =========================================================================
    // INPUT FIELD DIMENSIONS
    // =========================================================================

    readonly property int inputPaddingH: 16
    readonly property int inputPaddingV: 14
    readonly property int inputBorderRadius: radiusStandard

    // =========================================================================
    // SCROLLBAR DIMENSIONS
    // =========================================================================

    readonly property int scrollbarWidth: 4
    readonly property int scrollbarRadius: radiusStandard
    readonly property int scrollbarMargin: 2

    // =========================================================================
    // PROGRESS BAR DIMENSIONS
    // =========================================================================

    readonly property int progressBarWidth: 200
    readonly property int progressBarHeight: 4
    readonly property int progressBarRadius: radiusStandard

    // =========================================================================
    // TOGGLE/SWITCH DIMENSIONS
    // =========================================================================

    readonly property int toggleWidth: 44
    readonly property int toggleHeight: 24
    readonly property int toggleRadius: radiusStandard
    readonly property int toggleKnobSize: 18
    readonly property int toggleKnobRadius: radiusStandard

    // =========================================================================
    // DIALOG DIMENSIONS
    // =========================================================================

    readonly property int dialogRadius: radiusStandard
    readonly property int badgeRadius: radiusStandard

    // =========================================================================
    // Z-INDEX LAYERS
    // =========================================================================

    readonly property int zBase: 0
    readonly property int zContent: 10
    readonly property int zOverlay: 50
    readonly property int zNavigation: 60
    readonly property int zHeader: 80
    readonly property int zDialog: 100
    readonly property int zWindowControls: 101
    readonly property int zToast: 200
    readonly property int zDebug: 9999
}
