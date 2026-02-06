import QtQuick
import MakineAI 1.0

/**
 * MakineLogo.qml - Flutter makine_logo.dart birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/widgets/makine_logo.dart
 *
 * Flutter'daki gibi iki mod:
 * 1. Normal (isGlowing: false) - Statik gold glow
 * 2. Glowing (isGlowing: true) - Animasyonlu gold -> pink glow
 *
 * Ozellikler:
 * - SweepGradient yaklasimi (Gold, Salmon, Blue, Green)
 * - Animasyonlu glow efekti (isGlowing: true)
 * - Hover scale animasyonu
 * - White center dot
 * - Image asset destegi (fallback gradient)
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES - Flutter MakineLogo birebir
    // =========================================================================

    /// Logo boyutu - Flutter: size = 48 default
    property int logoSize: 48

    /// Glow animasyonu aktif mi - Flutter: isGlowing
    property bool isGlowing: false

    /// Animasyon degeri (0.0 - 1.0) - Flutter: animationValue
    /// isGlowing true iken kendi iç animasyonunu kullanir
    property real animationValue: 0.0

    /// Hover efekti aktif mi
    property bool hoverEnabled: true

    /// Hover durumu - readonly
    readonly property bool hovered: mouseArea.containsMouse

    /// Logo image path - Flutter: 'assets/images/logo.png'
    property string logoImage: ""

    // =========================================================================
    // SIGNALS
    // =========================================================================

    signal clicked()

    // =========================================================================
    // SIZE
    // =========================================================================

    width: logoSize
    height: logoSize

    // =========================================================================
    // INTERNAL ANIMATION - Flutter isGlowing animasyonu
    // =========================================================================

    property real _internalAnimValue: 0.0

    NumberAnimation on _internalAnimValue {
        id: glowAnimation
        from: 0.0
        to: 1.0
        duration: 2000
        loops: Animation.Infinite
        running: root.isGlowing
    }

    // Kullanilacak animasyon degeri
    readonly property real _effectiveAnimValue: isGlowing ? _internalAnimValue : animationValue

    // =========================================================================
    // HOVER SCALE - Flutter: 1.0 -> 1.05
    // =========================================================================

    scale: hoverEnabled && hovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    // =========================================================================
    // GLOW LAYER 1 - Flutter: Primary colored glow
    // =========================================================================

    // Flutter'daki glow rengi: gold -> pink interpolasyon
    function getGlowColor(anim) {
        // Color.lerp(gold, pink, anim)
        var gold = { r: 1.0, g: 0.84, b: 0.0 }       // #FFD700
        var pink = { r: 1.0, g: 0.41, b: 0.71 }      // #FF69B4
        return Qt.rgba(
            gold.r + (pink.r - gold.r) * anim,
            gold.g + (pink.g - gold.g) * anim,
            gold.b + (pink.b - gold.b) * anim,
            0.4 + anim * 0.4  // Flutter: 0.4 + animationValue * 0.4
        )
    }

    // Primary glow - Flutter: blurRadius: 16 + anim * 12, spreadRadius: anim * 4
    Rectangle {
        id: primaryGlow
        visible: root.isGlowing
        anchors.centerIn: logoRect
        width: logoRect.width + 16 + _effectiveAnimValue * 24  // blur approximation
        height: logoRect.height + 16 + _effectiveAnimValue * 24
        radius: logoRect.radius + 8
        color: getGlowColor(_effectiveAnimValue)
        z: -2

        Behavior on width { NumberAnimation { duration: 50 } }
        Behavior on height { NumberAnimation { duration: 50 } }
    }

    // Secondary glow - Flutter: white glow
    Rectangle {
        id: secondaryGlow
        visible: root.isGlowing
        anchors.centerIn: logoRect
        width: logoRect.width + 8 + _effectiveAnimValue * 16
        height: logoRect.height + 8 + _effectiveAnimValue * 16
        radius: logoRect.radius + 4
        color: Qt.rgba(1, 1, 1, 0.2 + _effectiveAnimValue * 0.3)  // Flutter
        z: -1

        Behavior on width { NumberAnimation { duration: 50 } }
        Behavior on height { NumberAnimation { duration: 50 } }
    }

    // =========================================================================
    // STATIC GLOW - Flutter MakineLogoSimple: gold alpha 0.25, blur 12
    // =========================================================================

    Rectangle {
        visible: !root.isGlowing
        anchors.centerIn: logoRect
        width: logoRect.width + 12
        height: logoRect.height + 12
        radius: logoRect.radius + 6
        color: Qt.rgba(Theme.gold.r, Theme.gold.g, Theme.gold.b, 0.25)
        z: -1
    }

    // =========================================================================
    // MAIN LOGO CONTAINER
    // =========================================================================

    Rectangle {
        id: logoRect
        anchors.fill: parent
        radius: logoSize * 0.25  // Flutter: size * 0.25
        clip: true

        // Flutter SweepGradient yaklaşımı - Linear gradient ile
        // Colors: Gold, Orange-yellow, Salmon/Pink, Light purple, Light blue, Teal, Light green, Yellow-green, Gold
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#E8C547" }   // Gold/Yellow
            GradientStop { position: 0.15; color: "#DEA54B" }  // Orange-yellow
            GradientStop { position: 0.30; color: "#E8A090" }  // Salmon/Pink
            GradientStop { position: 0.45; color: "#B8A0C8" }  // Light purple
            GradientStop { position: 0.55; color: "#90B8D0" }  // Light blue
            GradientStop { position: 0.70; color: "#80C8B8" }  // Teal
            GradientStop { position: 0.85; color: "#90D090" }  // Light green
            GradientStop { position: 1.0; color: "#E8C547" }   // Back to gold
        }

        // Gold border - Flutter'da yok ama visual improvement
        border.color: Theme.gold
        border.width: 2

        // =====================================================================
        // IMAGE LOADER - Flutter: Image.asset with errorBuilder
        // =====================================================================

        Image {
            id: logoImage
            anchors.fill: parent
            source: root.logoImage
            visible: status === Image.Ready
            fillMode: Image.PreserveAspectFit
        }

        // =====================================================================
        // CENTER DOT - Flutter fallback: white, size * 0.35, rounded
        // =====================================================================

        Rectangle {
            id: centerDot
            visible: logoImage.status !== Image.Ready
            anchors.centerIn: parent
            width: logoSize * 0.35
            height: width
            radius: width * 0.5  // Flutter: size * 0.175 (yarıcap)
            color: Qt.rgba(1, 1, 1, 0.9)  // Flutter: Colors.white.withAlpha(0.9)
        }
    }

    // =========================================================================
    // MOUSE AREA
    // =========================================================================

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        cursorShape: root.hoverEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
