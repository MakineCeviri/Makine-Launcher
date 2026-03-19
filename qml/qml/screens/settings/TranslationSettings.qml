import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * TranslationSettings.qml - Translation preferences panel
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

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
