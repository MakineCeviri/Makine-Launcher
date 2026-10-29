// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
import "home"
pragma ComponentBehavior: Bound

/**
 * AllGames.qml - "Tüm Oyunlar": every Turkish patch in the catalogue at once
 *
 * The whole catalogue as a grid, the name under each cover, searched from
 * the sidebar (`query`; CatalogProxyModel: Turkish folding, "rdr" for Red
 * Dead Redemption). The header holds the filter as icons (all, on this PC,
 * patch installed, installed by the launcher, on another site; the word and
 * count on hover), the order (a pop-up: A–Z or newest first) and the view:
 * covers, or a list with the patch's state, date and size. A sentence under the filters says
 * what the one on show means. Filters and games share one container; rows
 * fade out under its top bar and at its foot instead of being cut off.
 */
Item {
    id: allGames

    property real contentMargin: 16
    // What the sidebar's search holds
    property string query: ""
    // "grid" covers · "list" rows; kept between runs
    property string view: "grid"
    Settings {
        category: "allGames"
        property alias view: allGames.view
    }

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    // "Tüm oyunları göster" lets go of the search too: the sidebar clears it
    signal searchClearRequested()

    // "" every game · mine · installed · local · external (CatalogProxyModel.statusFilter)
    property string filter: ""
    // "" A–Z · newest
    property string sort: ""
    property string _query: ""

    // Opened from elsewhere (a shelf's "Tümünü gör"): carry over what was
    // asked for. The search comes from the sidebar (Main.qml sets it).
    function open(filter, sort) {
        allGames.filter = filter || ""
        allGames.sort = sort || ""
        allGames._view.positionViewAtBeginning()
    }
    // Down or Enter in the sidebar's search: on to the results
    function focusResults() { allGames._view.forceActiveFocus() }

    onQueryChanged: searchDebounce.restart()

    CatalogProxyModel {
        id: proxy
        sourceModel: GameService.supportedGamesModel
        searchFilter: allGames._query
        statusFilter: allGames.filter
        sortMode: allGames.sort
    }

    Timer {
        id: searchDebounce
        interval: 120
        onTriggered: allGames._query = allGames.query.trim()
    }

    // Per filter; re-counted when the catalogue or this PC's games change.
    // proxy.sourceModel is among the inputs: until it is wired every count is 0.
    readonly property var _counts: _count(GameService.library, GameService.supportedGamesModel.count,
                                          proxy.sourceModel)
    function _count() {
        return {
            "": proxy.countWithStatus(""),
            "mine": proxy.countWithStatus("mine"),
            "installed": proxy.countWithStatus("installed"),
            "local": proxy.countWithStatus("local"),
            "soon": proxy.countWithStatus("soon")
        }
    }
    readonly property var _sorts: [ { value: "", text: qsTr("A–Z") }, { value: "newest", text: qsTr("En yeni yama") } ]
    readonly property var _views: [ { value: "grid", icon: Icons.layoutGrid, tip: qsTr("Kapak görünümü") },
                                    { value: "list", icon: Icons.list, tip: qsTr("Liste görünümü") } ]
    readonly property var _view: allGames.view === "list" ? list : grid

    function _status(m) {
        return m.packageInstalled ? "installed"
             : m.isInstalled ? "installable"
             : m.comingSoon ? "soon" : ""
    }
    function _date(v) {
        const m = /^(\d{4})-(\d{2})-(\d{2})/.exec(v || "")
        return m ? new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]))
                     .toLocaleDateString(Qt.locale("tr_TR"), "d MMM yyyy") : "—"
    }
    function _size(bytes) {
        if (!(bytes > 0)) return "—"
        const mb = bytes / 1048576
        return mb >= 1024 ? Number(mb / 1024).toLocaleString(Qt.locale("tr_TR"), "f", 1) + " GB"
             : mb >= 1 ? Number(mb).toLocaleString(Qt.locale("tr_TR"), "f", 1) + " MB"
             : Math.max(1, Math.round(bytes / 1024)) + " KB"
    }
    readonly property var _filters: {
        // Icons in the header; the word and the count on hover
        const all = [
            { value: "", text: qsTr("Tümü"), icon: Icons.layers },
            { value: "mine", text: qsTr("Bilgisayarımda"), icon: Icons.monitor },
            { value: "installed", text: qsTr("Yaması kurulu"), icon: Icons.circleCheck },
            { value: "local", text: qsTr("Launcher'dan kurulur"), icon: Icons.download },
            { value: "soon", text: qsTr("Yakında"), icon: Icons.clock }
        ]
        const out = []
        for (const f of all) {
            const n = allGames._counts[f.value] ?? 0
            if (f.value === "" || n > 0 || allGames.filter === f.value)
                out.push({ value: f.value, icon: f.icon, tip: qsTr("%1 · %2 oyun").arg(f.text).arg(n) })
        }
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            Layout.topMargin: 52
            Layout.leftMargin: Dimensions.pageMargin
            Layout.rightMargin: Dimensions.pageMargin
            title: qsTr("Tüm Oyunlar")
            subtitle: allGames._query !== "" || allGames.filter !== ""
                      ? qsTr("%1 oyun gösteriliyor · toplam %2 Türkçe yama")
                            .arg(proxy.sourceCount).arg(allGames._counts[""])
                      : qsTr("%1 oyun için Türkçe yama · %2 tanesi bilgisayarında")
                            .arg(allGames._counts[""]).arg(allGames._counts["mine"])

            SegmentedControl {
                model: allGames._filters
                currentValue: allGames.filter
                onActivated: (value) => allGames.filter = value
            }
            MenuButton {
                icon: Icons.arrowUpDown
                toolTip: qsTr("Sıralama")
                model: allGames._sorts
                currentValue: allGames.sort
                onActivated: (value) => allGames.sort = value
            }
            SegmentedControl {
                model: allGames._views
                currentValue: allGames.view
                onActivated: (value) => allGames.view = value
            }
        }

    Card {
        id: box
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.topMargin: 22
        Layout.leftMargin: Dimensions.pageMargin
        Layout.rightMargin: Dimensions.pageMargin
        Layout.bottomMargin: 20
        radius: Dimensions.radiusSection
        glow: true

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 16
        spacing: 0

        // What the filter on show means (the filters sit in the header)
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 18
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
            text: allGames.filter === "mine"
                  ? qsTr("Bilgisayarında bulduğumuz oyunlar. Adının önünde yeşil tik olanlarda Türkçe yama zaten kurulu.")
                : allGames.filter === "installed"
                  ? qsTr("Türkçe yaması kurulu oyunların. Güncellemek ya da kaldırmak için oyunun sayfasını aç.")
                : allGames.filter === "local"
                  ? qsTr("Bu yamaları launcher kendisi indirip kurar: oyunun sayfasındaki tek düğme yeter, orijinal dosyalar yedeklenir.")
                : allGames.filter === "soon"
                  ? qsTr("Bu oyunların Türkçe yamaları hazırlanıyor. Hazır olunca burada kurulur olarak görünür.")
                : qsTr("Kataloğumuzdaki bütün Türkçe yamalar. Bir oyuna tıkla: sayfasında yamanın nasıl kurulduğunu ve emeği geçenleri görürsün.")
        }

        FadeLine { Layout.topMargin: 14; Layout.leftMargin: 12; Layout.rightMargin: 12 }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 1
            Layout.rightMargin: 1
            Layout.bottomMargin: 1

        GridView {
            id: grid
            anchors.fill: parent
            visible: allGames.view !== "list"
            clip: true
            model: proxy
            activeFocusOnTab: true
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: 800
            ScrollBar.vertical: StyledScrollBar {}

            leftMargin: 18 - _gap / 2
            rightMargin: 18 - _gap / 2

            readonly property int _gap: 22
            readonly property real _avail: width - leftMargin - rightMargin
            readonly property int _columns: Math.max(2, Math.floor(_avail / (150 + _gap)))
            readonly property real _cardW: cellWidth - _gap
            cellWidth: Math.floor(_avail / _columns)
            // Room under a two-line name for the cut between rows
            cellHeight: Math.round(_cardW * 185 / 130) + 86
            // Bumped when a cell's text changes height: the cuts re-measure
            // their rows (itemAtIndex notifies nothing)
            property int _textTick: 0

            function openAt(i) {
                if (i < 0 || i >= count) return
                const it = itemAtIndex(i)
                if (it) it.open()
            }
            Keys.onReturnPressed: grid.openAt(grid.currentIndex)
            Keys.onSpacePressed: grid.openAt(grid.currentIndex)

            header: Item { width: grid.width; height: 10 }
            footer: Item { width: grid.width; height: 24 }

            delegate: Item {
                id: cell
                required property var model
                required property int index

                width: grid.cellWidth
                height: grid.cellHeight

                // Where this cell's words end (a name may take two lines)
                readonly property real textBottom: coverItem.y + coverItem.implicitHeight
                onTextBottomChanged: grid._textTick++
                onIndexChanged: grid._textTick++

                // Between rows, the Makine cut: a lit hairline fading at both
                // ends, its middle in the accent with a soft glow — on one
                // pixel row, halfway between the row's lowest name and the
                // next row's covers (drawn by each row's first cell, across)
                Item {
                    id: cut
                    readonly property bool _rowStart: cell.index % grid._columns === 0
                    readonly property real _rowBottom: {
                        grid._textTick
                        if (!cut._rowStart) return cell.textBottom
                        let bottom = cell.textBottom
                        const end = Math.min(cell.index + grid._columns, grid.count)
                        for (let i = cell.index + 1; i < end; ++i) {
                            const other = grid.itemAtIndex(i)
                            if (other) bottom = Math.max(bottom, other.textBottom)
                        }
                        return bottom
                    }
                    visible: cut._rowStart
                             && Math.floor(cell.index / grid._columns) < Math.floor((grid.count - 1) / grid._columns)
                    width: Math.round(grid._avail * 0.62)
                    height: 1
                    x: Math.round((grid._avail - width) / 2)
                    // The next row's covers start 8 below this cell
                    y: Math.round((cut._rowBottom + cell.height + 8) / 2)
                    FadeLine {
                        y: 0
                        width: parent.width
                        lit: true
                    }
                    RectangularShadow {
                        x: Math.round((parent.width - width) / 2)
                        y: 0
                        width: 64; height: 1
                        blur: 9
                        spread: 1
                        color: Theme.withAlpha(Theme.primary, 0.55)
                    }
                    Rectangle {
                        x: Math.round((parent.width - width) / 2)
                        y: 0
                        width: 64; height: 1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: Theme.primary }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }
                }

                function open() {
                    allGames.gameSelected(cell.model.gameId || cell.model.steamAppId || "",
                                          cell.model.name || "", cell.model.installPath || "",
                                          cell.model.engine || "")
                }

                HomeCover {
                    id: coverItem
                    x: Math.round((cell.width - width) / 2)
                    y: 8
                    width: grid._cardW
                    gameId: cell.model.gameId || ""
                    name: cell.model.name || ""
                    steamAppId: cell.model.steamAppId || ""
                    status: allGames._status(cell.model)
                    activeFocusOnTab: false
                    highlighted: grid.activeFocus && cell.index === grid.currentIndex
                    onClicked: {
                        grid.currentIndex = cell.index
                        cell.open()
                    }
                }
            }
        }

        ListView {
            id: list
            anchors.fill: parent
            visible: allGames.view === "list"
            clip: true
            model: proxy
            activeFocusOnTab: true
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: 800
            ScrollBar.vertical: StyledScrollBar {}
            leftMargin: 12
            rightMargin: 12
            Keys.onReturnPressed: if (list.currentItem) list.currentItem.open()
            Keys.onSpacePressed: if (list.currentItem) list.currentItem.open()

            // Chapters: by first letter for A–Z, by the patch's day for newest
            section.property: allGames.sort === "newest" ? "version" : "name"
            section.criteria: allGames.sort === "newest" ? ViewSection.FullString : ViewSection.FirstCharacter
            section.delegate: Item {
                id: chapter
                required property string section
                width: list.width - list.leftMargin - list.rightMargin
                height: 52
                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    anchors.leftMargin: 6
                    spacing: 14
                    Text {
                        textFormat: Text.PlainText
                        text: allGames.sort === "newest" ? allGames._date(chapter.section)
                                                         : chapter.section.toLocaleUpperCase(Qt.locale("tr_TR"))
                        font.family: Dimensions.fontDisplay
                        font.pixelSize: allGames.sort === "newest" ? Dimensions.title3 : Dimensions.title1
                        font.weight: Font.Bold
                        color: Theme.primaryText
                    }
                    FadeLine { Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; lit: true }
                }
            }
            header: Item { width: list.width; height: 4 }
            footer: Item { width: list.width; height: 24 }

            // A game: its own small card — the cover bled to the left, the
            // name, its state and facts as chips, an arrow that comes forward.
            // Under the pointer the card lifts and its cover washes through it.
            delegate: Item {
                id: row
                required property var model
                required property int index
                readonly property string _state: allGames._status(row.model)
                readonly property bool _hot: rowHover.hovered || (list.activeFocus && row.index === list.currentIndex)

                width: list.width - list.leftMargin - list.rightMargin
                height: 80

                function open() {
                    allGames.gameSelected(row.model.gameId || row.model.steamAppId || "",
                                          row.model.name || "", row.model.installPath || "",
                                          row.model.engine || "")
                }

                // The cover from the image cache, as HomeCover gets it
                property string _src: ""
                function _resolve() {
                    const id = row.model.steamAppId || row.model.gameId
                    row._src = id ? ImageCache.resolve(id) : ""
                }
                Component.onCompleted: _resolve()
                ListView.onPooled: row._src = ""
                ListView.onReused: row._resolve()
                Connections {
                    target: ImageCache
                    enabled: row._src === ""
                    function onImageReady(readyId) {
                        if (readyId === (row.model.steamAppId || row.model.gameId))
                            row._resolve()
                    }
                }

                Item {
                    id: card
                    anchors.fill: parent
                    anchors.bottomMargin: 8
                    scale: rowTap.pressed ? 0.985 : 1
                    Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutBack } }
                    transform: Translate {
                        y: row._hot ? -2 : 0
                        Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                    }

                    Rectangle {
                        id: cardBg
                        anchors.fill: parent
                        radius: Dimensions.radiusLG
                        color: row._hot ? Theme.fillTertiary : Theme.fillQuaternary
                        border.width: 1
                        border.color: row._hot ? Theme.withAlpha(Theme.primary, 0.4) : Theme.withAlpha(Theme.textPrimary, 0.05)
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    // The cover's colours wash through the card under the pointer
                    Loader {
                        anchors.fill: parent
                        active: row._hot && row._src !== ""
                        opacity: row._hot ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
                        sourceComponent: Item {
                            opacity: Theme.darkMode ? 0.2 : 0.14
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                blurEnabled: true
                                // Inside the card only: padding would grow
                                // the mask past its edges
                                autoPaddingEnabled: false
                                blurMax: 64
                                blur: 1.0
                                saturation: 0.3
                                maskEnabled: true
                                maskSource: cardMask
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                            }
                            Image {
                                anchors.fill: parent
                                source: row._src
                                fillMode: Image.PreserveAspectCrop
                                sourceSize: Qt.size(64, 91)
                                asynchronous: true
                            }
                        }
                    }
                    Item {
                        id: cardMask
                        anchors.fill: parent
                        visible: false
                        layer.enabled: true
                        Rectangle { anchors.fill: parent; radius: Dimensions.radiusLG; color: "black" }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 14
                        spacing: 14

                        // The cover, standing in the card's left side
                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 56
                            radius: 6
                            color: Theme.surfaceLight
                            Image {
                                anchors.fill: parent
                                source: row._src
                                fillMode: Image.PreserveAspectCrop
                                sourceSize: Qt.size(80, 114)
                                asynchronous: true
                                mipmap: true
                                scale: row._hot ? 1.06 : 1
                                Behavior on scale { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                text: row.model.name || ""
                                elide: Text.ElideRight
                                font.pixelSize: Dimensions.headline
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Row {
                                spacing: 6
                                StatusBadge {
                                    visible: row._state !== ""
                                    tint: row._state === "installed" ? Theme.success
                                        : row._state === "installable" ? Theme.primary : Theme.textMuted
                                    text: row._state === "installed" ? qsTr("Yama kurulu")
                                        : row._state === "installable" ? qsTr("Bilgisayarında")
                                        : qsTr("Dış sitede")
                                }
                                Fact { glyph: Icons.calendar; text: allGames._date(row.model.version) }
                                Fact { glyph: Icons.hardDrive; text: allGames._size(row.model.downloadSize) }
                            }
                        }

                        // The arrow comes forward under the pointer
                        Rectangle {
                            implicitWidth: 30
                            implicitHeight: 30
                            radius: 15
                            color: row._hot ? Theme.withAlpha(Theme.primary, 0.16) : "transparent"
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                            Icon {
                                anchors.centerIn: parent
                                anchors.horizontalCenterOffset: row._hot ? 1 : -2
                                glyph: Icons.chevronRight
                                size: 15
                                color: row._hot ? Theme.primaryText : Theme.textTertiary
                                Behavior on anchors.horizontalCenterOffset { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutBack } }
                            }
                        }
                    }
                }

                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    id: rowTap
                    onTapped: {
                        list.currentIndex = row.index
                        row.open()
                    }
                }
            }
        }

        // Nothing under this search or filter
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            visible: proxy.count === 0 && GameService.supportedGamesModel.count > 0
            spacing: 12

            Icon {
                Layout.alignment: Qt.AlignHCenter
                glyph: Icons.search
                size: 40
                color: Theme.textTertiary
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                text: allGames._query !== "" ? qsTr("Sonuç yok") : qsTr("Bu filtrede oyun yok")
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                color: Theme.textPrimary
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                font.pixelSize: Dimensions.callout
                color: Theme.textMuted
                text: allGames._query !== ""
                      ? qsTr("\"%1\" için oyun bulamadık. Yazımı değiştir ya da filtreyi kaldır.").arg(allGames._query)
                      : qsTr("Başka bir filtre seç ya da bütün oyunlara dön.")
            }
            AppButton {
                icon: Icons.layoutGrid
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                style: "tinted"
                text: qsTr("Tüm oyunları göster")
                onClicked: {
                    allGames.searchClearRequested()
                    allGames._query = ""
                    allGames.filter = ""
                }
            }
        }

        // The catalogue has not arrived yet
        ColumnLayout {
            anchors.centerIn: parent
            visible: GameService.supportedGamesModel.count === 0
            spacing: 12
            Spinner { Layout.alignment: Qt.AlignHCenter; size: 24 }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Katalog yükleniyor…")
                font.pixelSize: Dimensions.callout
                color: Theme.textMuted
            }
        }

        // Rows fade out under the bar and at the container's foot
        EdgeFade {
            target: allGames._view
            topFade: allGames._view.atYBeginning ? 0 : 26
            bottomFade: allGames._view.atYEnd ? 0 : 34
        }
        }
    }
    }
    }

    // A fact beside the state: a small sign and a word
    component Fact: Row {
        id: fact
        property string glyph: ""
        property string text: ""
        spacing: 4
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            glyph: fact.glyph
            size: 12
            color: Theme.textMuted
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: fact.text
            font.pixelSize: Dimensions.footnote
            font.family: Dimensions.fontText
            font.features: { "tnum": 1 }
            color: Theme.textMuted
        }
    }
}
