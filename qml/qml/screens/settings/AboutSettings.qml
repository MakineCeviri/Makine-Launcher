import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * AboutSettings.qml - About page with app info, updates, shortcuts, licenses
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

    component SettingsDivider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.withAlpha(Theme.textPrimary, 0.04)
    }

    component InfoRow: Item {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 56
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginML
            anchors.rightMargin: Dimensions.marginML
            Label {
                Layout.fillWidth: true; text: label
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted; elide: Text.ElideRight
            }
            Label {
                text: value; font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium; color: Theme.textPrimary
            }
        }
    }
    // -- End local component overrides --

    // App info card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            InfoRow { label: qsTr("Versiyon"); value: Dimensions.appVersionFull }
            SettingsDivider {}
            InfoRow { label: qsTr("Qt Sürümü"); value: SettingsManager.qtVersion() }
            SettingsDivider {}
            InfoRow { label: qsTr("Grafik API"); value: SettingsManager.activeGraphicsApi() }
            SettingsDivider {}
            InfoRow { label: qsTr("Geliştirici"); value: qsTr("MakineAI Ekibi") }
            SettingsDivider {}
            InfoRow { label: qsTr("Lisans"); value: qsTr("Ücretsiz Lisans") }
            SettingsDivider {}
            InfoRow { label: qsTr("Platform"); value: Qt.platform.os }
        }
    }

    // Update check card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                // Row 1: Status + buttons
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
                                text: qsTr("Güncelleme Kontrolü")
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: {
                                    if (UpdateChecker.downloading)
                                        return qsTr("İndiriliyor... %1%").arg(Math.round(UpdateChecker.downloadProgress * 100))
                                    if (UpdateChecker.readyToInstall)
                                        return qsTr("Güncelleme kurulmaya hazır")
                                    switch (UpdateChecker.statusType) {
                                        case "checking": return qsTr("Kontrol ediliyor...")
                                        case "updateAvailable": return qsTr("Yeni sürüm mevcut: %1").arg(UpdateChecker.latestVersion)
                                        case "upToDate": return qsTr("Güncel sürümdesiniz")
                                        case "error": return qsTr("Kontrol başarısız oldu")
                                        default: return qsTr("Son kontrol yapılmadı")
                                    }
                                }
                                font.pixelSize: Dimensions.fontBody
                                color: {
                                    if (UpdateChecker.downloading) return Theme.primary
                                    if (UpdateChecker.readyToInstall) return Theme.success
                                    switch (UpdateChecker.statusType) {
                                        case "updateAvailable": return Theme.success
                                        case "error": return Theme.error
                                        default: return Theme.textMuted
                                    }
                                }
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        // Check button
                        Rectangle {
                            Layout.preferredWidth: _updateBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusStandard
                            visible: !UpdateChecker.checking && !UpdateChecker.downloading && !UpdateChecker.readyToInstall
                            color: _updateBtnMouse.containsMouse
                                ? Theme.withAlpha(Theme.primary, 0.20)
                                : Theme.withAlpha(Theme.primary, 0.10)
                            scale: _updateBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
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
                                onClicked: UpdateChecker.checkForUpdates()
                            }
                        }

                        // Download button
                        Rectangle {
                            Layout.preferredWidth: _dlBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusStandard
                            visible: UpdateChecker.updateAvailable && !UpdateChecker.checking && !UpdateChecker.downloading && !UpdateChecker.readyToInstall
                            color: _dlBtnMouse.containsMouse ? Theme.success : Theme.withAlpha(Theme.success, 0.85)
                            scale: _dlBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
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
                                onClicked: UpdateChecker.downloadUpdate()
                            }
                        }

                        // Cancel button
                        Rectangle {
                            Layout.preferredWidth: _cancelBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusStandard
                            visible: UpdateChecker.downloading
                            color: _cancelBtnMouse.containsMouse
                                ? Theme.withAlpha(Theme.error, 0.20)
                                : Theme.withAlpha(Theme.error, 0.10)
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            Label {
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
                                onClicked: UpdateChecker.cancelDownload()
                            }
                        }

                        // Install button
                        Rectangle {
                            Layout.preferredWidth: _installBtnLbl.width + 24
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusStandard
                            visible: UpdateChecker.readyToInstall
                            color: _installBtnMouse.containsMouse ? Theme.success : Theme.withAlpha(Theme.success, 0.85)
                            scale: _installBtnMouse.pressed ? 0.94 : 1.0
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Label {
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
                                onClicked: UpdateChecker.installUpdate()
                            }
                        }

                        // Spinner when checking
                        BusyIndicator {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            running: UpdateChecker.checking
                            visible: UpdateChecker.checking
                            palette.dark: Theme.primary
                        }
                    }
                }

                // Row 2: Download progress bar
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UpdateChecker.downloading ? 28 : 0
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    visible: UpdateChecker.downloading
                    clip: true

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
                            color: Theme.withAlpha(Theme.primary, 0.15)

                            Rectangle {
                                width: parent.width * UpdateChecker.downloadProgress
                                height: parent.height
                                radius: 3
                                color: Theme.primary

                                Behavior on width {
                                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                                }
                            }
                        }

                        Label {
                            text: {
                                var sizeMB = UpdateChecker.installerSize / (1024 * 1024)
                                var downloadedMB = sizeMB * UpdateChecker.downloadProgress
                                return qsTr("%1 / %2 MB").arg(downloadedMB.toFixed(1)).arg(sizeMB.toFixed(1))
                            }
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            Layout.preferredWidth: 100
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                // Row 3: Error message
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UpdateChecker.downloadError ? 32 : 0
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    visible: UpdateChecker.downloadError !== ""
                    clip: true

                    Behavior on Layout.preferredHeight {
                        NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: Dimensions.spacingSM

                        Label {
                            text: UpdateChecker.downloadError
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
                                ? Theme.withAlpha(Theme.primary, 0.20)
                                : Theme.withAlpha(Theme.primary, 0.10)

                            Label {
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
                                onClicked: UpdateChecker.downloadUpdate()
                            }
                        }
                    }
                }
            }
        }
    }

    // Community links card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ClickableRow {
                title: qsTr("Discord Desteği")
                subtitle: qsTr("Topluluk ve yardım için Discord sunucumuza katılın")
                icon: "\uD83D\uDCAC"  // comment balloon
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Geri Bildirim")
                subtitle: qsTr("Hata bildirimi ve öneriler için web sitemizi ziyaret edin")
                icon: "\uD83D\uDCE7"  // envelope
                onClicked: Qt.openUrlExternally("https://makineai.com/feedback")
            }
        }
    }

    // Keyboard shortcuts card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            id: shortcutsSection
            Layout.fillWidth: true
            spacing: 0

            readonly property var shortcuts: [
                { key: "Ctrl+F", desc: qsTr("Oyun ara") },
                { key: "Ctrl+R", desc: qsTr("Yeniden tara") },
                { key: "Ctrl+,", desc: qsTr("Ayarlar") },
                { key: "Ctrl+H", desc: qsTr("Ana sayfa") },
                { key: "Ctrl+N", desc: qsTr("Bildirimler") },
                { key: "Ctrl+1", desc: qsTr("Ana sayfa") },
                { key: "Ctrl+2", desc: qsTr("Kütüphane") },
                { key: "Ctrl+3", desc: qsTr("Projelerimiz") },
                { key: "Ctrl+Q", desc: qsTr("Çıkış") },
                { key: "Escape", desc: qsTr("Geri dön") },
                { key: "F3", desc: qsTr("Performans monitörü") }
            ]

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML

                    Label {
                        text: qsTr("Klavye Kısayolları")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: "\u2328"
                        font.pixelSize: Dimensions.fontTitle
                        color: Theme.textMuted
                    }
                }
            }

            SettingsDivider {}

            Repeater {
                model: shortcutsSection.shortcuts

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Dimensions.marginML
                            anchors.rightMargin: Dimensions.marginML

                            Label {
                                text: modelData.desc
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textSecondary
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.preferredWidth: _keyLbl.width + 16
                                Layout.preferredHeight: 24
                                radius: 4
                                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.10)
                                border.width: 1

                                Label {
                                    id: _keyLbl
                                    anchors.centerIn: parent
                                    text: modelData.key
                                    font.pixelSize: Dimensions.fontXS
                                    font.weight: Font.Medium
                                    font.family: "Consolas"
                                    color: Theme.textMuted
                                }
                            }
                        }
                    }

                    SettingsDivider {
                        visible: index < shortcutsSection.shortcuts.length - 1
                    }
                }
            }
        }
    }

    // Open source licenses card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            id: licensesSection
            Layout.fillWidth: true
            spacing: 0

            readonly property var licenseModel: [
                { name: "Qt Framework", license: "LGPL v3", url: "https://www.qt.io/licensing" },
                { name: "Boost", license: "BSL-1.0", url: "https://www.boost.org/LICENSE_1_0.txt" },
                { name: "OpenSSL", license: "Apache-2.0", url: "https://www.openssl.org/source/license.html" },
                { name: "spdlog", license: "MIT", url: "https://github.com/gabime/spdlog/blob/v1.x/LICENSE" },
                { name: "nlohmann/json", license: "MIT", url: "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT" },
                { name: "Inter Font", license: "OFL-1.1", url: "https://github.com/rsms/inter/blob/master/LICENSE.txt" }
            ]

            // Section header
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML

                    Label {
                        text: qsTr("Açık Kaynak Lisanslar")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: licensesSection.licenseModel.length.toString()
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.textMuted
                    }
                }
            }

            SettingsDivider {}

            Repeater {
                model: licensesSection.licenseModel

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Dimensions.marginML
                            anchors.rightMargin: Dimensions.marginML

                            Label {
                                Layout.fillWidth: true
                                text: modelData.name
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }

                            Label {
                                text: modelData.license
                                font.pixelSize: Dimensions.fontSM
                                font.weight: Font.Medium
                                color: Theme.textMuted
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(modelData.url)
                        }
                    }

                    SettingsDivider {
                        visible: index < licensesSection.licenseModel.length - 1
                    }
                }
            }
        }
    }

    // Support card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ClickableRow {
                title: qsTr("Aramıza Katıl")
                subtitle: qsTr("MakineAI ekibine katılın")
                icon: "\u{1F91D}"  // handshake
                onClicked: Qt.openUrlExternally("https://makineai.com")
            }
        }
    }
}
