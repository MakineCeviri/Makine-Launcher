import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
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
        implicitHeight: backupColumn.implicitHeight + 16

        ColumnLayout {
            id: backupColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 12

            RowLayout {
                spacing: 8

                Text {
                    text: qsTr("Yedekler")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Rectangle {
                    visible: BackupManager.backups.length > 0
                    width: badgeText.implicitWidth + 12
                    height: 20
                    radius: 10
                    color: Theme.withAlpha(Theme.primary, 0.15)

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: BackupManager.backups.length
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.Medium
                        color: Theme.primary
                    }
                }
            }

            EmptyStateMessage {
                visible: BackupManager.backups.length === 0
                title: qsTr("Henüz yedeklenmiş oyun yok")
                iconSource: "qrc:/qt/qml/MakineAI/resources/icons/folder.svg"
            }

            Repeater {
                model: BackupManager.backups

                BackupListItem {
                    modelData: modelData
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: BackupManager.isRestoring
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        radius: Dimensions.radiusStandard
        implicitHeight: restoreRow.implicitHeight + 16

        RowLayout {
            id: restoreRow
            anchors.fill: parent
            anchors.margins: 8
            spacing: 12

            BusyIndicator {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                running: BackupManager.isRestoring
            }

            Text {
                Layout.fillWidth: true
                text: BackupManager.restoreStatus || qsTr("Geri yükleniyor...")
                font.pixelSize: Dimensions.fontMD
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }
}
