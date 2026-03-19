import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

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
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL

        // Content (title and subtitle)
        ColumnLayout {
            spacing: Dimensions.spacingXS

            Label {
                textFormat: Text.PlainText
                text: root.title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
            }

            Label {
                textFormat: Text.PlainText
                text: root.description
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
            }
        }

        Item { Layout.fillWidth: true }

        // Badge — right-aligned
        Rectangle {
            implicitWidth: _badgeLbl.implicitWidth + 28
            implicitHeight: 32
            radius: Dimensions.radiusFull
            color: Theme.primary15

            Text {
                textFormat: Text.PlainText
                id: _badgeLbl
                anchors.centerIn: parent
                text: root.badgeText
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold
                color: Theme.primary
            }
        }
    }
}
