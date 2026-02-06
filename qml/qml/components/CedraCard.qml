import QtQuick
import MakineAI 1.0

/**
 * CedraCard.qml - Native Qt CedraCard birebir port
 * Kaynak: ui/src/widgets/cedracard.cpp
 *
 * Features:
 * - Animated gradient glow
 * - Logo with border animation
 * - Gradient text "CEDRA Interactive"
 */
Item {
    id: root

    implicitWidth: 500
    implicitHeight: 120

    // GPU optimization - set from parent
    property bool animationsEnabled: true

    // Animation value (0-1) cycles every 3 seconds
    property real animValue: 0.0

    // Flutter: smoothValue = (1 - ((animValue * 2 - 1).abs())).clamp(0, 1)
    property real smoothValue: 1.0 - Math.abs(animValue * 2.0 - 1.0)

    property color color1: Theme.lerpColor(Theme.gold, Theme.brown, smoothValue)
    property color color2: Theme.lerpColor(Theme.brown, Theme.gold, smoothValue)
    property color color3: Theme.lerpColor(Theme.olive, Theme.pastelBlue, smoothValue)

    // Animation - Native Qt: 3000ms loop
    // GPU optimization: controlled by animationsEnabled
    NumberAnimation on animValue {
        from: 0.0
        to: 1.0
        duration: 3000
        loops: Animation.Infinite
        running: root.animationsEnabled
    }

    // Outer glow
    Rectangle {
        anchors.fill: cardRect
        anchors.margins: -(5 + smoothValue * 4)
        radius: cardRect.radius + 4
        color: Theme.withAlpha(color1, 0.25 + smoothValue * 0.15)
    }

    // Second glow
    Rectangle {
        anchors.fill: cardRect
        anchors.margins: -2
        radius: cardRect.radius + 2
        color: Theme.withAlpha(color3, 0.1 + smoothValue * 0.08)
    }

    // Main card
    Rectangle {
        id: cardRect
        anchors.fill: parent
        anchors.margins: 8
        radius: Dimensions.radiusXS

        // Gradient background - Native Qt: BG1, BG2, BG3
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#0D0D12" }
            GradientStop { position: 0.5; color: "#10101A" }
            GradientStop { position: 1.0; color: "#0A0A0F" }
        }

        border.color: Theme.withAlpha(color1, 0.5 + smoothValue * 0.3)
        border.width: 1.5

        Row {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 20

            // Logo container
            Item {
                width: 56
                height: 56

                // Logo glow
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 8
                    color: Theme.withAlpha(color1, 0.4 + smoothValue * 0.25)
                }

                // Logo background
                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusXS
                    color: "#0A0A0F"
                    border.color: Theme.withAlpha(color1, 0.6 + smoothValue * 0.3)
                    border.width: 2

                    // Logo image placeholder
                    Text {
                        anchors.centerIn: parent
                        text: "C"
                        font.pixelSize: 28
                        font.weight: Font.Bold
                        color: color1
                    }
                }
            }

            // Text column
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // Title with animated color (color1 gold<->brown animasyonu)
                Text {
                    text: "CEDRA Interactive"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: color1  // Animasyonlu renk
                }

                // Subtitle
                Text {
                    text: "Turk oyun gelistirme ve ceviri toplulugu."
                    font.pixelSize: 14
                    color: Theme.textSecondary
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally("https://cedra.dev")
    }
}
