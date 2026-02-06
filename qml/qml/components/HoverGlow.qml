import QtQuick
import MakineAI 1.0

/**
 * HoverGlow.qml - Reusable hover glow effect layer
 *
 * Provides multi-layer glow effects on hover with smooth transitions.
 *
 * Features:
 * - Dual-layer outer glow (primary + secondary colors)
 * - Inner border glow
 * - Scale transform on hover/press
 * - Gradient highlight overlay
 * - GPU-optimized with conditional visibility
 *
 * Usage:
 *   Rectangle {
 *       id: myCard
 *       HoverGlow {
 *           anchors.fill: parent
 *           targetItem: parent
 *           glowColor: Theme.gold
 *       }
 *   }
 */
Item {
    id: root

    // Target item to apply hover effects to
    property Item targetItem: parent
    property bool hovered: false
    property bool pressed: false

    // Customizable properties
    property color glowColor: Theme.gold
    property color secondaryGlowColor: Theme.olive
    property real hoverScale: 1.03
    property real pressScale: 1.01
    property int transitionDuration: 200
    property real glowRadius: 8
    property real glowOpacity: 0.4
    property bool enableGlow: true
    property bool enableScale: true
    property bool enableBorderGlow: true

    // Read-only state
    readonly property real currentScale: pressed ? pressScale : (hovered ? hoverScale : 1.0)
    readonly property real currentGlow: hovered ? 1.0 : 0.0

    // Mouse area for hover detection (if not already handled externally)
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton  // Don't capture clicks, just hover

        onContainsMouseChanged: {
            root.hovered = containsMouse
        }
    }

    // Apply scale to target
    Binding {
        target: targetItem
        property: "scale"
        value: root.enableScale ? root.currentScale : 1.0
        when: root.enableScale
    }

    // Smooth scale behavior
    Behavior on currentScale {
        NumberAnimation {
            duration: root.transitionDuration
            easing.type: Easing.OutCubic
        }
    }

    // Outer glow layer 1 - Primary color
    Rectangle {
        visible: root.enableGlow && root.currentGlow > 0.01
        anchors.fill: parent
        anchors.margins: -root.glowRadius * root.currentGlow
        radius: (targetItem.radius || 0) + root.glowRadius
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b,
                              root.glowOpacity * root.currentGlow)
        border.width: 3 * root.currentGlow
        z: -1

        Behavior on anchors.margins {
            NumberAnimation { duration: root.transitionDuration; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: root.transitionDuration }
        }
        Behavior on border.width {
            NumberAnimation { duration: root.transitionDuration }
        }
    }

    // Outer glow layer 2 - Secondary color (larger, softer)
    Rectangle {
        visible: root.enableGlow && root.currentGlow > 0.01
        anchors.fill: parent
        anchors.margins: -(root.glowRadius + 6) * root.currentGlow
        radius: (targetItem.radius || 0) + root.glowRadius + 6
        color: "transparent"
        border.color: Qt.rgba(root.secondaryGlowColor.r, root.secondaryGlowColor.g,
                              root.secondaryGlowColor.b,
                              root.glowOpacity * 0.5 * root.currentGlow)
        border.width: 4 * root.currentGlow
        z: -2

        Behavior on anchors.margins {
            NumberAnimation { duration: root.transitionDuration; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: root.transitionDuration }
        }
    }

    // Inner border glow
    Rectangle {
        visible: root.enableBorderGlow && root.currentGlow > 0.01
        anchors.fill: parent
        radius: targetItem.radius || 0
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b,
                              0.5 * root.currentGlow)
        border.width: 2 * root.currentGlow
        z: 1

        Behavior on border.color {
            ColorAnimation { duration: root.transitionDuration }
        }
        Behavior on border.width {
            NumberAnimation { duration: root.transitionDuration }
        }
    }

    // Highlight overlay (subtle gradient)
    Rectangle {
        visible: root.currentGlow > 0.01
        anchors.fill: parent
        radius: targetItem.radius || 0
        opacity: 0.08 * root.currentGlow
        z: 2

        gradient: Gradient {
            GradientStop { position: 0.0; color: root.glowColor }
            GradientStop { position: 1.0; color: "transparent" }
        }

        Behavior on opacity {
            NumberAnimation { duration: root.transitionDuration }
        }
    }
}
