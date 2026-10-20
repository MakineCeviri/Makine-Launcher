// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
import "home"
pragma ComponentBehavior: Bound

/**
 * HomePage.qml — Ana Sayfa.
 *
 * Top to bottom: search over every patch · what the running-game watcher has
 * to say (only when it has something) · a lead carousel · this PC's games
 * that have a patch, with their state · the newest patches · every patch,
 * A–Z. Searching folds everything above the grid away and the grid becomes
 * the results.
 *
 * One GridView is the page: everything above the grid is its header, so the
 * page scrolls as one piece and only the covers on screen are instantiated.
 */
Item {
    id: homePage

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    signal settingsRequested()

    // ===== LAYOUT ===========================================================
    // Content is capped and centred; the grid's columns follow its width.
    readonly property real _pad: Math.max(Dimensions.marginXL, (width - 1440) / 2)
    readonly property real _inner: width - _pad * 2
    readonly property real _gap: Dimensions.marginLG
    readonly property int _cols: Math.max(2, Math.floor((_inner + _gap) / (150 + _gap)))
    readonly property real _cardW: (_inner - (_cols - 1) * _gap) / _cols
    readonly property real _shelfCardW: Math.max(140, Math.min(172, _cardW))

    property string _query: ""
    readonly property bool _searching: _query.length > 0
    readonly property int _total: GameService.supportedGamesModel
                                  ? GameService.supportedGamesModel.count : 0

    on_SearchingChanged: _toTop()

    function _toTop() {
        grid.pinnedTop = true
        grid.contentY = grid.originY
    }

    // Ctrl+F (Main.qml → HomeScreen)
    function focusSearch() {
        _toTop()
        if (grid.headerItem)
            grid.headerItem.focusSearch()
    }

    function _open(item) {
        homePage.gameSelected(item.gameId, item.name, item.installPath || "", item.engine || "")
    }

    // ===== FORMATTING =======================================================
    readonly property var _tr: Qt.locale("tr_TR")

    // "2026-10-08" → Date, or null for anything else.
    function _parseDate(v) {
        const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(v || "")
        return m ? new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3])) : null
    }
    function _daysAgo(d) {
        const today = new Date()
        today.setHours(0, 0, 0, 0)
        return Math.round((today - d) / 86400000)
    }
    // Recent dates read as time passed; older ones as a date.
    function _dateText(v, short) {
        const d = _parseDate(v)
        if (!d) return ""
        const days = _daysAgo(d)
        if (days <= 0) return qsTr("Bugün")
        if (days === 1) return qsTr("Dün")
        if (days < 7) return qsTr("%1 gün önce").arg(days)
        return d.toLocaleDateString(_tr, short ? "d MMMM" : "d MMMM yyyy")
    }
    function _sizeText(bytes) {
        if (!bytes || bytes <= 0) return ""
        const mb = bytes / 1048576
        if (mb < 1024) return qsTr("%1 MB").arg(Math.max(1, Math.round(mb)))
        return qsTr("%1 GB").arg((mb / 1024).toFixed(1).replace(".", ","))
    }

    // ===== DATA =============================================================
    // This PC's games that have a patch, the ones needing a step first.
    readonly property var _mine: _computeMine(GameService.games, GameService.installedTranslations)
    function _computeMine(games, installed) {
        const patched = {}
        for (const t of installed)
            patched[t.id] = t.hasUpdate === true
        const rank = { "update": 0, "ready": 1, "installed": 2 }
        const out = []
        for (const g of games) {
            const status = (g.id in patched) ? (patched[g.id] ? "update" : "installed") : "ready"
            out.push({ gameId: g.id, steamAppId: g.steamAppId || "", name: g.name || "",
                       installPath: g.installPath || "", engine: g.engine || "", status: status })
        }
        out.sort((a, b) => rank[a.status] - rank[b.status] || a.name.localeCompare(b.name))
        return out
    }
    readonly property int _updates: _mine.filter(g => g.status === "update").length
    readonly property int _installable: _mine.filter(g => g.status === "ready").length

    // Newest packages first (catalogue "v" date), and every package's date and
    // size by app id for the lead. Filled here, outside any binding: reading
    // the catalogue can rebuild its model, which re-entered the slides binding.
    property var _recent: []
    property var _packageInfo: ({})
    function _refreshRecent() {
        const out = []
        const info = {}
        for (const e of GameService.recentlyUpdatedGames(-1)) {
            info[e.steamAppId] = { version: e.version || "", size: e.downloadSize || 0 }
            if (out.length >= 16) continue
            out.push({ gameId: e.id || e.steamAppId, steamAppId: e.steamAppId || "",
                       name: e.name || "", installPath: e.installPath || "", engine: e.engine || "",
                       status: e.packageInstalled ? "installed" : (e.isInstalled ? "installable" : ""),
                       subtitle: _dateText(e.version, true), version: e.version || "",
                       size: e.downloadSize || 0, external: !!e.externalUrl })
        }
        _packageInfo = info
        _recent = out
    }
    Component.onCompleted: _refreshRecent()
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { homePage._refreshRecent() }
        function onDataChanged() { homePage._refreshRecent() }
    }

    // The lead: this PC's games that wait on a step, then the newest patches.
    readonly property var _slides: _computeSlides(_mine, _recent)
    function _computeSlides(mine, recent) {
        const out = []
        const seen = {}
        const add = (item, kind) => {
            // Steam art only exists for Steam app ids.
            if (!/^\d+$/.test(item.steamAppId) || seen[item.steamAppId]) return
            seen[item.steamAppId] = true
            const known = _packageInfo[item.steamAppId] || {}
            const version = item.version || known.version || ""
            const size = item.size || known.size || 0
            const d = _parseDate(version)
            const eyebrow = kind === "installable" ? qsTr("Bilgisayarında · Türkçe yama hazır")
                          : kind === "update" ? qsTr("Yama güncellemesi hazır")
                          : kind === "installed" ? qsTr("Türkçe yama kurulu")
                          : d && _daysAgo(d) <= 14 ? qsTr("Yeni güncellendi")
                          : qsTr("Öne çıkan yama")
            const meta = []
            if (version) meta.push(qsTr("Güncelleme: %1").arg(_dateText(version, false)))
            if (size > 0) meta.push(_sizeText(size))
            out.push({ gameId: item.gameId, steamAppId: item.steamAppId, name: item.name,
                       installPath: item.installPath || "", engine: item.engine || "",
                       kind: kind, eyebrow: eyebrow, meta: meta.join("  ·  ") })
        }
        for (const g of mine) {
            if (out.length >= 3) break
            if (g.status === "update" || g.status === "ready") add(g, g.status === "ready" ? "installable" : "update")
        }
        for (const r of recent) {
            if (out.length >= 5) break
            if (!r.external) add(r, r.status === "" ? "new" : r.status)
        }
        return out
    }

    // ===== THE PAGE =========================================================
    CatalogProxyModel {
        id: catalogProxy
        sourceModel: GameService.supportedGamesModel
        searchFilter: homePage._query
        rowOffset: 0
        rowLimit: -1
        wrapAround: false
    }

    GridView {
        id: grid
        anchors.fill: parent
        clip: true
        model: catalogProxy
        cellWidth: width / homePage._cols
        cellHeight: Math.round(homePage._cardW * 185 / 130) + Dimensions.spacingMD + 36 + homePage._gap
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 800
        reuseItems: true
        ScrollBar.vertical: StyledScrollBar {}

        // GridView holds its first cell still when the header changes height,
        // so sections arriving after the first frame (games, the lead) grew
        // upward and opened the page scrolled half way down. Until the user
        // scrolls, the page stays at its top.
        property bool pinnedTop: true
        onOriginYChanged: if (pinnedTop) contentY = originY
        onMovementStarted: pinnedTop = false
        onMovementEnded: pinnedTop = contentY <= originY + 1

        opacity: 0
        Component.onCompleted: {
            if (homePage.animationsEnabled) entryFade.start()
            else grid.opacity = 1
        }
        NumberAnimation {
            id: entryFade
            target: grid
            property: "opacity"
            from: 0; to: 1
            duration: Dimensions.animSlow
            easing.type: Dimensions.easeStandard
        }

        // Cells span the full width so the wheel works over the side margins;
        // each card is pulled in so the grid sits inside the content column.
        delegate: Item {
            id: cell
            required property int index
            required property string gameId
            required property string name
            required property string steamAppId
            required property string installPath
            required property string engine
            required property bool isInstalled
            required property bool packageInstalled

            readonly property int column: cell.index % homePage._cols
            width: grid.cellWidth
            height: grid.cellHeight

            HomeCover {
                x: homePage._pad + cell.column * (homePage._cardW + homePage._gap) - cell.column * grid.cellWidth
                width: homePage._cardW
                gameId: cell.gameId
                steamAppId: cell.steamAppId
                name: cell.name
                status: cell.packageInstalled ? "installed" : (cell.isInstalled ? "installable" : "")
                onClicked: homePage.gameSelected(cell.gameId, cell.name, cell.installPath, cell.engine)
                onActiveFocusChanged: if (activeFocus) grid.positionViewAtIndex(cell.index, GridView.Contain)
            }
        }

        header: Column {
            id: headerCol
            width: grid.width
            topPadding: Dimensions.marginLG
            bottomPadding: Dimensions.marginLG
            spacing: Dimensions.spacingPage + 4

            function focusSearch() {
                searchField.forceActiveFocus()
                searchField.selectAll()
            }

            // ── Search ──────────────────────────────────────────────────────
            Item {
                x: homePage._pad
                width: homePage._inner
                height: searchField.implicitHeight

                HomeSearchField {
                    id: searchField
                    width: Math.min(560, parent.width - (addGame.visible ? addGame.width + Dimensions.spacingXL : 0))
                    total: homePage._total
                    onQueryChanged: homePage._query = searchField.query
                }
                HomeButton {
                    id: addGame
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    kind: "secondary"
                    icon: ""
                    text: qsTr("Oyun ekle")
                    visible: !homePage._searching
                    onClicked: homePage.manualFolderRequested()
                }
            }

            HomeNotice {
                x: homePage._pad
                width: homePage._inner
            }

            HomeHero {
                x: homePage._pad
                width: homePage._inner
                height: Math.round(Math.max(300, Math.min(440, homePage._inner * 0.36)))
                visible: !homePage._searching && homePage._slides.length > 0
                slides: homePage._slides
                onOpenGame: (slide) => homePage._open(slide)
                onInstallGame: (slide) => homePage.installAndShowDetail(
                    slide.gameId, slide.name, slide.installPath, slide.engine)
            }

            // Batch install/uninstall progress (only while one runs or has results)
            ColumnLayout {
                x: homePage._pad
                width: homePage._inner
                visible: batchPanel.shouldShow && !homePage._searching
                BatchOperationsPanel {
                    id: batchPanel
                    animationsEnabled: homePage.animationsEnabled
                }
            }

            // ── This PC ─────────────────────────────────────────────────────
            HomeShelf {
                x: homePage._pad
                width: homePage._inner
                visible: !homePage._searching && homePage._mine.length > 0
                cardWidth: homePage._shelfCardW
                title: qsTr("Bilgisayarındaki oyunlar")
                subtitle: homePage._updates > 0
                    ? qsTr("%1 oyunun Türkçe yaması var · %2 yama güncellenmeli")
                          .arg(homePage._mine.length).arg(homePage._updates)
                    : homePage._installable > 0
                    ? qsTr("%1 oyunun Türkçe yaması var · %2 tanesi kurulmaya hazır")
                          .arg(homePage._mine.length).arg(homePage._installable)
                    : qsTr("%1 oyunun Türkçe yaması kurulu").arg(homePage._mine.length)
                items: homePage._mine
                onGameClicked: (item) => homePage._open(item)

                HomeButton {
                    kind: "ghost"
                    icon: ""
                    text: GameService.isScanning ? qsTr("Taranıyor…") : qsTr("Yeniden tara")
                    onClicked: if (!GameService.isScanning) GameService.scanAllLibraries()
                }
            }

            // No game of theirs has a patch (yet): say so, and the way forward.
            Rectangle {
                x: homePage._pad
                width: homePage._inner
                visible: !homePage._searching && homePage._mine.length === 0 && homePage._total > 0
                implicitHeight: emptyRow.implicitHeight + Dimensions.paddingXL * 2
                height: implicitHeight
                radius: Dimensions.radiusSection
                color: Theme.surface
                border.width: 1
                border.color: Theme.textPrimary08

                RowLayout {
                    id: emptyRow
                    anchors.fill: parent
                    anchors.margins: Dimensions.paddingXL
                    spacing: Dimensions.spacingXL

                    Text {
                        Layout.alignment: Qt.AlignVCenter
                        textFormat: Text.PlainText
                        text: ""
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 28
                        color: Theme.textMuted
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingXS
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: GameService.isScanning ? qsTr("Oyunların taranıyor…")
                                                         : qsTr("Bilgisayarında Türkçe yaması olan bir oyun bulamadık")
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.fontSubtitle
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Steam, Epic ve GOG kütüphaneleri kendiliğinden taranır. Oyunun başka bir klasördeyse onu ekleyebilirsin.")
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                        }
                    }
                    HomeButton {
                        Layout.alignment: Qt.AlignVCenter
                        kind: "secondary"
                        icon: ""
                        text: qsTr("Oyun klasörü ekle")
                        onClicked: homePage.manualFolderRequested()
                    }
                }
            }

            // ── Newest ──────────────────────────────────────────────────────
            HomeShelf {
                x: homePage._pad
                width: homePage._inner
                visible: !homePage._searching && homePage._recent.length > 0
                cardWidth: homePage._shelfCardW
                title: qsTr("Yeni ve güncellenen yamalar")
                subtitle: qsTr("Son eklenen ve düzeltilen Türkçe yamalar")
                items: homePage._recent
                onGameClicked: (item) => homePage._open(item)
            }

            // ── Every patch / results ───────────────────────────────────────
            HomeSectionHeader {
                x: homePage._pad
                width: homePage._inner
                title: homePage._searching ? qsTr("“%1” için sonuçlar").arg(homePage._query)
                                           : qsTr("Tüm Türkçe yamalar")
                subtitle: homePage._total === 0 ? qsTr("Katalog yükleniyor…")
                        : !homePage._searching ? qsTr("%1 oyun · A'dan Z'ye").arg(homePage._total)
                        : catalogProxy.count > 0 ? qsTr("%1 oyun bulundu").arg(catalogProxy.count)
                        : qsTr("Eşleşen oyun yok. Kısaltma ya da farklı bir yazımla dene: \"rdr2\", \"witcher\"…")
            }
        }

        footer: Column {
            width: grid.width
            topPadding: Dimensions.marginXL
            bottomPadding: Dimensions.marginXL
            spacing: Dimensions.spacingXL

            // Community: where announcements, requests and help live.
            Rectangle {
                x: homePage._pad
                width: homePage._inner
                visible: !homePage._searching
                height: communityRow.implicitHeight + Dimensions.paddingXL * 2
                radius: Dimensions.radiusSection
                color: Theme.surface
                border.width: 1
                border.color: Theme.textPrimary08

                RowLayout {
                    id: communityRow
                    anchors.fill: parent
                    anchors.margins: Dimensions.paddingXL
                    spacing: Dimensions.spacingXL

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingXS
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Makine Çeviri topluluğu")
                            font.pixelSize: Dimensions.fontSubtitle
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Duyurular, yama istekleri ve destek Discord sunucumuzda.")
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textMuted
                        }
                    }
                    Row {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Dimensions.spacingMD

                        HomeButton {
                            kind: "primary"
                            fill: Theme.discordColor
                            image: "qrc:/qt/qml/MakineLauncher/resources/icons/discord-white.svg"
                            text: qsTr("Discord'a katıl")
                            onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                        }
                        HomeButton {
                            kind: "secondary"
                            icon: ""
                            text: qsTr("Web sitesi")
                            onClicked: Qt.openUrlExternally(Dimensions.websiteUrl)
                        }
                        HomeButton {
                            kind: "secondary"
                            icon: ""
                            text: qsTr("Destek ol")
                            onClicked: Qt.openUrlExternally(Dimensions.donatePageUrl)
                        }
                    }
                }
            }

            Text {
                x: homePage._pad
                width: homePage._inner
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                text: qsTr("Güvenliğin için yamaları yalnızca Makine Launcher ve makineceviri.org üzerinden indir.")
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
            }
        }
    }
}
