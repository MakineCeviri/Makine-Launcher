// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * ProgramConsole — what a patch's own program prints while it installs.
 *
 * Shown once a "run" step has written something, and kept after the install
 * ends so a failure can be read. While the program runs, the line below passes
 * typed answers to its prompts ("set /p", "press a key"); an empty line is the
 * key press.
 */
Rectangle {
    id: programConsole

    required property var vm

    readonly property bool _acceptsInput: programConsole.vm.isInstallingTranslation
                                          && !programConsole.vm.isDownloading
    property bool _copied: false

    function _send() {
        GameService.sendInstallInput(inputField.text)
        inputField.text = ""
    }

    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
    Layout.topMargin: Dimensions.spacingXL
    visible: programConsole.vm.installConsole !== ""
    implicitHeight: consoleContent.implicitHeight + Dimensions.marginML * 2
    radius: Dimensions.radiusLG
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    Timer {
        id: copiedTimer
        interval: 2000
        onTriggered: programConsole._copied = false
    }

    ColumnLayout {
        id: consoleContent
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.margins: Dimensions.marginML
        spacing: Dimensions.spacingLG

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            Text {
                textFormat: Text.PlainText
                text: qsTr("Program çıktısı")
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            Text {
                textFormat: Text.PlainText
                text: programConsole._copied ? qsTr("Kopyalandı") : qsTr("Tümünü kopyala")
                font.pixelSize: Dimensions.fontSM
                color: copyMouse.containsMouse ? Theme.textPrimary : Theme.textMuted

                MouseArea {
                    id: copyMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        outputArea.selectAll()
                        outputArea.copy()
                        outputArea.deselect()
                        programConsole._copied = true
                        copiedTimer.restart()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 220
            radius: Dimensions.radiusMD
            color: Theme.bgPrimary
            border.color: Theme.primary08
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
                placeholderText: qsTr("Program bir şey sorarsa yanıtı yazıp Enter'a basın")
                placeholderTextColor: Theme.textMuted
                selectByMouse: true
                onAccepted: programConsole._send()
                background: Rectangle {
                    radius: Dimensions.radiusMD
                    color: Theme.primary06
                    border.color: inputField.activeFocus ? Theme.primary : Theme.primary12
                    border.width: 1
                }
            }

            Rectangle {
                implicitWidth: sendLabel.implicitWidth + 28
                implicitHeight: 34
                radius: Dimensions.radiusMD
                color: sendMouse.containsMouse ? Theme.primary12 : Theme.primary08

                Text {
                    id: sendLabel
                    anchors.centerIn: parent
                    textFormat: Text.PlainText
                    // Empty: the key press a "Devam etmek için bir tuşa basın" waits for.
                    text: inputField.text === "" ? qsTr("Enter") : qsTr("Gönder")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                MouseArea {
                    id: sendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: programConsole._send()
                }
            }
        }
    }
}
