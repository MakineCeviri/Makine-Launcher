import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string description: ""
    property string iconSource: ""
    property color iconColor: Theme.textMuted
    property int iconSize: 24

    Layout.fillWidth: true
    Layout.preferredHeight: 80
    color: Qt.rgba(1, 1, 1, 0.03)
    radius: Dimensions.radiusStandard

    RowLayout {
        anchors.centerIn: parent
        spacing: Dimensions.spacingLG

        Image {
            source: root.iconSource
            width: root.iconSize
            height: root.iconSize
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            Layout.alignment: Qt.AlignVCenter
            visible: root.iconSource !== ""
        }

        ColumnLayout {
            spacing: Dimensions.spacingXXS
            Layout.alignment: Qt.AlignVCenter

            Label {
                text: root.title
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted
            }

            Label {
                text: root.description
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
                visible: root.description !== ""
            }
        }
    }
}
