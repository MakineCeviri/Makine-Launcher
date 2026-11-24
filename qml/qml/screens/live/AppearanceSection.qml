// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * AppearanceSection — "Görünüm": a preview, the few choices that matter, and the
 * rarely used ones folded under "Gelişmiş". Loaded on demand by the page.
 */
ColumnLayout {
    id: look

    // The page (MakineLiveScreen): _style, _consoleMode, _setStyle, _call, _busyMethod, _gta6
    property var screen: null

    readonly property var _style: screen ? screen._style : ({})
    readonly property bool _console: !!screen && screen._consoleMode

    // Resolved values, the same fallbacks SubtitleBlock uses
    // "GTA VI" is the band preset with the plum band: no new preset name, so
    // every renderer (overlay, Makine TV, phone, OBS) already understands it
    readonly property bool gta6: String(_style.bandColor || "").toLowerCase() === String(Theme.gta6Band).toLowerCase()
    readonly property string preset: gta6 ? "gta6" : (_style.preset || "band")
    readonly property int sizePx: _style.sizePx || 28
    readonly property int maxLines: _style.maxLines || 2
    readonly property real bandOpacity: preview.bandOpacity
    readonly property string colorKey: String(preview.textColor).toLowerCase() === "#f2c230" ? "yellow" : "white"
    readonly property string position: _style.position || "region"
    readonly property bool showSource: _style.showSource === true
    readonly property bool free: _style.boxX !== undefined && _style.boxX !== null

    Layout.fillWidth: true
    spacing: 14

    function nearest(value, steps) {
        let best = steps[0]
        for (let i = 1; i < steps.length; ++i)
            if (Math.abs(steps[i] - value) < Math.abs(best - value)) best = steps[i]
        return best
    }

    // Preview at the real size
    Card {
        Layout.fillWidth: true
        implicitHeight: 190
        radius: Dimensions.radiusSection
        reveal: false

        Rectangle {
            id: lookScene
            anchors.fill: parent
            anchors.margins: 10
            radius: Dimensions.radiusLG
            clip: true
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.textPrimary }
                GradientStop { position: 0.5; color: Theme.textMuted }
                GradientStop { position: 1.0; color: Theme.bgPrimary }
            }
            SunsetScene { active: look.screen ? look.screen._gta6 : false }

            SubtitleBlock {
                id: preview
                x: Math.round((lookScene.width - width) / 2)
                y: look.position === "top" ? 4
                 : look.position === "bottom" ? lookScene.height - height - 4
                 : Math.round((lookScene.height - height) / 2)
                maxWidth: lookScene.width * 0.8
                stretchWidth: lookScene.width * 0.95
                animate: Dimensions.animFast > 0
                style: look._style
                lines: [
                    { text: qsTr("Arabayı arkaya çek, birazdan dönerim."), sourceText: "Pull the car around back, I'll be right out.", kind: "verified" },
                    { text: qsTr("Kimseye bir şey söyleme."), sourceText: "Don't tell anyone.", kind: "machine" }
                ]
            }
        }
    }

    // The choices most players touch
    ListSection {
        Layout.fillWidth: true
        footer: qsTr("Onay bekleyen makine çevirisinin bandında solda ince sarı bir çizgi olur. Değişiklik bir sonraki satırda oyuna yansır.")

        ListRow {
            icon: Icons.palette
            title: qsTr("Biçim")
            subtitle: look.preset === "gta6" ? qsTr("Erik moru bant, beyaz yazı: GTA VI'nın gün batımında da okunur; TV'de ve telefonda da aynı.")
                    : look.preset === "contrast" ? qsTr("Koyu bant, sarı yazı: parlak, kalabalık sahneler için.")
                    : look.preset === "classic" ? qsTr("Bant yok; kalın kontur ve gölge, film altyazısı gibi.")
                    : look.preset === "minimal" ? qsTr("Bant yok, yalnız kontur; en az yer kaplar.")
                    : qsTr("Yazının arkasında koyu bant; her sahnede okunur.")
            SegmentedControl {
                small: true
                readonly property var _stock: [{ value: "band", text: qsTr("Bant") }, { value: "contrast", text: qsTr("Kontrast") },
                                               { value: "classic", text: qsTr("Film") }, { value: "minimal", text: qsTr("Sade") }]
                model: look._console || look.gta6 ? [{ value: "gta6", text: "GTA VI" }].concat(_stock) : _stock
                currentValue: look.preset
                // A preset brings its own band, colour and outline back
                onActivated: (value) => look.screen._setStyle(
                    value === "gta6" ? { preset: "band", bandColor: String(Theme.gta6Band), bandOpacity: 0.92,
                                         textColor: null, outlinePx: null, shadow: null }
                                     : { preset: value, bandColor: null, bandOpacity: null, textColor: null,
                                         outlinePx: null, shadow: null })
            }
        }
        ListRow {
            icon: Icons.type
            title: qsTr("Yazı boyu")
            subtitle: qsTr("1080p ekrandaki boy; ekran büyüdükçe altyazı da büyür.")
            SegmentedControl {
                small: true
                model: [{ value: "22", text: qsTr("Küçük") }, { value: "28", text: qsTr("Orta") },
                        { value: "34", text: qsTr("Büyük") }, { value: "42", text: qsTr("Çok büyük") }]
                currentValue: String(look.nearest(look.sizePx, [22, 28, 34, 42]))
                onActivated: (value) => look.screen._setStyle({ sizePx: Number(value) })
            }
        }
        ListRow {
            icon: Icons.swatchBook
            title: qsTr("Renk")
            subtitle: qsTr("Sarı, oyunun kendi beyaz altyazısından ayrılır.")
            SegmentedControl {
                small: true
                model: [{ value: "white", text: qsTr("Beyaz") }, { value: "yellow", text: qsTr("Sarı") }]
                currentValue: look.colorKey
                onActivated: (value) => look.screen._setStyle({ textColor: value === "yellow" ? preview.signatureYellow.toString() : "#FFFFFF" })
            }
        }
        ListRow {
            icon: Icons.pictureInPicture2
            title: qsTr("Yer")
            subtitle: look.position === "region" ? qsTr("İngilizce altyazının tam üstünde çıkar.")
                    : look.position === "top" ? qsTr("Ekranın üstünde çıkar.")
                    : qsTr("Ekranın altında çıkar.")
            SegmentedControl {
                small: true
                model: [{ value: "region", text: qsTr("Altyazının üstü") }, { value: "bottom", text: qsTr("Alt") },
                        { value: "top", text: qsTr("Üst") }, { value: "free", text: qsTr("Serbest") }]
                currentValue: look.free ? "free" : look.position
                // Serbest: drag the box in the game (F8); the place is kept as boxX/boxY
                onActivated: (value) => value === "free" ? look.screen._call("box.move", { on: true })
                                                         : look.screen._setStyle({ position: value, boxX: null, boxY: null })
            }
        }
        ListRow {
            visible: look.position !== "region" && !look.free
            icon: Icons.arrowUpDown
            title: qsTr("Kenardan uzaklık")
            subtitle: qsTr("Altyazının ekranın kenarından ne kadar içeride duracağı.")
            SegmentedControl {
                small: true
                model: [{ value: "12", text: qsTr("Dar") }, { value: "24", text: qsTr("Orta") },
                        { value: "48", text: qsTr("Geniş") }, { value: "96", text: qsTr("Çok") }]
                currentValue: String(look.nearest(look._style.marginPx || 24, [12, 24, 48, 96]))
                onActivated: (value) => look.screen._setStyle({ marginPx: Number(value) })
            }
        }
        ListRow {
            icon: Icons.maximize2
            title: qsTr("Genişlik")
            subtitle: qsTr("Bandın en geniş hâli, ekranın yüzdesi olarak; uzun satırda yine genişler.")
            SegmentedControl {
                small: true
                model: [{ value: "50", text: "%50" }, { value: "62", text: "%62" },
                        { value: "80", text: "%80" }, { value: "100", text: "%100" }]
                currentValue: String(look.nearest(look._style.widthPct || 62, [50, 62, 80, 100]))
                onActivated: (value) => look.screen._setStyle({ widthPct: Number(value) })
            }
        }
        ListRow {
            icon: Icons.eye
            title: qsTr("Örnek göster")
            subtitle: qsTr("Oyunun üstünde birkaç saniye örnek bir altyazı çıkar; ayarı orada dene.")
            AppButton {
                style: "gray"
                text: qsTr("Göster")
                busy: !!look.screen && look.screen._busyMethod === "sample"
                onClicked: look.screen._call("sample")
            }
        }
    }

    // Rarely touched
    LiveFold {
        title: qsTr("Gelişmiş")
        subtitle: qsTr("Satır sayısı, bant, yazı tipi, gösterim süresi")
        icon: Icons.slidersHorizontal

        ListSection {
            Layout.fillWidth: true

            ListRow {
                icon: Icons.listOrdered
                title: qsTr("Satır")
                subtitle: qsTr("Bir altyazı en çok kaç satır olsun; uzun satırda bant genişler, yazı küçülmez.")
                SegmentedControl {
                    small: true
                    model: [{ value: "1", text: "1" }, { value: "2", text: "2" }, { value: "3", text: "3" }]
                    currentValue: String(Math.min(3, look.maxLines))
                    onActivated: (value) => look.screen._setStyle({ maxLines: Number(value) })
                }
            }
            ListRow {
                icon: Icons.layers
                title: qsTr("Bant")
                subtitle: qsTr("Koyu bant altındaki İngilizce altyazıyı örter. Bant azaldıkça kontur kalınlaşır.")
                SegmentedControl {
                    small: true
                    model: [{ value: "0", text: qsTr("Yok") }, { value: "0.5", text: qsTr("Hafif") },
                            { value: "0.9", text: qsTr("Koyu") }, { value: "1", text: qsTr("Tam") }]
                    currentValue: String(look.nearest(look.bandOpacity, [0, 0.5, 0.9, 1]))
                    onActivated: (value) => look.screen._setStyle({ bandOpacity: Number(value) })
                }
            }
            ListRow {
                icon: Icons.swatchBook
                title: qsTr("Bant rengi")
                subtitle: qsTr("Makine Live'ın koyu tonları; hepsi yazıyı öne çıkarır.")
                SegmentedControl {
                    small: true
                    model: [{ value: "#0f1012", text: qsTr("Mürekkep") }, { value: "#16171a", text: qsTr("Gece") },
                            { value: "#1d1f23", text: qsTr("Koyu") }, { value: "#24262b", text: qsTr("Gri") },
                            { value: "#000000", text: qsTr("Siyah") }]
                    currentValue: String(look._style.bandColor || "#0f1012").toLowerCase()
                    onActivated: (value) => look.screen._setStyle({ bandColor: value })
                }
            }
            ListRow {
                icon: Icons.layers
                title: qsTr("Gölge")
                subtitle: qsTr("Bandın ya da yazının altında yumuşak bir gölge.")
                AppSwitch {
                    checked: preview.shadow
                    accessibleName: qsTr("Gölge")
                    onToggled: look.screen._setStyle({ shadow: !preview.shadow })
                }
            }
            ListRow {
                icon: Icons.type
                title: qsTr("Yazı tipi")
                subtitle: qsTr("Launcher'ın kendi yazı tipleri.")
                SegmentedControl {
                    small: true
                    model: [{ value: "Inter Display", text: "Inter Display" }, { value: "Inter", text: "Inter" }]
                    currentValue: look._style.font || "Inter Display"
                    onActivated: (value) => look.screen._setStyle({ font: value === "Inter Display" ? null : value })
                }
            }
            ListRow {
                icon: Icons.languages
                title: qsTr("İngilizcesini de göster")
                subtitle: qsTr("Türkçenin altında küçük harflerle aslı durur.")
                AppSwitch {
                    checked: look.showSource
                    accessibleName: qsTr("İngilizcesini de göster")
                    onToggled: look.screen._setStyle({ showSource: !look.showSource })
                }
            }
            ListRow {
                icon: Icons.clock
                title: qsTr("En kısa gösterim")
                subtitle: qsTr("Kısa bir satır da en az bu kadar ekranda kalır; okumaya yetişmiyorsan artır.")
                SegmentedControl {
                    small: true
                    model: [{ value: "600", text: qsTr("0,6 sn") }, { value: "1200", text: qsTr("1,2 sn") },
                            { value: "2000", text: qsTr("2 sn") }, { value: "3000", text: qsTr("3 sn") }]
                    currentValue: String(look.nearest(look._style.minShowMs || 1200, [600, 1200, 2000, 3000]))
                    onActivated: (value) => look.screen._setStyle({ minShowMs: Number(value) })
                }
            }
            ListRow {
                icon: Icons.rotateCcw
                title: qsTr("Varsayılana dön")
                subtitle: qsTr("Bütün görünüm ayarlarını ilk hâline getirir.")
                AppButton {
                    style: "plain"
                    text: qsTr("Sıfırla")
                    onClicked: look.screen._setStyle({ preset: null, font: null, sizePx: null, maxLines: null, textColor: null,
                                                       bandColor: null, bandOpacity: null, outlinePx: null, shadow: null,
                                                       widthPct: null, marginPx: null, position: null, showSource: null,
                                                       boxX: null, boxY: null, minShowMs: null })
                }
            }
        }
    }
}
