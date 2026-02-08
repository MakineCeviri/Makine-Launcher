import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string description: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 72
    color: "transparent"
    radius: Dimensions.radiusStandard
    opacity: 0.6

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL

        // Content (title, "Yakında" badge and description)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            RowLayout {
                spacing: Dimensions.spacingMD

                Label {
                    text: root.title
                    font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium
                    color: Theme.textMuted
                }

                Rectangle {
                    implicitWidth: comingSoonText.implicitWidth + 16
                    implicitHeight: 20
                    radius: Dimensions.radiusStandard
                    color: Qt.rgba(1, 1, 1, 0.08)

                    Text {
                        id: comingSoonText
                        anchors.centerIn: parent
                        text: qsTr("Yakında")
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.DemiBold
                        color: Theme.textMuted
                    }
                }
            }

            Label {
                text: root.description
                font.pixelSize: Dimensions.fontBody
                color: Theme.withAlpha(Theme.textMuted, 0.7)
            }
        }

        // Right item (Lock icon)
        Image {
            source: "qrc:/qt/qml/MakineAI/resources/icons/lock.svg"
            width: 20
            height: 20
            sourceSize: Qt.size(20, 20)
            antialiasing: true
        }
    }
}
