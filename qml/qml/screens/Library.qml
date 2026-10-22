// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * Library.qml - This PC's games with a Turkish patch, and what each needs next
 *
 * One grouped list, the way the App Store lists updates: each row carries
 * the game's state and its one action — install, update, the patch's own
 * site — and opens the game's page. The segmented control on top filters;
 * GameService.library orders the rows: what needs a step first.
 */
Item {
    id: libraryRoot

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    // Install (or update) right away: the game's page opens and starts it.
    signal installRequested(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    // "Nasıl çalışır?": what the states and buttons here mean
    signal helpRequested()

    property string filter: "all"   // all | ready | update | installed

    readonly property var _lib: GameService.library
    readonly property int _ready: _lib.filter(g => g.status === "ready").length
    readonly property int _updates: _lib.filter(g => g.status === "update").length
    readonly property int _patched: _lib.filter(g => g.status === "installed" || g.status === "update").length
    readonly property var _rows: libraryRoot.filter === "all" ? _lib
        : libraryRoot.filter === "installed"
          ? _lib.filter(g => g.status === "installed" || g.status === "update")
        : _lib.filter(g => g.status === libraryRoot.filter)

    // A filter whose rows are gone (the last update installed) falls back.
    on_RowsChanged: if (_rows.length === 0 && filter !== "all") filter = "all"

    readonly property var _filters: {
        const out = [ { value: "all", text: qsTr("Tümü"), count: _lib.length } ]
        if (_ready > 0 || filter === "ready") out.push({ value: "ready", text: qsTr("Kurulmayı bekliyor"), count: _ready })
        if (_updates > 0 || filter === "update") out.push({ value: "update", text: qsTr("Güncelleme var"), count: _updates })
        if (_patched > 0 || filter === "installed") out.push({ value: "installed", text: qsTr("Yama kurulu"), count: _patched })
        return out
    }

    function _shownPath(p) {
        return String(p || "").split("/").join(String.fromCharCode(92))
    }
    function _source(g) {
        switch (g.source) {
        case "steam": return "Steam"
        case "epic": return "Epic Games"
        case "gog": return "GOG"
        case "xbox": return "Xbox"
        case "ubisoft": return "Ubisoft Connect"
        case "manual": return qsTr("Elle eklendi")
        default: return qsTr("Otomatik bulundu")
        }
    }

    onVisibleChanged: if (visible) flickable.contentY = 0

    Flickable {
        id: flickable
        anchors.fill: parent
        contentHeight: contentColumn.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: flickable.parent
            anchors.top: flickable.top; anchors.right: flickable.right; anchors.bottom: flickable.bottom
        }

        ColumnLayout {
            id: contentColumn
            width: Math.min(flickable.width - Dimensions.pageMargin * 2, Dimensions.contentMaxWidth)
            x: (flickable.width - width) / 2
            spacing: 20

            PageHeader {
                Layout.topMargin: 52
                title: qsTr("Kütüphanem")
                subtitle: GameService.isScanning && libraryRoot._lib.length === 0
                          ? qsTr("Oyunların taranıyor…")
                          : libraryRoot._lib.length === 0
                            ? qsTr("Bilgisayarında Türkçe yaması olan bir oyun bulamadık.")
                            : qsTr("Bilgisayarında Türkçe yaması olan %1 oyun var; %2 tanesinde yama kurulu.")
                                  .arg(libraryRoot._lib.length).arg(libraryRoot._patched)

                AppButton {
                    style: "gray"
                    icon: Icons.circleHelp
                    toolTip: qsTr("Nasıl çalışır? Buradaki durumlar ve düğmeler ne demek")
                    onClicked: libraryRoot.helpRequested()
                }
                AppButton {
                    style: "gray"
                    icon: Icons.refreshCw
                    busy: GameService.isScanning
                    text: GameService.isScanning ? qsTr("Taranıyor…") : qsTr("Yeniden tara")
                    toolTip: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine yeniden bak")
                    onClicked: GameService.scanAllLibraries()
                }
                AppButton {
                    style: "gray"
                    icon: Icons.plus
                    text: qsTr("Oyun ekle")
                    toolTip: qsTr("Listede olmayan bir oyunun klasörünü seç")
                    onClicked: libraryRoot.manualFolderRequested()
                }
            }

            // The games, their filters on the container's top bar
            Card {
                Layout.fillWidth: true
                Layout.topMargin: 2
                visible: libraryRoot._rows.length > 0
                implicitHeight: libCol.implicitHeight
                radius: Dimensions.radiusSection
                glow: true

                ColumnLayout {
                    id: libCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                SegmentedControl {
                    visible: libraryRoot._lib.length > 0 && libraryRoot._filters.length > 1
                    Layout.topMargin: 14
                    Layout.leftMargin: 16
                    Layout.bottomMargin: 12
                    model: libraryRoot._filters
                    currentValue: libraryRoot.filter
                    onActivated: (value) => libraryRoot.filter = value
                }
                FadeLine {
                    visible: libraryRoot._lib.length > 0 && libraryRoot._filters.length > 1
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                }

                ColumnLayout {
                    id: rowsCol
                    Layout.fillWidth: true
                    spacing: 0

                    Repeater {
                        model: libraryRoot._rows

                        delegate: Item {
                            id: row
                            required property var modelData
                            required property int index

                            readonly property string status: row.modelData.status || "ready"
                            readonly property color tone: row.status === "update" ? Theme.warning
                                                        : row.status === "installed" ? Theme.success
                                                        : row.status === "external" ? Theme.textMuted
                                                        : Theme.primary
                            readonly property bool _first: row.index === 0
                            readonly property bool _last: row.index === libraryRoot._rows.length - 1
                            function open() {
                                libraryRoot.gameSelected(row.modelData.id, row.modelData.name || "",
                                                         row.modelData.installPath || "", row.modelData.engine || "")
                            }
                            function act() {
                                if (row.status === "external")
                                    Qt.openUrlExternally(row.modelData.externalUrl)
                                else if (row.status === "installed")
                                    row.open()
                                else
                                    libraryRoot.installRequested(row.modelData.id, row.modelData.name || "",
                                                                 row.modelData.installPath || "", row.modelData.engine || "")
                            }

                            Layout.fillWidth: true
                            implicitHeight: 92

                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: row.modelData.name || ""
                            Keys.onReturnPressed: row.open()
                            Keys.onSpacePressed: row.open()

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                visible: rowHover.hovered || row.activeFocus
                                color: rowTap.pressed ? Theme.fillTertiary : Theme.fillQuaternary
                                topLeftRadius: row._first ? Dimensions.radiusLG - 1 : 0
                                topRightRadius: row._first ? Dimensions.radiusLG - 1 : 0
                                bottomLeftRadius: row._last ? Dimensions.radiusLG - 1 : 0
                                bottomRightRadius: row._last ? Dimensions.radiusLG - 1 : 0
                            }
                            Rectangle {
                                visible: !row._first
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 16 + 48 + 16
                                height: 1
                                color: Theme.separator
                            }

                            HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { id: rowTap; onTapped: row.open() }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 16

                                Thumb {
                                    Layout.alignment: Qt.AlignVCenter
                                    gameId: row.modelData.id || ""
                                    steamAppId: row.modelData.steamAppId || ""
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: row.modelData.name || ""
                                        elide: Text.ElideRight
                                        font.pixelSize: Dimensions.headline
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.PlainText
                                        text: libraryRoot._source(row.modelData) + "  ·  "
                                              + libraryRoot._shownPath(row.modelData.installPath)
                                        elide: Text.ElideMiddle
                                        font.pixelSize: Dimensions.footnote
                                        color: Theme.textMuted
                                    }
                                    StatusBadge {
                                        Layout.topMargin: 3
                                        tint: row.tone
                                        text: row.status === "update" ? qsTr("Güncelleme var")
                                            : row.status === "installed" ? qsTr("Yama kurulu")
                                            : row.status === "external" ? qsTr("Yaması başka sitede")
                                            : qsTr("Yama hazır, kurulmadı")
                                    }
                                }

                                AppButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    style: row.status === "ready" ? "filled"
                                         : row.status === "update" ? "filled" : "gray"
                                    tint: row.status === "update" ? Theme.warning : Theme.primary
                                    icon: row.status === "ready" ? Icons.download
                                        : row.status === "update" ? Icons.refreshCw
                                        : row.status === "external" ? Icons.arrowUpRight : ""
                                    trailingIcon: row.status === "installed" ? Icons.chevronRight : ""
                                    text: row.status === "ready" ? qsTr("Türkçe Yamayı Kur")
                                        : row.status === "update" ? qsTr("Güncelle")
                                        : row.status === "external" ? qsTr("Sitesine git")
                                        : qsTr("Sayfasını aç")
                                    onClicked: row.act()
                                }
                            }
                        }
                    }
                }
                }
            }

            // Nothing yet: what to do about it
            Card {
                Layout.fillWidth: true
                visible: libraryRoot._lib.length === 0 && !GameService.isScanning
                implicitHeight: emptyCol.implicitHeight + 64

                ColumnLayout {
                    id: emptyCol
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 64, 520)
                    spacing: 12

                    IconTile {
                        Layout.alignment: Qt.AlignHCenter
                        size: 56
                        glyph: Icons.gamepad2
                        tint: Theme.primary
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        text: qsTr("Kütüphanende henüz oyun yok")
                        font.pixelSize: Dimensions.title3
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.2
                        font.pixelSize: Dimensions.callout
                        color: Theme.textMuted
                        text: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine baktık. Oyunun başka bir yerdeyse klasörünü ekle ya da pencereye sürükleyip bırak; hangi oyun olduğunu biz buluruz.")
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        style: "filled"
                        icon: Icons.folderOpen
                        text: qsTr("Oyun klasörünü seç")
                        onClicked: libraryRoot.manualFolderRequested()
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: libraryRoot._lib.length > 0
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
                text: qsTr("Bir oyunun listede yok mu? Klasörünü pencereye sürükleyip bırak ya da \"Oyun ekle\"yi kullan.")
            }
        }
    }

    // A small cover for a list row (pre-rounded art from the image cache)
    component Thumb: Item {
        id: thumb
        property string gameId: ""
        property string steamAppId: ""
        property string _src: ""
        function _resolve() {
            const id = thumb.steamAppId || thumb.gameId
            thumb._src = id ? ImageCache.resolve(id) : ""
        }
        onSteamAppIdChanged: _resolve()
        onGameIdChanged: _resolve()
        Component.onCompleted: _resolve()

        implicitWidth: 48
        implicitHeight: 68

        // Fixed target, gated by `enabled` (NATIVE-74).
        Connections {
            target: ImageCache
            enabled: thumb._src === ""
            function onImageReady(readyId) {
                if (readyId === (thumb.steamAppId || thumb.gameId)) thumb._resolve()
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: width * 0.118
            color: Theme.surfaceLight
            visible: img.status !== Image.Ready
        }
        Image {
            id: img
            anchors.fill: parent
            source: thumb._src
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(96, 136)
            asynchronous: true
            mipmap: true
        }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: flickable
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
