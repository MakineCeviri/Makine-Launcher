import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: 16

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
                title: "Donanım Hızlandırma"
                description: "GPU kullanarak daha hızlı çeviri"
                checked: SettingsManager.hardwareAcceleration
                onToggled: SettingsManager.hardwareAcceleration = !SettingsManager.hardwareAcceleration
            }

            SettingsDivider {}

            ToggleSetting {
                title: "Global Önbellek"
                description: "Çevirileri tüm oyunlar için paylaş"
                checked: SettingsManager.useGlobalCache
                onToggled: SettingsManager.useGlobalCache = !SettingsManager.useGlobalCache
            }

            SettingsDivider {}

            ToggleSetting {
                title: "Uygulama Animasyonları"
                description: "Arayüz animasyonlarını etkinleştir"
                checked: SettingsManager.enableAnimations
                onToggled: SettingsManager.enableAnimations = !SettingsManager.enableAnimations
            }
        }
    }
}
