// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * PatchPicker — a game with more than one Turkish patch: the default first,
 * then the others in the catalogue's order. Each says what it is, who made
 * it, its date, the game version it was made for and its size; the installed
 * one is marked. Picking one shows what can be done with it: install it,
 * switch to it from the installed one, or update it.
 */
Card {
    id: picker

    required property var vm

    signal installRequested(string pkg)
    signal switchRequested(string pkg)
    signal updateRequested()

    readonly property var _sel: picker.vm.selectedPatch
    readonly property string _action: picker._sel ? picker._sel.action : ""
    readonly property bool _busy: picker.vm.isInstallingTranslation || picker.vm.isDownloading
                                  || picker.vm.isUndoing

    Layout.fillWidth: true
    visible: picker.vm.hasAlternatives && !picker.vm.comingSoon
    implicitHeight: col.implicitHeight
    radius: Dimensions.radiusLG

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0

        // ---- Heading ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.bottomMargin: 12
            spacing: 12

            IconTile { size: 26; glyph: Icons.layers; tint: Theme.primary }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Yamalar")
                    font.pixelSize: Dimensions.title3
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Text {
                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: qsTr("Bu oyun için %1 Türkçe yama var. Oyunda aynı anda yalnız biri kurulu olur.")
                              .arg(picker.vm.patchOptions.length)
                    font.pixelSize: Dimensions.callout
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ---- One row per patch ----
        Repeater {
            model: picker.vm.patchOptions

            delegate: Item {
                id: option
                required property var modelData
                required property int index

                readonly property bool selected: option.modelData.pkg === picker.vm.selectedPkg
                readonly property string _translators: picker.vm.translatorsText(option.modelData.contributors)
                readonly property string _compat: picker.vm.gameVersionText(option.modelData.gameVersion)
                readonly property string _meta: [
                    option._translators !== "" ? qsTr("Çeviri: %1").arg(option._translators) : "",
                    picker.vm.dateText(option.modelData.version),
                    picker.vm.sizeText(option.modelData.downloadSize > 0 ? option.modelData.downloadSize
                                                                         : option.modelData.sizeBytes)
                ].filter(s => s !== "").join("  ·  ")

                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                implicitHeight: optionRow.implicitHeight + 24

                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusMD
                    color: option.selected ? Theme.withAlpha(Theme.primary, Theme.darkMode ? 0.14 : 0.08)
                         : hover.hovered ? Theme.fillQuaternary : "transparent"
                    border.width: option.selected ? 1 : 0
                    border.color: Theme.withAlpha(Theme.primary, 0.45)
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                }

                // Hairline between rows (not under the selected one)
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 46
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.separator
                    visible: option.index < picker.vm.patchOptions.length - 1 && !option.selected
                }

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                MouseArea {
                    anchors.fill: parent
                    onClicked: picker.vm.selectPatch(option.modelData.pkg)
                }

                Accessible.role: Accessible.RadioButton
                Accessible.name: picker.vm.patchLabel(option.modelData)
                Accessible.checked: option.selected

                RowLayout {
                    id: optionRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 14

                    // Radio
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 2
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        radius: 10
                        color: option.selected ? Theme.primary : "transparent"
                        border.width: option.selected ? 0 : 1.5
                        border.color: Theme.textMuted
                        Icon {
                            anchors.centerIn: parent
                            visible: option.selected
                            glyph: Icons.check
                            size: 13
                            color: "#FFFFFF"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                Layout.maximumWidth: optionRow.width * 0.6
                                textFormat: Text.PlainText
                                text: picker.vm.patchLabel(option.modelData)
                                font.pixelSize: Dimensions.headline
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                            }
                            StatusBadge {
                                visible: option.modelData.installed
                                tint: Theme.success
                                text: option.modelData.action === "update" ? qsTr("Kurulu · güncelleme var")
                                                                           : qsTr("Kurulu")
                            }
                            StatusBadge {
                                visible: option.modelData.pkg === "" && option.modelData.label === ""
                                         && !option.modelData.installed
                                tint: Theme.textSecondary
                                showDot: false
                                text: qsTr("Varsayılan")
                            }
                            StatusBadge {
                                visible: option.modelData.comingSoon
                                tint: Theme.textSecondary
                                showDot: false
                                text: qsTr("Yakında")
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: text !== ""
                            textFormat: Text.PlainText
                            text: option.modelData.description || ""
                            font.pixelSize: Dimensions.callout
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: text !== ""
                            textFormat: Text.PlainText
                            text: option._meta
                            font.pixelSize: Dimensions.footnote
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                        }

                        // The game version it was made for; amber when the game is newer
                        RowLayout {
                            Layout.fillWidth: true
                            visible: option._compat !== ""
                            spacing: 6
                            Icon {
                                glyph: option.modelData.warn ? Icons.triangleAlert : Icons.gamepad2
                                size: 13
                                color: option.modelData.warn ? Theme.warning : Theme.textMuted
                            }
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: qsTr("Uyumlu sürüm: %1").arg(option._compat)
                                      + (option.modelData.warn ? "  ·  " + qsTr("oyun daha yeni") : "")
                                font.pixelSize: Dimensions.footnote
                                color: option.modelData.warn ? Theme.warning : Theme.textMuted
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // ---- What the picked patch offers ----
        FadeLine {
            Layout.topMargin: 10
            lit: true
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.topMargin: 14
            spacing: 14

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: {
                    const sel = picker._sel
                    if (!sel) return ""
                    const name = picker.vm.patchLabel(sel)
                    switch (picker._action) {
                    case "installed": return qsTr("%1 kurulu ve güncel.").arg(name)
                    case "update":    return qsTr("%1 kurulu; yeni sürümü hazır.").arg(name)
                    case "switch":    return qsTr("Şu an %1 kurulu. Seçtiğin yamaya geçince o kaldırılır, %2 kurulur.")
                                                 .arg(picker.vm.patchLabel(picker.vm.installedPatch)).arg(name)
                    default:          return picker.vm.isGameInstalled
                                          ? qsTr("%1 kurulacak.").arg(name)
                                          : qsTr("Kurmak için önce oyunun klasörü bulunmalı.")
                    }
                }
                font.pixelSize: Dimensions.callout
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }

            AppButton {
                Layout.alignment: Qt.AlignVCenter
                visible: picker._action !== "installed" && picker._sel !== null
                enabled: !picker._busy && picker.vm.isGameInstalled
                         && !(picker._sel && picker._sel.comingSoon)
                style: "filled"
                icon: picker._action === "switch" ? Icons.refreshCw
                    : picker._action === "update" ? Icons.download : Icons.download
                text: picker._action === "switch" ? qsTr("Bu yamaya geç")
                    : picker._action === "update" ? qsTr("Güncelle")
                    : qsTr("Kur")
                onClicked: {
                    if (picker._action === "switch")
                        picker.switchRequested(picker._sel.pkg)
                    else if (picker._action === "update")
                        picker.updateRequested()
                    else
                        picker.installRequested(picker._sel.pkg)
                }
            }
        }
    }
}
