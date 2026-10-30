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
 * HomePage.qml — "Ana Sayfa": what there is to do and what is new.
 *
 * Top to bottom: the page's title with a sentence on this PC's games; a
 * running game (when the watcher sees one); the featured art; shelves of
 * this PC's games, the newest patches and the one-click patches; the switch
 * for recognising a started game; where patches may safely come from.
 */
Item {
    id: homePage

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    signal settingsRequested()
    signal libraryRequested()
    signal allGamesRequested(string filter, string sort)

    onVisibleChanged: if (visible) page.contentY = 0

    // ── Data ─────────────────────────────────────────────────────────────
    readonly property var _lib: GameService.library
    readonly property int _patched: _lib.filter(g => g.status === "installed" || g.status === "update").length
    readonly property int _ready: _lib.filter(g => g.status === "ready").length
    readonly property int _updates: _lib.filter(g => g.status === "update").length
    // "Yakında" games are in the library but have no patch yet: not counted as patched games.
    readonly property int _soon: _lib.filter(g => g.status === "soon").length

    readonly property string _summary: {
        if (GameService.isScanning && _lib.length === 0)
            return qsTr("Bilgisayarındaki oyunlara bakıyoruz…")
        if (_lib.length === 0)
            return qsTr("Kütüphanelerinde Türkçe yaması olan bir oyun bulamadık. Oyunun klasörünü ekle ya da pencereye sürükleyip bırak.")
        let s = _lib.length > _soon
            ? qsTr("Bilgisayarında Türkçe yaması olan %1 oyun var; %2 tanesinde yama kurulu.").arg(_lib.length - _soon).arg(_patched)
            : qsTr("Bilgisayarındaki oyunların Türkçe yaması henüz hazır değil.")
        if (_ready > 0) s += " " + qsTr("%1 oyunun yaması kurulmayı bekliyor.").arg(_ready)
        else if (_updates > 0) s += " " + qsTr("%1 yamanın güncellemesi var.").arg(_updates)
        if (_soon > 0) s += " " + qsTr("%1 oyunun yaması yakında.").arg(_soon)
        return s
    }

    readonly property var _mine: _lib.map(g => ({
        id: g.id, name: g.name || "", steamAppId: g.steamAppId || "",
        installPath: g.installPath || "", engine: g.engine || "", status: g.status }))

    property var _recent: []
    function _refreshRecent() {
        const out = []
        for (const e of GameService.recentlyUpdatedGames(24)) {
            out.push({ id: e.id || e.steamAppId, name: e.name || "", steamAppId: e.steamAppId || "",
                       installPath: e.installPath || "", engine: e.engine || "",
                       // When, under the name; the state, as the cover's mark
                       subtitle: _dateText(e.version),
                       status: e.packageInstalled ? "installed" : e.isInstalled ? "ready" : "" })
        }
        _recent = out
    }
    function _dateText(v) {
        const m = /^(\d{4})-(\d{2})-(\d{2})(\.\d+)?$/.exec(v || "")  // same-day republish: "2026-10-23.2"
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
        function onModelReset() { homePage._refreshRecent() }
        function onDataChanged() { homePage._refreshRecent() }
    }
    Component.onCompleted: _refreshRecent()

    // The patches the launcher installs itself, A–Z
    CatalogProxyModel {
        id: localProxy
        sourceModel: GameService.supportedGamesModel
        statusFilter: "local"
        rowLimit: 40
    }

    // ── Page ─────────────────────────────────────────────────────────────
    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: col.implicitHeight + 48
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: page.parent
            anchors.top: page.top; anchors.right: page.right; anchors.bottom: page.bottom
        }

        ColumnLayout {
            id: col
            width: page.width
            spacing: 34

            PageHeader {
                Layout.topMargin: 52
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                title: qsTr("Ana Sayfa")
                subtitle: homePage._summary

                AppButton {
                    style: "gray"
                    icon: Icons.folderSearch
                    text: qsTr("Oyun ekle")
                    toolTip: qsTr("Listede olmayan bir oyunun klasörünü seç")
                    onClicked: homePage.manualFolderRequested()
                }
            }

            RunningGameCard {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                Layout.topMargin: -12
                onLibraryRequested: homePage.libraryRequested()
            }

            BatchOperationsPanel {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                animationsEnabled: homePage.animationsEnabled
            }

            // The featured art, and beside it Makine's own corner (while the page is wide enough)
            RowLayout {
                id: heroRow
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                Layout.topMargin: -10
                spacing: 16
                // (from the settings and the page's width, not from the children's visible: a hidden row hides them)
                readonly property real _width: page.width - Dimensions.pageMargin * 2
                readonly property bool _corner: Companion.available && SettingsManager.companionEnabled && SettingsManager.companionCorner
                                                && SettingsManager.companionPlace === "launcher" && heroRow._width >= 760
                visible: hero._count > 0 || heroRow._corner

                FeaturedHero {
                    id: hero
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(Math.max(250, Math.min(400, (page.width - Dimensions.pageMargin * 2) * 0.36)))
                    visible: hero._count > 0
                    inView: page.contentY < heroRow.y + hero.height
                    onGameSelected: (gameId, gameName, installPath, engine) =>
                        homePage.gameSelected(gameId, gameName, installPath, engine)
                    onInstallRequested: (gameId, gameName, installPath, engine) =>
                        homePage.installAndShowDetail(gameId, gameName, installPath, engine)
                }

                MakineCorner {
                    id: corner
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Math.round(Math.max(250, Math.min(330, heroRow._width * 0.26)))
                    Layout.preferredHeight: hero._count > 0 ? hero.Layout.preferredHeight : 270
                    visible: heroRow._corner
                    view: page
                    clearTop: 56
                }
            }

            // This PC's games — or how to bring them in
            Shelf {
                Layout.fillWidth: true
                visible: homePage._mine.length > 0
                title: qsTr("Bilgisayarındaki oyunlar")
                subtitle: qsTr("Katalogdaki oyunların; adın önündeki işaret yamanın durumunu söyler (üstüne gelince yazar).")
                seeAllText: qsTr("Kütüphanem")
                model: homePage._mine
                onGameClicked: (gameId, gameName, installPath, engine) =>
                    homePage.gameSelected(gameId, gameName, installPath, engine)
                onSeeAllClicked: homePage.libraryRequested()
            }

            Card {
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                visible: homePage._mine.length === 0 && !GameService.isScanning
                implicitHeight: emptyRow.implicitHeight + 40

                RowLayout {
                    id: emptyRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 16

                    IconTile { size: 44; glyph: Icons.folderSearch; tint: Theme.primary }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Oyunlarını bulalım")
                            font.pixelSize: Dimensions.headline
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            text: qsTr("Steam, Epic ve GOG kütüphanelerini taradık; Türkçe yaması olan bir oyun çıkmadı. Oyun başka bir klasördeyse onu seç ya da klasörü bu pencereye sürükleyip bırak — hangi oyun olduğunu biz buluruz.")
                            wrapMode: Text.WordWrap
                            font.pixelSize: Dimensions.callout
                            color: Theme.textMuted
                        }
                    }
                    AppButton {
                        style: "filled"
                        icon: Icons.folderOpen
                        text: qsTr("Oyun klasörünü seç")
                        onClicked: homePage.manualFolderRequested()
                    }
                }
            }

            Shelf {
                Layout.fillWidth: true
                visible: homePage._recent.length > 0
                title: qsTr("Yeni güncellenen yamalar")
                subtitle: qsTr("Son yayımlanan ya da güncellenen Türkçe yamalar, en yenisi başta.")
                model: homePage._recent
                onGameClicked: (gameId, gameName, installPath, engine) =>
                    homePage.gameSelected(gameId, gameName, installPath, engine)
                onSeeAllClicked: homePage.allGamesRequested("", "newest")
            }

            Shelf {
                Layout.fillWidth: true
                visible: localProxy.count > 0
                title: qsTr("Tek tıkla kurulan yamalar")
                subtitle: qsTr("Bunları launcher kendisi indirip kurar; orijinal dosyaların yedeklenir.")
                model: localProxy
                onGameClicked: (gameId, gameName, installPath, engine) =>
                    homePage.gameSelected(gameId, gameName, installPath, engine)
                onSeeAllClicked: homePage.allGamesRequested("local", "")
            }

            // Recognising a started game: what it does, switched right here
            ListSection {
                Layout.leftMargin: Dimensions.pageMargin
                Layout.rightMargin: Dimensions.pageMargin
                footer: qsTr("Yamalarını yalnızca makineceviri.org'dan ve bu launcher'dan indir; başka sitelerdeki kopyalar değiştirilmiş olabilir.")

                ListRow {
                    icon: Icons.scanSearch
                    iconTint: Theme.success
                    title: qsTr("Çalıştırdığın oyunu tanı")
                    subtitle: qsTr("Açıkken bir oyun başlattığında launcher onu fark eder; Türkçe yaması varsa kütüphanene ekler ve haber verir.")
                    AppSwitch {
                        checked: SettingsManager.autoDetectGames
                        accessibleName: qsTr("Çalıştırdığın oyunu tanı")
                        onToggled: SettingsManager.autoDetectGames = !SettingsManager.autoDetectGames
                    }
                }
            }
        }
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: page
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
