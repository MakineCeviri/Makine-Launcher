import QtQuick
import MakineAI 1.0

/**
 * CedraCard.qml - CEDRA Interactive branding card with animated gradient
 */
Item {
    id: root

    implicitWidth: 500
    implicitHeight: 120

    // GPU optimization - set from parent
    property bool animationsEnabled: true

    // Animation value (0-1) cycles every 3 seconds
    property real animValue: 0.0

    // Smooth sinusoidal transition (0→1→0)
    readonly property real smoothValue: 1.0 - Math.abs(animValue * 2.0 - 1.0)

    readonly property color color1: Theme.lerpColor(Theme.gold, Theme.brown, smoothValue)
    readonly property color color2: Theme.lerpColor(Theme.brown, Theme.gold, smoothValue)
    readonly property color color3: Theme.lerpColor(Theme.olive, Theme.pastelBlue, smoothValue)

    NumberAnimation on animValue {
        from: 0.0
        to: 1.0
        duration: 3000
        loops: Animation.Infinite
        running: root.animationsEnabled
    }

    Rectangle {
        anchors.fill: cardRect
        anchors.margins: -(5 + smoothValue * 4)
        radius: cardRect.radius + 4
        color: Theme.withAlpha(color1, 0.25 + smoothValue * 0.15)
    }

    Rectangle {
        anchors.fill: cardRect
        anchors.margins: -2
        radius: cardRect.radius + 2
        color: Theme.withAlpha(color3, 0.1 + smoothValue * 0.08)
    }

    Rectangle {
        id: cardRect
        anchors.fill: parent
        anchors.margins: 8
        radius: Dimensions.radiusXS

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.cardDarkerStart }
            GradientStop { position: 0.5; color: Theme.cardDarkerMid }
            GradientStop { position: 1.0; color: Theme.splashBackground }
        }

        border.color: Theme.withAlpha(color1, 0.5 + smoothValue * 0.3)
        border.width: 1.5

        Row {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 20

            Item {
                width: 56
                height: 56

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: Dimensions.radiusStandard
                    color: Theme.withAlpha(color1, 0.4 + smoothValue * 0.25)
                }

                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusXS
                    color: Theme.splashBackground
                    border.color: Theme.withAlpha(color1, 0.6 + smoothValue * 0.3)
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "C"
                        font.pixelSize: 28
                        font.weight: Font.Bold
                        color: color1
                    }
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    text: "CEDRA Interactive"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: color1
                }

                Text {
                    text: "Türk oyun geliştirme ve çeviri topluluğu."
                    font.pixelSize: 14
                    color: Theme.textSecondary
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally(Dimensions.cedraDeveloperUrl)
    }
}
