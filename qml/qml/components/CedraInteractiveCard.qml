import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * CedraInteractiveCard.qml - Flutter _CedraInteractiveCard birebir port
 * Kaynak: archive/v0.0.8-flutter/UI/lib/screens/home_screen.dart
 *
 * Features:
 * - 3000ms animated gradient border
 * - Premium glow effects (gold, olive, pastel blue)
 * - CEDRA logo with animated border
 * - ShaderMask gradient text title
 * - Dark premium background gradient
 */
Rectangle {
    id: root

    property bool isDark: true
    property bool animationsEnabled: true  // GPU optimization

    implicitHeight: 104  // padding 24 * 2 + content 56
    radius: Dimensions.radiusStandard

    // Animation phase (0 to 1, cycles over 3000ms)
    property real animPhase: 0.0

    NumberAnimation on animPhase {
        from: 0.0
        to: 1.0
        duration: 3000
        loops: Animation.Infinite
        running: root.visible && root.animationsEnabled  // GPU optimization: stop when not visible
    }

    // Smooth sinusoidal transition - Flutter: smoothValue
    property real smoothValue: 1 - Math.abs(animPhase * 2 - 1)

    // Animated colors - Flutter: color1, color2, color3
    // gold (#DDC66A) <-> brown (#9B7649)
    property color color1: Qt.rgba(
        Theme.gold.r * (1 - smoothValue) + Theme.brown.r * smoothValue,
        Theme.gold.g * (1 - smoothValue) + Theme.brown.g * smoothValue,
        Theme.gold.b * (1 - smoothValue) + Theme.brown.b * smoothValue,
        1
    )
    property color color2: Qt.rgba(
        Theme.brown.r * (1 - smoothValue) + Theme.gold.r * smoothValue,
        Theme.brown.g * (1 - smoothValue) + Theme.gold.g * smoothValue,
        Theme.brown.b * (1 - smoothValue) + Theme.gold.b * smoothValue,
        1
    )
    // olive (#759764) <-> pastel blue (#A4C2C9)
    property color color3: Qt.rgba(
        Theme.olive.r * (1 - smoothValue) + Theme.pastelBlue.r * smoothValue,
        Theme.olive.g * (1 - smoothValue) + Theme.pastelBlue.g * smoothValue,
        Theme.olive.b * (1 - smoothValue) + Theme.pastelBlue.b * smoothValue,
        1
    )

    // Background gradient - Flutter: #1A1A2E -> #12121F -> #0A0A14
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: "#1A1A2E" }
        GradientStop { position: 0.5; color: "#12121F" }
        GradientStop { position: 1.0; color: "#0A0A14" }
    }

    // Animated border - Flutter: color1 with alpha 0.5 + smoothValue * 0.3
    border.color: Qt.rgba(color1.r, color1.g, color1.b, 0.5 + smoothValue * 0.3)
    border.width: 1.5

    // =========================================================================
    // PREMIUM GLOW EFFECT - Flutter BoxShadow birebir port
    // Flutter: blurRadius 20 + smoothValue*15, spreadRadius smoothValue*4
    // =========================================================================

    // Primary glow source (color1 - gold)
    // Flutter: alpha 0.25 + smoothValue*0.15, blur 20 + smoothValue*15, spread smoothValue*4
    Rectangle {
        id: glowSource
        anchors.fill: parent
        // Simulate spreadRadius with margin: blur + spread
        anchors.margins: -20 - (smoothValue * 4)
        radius: parent.radius + 12
        color: color1
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: glowSource
        source: glowSource
        blurEnabled: true
        blur: (20 + smoothValue * 15) / 64  // normalize to 0-1 range
        blurMax: 48
        opacity: 0.25 + smoothValue * 0.15  // Flutter exact values
        z: -1
        visible: root.visible && root.animationsEnabled
    }

    // Secondary glow source (color3 - olive/blue)
    // Flutter: alpha 0.1 + smoothValue*0.08, blur 30 + smoothValue*10, spread smoothValue*2
    Rectangle {
        id: secondaryGlowSource
        anchors.fill: parent
        anchors.margins: -30 - (smoothValue * 2)
        radius: parent.radius + 16
        color: color3
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: secondaryGlowSource
        source: secondaryGlowSource
        blurEnabled: true
        blur: (30 + smoothValue * 10) / 64
        blurMax: 56
        opacity: 0.1 + smoothValue * 0.08  // Flutter exact values
        z: -2
        visible: root.visible && root.animationsEnabled
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // CEDRA Logo container with premium glow - Flutter: 56x56
        Rectangle {
            id: logoContainer
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: Dimensions.radiusStandard
            color: "#1A1A2E"

            // Animated border - Flutter: color1 alpha 0.6 + smoothValue * 0.3, width 2
            border.color: Qt.rgba(root.color1.r, root.color1.g, root.color1.b, 0.6 + root.smoothValue * 0.3)
            border.width: 2

            // Logo glow
            Rectangle {
                anchors.fill: parent
                anchors.margins: -6 - root.smoothValue * 4
                radius: parent.radius + 3
                color: "transparent"
                border.width: 2 + root.smoothValue * 1.5
                border.color: Qt.rgba(root.color1.r, root.color1.g, root.color1.b, 0.3 + root.smoothValue * 0.2)
                z: -1
            }

            // CEDRA text placeholder (since we don't have the logo image)
            Text {
                anchors.centerIn: parent
                text: "C"
                font.pixelSize: 28
                font.weight: Font.Bold
                color: root.color1
            }
        }

        // Content column
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            // Animated gradient title - Flutter: ShaderMask with color1, color2, color1
            GradientText {
                id: titleText
                text: "CEDRA Interactive"
                pixelSize: 22
                fontWeight: Font.Bold
                animationsEnabled: root.animationsEnabled
                animationDuration: 3000
                color1: root.color1
                color2: root.color2
                color3: root.color3
            }

            // Description - Flutter: fontSize 14, height 1.5
            Text {
                Layout.fillWidth: true
                text: "Turk oyun gelistirme ve ceviri toplulugu."
                font.pixelSize: 14
                color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
                lineHeight: 1.5
                wrapMode: Text.WordWrap
            }
        }
    }
}
