// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeNotice — what the running-game watcher has to say, shown only when it
 * has something: a supported game is running (patching waits until it is
 * closed), or a game-like process is running that the catalogue did not
 * recognise (the user can point at it).
 */
Rectangle {
    id: notice

    readonly property bool _watching: SettingsManager.autoDetectGames
    readonly property bool _running: notice._watching && ProcessScanner.gameRunning
    readonly property var _candidates: notice._watching && !notice._running
                                       ? ProcessScanner.heavyProcesses : []

    visible: notice._running || notice._candidates.length > 0
    implicitHeight: body.implicitHeight + Dimensions.paddingLG * 2
    radius: Dimensions.radiusLG
    color: notice._running ? Theme.warningBg : Theme.textPrimary04
    border.width: 1
    border.color: notice._running ? Theme.withAlpha(Theme.warning, 0.3) : Theme.textPrimary08

    Row {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Dimensions.paddingLG + 4
        anchors.rightMargin: Dimensions.paddingLG
        spacing: Dimensions.spacingLG

        Text {
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: notice._running ? "" : ""
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 18
            color: notice._running ? Theme.warning : Theme.textSecondary
        }

        Column {
            width: body.width - 18 - body.spacing
            spacing: Dimensions.spacingXS

            Text {
                width: parent.width
                textFormat: Text.PlainText
                text: notice._running
                    ? qsTr("%1 çalışıyor").arg(ProcessScanner.runningGameName)
                    : qsTr("Hangi oyunu oynuyorsun?")
                elide: Text.ElideRight
                font.pixelSize: Dimensions.fontMD
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                width: parent.width
                textFormat: Text.PlainText
                text: notice._running
                    ? qsTr("Türkçe yamayı kurmak ya da kaldırmak için önce oyunu kapat.")
                    : qsTr("Açık bir oyun var ama hangisi olduğunu tanıyamadık. Aşağıdan seçersen eşleştiririz.")
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
            }

            Flow {
                width: parent.width
                visible: notice._candidates.length > 0
                topPadding: Dimensions.spacingXS
                spacing: Dimensions.spacingSM

                Repeater {
                    model: notice._candidates

                    Rectangle {
                        id: chip
                        required property var modelData
                        width: chipLabel.implicitWidth + Dimensions.paddingLG * 1.5
                        height: 28
                        radius: 14
                        color: chipHover.hovered ? Theme.textPrimary12 : Theme.textPrimary06
                        Accessible.role: Accessible.Button
                        Accessible.name: chip.modelData.name

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            textFormat: Text.PlainText
                            text: chip.modelData.name
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.Medium
                            color: Theme.textPrimary
                        }
                        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: ProcessScanner.resolveSelectedProcess(chip.modelData.pid) }
                    }
                }
            }
        }
    }
}
