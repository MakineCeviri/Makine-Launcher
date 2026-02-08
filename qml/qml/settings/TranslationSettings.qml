import QtQuick
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

ColumnLayout {
    spacing: Dimensions.spacingXL

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: langColumn.implicitHeight + 16

        ColumnLayout {
            id: langColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            InfoSettingWithBadge {
                title: qsTr("Çeviri Dili")
                description: qsTr("Oyunların çevrileceği dil")
                badgeText: qsTr("Türkçe")
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        color: Theme.surface
        border.color: Theme.withAlpha(Theme.textPrimary, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: qualityColumn.implicitHeight + 16

        ColumnLayout {
            id: qualityColumn
            anchors.fill: parent
            anchors.margins: Dimensions.marginSM
            spacing: 0

            DisabledSetting {
                title: qsTr("Çeviri Kalitesi")
                description: qsTr("Bu özellik gelecek güncellemelerde eklenecektir")
            }
        }
    }
}
