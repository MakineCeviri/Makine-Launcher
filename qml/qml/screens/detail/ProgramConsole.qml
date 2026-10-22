// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * ProgramConsole — what a patch's own program prints while it installs.
 *
 * Shown once a "run" step has written something, and kept after the install
 * ends so a failure can be read. While the program runs, the line below passes
 * typed answers to its prompts ("set /p", "press a key"); an empty line is the
 * key press.
 */
Card {
    id: programConsole

    required property var vm

    readonly property bool _acceptsInput: (programConsole.vm.isInstallingTranslation
                                           && !programConsole.vm.isDownloading)
                                          || programConsole.vm.isUndoing
    property bool _copied: false

    function _send() {
        GameService.sendInstallInput(inputField.text)
        inputField.text = ""
    }

    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.pageMargin; Layout.rightMargin: Dimensions.pageMargin
    Layout.maximumWidth: 1240
    Layout.alignment: Qt.AlignHCenter
    Layout.topMargin: 18
    visible: programConsole.vm.installConsole !== ""
    implicitHeight: consoleContent.implicitHeight + 40
    radius: Dimensions.radiusLG

    Timer {
        id: copiedTimer
        interval: 2000
        onTriggered: programConsole._copied = false
    }

    ColumnLayout {
        id: consoleContent
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            IconTile { size: 26; glyph: Icons.terminal; tint: Theme.textMuted }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Yamanın kendi programı")
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Bu yama kurulumu kendi programıyla yapıyor; yazdıkları burada. Bir şey sorarsa yanıtı aşağıya yaz.")
                    wrapMode: Text.WordWrap
                    font.pixelSize: Dimensions.footnote
                    color: Theme.textMuted
                }
            }

            AppButton {
                size: "small"
                style: "gray"
                icon: programConsole._copied ? Icons.check : Icons.copy
                text: programConsole._copied ? qsTr("Kopyalandı") : qsTr("Tümünü kopyala")
                onClicked: {
                    outputArea.selectAll()
                    outputArea.copy()
                    outputArea.deselect()
                    programConsole._copied = true
                    copiedTimer.restart()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            radius: Dimensions.radiusMD
            color: Theme.darkMode ? "#0E0E10" : "#F2F2F4"
            border.color: Theme.separator
            border.width: 1

            // Reading old lines: do not jump to the newest one under the cursor.
            HoverHandler { id: outputHover }

            ScrollView {
                id: outputScroll
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                ScrollBar.vertical: StyledScrollBar {}

                TextArea {
                    id: outputArea
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    text: programConsole.vm.installConsole
                    font.family: "Consolas"
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.textSecondary
                    background: null

                    onTextChanged: Qt.callLater(function() {
                        if (outputHover.hovered) return
                        var bar = outputScroll.ScrollBar.vertical
                        bar.position = Math.max(0, 1.0 - bar.size)
                    })
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: programConsole._acceptsInput
            spacing: Dimensions.spacingMD

            TextField {
                id: inputField
                Layout.fillWidth: true
                implicitHeight: 34
                leftPadding: 10
                rightPadding: 10
                font.family: "Consolas"
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                placeholderText: qsTr("Program bir şey sorarsa yanıtı yazıp Enter'a bas")
                placeholderTextColor: Theme.textMuted
                selectByMouse: true
                onAccepted: programConsole._send()
                background: Rectangle {
                    radius: Dimensions.radiusMD
                    color: Theme.fillTertiary
                    border.color: inputField.activeFocus ? Theme.withAlpha(Theme.primary, 0.6) : "transparent"
                    border.width: 2
                }
            }

            AppButton {
                style: "filled"
                icon: Icons.send
                // Empty: the key press a "Devam etmek için bir tuşa basın" waits for.
                text: inputField.text === "" ? qsTr("Enter") : qsTr("Gönder")
                onClicked: programConsole._send()
            }

            // An install stops from its progress pill; the undo an uninstall
            // runs has none, and the patch stays installed when it stops.
            AppButton {
                visible: programConsole.vm.isUndoing
                style: "gray"
                icon: Icons.x
                text: qsTr("Durdur")
                toolTip: qsTr("Kaldırma programını durdur; yama kurulu kalır")
                onClicked: GameService.cancelInstallation()
            }
        }
    }
}
