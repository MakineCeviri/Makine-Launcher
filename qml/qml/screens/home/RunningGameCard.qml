// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * RunningGameCard — what the process watcher sees right now (only while
 * "Çalıştırdığın oyunu tanı" is on): a game that is running, or a few heavy
 * programs one of which may be a game — the user picks it.
 */
Card {
    id: card

    signal libraryRequested()

    readonly property bool _on: SettingsManager.autoDetectGames
    readonly property bool gameDetected: _on && ProcessScanner.gameRunning
    readonly property var _heavy: ProcessScanner.heavyProcesses
    readonly property bool candidates: _on && !gameDetected && _heavy.length > 0

    visible: gameDetected || candidates
    implicitHeight: col.implicitHeight + 32

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            IconTile {
                size: 40
                glyph: card.gameDetected ? Icons.gamepad2 : Icons.scanSearch
                tint: card.gameDetected ? Theme.success : Theme.primary
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: card.gameDetected ? qsTr("%1 çalışıyor").arg(ProcessScanner.runningGameName)
                                            : qsTr("Bir oyun çalışıyor olabilir")
                    elide: Text.ElideRight
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: card.gameDetected
                          ? qsTr("Türkçe yamayı kurmak ya da kaldırmak için önce oyunu kapat; açıkken dosyaları kilitli olur.")
                          : qsTr("Çok kaynak kullanan şu programlardan biri oyunsa üstüne tıkla; hangi oyun olduğunu bulup kütüphanene ekleriz.")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.callout
                    color: Theme.textMuted
                }
            }
            AppButton {
                icon: Icons.gamepad2
                visible: card.gameDetected
                style: "gray"
                text: qsTr("Kütüphaneye git")
                onClicked: card.libraryRequested()
            }
        }

        Flow {
            Layout.fillWidth: true
            visible: card.candidates
            spacing: 8

            Repeater {
                model: card._heavy
                AppButton {
                    required property var modelData
                    size: "small"
                    style: "gray"
                    icon: Icons.appWindow
                    text: modelData.name
                    onClicked: ProcessScanner.resolveSelectedProcess(modelData.pid)
                }
            }
        }
    }
}
