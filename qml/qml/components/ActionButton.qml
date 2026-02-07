import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Button {
    id: root

    property string buttonText: ""
    property string buttonIconSource: ""
    property color buttonBaseColor: Theme.primary
    property color buttonHoverColor: Theme.primaryHover
    property color buttonTextColor: "white"
    property color buttonIconColor: "white"

    text: buttonText
    activeFocusOnTab: true
    implicitWidth: contentLayout.implicitWidth + 48
    implicitHeight: contentLayout.implicitHeight + 16

    background: Rectangle {
        radius: Dimensions.radiusStandard
        color: root.pressed ? Qt.darker(root.buttonHoverColor, 1.1) :
               root.hovered ? root.buttonHoverColor : root.buttonBaseColor

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    contentItem: RowLayout {
        id: contentLayout
        spacing: 8

        Image {
            visible: root.buttonIconSource !== ""
            source: root.buttonIconSource
            sourceSize: Qt.size(18, 18)
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: root.buttonText
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: root.buttonTextColor
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
