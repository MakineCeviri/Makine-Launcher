import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * GeneralSettings.qml - General application settings panel
 */
ColumnLayout {
    id: generalRoot
    spacing: Dimensions.spacingXL

    signal clearCacheRequested()
    signal resetSettingsRequested()

    // Settings state - bound to SettingsManager
    property bool autoDetectGames: SettingsManager.autoDetectGames
    property bool startWithWindows: SettingsManager.startWithWindows
    property bool minimizeToTray: SettingsManager.minimizeToTray
    property bool disableAnimations: !SettingsManager.enableAnimations
    property bool showNotifications: SettingsManager.showNotifications
    property bool gameUpdateMonitoring: SettingsManager.gameUpdateMonitoring

    onAutoDetectGamesChanged: SettingsManager.autoDetectGames = autoDetectGames
    onStartWithWindowsChanged: SettingsManager.startWithWindows = startWithWindows
    onMinimizeToTrayChanged: SettingsManager.minimizeToTray = minimizeToTray
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations
    onShowNotificationsChanged: SettingsManager.showNotifications = showNotifications
    onGameUpdateMonitoringChanged: SettingsManager.gameUpdateMonitoring = gameUpdateMonitoring

    component ThemeSetting: Item {
        property bool isDarkTheme: SettingsManager.isDarkMode
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
                    Layout.fillWidth: true; text: qsTr("Tema")
                    font.pixelSize: Dimensions.fontMD; font.weight: Font.Medium
                    color: Theme.textPrimary; elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true; text: qsTr("Uygulama görünümünü seç")
                    font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: _themeRow.width + 8
                Layout.preferredHeight: 40
                radius: Dimensions.radiusStandard
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                Row {
                    id: _themeRow
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingXS
                    // Light theme - disabled
                    Rectangle {
                        width: _lightRow.width + 28; height: 32
                        radius: Dimensions.radiusStandard
                        color: "transparent"; opacity: 0.4
                        Row {
                            id: _lightRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM
                            Label {
                                text: qsTr("Açık")
                                font.pixelSize: Dimensions.fontBody; font.weight: Font.Medium
                                color: Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Rectangle {
                                width: _ykLbl.width + 10; height: 16; radius: 8
                                color: Theme.withAlpha(Theme.textPrimary, 0.1)
                                anchors.verticalCenter: parent.verticalCenter
                                Label {
                                    id: _ykLbl; anchors.centerIn: parent
                                    text: qsTr("Yakında")
                                    font.pixelSize: Dimensions.fontCaption
                                    font.weight: Font.DemiBold; color: Theme.textMuted
                                }
                            }
                        }
                    }
                    // Dark theme - active
                    Rectangle {
                        width: _darkRow.width + 28; height: 32
                        radius: Dimensions.radiusStandard
                        color: Theme.withAlpha(Theme.textPrimary, 0.12)
                        border.color: Theme.withAlpha(Theme.textPrimary, 0.2)
                        border.width: 1
                        Row {
                            id: _darkRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM
                            Label {
                                text: qsTr("Koyu")
                                font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ThemeSetting {}

            SettingsDivider {}

            // Accent color picker
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: _accentCol.implicitHeight + 2 * Dimensions.marginML

                ColumnLayout {
                    id: _accentCol
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML
                    anchors.topMargin: Dimensions.marginMS
                    anchors.bottomMargin: Dimensions.marginMS
                    spacing: Dimensions.spacingLG

                    Label {
                        text: qsTr("Vurgu Rengi")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: Dimensions.spacingLG
                        rowSpacing: Dimensions.spacingLG

                        Repeater {
                            model: SettingsManager.accentPresets()

                            Rectangle {
                                required property var modelData
                                required property int index

                                property bool isSelected: SettingsManager.accentPreset === modelData.id

                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                radius: Dimensions.radiusStandard
                                color: _presetMouse.containsMouse
                                    ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                    : (isSelected ? Theme.withAlpha(Theme.textPrimary, 0.04) : "transparent")
                                border.color: isSelected
                                    ? Theme.withAlpha(modelData.colors[2], 0.6)
                                    : (_presetMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.12) : Theme.withAlpha(Theme.textPrimary, 0.06))
                                border.width: isSelected ? 1.5 : 1

                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    // 5-tone color strip
                                    Row {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 14
                                        spacing: 2

                                        Repeater {
                                            model: modelData.colors
                                            Rectangle {
                                                required property string modelData
                                                required property int index
                                                width: (parent.width - 8) / 5
                                                height: 14
                                                radius: index === 0 ? 4 : (index === 4 ? 4 : 2)
                                                color: modelData
                                            }
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        font.pixelSize: Dimensions.fontCaption
                                        font.weight: isSelected ? Font.DemiBold : Font.Normal
                                        color: isSelected ? Theme.textPrimary : Theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: _presetMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: SettingsManager.accentPreset = modelData.id
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Otomatik Oyun Tespiti")
                description: qsTr("Oyunları otomatik olarak tespit et")
                checked: generalRoot.autoDetectGames
                disableAnimations: generalRoot.disableAnimations
                onToggled: generalRoot.autoDetectGames = !generalRoot.autoDetectGames
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Windows ile Başlat")
                description: qsTr("Bilgisayar açıldığında otomatik başlat")
                checked: generalRoot.startWithWindows
                disableAnimations: generalRoot.disableAnimations
                onToggled: generalRoot.startWithWindows = !generalRoot.startWithWindows
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Sistem Tepsisine Küçült")
                description: qsTr("Kapatıldığında arka planda çalışır")
                checked: generalRoot.minimizeToTray
                disableAnimations: generalRoot.disableAnimations
                onToggled: generalRoot.minimizeToTray = !generalRoot.minimizeToTray
            }

            SettingsDivider {}

            ToggleSetting {
                title: qsTr("Güncelleme Kontrolü")
                description: qsTr("Başlatıldığında yeni sürüm olup olmadığını kontrol et")
                checked: generalRoot.showNotifications
                disableAnimations: generalRoot.disableAnimations
                onToggled: generalRoot.showNotifications = !generalRoot.showNotifications
            }
        }
    }

    // Planned features
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ToggleSetting {
                title: qsTr("Oyun Güncelleme İzleme")
                description: qsTr("Arka planda oyun güncellemelerini tespit et ve çeviri uyumluluğunu kontrol et")
                checked: generalRoot.gameUpdateMonitoring
                disableAnimations: generalRoot.disableAnimations
                onToggled: generalRoot.gameUpdateMonitoring = !generalRoot.gameUpdateMonitoring
            }

            SettingsDivider {}

            // Translation data directory
            Item {
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
                            text: qsTr("Çeviri Veri Dizini")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: SettingsManager.translationDataPath
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: _openDirLbl.width + 24
                        Layout.preferredHeight: 28
                        radius: Dimensions.radiusStandard
                        color: _openDirMouse.containsMouse
                            ? Theme.withAlpha(Theme.primary, 0.20)
                            : Theme.withAlpha(Theme.primary, 0.10)
                        scale: _openDirMouse.pressed ? 0.94 : 1.0
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                        Label {
                            id: _openDirLbl
                            anchors.centerIn: parent
                            text: qsTr("Klasörü Aç")
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.DemiBold
                            color: Theme.primary
                        }

                        MouseArea {
                            id: _openDirMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("file:///" + SettingsManager.translationDataPath)
                        }
                    }
                }
            }

            SettingsDivider {}

            // Cache management
            Item {
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
                            text: qsTr("Önbellek Yönetimi")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("Uygulama önbellek dosyalarını temizle (%1)").arg(ImageCache.cacheSizeFormatted)
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: _clearCacheLbl.width + 24
                        Layout.preferredHeight: 28
                        radius: Dimensions.radiusStandard
                        color: _clearCacheMouse.containsMouse
                            ? Theme.withAlpha(Theme.warning, 0.20)
                            : Theme.withAlpha(Theme.warning, 0.10)
                        scale: _clearCacheMouse.pressed ? 0.94 : 1.0
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                        Label {
                            id: _clearCacheLbl
                            anchors.centerIn: parent
                            text: qsTr("Temizle")
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.DemiBold
                            color: Theme.warning
                        }

                        MouseArea {
                            id: _clearCacheMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: generalRoot.clearCacheRequested()
                        }
                    }
                }
            }

            SettingsDivider {}

            DisabledSetting {
                title: qsTr("Proxy Ayarları")
                description: qsTr("Ağ bağlantısı için proxy yapılandırması")
            }
        }
    }

    // Reset to defaults
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 48
        Layout.topMargin: Dimensions.spacingSM

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: _resetLbl.width + 32
                Layout.preferredHeight: 34
                radius: Dimensions.radiusStandard
                color: _resetMouse.containsMouse
                    ? Theme.withAlpha(Theme.error, 0.12)
                    : "transparent"
                border.color: Theme.withAlpha(Theme.error, 0.25)
                border.width: 1
                scale: _resetMouse.pressed ? 0.94 : 1.0
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Ayarları Sıfırla")
                activeFocusOnTab: true
                Keys.onReturnPressed: generalRoot.resetSettingsRequested()

                Label {
                    id: _resetLbl
                    anchors.centerIn: parent
                    text: qsTr("Ayarları Sıfırla")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: _resetMouse.containsMouse ? Theme.error : Theme.textMuted
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }

                MouseArea {
                    id: _resetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: generalRoot.resetSettingsRequested()
                }
            }
        }
    }
}
