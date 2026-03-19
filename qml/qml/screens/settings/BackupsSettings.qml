import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * BackupsSettings.qml - Backup management panel
 */
ColumnLayout {
    spacing: Dimensions.spacingXL

    // Backup list card
    SettingsCard {
        Layout.fillWidth: true

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            // Section header
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56

                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Yedekler")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: Dimensions.marginML
                }
            }

            SettingsDivider {}

            // Empty state
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                Layout.margins: Dimensions.marginML
                radius: Dimensions.radiusMD
                color: Theme.primary04
                visible: BackupManager.backups.length === 0

                // Fade-in + scale on appear
                opacity: 0
                scale: 0.95
                Component.onCompleted: emptyStateAnim.start()
                ParallelAnimation {
                    id: emptyStateAnim
                    NumberAnimation { target: parent; property: "opacity"; from: 0; to: 1; duration: 400; easing.type: Easing.OutCubic }
                    NumberAnimation { target: parent; property: "scale"; from: 0.95; to: 1; duration: 400; easing.type: Easing.OutCubic }
                }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: Dimensions.spacingLG

                    Text {
                        textFormat: Text.PlainText
                        text: "\uD83D\uDCC1"
                        font.pixelSize: Dimensions.headlineLarge
                    }

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Henüz yedeklenmiş çeviri yok")
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textMuted
                    }
                }
            }

            // Backup list
            Repeater {
                model: BackupManager.backups

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    Layout.leftMargin: Dimensions.marginML
                    Layout.rightMargin: Dimensions.marginML
                    Layout.topMargin: Dimensions.spacingSM
                    radius: Dimensions.radiusMD
                    color: backupItemMouse.containsMouse ? Theme.primary06 : Theme.primary04
                    border.color: Theme.primary08
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Dimensions.marginMS
                        spacing: Dimensions.spacingLG

                        // Game icon placeholder
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: Dimensions.radiusMD
                            color: Theme.primary12

                            Text {
                                textFormat: Text.PlainText
                                anchors.centerIn: parent
                                text: modelData.gameName ? modelData.gameName.substring(0, 2).toUpperCase() : "?"
                                font.pixelSize: Dimensions.fontLG
                                font.weight: Font.Bold
                                color: Theme.textMuted
                            }
                        }

                        // Backup info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXXS

                            Text {
                                textFormat: Text.PlainText
                                text: modelData.gameName || qsTr("Bilinmeyen Oyun")
                                font.pixelSize: Dimensions.fontMD
                                font.weight: Font.Medium
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }

                            Text {
                                textFormat: Text.PlainText
                                text: {
                                    var date = new Date(modelData.createdAt)
                                    return date.toLocaleDateString("tr-TR") + " - " +
                                           (modelData.sizeBytes > 1048576
                                            ? (modelData.sizeBytes / 1048576).toFixed(1) + " MB"
                                            : (modelData.sizeBytes / 1024).toFixed(0) + " KB")
                                }
                                font.pixelSize: Dimensions.fontSM
                                color: Theme.textMuted
                            }
                        }

                        // Restore button
                        Rectangle {
                            Layout.preferredWidth: restoreBtnContent.width + 20
                            Layout.preferredHeight: 32
                            radius: Dimensions.radiusMD
                            color: restoreBtnMouse.containsMouse ? Theme.primaryHover : Theme.primary
                            scale: restoreBtnMouse.pressed ? 0.92 : 1.0

                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Restore backup")
                            activeFocusOnTab: true
                            Keys.onReturnPressed: BackupManager.restoreBackup(modelData.id, modelData.originalPath)
                            Keys.onSpacePressed: BackupManager.restoreBackup(modelData.id, modelData.originalPath)

                            Row {
                                id: restoreBtnContent
                                anchors.centerIn: parent
                                spacing: Dimensions.spacingSM

                                Text {
                                    textFormat: Text.PlainText
                                    text: "\u21A9"
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.textOnColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    textFormat: Text.PlainText
                                    text: qsTr("Geri Al")
                                    font.pixelSize: Dimensions.fontSM
                                    font.weight: Font.Medium
                                    color: Theme.textOnColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // Focus indicator
                            FocusRing { offset: -1 }

                            MouseArea {
                                id: restoreBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    BackupManager.restoreBackup(modelData.id, modelData.originalPath)
                                }
                            }
                        }

                        // Delete button
                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            radius: Dimensions.radiusMD
                            color: deleteBtnMouse.containsMouse ? Theme.error15 : "transparent"
                            scale: deleteBtnMouse.pressed ? 0.85 : 1.0

                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Delete backup")
                            activeFocusOnTab: true
                            Keys.onReturnPressed: BackupManager.deleteBackup(modelData.id)
                            Keys.onSpacePressed: BackupManager.deleteBackup(modelData.id)

                            Text {
                                textFormat: Text.PlainText
                                anchors.centerIn: parent
                                text: "\uD83D\uDDD1"
                                font.pixelSize: Dimensions.fontMD
                            }

                            // Focus indicator
                            FocusRing { offset: -1 }

                            MouseArea {
                                id: deleteBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    BackupManager.deleteBackup(modelData.id)
                                }
                            }

                            StyledToolTip {
                                visible: deleteBtnMouse.containsMouse
                                text: qsTr("Yedeği sil")
                                delay: 500
                            }
                        }
                    }

                    MouseArea {
                        id: backupItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                }
            }

            Item { Layout.preferredHeight: Dimensions.marginML }
        }
    }

    // Restore status indicator
    SettingsCard {
        Layout.fillWidth: true
        visible: BackupManager.isRestoring

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            BusyIndicator {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                running: BackupManager.isRestoring
            }

            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: BackupManager.restoreStatus || qsTr("Geri yükleniyor...")
                font.pixelSize: Dimensions.fontMD
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
        }
    }
}
