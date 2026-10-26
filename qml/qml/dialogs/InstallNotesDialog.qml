// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * InstallNotesDialog.qml - The last install step, when that step is the user's
 *
 * Field report: "Far Cry 6 yama kurulu görünüyor ama oyun hâlâ İngilizce." The
 * patch had installed correctly. What was missing was a sentence, and the
 * launcher had nowhere to say it.
 *
 * Three things go in here, most reliable first:
 *
 *   languageSlot  derived from the files the install actually wrote. Curse of
 *                 the Dead Gods writes the French files, Alan Wake 2 the
 *                 English ones — neither catalogue note says so, and the game
 *                 shows nothing until it is switched to that language. This
 *                 line cannot contradict the package because it is read off it.
 *   message       the catalogue note. Authored prose, so it can be wrong, so it
 *                 comes second. Laid out to be read: one block per paragraph,
 *                 a paragraph that warns (anti-cheat, online, "dikkat") in an
 *                 amber callout, the arrows of a menu path ("Settings →
 *                 General") in the accent.
 *   writtenFiles  what landed on disk, folded away until asked for. Turns the
 *                 next support thread from "it doesn't work" into a file list
 *                 someone can read. The launcher's own ".makine" record is not
 *                 the user's business and is left out.
 *
 * Shown once, right after a successful install, and only when there is
 * something to do — see makine::postinstall::shouldShowAfterInstall.
 */
BaseDialog {
    id: root

    property string message: ""
    property string languageSlot: ""
    property var writtenFiles: []

    // A Turkish slot means the default already works; the caller only sends us
    // here for a foreign one, and then the language line is the whole point.
    readonly property bool _foreignSlot: languageSlot !== "" && languageSlot !== "Türkçe"
    readonly property var _files: (root.writtenFiles || []).filter(
        (f) => !String(f).split(/[\\/]/).pop().startsWith(".makine"))
    readonly property int _fileCount: root._files.length
    property bool _filesOpen: false

    // The note in paragraphs; a warning one is marked
    readonly property var _paragraphs: String(root.message).split(/\n\s*\n/)
        .map((p) => p.trim()).filter((p) => p !== "")
        .map((p) => ({ text: p, warn: /(battleye|easyanticheat|anti.?cheat|online|dikkat|uyarı|çalışmayabilir|\bban)/i.test(p) }))

    // Plain text made safe for StyledText, the arrows of a path in the accent
    function _styled(text) {
        const esc = String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        return esc.replace(/\n/g, "<br>")
                  .replace(/\s*(→|->)\s*/g, " <b><font color=\"" + Theme.primaryText + "\">→</font></b> ")
    }

    accentColor: _foreignSlot ? Theme.warning : Theme.success

    width: 540
    contentHeight: Math.min(_contentColumn.implicitHeight, 380)
    title: _foreignSlot ? qsTr("Yama kuruldu — oyunun dilini değiştirin")
                        : qsTr("Yama kuruldu — son bir adım var")

    header: SheetHeader {
        glyph: root._foreignSlot ? Icons.languages : Icons.circleCheck
        tint: root.accentColor
        title: root.title
        subtitle: qsTr("Bu bilgi oyunun sayfasındaki \"Yama notları\" bölümünde de durur.")
        onCloseClicked: { root.cancelled(); root.close() }
    }

    // One scroller for everything: notes run to numbered lists (Kenshi, Skyrim,
    // The Sims 4) and an instruction cut in half is the failure this dialog
    // exists to prevent.
    contentItem: ScrollView {
        id: _scroller
        clip: true
        // Only down: the column's implicit width is its longest line unwrapped,
        // and a ScrollView sized to it scrolled the notes sideways, off the left
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: _contentColumn
            width: _scroller.availableWidth
            spacing: 12

            // ===== Derived from the installed files =====
            Callout {
                visible: root.languageSlot !== ""
                glyph: Icons.languages
                tint: root.accentColor
                order: 0
                text: root._foreignSlot
                      ? qsTr("Çeviri oyunun <b>%1</b> dil dosyalarına yazıldı. Görmek için oyunun dilini %1 yapın ve oyunu yeniden başlatın.")
                          .arg(root.languageSlot)
                      : qsTr("Çeviri <b>%1</b> dil dosyalarına yazıldı.").arg(root.languageSlot)
            }

            // ===== The catalogue note, paragraph by paragraph =====
            Repeater {
                model: root._paragraphs
                delegate: Callout {
                    required property var modelData
                    required property int index
                    plain: !modelData.warn
                    glyph: Icons.triangleAlert
                    tint: Theme.warning
                    order: index + 1
                    text: root._styled(modelData.text)
                }
            }

            // ===== What actually landed, folded =====
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                visible: root._fileCount > 0
                spacing: 8

                Row {
                    spacing: 6
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: Icons.chevronRight
                        size: 13
                        color: Theme.textMuted
                        rotation: root._filesOpen ? 90 : 0
                        Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: qsTr("Yazılan dosyalar (%1)").arg(root._fileCount)
                        font.pixelSize: Dimensions.footnote
                        font.weight: Font.DemiBold
                        color: filesHover.hovered ? Theme.textSecondary : Theme.textMuted
                    }
                    HoverHandler { id: filesHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root._filesOpen = !root._filesOpen }
                }
                Rectangle {
                    Layout.fillWidth: true
                    visible: root._filesOpen
                    implicitHeight: filesText.implicitHeight + 20
                    radius: Dimensions.radiusMD
                    color: Theme.fillQuaternary
                    Label {
                        id: filesText
                        anchors.fill: parent
                        anchors.margins: 10
                        textFormat: Text.PlainText
                        font.family: "Cascadia Mono"
                        font.pixelSize: Dimensions.caption1
                        color: Theme.textMuted
                        wrapMode: Text.WrapAnywhere
                        lineHeight: 1.35
                        text: {
                            const shown = root._files.slice(0, 12).join("\n")
                            const rest = root._fileCount - 12
                            return shown + (rest > 0 ? "\n" + qsTr("+%1 dosya daha").arg(rest) : "")
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Dimensions.spacingXS }
        }
    }

    footer: Item {
        implicitHeight: 76

        AppButton {
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            style: "filled"
            icon: Icons.check
            text: qsTr("Anladım")
            onClicked: root.close()
        }
    }

    // ── Parts ────────────────────────────────────────────────────────────

    // A paragraph of the note, rising in after the one before; one that needs
    // attention (not `plain`) on a wash of its colour, with its sign
    component Callout: Rectangle {
        id: callout
        property string glyph: ""
        property color tint: Theme.warning
        property string text: ""
        property int order: 0
        property bool plain: false
        Layout.fillWidth: true
        Layout.leftMargin: 22
        Layout.rightMargin: 22
        implicitHeight: calloutRow.implicitHeight + (callout.plain ? 0 : 26)
        radius: Dimensions.radiusLG
        color: callout.plain ? "transparent" : Theme.withAlpha(callout.tint, 0.09)
        border.color: Theme.withAlpha(callout.tint, 0.24)
        border.width: callout.plain ? 0 : 1
        opacity: 0
        transform: Translate { id: calloutRise; y: 8 }
        Component.onCompleted: calloutIn.start()
        SequentialAnimation {
            id: calloutIn
            PauseAnimation { duration: 120 + callout.order * 70 }
            ParallelAnimation {
                NumberAnimation { target: callout; property: "opacity"; to: 1; duration: Dimensions.animNormal; easing.type: Easing.OutCubic }
                NumberAnimation { target: calloutRise; property: "y"; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutExpo }
            }
            ScriptAction { script: if (!callout.plain) calloutTile.play() }
        }

        RowLayout {
            id: calloutRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: callout.plain ? 0 : 14
            anchors.rightMargin: callout.plain ? 0 : 14
            spacing: 12
            IconTile {
                id: calloutTile
                visible: !callout.plain
                Layout.alignment: Qt.AlignTop
                size: 26
                glyph: callout.glyph
                tint: callout.tint
            }
            Label {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                font.pixelSize: callout.plain ? Dimensions.body : Dimensions.callout
                font.weight: callout.plain ? Font.Normal : Font.Medium
                color: callout.plain ? Theme.textSecondary : Theme.textPrimary
                text: callout.text
            }
        }
    }
}
