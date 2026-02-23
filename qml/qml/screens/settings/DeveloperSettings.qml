import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * DeveloperSettings.qml - Developer tools and test features
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

            DisabledSetting {
                title: qsTr("Translation Memory")
                description: qsTr("Çeviri belleği test verisi aktarma ve yönetim araçları")
            }

            SettingsDivider {}

            DisabledSetting {
                title: qsTr("Glossary Yönetimi")
                description: qsTr("Terim sözlüğü görüntüleme ve düzenleme")
            }

            SettingsDivider {}

            DisabledSetting {
                title: qsTr("Adaptasyon Motoru")
                description: qsTr("Güncelleme tespiti ve otomatik uyarlama araçları")
            }
        }
    }
}
