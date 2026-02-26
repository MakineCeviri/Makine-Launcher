import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * DisabledSetting.qml - Disabled/coming-soon setting row with badge
 */
Item {
    property string title: ""
    property string description: ""
    Layout.fillWidth: true
    Layout.preferredHeight: 72
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
                text: title; font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium; color: Theme.textMuted
                Layout.fillWidth: true; elide: Text.ElideRight
            }
            Label {
                textFormat: Text.PlainText
                text: description; font.pixelSize: Dimensions.fontBody
                color: Theme.withAlpha(Theme.textMuted, 0.7)
                Layout.fillWidth: true; elide: Text.ElideRight
            }
        }
        Rectangle {
            Layout.preferredWidth: _yLbl.width + 24
            Layout.preferredHeight: 28; radius: 14
            color: Theme.withAlpha(Theme.textPrimary, 0.08)
            Label {
                textFormat: Text.PlainText
                id: _yLbl; anchors.centerIn: parent
                text: qsTr("Yakında")
                font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold
                color: Theme.textMuted
            }
        }
    }
}
