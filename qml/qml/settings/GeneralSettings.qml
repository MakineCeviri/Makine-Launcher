import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: 16

    // Language selection
    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: langColumn.implicitHeight + 16

        ColumnLayout {
            id: langColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("Uygulama Dili")
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("Arayüz dilini değiştir (yeniden başlatma gerekebilir)")
                        font.pixelSize: 11
                        color: Theme.textMuted
                    }
                }

                ComboBox {
                    id: langCombo
                    Layout.preferredWidth: 140
                    model: [
                        { value: "tr", label: "Türkçe" },
                        { value: "en", label: "English" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: SettingsManager.appLanguage === "en" ? 1 : 0
                    onActivated: SettingsManager.appLanguage = currentValue

                    background: Rectangle {
                        radius: Dimensions.radiusSmall
                        color: Theme.surfaceLight
                        border.color: Theme.surfaceActive
                        border.width: 1
                    }

                    contentItem: Text {
                        text: langCombo.displayText
                        font.pixelSize: 13
                        color: Theme.textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                }
            }
        }
    }

    // Theme
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

    // Toggle settings
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
                title: qsTr("Otomatik Oyun Tespiti")
                description: qsTr("Oyunları otomatik olarak tespit et")
                checked: SettingsManager.autoDetectGames
                onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Windows ile Başlat")
                description: qsTr("Bilgisayar açıldığında otomatik başlat")
                checked: SettingsManager.startWithWindows
                onToggled: SettingsManager.startWithWindows = !SettingsManager.startWithWindows
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Sistem Tepsisine Küçült")
                description: qsTr("Kapatıldığında arka planda çalışır")
                checked: SettingsManager.minimizeToTray
                onToggled: SettingsManager.minimizeToTray = !SettingsManager.minimizeToTray
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Bildirimler")
                description: qsTr("Oyun tespit edildiğinde bildirim göster")
                checked: SettingsManager.showNotifications
                onToggled: SettingsManager.showNotifications = !SettingsManager.showNotifications
            }
        }
    }
}
