import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * DisabledSetting.qml - Disabled/coming-soon setting row with badge
 */
Item {
    property string title: ""
    property string description: ""
    Layout.fillWidth: true
    Layout.preferredHeight: 72
    opacity: 0.5

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Dimensions.marginML
        anchors.rightMargin: Dimensions.marginML
        spacing: Dimensions.spacingXL

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingXS

            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: title
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: description
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.preferredWidth: _dsLbl.width + 16
            Layout.preferredHeight: 22
            radius: 11
            color: Theme.primary10

            Label {
                textFormat: Text.PlainText
                id: _dsLbl
                anchors.centerIn: parent
                text: qsTr("Yakında")
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.DemiBold
                color: Theme.textMuted
            }
        }
    }
}
