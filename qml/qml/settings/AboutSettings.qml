import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: Dimensions.spacingXL

    // App info card
    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: infoColumn.implicitHeight + 16

        ColumnLayout {
            id: infoColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            InfoRow { label: qsTr("Uygulama"); value: Dimensions.appName }
            SettingsDivider {}
            InfoRow { label: qsTr("Versiyon"); value: Dimensions.appVersionFull }
            SettingsDivider {}
            InfoRow { label: qsTr("Geliştirici"); value: qsTr("Makine Çeviri") }
            SettingsDivider {}
            InfoRow { label: qsTr("Lisans"); value: qsTr("Ücretsiz Lisans") }
            SettingsDivider {}
            InfoRow { label: qsTr("Platform"); value: Qt.platform.os }
            SettingsDivider {}
            InfoRow { label: "Qt"; value: "6.10.1" }
        }
    }

    // Links card
    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: linksColumn.implicitHeight + 16

        ColumnLayout {
            id: linksColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            ClickableRow {
                title: qsTr("Discord Desteği")
                subtitle: qsTr("Topluluk ve yardım için Discord sunucumuza katılın.")
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/discord.svg"
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }

            SettingsDivider {}

            ClickableRow {
                title: "GitHub"
                subtitle: qsTr("Kaynak kodu, hata bildirimi ve katkılar")
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                onClicked: Qt.openUrlExternally("https://github.com/jlceaser/MakineAI")
            }

            SettingsDivider {}

            ClickableRow {
                title: qsTr("Geri Bildirim")
                subtitle: qsTr("Hata bildirimi ve öneriler için web sitemizi ziyaret edin.")
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                onClicked: Qt.openUrlExternally("https://makineai.com/feedback")
            }
        }
    }

    // Open source licenses card
    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: licensesColumn.implicitHeight + 16

        ColumnLayout {
            id: licensesColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.leftMargin: Dimensions.marginSM
                Layout.rightMargin: Dimensions.marginSM
                spacing: Dimensions.spacingMD

                Text {
                    text: qsTr("Açık Kaynak Lisanslar")
                    font.pixelSize: Dimensions.fontBody
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }
            }

            SettingsDivider {}

            Repeater {
                model: [
                    { name: "Qt Framework", license: "LGPL v3", url: "https://www.qt.io/licensing" },
                    { name: "Boost", license: "BSL-1.0", url: "https://www.boost.org/LICENSE_1_0.txt" },
                    { name: "OpenSSL", license: "Apache-2.0", url: "https://www.openssl.org/source/license.html" },
                    { name: "spdlog", license: "MIT", url: "https://github.com/gabime/spdlog/blob/v1.x/LICENSE" },
                    { name: "nlohmann/json", license: "MIT", url: "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT" },
                    { name: "Inter Font", license: "OFL-1.1", url: "https://github.com/rsms/inter/blob/master/LICENSE.txt" }
                ]

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    ClickableRow {
                        title: modelData.name
                        subtitle: modelData.license
                        iconSource: "qrc:/qt/qml/MakineAI/resources/icons/chevron-right.svg"
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }

                    SettingsDivider {
                        visible: index < 5
                    }
                }
            }
        }
    }
}
