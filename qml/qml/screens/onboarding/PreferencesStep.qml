// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * PreferencesStep — the few choices worth making before the first use.
 *
 * The accent colour used to be a step of its own, asked before the app had
 * shown anything. It is one row here, next to what actually changes how the
 * launcher behaves: whether it watches for a running game, whether it starts
 * with Windows, and whether it sends error reports — said up front rather
 * than found later in Settings. Every change applies at once, and the whole
 * wizard takes the accent as it is picked. The rows are Settings' own
 * (ListSection, ListRow, AppSwitch).
 */
Item {
    id: step

    signal finish()

    function primaryAction() { step.finish() }

    readonly property var _presets: SettingsManager.accentPresets()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(step.width - 96, 640)
        spacing: 0

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Kendine göre ayarla")
            font.family: Dimensions.fontDisplay
            font.pixelSize: 34
            font.weight: Font.Bold
            font.letterSpacing: -0.8
            color: Theme.textPrimary
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 28
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Hepsini sonra Ayarlar'dan değiştirebilirsin.")
            font.pixelSize: Dimensions.headline
            color: Theme.textSecondary
        }

        ListSection {
            ListRow {
                icon: Icons.palette
                iconTint: Theme.primary
                title: qsTr("Vurgu rengi")
                subtitle: qsTr("Düğmeler, seçimler ve ilerleme çubukları bu rengi alır.")

                Row {
                    spacing: 7
                    Repeater {
                        model: step._presets

                        // A colour; the chosen one ringed, with a check
                        Item {
                            id: swatch
                            required property var modelData
                            readonly property bool selected: SettingsManager.accentPreset === swatch.modelData.id
                            width: 26; height: 26
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: swatch.modelData.name || swatch.modelData.id
                            Accessible.checked: swatch.selected

                            Rectangle {
                                anchors.centerIn: parent
                                width: 26; height: 26; radius: 13
                                color: "transparent"
                                border.width: 2
                                border.color: Theme.textPrimary
                                opacity: swatch.selected ? 0.9 : 0
                                scale: swatch.selected ? 1 : 0.7
                                Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                Behavior on scale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: swatch.selected ? 18 : 22
                                height: width
                                radius: width / 2
                                // "base": the fill the accent paints with
                                color: swatch.modelData.colors[2]
                                scale: swatchHover.hovered && !swatch.selected ? 1.12 : 1
                                Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }
                                Behavior on scale { NumberAnimation { duration: Dimensions.animFast } }
                                Icon {
                                    anchors.centerIn: parent
                                    glyph: Icons.check
                                    size: 11
                                    color: "#FFFFFF"
                                    opacity: swatch.selected ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                }
                            }
                            HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                margin: 3
                                onTapped: SettingsManager.accentPreset = swatch.modelData.id
                            }
                        }
                    }
                }
            }
            ListRow {
                icon: Icons.scanSearch
                iconTint: "#FF9F0A"
                title: qsTr("Açık oyunu fark et")
                subtitle: qsTr("Oyun açıkken yama kurulmaz; tanınmayan bir oyunu da kütüphanene eklemeni sağlar.")
                AppSwitch {
                    accessibleName: qsTr("Açık oyunu fark et")
                    checked: SettingsManager.autoDetectGames
                    onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
                }
            }
            ListRow {
                icon: Icons.power
                iconTint: "#30D158"
                title: qsTr("Windows açılınca başlat")
                subtitle: qsTr("Yama güncellemelerini kaçırmazsın; küçültülmüş olarak, sessizce açılır.")
                AppSwitch {
                    accessibleName: qsTr("Windows açılınca başlat")
                    checked: SettingsManager.startWithWindows
                    onToggled: SettingsManager.startWithWindows = !SettingsManager.startWithWindows
                }
            }
            ListRow {
                icon: Icons.shieldCheck
                iconTint: "#5E5CE6"
                title: qsTr("Çökme ve hata raporlarını gönder")
                subtitle: qsTr("Sorunları bunlarla bulup düzeltiyoruz. Raporlar kişisel bilgi içermez; kapatırsan hiçbiri gönderilmez.")
                AppSwitch {
                    accessibleName: qsTr("Çökme ve hata raporlarını gönder")
                    checked: SettingsManager.sendCrashReports
                    onToggled: SettingsManager.sendCrashReports = !SettingsManager.sendCrashReports
                }
            }
        }
    }
}
