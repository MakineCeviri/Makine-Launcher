// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * CatalogSection.qml - Game catalog with search and two scrollable rows.
 * Uses CatalogProxyModel (C++) instead of JS filter/slice/concat.
 */
Rectangle {
    id: catalog

    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    function replayContentFade() {
        catalogContent.opacity = 0
        catalogContentFade.start()
    }

    // Ctrl+F (from Main.qml) → focus the search box
    function focusSearch() {
        searchInput.forceActiveFocus()
        searchInput.selectAll()
    }

    Layout.fillWidth: true
    Layout.fillHeight: true

    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    radius: Dimensions.radiusSection

    GradientBorder { cornerRadius: parent.radius }

    // Click anywhere outside the search box (header, dividers, empty
    // space, or game strips) blurs it so it collapses. TapHandler is
    // preferred over MouseArea here because it is a passive pointer
    // handler — it does NOT consume the press. The HorizontalGameStrip's
    // own drag MouseArea below still receives every event, and clicks on
    // game cards still fire. A QML TextInput keeps active focus until
    // something else takes it — without this the box never closed (user
    // had to restart the app, since the prior MouseArea was being
    // shadowed by the strip's anchors.fill MouseArea).
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onTapped: if (searchInput.activeFocus) searchInput.focus = false
    }

    // Internal state
    readonly property real contentPadding: 16
    property string searchQuery: ""
    readonly property bool _isSearching: searchQuery.length > 0
    property bool _row2Ready: false

    // What the strips show: every patch (two rows), this PC's games, or the
    // newest patches (one row of large covers, like search results).
    property string mode: "all"   // all | mine | new
    readonly property bool _single: _isSearching || mode !== "all"

    // This PC's games that have a patch, the ones needing a step first.
    readonly property var _mine: GameService.library.map(g => ({
        id: g.id, name: g.name || "", steamAppId: g.steamAppId || "",
        installPath: g.installPath || "", engine: g.engine || "", status: g.status,
        badge: g.status === "update" ? qsTr("Güncelleme var")
             : g.status === "installed" ? qsTr("Kurulu")
             : g.status === "external" ? qsTr("Dış sitede") : qsTr("Yama hazır"),
        badgeColor: g.status === "update" ? Theme.warning
                  : g.status === "installed" ? Theme.success
                  : g.status === "external" ? Theme.textSecondary : Theme.primary }))

    // Newest packages, dated on the cover.
    property var _recent: []
    function _refreshRecent() {
        const out = []
        for (const e of GameService.recentlyUpdatedGames(24)) {
            out.push({ id: e.id || e.steamAppId, name: e.name || "", steamAppId: e.steamAppId || "",
                       installPath: e.installPath || "", engine: e.engine || "",
                       badge: _dateText(e.version),
                       badgeColor: e.packageInstalled ? Theme.success
                                 : e.isInstalled ? Theme.primary : Theme.accentLight })
        }
        _recent = out
    }
    function _dateText(v) {
        const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(v || "")
        if (!m) return ""
        const d = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]))
        const today = new Date()
        today.setHours(0, 0, 0, 0)
        const days = Math.round((today - d) / 86400000)
        if (days <= 0) return qsTr("Bugün")
        if (days === 1) return qsTr("Dün")
        if (days < 7) return qsTr("%1 gün önce").arg(days)
        return d.toLocaleDateString(Qt.locale("tr_TR"), "d MMMM")
    }
    Connections {
        target: GameService.supportedGamesModel
        function onModelReset() { catalog._refreshRecent() }
        function onDataChanged() { catalog._refreshRecent() }
    }

    readonly property int _totalCount: GameService.supportedGamesModel
                                        ? GameService.supportedGamesModel.count : 0
    readonly property int _half: Math.ceil(_totalCount / 2)

    // Row 1 proxy: first half (or all during search)
    CatalogProxyModel {
        id: row1Proxy
        sourceModel: GameService.supportedGamesModel
        searchFilter: catalog.searchQuery
        rowOffset: 0
        rowLimit: catalog._isSearching ? -1 : catalog._half
        wrapAround: !catalog._isSearching
    }

    // Row 2 proxy: second half (hidden during search)
    CatalogProxyModel {
        id: row2Proxy
        sourceModel: catalog._row2Ready ? GameService.supportedGamesModel : null
        // The strip below is hidden while searching (visible: !_isSearching), yet
        // this binding still rebuilt it on every keystroke. That second, invisible
        // beginResetModel/endResetModel raced with row1's reuseItems ListView
        // relayout and corrupted the heap (surfaced as WideCharToMultiByte /
        // RtlpHpSegReAlloc crashes when typing). Freeze the filter to "" while
        // searching so the hidden strip never rebuilds — one reset per keystroke.
        searchFilter: catalog._isSearching ? "" : catalog.searchQuery
        rowOffset: catalog._half
        rowLimit: -1
        wrapAround: true
    }

    Timer {
        id: _row2Defer
        interval: 800
        onTriggered: catalog._row2Ready = true
    }

    Component.onCompleted: {
        _row2Defer.start()
        _refreshRecent()
    }

    Timer {
        id: searchDebounce
        interval: 100
        onTriggered: catalog.searchQuery = searchInput.text.trim()
    }

    // Square off bottom corners
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Dimensions.radiusSection
        color: parent.color
    }

    ColumnLayout {
        id: catalogContent
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: catalog.contentPadding
        anchors.rightMargin: catalog.contentPadding
        anchors.bottomMargin: catalog.contentPadding
        spacing: 0
        opacity: 0

        Component.onCompleted: catalogContentFade.start()
        NumberAnimation {
            id: catalogContentFade
            target: catalogContent; property: "opacity"
            from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic
        }

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            spacing: Dimensions.spacingSM

            Rectangle {
                Layout.preferredWidth: 6; Layout.preferredHeight: 6
                radius: 3; color: Theme.accentBase
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                textFormat: Text.PlainText
                text: qsTr("Türkçe Yamalar")
                font.pixelSize: Dimensions.fontLG
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            // Which games the strips show
            Row {
                Layout.leftMargin: Dimensions.spacingLG
                Layout.alignment: Qt.AlignVCenter
                spacing: Dimensions.spacingSM
                visible: !catalog._isSearching

                Repeater {
                    model: [
                        { mode: "all",  text: qsTr("Tümü"), count: catalog._totalCount },
                        { mode: "mine", text: qsTr("Bilgisayarımda"), count: catalog._mine.length },
                        { mode: "new",  text: qsTr("Yeni güncellenen"), count: -1 }
                    ]

                    Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool selected: catalog.mode === chip.modelData.mode
                        height: 26
                        width: chipRow.implicitWidth + 20
                        radius: 13
                        color: chip.selected ? Theme.primary15
                             : chipHover.hovered ? Theme.textPrimary06 : "transparent"
                        border.width: 1
                        border.color: chip.selected ? Theme.primary30 : Theme.textPrimary10
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: chip.modelData.text
                        Accessible.checked: chip.selected

                        Row {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                textFormat: Text.PlainText
                                text: chip.modelData.text
                                font.pixelSize: Dimensions.fontXS
                                font.weight: chip.selected ? Font.DemiBold : Font.Medium
                                color: chip.selected ? Theme.textPrimary : Theme.textSecondary
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: chip.modelData.count > 0
                                textFormat: Text.PlainText
                                text: chip.modelData.count
                                font.pixelSize: Dimensions.fontXS
                                color: chip.selected ? Theme.primary : Theme.textMuted
                            }
                        }
                        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: catalog.mode = chip.modelData.mode }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Search box
            Rectangle {
                Layout.preferredHeight: 30
                Layout.preferredWidth: searchInput.activeFocus || searchInput.text ? 240 : 200
                Behavior on Layout.preferredWidth { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                radius: Dimensions.radiusMD
                color: searchInput.activeFocus ? Theme.primary05 : Theme.textPrimary10
                border.color: searchInput.activeFocus
                    ? Theme.accentBase40
                    : Theme.textPrimary20
                border.width: 1
                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                Row {
                    anchors.left: parent.left; anchors.right: clearBtn.left
                    anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.leftMargin: 10; anchors.rightMargin: 4
                    spacing: 8

                    Text {
                        textFormat: Text.PlainText
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\uE721"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 13
                        color: searchInput.activeFocus ? Theme.accentBase : Theme.textMuted
                    }

                    TextInput {
                        id: searchInput
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 28
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textPrimary
                        clip: true; selectByMouse: true
                        onTextChanged: {
                            searchDebounce.restart()
                            if (text !== "") catalog.mode = "all"
                        }
                        Keys.onEscapePressed: { text = ""; focus = false }

                        Text {
                            textFormat: Text.PlainText
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Oyun ara... (%1)").arg(catalog._totalCount)
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            visible: !searchInput.text && !searchInput.activeFocus
                        }
                    }
                }

                // Clear button — slides in from right
                Rectangle {
                    id: clearBtn
                    anchors.right: parent.right; anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: searchInput.text.length > 0 ? 22 : 0
                    height: 22; radius: 4; clip: true
                    color: clearMa.containsMouse ? Theme.textPrimary15 : "transparent"
                    opacity: searchInput.text.length > 0 ? 1 : 0

                    Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: "\uE711"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 10
                        color: clearMa.containsMouse ? Theme.textPrimary : Theme.textMuted
                    }

                    MouseArea {
                        id: clearMa; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { searchInput.text = ""; searchInput.focus = true }
                    }
                }
            }
        }

        SettingsDivider { variant: "section" }

        // Loading state
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog._totalCount === 0 && !catalog.searchQuery

            Column {
                anchors.centerIn: parent; spacing: 12
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: visible; width: 32; height: 32
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Katalog y\u00FCkleniyor\u2026")
                    font.pixelSize: Dimensions.fontSM; color: Theme.textMuted
                }
            }
        }

        // Empty search state
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: row1Proxy.sourceCount === 0 && catalog.searchQuery.length > 0

            Column {
                anchors.centerIn: parent; spacing: 8
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\uE773"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 28; color: Theme.textMuted; opacity: 0.5
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("\"%1\" ile e\u015Fle\u015Fen oyun bulunamad\u0131").arg(catalog.searchQuery)
                    font.pixelSize: Dimensions.fontSM; color: Theme.textMuted
                }
            }
        }

        // No game of theirs has a patch (yet)
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog.mode === "mine" && !catalog._isSearching && catalog._mine.length === 0

            Column {
                anchors.centerIn: parent; spacing: 8
                width: Math.min(parent.width, 420)
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\uE7FC"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 28; color: Theme.textMuted; opacity: 0.5
                }
                Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: GameService.isScanning
                          ? qsTr("Oyunların taranıyor…")
                          : qsTr("Bilgisayarında Türkçe yaması olan bir oyun bulamadık. Oyunun başka bir klasördeyse “Oyun Ekle” ile ekleyebilirsin.")
                    font.pixelSize: Dimensions.fontSM; color: Theme.textMuted
                }
            }
        }

        Item { Layout.preferredHeight: Dimensions.cardGap }

        // Row 1 (expands to full height with large cards during search). One
        // row of large covers keeps its own height and hangs under the chips;
        // stretched, it floated in the middle of the section.
        HorizontalGameStrip {
            Layout.fillWidth: true
            Layout.fillHeight: !catalog._single
            Layout.preferredHeight: catalog._single ? Math.ceil(Dimensions.cardHeight * 1.3 * 1.03) + Dimensions.cardGap : -1
            visible: catalog.mode === "mine" && !catalog._isSearching ? catalog._mine.length > 0
                   : (row1Proxy.count > 0 || catalog._totalCount > 0)
            model: catalog._isSearching || catalog.mode === "all" ? row1Proxy
                 : catalog.mode === "mine" ? catalog._mine : catalog._recent
            wrapAround: !catalog._single
            largeCards: catalog._single
            showStatus: true
            wheelEnabled: !catalog._isSearching || row1Proxy.sourceCount >= 4
            driftSpeed: 0
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        // Row separator with padding
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.cardGap * 2
            visible: !catalog._single && catalog._row2Ready && row2Proxy.count > 0

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.6
                height: 1
                color: Theme.textPrimary08
            }
        }

        // Below a single row: the room that row no longer takes
        Item { Layout.fillHeight: true; visible: catalog._single }

        // Row 2
        HorizontalGameStrip {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: !catalog._single && catalog._row2Ready && row2Proxy.count > 0
            model: row2Proxy
            wrapAround: true
            showStatus: true
            driftSpeed: 0
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        Item { Layout.preferredHeight: Dimensions.cardGap }
    }

    // Edge fades — inline component named CatalogEdgeFade to avoid shadowing SectionContainer's EdgeFade
    component CatalogEdgeFade: Rectangle {
        property bool mirror: false
        anchors { top: parent.top; bottom: parent.bottom; topMargin: 40 }
        width: 28; z: 10; rotation: mirror ? 180 : 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.90) }
            GradientStop { position: 0.4; color: Qt.rgba(0.055, 0.055, 0.055, 0.25) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    CatalogEdgeFade { anchors.left: parent.left }
    CatalogEdgeFade { anchors.right: parent.right; mirror: true }
}
