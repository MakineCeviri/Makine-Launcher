import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string description: ""
    property string status: ""
    property color statusColor: Theme.primary

    width: 280
    height: 140
    color: Qt.rgba(1, 1, 1, 0.03)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    radius: Dimensions.radiusXS

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Dimensions.marginMD
        spacing: 10

        // Top row
        RowLayout {
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: Dimensions.radiusXS
                color: Theme.withAlpha(root.statusColor, 0.12)

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/MakineAI/resources/icons/box.svg"
                    sourceSize: Qt.size(18, 18)
                    antialiasing: true
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: "white"
            }
        }

        // Description
        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.description
            font.pixelSize: 13
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }

        // Status badge
        Rectangle {
            implicitWidth: statusText.implicitWidth + Dimensions.paddingLG
            implicitHeight: 22
            radius: Dimensions.radiusXS
            color: Theme.withAlpha(root.statusColor, 0.12)

            Text {
                id: statusText
                anchors.centerIn: parent
                text: root.status
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: root.statusColor
            }
        }
    }
}
