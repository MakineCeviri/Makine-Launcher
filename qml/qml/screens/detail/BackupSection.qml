import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

SectionContainer {
    id: backupRoot
    contentSpacing: Dimensions.spacingLG

    // Single ViewModel reference — all backup state accessed via vm
    required property var vm

    property var gameBackups: BackupManager.getBackupsForGame(backupRoot.vm.gameId)
    property bool hasBackups: gameBackups.length > 0
    property var latestBackup: BackupManager.getLatestBackup(backupRoot.vm.gameId)

    // Cached binding — vm.updateImpact.level evaluated once, not per-consumer
    readonly property string _impactLevel: vm.updateImpact ? vm.updateImpact.level : ""

    Connections {
        target: BackupManager
        function onBackupsChanged() {
            backupRoot.gameBackups = BackupManager.getBackupsForGame(backupRoot.vm.gameId)
            backupRoot.latestBackup = BackupManager.getLatestBackup(backupRoot.vm.gameId)
        }
        function onBackupRestored(gId) {
            if (gId === backupRoot.vm.gameId) {
                backupRoot.gameBackups = BackupManager.getBackupsForGame(backupRoot.vm.gameId)
                backupRoot.latestBackup = BackupManager.getLatestBackup(backupRoot.vm.gameId)
            }
        }
    }

    Text {
        textFormat: Text.PlainText
        text: qsTr("Yedekleme Yönetimi")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold; color: Theme.textPrimary
    }

    SettingsDivider { variant: "section" }

    Text {
        textFormat: Text.PlainText
        Layout.fillWidth: true
        text: qsTr("Çeviri uygulamadan önce oyun dosyaları otomatik olarak yedeklenir.")
        font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; wrapMode: Text.WordWrap
    }

    // Restore in progress
    RowLayout {
        visible: BackupManager.isRestoring
        spacing: Dimensions.spacingMD
        BusyIndicator { width: 20; height: 20; running: visible }
        Text {
            textFormat: Text.PlainText
            text: BackupManager.restoreStatus
            font.pixelSize: Dimensions.fontBody
            color: Theme.primary
        }
    }

    // Has backups
    ColumnLayout {
        Layout.fillWidth: true
        visible: backupRoot.hasBackups && !BackupManager.isRestoring
        spacing: Dimensions.spacingLG

        // Latest backup info
        RowLayout {
            Layout.fillWidth: true; spacing: Dimensions.spacingLG

            Rectangle {
                width: 40; height: 40; radius: 20
                color: Theme.success12
                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: "\u2713"
                    font.pixelSize: Dimensions.fontTitle
                    color: Theme.success
                }
            }

            ColumnLayout {
                Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Son Yedek")
                    font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold; color: Theme.textPrimary
                }
                Text {
                    textFormat: Text.PlainText
                    text: {
                        var b = backupRoot.latestBackup
                        if (!b || !b.date) return ""
                        var parts = []
                        parts.push(b.date)
                        if (b.sizeFormatted) parts.push(b.sizeFormatted)
                        if (b.fileCount) parts.push(qsTr("%1 dosya").arg(b.fileCount))
                        return parts.join(" \u2022 ")
                    }
                    font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
                }
            }

            // Count badge
            Rectangle {
                width: countLbl.width + 12; height: 22
                radius: Dimensions.radiusFull
                color: Theme.textPrimary06
                Text {
                    textFormat: Text.PlainText
                    id: countLbl
                    anchors.centerIn: parent
                    text: qsTr("%1 yedek").arg(backupRoot.gameBackups.length)
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }
            }
        }

        // Stale backup warning
        Rectangle {
            Layout.fillWidth: true
            visible: backupRoot.hasBackups
                     && (backupRoot._impactLevel === "broken" || backupRoot._impactLevel === "lost")
            implicitHeight: staleRow.height + 16
            radius: Dimensions.radiusSM
            color: Theme.warning08
            border.color: Theme.warning20; border.width: 1

            Row {
                id: staleRow
                anchors.left: parent.left; anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                spacing: Dimensions.spacingMD
                Text {
                    textFormat: Text.PlainText
                    text: "\u26A0"
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.warning
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Bu yedek eski oyun sürümüne ait. Geri yüklemek oyunu bozabilir.")
                    font.pixelSize: Dimensions.fontCaption; color: Theme.warning
                    wrapMode: Text.WordWrap; width: parent.width - 40
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Buttons
        RowLayout {
            spacing: Dimensions.spacingLG

            // Restore
            Rectangle {
                implicitWidth: restoreRow.width + 32; implicitHeight: 38
                radius: Dimensions.radiusStandard
                color: restoreMouse.containsMouse ? Theme.error20 : Theme.error10
                border.color: Theme.error30; border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Yamayı Kaldır")

                Row {
                    id: restoreRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                    Text {
                        textFormat: Text.PlainText
                        text: "\u2715"
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Yamayı Kaldır")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.DemiBold
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: restoreMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: GameService.uninstallTranslation(backupRoot.vm.gameId)
                }
            }

            // Delete all
            Rectangle {
                implicitWidth: deleteRow.width + 32; implicitHeight: 38
                radius: Dimensions.radiusStandard
                color: deleteMouse.containsMouse ? Theme.textPrimary08 : Theme.textPrimary04
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Yedekleri Sil")

                Row {
                    id: deleteRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Yedekleri Sil")
                        font.pixelSize: Dimensions.fontSM
                        font.weight: Font.Medium
                        color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: deleteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: deleteBackupsConfirm.open()
                }
            }
        }
    }

    // No backups
    Row {
        visible: !backupRoot.hasBackups && !BackupManager.isRestoring
        spacing: Dimensions.spacingLG
        Text {
            textFormat: Text.PlainText
            text: "\u2139"
            font.pixelSize: Dimensions.fontTitle
            color: Theme.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            textFormat: Text.PlainText
            text: qsTr("Bu oyun için henüz yedek bulunmuyor.")
            font.pixelSize: Dimensions.fontBody
            color: Theme.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ===== CONFIRM DIALOG =====
    ConfirmDialog {
        id: deleteBackupsConfirm
        parent: Overlay.overlay
        title: qsTr("Yedekleri Sil")
        message: qsTr("Bu oyuna ait tüm yedek dosyaları kalıcı olarak silinecek. Bu işlem geri alınamaz.")
        confirmText: qsTr("Sil")
        onConfirmed: {
            var all = backupRoot.gameBackups
            for (var i = 0; i < all.length; i++)
                BackupManager.deleteBackup(all[i].id)
        }
    }
}
