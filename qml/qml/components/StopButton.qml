import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * StopButton.qml - Native Qt StopButton birebir port
 * Kaynak: ui/src/widgets/stopbutton.cpp
 *
 * Features:
 * - Full width button
 * - "Kapat" text (Native Qt)
 * - 200ms animated hover transition
 * - Default: white-based colors, Hover: red-based colors
 */
Rectangle {
    id: root

    signal clicked()

    // Native Qt: Full width, height 44
    implicitWidth: parent ? parent.width : 200
    implicitHeight: 44
    radius: Dimensions.radiusXS  // 4

    // Native Qt: hoverProgress animation (200ms)
    property real hoverProgress: 0.0
    Behavior on hoverProgress {
        NumberAnimation { duration: 200 }
    }

    // Native Qt: bg color
    // Default: white 5%, Hover: red 15%
    color: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.05), Theme.withAlpha(Theme.error, 0.15), hoverProgress)

    // Native Qt: border color
    // Default: white 10%, Hover: red 50%
    border.color: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.1), Theme.withAlpha(Theme.error, 0.5), hoverProgress)
    border.width: 1

    scale: mouseArea.pressed ? 0.98 : 1.0
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

    // Native Qt: text/icon color
    // Default: white 70%, Hover: red
    property color contentColor: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.7), Theme.error, hoverProgress)

    Row {
        anchors.centerIn: parent
        spacing: 8

        // Native Qt: Stop icon (square with rounded corners, size 18)
        Rectangle {
            width: 12
            height: 12
            radius: 2
            color: root.contentColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // Text - Native Qt: "Kapat", fontSize 14, w500
        Text {
            text: "Kapat"
            font.pixelSize: 14
            font.weight: Font.Medium
            color: root.contentColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onEntered: root.hoverProgress = 1.0
        onExited: root.hoverProgress = 0.0
    }
}
