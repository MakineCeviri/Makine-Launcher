import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * TranslationSettings.qml - Translation preferences panel
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

    // -- Local component overrides (pixel-match SettingsScreen inline versions) --
    component SettingsCard: Rectangle {
        default property alias content: _cc.data
        implicitHeight: _cc.implicitHeight
        radius: Dimensions.radiusStandard
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        ColumnLayout { id: _cc; anchors.fill: parent; spacing: 0 }
    }

    component SettingsDivider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.withAlpha(Theme.textPrimary, 0.04)
    }

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
                    Layout.fillWidth: true; text: title
                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                    color: Theme.textPrimary; elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true; text: description
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted; elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: Math.max(_bLbl.width + 28, 70)
                Layout.preferredHeight: 28; radius: 14
                color: Theme.withAlpha(Theme.primary, 0.15)
                Label {
                    id: _bLbl; anchors.centerIn: parent; text: badgeText
                    font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                    color: Theme.primary; elide: Text.ElideRight
                }
            }
        }
    }

    component DisabledSetting: Item {
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
                    text: title; font.pixelSize: Dimensions.fontMD
                    font.weight: Font.Medium; color: Theme.textMuted
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
                Label {
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
                    id: _yLbl; anchors.centerIn: parent
                    text: qsTr("Yakında")
                    font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }
        }
    }
    // -- End local component overrides --

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
