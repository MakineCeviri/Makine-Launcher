pragma Singleton
import QtQuick
import MakineAI 1.0

/**
 * Dimensions.qml - Design system constants for MakineAI
 */
QtObject {
    // =========================================================================
    // APP INFO
    // =========================================================================

    readonly property string appName: "MakineAI"
    readonly property string appVersion: "0.1.0pre-alpha"
    readonly property string appVersionFull: "v0.1.0pre-alpha"

    // =========================================================================
    // EXTERNAL LINKS
    // =========================================================================

    readonly property string discordUrl: "https://discord.com/invite/QDezpy4QtV"
    readonly property string websiteUrl: "https://makineçeviri.com/"
    readonly property string donatePageUrl: "https://makineai.com/destekci-ol"

    // GitHub
    readonly property string githubOwner: "jlceaser"
    readonly property string githubRepo: "MakineAI"
    readonly property string githubReleasesUrl: "https://api.github.com/repos/" + githubOwner + "/" + githubRepo + "/releases/latest"

    // =========================================================================
    // MARGINS
    // =========================================================================

    readonly property int marginXXS: 2
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

    readonly property int paddingSM: 8
    readonly property int paddingMD: 12
    readonly property int paddingLG: 16

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

    readonly property int radiusStandard: 4
    readonly property int radiusXS: radiusStandard
    readonly property int radiusMD: radiusStandard
    readonly property int radiusLG: radiusStandard
    readonly property int radiusFull: 9999

    // =========================================================================
    // NAVBAR
    // =========================================================================

    readonly property int navbarHeight: 56
    readonly property int navbarIconSizeLogo: 32

    // =========================================================================
    // TITLEBAR
    // =========================================================================

    readonly property int titlebarHeight: 32

    // =========================================================================
    // GAME CARD
    // =========================================================================

    readonly property int cardWidth: 140
    readonly property int cardHeight: 200
    readonly property int cardImageHeight: 160
    readonly property int cardGap: 16
    readonly property int cardBorderRadius: radiusStandard

    // =========================================================================
    // LAYOUT CONSTANTS
    // =========================================================================

    readonly property int sidebarWidth: 280
    readonly property int patchCardExtraWidth: 10
    readonly property int patchCardExtraHeight: 30

    // =========================================================================
    // ANIMATION DURATIONS (ms) — respect user's animation preference
    // =========================================================================

    readonly property bool _animEnabled: typeof SettingsManager !== "undefined" ? SettingsManager.enableAnimations : true
    readonly property int animInstant: _animEnabled ? 50 : 0
    readonly property int animVeryFast: _animEnabled ? 100 : 0
    readonly property int animFast: _animEnabled ? 150 : 0
    readonly property int animNormal: _animEnabled ? 250 : 0
    readonly property int animSlow: _animEnabled ? 400 : 0
    readonly property int animVerySlow: _animEnabled ? 800 : 0
    readonly property int animGradient: _animEnabled ? 2000 : 0

    // =========================================================================
    // FONT SIZES
    // =========================================================================

    readonly property int fontMicro: 8
    readonly property int fontMini: 9
    readonly property int fontCaption: 10
    readonly property int fontBody: 13
    readonly property int fontSubtitle: 15
    readonly property int fontTitle: 18
    readonly property int fontHeadline: 22
    readonly property int fontHero: 28
    readonly property int fontBanner: 32

    // TextTheme sizes (used)
    readonly property int displayXL: 56
    readonly property int displayLarge: 48
    readonly property int displayMedium: 36
    readonly property int headlineXL: 26
    readonly property int headlineLarge: 24

    // Legacy aliases (used)
    readonly property int fontXS: 11
    readonly property int fontSM: 12
    readonly property int fontMD: 14
    readonly property int fontLG: 16
    readonly property int fontXL: 20

    // Letter spacing
    readonly property real letterSpacingHeadline: -0.5

    // =========================================================================
    // WINDOW SIZES
    // =========================================================================

    readonly property int minWindowWidth: 900
    readonly property int minWindowHeight: 620

    // =========================================================================
    // INTERACTION DURATIONS
    // =========================================================================

    readonly property int transitionDuration: _animEnabled ? 200 : 0
    readonly property int fadeTransitionDuration: _animEnabled ? 300 : 0
    readonly property int tooltipDelay: 500
    readonly property real pressScale: 0.97

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
