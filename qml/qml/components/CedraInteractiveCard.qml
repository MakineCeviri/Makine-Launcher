import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * CedraInteractiveCard.qml - Premium MakineAI branding card with animated glow effects
 */
Rectangle {
    id: root

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

    // Smooth sinusoidal transition (0→1→0)
    readonly property real smoothValue: 1.0 - Math.abs(animPhase * 2.0 - 1.0)

    // Animated color pairs using Theme.lerpColor (same as CedraCard)
    readonly property color color1: Theme.lerpColor(Theme.gold, Theme.brown, smoothValue)
    readonly property color color2: Theme.lerpColor(Theme.brown, Theme.gold, smoothValue)
    readonly property color color3: Theme.lerpColor(Theme.olive, Theme.pastelBlue, smoothValue)

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
        layer.enabled: root.animationsEnabled
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
        layer.enabled: root.animationsEnabled
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
        anchors.margins: Dimensions.marginLG
        spacing: Dimensions.spacingXXL

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
                font.pixelSize: Dimensions.fontHero
                font.weight: Font.Bold
                color: root.color1
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingMD

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
                text: qsTr("Türk oyun geliştirme ve çeviri topluluğu.")
                font.pixelSize: Dimensions.fontMD
                color: Theme.textSecondary
                lineHeight: 1.5
                wrapMode: Text.WordWrap
            }
        }
    }
}
