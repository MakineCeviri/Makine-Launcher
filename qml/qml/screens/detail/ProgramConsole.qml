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
 * ends so a failure can be read. A terminal, made readable: what was typed
 * (">" lines and "↵") in the accent, errors in red, warnings in amber, done
 * lines in green, a caret blinking at the end while the program runs. A
 * "press any key to continue" pause is answered by itself (GameService,
 * processconsole::waitsForKey); the line below passes typed answers to real
 * questions ("set /p"), and an empty line is a key press.
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

    // The output as rich text: one span per line, coloured by what it says
    function _html(text) {
        const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        const tone = (c) => String(c)
        const lines = String(text).split("\n").map((line) => {
            const e = esc(line)
            if (/^\s*>/.test(line) || line.trim() === "↵" || /↵$/.test(line))
                return "<span style=\"color:" + tone(Theme.primaryText) + "\">" + e + "</span>"
            if (/(error|hata|failed|başarısız|bulunamad|not found|denied|reddedildi)/i.test(line))
                return "<span style=\"color:" + tone(Theme.error) + "\">" + e + "</span>"
            if (/(warning|uyarı|dikkat)/i.test(line))
                return "<span style=\"color:" + tone(Theme.warning) + "\">" + e + "</span>"
            if (/(\bok\b|tamam|başarı|success|\bdone\b|bitti|kuruldu|installed|tamamlandı)/i.test(line))
                return "<span style=\"color:" + tone(Theme.success) + "\">" + e + "</span>"
            return e
        })
        return "<pre style=\"white-space:pre-wrap; margin:0; font-family:'Cascadia Mono','Consolas',monospace\">"
               + lines.join("\n") + "</pre>"
    }

    Layout.fillWidth: true
    Layout.leftMargin: Dimensions.pageMargin; Layout.rightMargin: Dimensions.pageMargin
    Layout.maximumWidth: 1240
    Layout.alignment: Qt.AlignHCenter
    Layout.topMargin: 18
    visible: programConsole.vm.installConsole !== ""
    implicitHeight: consoleContent.implicitHeight + 40
    radius: Dimensions.radiusSection

    Timer {
        id: copiedTimer
        interval: 2000
        onTriggered: programConsole._copied = false
    }
    // Copies the plain text, not the colours
    TextEdit {
        id: copyHelper
        visible: false
        textFormat: TextEdit.PlainText
    }

    ColumnLayout {
        id: consoleContent
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: Dimensions.spacingLG

            IconTile {
                size: 34
                glyph: Icons.terminal
                tint: programConsole._acceptsInput ? Theme.primary : Theme.textMuted
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    spacing: 10
                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Yamanın kendi programı")
                        font.pixelSize: Dimensions.headline
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                    // Running, with a breathing dot — or done
                    Rectangle {
                        implicitWidth: stateRow.implicitWidth + 16
                        implicitHeight: 22
                        radius: 11
                        color: Theme.withAlpha(programConsole._acceptsInput ? Theme.primary : Theme.success, 0.14)
                        Row {
                            id: stateRow
                            anchors.centerIn: parent
                            spacing: 6
                            Rectangle {
                                id: liveDot
                                anchors.verticalCenter: parent.verticalCenter
                                width: 7; height: 7; radius: 3.5
                                color: programConsole._acceptsInput ? Theme.primary : Theme.success
                                SequentialAnimation on opacity {
                                    running: programConsole._acceptsInput && programConsole.visible && Dimensions.animSlow > 0
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.25; duration: 700; easing.type: Easing.InOutSine }
                                    NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
                                    onStopped: liveDot.opacity = 1
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                                text: programConsole._acceptsInput ? qsTr("Çalışıyor") : qsTr("Bitti")
                                font.pixelSize: Dimensions.caption1
                                font.family: Dimensions.fontText
                                font.weight: Font.DemiBold
                                color: programConsole._acceptsInput ? Theme.primaryText : Theme.success
                            }
                        }
                    }
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
                    copyHelper.text = programConsole.vm.installConsole
                    copyHelper.selectAll()
                    copyHelper.copy()
                    programConsole._copied = true
                    copiedTimer.restart()
                }
            }
        }

        // The terminal
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            radius: Dimensions.radiusLG
            color: Theme.darkMode ? "#09090C" : "#F4F4F6"
            border.color: Theme.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Theme.separator
            border.width: 1

            // Lit along the top, like the containers
            Rectangle {
                visible: Theme.darkMode
                x: Dimensions.radiusLG
                width: parent.width - Dimensions.radiusLG * 2
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.12) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            // Reading old lines: do not jump to the newest one under the cursor.
            HoverHandler { id: outputHover }

            ScrollView {
                id: outputScroll
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                ScrollBar.vertical: StyledScrollBar {}

                TextArea {
                    id: outputArea
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    text: programConsole._html(programConsole.vm.installConsole)
                    font.family: "Cascadia Mono"
                    font.pixelSize: Dimensions.fontCaption
                    color: Theme.darkMode ? "#D4D4DC" : Theme.textSecondary
                    selectionColor: Theme.primary40
                    leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8
                    background: null

                    onTextChanged: Qt.callLater(function() {
                        if (outputHover.hovered) return
                        var bar = outputScroll.ScrollBar.vertical
                        bar.position = Math.max(0, 1.0 - bar.size)
                    })

                    // A caret blinking after the last character while the program runs
                    Rectangle {
                        readonly property rect _at: outputArea.positionToRectangle(outputArea.length)
                        visible: programConsole._acceptsInput
                        x: _at.x + 2
                        y: _at.y + 2
                        width: 7
                        height: Math.max(8, _at.height - 4)
                        radius: 1
                        color: Theme.primary
                        SequentialAnimation on opacity {
                            running: programConsole._acceptsInput && programConsole.visible
                            loops: Animation.Infinite
                            NumberAnimation { to: 0; duration: 80 }
                            PauseAnimation { duration: 450 }
                            NumberAnimation { to: 1; duration: 80 }
                            PauseAnimation { duration: 450 }
                        }
                    }
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
                implicitHeight: 38
                leftPadding: 30
                rightPadding: 12
                font.family: "Cascadia Mono"
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                placeholderText: qsTr("Program bir şey sorarsa yanıtını yaz")
                placeholderTextColor: Theme.textMuted
                selectByMouse: true
                onAccepted: programConsole._send()
                background: Rectangle {
                    radius: Dimensions.radiusLG
                    color: inputField.activeFocus ? Theme.fillSecondary : Theme.fillTertiary
                    border.color: inputField.activeFocus ? Theme.withAlpha(Theme.primary, 0.6) : "transparent"
                    border.width: 1.5
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    // The prompt
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: "›"
                        font.pixelSize: Dimensions.headline
                        font.weight: Font.Bold
                        color: Theme.primaryText
                    }
                }
            }

            AppButton {
                style: "filled"
                icon: Icons.send
                // Empty: the key press a prompt waits for.
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

        // What needs no typing
        RowLayout {
            visible: programConsole._acceptsInput
            spacing: 7
            Icon {
                glyph: Icons.keyboard
                size: 13
                color: Theme.textMuted
            }
            Text {
                textFormat: Text.PlainText
                text: qsTr("\"Devam etmek için bir tuşa basın\" gibi bekleyişler kendiliğinden geçilir; yalnız soruları yanıtla.")
                font.pixelSize: Dimensions.footnote
                font.family: Dimensions.fontText
                color: Theme.textMuted
            }
        }
    }
}
