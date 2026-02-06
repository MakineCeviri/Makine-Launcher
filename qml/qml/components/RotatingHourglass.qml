import QtQuick
import MakineAI 1.0

/**
 * RotatingHourglass.qml - Continuously rotating hourglass icon with configurable size and color
 */
Item {
    id: root

    property int iconSize: 36
    property color iconColor: Theme.error
    property bool running: true

    implicitWidth: iconSize
    implicitHeight: iconSize

    Text {
        id: hourglass
        anchors.centerIn: parent
        text: "\u23F3"  // Hourglass
        font.pixelSize: root.iconSize
        color: root.iconColor

        // Rotation animation: continuous
        rotation: 0

        RotationAnimation {
            target: hourglass
            property: "rotation"
            from: 0
            to: 360
            duration: 1500
            loops: Animation.Infinite
            running: root.running
        }
    }
}
