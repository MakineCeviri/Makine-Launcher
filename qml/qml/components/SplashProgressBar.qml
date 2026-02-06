import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Item {
    id: root

    property real progressValue: 0.0 // Value from 0.0 to 1.0
    property string statusText: ""

    width: 200
    height: 50

    // Progress bar background
    Rectangle {
        id: progressBg
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        height: 6
        radius: 3
        color: Qt.rgba(1, 1, 1, 0.08)

        // Progress fill
        Rectangle {
            width: parent.width * root.progressValue
            height: parent.height
            radius: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.splashGold }
                GradientStop { position: 0.5; color: Theme.splashOrange }
                GradientStop { position: 1.0; color: Theme.splashPink }
            }

            Behavior on width { NumberAnimation { duration: 200 } }
        }
    }

    // Loading text - dynamic status
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: progressBg.bottom
        anchors.topMargin: 14
        text: root.statusText
        font.pixelSize: 11
        font.weight: Font.Medium
        font.letterSpacing: 1.5
        color: Qt.rgba(1, 1, 1, 0.35)
    }
}
