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
        implicitHeight: themeColumn.implicitHeight + 16

        ColumnLayout {
            id: themeColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            ThemeSetting {}
        }
    }

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: settingsColumn.implicitHeight + 16

        ColumnLayout {
            id: settingsColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            ToggleSetting {
                title: "Otomatik Oyun Tespiti"
                description: "Oyunları otomatik olarak tespit et"
                checked: SettingsManager.autoDetectGames
                onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
            }

            SettingsDivider {}

            ToggleSetting {
                title: "Windows ile Başlat"
                description: "Bilgisayar açıldığında otomatik başlat"
                checked: SettingsManager.startWithWindows
                onToggled: SettingsManager.startWithWindows = !SettingsManager.startWithWindows
            }

            SettingsDivider {}

            ToggleSetting {
                title: "Sistem Tepsisine Küçült"
                description: "Kapatıldığında arka planda çalışır"
                checked: SettingsManager.minimizeToTray
                onToggled: SettingsManager.minimizeToTray = !SettingsManager.minimizeToTray
            }

            SettingsDivider {}

            ToggleSetting {
                title: "Bildirimler"
                description: "Oyun tespit edildiğinde bildirim göster"
                checked: SettingsManager.showNotifications
                onToggled: SettingsManager.showNotifications = !SettingsManager.showNotifications
            }
        }
    }
}
