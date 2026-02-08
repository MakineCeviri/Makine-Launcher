import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property color categoryColor: Theme.primary
    property string icon: ""
    property var model: []

    implicitHeight: catContent.implicitHeight
    color: Theme.surface
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1
    radius: Dimensions.radiusXS

    ColumnLayout {
        id: catContent
        width: parent.width
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: Dimensions.radiusXS
                color: Theme.withAlpha(root.categoryColor, 0.15)

                Image {
                    anchors.centerIn: parent
                    source: root.icon
                    sourceSize: Qt.size(22, 22)
                    antialiasing: true
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Label {
                    text: root.title
                    font.pixelSize: Dimensions.fontTitle
                    font.weight: Font.DemiBold
                    color: "white"
                }

                Label {
                    text: root.subtitle
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
        }

        // Project cards area
        Flow {
            Layout.fillWidth: true
            Layout.margins: Dimensions.marginMD
            spacing: Dimensions.cardGap

            Repeater {
                model: root.model
                delegate: ProjectCard {
                    title: modelData.title
                    description: modelData.description
                    status: modelData.status
                    statusColor: modelData.statusColor
                }
            }
        }
    }
}
