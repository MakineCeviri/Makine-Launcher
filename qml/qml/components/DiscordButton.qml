import QtQuick
import MakineAI 1.0

/**
 * DiscordButton.qml - Native Qt DiscordButton birebir port
 * Kaynak: ui/src/widgets/discordbutton.cpp
 *
 * Features:
 * - Discord brand color #5865F2
 * - Pulsing glow animation
 * - Optional label text
 */
Rectangle {
    id: root

    property bool showLabel: false
    property color discordColor: "#5865F2"
    property bool animationsEnabled: true  // GPU optimization

    signal clicked()

    // Pulse animation value (0-1)
    property real pulseValue: 0.0

    implicitWidth: showLabel ? 110 : 42
    implicitHeight: 36
    radius: Dimensions.radiusXS

    // Flutter: glowOpacity = 0.15 + (pulseValue * 0.25)
    property real glowOpacity: 0.15 + (pulseValue * 0.25)

    // Background color
    color: mouseArea.containsMouse ?
           Theme.withAlpha(discordColor, 0.15) :
           Theme.withAlpha(discordColor, glowOpacity * 0.5)

    // Border
    border.color: Theme.withAlpha(discordColor, mouseArea.containsMouse ? 0.5 : glowOpacity)
    border.width: 1

    Behavior on color { ColorAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    // Glow effect
    Rectangle {
        anchors.fill: parent
        anchors.margins: mouseArea.containsMouse ? -3 : -(2 + pulseValue)
        radius: parent.radius + 2
        color: "transparent"
        border.color: Theme.withAlpha(discordColor, glowOpacity * 0.6)
        border.width: 2
        z: -1
    }

    // Pulse animation - Native Qt: duration 2000ms, reverse
    // GPU optimization: controlled by animationsEnabled
    SequentialAnimation on pulseValue {
        loops: Animation.Infinite
        running: root.animationsEnabled
        NumberAnimation { to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 0.0; duration: 1000; easing.type: Easing.InOutQuad }
    }

    // Content
    Row {
        anchors.centerIn: parent
        spacing: 8

        // Discord icon (simplified shape)
        Item {
            width: 18
            height: 18
            anchors.verticalCenter: parent.verticalCenter

            // Simplified Discord logo shape
            Rectangle {
                anchors.centerIn: parent
                width: 14
                height: 8
                radius: 3
                color: mouseArea.containsMouse ? discordColor : Theme.withAlpha(discordColor, 0.9)
            }

            // Left ear
            Rectangle {
                x: 2
                y: 2
                width: 4
                height: 4
                radius: 2
                color: mouseArea.containsMouse ? discordColor : Theme.withAlpha(discordColor, 0.9)
            }

            // Right ear
            Rectangle {
                x: 12
                y: 2
                width: 4
                height: 4
                radius: 2
                color: mouseArea.containsMouse ? discordColor : Theme.withAlpha(discordColor, 0.9)
            }
        }

        // Label text
        Text {
            visible: root.showLabel
            text: "Discord"
            font.pixelSize: 13
            font.weight: Font.Medium
            color: mouseArea.containsMouse ? discordColor : Theme.textSecondary
            anchors.verticalCenter: parent.verticalCenter

            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
