// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * CompatibilityNote — the game version the patch was made for, under the
 * action. When the game is newer than that (the player's installed copy, or
 * else Steam's latest public build), a warning in amber: things may not fit,
 * and a word on Discord helps us fix it. Otherwise one quiet line.
 */
ColumnLayout {
    id: note

    required property var vm

    readonly property var _patch: note.vm.selectedPatch
    readonly property var _gv: note._patch ? note._patch.gameVersion : null
    readonly property string _compat: note.vm.gameVersionText(note._gv)
    readonly property bool _warn: note._patch !== null && note._patch.warn === true
    readonly property bool _fromInstalled: note.vm.gameBuild.source === "installed"

    Layout.fillWidth: true
    spacing: 0
    visible: note.vm.hasTranslation && !note.vm.comingSoon && (note._warn || note._compat !== "")

    // ---- The warning ----
    Rectangle {
        Layout.fillWidth: true
        visible: note._warn
        implicitHeight: warnRow.implicitHeight + 32
        radius: Dimensions.radiusLG
        color: Theme.withAlpha(Theme.warning, Theme.darkMode ? 0.12 : 0.07)
        border.color: Theme.withAlpha(Theme.warning, 0.32)
        border.width: 1

        RowLayout {
            id: warnRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            IconTile {
                Layout.alignment: Qt.AlignTop
                size: 34
                glyph: Icons.triangleAlert
                tint: Theme.warning
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: note._fromInstalled
                          ? qsTr("Oyununuz bu yama hazırlandıktan sonra güncellendi")
                          : qsTr("Oyunun Steam'deki son sürümü bu yamadan yeni")
                    font.pixelSize: Dimensions.headline
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Uyumsuzluklar olabilir. Hata alırsanız Discord'dan bize bildirirseniz seviniriz.")
                    font.pixelSize: Dimensions.callout
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    textFormat: Text.PlainText
                    text: {
                        const parts = []
                        if (note.vm.hasAlternatives)
                            parts.push(note.vm.patchLabel(note._patch))
                        if (note._compat !== "")
                            parts.push(qsTr("yamanın sürümü: %1").arg(note._compat))
                        const v = note.vm.dateText(note._patch ? note._patch.version : "")
                        if (v !== "")
                            parts.push(qsTr("yama tarihi: %1").arg(v))
                        const cur = note.vm.gameBuild
                        if (cur.build > 0)
                            parts.push((note._fromInstalled ? qsTr("oyununuz: %1") : qsTr("Steam'deki: %1"))
                                       .arg(note.vm.gameVersionText({ steamBuild: cur.build, date: cur.date || "" })))
                        return parts.join("  ·  ")
                    }
                    font.pixelSize: Dimensions.footnote
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }

            AppButton {
                Layout.alignment: Qt.AlignVCenter
                style: "filled"
                tint: Theme.discordColor
                icon: Icons.discord
                text: qsTr("Discord'da bildir")
                toolTip: qsTr("Makine Çeviri Discord sunucusunu açar")
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }
        }
    }

    // ---- No warning: the version, quietly ----
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 4
        visible: !note._warn && note._compat !== ""
        spacing: 8
        Icon {
            glyph: Icons.gamepad2
            size: 14
            color: Theme.textMuted
        }
        Text {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            text: (note.vm.hasAlternatives ? note.vm.patchLabel(note._patch) + "  ·  " : "")
                  + qsTr("Uyumlu sürüm: %1").arg(note._compat)
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
    }
}
