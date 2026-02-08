import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: Dimensions.spacingXL

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: perfColumn.implicitHeight + 16

        ColumnLayout {
            id: perfColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            ToggleSetting {
                title: qsTr("Donanım Hızlandırma")
                description: qsTr("GPU kullanarak daha hızlı çeviri")
                checked: SettingsManager.hardwareAcceleration
                onToggled: SettingsManager.hardwareAcceleration = !SettingsManager.hardwareAcceleration
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Global Önbellek")
                description: qsTr("Çevirileri tüm oyunlar için paylaş")
                checked: SettingsManager.useGlobalCache
                onToggled: SettingsManager.useGlobalCache = !SettingsManager.useGlobalCache
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Uygulama Animasyonları")
                description: qsTr("Arayüz animasyonlarını etkinleştir")
                checked: SettingsManager.enableAnimations
                onToggled: SettingsManager.enableAnimations = !SettingsManager.enableAnimations
            }
        }
    }
}
