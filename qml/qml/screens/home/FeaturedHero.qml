// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * FeaturedHero — the home page's large art, one slide at a time: this PC's
 * games whose Turkish patch waits for a step, then the newest patches, then
 * the community banner. Each game slide has its one action. Slides move on
 * every 8 s unless the pointer is on the hero; the dots and the arrows pick
 * one. A banner that fails to load is left out rather than shown black.
 */
Item {
    id: hero

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installRequested(string gameId, string gameName, string installPath, string engine)

    property int current: 0
    // False while the page has it scrolled away: no slide change to draw
    property bool inView: true

    // ── Slides ───────────────────────────────────────────────────────────
    property var _recent: []
    function _refreshRecent() { hero._recent = GameService.recentlyUpdatedGames(8) }
    Component.onCompleted: _refreshRecent()
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { hero._refreshRecent() }
    }

    readonly property var _featured: _computeFeatured(GameService.library, _recent)
    function _computeFeatured(library, recent) {
        const out = []
        const seen = {}
        const add = (o) => {
            // Steam art exists only for Steam app ids.
            if (!/^\d+$/.test(o.steamAppId) || seen[o.steamAppId]) return
            seen[o.steamAppId] = true
            out.push(o)
        }
        // This PC's games with a step to take (library() lists them first).
        for (const g of library) {
            if (out.length >= 2) break
            const kind = g.status === "ready" ? "installable" : g.status === "update" ? "update" : ""
            if (kind) add({ kind: kind, gameId: g.id, name: g.name || "", steamAppId: g.steamAppId || "",
                            installPath: g.installPath || "", engine: g.engine || "", version: "" })
        }
        for (const e of recent) {
            if (out.length >= 5) break
            if (e.externalUrl) continue
            add({ kind: e.packageInstalled ? "installed" : e.isInstalled ? "installable" : "new",
                  gameId: e.id || e.steamAppId, name: e.name || "", steamAppId: e.steamAppId || "",
                  installPath: e.installPath || "", engine: e.engine || "", version: e.version || "" })
        }
        return out
    }
    readonly property bool _bannerReady: banner.status === Image.Ready
    readonly property int _count: _featured.length + (_bannerReady ? 1 : 0)
    on_CountChanged: if (hero.current >= hero._count) hero.current = 0

    function _eyebrow(f) {
        if (f.kind === "installable") return qsTr("Bilgisayarında · Türkçe yama hazır")
        if (f.kind === "update") return qsTr("Yama güncellemesi hazır")
        if (f.kind === "installed") return qsTr("Türkçe yaman kurulu")
        const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(f.version)
        if (!m) return qsTr("Yeni güncellenen yama")
        const d = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]))
        return qsTr("Yeni güncellendi · %1").arg(d.toLocaleDateString(Qt.locale("tr_TR"), "d MMMM"))
    }
    function _blurb(f) {
        if (f.kind === "installable") return qsTr("Oyun bilgisayarında bulundu. Tek tıkla kur; orijinal dosyaların yedeklenir.")
        if (f.kind === "update") return qsTr("Kurulu yamanın yeni sürümü yayımlandı. Sayfasından güncelleyebilirsin.")
        if (f.kind === "installed") return qsTr("Yama yeni sürümüne güncellendi ve bilgisayarında kurulu.")
        return qsTr("Türkçe yaması yeni yayımlandı ya da güncellendi. Neler değiştiğini sayfasında gör.")
    }
    function go(step) {
        if (hero._count < 2) return
        hero.current = (hero.current + step + hero._count) % hero._count
    }

    HoverHandler { id: heroHover }
    Timer {
        interval: 8000
        repeat: true
        running: hero.visible && hero.inView && hero._count > 1 && !heroHover.hovered && hero.Window.active
        onTriggered: hero.go(1)
    }

    // The community banner — fetched fresh, it changes often
    Image {
        id: banner
        visible: false
        source: "https://cdn.makineceviri.org/assets/banners/announcement.png?v=" + Date.now()
        sourceSize.width: 1600
        asynchronous: true
        cache: false
    }

    RectangularShadow {
        anchors.fill: art
        radius: Dimensions.radiusHero
        offset.y: 8
        blur: 30
        spread: -6
        color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.5 : 0.18)
        visible: hero._count > 0
    }

    // ── Art (rounded by a mask) ──────────────────────────────────────────
    Item {
        id: art
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Rectangle { anchors.fill: parent; color: Theme.surface }

        Repeater {
            model: hero._featured

            Item {
                id: slide
                required property var modelData
                required property int index
                readonly property bool fallback: shot.status === Image.Error
                property string coverSrc: ""
                function resolveCover() { slide.coverSrc = ImageCache.resolve(slide.modelData.steamAppId) }
                onFallbackChanged: if (slide.fallback) slide.resolveCover()

                anchors.fill: parent
                opacity: hero.current === slide.index ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.InOutQuad } }

                // Fixed target, gated by `enabled` (NATIVE-74).
                Connections {
                    target: ImageCache
                    enabled: slide.fallback && slide.coverSrc === ""
                    function onImageReady(readyId) {
                        if (readyId === slide.modelData.steamAppId) slide.resolveCover()
                    }
                }

                Image {
                    id: shot
                    anchors.fill: parent
                    source: GameService.steamHeroUrl(slide.modelData.steamAppId)
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 1600
                    asynchronous: true
                }

                // No Steam hero: the cover, blurred into a backdrop, beside itself
                Image {
                    id: backdrop
                    anchors.fill: parent
                    visible: false
                    source: slide.fallback ? slide.coverSrc : ""
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(260, 370)
                    asynchronous: true
                }
                MultiEffect {
                    anchors.fill: parent
                    visible: slide.fallback && backdrop.status === Image.Ready
                    source: backdrop
                    blurEnabled: true
                    blurMax: 64
                    blur: 1.0
                    brightness: -0.25
                    saturation: 0.2
                }
                Image {
                    visible: slide.fallback
                    anchors.right: parent.right
                    anchors.rightMargin: 40
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height - 56
                    width: height * 130 / 185
                    source: slide.coverSrc
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }

                // Legibility: the text sits on a dark foot and a dark left side
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.35; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.78) }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.55) }
                        GradientStop { position: 0.55; color: "transparent" }
                    }
                }

                TapHandler {
                    onTapped: hero.gameSelected(slide.modelData.gameId, slide.modelData.name,
                                                slide.modelData.installPath, slide.modelData.engine)
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 32
                    anchors.bottomMargin: 30
                    width: Math.min(parent.width * 0.6, 560)
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: hero._eyebrow(slide.modelData)
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.caption1
                        font.weight: Font.Bold
                        font.letterSpacing: 0.8
                        font.capitalization: Font.AllUppercase
                        color: Qt.rgba(1, 1, 1, 0.78)
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        text: slide.modelData.name
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.family: Dimensions.fontDisplay
                        font.pixelSize: hero.height > 300 ? 34 : Dimensions.title1
                        font.weight: Font.Bold
                        font.letterSpacing: -0.5
                        lineHeight: 1.02
                        color: "#FFFFFF"
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 8
                        visible: hero.height > 260
                        textFormat: Text.PlainText
                        text: hero._blurb(slide.modelData)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.callout
                        color: Qt.rgba(1, 1, 1, 0.82)
                    }
                    RowLayout {
                        spacing: 10
                        AppButton {
                            readonly property bool install: slide.modelData.kind === "installable"
                            style: "white"
                            text: install ? qsTr("Türkçe Yamayı Kur")
                                : slide.modelData.kind === "update" ? qsTr("Yamayı Güncelle")
                                : qsTr("İncele")
                            icon: install ? Icons.download : ""
                            onClicked: {
                                const f = slide.modelData
                                if (install) hero.installRequested(f.gameId, f.name, f.installPath, f.engine)
                                else hero.gameSelected(f.gameId, f.name, f.installPath, f.engine)
                            }
                        }
                    }
                }
            }
        }

        // Community banner slide
        Item {
            anchors.fill: parent
            readonly property int _index: hero._featured.length
            visible: hero._bannerReady && opacity > 0
            opacity: hero.current === _index ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.InOutQuad } }

            Image {
                anchors.fill: parent
                source: hero._bannerReady ? banner.source : ""
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 1600
                asynchronous: true
                cache: false
            }
            TapHandler { onTapped: Qt.openUrlExternally(Dimensions.discordUrl) }
            HoverHandler { cursorShape: Qt.PointingHandCursor }

            AppButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 32
                anchors.bottomMargin: 30
                style: "glass"
                icon: Icons.messagesSquare
                text: qsTr("Discord topluluğuna katıl")
                onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
            }
        }
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: Dimensions.radiusHero; color: "white" }
    }

    // Hairline on the edge (dark): art against a dark page needs an edge
    Rectangle {
        anchors.fill: parent
        radius: Dimensions.radiusHero
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, Theme.darkMode ? 0.08 : 0)
    }
    // …and the lit top edge the containers have
    Rectangle {
        visible: Theme.darkMode
        x: Dimensions.radiusHero * 0.7
        width: parent.width - Dimensions.radiusHero * 1.4
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: Qt.rgba(1, 1, 1, 0.22) }
            GradientStop { position: 0.7; color: Qt.rgba(1, 1, 1, 0.22) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // ── Controls ─────────────────────────────────────────────────────────
    AppButton {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        style: "glass"
        icon: Icons.chevronLeft
        toolTip: qsTr("Önceki")
        opacity: heroHover.hovered && hero._count > 1 ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        onClicked: hero.go(-1)
    }
    AppButton {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        style: "glass"
        icon: Icons.chevronRight
        toolTip: qsTr("Sonraki")
        opacity: heroHover.hovered && hero._count > 1 ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
        onClicked: hero.go(1)
    }

    // Page control
    Rectangle {
        visible: hero._count > 1
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 24
        anchors.bottomMargin: 30
        width: dots.implicitWidth + 18
        height: 22
        radius: 11
        color: Qt.rgba(0, 0, 0, 0.35)

        Row {
            id: dots
            anchors.centerIn: parent
            spacing: 7

            Repeater {
                model: hero._count

                Rectangle {
                    id: dot
                    required property int index
                    readonly property bool active: dot.index === hero.current
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 3.5
                    color: dot.active ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.42)
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Öne çıkan %1").arg(dot.index + 1)

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        margin: 5
                        onTapped: hero.current = dot.index
                    }
                }
            }
        }
    }
}
