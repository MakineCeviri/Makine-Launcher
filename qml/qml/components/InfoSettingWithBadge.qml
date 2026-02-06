import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property string title: ""
    property string description: ""
    property string badgeText: ""

    Layout.fillWidth: true
    Layout.preferredHeight: 72
    color: "transparent"
    radius: Dimensions.radiusStandard

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        // Content (title and subtitle)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: root.title
                font.pixelSize: 14
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Label {
                text: root.description
                font.pixelSize: 13
                color: Theme.textMuted
            }
        }

        // Badge
        Rectangle {
            implicitWidth: badgeText.implicitWidth + 28
            implicitHeight: 32
            radius: 16
            color: Theme.withAlpha(Theme.primary, 0.15)

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: root.badgeText
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.primary
            }
        }
    }
}
