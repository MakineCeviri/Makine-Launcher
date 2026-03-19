import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * CategoryItem.qml - Kategori liste öğesi
 */
Rectangle {
    id: root

    property string text: ""
    property string iconSource: ""
    property bool isSelected: false

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.text
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    implicitWidth: 200
    implicitHeight: 44
    radius: Dimensions.radiusStandard
    color: isSelected ? Theme.primary15 : (mouseArea.containsMouse ? Theme.textMuted05 : "transparent")

    Behavior on color {
        ColorAnimation { duration: Dimensions.animFast }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginMS
        anchors.rightMargin: Dimensions.marginMS
        spacing: Dimensions.spacingBase

        Image {
            source: root.iconSource
            sourceSize: Qt.size(18, 18)
            visible: root.iconSource !== ""
            asynchronous: true
            mipmap: true
            opacity: root.isSelected ? 1.0 : 0.6
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        }

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: root.text
            font.pixelSize: Dimensions.fontMD
            font.weight: root.isSelected ? Font.DemiBold : Font.Normal
            color: root.isSelected ? Theme.primary : Theme.textSecondary
            elide: Text.ElideRight
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
