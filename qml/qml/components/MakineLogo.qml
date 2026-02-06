import QtQuick
import MakineAI 1.0

/**
 * MakineLogo.qml - Animated logo with gradient glow and hover effects
 */
Item {
    id: root

    // =========================================================================
    // PUBLIC PROPERTIES
    // =========================================================================

    /// Logo size (default: 48)
    property int logoSize: 48

    /// Whether glow animation is active
    property bool isGlowing: false

    /// Animation value (0.0 - 1.0)
    /// When isGlowing is true, uses its own internal animation
    property real animationValue: 0.0

    /// Whether hover effect is enabled
    property bool hoverEnabled: true

    /// Hover state - readonly
    readonly property bool hovered: mouseArea.containsMouse

    /// Logo image path
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
    // INTERNAL ANIMATION
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

    // Effective animation value to use
    readonly property real _effectiveAnimValue: isGlowing ? _internalAnimValue : animationValue

    // =========================================================================
    // HOVER SCALE
    // =========================================================================

    scale: hoverEnabled && hovered ? 1.05 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    // =========================================================================
    // GLOW LAYER 1 - Primary colored glow
    // =========================================================================

    function getGlowColor(anim) {
        var gold = { r: 1.0, g: 0.84, b: 0.0 }
        var pink = { r: 1.0, g: 0.41, b: 0.71 }
        return Qt.rgba(
            gold.r + (pink.r - gold.r) * anim,
            gold.g + (pink.g - gold.g) * anim,
            gold.b + (pink.b - gold.b) * anim,
            0.4 + anim * 0.4
        )
    }

    Rectangle {
        id: primaryGlow
        visible: root.isGlowing
        anchors.centerIn: logoRect
        width: logoRect.width + 16 + _effectiveAnimValue * 24
        height: logoRect.height + 16 + _effectiveAnimValue * 24
        radius: logoRect.radius + 8
        color: getGlowColor(_effectiveAnimValue)
        z: -2

        Behavior on width { NumberAnimation { duration: 50 } }
        Behavior on height { NumberAnimation { duration: 50 } }
    }

    Rectangle {
        id: secondaryGlow
        visible: root.isGlowing
        anchors.centerIn: logoRect
        width: logoRect.width + 8 + _effectiveAnimValue * 16
        height: logoRect.height + 8 + _effectiveAnimValue * 16
        radius: logoRect.radius + 4
        color: Qt.rgba(1, 1, 1, 0.2 + _effectiveAnimValue * 0.3)
        z: -1

        Behavior on width { NumberAnimation { duration: 50 } }
        Behavior on height { NumberAnimation { duration: 50 } }
    }

    // =========================================================================
    // STATIC GLOW - gold alpha 0.25, blur 12
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
        radius: logoSize * 0.25
        clip: true

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#E8C547" }
            GradientStop { position: 0.15; color: "#DEA54B" }
            GradientStop { position: 0.30; color: "#E8A090" }
            GradientStop { position: 0.45; color: "#B8A0C8" }
            GradientStop { position: 0.55; color: "#90B8D0" }
            GradientStop { position: 0.70; color: "#80C8B8" }
            GradientStop { position: 0.85; color: "#90D090" }
            GradientStop { position: 1.0; color: "#E8C547" }
        }

        border.color: Theme.gold
        border.width: 2

        // =====================================================================
        // IMAGE LOADER
        // =====================================================================

        Image {
            id: logoImage
            anchors.fill: parent
            source: root.logoImage
            visible: status === Image.Ready
            fillMode: Image.PreserveAspectFit
        }

        // =====================================================================
        // CENTER DOT - Fallback when image is not available
        // =====================================================================

        Rectangle {
            id: centerDot
            visible: logoImage.status !== Image.Ready
            anchors.centerIn: parent
            width: logoSize * 0.35
            height: width
            radius: width * 0.5
            color: Qt.rgba(1, 1, 1, 0.9)
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
