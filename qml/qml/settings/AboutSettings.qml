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
        implicitHeight: infoColumn.implicitHeight + 16

        ColumnLayout {
            id: infoColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            InfoRow { label: "Uygulama"; value: Dimensions.appName }
            SettingsDivider {}
            InfoRow { label: "Versiyon"; value: Dimensions.appVersionFull }
            SettingsDivider {}
            InfoRow { label: "Geliştirici"; value: "Makine Çeviri" }
            SettingsDivider {}
            InfoRow { label: "Lisans"; value: "Ücretsiz Lisans" }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: linksColumn.implicitHeight + 16

        ColumnLayout {
            id: linksColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            ClickableRow {
                title: "Discord Desteği"
                subtitle: "Topluluk ve yardım için Discord sunucumuza katılın."
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }

            SettingsDivider {}

            ClickableRow {
                title: "Geri Bildirim"
                subtitle: "Hata bildirimi ve öneriler için web sitemizi ziyaret edin."
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/info.svg"
                onClicked: Qt.openUrlExternally("https://makineai.com/feedback")
            }
        }
    }
}
