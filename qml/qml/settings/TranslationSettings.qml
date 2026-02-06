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
        implicitHeight: langColumn.implicitHeight + 16

        ColumnLayout {
            id: langColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            InfoSettingWithBadge {
                title: "Çeviri Dili"
                description: "Oyunların çevrileceği dil"
                badgeText: "Türkçe"
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: qualityColumn.implicitHeight + 16

        ColumnLayout {
            id: qualityColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            DisabledSetting {
                title: "Çeviri Kalitesi"
                description: "Bu özellik gelecek güncellemelerde eklenecektir"
            }
        }
    }
}
