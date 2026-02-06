import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * GlassPanel.qml - Premium glassmorphism panel
 *
 * Features:
 * - Real blur effect on background content
 * - Frosted glass appearance
 * - Animated glow border
 * - Configurable blur intensity
 */
Item {
    id: root

    property real blurRadius: 20
    property real glassOpacity: 0.7
    property color tintColor: Theme.surface
    property color borderColor: Qt.rgba(1, 1, 1, 0.1)
    property real borderWidth: 1
    property real radius: 0
    property bool animationsEnabled: true

    // For capturing background content
    property Item backgroundSource: null

    // Glass background
    Rectangle {
        id: glassBackground
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        clip: true

        // Blur effect on captured background
        ShaderEffectSource {
            id: effectSource
            anchors.fill: parent
            sourceItem: root.backgroundSource
            sourceRect: Qt.rect(
                root.x,
                root.y,
                root.width,
                root.height
            )
            visible: false
        }

        // Apply blur
        MultiEffect {
            id: blurEffect
            anchors.fill: parent
            source: effectSource
            blurEnabled: root.backgroundSource !== null
            blur: Math.min(root.blurRadius / 64.0, 1.0)
            blurMax: 64
            visible: root.backgroundSource !== null
        }

        // Frosted glass overlay
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: Qt.rgba(
                root.tintColor.r,
                root.tintColor.g,
                root.tintColor.b,
                root.glassOpacity
            )
        }

        // Subtle gradient for depth
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.05) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.05) }
            }
        }

        // Border
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "transparent"
            border.color: root.borderColor
            border.width: root.borderWidth
        }
    }
}
