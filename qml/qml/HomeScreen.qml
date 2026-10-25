// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "components"
import "screens"
pragma ComponentBehavior: Bound

/**
 * HomeScreen.qml - Main container for Home and Library sub-pages
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal settingsRequested()
    signal manualFolderRequested()
    signal libraryRequested()
    // "Tümünü gör" on the home page, with what it was showing
    signal allGamesRequested(string filter, string sort, string query)
    signal helpRequested()

    Component.onCompleted: {
        if (typeof SceneProfiler !== "undefined")
            SceneProfiler.screenLoaded("Home")
        // No scan here: GameService::initialize() already schedules the
        // startup library scan (2 s with a games cache, 0.5 s without).
        // A second timer-driven scan re-read index.json on a worker thread
        // and re-published identical results a few seconds later.
    }

    // Update check handled by UpdateService in main.cpp startup.

    // Current sub-page index: 0=Home, 1=Library, 2=Tüm Oyunlar, 3=Nasıl çalışır?, 4=Yenilikler
    property int currentPage: 0

    function showHomePage() { currentPage = 0 }
    function showLibraryPage() { currentPage = 1 }
    // With arguments: open on that filter, order and search (see AllGames.open).
    property var _allGamesArgs: null
    function showAllGamesPage(filter, sort, query) {
        currentPage = 2
        if (filter === undefined) return
        if (allGamesLoader.item) allGamesLoader.item.open(filter, sort, query)
        else _allGamesArgs = [filter, sort, query]
    }
    function showHelpPage() {
        currentPage = 3
        if (helpLoader.item) helpLoader.item.scrollToTop()
    }
    function showWhatsNewPage() {
        currentPage = 4
        if (whatsNewLoader.item) whatsNewLoader.item.scrollToTop()
    }

    // Ctrl+F → the search of the page on show: Tüm Oyunlar, else the home page's
    function focusSearch() {
        if (currentPage === 2) {
            if (allGamesLoader.item) allGamesLoader.item.focusSearch()
            return
        }
        currentPage = 0
        if (homePageLoader.item)
            homePageLoader.item.focusSearch()
    }

    // Sub-pages share the same slot, switched by visibility
    // Async-loaded: defers the home page's heavier parts (featured art,
    // shelves of covers) until after the first frame.
    Loader {
        id: homePageLoader
        anchors.fill: parent
        active: true
        visible: root.currentPage === 0
        asynchronous: true
        sourceComponent: Component {
            HomePage {
                animationsEnabled: root.animationsEnabled
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    root.installAndShowDetail(gameId, gameName, installPath, engine)
                }
                onManualFolderRequested: root.manualFolderRequested()
                onSettingsRequested: root.settingsRequested()
                onLibraryRequested: root.libraryRequested()
                onAllGamesRequested: (filter, sort, query) => root.allGamesRequested(filter, sort, query)
            }
        }
    }

    Loader {
        id: libraryLoader
        anchors.fill: parent
        active: root.currentPage === 1 || libraryLoader.status === Loader.Ready
        visible: root.currentPage === 1
        asynchronous: true
        sourceComponent: Component {
            Library {
                animationsEnabled: root.animationsEnabled
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
                onInstallRequested: function(gameId, gameName, installPath, engine) {
                    root.installAndShowDetail(gameId, gameName, installPath, engine)
                }
                onManualFolderRequested: root.manualFolderRequested()
                onHelpRequested: root.helpRequested()
            }
        }
    }

    Loader {
        id: allGamesLoader
        anchors.fill: parent
        active: root.currentPage === 2 || allGamesLoader.status === Loader.Ready
        visible: root.currentPage === 2
        asynchronous: true
        onLoaded: {
            if (root._allGamesArgs) {
                item.open(root._allGamesArgs[0], root._allGamesArgs[1], root._allGamesArgs[2])
                root._allGamesArgs = null
            }
        }
        sourceComponent: Component {
            AllGames {
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
            }
        }
    }

    Loader {
        id: helpLoader
        anchors.fill: parent
        active: root.currentPage === 3 || helpLoader.status === Loader.Ready
        visible: root.currentPage === 3
        asynchronous: true
        sourceComponent: Component {
            HelpScreen {
                contentMargin: root.contentMargin
                onAllGamesRequested: root.allGamesRequested("", "", "")
                onLibraryRequested: root.libraryRequested()
            }
        }
    }

    Loader {
        id: whatsNewLoader
        anchors.fill: parent
        active: root.currentPage === 4 || whatsNewLoader.status === Loader.Ready
        visible: root.currentPage === 4
        asynchronous: true
        sourceComponent: Component {
            WhatsNewScreen {
                contentMargin: root.contentMargin
            }
        }
    }
}
