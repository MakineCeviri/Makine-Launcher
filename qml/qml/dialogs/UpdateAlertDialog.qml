import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * UpdateAlertDialog.qml - Startup dialog showing games with broken translations
 *
 * Shows games with broken or outdated translations, with per-game repair action
 * with per-game repair and "Repair All" bulk action.
 */
BaseDialog {
    id: root

    property var affectedGames: []  // [{gameId, gameName, impact}]

    function addAffectedGame(gameId, gameName, impact) {
        // Avoid duplicates
        for (var i = 0; i < affectedGames.length; i++) {
            if (affectedGames[i].gameId === gameId) return
        }
        var copy = affectedGames.slice()
        copy.push({gameId: gameId, gameName: gameName, impact: impact})
        affectedGames = copy
    }

    accentColor: Theme.warning
    width: 480
    contentHeight: contentCol.implicitHeight

    header: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG; anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 16
                color: Theme.warning10
                border.color: Theme.warning20; border.width: 1
                Text {
                    textFormat: Text.PlainText
                    anchors.centerIn: parent
                    text: "\u26A0"
                    font.pixelSize: 14
                    color: Theme.warning
                }
            }

            Label {
                textFormat: Text.PlainText
                text: qsTr("Çeviri Güncellemeleri Gerekli")
                font.pixelSize: Dimensions.fontLG; font.weight: Font.DemiBold; color: Theme.textPrimary
                elide: Text.ElideRight; Layout.fillWidth: true
            }

            DialogCloseButton { onClicked: root.close() }
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: Theme.textPrimary06
        }
    }

    contentItem: ColumnLayout {
        id: contentCol
        spacing: Dimensions.spacingMD

        Item { Layout.preferredHeight: Dimensions.spacingXS }

        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.leftMargin: Dimensions.paddingLG; Layout.rightMargin: Dimensions.paddingLG
            text: qsTr("Aşağıdaki oyunlar güncellendi ve çevirileri etkilenmiş olabilir.")
            font.pixelSize: Dimensions.fontSM; color: Theme.textSecondary
            wrapMode: Text.WordWrap; lineHeight: 1.5
        }

        // Game list
        Repeater {
            model: root.affectedGames

            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.paddingLG; Layout.rightMargin: Dimensions.paddingLG
                implicitHeight: gameRow.height + 20
                radius: Dimensions.radiusStandard
                color: modelData.impact.level === "broken" ? Theme.error06 : Theme.warning06
                border.color: modelData.impact.level === "broken" ? Theme.error15 : Theme.warning15
                border.width: 1

                RowLayout {
                    id: gameRow
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 10
                    spacing: Dimensions.spacingMD

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Text {
                            textFormat: Text.PlainText
                            text: modelData.gameName
                            font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.textPrimary
                            elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        Text {
                            textFormat: Text.PlainText
                            text: modelData.impact.summary || ""
                            font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted
                            elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        implicitWidth: repairLbl.width + 24; implicitHeight: 30
                        radius: Dimensions.radiusStandard
                        color: repairBtnMa.containsMouse ? Theme.accent20 : Theme.accent10
                        border.color: Theme.accent25; border.width: 1
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        Text {
                            textFormat: Text.PlainText
                            id: repairLbl; anchors.centerIn: parent
                            text: qsTr("Onar"); font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold; color: Theme.accent
                        }
                        MouseArea {
                            id: repairBtnMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: GameService.recoverTranslation(modelData.gameId)
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: Dimensions.spacingXS }
    }

    footer: Item {
        implicitHeight: 56

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 1; color: Theme.textPrimary06
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.paddingLG; anchors.rightMargin: Dimensions.paddingLG
            spacing: Dimensions.spacingMD

            Item { Layout.fillWidth: true }

            // Close
            Rectangle {
                Layout.preferredWidth: closeLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34; radius: Dimensions.radiusStandard
                color: closeFooterMa.containsMouse ? Theme.textPrimary08 : "transparent"
                border.color: Theme.textPrimary12; border.width: 1
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Label {
                    textFormat: Text.PlainText
                    id: closeLbl
                    anchors.centerIn: parent
                    text: qsTr("Kapat")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.Medium
                    color: Theme.textSecondary
                }
                MouseArea { id: closeFooterMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.close() }
            }

            // Repair All
            Rectangle {
                visible: root.affectedGames.length > 1
                Layout.preferredWidth: repairAllLbl.width + Dimensions.paddingLG * 2
                Layout.preferredHeight: 34; radius: Dimensions.radiusStandard
                color: repairAllMa.containsMouse ? Theme.accent : Theme.accent85
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Label {
                    textFormat: Text.PlainText
                    id: repairAllLbl
                    anchors.centerIn: parent
                    text: qsTr("Tümünü Onar")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }
                MouseArea {
                    id: repairAllMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        for (var i = 0; i < root.affectedGames.length; i++)
                            GameService.recoverTranslation(root.affectedGames[i].gameId)
                        root.close()
                    }
                }
            }
        }
    }
}
