// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * AnnouncementCard.qml - Discord community banner inside styled container.
 * Click opens Discord invite.
 *
 * After the banner it shows, one at a time, a few games worth a look: this
 * PC's games whose Turkish patch waits to be installed or updated, then the
 * newest patches — each with its one action. It moves on every 8 s unless
 * the pointer is on it; the dots pick a slide.
 */
Rectangle {
    id: root

    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 200

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installRequested(string gameId, string gameName, string installPath, string engine)

    // 0 = the announcement banner, 1.. = _featured[current - 1]
    property int current: 0
    readonly property int _count: 1 + _featured.length

    property var _recent: []
    // Games whose patch lives on another site: nothing to install from here.
    property var _external: ({})
    function _refreshRecent() {
        const all = GameService.recentlyUpdatedGames(100000)
        const ext = {}
        for (const e of all)
            if (e.externalUrl) ext[e.id || e.steamAppId] = true
        root._external = ext
        root._recent = all.slice(0, 8)
    }
    Component.onCompleted: _refreshRecent()
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { root._refreshRecent() }
    }

    readonly property var _featured: _computeFeatured(GameService.games,
                                                      GameService.installedTranslations, _recent, _external)
    function _computeFeatured(games, installed, recent, external) {
        const patched = {}
        for (const t of installed)
            patched[t.id] = t.hasUpdate === true
        const out = []
        const seen = {}
        const add = (o) => {
            // Steam art exists only for Steam app ids.
            if (!/^\d+$/.test(o.steamAppId) || seen[o.steamAppId]) return
            seen[o.steamAppId] = true
            out.push(o)
        }
        for (const g of games) {
            if (out.length >= 2) break
            if (external[g.id] || external[g.steamAppId]) continue
            const kind = !(g.id in patched) ? "installable" : patched[g.id] ? "update" : ""
            if (kind) add({ kind: kind, gameId: g.id, name: g.name || "", steamAppId: g.steamAppId || "",
                            installPath: g.installPath || "", engine: g.engine || "", version: "" })
        }
        for (const e of recent) {
            if (out.length >= 4) break
            if (e.externalUrl) continue
            add({ kind: e.packageInstalled ? "installed" : e.isInstalled ? "installable" : "new",
                  gameId: e.id || e.steamAppId, name: e.name || "", steamAppId: e.steamAppId || "",
                  installPath: e.installPath || "", engine: e.engine || "", version: e.version || "" })
        }
        return out
    }
    function _eyebrow(f) {
        if (f.kind === "installable") return qsTr("Bilgisayarında · Türkçe yama hazır")
        if (f.kind === "update") return qsTr("Yama güncellemesi hazır")
        if (f.kind === "installed") return qsTr("Türkçe yama kurulu")
        const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(f.version)
        if (!m) return qsTr("Yeni güncellenen yama")
        const d = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]))
        return qsTr("Yeni güncellendi · %1").arg(d.toLocaleDateString(Qt.locale("tr_TR"), "d MMMM"))
    }

    on_CountChanged: if (root.current >= root._count) root.current = 0

    HoverHandler { id: cardHover }
    Timer {
        interval: 8000
        repeat: true
        running: root.visible && root._count > 1 && !cardHover.hovered && root.Window.active
        onTriggered: root.current = (root.current + 1) % root._count
    }

    Layout.fillWidth: true
    Layout.horizontalStretchFactor: 2
    Layout.preferredHeight: layoutTopRowHeight

    radius: Dimensions.radiusSection
    color: Theme.surface

    GradientBorder {
        cornerRadius: root.radius
    }

    readonly property string _bannerUrl:
        "https://cdn.makineceviri.org/assets/banners/announcement.png?v=" + Date.now()

    // Banner image (hidden — rendered via MultiEffect mask)
    // cache: false — always fetch fresh from CDN (banner changes frequently)
    Image {
        id: bannerImg
        anchors.fill: parent
        source: root._bannerUrl
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(width, height)
        asynchronous: true
        cache: false
        mipmap: true
        visible: false
    }

    // Rounded corner mask
    Item {
        id: bannerMask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: Dimensions.radiusSection; color: "white" }
    }

    // Masked banner — image clipped to container radius
    MultiEffect {
        anchors.fill: bannerImg
        source: bannerImg
        maskEnabled: true
        maskSource: bannerMask
        visible: bannerImg.status === Image.Ready && opacity > 0
        opacity: root.current === 0 ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
    }

    // ── Featured games ───────────────────────────────────────────────────────
    // Same rounded mask as the banner; art is Steam's library hero, or the
    // cover blurred behind itself when Steam has none under a guessable path.
    Item {
        id: featuredLayer
        anchors.fill: parent
        visible: root._featured.length > 0
        layer.enabled: visible
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: bannerMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Repeater {
            model: root._featured

            Item {
                id: slide
                required property var modelData
                required property int index
                readonly property bool fallback: art.status === Image.Error
                property string coverSrc: ""
                function resolveCover() { slide.coverSrc = ImageCache.resolve(slide.modelData.steamAppId) }
                onFallbackChanged: if (slide.fallback) slide.resolveCover()

                anchors.fill: parent
                opacity: root.current === slide.index + 1 ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }

                // Fixed target, gated by `enabled`: see GameCard (NATIVE-74).
                Connections {
                    target: ImageCache
                    enabled: slide.fallback && slide.coverSrc === ""
                    function onImageReady(readyId) {
                        if (readyId === slide.modelData.steamAppId) slide.resolveCover()
                    }
                }

                Rectangle { anchors.fill: parent; color: Theme.surface }

                Image {
                    id: art
                    anchors.fill: parent
                    source: GameService.steamHeroUrl(slide.modelData.steamAppId)
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 1280
                    asynchronous: true
                }
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
                    blurMax: 48
                    blur: 1.0
                    brightness: -0.3
                }

                // Legibility: dark on the text side, clean on the art side.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0;  color: Theme.withAlpha(Theme.bgPrimary, 0.9) }
                        GradientStop { position: 0.45; color: Theme.withAlpha(Theme.bgPrimary, 0.55) }
                        GradientStop { position: 0.8;  color: Theme.withAlpha(Theme.bgPrimary, 0.0) }
                    }
                }

                Image {
                    visible: slide.fallback
                    anchors.right: parent.right
                    anchors.rightMargin: Dimensions.marginLG
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height - Dimensions.marginMD * 2
                    width: height * 130 / 185
                    source: slide.coverSrc
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.gameSelected(slide.modelData.gameId, slide.modelData.name,
                                                 slide.modelData.installPath, slide.modelData.engine)
                }

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: Dimensions.marginML
                    anchors.bottomMargin: Dimensions.marginMD + 2
                    width: parent.width * 0.62
                    spacing: Dimensions.spacingSM

                    Text {
                        width: parent.width
                        textFormat: Text.PlainText
                        text: root._eyebrow(slide.modelData)
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                        font.capitalization: Font.AllUppercase
                        color: Theme.darkMode ? Theme.accentLight : Theme.accentDark
                    }
                    Text {
                        width: parent.width
                        textFormat: Text.PlainText
                        text: slide.modelData.name
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.fontHeadline
                        font.weight: Font.Bold
                        font.letterSpacing: Dimensions.letterSpacingHeadline
                        color: Theme.textPrimary
                    }

                    // The one thing to do with this game
                    Rectangle {
                        id: cta
                        readonly property bool install: slide.modelData.kind === "installable"
                        width: ctaLabel.implicitWidth + 28
                        height: 30
                        radius: Dimensions.radiusMD
                        color: cta.install ? (ctaHover.hovered ? Theme.primaryHover : Theme.primary)
                                           : (ctaHover.hovered ? Theme.textPrimary20 : Theme.textPrimary12)
                        Accessible.role: Accessible.Button
                        Accessible.name: ctaLabel.text

                        Text {
                            id: ctaLabel
                            anchors.centerIn: parent
                            textFormat: Text.PlainText
                            text: cta.install ? qsTr("Türkçe Yamayı Kur")
                                : slide.modelData.kind === "update" ? qsTr("Yamayı Güncelle")
                                : qsTr("İncele")
                            font.pixelSize: Dimensions.fontSM
                            font.weight: Font.DemiBold
                            color: cta.install ? Theme.textOnColor : Theme.textPrimary
                        }
                        HoverHandler { id: ctaHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: {
                                const f = slide.modelData
                                if (cta.install)
                                    root.installRequested(f.gameId, f.name, f.installPath, f.engine)
                                else
                                    root.gameSelected(f.gameId, f.name, f.installPath, f.engine)
                            }
                        }
                    }
                }
            }
        }
    }

    // Top edge glass highlight
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.rightMargin: 1; anchors.topMargin: 1
        height: 1; radius: Dimensions.radiusSection; z: 5
        opacity: 0.7
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.2; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.18) }
            GradientStop { position: 0.8; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    MouseArea {
        id: bannerMa
        anchors.fill: parent
        enabled: root.current === 0
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
    }

    // Discord pill button — slides up from bottom-right on hover
    Item {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 54
        height: 54
        clip: true
        z: 10
        visible: root.current === 0

        Rectangle {
            id: discordBtn
            width: 40
            height: 40
            radius: Dimensions.radiusMD
            color: Theme.discordColor
            x: 0
            y: bannerMa.containsMouse ? 0 : parent.height

            Behavior on y {
                NumberAnimation {
                    duration: Dimensions.animNormal
                    easing.type: Easing.OutCubic
                }
            }

            Image {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/MakineLauncher/resources/icons/discord-white.svg"
                width: 22; height: 22
                sourceSize: Qt.size(22, 22)
                mipmap: true
            }
        }
    }

    // Which slide — top right, clear of the Discord button and the text, on a
    // dark pill so it reads on a white cover as well as a dark one.
    Rectangle {
        anchors.fill: dots
        anchors.margins: -6
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.45)
        z: 11
        visible: dots.visible
    }
    Row {
        id: dots
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: Dimensions.marginMD
        anchors.topMargin: Dimensions.marginMD
        spacing: 5
        z: 12
        visible: root._count > 1

        Repeater {
            model: root._count

            Rectangle {
                id: dot
                required property int index
                readonly property bool active: dot.index === root.current
                anchors.verticalCenter: parent.verticalCenter
                width: dot.active ? 18 : 6
                height: 6
                radius: 3
                color: dot.active ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.45)
                Behavior on width { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Accessible.role: Accessible.Button
                Accessible.name: dot.index === 0 ? qsTr("Duyuru") : qsTr("Öne çıkan %1").arg(dot.index)

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    margin: 6
                    onTapped: root.current = dot.index
                }
            }
        }
    }
}
