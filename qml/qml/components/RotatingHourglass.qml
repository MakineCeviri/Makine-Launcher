import QtQuick
import MakineAI 1.0

/**
 * RotatingHourglass.qml - Native Qt RotatingHourglass birebir port
 * Kaynak: ui/src/widgets/rotatinghourglass.cpp
 *
 * Features:
 * - Continuous rotation animation
 * - Configurable size and color
 * - Start/stop control
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

        // Rotation animation - Native Qt: continuous
        rotation: 0

        // Native Qt: duration 1500ms
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
