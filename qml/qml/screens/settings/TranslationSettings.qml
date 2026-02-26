import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * TranslationSettings.qml - Translation preferences panel
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

    component InfoSettingWithBadge: Item {
        property string title: ""
        property string description: ""
        property string badgeText: ""
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
                    Layout.fillWidth: true; text: title
                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                    color: Theme.textPrimary; elide: Text.ElideRight
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true; text: description
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted; elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: Math.max(_bLbl.width + 28, 70)
                Layout.preferredHeight: 28; radius: 14
                color: Theme.primary15
                Label {
                    textFormat: Text.PlainText
                    id: _bLbl; anchors.centerIn: parent; text: badgeText
                    font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                    color: Theme.primary; elide: Text.ElideRight
                }
            }
        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            InfoSettingWithBadge {
                title: qsTr("Çeviri Dili")
                description: qsTr("Oyunların çevrileceği dil")
                badgeText: qsTr("Türkçe")
            }
        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            DisabledSetting {
                title: qsTr("Çeviri Kalitesi")
                description: qsTr("Bu özellik gelecek güncellemelerde eklenecektir")
            }
        }
    }
}
