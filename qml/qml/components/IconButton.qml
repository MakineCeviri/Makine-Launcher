import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MakineAI 1.0

/**
 * IconButton.qml - Sadece ikon içeren buton
 */
Button {
    id: root

    property string iconSource: ""
    property int iconSize: 20
    property color iconColor: Theme.textSecondary
    property color hoverColor: Theme.primary
    property color hoverIconColor: Theme.textPrimary
    property color backgroundColor: "transparent"
    property color hoverBackgroundColor: Theme.withAlpha(Theme.primary, 0.1)
    property int radius: Dimensions.radiusStandard

    implicitWidth: 36
    implicitHeight: 36

    background: Rectangle {
        radius: root.radius
        color: root.hovered ? root.hoverBackgroundColor : root.backgroundColor

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    contentItem: Item {
        Image {
            id: iconImage
            anchors.centerIn: parent
            source: root.iconSource
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            fillMode: Image.PreserveAspectFit
            visible: false
        }

        MultiEffect {
            anchors.fill: iconImage
            source: iconImage
            colorization: 1.0
            colorizationColor: root.hovered ? root.hoverIconColor : root.iconColor

            Behavior on colorizationColor {
                ColorAnimation { duration: 150 }
            }
        }
    }
}
