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
 * AllGames.qml - "Tüm Oyunlar": every Turkish patch in the catalogue at once
 *
 * The whole catalogue as a grid, the name under each cover, searchable
 * (CatalogProxyModel: Turkish folding, "rdr" for Red Dead Redemption) and
 * filtered on top: on this PC, patch installed, installed by the launcher or
 * from another site; A–Z or newest first. A sentence under the filters says
 * what the one on show means. Filters and grid share one container; rows
 * fade out under its top bar and at its foot instead of being cut off.
 */
Item {
    id: allGames

    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)

    // "" every game · mine · installed · local · external (CatalogProxyModel.statusFilter)
    property string filter: ""
    // "" A–Z · newest
    property string sort: ""
    property string _query: ""

    // Opened from elsewhere (a shelf's "Tümünü gör", the home search):
    // carry over what was asked for. A query keeps the typing going here.
    function open(filter, sort, query) {
        allGames.filter = filter || ""
        allGames.sort = sort || ""
        search.text = query || ""
        allGames._query = search.text.trim()
        grid.positionViewAtBeginning()
        if (search.text !== "")
            Qt.callLater(() => search.focusAtEnd())
    }
    function focusSearch() { search.focusField() }

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
        onTriggered: allGames._query = search.text.trim()
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
            "external": proxy.countWithStatus("external")
        }
    }
    readonly property bool _narrow: allGames.width < 1000
    readonly property var _sorts: [ { value: "", text: qsTr("A–Z") }, { value: "newest", text: qsTr("En yeni") } ]
    readonly property var _filters: {
        const all = [
            { value: "", text: qsTr("Tümü") },
            { value: "mine", text: qsTr("Bilgisayarımda") },
            { value: "installed", text: qsTr("Yaması kurulu") },
            { value: "local", text: qsTr("Launcher'dan kurulur") },
            { value: "external", text: qsTr("Dış sitede") }
        ]
        const out = []
        for (const f of all) {
            const n = allGames._counts[f.value] ?? 0
            if (f.value === "" || n > 0 || allGames.filter === f.value)
                out.push({ value: f.value, text: f.text, count: n })
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

            // Narrow window: the order moves up beside the search
            SegmentedControl {
                visible: allGames._narrow
                small: true
                model: allGames._sorts
                currentValue: allGames.sort
                onActivated: (value) => allGames.sort = value
            }
            SearchField {
                id: search
                Layout.preferredWidth: Math.min(320, allGames.width * 0.32)
                placeholder: qsTr("Ara — ör. rdr, elden, türkçe")
                onTextChanged: searchDebounce.restart()
                Keys.onDownPressed: grid.forceActiveFocus()
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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            spacing: 12

            SegmentedControl {
                model: allGames._filters
                currentValue: allGames.filter
                // A narrow window drops the counts and the larger type first
                small: allGames.width < 1180
                compact: allGames.width < 980
                onActivated: (value) => allGames.filter = value
            }
            Item { Layout.fillWidth: true }
            SegmentedControl {
                visible: !allGames._narrow
                small: true
                model: allGames._sorts
                currentValue: allGames.sort
                onActivated: (value) => allGames.sort = value
            }
        }

        // What the filter on show means
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 22
            Layout.rightMargin: 18
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            font.pixelSize: Dimensions.footnote
            color: Theme.textMuted
            text: allGames.filter === "mine"
                  ? qsTr("Bilgisayarında bulduğumuz oyunlar. Kapağında \"Kurulu\" yazanlarda Türkçe yama zaten kurulu.")
                : allGames.filter === "installed"
                  ? qsTr("Türkçe yaması kurulu oyunların. Güncellemek ya da kaldırmak için oyunun sayfasını aç.")
                : allGames.filter === "local"
                  ? qsTr("Bu yamaları launcher kendisi indirip kurar: oyunun sayfasındaki tek düğme yeter, orijinal dosyalar yedeklenir.")
                : allGames.filter === "external"
                  ? qsTr("Bu yamalar başka çeviri ekiplerinin sitelerinde yayımlanıyor. Oyunun sayfasındaki düğme seni o sayfaya götürür; kurulumu o site anlatır.")
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
            cellHeight: Math.round(_cardW * 185 / 130) + 78

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

                function open() {
                    allGames.gameSelected(cell.model.gameId || cell.model.steamAppId || "",
                                          cell.model.name || "", cell.model.installPath || "",
                                          cell.model.engine || "")
                }

                HomeCover {
                    x: Math.round((cell.width - width) / 2)
                    y: 8
                    width: grid._cardW
                    gameId: cell.model.gameId || ""
                    name: cell.model.name || ""
                    steamAppId: cell.model.steamAppId || ""
                    status: cell.model.packageInstalled ? "installed"
                          : cell.model.isInstalled ? "installable"
                          : (cell.model.externalUrl || "") !== "" ? "external" : ""
                    activeFocusOnTab: false
                    highlighted: grid.activeFocus && cell.index === grid.currentIndex
                    onClicked: {
                        grid.currentIndex = cell.index
                        cell.open()
                    }
                }
            }

            // Nothing under this search or filter
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 420)
                visible: grid.count === 0 && GameService.supportedGamesModel.count > 0
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
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    style: "tinted"
                    text: qsTr("Tüm oyunları göster")
                    onClicked: {
                        search.clear()
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
        }

        // Rows fade out under the bar and at the container's foot
        EdgeFade {
            edge: "top"; size: 26
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            active: !grid.atYBeginning
        }
        EdgeFade {
            edge: "bottom"; size: 34
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            active: !grid.atYEnd
        }
        }
    }
    }
    }
}
