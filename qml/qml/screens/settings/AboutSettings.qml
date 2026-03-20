import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AboutSettings.qml - About page with app info, updates, shortcuts, licenses
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

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
                textFormat: Text.PlainText
                Layout.fillWidth: true; text: label
                font.pixelSize: Dimensions.fontMD
                color: Theme.textMuted; elide: Text.ElideRight
            }
            Label {
                textFormat: Text.PlainText
                text: value; font.pixelSize: Dimensions.fontMD
                font.weight: Font.Medium; color: Theme.textPrimary
            }
        }
    }

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
            InfoRow {
                label: qsTr("Grafik API")
                value: {
                    switch (SettingsManager.graphicsBackend) {
                        case "vulkan": return "Vulkan"
                        case "d3d11": return "Direct3D 11"
                        case "opengl": return "OpenGL"
                        default: return SettingsManager.activeGraphicsApi()
                    }
                }
            }
            SettingsDivider {}
            InfoRow { label: qsTr("Geliştirici"); value: qsTr("Makine Çeviri") }
            SettingsDivider {}
            InfoRow { label: qsTr("Lisans"); value: qsTr("AGPL-3.0 + Commons Clause") }
            SettingsDivider {}
            InfoRow { label: qsTr("Platform"); value: "Windows" }
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
                { key: "Ctrl+R", desc: qsTr("Uygulamayı yeniden başlat") },
                { key: "Alt+F4", desc: qsTr("Uygulamayı kapat") }
            ]

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Dimensions.marginML
                    anchors.rightMargin: Dimensions.marginML

                    Label {
                        textFormat: Text.PlainText
                        text: qsTr("Klavye Kısayolları")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                }
            }

            SettingsDivider {}

            Repeater {
                model: shortcutsSection.shortcuts

                ColumnLayout {
                    required property var modelData
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
                                textFormat: Text.PlainText
                                text: modelData.desc
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textSecondary
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.preferredWidth: _keyLbl.width + 16
                                Layout.preferredHeight: 24
                                radius: Dimensions.radiusMD
                                color: Theme.primary08
                                border.color: Theme.primary12
                                border.width: 1

                                Label {
                                    textFormat: Text.PlainText
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
                        textFormat: Text.PlainText
                        text: qsTr("Açık Kaynak Lisanslar")
                        font.pixelSize: Dimensions.fontMD
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        textFormat: Text.PlainText
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
                    required property var modelData
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
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: modelData.name
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }

                            Label {
                                textFormat: Text.PlainText
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

    // Community & support links card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            ClickableRow {
                title: qsTr("Discord Desteği")
                subtitle: qsTr("Topluluk ve yardım için Discord sunucumuza katılın")
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Geri Bildirim")
                subtitle: qsTr("Hata bildirimi ve öneriler için iletişim sayfamızı ziyaret edin")
                onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Aramıza Katıl")
                subtitle: qsTr("MakineCeviri ekibine katılın")
                onClicked: Qt.openUrlExternally("https://makineceviri.org/iletisim/")
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Gizlilik Politikası")
                subtitle: qsTr("Kişisel verilerinizin nasıl işlendiğini öğrenin")
                onClicked: Qt.openUrlExternally("https://makineceviri.org/gizlilik-politikasi/")
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Kullanım Koşulları")
                subtitle: qsTr("Hizmet şartları ve kullanım kuralları")
                onClicked: Qt.openUrlExternally("https://makineceviri.org/kullanim-kosullari/")
            }
        }
    }
}
