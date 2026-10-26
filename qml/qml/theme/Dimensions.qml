// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

pragma Singleton
import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Dimensions.qml - Design system constants for Makine-Launcher
 */
QtObject {
    // =========================================================================
    // APP INFO
    // =========================================================================

    readonly property string appName: "Makine Launcher"
    readonly property string appVersion: Qt.application.version
    readonly property string appVersionFull: "v" + Qt.application.version

    // =========================================================================
    // EXTERNAL LINKS
    // =========================================================================

    readonly property string discordUrl: "https://discord.com/invite/makine-ceviri-885595211806441482"
    // Patch install videos
    readonly property string youtubeUrl: "https://www.youtube.com/@Makine-Ceviri"
    readonly property string instagramUrl: "https://www.instagram.com/makineceviri/"
    readonly property string xUrl: "https://x.com/Makineceviri"
    readonly property string linkedinUrl: "https://www.linkedin.com/company/makineceviri/"
    readonly property string githubUrl: "https://github.com/MakineCeviri"
    readonly property string websiteUrl: "https://makineceviri.org/"
    readonly property string donatePageUrl: "https://makineceviri.org/destekci-ol"

    // GitHub
    readonly property string githubOwner: "MakineCeviri"
    readonly property string githubRepo: "Makine-Launcher"
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
    // Reading width of a page's content on a wide window (the hero art stays
    // full width): past this, lines of text get too long to follow.
    readonly property int contentMaxWidth: 1180
    readonly property int marginXXL: 48

    // =========================================================================
    // PADDING
    // =========================================================================

    readonly property int paddingXS: 4
    readonly property int paddingSM: 8
    readonly property int paddingMD: 12
    readonly property int paddingLG: 16
    readonly property int paddingXL: 24
    readonly property int paddingXXL: 32

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

    readonly property int radiusXS: 4
    readonly property int radiusSM: 6
    readonly property int radiusMD: 8
    readonly property int radiusLG: 12
    readonly property int radiusSection: 16
    readonly property int radiusFull: 9999

    // Small controls (buttons in rows, fields, menus)
    readonly property int radiusStandard: radiusMD
    readonly property int radiusControl: radiusMD
    // Cards, grouped list sections
    readonly property int radiusCard: 14
    // Sheets and alerts
    readonly property int radiusSheet: 18
    // Large art (hero, featured)
    readonly property int radiusHero: 20

    // =========================================================================
    // NAVBAR
    // =========================================================================

    readonly property int navbarHeight: 56
    readonly property int navbarIconSizeLogo: 28

    // App sidebar (Ana Sayfa, Tüm Oyunlar, …) and a page's horizontal margin
    readonly property int appSidebarWidth: 224
    readonly property int pageMargin: 32
    // Large title row at the top of a page
    readonly property int pageHeaderTop: 28

    // =========================================================================
    // TITLEBAR
    // =========================================================================

    readonly property int titlebarHeight: 40

    // =========================================================================
    // GAME CARD
    // =========================================================================

    readonly property int cardWidth: 130
    readonly property int cardHeight: 185
    readonly property int cardGap: 16
    readonly property int cardBorderRadius: radiusSection

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
    readonly property int animMedium: _animEnabled ? 200 : 0
    readonly property int animNormal: _animEnabled ? 250 : 0
    readonly property int animSlow: _animEnabled ? 400 : 0
    readonly property int animVerySlow: _animEnabled ? 800 : 0
    readonly property int animPulse: _animEnabled ? 800 : 0
    readonly property int animLoadingCycle: _animEnabled ? 1500 : 0
    readonly property int animGradient: _animEnabled ? 2000 : 0

    // Page transition durations
    readonly property int animPageOut: _animEnabled ? 160 : 0
    readonly property int animPageIn: _animEnabled ? 240 : 0

    // =========================================================================
    // EASING VOCABULARY — one motion language for the whole app.
    // Prefer these over inline Easing.* so timing/feel stays consistent.
    //   easeStandard  : default — entrances, hovers, fades (smooth decel)
    //   easeMove      : symmetric position/size changes (in-out)
    //   easeEntrance  : strong, refined arrival decel
    //   easeExit      : quick, decisive disappearance
    //   easeEmphasized: playful spring/overshoot — accents only, sparingly
    // =========================================================================

    readonly property int easeStandard:  Easing.OutCubic
    readonly property int easeMove:       Easing.InOutQuad
    readonly property int easeEntrance:   Easing.OutQuint
    readonly property int easeExit:       Easing.InCubic
    readonly property int easeEmphasized: Easing.OutBack

    // =========================================================================
    // FONT SIZES
    // =========================================================================

    // Apple text styles. Inter Display stands in for SF Pro and is the app's
    // font (main.cpp); its tighter cut reads closer to SF than Inter's text
    // cut, which keeps only the caption sizes (fontText), where Display
    // spacing gets cramped.
    readonly property string fontDisplay: "Inter Display"
    readonly property string fontText: "Inter"
    readonly property int largeTitle: 30
    readonly property int title1: 26
    readonly property int title2: 21
    readonly property int title3: 18
    readonly property int headline: 15
    readonly property int body: 14
    readonly property int callout: 13
    readonly property int footnote: 12
    readonly property int caption1: 11
    readonly property int caption2: 10
    readonly property real trackingTitle: -0.4
    readonly property real trackingHeadline: -0.2

    readonly property int fontMicro: 8
    readonly property int fontMini: 9
    readonly property int fontCaption: 10
    readonly property int fontBody: 13
    readonly property int fontSubtitle: 15
    readonly property int fontTitle: 18
    readonly property int fontHeadline: 22
    readonly property int fontHero: 28
    readonly property int fontBanner: 32

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

    // The window keeps these proportions (main.cpp's WM_SIZING lock uses
    // 900:620); Main.qml lowers the minimum on a screen that cannot hold it.
    readonly property int minWindowWidth: 1000
    readonly property int minWindowHeight: 689
    readonly property int defaultWindowWidth: 1240
    // Pages are laid out for this width; a narrower window scales them down
    readonly property int designWidth: 1180
    // That scale, set by Main.qml (window width / designWidth, 0.72…1)
    property real uiZoom: 1.0

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

    readonly property int toggleWidth: 42
    readonly property int toggleHeight: 25
    readonly property int toggleRadius: 13
    readonly property int toggleKnobSize: 21
    readonly property int toggleKnobRadius: 11

    // =========================================================================
    // DIALOG DIMENSIONS
    // =========================================================================

    readonly property int dialogRadius: radiusSheet
    readonly property int badgeRadius: radiusFull

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
