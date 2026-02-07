import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import MakineAI 1.0

Rectangle {
    id: root
    property var modelData: ({}) // gameName, createdAt, sizeBytes, id, originalPath

    Layout.fillWidth: true
    Layout.preferredHeight: 72
    color: mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"
    radius: Dimensions.radiusStandard

    Behavior on color { ColorAnimation { duration: 150 } }

    Accessible.role: Accessible.ListItem
    Accessible.name: root.modelData.gameName || ""

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // Left item (Game icon)
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            radius: Dimensions.radiusStandard
            color: Theme.surfaceActive

            Text {
                anchors.centerIn: parent
                text: root.modelData.gameName ? root.modelData.gameName.substring(0, 2).toUpperCase() : "?"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: Theme.textMuted
            }
        }

        // Content (Backup info)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: root.modelData.gameName || qsTr("Bilinmeyen Oyun")
                font.pixelSize: 14
                font.weight: Font.Medium
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Text {
                text: {
                    var date = new Date(root.modelData.createdAt)
                    return date.toLocaleDateString("tr-TR") + " - " +
                           (root.modelData.sizeBytes > 1048576
                            ? (root.modelData.sizeBytes / 1048576).toFixed(1) + " MB"
                            : (root.modelData.sizeBytes / 1024).toFixed(0) + " KB")
                }
                font.pixelSize: 12
                color: Theme.textMuted
            }
        }

        // Right item (Buttons)
        RowLayout {
            spacing: 8

            // Restore button
            Button {
                id: restoreBtn
                text: qsTr("Geri Al")
                Layout.preferredHeight: 32
                activeFocusOnTab: true

                contentItem: RowLayout {
                    spacing: 6
                    Image {
                        source: "qrc:/qt/qml/MakineAI/resources/icons/rotate-ccw.svg"
                        sourceSize: Qt.size(14, 14)
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Text {
                        text: restoreBtn.text
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: Theme.textPrimary
                    }
                }

                background: Rectangle {
                    radius: Dimensions.radiusStandard
                    color: restoreBtn.hovered ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1
                }

                onClicked: {
                    BackupManager.restoreBackup(root.modelData.id, root.modelData.originalPath)
                }
            }

            // Delete button
            Button {
                id: deleteBtn
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                activeFocusOnTab: true
                Accessible.name: qsTr("Delete backup")

                contentItem: Image {
                    source: "qrc:/qt/qml/MakineAI/resources/icons/trash.svg"
                    sourceSize: Qt.size(14, 14)
                    anchors.centerIn: parent
                }

                background: Rectangle {
                    radius: Dimensions.radiusStandard
                    color: deleteBtn.hovered ? Theme.withAlpha(Theme.error, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                }

                onClicked: {
                    BackupManager.deleteBackup(root.modelData.id)
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Yedeği sil")
                ToolTip.delay: 500
            }
        }
    }
}
