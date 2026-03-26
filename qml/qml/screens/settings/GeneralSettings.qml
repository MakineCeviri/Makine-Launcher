import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

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

    onAutoDetectGamesChanged: SettingsManager.autoDetectGames = autoDetectGames
    onStartWithWindowsChanged: SettingsManager.startWithWindows = startWithWindows
    onMinimizeToTrayChanged: SettingsManager.minimizeToTray = minimizeToTray
    onDisableAnimationsChanged: SettingsManager.enableAnimations = !disableAnimations

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

        }
    }

    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            // Update check — single State enum drives all UI
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

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
                                textFormat: Text.PlainText
                                text: qsTr("Güncelleme Kontrolü")
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Label {
                                textFormat: Text.PlainText
                                text: {
                                    switch (UpdateService.state) {
                                    case UpdateService.Checking:
                                        return qsTr("Kontrol ediliyor...")
                                    case UpdateService.Available:
                                        return qsTr("Yeni sürüm mevcut: %1").arg(UpdateService.version)
                                    case UpdateService.Downloading:
                                        return qsTr("İndiriliyor... %1%").arg(Math.round(UpdateService.progress * 100))
                                    case UpdateService.Verifying:
                                        return qsTr("Doğrulanıyor...")
                                    case UpdateService.Ready:
                                        return qsTr("Güncelleme kurulmaya hazır")
                                    case UpdateService.Installing:
                                        return qsTr("Güncelleme kuruluyor...")
                                    case UpdateService.Idle:
                                        return UpdateService.error ? qsTr("Kontrol başarısız oldu")
                                                                   : qsTr("Güncel sürümdesiniz")
                                    default:
                                        return qsTr("Son kontrol yapılmadı")
                                    }
                                }
                                font.pixelSize: Dimensions.fontBody
                                color: {
                                    switch (UpdateService.state) {
                                    case UpdateService.Available: return Theme.success
                                    case UpdateService.Downloading:
                                    case UpdateService.Verifying: return Theme.primary
                                    case UpdateService.Ready: return Theme.success
                                    case UpdateService.Idle:
                                        return UpdateService.error ? Theme.error : Theme.textMuted
                                    default: return Theme.textMuted
                                    }
                                }
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        // Check button — visible in Idle state
                        Rectangle {
                            Layout.preferredWidth: _updateBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusMD
                            visible: UpdateService.state === UpdateService.Idle
                            color: _updateBtnMouse.containsMouse
                                ? Theme.primary20
                                : Theme.primary10
                            scale: _updateBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
                                textFormat: Text.PlainText
                                id: _updateBtnLbl
                                anchors.centerIn: parent
                                text: qsTr("Kontrol Et")
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.DemiBold
                                color: Theme.primary
                            }

                            MouseArea {
                                id: _updateBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: UpdateService.check()
                            }
                        }

                        // Download button — visible in Available state
                        Rectangle {
                            Layout.preferredWidth: _dlBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusMD
                            visible: UpdateService.state === UpdateService.Available
                            color: _dlBtnMouse.containsMouse ? Theme.success : Theme.success85
                            scale: _dlBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
                                textFormat: Text.PlainText
                                id: _dlBtnLbl
                                anchors.centerIn: parent
                                text: qsTr("İndir ve Kur")
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.DemiBold
                                color: Theme.textOnColor
                            }

                            MouseArea {
                                id: _dlBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: UpdateService.download()
                            }
                        }

                        // Cancel button — visible while downloading/verifying
                        Rectangle {
                            Layout.preferredWidth: _cancelBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusMD
                            visible: UpdateService.state === UpdateService.Downloading
                                     || UpdateService.state === UpdateService.Verifying
                            color: _cancelBtnMouse.containsMouse
                                ? Theme.error20
                                : Theme.error10
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            Label {
                                textFormat: Text.PlainText
                                id: _cancelBtnLbl
                                anchors.centerIn: parent
                                text: qsTr("İptal")
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.DemiBold
                                color: Theme.error
                            }

                            MouseArea {
                                id: _cancelBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: UpdateService.cancel()
                            }
                        }

                        // Install button — visible when Ready
                        Rectangle {
                            Layout.preferredWidth: _installBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusMD
                            visible: UpdateService.state === UpdateService.Ready
                            color: _installBtnMouse.containsMouse ? Theme.success : Theme.success85
                            scale: _installBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
                                textFormat: Text.PlainText
                                id: _installBtnLbl
                                anchors.centerIn: parent
                                text: qsTr("Şimdi Kur")
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.DemiBold
                                color: Theme.textOnColor
                            }

                            MouseArea {
                                id: _installBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: UpdateService.install()
                            }
                        }

                        // Spinner when checking
                        BusyIndicator {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            running: UpdateService.state === UpdateService.Checking
                            visible: running
                            palette.dark: Theme.primary
                        }
                    }
                }

                // Download progress bar
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: (UpdateService.state === UpdateService.Downloading
                                            || UpdateService.state === UpdateService.Verifying) ? 28 : 0
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    visible: UpdateService.state === UpdateService.Downloading
                             || UpdateService.state === UpdateService.Verifying

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: Dimensions.spacingMD

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Theme.primary15

                            Rectangle {
                                width: parent.width * UpdateService.progress
                                height: parent.height
                                radius: 3
                                color: Theme.primary

                                Behavior on width {
                                    NumberAnimation { duration: Dimensions.animMedium; easing.type: Easing.OutCubic }
                                }
                            }
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: {
                                var sizeMB = UpdateService.totalBytes / (1024 * 1024)
                                var downloadedMB = sizeMB * UpdateService.progress
                                return qsTr("%1 / %2 MB").arg(downloadedMB.toFixed(1)).arg(sizeMB.toFixed(1))
                            }
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            Layout.preferredWidth: 100
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                // Error message
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UpdateService.error ? 32 : 0
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    visible: UpdateService.error !== ""

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: Dimensions.spacingSM

                        Label {
                            textFormat: Text.PlainText
                            text: UpdateService.error
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.error
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.preferredWidth: _retryLbl.width + 16
                            Layout.preferredHeight: 24
                            radius: Dimensions.radiusSM
                            color: _retryMouse.containsMouse
                                ? Theme.primary20
                                : Theme.primary10

                            Label {
                                textFormat: Text.PlainText
                                id: _retryLbl
                                anchors.centerIn: parent
                                text: qsTr("Tekrar Dene")
                                font.pixelSize: Dimensions.fontXS
                                font.weight: Font.DemiBold
                                color: Theme.primary
                            }

                            MouseArea {
                                id: _retryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: UpdateService.download()
                            }
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
                            textFormat: Text.PlainText
                            text: qsTr("Önbellek Yönetimi")
                            font.pixelSize: Dimensions.fontMD
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            textFormat: Text.PlainText
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
                        radius: Dimensions.radiusMD
                        color: _clearCacheMouse.containsMouse
                            ? Theme.warning20
                            : Theme.warning10
                        scale: _clearCacheMouse.pressed ? 0.94 : 1.0
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                        Label {
                            textFormat: Text.PlainText
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
            anchors.rightMargin: 0

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: _resetLbl.width + 32
                Layout.preferredHeight: 34
                radius: Dimensions.radiusMD
                color: _resetMouse.containsMouse
                    ? Theme.error12
                    : "transparent"
                border.color: Theme.error25
                border.width: 1
                scale: _resetMouse.pressed ? 0.94 : 1.0
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Ayarları Sıfırla")
                activeFocusOnTab: true
                Keys.onReturnPressed: generalRoot.resetSettingsRequested()

                Label {
                    textFormat: Text.PlainText
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
