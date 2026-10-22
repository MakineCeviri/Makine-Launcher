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
 *                 comes second.
 *   writtenFiles  what landed on disk. Turns the next support thread from
 *                 "it doesn't work" into a file list someone can read.
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
    readonly property int _fileCount: writtenFiles ? writtenFiles.length : 0

    accentColor: _foreignSlot ? Theme.warning : Theme.success

    width: 480
    contentHeight: Math.min(_contentColumn.implicitHeight, 340)
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

        ColumnLayout {
            id: _contentColumn
            width: _scroller.availableWidth
            spacing: Dimensions.spacingMD

            // ===== Derived from the installed files =====
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.preferredHeight: _slotText.implicitHeight + 28
                visible: root.languageSlot !== ""
                radius: Dimensions.radiusLG
                color: Theme.withAlpha(root.accentColor, 0.08)
                border.color: Theme.withAlpha(root.accentColor, 0.20)
                border.width: 1

                Label {
                    id: _slotText
                    anchors.fill: parent
                    anchors.margins: 14
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    font.pixelSize: Dimensions.callout
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    text: root._foreignSlot
                          ? qsTr("Çeviri oyunun %1 dil dosyalarına yazıldı. Görmek için oyunun dilini %1 yapın ve oyunu yeniden başlatın.")
                              .arg(root.languageSlot)
                          : qsTr("Çeviri %1 dil dosyalarına yazıldı.").arg(root.languageSlot)
                }
            }

            // ===== The catalogue note =====
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                visible: root.message !== ""
                textFormat: Text.PlainText
                text: root.message
                font.pixelSize: Dimensions.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }

            // ===== What actually landed =====
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                visible: root._fileCount > 0
                textFormat: Text.PlainText
                font.family: "Consolas"
                font.pixelSize: Dimensions.caption1
                color: Theme.textMuted
                wrapMode: Text.WrapAnywhere
                lineHeight: 1.4
                text: {
                    var shown = root.writtenFiles.slice(0, 6).join("\n")
                    var rest = root._fileCount - 6
                    return qsTr("Yazılan dosyalar (%1):").arg(root._fileCount) + "\n" + shown
                           + (rest > 0 ? "\n+" + rest + " dosya daha" : "")
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
            text: qsTr("Anladım")
            onClicked: root.close()
        }
    }
}
