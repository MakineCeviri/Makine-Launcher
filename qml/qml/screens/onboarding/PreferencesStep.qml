// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * PreferencesStep — the few choices worth making before the first use.
 *
 * The accent colour used to be a step of its own, asked before the app had
 * shown anything. It is one row here, next to what actually changes how the
 * launcher behaves: whether it watches for a running game, whether it starts
 * with Windows, and whether it sends error reports — said up front rather
 * than found later in Settings. Every change applies at once.
 */
Item {
    id: step

    signal finish()
    signal back()

    function primaryAction() { step.finish() }

    readonly property var _presets: SettingsManager.accentPresets()

    // ── Rows ─────────────────────────────────────────────────────────────────
    component PrefRow: Item {
        id: prefRow
        property string title: ""
        property string description: ""
        default property alias control: controlSlot.data

        Layout.fillWidth: true
        implicitHeight: Math.max(texts.implicitHeight, controlSlot.implicitHeight) + 32

        ColumnLayout {
            id: texts
            anchors.left: parent.left
            anchors.right: controlSlot.left
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: prefRow.title
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: "#FFFFFF"
            }
            Text {
                Layout.fillWidth: true
                visible: prefRow.description !== ""
                textFormat: Text.PlainText
                text: prefRow.description
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                font.pixelSize: 13
                color: Qt.rgba(1, 1, 1, 0.55)
            }
        }
        Item {
            id: controlSlot
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            width: implicitWidth
            height: implicitHeight
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Qt.rgba(1, 1, 1, 0.07)
    }

    component Switch: Rectangle {
        id: sw
        property bool checked: false
        property string label: ""
        signal toggled()

        width: 44; height: 24; radius: 12
        color: sw.checked ? Theme.accentBase : Qt.rgba(1, 1, 1, 0.14)
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        border.width: sw.activeFocus ? 2 : 0
        border.color: "#FFFFFF"

        activeFocusOnTab: true
        Accessible.role: Accessible.CheckBox
        Accessible.name: sw.label
        Accessible.checked: sw.checked
        Keys.onSpacePressed: sw.toggled()

        Rectangle {
            width: 18; height: 18; radius: 9
            anchors.verticalCenter: parent.verticalCenter
            x: sw.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"
            Behavior on x { NumberAnimation { duration: Dimensions.animFast; easing.type: Dimensions.easeStandard } }
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: sw.toggled() }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(step.width - 96, 620)
        spacing: 0

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Kendine göre ayarla")
            font.pixelSize: 32
            font.weight: Font.Bold
            font.letterSpacing: -0.6
            color: "#FFFFFF"
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 28
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            text: qsTr("Hepsini sonra Ayarlar'dan değiştirebilirsin.")
            font.pixelSize: 15
            color: Qt.rgba(1, 1, 1, 0.65)
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: rows.implicitHeight + 8
            radius: 16
            color: Qt.rgba(1, 1, 1, 0.04)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)

            ColumnLayout {
                id: rows
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 0

                PrefRow {
                    title: qsTr("Vurgu rengi")
                    description: qsTr("Düğmeler, seçimler ve ilerleme çubukları bu rengi alır.")

                    Row {
                        spacing: 8
                        Repeater {
                            model: step._presets

                            Rectangle {
                                id: swatch
                                required property var modelData
                                readonly property bool selected: SettingsManager.accentPreset === swatch.modelData.id
                                width: 26; height: 26; radius: 13
                                color: swatch.modelData.colors[2]
                                border.width: swatch.selected ? 2 : 0
                                border.color: "#FFFFFF"
                                scale: swatchHover.hovered ? 1.12 : 1.0
                                Behavior on scale { NumberAnimation { duration: Dimensions.animFast } }
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: swatch.modelData.name || swatch.modelData.id
                                Accessible.checked: swatch.selected

                                HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler {
                                    margin: 3
                                    onTapped: SettingsManager.accentPreset = swatch.modelData.id
                                }
                            }
                        }
                    }
                }

                Divider {}

                PrefRow {
                    title: qsTr("Açık oyunu fark et")
                    description: qsTr("Oyun açıkken yama kurulmaz; tanınmayan bir oyunu da kütüphanene eklemeni sağlar.")
                    Switch {
                        label: qsTr("Açık oyunu fark et")
                        checked: SettingsManager.autoDetectGames
                        onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
                    }
                }

                Divider {}

                PrefRow {
                    title: qsTr("Windows açılınca başlat")
                    description: qsTr("Yama güncellemelerini kaçırmazsın; küçültülmüş olarak, sessizce açılır.")
                    Switch {
                        label: qsTr("Windows açılınca başlat")
                        checked: SettingsManager.startWithWindows
                        onToggled: SettingsManager.startWithWindows = !SettingsManager.startWithWindows
                    }
                }

                Divider {}

                PrefRow {
                    title: qsTr("Çökme ve hata raporlarını gönder")
                    description: qsTr("Sorunları bunlarla bulup düzeltiyoruz. Raporlar kişisel bilgi içermez; kapatırsan hiçbiri gönderilmez.")
                    Switch {
                        label: qsTr("Çökme ve hata raporlarını gönder")
                        checked: SettingsManager.sendCrashReports
                        onToggled: SettingsManager.sendCrashReports = !SettingsManager.sendCrashReports
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 36
            spacing: 12

            OnboardingButton {
                kind: "quiet"
                text: qsTr("Geri")
                onClicked: step.back()
            }
            OnboardingButton {
                kind: "primary"
                text: qsTr("Makine Launcher'ı aç")
                trailingIcon: "\uE72A"
                onClicked: step.finish()
            }
        }
    }
}
