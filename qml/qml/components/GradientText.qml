import QtQuick
import QtQuick.Effects
import MakineAI 1.0

/**
 * GradientText.qml - Animated gradient text effect
 * Flutter equivalent: ShaderMask with LinearGradient
 *
 * Features:
 * - Smooth color-shifting gradient on text
 * - Configurable animation duration
 * - Multiple color stops support
 * - GPU-optimized with animationsEnabled flag
 */
Item {
    id: root

    property string text: ""
    property int pixelSize: 16
    property int fontWeight: Font.Normal
    property bool animationsEnabled: true
    property int animationDuration: 3000

    // Gradient colors
    property color color1: Theme.gold
    property color color2: Theme.olive
    property color color3: Theme.pastelBlue

    // Animation phase (0 to 1)
    property real animPhase: 0.0

    implicitWidth: textItem.implicitWidth
    implicitHeight: textItem.implicitHeight

    // Smooth sinusoidal transition
    property real smoothValue: 0.5 + 0.5 * Math.sin(animPhase * Math.PI * 2)

    // Animated colors based on phase
    property color currentColor1: Qt.rgba(
        color1.r * (1 - smoothValue) + color2.r * smoothValue,
        color1.g * (1 - smoothValue) + color2.g * smoothValue,
        color1.b * (1 - smoothValue) + color2.b * smoothValue,
        1
    )

    property color currentColor2: Qt.rgba(
        color2.r * (1 - smoothValue) + color3.r * smoothValue,
        color2.g * (1 - smoothValue) + color3.g * smoothValue,
        color2.b * (1 - smoothValue) + color3.b * smoothValue,
        1
    )

    property color currentColor3: Qt.rgba(
        color3.r * (1 - smoothValue) + color1.r * smoothValue,
        color3.g * (1 - smoothValue) + color1.g * smoothValue,
        color3.b * (1 - smoothValue) + color1.b * smoothValue,
        1
    )

    // Animation - GPU optimization: only run when visible
    NumberAnimation on animPhase {
        from: 0.0
        to: 1.0
        duration: root.animationDuration
        loops: Animation.Infinite
        running: root.visible && root.animationsEnabled
    }

    // Hidden text for mask
    Text {
        id: textItem
        text: root.text
        font.pixelSize: root.pixelSize
        font.weight: root.fontWeight
        visible: false
    }

    // Gradient rectangle (will be masked by text)
    Rectangle {
        id: gradientRect
        anchors.fill: textItem
        visible: false

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.currentColor1 }
            GradientStop { position: 0.5; color: root.currentColor2 }
            GradientStop { position: 1.0; color: root.currentColor3 }
        }
    }

    // Apply text as mask to gradient
    MultiEffect {
        anchors.fill: textItem
        source: gradientRect
        maskEnabled: true
        maskSource: textItem
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
