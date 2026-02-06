pragma Singleton
import QtQuick

/**
 * Dimensions.qml - Flutter constants.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/core/constants.dart
 */
QtObject {
    // =========================================================================
    // APP INFO - Flutter AppConstants birebir
    // =========================================================================

    readonly property string appName: "MakineAI"
    readonly property string appVersion: "0.1.0alpha"
    readonly property string appVersionFull: "v0.1.0alpha"
    readonly property string appBuildNumber: "1"
    readonly property string appDescription: "Türkçe Yama Launcher"
    readonly property string appCopyright: "© 2026 CEDRA Interactive"

    // =========================================================================
    // EXTERNAL LINKS - Flutter AppConstants birebir
    // =========================================================================

    readonly property string discordUrl: "https://discord.com/invite/QDezpy4QtV"
    readonly property string websiteUrl: "https://makineçeviri.com/"
    readonly property string donateUrl: "https://www.patreon.com/makineceviri"
    readonly property string logoUrl: "https://makineceviri.com/assets/logo.png"
    // =========================================================================
    // KENAR BOŞLUKLARI (Margin)
    // =========================================================================

    readonly property int marginXS: 4
    readonly property int marginSM: 8
    readonly property int marginMD: 16
    readonly property int marginLG: 24
    readonly property int marginXL: 32
    readonly property int marginXXL: 48  // Flutter: EdgeInsets.all(48)

    // =========================================================================
    // İÇ BOŞLUKLAR (Padding)
    // =========================================================================

    readonly property int paddingXS: 4
    readonly property int paddingSM: 8
    readonly property int paddingMD: 12
    readonly property int paddingLG: 16
    readonly property int paddingXL: 24
    readonly property int paddingXXL: 48  // Flutter: content padding

    // =========================================================================
    // KÖŞE YUVARLAKLIKLARI - Flutter AppConstants birebir
    // =========================================================================

    readonly property int radiusTiny: 3   // Flutter: borderRadiusTiny
    readonly property int radiusXS: 4     // Flutter: borderRadiusSmall
    readonly property int radiusSM: 6     // Flutter: borderRadiusMedium
    readonly property int radiusMD: 8     // Flutter: borderRadiusLarge
    readonly property int radiusLG: 12
    readonly property int radiusXL: 16
    readonly property int radiusFull: 9999

    // =========================================================================
    // NAVBAR
    // =========================================================================

    readonly property int navbarHeight: 72        // Flutter: height 72
    readonly property int navbarIconSize: 18      // Flutter: icon size 18
    readonly property int navbarIconSizeLogo: 38  // Flutter: logo size 38

    // =========================================================================
    // TITLEBAR
    // =========================================================================

    readonly property int titlebarHeight: 32      // Flutter: height 32
    readonly property int windowButtonSize: 28    // Flutter: ~28px buttons

    // =========================================================================
    // OYUN KARTI
    // =========================================================================

    readonly property int cardWidth: 140          // Flutter: width 140
    readonly property int cardHeight: 200         // Flutter: height 200
    readonly property int cardImageHeight: 140    // Flutter: image fills card
    readonly property int cardGap: 16             // Flutter: spacing 16
    readonly property int cardBorderRadius: 4     // Flutter: borderRadius 4

    // =========================================================================
    // ANİMASYON SÜRELERİ (ms)
    // =========================================================================

    readonly property int animFast: 150           // Flutter: 150ms
    readonly property int animNormal: 250         // Flutter: 250ms
    readonly property int animSlow: 400           // Flutter: 400ms
    readonly property int animGradient: 2000      // Flutter: 2000ms gradient loop

    // =========================================================================
    // BLUR MİKTARLARI
    // =========================================================================

    readonly property int blurLight: 10           // Flutter: sigmaX/Y 10
    readonly property int blurMedium: 20          // Flutter: sigmaX/Y 20
    readonly property int blurHeavy: 80           // Flutter: sigmaX/Y 80

    // =========================================================================
    // FONT BOYUTLARI (Typography from colors.h)
    // =========================================================================

    // Flutter TextTheme sizes
    readonly property int displayLarge: 48        // displayLarge
    readonly property int displayMedium: 36       // displayMedium
    readonly property int headlineLarge: 24       // headlineLarge
    readonly property int headlineMedium: 20      // headlineMedium
    readonly property int titleLarge: 16          // titleLarge
    readonly property int bodyLarge: 14           // bodyLarge
    readonly property int bodyMedium: 14          // bodyMedium
    readonly property int bodySmall: 12           // bodySmall
    readonly property int labelSmall: 11          // labelSmall

    // Eski isimler (uyumluluk için)
    readonly property int fontXS: 11
    readonly property int fontSM: 12
    readonly property int fontMD: 14
    readonly property int fontLG: 16
    readonly property int fontXL: 20
    readonly property int fontXXL: 24
    readonly property int fontDisplay: 48

    // Font weights (Qt font weights)
    readonly property int weightNormal: 50        // Font.Normal (400)
    readonly property int weightMedium: 57        // Font.Medium (500)
    readonly property int weightSemiBold: 63      // Font.DemiBold (600)
    readonly property int weightBold: 75          // Font.Bold (700)

    // Letter spacing (Flutter ile birebir)
    readonly property real letterSpacingDisplay: -1.5
    readonly property real letterSpacingHeadline: -0.5
    readonly property real letterSpacingBody: 0.0

    // =========================================================================
    // ICON SIZES
    // =========================================================================

    readonly property int iconXS: 14
    readonly property int iconSM: 16
    readonly property int iconMD: 20
    readonly property int iconLG: 24
    readonly property int iconXL: 32

    // =========================================================================
    // SPLASH SCREEN INTERVALS
    // =========================================================================

    readonly property int splashInterval1: 0
    readonly property int splashInterval2: 400
    readonly property int splashInterval3: 600
    readonly property int splashInterval4: 1200
    readonly property int splashInterval5: 2000
    readonly property int splashTotal: 2200

    // =========================================================================
    // WINDOW SIZES (Flutter AppConstants)
    // =========================================================================

    readonly property int minWindowWidth: 900
    readonly property int minWindowHeight: 600
    readonly property int defaultWindowWidth: 1280
    readonly property int defaultWindowHeight: 720

    // =========================================================================
    // ANIMATION DURATIONS (Flutter AppConstants)
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
    readonly property int inputBorderRadius: 12

    // =========================================================================
    // SCROLLBAR DIMENSIONS
    // =========================================================================

    readonly property int scrollbarWidth: 4
    readonly property int scrollbarRadius: 2
    readonly property int scrollbarMargin: 2

    // =========================================================================
    // PROGRESS BAR DIMENSIONS (Splash screen)
    // =========================================================================

    readonly property int progressBarWidth: 200
    readonly property int progressBarHeight: 4
    readonly property int progressBarRadius: 4
}
