import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: Dimensions.spacingXL

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
            anchors.margins: Dimensions.marginSM
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: Dimensions.marginSM
                Layout.rightMargin: Dimensions.marginSM
                spacing: Dimensions.spacingLG

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXXS
                    Text {
                        text: qsTr("Uygulama Dili")
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("Arayüz dilini değiştir (yeniden başlatma gerekebilir)")
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textMuted
                    }
                }

                ComboBox {
                    id: langCombo
                    Layout.preferredWidth: 140
                    Accessible.name: qsTr("Application language")
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
                        font.pixelSize: Dimensions.fontBody
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
            anchors.margins: Dimensions.marginSM
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
            anchors.margins: Dimensions.marginSM
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

    // Cache management
    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: cacheColumn.implicitHeight + 16

        ColumnLayout {
            id: cacheColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: Dimensions.marginSM
                Layout.rightMargin: Dimensions.marginSM
                spacing: Dimensions.spacingLG

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXXS
                    Text {
                        text: qsTr("Önbellek Yönetimi")
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("Oyun verisi ve Steam detay önbelleğini temizle")
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textMuted
                    }
                }

                Rectangle {
                    Layout.preferredWidth: clearCacheBtnText.width + 24
                    Layout.preferredHeight: 32
                    radius: Dimensions.radiusSmall
                    color: clearCacheMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Clear cache")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.clearCache()
                    Keys.onSpacePressed: SettingsManager.clearCache()

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    Text {
                        id: clearCacheBtnText
                        anchors.centerIn: parent
                        text: qsTr("Temizle")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.textSecondary
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: clearCacheMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SettingsManager.clearCache()
                    }
                }
            }

            SettingsDivider {}

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: Dimensions.marginSM
                Layout.rightMargin: Dimensions.marginSM
                spacing: Dimensions.spacingLG

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXXS
                    Text {
                        text: qsTr("Varsayılana Sıfırla")
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    Text {
                        text: qsTr("Tüm ayarları fabrika değerlerine döndür")
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textMuted
                    }
                }

                Rectangle {
                    Layout.preferredWidth: resetBtnText.width + 24
                    Layout.preferredHeight: 32
                    radius: Dimensions.radiusSmall
                    color: resetMouse.containsMouse ? Theme.withAlpha(Theme.destructive, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: resetMouse.containsMouse ? Theme.withAlpha(Theme.destructive, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Reset to defaults")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: SettingsManager.resetToDefaults()
                    Keys.onSpacePressed: SettingsManager.resetToDefaults()

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                    Text {
                        id: resetBtnText
                        anchors.centerIn: parent
                        text: qsTr("Sıfırla")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: resetMouse.containsMouse ? Theme.destructive : Theme.textSecondary
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    // Focus indicator
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.destructive, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: resetMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: SettingsManager.resetToDefaults()
                    }
                }
            }
        }
    }
}
