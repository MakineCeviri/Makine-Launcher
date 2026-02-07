import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * CedraInteractiveCard.qml - Premium MakineAI branding card with animated glow effects
 */
Rectangle {
    id: root

    property bool isDark: true
    property bool animationsEnabled: true

    implicitHeight: 104
    radius: Dimensions.radiusStandard

    // Animation phase (0 to 1, cycles over 3000ms)
    property real animPhase: 0.0

    NumberAnimation on animPhase {
        from: 0.0
        to: 1.0
        duration: 3000
        loops: Animation.Infinite
        running: root.visible && root.animationsEnabled
    }

    // Smooth sinusoidal transition
    property real smoothValue: 1 - Math.abs(animPhase * 2 - 1)

    // gold <-> brown
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
    // olive <-> pastel blue
    property color color3: Qt.rgba(
        Theme.olive.r * (1 - smoothValue) + Theme.pastelBlue.r * smoothValue,
        Theme.olive.g * (1 - smoothValue) + Theme.pastelBlue.g * smoothValue,
        Theme.olive.b * (1 - smoothValue) + Theme.pastelBlue.b * smoothValue,
        1
    )

    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: Theme.cardDarkStart }
        GradientStop { position: 0.5; color: Theme.cardDarkMid }
        GradientStop { position: 1.0; color: Theme.cardDarkEnd }
    }

    border.color: Theme.withAlpha(color1, 0.5 + smoothValue * 0.3)
    border.width: 1.5

    Rectangle {
        id: glowSource
        anchors.fill: parent
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
        blur: (20 + smoothValue * 15) / 64
        blurMax: 48
        opacity: 0.25 + smoothValue * 0.15
        z: -1
        visible: root.visible && root.animationsEnabled
    }

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
        opacity: 0.1 + smoothValue * 0.08
        z: -2
        visible: root.visible && root.animationsEnabled
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        Rectangle {
            id: logoContainer
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: Dimensions.radiusStandard
            color: Theme.cardDarkStart

            border.color: Theme.withAlpha(root.color1, 0.6 + root.smoothValue * 0.3)
            border.width: 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: -6 - root.smoothValue * 4
                radius: parent.radius + 3
                color: "transparent"
                border.width: 2 + root.smoothValue * 1.5
                border.color: Theme.withAlpha(root.color1, 0.3 + root.smoothValue * 0.2)
                z: -1
            }

            Text {
                anchors.centerIn: parent
                text: "C"
                font.pixelSize: 28
                font.weight: Font.Bold
                color: root.color1
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            GradientText {
                id: titleText
                text: "MakineAI"
                pixelSize: 22
                fontWeight: Font.Bold
                animationsEnabled: root.animationsEnabled
                animationDuration: 3000
                color1: root.color1
                color2: root.color2
                color3: root.color3
            }

            Text {
                Layout.fillWidth: true
                text: "Türk oyun geliştirme ve çeviri topluluğu."
                font.pixelSize: 14
                color: isDark ? Theme.textSecondary : Theme.lightTextSecondary
                lineHeight: 1.5
                wrapMode: Text.WordWrap
            }
        }
    }
}
