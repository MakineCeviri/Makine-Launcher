import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * DeveloperSettings.qml - Developer tools and test features
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

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
