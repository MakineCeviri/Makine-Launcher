// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * AllGames.qml - "Tüm Oyunlar": every Turkish patch in the catalogue at once
 *
 * The home page shows the catalogue as two rows that scroll sideways — fine
 * for a look around, no way to see what there is. This is the whole of it as
 * a grid, the name under each cover, with the search of the home page
 * (CatalogProxyModel: Turkish folding, "rdr" for Red Dead Redemption) and
 * filters on top: on this PC, patch installed, installed by the launcher or
 * from another site; A–Z or newest first.
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

    // Opened from the home page's "Tümünü gör": carry over what was on show.
    function open(filter, sort, query) {
        allGames.filter = filter || ""
        allGames.sort = sort || ""
        searchInput.text = query || ""
        allGames._query = searchInput.text.trim()
        grid.positionViewAtBeginning()
    }
    function focusSearch() {
        searchInput.forceActiveFocus()
        searchInput.selectAll()
    }

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
        onTriggered: allGames._query = searchInput.text.trim()
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

    // ── Small parts ──────────────────────────────────────────────────────────
    component Chip: Rectangle {
        id: chip
        property string text: ""
        property int count: -1
        property bool selected: false
        signal clicked()

        implicitWidth: chipRow.implicitWidth + 24
        implicitHeight: 34
        radius: 17
        color: chip.selected ? Theme.primary15
             : chipHover.hovered ? Theme.textPrimary06 : "transparent"
        border.width: chip.activeFocus ? 2 : 1
        border.color: chip.activeFocus ? Theme.accentLight
                    : chip.selected ? Theme.primary30 : Theme.textPrimary10
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        activeFocusOnTab: true
        Accessible.role: Accessible.RadioButton
        Accessible.name: chip.text
        Accessible.checked: chip.selected
        Keys.onReturnPressed: chip.clicked()
        Keys.onSpacePressed: chip.clicked()

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingSM
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: chip.text
                font.pixelSize: Dimensions.fontBody
                font.weight: chip.selected ? Font.DemiBold : Font.Medium
                color: chip.selected ? Theme.textPrimary : Theme.textSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.count >= 0
                textFormat: Text.PlainText
                text: String(chip.count)
                font.pixelSize: Dimensions.fontBody
                color: chip.selected ? Theme.accentLight : Theme.textMuted
            }
        }
        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: chip.clicked() }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Rectangle {
        id: panel
        anchors.fill: parent
        anchors.margins: allGames.contentMargin
        radius: Dimensions.radiusSection
        color: Qt.rgba(0.027, 0.027, 0.04, 0.85)

        GradientBorder { cornerRadius: Dimensions.radiusSection }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Dimensions.paddingXL
            anchors.bottomMargin: 0
            spacing: Dimensions.spacingXL

            // Title, how many, search
            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingXL

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingXS

                    Text {
                        textFormat: Text.PlainText
                        text: qsTr("Tüm Oyunlar")
                        font.pixelSize: Dimensions.fontHero
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                        text: allGames._query !== "" || allGames.filter !== ""
                              ? qsTr("%1 oyun gösteriliyor · toplam %2 Türkçe yama")
                                    .arg(proxy.sourceCount).arg(allGames._counts[""])
                              : qsTr("%1 oyun için Türkçe yama · %2 tanesi bilgisayarında")
                                    .arg(allGames._counts[""]).arg(allGames._counts["mine"])
                    }
                }

                // Search
                Rectangle {
                    Layout.preferredWidth: 340
                    Layout.preferredHeight: 42
                    radius: Dimensions.radiusLG
                    color: searchInput.activeFocus ? Theme.primary05 : Theme.textPrimary06
                    border.width: 1
                    border.color: searchInput.activeFocus ? Theme.accentBase40 : Theme.textPrimary12
                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Dimensions.paddingLG
                        anchors.rightMargin: Dimensions.paddingSM
                        spacing: Dimensions.spacingMD

                        Text {
                            textFormat: Text.PlainText
                            text: "\uE721"
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 14
                            color: searchInput.activeFocus ? Theme.accentLight : Theme.textMuted
                        }
                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            font.pixelSize: Dimensions.fontMD
                            color: Theme.textPrimary
                            clip: true
                            selectByMouse: true
                            activeFocusOnTab: true
                            onTextChanged: searchDebounce.restart()
                            Keys.onEscapePressed: { text = ""; focus = false }
                            Keys.onDownPressed: grid.forceActiveFocus()
                            Accessible.role: Accessible.EditableText
                            Accessible.name: qsTr("Oyun ara")

                            Text {
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                textFormat: Text.PlainText
                                visible: searchInput.text === ""
                                text: qsTr("Oyun ara — ör. rdr, elden, türkçe")
                                font.pixelSize: Dimensions.fontMD
                                color: Theme.textMuted
                            }
                        }
                        Rectangle {
                            visible: searchInput.text !== ""
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: Dimensions.radiusMD
                            color: clearHover.hovered ? Theme.textPrimary12 : "transparent"
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Aramayı temizle")
                            Text {
                                anchors.centerIn: parent
                                textFormat: Text.PlainText
                                text: "\uE711"
                                font.family: "Segoe MDL2 Assets"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                            }
                            HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: { searchInput.text = ""; allGames._query = "" } }
                        }
                    }
                }
            }

            // Filters and order
            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingMD

                Repeater {
                    model: [
                        { key: "", text: qsTr("Tümü") },
                        { key: "mine", text: qsTr("Bilgisayarımda") },
                        { key: "installed", text: qsTr("Yaması kurulu") },
                        { key: "local", text: qsTr("Launcher'dan kurulur") },
                        { key: "external", text: qsTr("Dış sitede") }
                    ]
                    delegate: Chip {
                        required property var modelData
                        text: modelData.text
                        count: allGames._counts[modelData.key] ?? -1
                        selected: allGames.filter === modelData.key
                        visible: modelData.key === "" || count > 0
                        onClicked: allGames.filter = modelData.key
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    textFormat: Text.PlainText
                    text: qsTr("Sırala")
                    font.pixelSize: Dimensions.fontBody
                    color: Theme.textMuted
                }
                Chip {
                    text: qsTr("A–Z")
                    selected: allGames.sort === ""
                    onClicked: allGames.sort = ""
                }
                Chip {
                    text: qsTr("Yeni güncellenen")
                    selected: allGames.sort === "newest"
                    onClicked: allGames.sort = "newest"
                }
            }

            // What the filter on show means
            Text {
                Layout.fillWidth: true
                Layout.topMargin: -Dimensions.spacingSM
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.fontSM
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

            // The games
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: proxy
                activeFocusOnTab: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: StyledScrollBar {}

                readonly property int _gap: Dimensions.spacingXL
                readonly property int _columns: Math.max(2, Math.floor((width + _gap) / (156 + _gap)))
                readonly property real _cardW: cellWidth - _gap
                readonly property real _cardH: Math.round(_cardW * 185 / 130)
                cellWidth: Math.floor(width / _columns)
                cellHeight: _cardH + 64

                function openAt(i) {
                    if (i < 0 || i >= count) return
                    const it = itemAtIndex(i)
                    if (it) it.open()
                }
                Keys.onReturnPressed: grid.openAt(grid.currentIndex)
                Keys.onSpacePressed: grid.openAt(grid.currentIndex)

                // Room at the end so the last row clears the panel's edge
                footer: Item { width: grid.width; height: Dimensions.spacingXL }

                delegate: Item {
                    id: cell
                    required property var model
                    required property int index

                    width: grid.cellWidth
                    height: grid.cellHeight

                    readonly property bool _external: (cell.model.externalUrl || "") !== ""
                    function open() {
                        allGames.gameSelected(cell.model.gameId || cell.model.steamAppId || "",
                                              cell.model.name || "", cell.model.installPath || "",
                                              cell.model.engine || "")
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: cell.model.name || ""

                    GameCard {
                        id: cover
                        x: Math.round((cell.width - width) / 2)
                        y: 6
                        height: grid._cardH
                        showName: false
                        gameId: cell.model.gameId || ""
                        gameName: cell.model.name || ""
                        steamAppId: cell.model.steamAppId || ""
                        installPath: cell.model.installPath || ""
                        badge: cell.model.packageInstalled ? qsTr("Kurulu")
                             : cell.model.isInstalled ? qsTr("Bilgisayarında")
                             : cell._external ? qsTr("Dış sitede") : ""
                        badgeColor: cell.model.packageInstalled ? Theme.success
                                  : cell.model.isInstalled ? Theme.primary : Theme.textSecondary
                    }

                    // Keyboard position
                    Rectangle {
                        visible: grid.activeFocus && cell.index === grid.currentIndex
                        x: cover.x - 4
                        y: cover.y - 4
                        width: cover.width + 8
                        height: cover.height + 8
                        radius: cover.width * 0.07 + 4
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.accentLight
                    }

                    Text {
                        anchors.top: cover.bottom
                        anchors.topMargin: Dimensions.spacingMD
                        x: cover.x
                        width: cover.width
                        textFormat: Text.PlainText
                        text: cell.model.name || ""
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        lineHeight: 1.1
                        font.pixelSize: Dimensions.fontBody
                        font.weight: Font.DemiBold
                        color: cellMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    }

                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            grid.currentIndex = cell.index
                            cell.open()
                        }
                    }
                }

                // Nothing under this search or filter
                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 460)
                    visible: grid.count === 0 && GameService.supportedGamesModel.count > 0
                    spacing: Dimensions.spacingLG

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        textFormat: Text.PlainText
                        text: "\uE721"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 28
                        color: Theme.textMuted
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                        text: allGames._query !== ""
                              ? qsTr("\"%1\" için oyun bulamadık. Yazımı değiştir ya da filtreyi kaldır.").arg(allGames._query)
                              : qsTr("Bu filtrede oyun yok.")
                    }
                    Chip {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Tüm oyunları göster")
                        onClicked: {
                            searchInput.text = ""
                            allGames._query = ""
                            allGames.filter = ""
                        }
                    }
                }
            }
        }
    }
}
