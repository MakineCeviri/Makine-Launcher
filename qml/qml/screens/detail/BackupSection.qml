import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

ColumnLayout {
    id: backupRoot
    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
    spacing: Dimensions.spacingLG

    // Required properties from parent
    required property string gameId

    property var gameBackups: BackupManager.getBackupsForGame(gameId)
    property bool hasBackups: gameBackups.length > 0
    property var latestBackup: BackupManager.getLatestBackup(gameId)

    Connections {
        target: BackupManager
        function onBackupsChanged() {
            backupRoot.gameBackups = BackupManager.getBackupsForGame(backupRoot.gameId)
            backupRoot.latestBackup = BackupManager.getLatestBackup(backupRoot.gameId)
        }
        function onBackupRestored(gId) {
            if (gId === backupRoot.gameId) {
                backupRoot.gameBackups = BackupManager.getBackupsForGame(backupRoot.gameId)
                backupRoot.latestBackup = BackupManager.getLatestBackup(backupRoot.gameId)
            }
        }
    }

    Text {
        text: qsTr("Yedekleme Yönetimi")
        font.pixelSize: Dimensions.fontTitle; font.weight: Font.DemiBold; color: Theme.textPrimary
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: backupContent.height + Dimensions.marginML * 2
        radius: Dimensions.radiusStandard
        color: Theme.glassBackground; border.color: Theme.glassBorder; border.width: 1

        ColumnLayout {
            id: backupContent
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top; anchors.margins: Dimensions.marginML
            spacing: Dimensions.spacingLG

            Text {
                Layout.fillWidth: true
                text: qsTr("Çeviri uygulamadan önce oyun dosyaları otomatik olarak yedeklenir.")
                font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; wrapMode: Text.WordWrap
            }

            // Restore in progress
            RowLayout {
                visible: BackupManager.isRestoring
                spacing: Dimensions.spacingMD
                BusyIndicator { width: 20; height: 20; running: visible }
                Text { text: BackupManager.restoreStatus; font.pixelSize: Dimensions.fontBody; color: Theme.primary }
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
                        color: Theme.withAlpha(Theme.success, 0.12)
                        Text { anchors.centerIn: parent; text: "\u2713"; font.pixelSize: Dimensions.fontTitle; color: Theme.success }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                        Text {
                            text: qsTr("Son Yedek")
                            font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold; color: Theme.textPrimary
                        }
                        Text {
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
                        color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        Text { id: countLbl; anchors.centerIn: parent; text: qsTr("%1 yedek").arg(backupRoot.gameBackups.length); font.pixelSize: Dimensions.fontCaption; font.weight: Font.Medium; color: Theme.textSecondary }
                    }
                }

                // Buttons
                RowLayout {
                    spacing: Dimensions.spacingLG

                    // Restore
                    Rectangle {
                        implicitWidth: restoreRow.width + 32; implicitHeight: 38
                        radius: Dimensions.radiusStandard
                        color: restoreMouse.containsMouse ? Theme.withAlpha(Theme.warning, 0.20) : Theme.withAlpha(Theme.warning, 0.10)
                        border.color: Theme.withAlpha(Theme.warning, 0.30); border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Orijinale Dön")

                        Row {
                            id: restoreRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                            Text { text: "\u21BB"; font.pixelSize: Dimensions.fontSM; color: Theme.warning; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: qsTr("Orijinale Dön"); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.warning; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea {
                            id: restoreMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var b = backupRoot.latestBackup
                                if (b && b.id) BackupManager.restoreBackup(b.id)
                            }
                        }
                    }

                    // Delete all
                    Rectangle {
                        implicitWidth: deleteRow.width + 32; implicitHeight: 38
                        radius: Dimensions.radiusStandard
                        color: deleteMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.08) : Theme.withAlpha(Theme.textPrimary, 0.04)
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Yedekleri Sil")

                        Row {
                            id: deleteRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                            Text { text: qsTr("Yedekleri Sil"); font.pixelSize: Dimensions.fontSM; font.weight: Font.Medium; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
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
                Text { text: "\u2139"; font.pixelSize: Dimensions.fontTitle; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("Bu oyun için henüz yedek bulunmuyor."); font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
            }
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
