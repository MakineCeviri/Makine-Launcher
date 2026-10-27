// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * DisplaySettings.qml - "Görünüm": light or dark, the accent colour,
 * motion, and the graphics API the window draws with.
 */
ColumnLayout {
    id: displayRoot
    spacing: 28

    // A small picture of the launcher in one appearance, as macOS shows them
    component AppearanceChoice: Item {
        id: choice
        property bool dark: true
        property string label: ""
        readonly property bool selected: SettingsManager.isDarkMode === choice.dark

        implicitWidth: 132
        implicitHeight: thumb.height + 30

        activeFocusOnTab: true
        Accessible.role: Accessible.RadioButton
        Accessible.name: choice.label
        Accessible.checked: choice.selected
        Keys.onReturnPressed: SettingsManager.isDarkMode = choice.dark
        Keys.onSpacePressed: SettingsManager.isDarkMode = choice.dark

        Rectangle {
            id: thumb
            width: parent.width
            height: 84
            radius: 10
            color: choice.dark ? "#141416" : "#F5F5F7"
            border.width: 1
            border.color: choice.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)

            // Sidebar
            Rectangle {
                x: 6; y: 6
                width: 34; height: parent.height - 12
                radius: 6
                color: choice.dark ? "#232326" : "#E4E4E7"
                Column {
                    x: 5; y: 8; spacing: 5
                    Repeater {
                        model: 4
                        Rectangle {
                            required property int index
                            width: 22; height: 4; radius: 2
                            color: index === 0 ? Theme.primary : (choice.dark ? "#48484A" : "#C7C7CC")
                        }
                    }
                }
            }
            // A hero and two rows of tiles
            Rectangle {
                x: 46; y: 8
                width: parent.width - 54; height: 26
                radius: 5
                color: choice.dark ? "#121217" : "#FFFFFF"
            }
            Row {
                x: 46; y: 40; spacing: 5
                Repeater {
                    model: 5
                    Rectangle {
                        width: (thumb.width - 54 - 20) / 5; height: 34
                        radius: 3
                        color: choice.dark ? "#3A3A3C" : "#D1D1D6"
                    }
                }
            }
        }
        Rectangle {
            anchors.fill: thumb
            anchors.margins: -4
            radius: 14
            color: "transparent"
            border.width: 3
            border.color: choice.selected ? Theme.primary
                        : choice.activeFocus ? Theme.withAlpha(Theme.primary, 0.45) : "transparent"
        }

        Text {
            anchors.top: thumb.bottom
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            textFormat: Text.PlainText
            text: choice.label
            font.pixelSize: Dimensions.callout
            font.weight: choice.selected ? Font.DemiBold : Font.Medium
            color: choice.selected ? Theme.textPrimary : Theme.textSecondary
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        MouseArea {
            anchors.fill: parent
            onClicked: SettingsManager.isDarkMode = choice.dark
        }
    }

    ListSection {
        title: qsTr("Görünüm")
        footer: qsTr("Açık ve koyu görünüm aynı düzeni kullanır; seçim hemen uygulanır.")

        Item {
            Layout.fillWidth: true
            implicitHeight: appearanceRow.implicitHeight + 36

            Row {
                id: appearanceRow
                anchors.centerIn: parent
                spacing: 28
                AppearanceChoice { dark: false; label: qsTr("Açık") }
                AppearanceChoice { dark: true; label: qsTr("Koyu") }
            }
        }

        ListRow {
            id: accentRow
            readonly property var _presets: SettingsManager.accentPresets()
            readonly property string _name: {
                for (const p of accentRow._presets)
                    if (p.id === SettingsManager.accentPreset) return p.name
                return ""
            }
            icon: Icons.palette
            iconTint: Theme.primary
            title: qsTr("Vurgu rengi")
            subtitle: qsTr("Düğmelerde, seçili öğelerde ve bağlantılarda kullanılan renk: %1.").arg(accentRow._name)

            Row {
                spacing: 8
                Repeater {
                    model: accentRow._presets

                    Item {
                        id: swatch
                        required property var modelData
                        readonly property bool selected: SettingsManager.accentPreset === swatch.modelData.id
                        width: 22; height: 22

                        activeFocusOnTab: true
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: swatch.modelData.name
                        Accessible.checked: swatch.selected
                        Keys.onReturnPressed: SettingsManager.accentPreset = swatch.modelData.id
                        Keys.onSpacePressed: SettingsManager.accentPreset = swatch.modelData.id

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: swatch.modelData.colors[2]
                            border.width: 1
                            border.color: Qt.rgba(0, 0, 0, 0.12)
                        }
                        // The chosen one: a white dot in the middle, as on macOS
                        Rectangle {
                            visible: swatch.selected
                            anchors.centerIn: parent
                            width: 8; height: 8; radius: 4
                            color: "#FFFFFF"
                        }
                        Rectangle {
                            visible: swatch.activeFocus
                            anchors.fill: parent
                            anchors.margins: -3
                            radius: width / 2
                            color: "transparent"
                            border.width: 2
                            border.color: Theme.withAlpha(Theme.primary, 0.5)
                        }
                        HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: SettingsManager.accentPreset = swatch.modelData.id
                        }
                        StyledToolTip {
                            visible: swatchHover.hovered
                            text: swatch.modelData.name
                        }
                    }
                }
            }
        }
    }

    // Makine, the launcher's companion (CompanionService): shown or not, where he lives, his corner on
    // the home page, his look, his speech, his size; a build without him has none of it
    ListSection {
        visible: Companion.available
        title: qsTr("Makine")
        footer: qsTr("Tıklarsan tepki verir, başından ya da elinden tutup sürükleyebilir, fırlatabilirsin; yerinden çıkarırsan uçarak geri döner. Uyurken olduğu yerde kalır. Üstüne sağ tıklayınca ne yapacağını ve nerede duracağını seçersin.")

        ListRow {
            icon: Icons.sparkle
            iconTint: Theme.primary
            title: qsTr("Makine'yi göster")
            subtitle: qsTr("Yaptığın işe eşlik eder: oyunlarını ararken bakınır, yama inerken ışık toplar, kurulunca sevinir, bir sorun olursa söyler.")
            AppSwitch {
                checked: SettingsManager.companionEnabled
                accessibleName: qsTr("Makine'yi göster")
                onToggled: SettingsManager.companionEnabled = !SettingsManager.companionEnabled
            }
        }

        ListRow {
            enabled: SettingsManager.companionEnabled
            icon: Icons.monitor
            iconTint: "#0A84FF"
            title: qsTr("Nerede dursun")
            subtitle: SettingsManager.companionPlace === "clock"
                      ? qsTr("En küçük haliyle, elsiz, sağ altta saatin ve tarihin hemen üstünde durur; görev çubuğunun üstüne çıkmaz. Tam ekran bir oyun ya da sunum açıkken görünmez.")
                      : SettingsManager.companionPlace === "desktop"
                      ? qsTr("Launcher'ın penceresinden çıkıp ekranın altında, saatin yanında süzülür; launcher tepsiye küçültülmüşken de yanında olur. Tam ekran bir oyun ya da sunum açıkken görünmez.")
                      : qsTr("Ana sayfada kendi köşesinde, başka sayfalarda kenar çubuğunun boş yerinde durur; sayfa değişince oraya uçar.")
            SegmentedControl {
                small: true
                model: [
                    { value: "launcher", text: qsTr("Launcher") },
                    { value: "desktop", text: qsTr("Masaüstü") },
                    { value: "clock", text: qsTr("Saatin üstü") }
                ]
                currentValue: SettingsManager.companionPlace
                onActivated: (value) => { SettingsManager.companionPlace = value }
            }
        }

        ListRow {
            enabled: SettingsManager.companionEnabled && SettingsManager.companionPlace === "launcher"
            icon: Icons.house
            iconTint: "#FF9F0A"
            title: qsTr("Ana sayfada kendi köşesi")
            subtitle: qsTr("Ana sayfada öne çıkan oyunların yanındaki kendi köşesinde durur. Kapatırsan hep kenar çubuğunda kalır.")
            AppSwitch {
                checked: SettingsManager.companionCorner
                accessibleName: qsTr("Ana sayfada kendi köşesi")
                onToggled: SettingsManager.companionCorner = !SettingsManager.companionCorner
            }
        }

        ListRow {
            enabled: SettingsManager.companionEnabled && SettingsManager.companionPlace !== "clock"
            icon: Icons.sparkles
            iconTint: "#BF5AF2"
            title: qsTr("Görünüşü")
            subtitle: SettingsManager.companionPlace === "clock"
                      ? qsTr("Saatin üstünde hep sade, elsiz halindedir.")
                      : qsTr("Elleriyle selam verir, beşlik çakar, bayrağı tutar; elinden tutup sallayabilirsin. Sade halinde elleri yoktur, yalnız başı ve ışığı kalır.")
            SegmentedControl {
                small: true
                model: [
                    { value: "full", text: qsTr("Elleriyle") },
                    { value: "plain", text: qsTr("Sade") }
                ]
                currentValue: SettingsManager.companionLook
                onActivated: (value) => { SettingsManager.companionLook = value }
            }
        }

        ListRow {
            enabled: SettingsManager.companionEnabled
            icon: Icons.messageCircle
            iconTint: "#30D158"
            title: qsTr("Konuşsun")
            subtitle: qsTr("Konuşma balonları. Kapatırsan yalnızca bir sorun olduğunda yazar.")
            AppSwitch {
                checked: SettingsManager.companionTalk
                accessibleName: qsTr("Konuşsun")
                onToggled: SettingsManager.companionTalk = !SettingsManager.companionTalk
            }
        }

        ListRow {
            enabled: SettingsManager.companionEnabled && SettingsManager.companionPlace !== "clock"
            icon: Icons.maximize2
            iconTint: Theme.textMuted
            title: qsTr("Boyutu")
            subtitle: SettingsManager.companionPlace === "clock"
                      ? qsTr("Saatin üstünde boyu görev çubuğuna göre ayarlanır.")
                      : qsTr("Her yerde aynı boyda durur: köşesinde, kenar çubuğunda ve masaüstünde. Pencere çok kısaysa kenar çubuğuna sığacak kadar küçülür.")
            SegmentedControl {
                small: true
                model: [
                    { value: "small", text: qsTr("Küçük") },
                    { value: "medium", text: qsTr("Orta") },
                    { value: "large", text: qsTr("Büyük") }
                ]
                currentValue: SettingsManager.companionSize
                onActivated: (value) => { SettingsManager.companionSize = value }
            }
        }
    }

    ListSection {
        title: qsTr("Hareket ve çizim")

        ListRow {
            icon: Icons.sparkles
            iconTint: "#BF5AF2"
            title: qsTr("Animasyonlar")
            subtitle: qsTr("Sayfa geçişleri ve küçük hareketler. Kapatırsan her şey anında değişir; yavaş bilgisayarlarda rahatlatır.")
            AppSwitch {
                checked: SettingsManager.enableAnimations
                accessibleName: qsTr("Animasyonlar")
                onToggled: SettingsManager.enableAnimations = !SettingsManager.enableAnimations
            }
        }

        ListRow {
            id: backendSetting
            readonly property string activeApi: SettingsManager.activeGraphicsApi()
            property bool userChanged: false
            readonly property bool needsRestart: {
                if (!userChanged) return false
                var cfg = SettingsManager.graphicsBackend
                if (cfg === "" || cfg === "auto") return false
                if (cfg === "vulkan" && activeApi !== "Vulkan") return true
                if (cfg === "d3d11" && activeApi !== "Direct3D 11") return true
                if (cfg === "opengl" && activeApi !== "OpenGL") return true
                return false
            }
            icon: Icons.cpu
            iconTint: Theme.textMuted
            title: qsTr("Grafik arabirimi")
            subtitle: backendSetting.needsRestart
                      ? qsTr("Launcher'ı yeniden başlatınca geçerli olur.")
                      : qsTr("Şu an: %1. Görüntüde bozulma ya da titreme olursa başka birini dene.").arg(backendSetting.activeApi)

            SegmentedControl {
                small: true
                model: [
                    { value: "auto", text: qsTr("Otomatik") },
                    { value: "d3d11", text: "D3D11" },
                    { value: "vulkan", text: "Vulkan" },
                    { value: "opengl", text: "OpenGL" }
                ]
                currentValue: SettingsManager.graphicsBackend === "" ? "auto" : SettingsManager.graphicsBackend
                onActivated: (value) => {
                    SettingsManager.graphicsBackend = value
                    backendSetting.userChanged = true
                }
            }
        }
    }
}
