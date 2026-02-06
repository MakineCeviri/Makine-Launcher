import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * GlassCard.qml - Glassmorphism card with blur effect
 * Uses Qt6 MultiEffect for real background blur
 *
 * Features:
 * - Real background blur (sigma 10-20)
 * - Glass morphism overlay
 * - Animated border glow on hover
 * - Configurable opacity and radius
 */
Item {
    id: root

    // Configurable properties
    property real glassOpacity: 0.03      // Base: white 3%
    property real borderOpacity: 0.08     // Base: white 8%
    property int blurRadius: 10           // Blur sigma
    property bool hoverEnabled: false
    property bool hovered: mouseArea.containsMouse
    property bool enableBlur: true        // Can disable for performance
    property Item blurSource: null        // Optional: source for blur
    property color glowColor: Theme.gold  // Hover glow color
    property bool enableGlow: true        // Enable hover glow

    // Size hint
    implicitWidth: 200
    implicitHeight: 100

    // Default content alias
    default property alias content: contentItem.data

    // =========================================================================
    // BLUR BACKGROUND (Real glassmorphism)
    // =========================================================================

    // Capture background for blur effect
    ShaderEffectSource {
        id: blurSourceItem
        anchors.fill: parent
        sourceItem: root.blurSource
        sourceRect: Qt.rect(root.x, root.y, root.width, root.height)
        visible: false
        live: true
        enabled: root.enableBlur && root.blurSource !== null
    }

    // Blurred background layer
    MultiEffect {
        id: blurEffect
        anchors.fill: parent
        source: blurSourceItem
        blurEnabled: root.enableBlur && blurSourceItem.enabled
        blur: Math.min(1.0, root.blurRadius / 64.0)
        blurMax: 32
        visible: root.enableBlur && root.blurSource !== null

        // Mask to card shape
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: cardMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    // Mask shape for blur
    Rectangle {
        id: cardMask
        anchors.fill: parent
        radius: Dimensions.radiusXS
        visible: false
        layer.enabled: true
    }

    // =========================================================================
    // HOVER GLOW LAYERS
    // =========================================================================

    // Glow source (invisible)
    Rectangle {
        id: hoverGlowSource
        x: -16
        y: -12
        width: parent.width + 32
        height: parent.height + 28
        radius: Dimensions.radiusXS + 12
        color: root.glowColor
        visible: false
        layer.enabled: true
    }

    // Real blur glow on hover - MORE VISIBLE
    MultiEffect {
        x: hoverGlowSource.x
        y: hoverGlowSource.y + 4
        width: hoverGlowSource.width
        height: hoverGlowSource.height
        source: hoverGlowSource
        blurEnabled: true
        blur: 0.8
        blurMax: 48
        opacity: (root.enableGlow && root.hovered) ? 0.5 : 0
        z: -1

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    // Inner glow border on hover
    Rectangle {
        visible: root.enableGlow && root.hovered
        anchors.fill: parent
        anchors.margins: -2
        radius: Dimensions.radiusXS + 2
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b, 0.5)
        border.width: 2
        z: -0.5

        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }
    }

    // Outer glow ring 1 on hover
    Rectangle {
        visible: root.enableGlow && root.hovered
        anchors.fill: parent
        anchors.margins: -8
        radius: Dimensions.radiusXS + 8
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b, 0.35)
        border.width: 3
        z: -2

        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }
    }

    // Outer glow ring 2 (larger, softer)
    Rectangle {
        visible: root.enableGlow && root.hovered
        anchors.fill: parent
        anchors.margins: -14
        radius: Dimensions.radiusXS + 14
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b, 0.2)
        border.width: 3
        z: -3

        Behavior on border.color {
            ColorAnimation { duration: 200 }
        }
    }

    // =========================================================================
    // GLASS PANEL
    // =========================================================================

    Rectangle {
        id: glassPanel
        anchors.fill: parent
        radius: Dimensions.radiusXS  // 4

        // Glass color with hover effect
        color: Qt.rgba(1, 1, 1, root.hoverEnabled && root.hovered ? root.glassOpacity + 0.03 : root.glassOpacity)

        // Animated border
        border.color: Qt.rgba(1, 1, 1, root.hoverEnabled && root.hovered ? root.borderOpacity + 0.06 : root.borderOpacity)
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        // Inner highlight gradient (subtle top shine)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: parent.radius - 1
            opacity: root.hovered ? 0.08 : 0.02

            gradient: Gradient {
                GradientStop { position: 0.0; color: "white" }
                GradientStop { position: 0.3; color: "transparent" }
            }

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }
        }
    }

    // =========================================================================
    // CONTENT CONTAINER
    // =========================================================================

    Item {
        id: contentItem
        anchors.fill: parent
        z: 1
    }

    // =========================================================================
    // MOUSE AREA
    // =========================================================================

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.hoverEnabled
        acceptedButtons: Qt.NoButton
    }
}
