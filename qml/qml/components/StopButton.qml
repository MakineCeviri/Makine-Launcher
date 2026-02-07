import QtQuick
import QtQuick.Controls
import MakineAI 1.0

/**
 * StopButton.qml - Full-width stop button with animated hover transition
 */
Rectangle {
    id: root

    signal clicked()

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 44
    radius: Dimensions.radiusXS  // 4

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Stop")
    Accessible.onPressAction: clicked()
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()

    // hoverProgress animation (200ms)
    property real hoverProgress: 0.0
    Behavior on hoverProgress {
        NumberAnimation { duration: 200 }
    }

    // bg color: Default white 5%, Hover red 15%
    color: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.05), Theme.withAlpha(Theme.error, 0.15), hoverProgress)

    // border color: Default white 10%, Hover red 50%
    border.color: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.1), Theme.withAlpha(Theme.error, 0.5), hoverProgress)
    border.width: 1

    scale: mouseArea.pressed ? 0.98 : 1.0
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

    // text/icon color: Default white 70%, Hover red
    property color contentColor: Theme.lerpColor(Qt.rgba(1, 1, 1, 0.7), Theme.error, hoverProgress)

    Row {
        anchors.centerIn: parent
        spacing: 8

        // Stop icon (square with rounded corners)
        Rectangle {
            width: 12
            height: 12
            radius: 2
            color: root.contentColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // Text
        Text {
            text: qsTr("Kapat")
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
