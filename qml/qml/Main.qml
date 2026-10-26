// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Shapes
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Main.qml - Application main window with title bar, navigation, and content stack
 */
ApplicationWindow {
    id: window
    visible: true

    // The launcher's proportions (main.cpp locks them while resizing) and
    // the largest window of them the screen's work area holds: a small or
    // highly scaled screen lowers the minimum instead of cutting the window.
    readonly property real _ratio: Dimensions.minWindowWidth / Dimensions.minWindowHeight
    // (desktopAvailable* can read 0 before the window reaches its screen)
    readonly property real _availW: Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth
                                    : Screen.width > 0 ? Screen.width : 1366
    readonly property real _availH: Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight
                                    : Screen.height > 0 ? Screen.height * 0.94 : 728
    readonly property real _fitWidth: Math.min(_availW * 0.96, _availH * 0.96 * _ratio)
    minimumWidth: Math.round(Math.min(Dimensions.minWindowWidth, _fitWidth))
    minimumHeight: Math.round(minimumWidth / _ratio)

    // Initial size — roomy, centered (replaces C++ MoveWindow)
    width: Math.round(Math.max(minimumWidth, Math.min(Dimensions.defaultWindowWidth, _fitWidth * 0.92)))
    height: Math.round(width / _ratio)
    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)

    title: "Makine \u00C7eviri - Makine Launcher"
    color: Theme.chrome

    flags: Qt.Window | Qt.FramelessWindowHint

    // Aspect ratio lock handled natively via WM_SIZING in C++ (main.cpp)
    // — zero recursive bindings, zero frame drops during resize

    // Coming back to the launcher re-checks the catalog (at most every
    // 10 minutes; quiet when nothing changed), so a newly published game or
    // package update appears without a restart.
    // The same for a new launcher version, at most every 6 hours.
    onActiveChanged: {
        if (!active || window._onboardingActive) return
        ManifestSync.syncIfStale(10 * 60 * 1000)
        UpdateService.checkIfStale(6 * 60 * 60 * 1000)
    }

    Component.onCompleted: {
        // Freeze geometry — assigning the current value back breaks the
        // Screen.width / Screen.height bindings above, so dragging the
        // window to another monitor will not resize or recenter it.
        const w = window.width, h = window.height, xx = window.x, yy = window.y
        window.width = w
        window.height = h
        window.x = xx
        window.y = yy

        // --onboarding shows the welcome flow again (design work, or a user
        // who wants to redo their first-run choices).
        if (typeof SettingsManager !== "undefined")
            window._onboardingActive = !SettingsManager.onboardingCompleted
                                       || Qt.application.arguments.indexOf("--onboarding") >= 0

        // First start after the launcher updated: what changed, once
        if (!window._onboardingActive && !UpdateService.notesSeen)
            Qt.callLater(window._go, 5)
    }

    // Cross-monitor refresh — when the window crosses to a screen with a
    // different DPI or refresh rate, briefly nudge the width to force the
    // QML scene graph to re-evaluate font metrics, layouts and atlas
    // textures with the new pixel ratio. Without this, the user can see
    // stale glyph rendering or one-frame layout glitches at the boundary.
    onScreenChanged: {
        Qt.callLater(function() {
            const w = window.width
            window.width = w + 1
            window.width = w
        })
    }

    property int currentNavIndex: 0

    // Shows the page behind a navigation index: 0 Ana Sayfa · 1 Kütüphanem ·
    // 2 Ayarlar · 3 Tüm Oyunlar · 4 Nasıl çalışır? · 5 Yenilikler
    // ===== HISTORY =====
    // Where the user has been — pages by index, games by what opened them —
    // walked back and forward by the mouse's side buttons, Alt+← / Alt+→,
    // Esc and the game page's back button, the way a browser walks pages.
    readonly property var _pageTitles: [qsTr("Ana Sayfa"), qsTr("Kütüphanem"), qsTr("Ayarlar"), qsTr("Tüm Oyunlar"),
                                        qsTr("Nasıl çalışır?"), qsTr("Yenilikler"), qsTr("Topluluk")]
    property var _backStack: []
    property var _forwardStack: []
    property var _shownGame: null
    property bool _walking: false

    function _here() {
        return contentStackContainer.currentIndex === 2 && window._shownGame
               ? { game: window._shownGame } : { page: window.currentNavIndex }
    }
    function _remember() {
        if (window._walking) return
        const b = window._backStack.slice()
        b.push(window._here())
        if (b.length > 50) b.shift()
        window._backStack = b
        window._forwardStack = []
    }
    // A game chosen on Home (or one whose install was asked for): its page
    function _openGame(gameId, gameName, installPath, engine, install) {
        GameService.noteScreen(install ? "game-install" : "game", gameId)
        window._remember()
        window._shownGame = { id: gameId, name: gameName, path: installPath, engine: engine }
        detailVM.loadGame(gameDataResolver.resolve(gameId, gameName, installPath, engine, install))
        detailVM.fromLibrary = (homeView.currentPage === 1)
        window.previousNavIndex = window.currentNavIndex
        contentStackContainer.navigateTo(2)
    }
    function _show(entry) {
        window._walking = true
        if (entry.game) {
            const g = entry.game
            detailVM.loadGame(gameDataResolver.resolve(g.id, g.name, g.path, g.engine, false))
            window._shownGame = g
            contentStackContainer.navigateTo(2)
        } else {
            window._go(entry.page)
        }
        window._walking = false
    }
    function historyBack() {
        if (window._onboardingActive) return
        if (window._backStack.length === 0) {
            if (contentStackContainer.currentIndex !== 0 || window.currentNavIndex !== 0)
                window._go(contentStackContainer.currentIndex === 2 ? window.previousNavIndex : 0)
            return
        }
        const b = window._backStack.slice()
        const to = b.pop()
        window._backStack = b
        const f = window._forwardStack.slice()
        f.push(window._here())
        window._forwardStack = f
        window._show(to)
    }
    function historyForward() {
        if (window._onboardingActive || window._forwardStack.length === 0) return
        const f = window._forwardStack.slice()
        const to = f.pop()
        window._forwardStack = f
        const b = window._backStack.slice()
        b.push(window._here())
        window._backStack = b
        window._show(to)
    }
    // Where back leads, for the game page's back button
    readonly property string backTitle: {
        const top = window._backStack.length > 0 ? window._backStack[window._backStack.length - 1] : null
        if (top && top.game) return top.game.name || qsTr("Geri")
        const page = top ? top.page : window.previousNavIndex
        return window._pageTitles[page] ?? qsTr("Geri")
    }

    function _go(index) {
        // A move to another page is a step in the history
        if (contentStackContainer.currentIndex === 2 || window.currentNavIndex !== index)
            window._remember()
        window.currentNavIndex = index
        GameService.noteScreen(["home", "library", "settings", "all-games", "help", "whats-new", "community"][index] || "?")
        if (index === 2) {
            contentStackContainer.navigateTo(1)
            return
        }
        contentStackContainer.navigateTo(0)
        if (index === 1) homeView.showLibraryPage()
        else if (index === 3) homeView.showAllGamesPage()
        else if (index === 4) homeView.showHelpPage()
        else if (index === 5) homeView.showWhatsNewPage()
        else if (index === 6) homeView.showCommunityPage()
        else homeView.showHomePage()
    }
    property int previousNavIndex: 0  // Remember nav index before game detail

    // Force quit flag — bypasses minimize-to-tray on close
    property bool forceQuit: false

    // Onboarding: start false to avoid creating/destroying OnboardingWizard
    // for returning users. Component.onCompleted flips to true if needed.
    property bool _onboardingActive: false

    // Settings preloaded behind splash (C++ trigger in main.cpp).
    // GameDetail stays on-demand with _keepAlive — acceptable first-visit delay.
    property bool _settingsPreload: false

    Component.onDestruction: pageChangeTimer.stop()

    onClosing: function(close) {
        if (SettingsManager.minimizeToTray && !window.forceQuit) {
            close.accepted = false
            window.minimizeToTray()
        }
        // When minimizeToTray is off, close.accepted stays true (default).
        // setQuitOnLastWindowClosed(true) handles the rest — no Qt.quit() needed.
    }

    function minimizeToTray() {
        window.hide()
    }

    // ===== CONTROLLERS =====
    GameDataResolver { id: gameDataResolver }
    GameDetailViewModel { id: detailVM }

    InstallFlowController {
        id: installFlow
        viewModel: detailVM
        onShowAntiCheatWarning: antiCheatWarningLoader.active = true
        onShowInstallOptions: installOptionsLoader.active = true
        onShowVariantSelection: variantSelectionLoader.active = true
    }

    // ===== SYSTEM TRAY =====
    Connections {
        target: SystemTrayManager
        function onShowWindowRequested() {
            window.show(); window.raise(); window.requestActivate()
        }
        function onQuitRequested() {
            window.forceQuit = true
            Qt.quit()
        }
        function onSettingsRequested() {
            window.show(); window.raise(); window.requestActivate()
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
        function onUpdateCheckRequested() {
            UpdateService.check()
        }
    }

    // TranslationDownloader → InstallFlowService is wired natively in
    // main.cpp (B2-03) so the install pipeline survives QML reload/GC.

    // ===== GAME SERVICE: anti-cheat + translation impact signals =====
    Connections {
        target: GameService
        function onAntiCheatWarningNeeded(gameId, antiCheatData) {
            installFlow.onAntiCheatWarningNeeded(gameId, antiCheatData)
        }
    }

    // ===== CORE BRIDGE: package detail enrichment =====
    Connections {
        target: CoreBridge
        function onPackageDetailEnriched(appId) {
            installFlow.onPackageDetailEnriched(appId)
            // Refresh ViewModel when detail arrives for the active game
            if (detailVM.gameId === appId)
                detailVM._applyGameDetails()
        }
    }

    // ===== AUTO DETECT SETTING: stop/start process scanner =====
    Connections {
        target: SettingsManager
        function onAutoDetectGamesChanged() {
            if (!SettingsManager.autoDetectGames)
                ProcessScanner.stopWatching()
            else
                ProcessScanner.startWatching(windowActive ? 30000 : 120000)
        }
    }

    // ===== KEYBOARD SHORTCUTS =====
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    // Back and forward walk the history (a game's page goes back where it
    // was opened from, not always home)
    Shortcut {
        sequences: [StandardKey.Back, "Alt+Left"]
        enabled: !window._onboardingActive
        onActivated: window.historyBack()
    }
    Shortcut {
        sequences: [StandardKey.Forward, "Alt+Right"]
        enabled: !window._onboardingActive
        onActivated: window.historyForward()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !window._onboardingActive && contentStackContainer.currentIndex !== 0
        onActivated: window.historyBack()
    }
    Shortcut {
        sequence: "Ctrl+,"
        enabled: !window._onboardingActive
        onActivated: window._go(2)
    }
    Shortcut {
        sequence: "Ctrl+H"
        enabled: !window._onboardingActive
        onActivated: window._go(0)
    }
    Shortcut {
        sequence: "Ctrl+1"
        enabled: !window._onboardingActive
        onActivated: window._go(0)
    }
    Shortcut {
        sequence: "Ctrl+2"
        enabled: !window._onboardingActive
        onActivated: window._go(1)
    }
    Shortcut {
        sequence: "Ctrl+3"
        enabled: !window._onboardingActive
        onActivated: window._go(3)
    }
    Shortcut {
        sequence: "F1"
        enabled: !window._onboardingActive
        onActivated: window._go(4)
    }
    Shortcut {
        sequence: "Ctrl+R"
        enabled: !window._onboardingActive
        onActivated: GameService.checkForUpdates()
    }
    Shortcut {
        sequence: "Ctrl+F"
        enabled: !window._onboardingActive
        onActivated: sidebar.focusSearch()
    }

    // GPU/CPU Optimization: Pause animations whenever the app is not the
    // active foreground application. This keeps idle background draw cost
    // near zero — users reported the launcher consuming resources while
    // they were focused on other apps. Background work (downloads, installs,
    // manifest sync) continues independently in the C++ services layer.
    readonly property bool animationsEnabled: SettingsManager.enableAnimations &&
                                              window.visible &&
                                              window.visibility !== Window.Minimized &&
                                              window.visibility !== Window.Hidden &&
                                              Qt.application.state === Qt.ApplicationActive

    // Visibility-aware resource management (used by ProcessScanner watcher
    // below — keeps the slow-poll path when minimized but still in taskbar).
    readonly property bool windowActive: window.visible &&
                                         window.visibility !== Window.Minimized &&
                                         window.visibility !== Window.Hidden

    onWindowActiveChanged: {
        if (window._onboardingActive) return
        if (!SettingsManager.autoDetectGames) return
        if (windowActive) {
            ProcessScanner.startWatching(30000)
        } else if (!window.visible) {
            // Window is hidden (tray) — stop scanning entirely, zero CPU
            ProcessScanner.stopWatching()
        } else {
            // Minimized but still visible in taskbar — slow poll, 120s
            ProcessScanner.startWatching(120000)
        }
    }

    // ===== FIT TO THE WINDOW =====
    // The pages are laid out for a window `designWidth` wide. A smaller one
    // shows them scaled down to fit (down to 72 %) instead of crowding them;
    // a bigger one is never scaled up — it gets more room. Everything the
    // window shows sits on this stage; dialogs, menus and tips follow the
    // same factor through Dimensions.uiZoom.
    readonly property real uiZoom: Math.max(0.72, Math.min(1, window.width / Dimensions.designWidth))
    Binding { target: Dimensions; property: "uiZoom"; value: window.uiZoom }

    Item {
        id: stage
        width: window.width / window.uiZoom
        height: window.height / window.uiZoom
        scale: window.uiZoom
        transformOrigin: Item.TopLeft

        // A press anywhere outside the text field being typed in lets it go
        // (its ring with it). A PointHandler watches without taking the press.
        PointHandler {
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (!active) return
                const f = window.activeFocusItem
                if (!f || !("cursorPosition" in f)) return
                const p = f.mapFromItem(stage, point.position.x, point.position.y)
                if (p.x < -8 || p.y < -8 || p.x > f.width + 8 || p.y > f.height + 8)
                    stage.forceActiveFocus()
            }
        }

        // The mouse's side buttons: back and forward through the history.
        // Only those two buttons, so every other press goes where it went.
        TapHandler {
            acceptedButtons: Qt.BackButton | Qt.ForwardButton
            onTapped: (eventPoint, button) => button === Qt.BackButton ? window.historyBack()
                                                                       : window.historyForward()
        }
    }

    // The window is a frame (Theme.chrome) with the sidebar standing on it and
    // the pages in one lit, rounded panel joined to the sidebar's edge. This
    // is the panel's floor; panelFrame below rounds and outlines it.
    // (Items on the stage are placed by x/y, not anchored to one another:
    // anchors are resolved before `parent: stage` lands, against the window.)
    AppBackground {
        parent: stage
        // Stacking on the stage follows z, not the order the items reach it
        z: -1
        x: mainContent.x; y: mainContent.y
        width: mainContent.width; height: mainContent.height
        visible: !window._onboardingActive
    }

    // ===== SIDEBAR =====
    Sidebar {
        id: sidebar
        parent: stage
        visible: !window._onboardingActive
        x: 8
        y: 8
        width: Dimensions.appSidebarWidth
        height: parent.height - 16
        windowRef: window
        currentIndex: window.currentNavIndex
        onNavigate: (index) => window._go(index)
        // The search searches the catalogue: typing shows "Tüm Oyunlar"
        onSearchEdited: (text) => {
            homeView.allGamesQuery = text
            if (text.trim() !== "" && (window.currentNavIndex !== 3 || contentStackContainer.currentIndex !== 0))
                window._go(3)
        }
        onSearchAccepted: if (window.currentNavIndex === 3) homeView.focusAllGamesResults()
    }

    Item {
        id: mainContent
        parent: stage
        z: 1
        visible: !window._onboardingActive
        x: sidebar.x + sidebar.width
        y: 8
        width: stage.width - x - 8
        height: stage.height - 16

        // ===== CONTENT STACK - crossfade between pages =====
        Item {
            id: contentStackContainer
            anchors.fill: parent
            clip: true

            property int currentIndex: 0
            property int previousIndex: 0
            property bool transitioning: false

            property bool homeVisible: true
            property bool settingsVisible: false
            property bool gameDetailVisible: false

            readonly property var _pageNames: ["Home", "Settings", "GameDetail"]

            function navigateTo(index) {
                if (index === currentIndex || transitioning) return
                transitioning = true
                previousIndex = currentIndex

                if (typeof SceneProfiler !== "undefined")
                    SceneProfiler.beginTransition(_pageNames[previousIndex] || "?", _pageNames[index] || "?")

                var outgoingPage = getPage(previousIndex)
                var incomingPage = getPage(index)

                if (outgoingPage && incomingPage) {
                    setPageVisible(index, true)
                    incomingPage.opacity = 0
                    incomingPage.y = 8

                    fadeOutAnimation.target = outgoingPage
                    fadeInAnimation.target = incomingPage

                    fadeOutAnimation.start()
                    fadeInAnimation.start()

                    pageChangeTimer.newIndex = index
                    pageChangeTimer.start()
                }
            }

            function setPageVisible(index, visible) {
                switch(index) {
                    case 0: homeVisible = visible; break
                    case 1: settingsVisible = visible; break
                    case 2: gameDetailVisible = visible; break
                }
            }

            function getPage(index) {
                switch(index) {
                    case 0: return homeView
                    case 1: return settingsLoader
                    case 2: return gameDetailLoader
                    default: return null
                }
            }

            Timer {
                id: pageChangeTimer
                interval: Dimensions.transitionDuration
                property int newIndex: 0
                onTriggered: {
                    var oldPage = contentStackContainer.getPage(contentStackContainer.previousIndex)
                    if (oldPage) {
                        oldPage.opacity = 1.0
                        oldPage.y = 0
                    }
                    contentStackContainer.setPageVisible(contentStackContainer.previousIndex, false)
                    contentStackContainer.currentIndex = newIndex
                    contentStackContainer.transitioning = false

                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.endTransition()
                }
            }

            // Outgoing page: fade out + subtle slide up
            ParallelAnimation {
                id: fadeOutAnimation
                property var target: null
                NumberAnimation {
                    target: fadeOutAnimation.target
                    property: "opacity"
                    from: 1.0; to: 0
                    duration: Dimensions.animPageOut
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: fadeOutAnimation.target
                    property: "y"
                    from: 0; to: -6
                    duration: Dimensions.animPageOut
                    easing.type: Easing.InCubic
                }
            }

            // Incoming page: fade in + slide up from below
            ParallelAnimation {
                id: fadeInAnimation
                property var target: null
                NumberAnimation {
                    target: fadeInAnimation.target
                    property: "opacity"
                    from: 0; to: 1.0
                    duration: Dimensions.animPageIn
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: fadeInAnimation.target
                    property: "y"
                    from: 8; to: 0
                    duration: Dimensions.animPageIn
                    easing.type: Easing.OutCubic
                }
            }

            HomeScreen {
                id: homeView
                anchors.fill: parent
                visible: contentStackContainer.homeVisible
                animationsEnabled: window.animationsEnabled

                onGameSelected: function(gameId, gameName, installPath, engine) {
                    window._openGame(gameId, gameName, installPath, engine, false)
                }
                onManualFolderRequested: manualFolderDialog.open()
                onLibraryRequested: window._go(1)
                onHelpRequested: window._go(4)
                onAllGamesRequested: (filter, sort) => {
                    window.currentNavIndex = 3
                    contentStackContainer.navigateTo(0)
                    sidebar.setSearch("")
                    homeView.showAllGamesPage(filter, sort)
                }
                onSearchClearRequested: sidebar.setSearch("")

                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    window._openGame(gameId, gameName, installPath, engine, true)
                }
            }

            // Lazy-loaded settings page (keep alive after first load)
            Loader {
                id: settingsLoader
                anchors.fill: parent
                active: contentStackContainer.settingsVisible || window._settingsPreload
                visible: contentStackContainer.settingsVisible
                asynchronous: true
                onLoaded: {
                    // Keep alive forever: once loaded, never deactivate.
                    // Use _settingsPreload as the persistent flag to avoid
                    // a binding loop (active → onLoaded → active cycle).
                    window._settingsPreload = true
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.markLoaderReady("SettingsScreen")
                }
                onVisibleChanged: if (visible && item) Qt.callLater(item.resetScroll)
                sourceComponent: Component {
                    SettingsScreen {
                        onBack: {
                            window.currentNavIndex = 0
                            contentStackContainer.navigateTo(0)
                        }
                    }
                }
            }

            // Lazy-loaded game detail page (keep alive after first load)
            Loader {
                id: gameDetailLoader
                property bool _gameDetailKeepAlive: false
                anchors.fill: parent
                active: contentStackContainer.gameDetailVisible || _gameDetailKeepAlive
                visible: contentStackContainer.gameDetailVisible
                asynchronous: true
                onLoaded: {
                    // Set keep-alive on next event loop iteration to break
                    // the binding loop (active → onLoaded → active cycle).
                    Qt.callLater(function() { gameDetailLoader._gameDetailKeepAlive = true })
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.markLoaderReady("GameDetailScreen")
                }
                sourceComponent: Component {
                    GameDetailScreen {
                        viewModel: detailVM
                        onTranslateClicked: {
                            installFlow.startInstallFlow(detailVM.gameId, detailVM.gameName)
                        }
                        // The back button names where it leads, as Apple's do
                        backTitle: window.backTitle
                        onBackClicked: window.historyBack()
                    }
                }
            }

        }
    }

    // ===== PAGE PANEL =====
    // Rounds the panel by painting the frame over the pages' square corners
    // (no layer, no mask: the frame is one flat colour), then draws its
    // hairline with the lit top edge every container has.
    Item {
        id: panelFrame
        parent: stage
        z: 2
        x: mainContent.x; y: mainContent.y
        width: mainContent.width; height: mainContent.height
        visible: mainContent.visible
        readonly property real r: Dimensions.radiusSection

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: -1
                fillColor: Theme.chrome
                fillRule: ShapePath.OddEvenFill
                PathRectangle { x: -1; y: -1; width: panelFrame.width + 2; height: panelFrame.height + 2 }
                PathRectangle { x: 0; y: 0; width: panelFrame.width; height: panelFrame.height; radius: panelFrame.r }
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: panelFrame.r
            color: "transparent"
            border.width: 1
            border.color: Theme.containerBorder
        }
        // Lit along the top and, fainter, along the foot
        Repeater {
            model: Theme.darkMode ? [0, 1] : []
            Rectangle {
                required property int modelData
                x: panelFrame.r
                y: modelData === 0 ? 0 : panelFrame.height - 1
                width: panelFrame.width - panelFrame.r * 2
                height: 1
                opacity: modelData === 0 ? 1 : 0.7
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.3; color: Theme.containerHighlight }
                    GradientStop { position: 0.7; color: Theme.containerHighlight }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }
    }

    // ===== WINDOW CHROME =====
    // No title bar: the top strip of the page drags the window (a press that
    // a control under it takes stays that control's). The caption buttons sit
    // flush in the page panel's top right corner, as Windows 11 puts them in
    // the window's — in the window's own while the welcome flow covers it.
    Item {
        id: dragStrip
        parent: stage
        visible: !window._onboardingActive
        // On a game's page the strip starts after its back button: a press
        // under the strip never reaches the control below it
        x: mainContent.x + (contentStackContainer.gameDetailVisible ? 280 : 0)
        width: windowControls.x - x
        height: 44
        z: Dimensions.zWindowControls - 1
        DragHandler {
            target: null
            onActiveChanged: if (active) window.startSystemMove()
        }
    }

    WindowControls {
        id: windowControls
        parent: stage
        // The welcome flow has the same framed panel (OnboardingWizard): its corner
        x: window._onboardingActive ? stage.width - 8 - width : mainContent.x + mainContent.width - width
        y: window._onboardingActive ? 8 : mainContent.y
        z: Dimensions.zWindowControls
        cornerRadius: panelFrame.r
        showTray: !window._onboardingActive
        onTrayClicked: window.minimizeToTray()
        onMinimizeClicked: window.showMinimized()
        onCloseClicked: window.close()
    }

    // ===== NOTICES =====
    // States of the whole app, floated over the top of the page: the build
    // failing its integrity check, and no internet.
    Column {
        parent: stage
        x: mainContent.x + Math.round((mainContent.width - width) / 2)
        anchors.top: parent.top
        anchors.topMargin: 10
        spacing: 8
        z: Dimensions.zToast

        AppNotice {
            visible: IntegrityService.status === "failed" && !_dismissed
            property bool _dismissed: false
            glyph: Icons.shieldAlert
            tint: Theme.warning
            text: qsTr("Bütünlük doğrulaması başarısız — bu program değiştirilmiş olabilir. Launcher'ı yalnız makineceviri.org'dan indir.")
            dismissible: true
            onDismissed: _dismissed = true
        }
        // An install or uninstall cut off last time (crash, power) that the
        // start-up recovery could not fully undo
        AppNotice {
            readonly property string _game: typeof recoveryFailedGameId === "string" ? recoveryFailedGameId : ""
            property bool _dismissed: false
            visible: _game !== "" && !_dismissed && !window._onboardingActive
            glyph: Icons.triangleAlert
            tint: Theme.warning
            dismissible: true
            text: {
                const g = _game !== "" && _game !== "?" ? GameService.getGameById(_game) : null
                const name = (g && g.name) || ""
                return name !== ""
                    ? qsTr("%1 için geçen sefer yarım kalan işlem tam olarak geri alınamadı. Oyunun sayfasından yamayı kaldırıp yeniden kur ya da mağazada oyun dosyalarını doğrulat.").arg(name)
                    : qsTr("Geçen sefer yarım kalan bir yama işlemi tam olarak geri alınamadı. O oyunun sayfasından yamayı kaldırıp yeniden kur ya da mağazada oyun dosyalarını doğrulat.")
            }
            onDismissed: _dismissed = true
        }
        AppNotice {
            visible: ManifestSync.isOffline
            glyph: Icons.wifiOff
            tint: Theme.error
            busy: ManifestSync.isSyncing
            text: ManifestSync.isSyncing ? qsTr("İnternete bağlanılamıyor — yeniden deneniyor…")
                                         : qsTr("İnternete bağlanılamıyor. Kurulu yamaların çalışır; yeni yama için bağlantı gerekir.")
        }
    }

    // A game folder dropped anywhere but a game's page (which takes it itself):
    // found out which game it is and opened, the same as "Oyun klasörünü seç".
    FolderDropOverlay {
        parent: contentStackContainer
        anchors.fill: parent
        z: 40
        accepts: !contentStackContainer.gameDetailVisible
        title: qsTr("Oyun klasörünü bırak")
        subtitle: qsTr("Hangi oyun olduğunu bulup sayfasını açarız; Türkçe yaması varsa tek tıkla kurulur.")
        onFolderDropped: (url) => GameService.addManualGame(url.toString())
    }

    // ===== MANUAL GAME FOLDER DIALOG =====
    FolderDialog {
        id: manualFolderDialog
        title: qsTr("Oyun Klasörünü Seç")
        onAccepted: GameService.addManualGame(selectedFolder.toString())
    }

    Connections {
        target: GameService
        function onManualGameAdded(gameId) {
            if (!gameId || gameId === "") return
            var gameData = GameService.getGameById(gameId)
            var gameName = (gameData && gameData.name) || ""
            var engine = (gameData && gameData.engine) || "Unknown"
            var installPath = (gameData && gameData.installPath) || ""
            homeView.gameSelected(gameId, gameName, installPath, engine)
        }
        function onManualGameRejected(path, reason) {
            gameToast.show("", "", qsTr("Oyun eklenemedi"), reason)
        }
        // An install or uninstall the user walked away from still has to say
        // how it ended; the detail page only speaks for the game it shows.
        function onTranslationInstallCompleted(gameId, success, message) {
            if (window._detailShows(gameId)) return
            var name = window._gameName(gameId)
            gameToast.show("", name,
                success ? qsTr("%1 için Türkçe yama kuruldu").arg(name)
                        : qsTr("%1 için yama kurulamadı").arg(name),
                success ? qsTr("Ayrıntılar için oyunun sayfasını açın.") : message)
        }
        function onTranslationUninstalled(gameId, success, message) {
            if (success || window._detailShows(gameId)) return
            gameToast.show("", "", qsTr("%1 için yama kaldırılamadı").arg(window._gameName(gameId)),
                           message)
        }
    }

    function _detailShows(gameId) {
        return contentStackContainer.gameDetailVisible && detailVM.gameId === gameId
    }

    function _gameName(gameId) {
        var game = GameService.getGameById(gameId)
        return (game && game.name) || gameId
    }

    Connections {
        target: homeView
        function onSettingsRequested() {
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
    }

    // ===== ANTI-CHEAT WARNING DIALOG (lazy) =====
    Loader {
        id: antiCheatWarningLoader
        active: false
        sourceComponent: Component {
            AntiCheatWarningDialog {
                parent: Overlay.overlay
                onContinueAnyway: {
                    installFlow.onAntiCheatContinue()
                }
                onClosed: {
                    antiCheatWarningLoader.active = false
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogClose("AntiCheatWarning")
                }
                Component.onCompleted: {
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogOpen("AntiCheatWarning")
                    if (installFlow.pendingAntiCheatData) {
                        gameName = installFlow.pendingAntiCheatData.gameName
                        detectedSystems = installFlow.pendingAntiCheatData.detectedSystems
                        installFlow.pendingAntiCheatData = null
                    }
                    // An install only starts from the game's page, so detailVM is that game
                    artUrl = detailVM.heroUrl
                    if (detailVM.steamAppId !== "")
                        artFallbackUrl = "https://cdn.makineceviri.org/assets/banners/" + detailVM.steamAppId + ".jpg"
                    open()
                }
            }
        }
    }

    // ===== INSTALL OPTIONS DIALOG (lazy) =====
    Loader {
        id: installOptionsLoader
        active: false
        sourceComponent: Component {
            InstallOptionsDialog {
                parent: Overlay.overlay
                onOptionsConfirmed: function(selectedIds) {
                    installFlow.onOptionsConfirmed(selectedIds)
                }
                onCancelled: installFlow.onOptionsCancelled()
                onClosed: {
                    installOptionsLoader.active = false
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogClose("InstallOptions")
                }
                Component.onCompleted: {
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogOpen("InstallOptions")
                    if (installFlow.pendingInstallOptionsData) {
                        options = installFlow.pendingInstallOptionsData.options
                        specialMode = installFlow.pendingInstallOptionsData.specialDialog || ""
                        gameName = installFlow.pendingInstallOptionsData.gameName || ""
                    }
                    open()
                }
            }
        }
    }

    // ===== VARIANT SELECTION DIALOG (lazy) =====
    Loader {
        id: variantSelectionLoader
        active: false
        sourceComponent: Component {
            VariantSelectionDialog {
                parent: Overlay.overlay
                onVariantSelected: function(variant) {
                    installFlow.onVariantSelected(variant)
                }
                onCancelled: installFlow.onVariantCancelled()
                onClosed: {
                    variantSelectionLoader.active = false
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogClose("VariantSelection")
                }
                Component.onCompleted: {
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogOpen("VariantSelection")
                    if (installFlow.pendingVariantData) {
                        variants = installFlow.pendingVariantData.variants
                        variantType = installFlow.pendingVariantData.variantType || "version"
                    }
                    open()
                }
            }
        }
    }

    // ===== UPDATE ALERT DIALOG (lazy) =====
    property var _pendingImpacts: []
    Loader {
        id: updateAlertLoader
        active: false
        sourceComponent: Component {
            UpdateAlertDialog {
                parent: Overlay.overlay
                onClosed: {
                    updateAlertLoader.active = false
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogClose("UpdateAlert")
                }
                Component.onCompleted: {
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogOpen("UpdateAlert")
                    if (window._pendingImpacts) {
                        for (var i = 0; i < window._pendingImpacts.length; i++) {
                            var p = window._pendingImpacts[i]
                            addAffectedGame(p.gameId, p.gameName, p.impact)
                        }
                        window._pendingImpacts = []
                        if (affectedGames.length > 0) open()
                    }
                }
            }
        }
    }

    // ===== ONBOARDING WIZARD OVERLAY (unified auth + onboarding gate) =====
    Loader {
        id: onboardingLoader
        parent: stage
        anchors.fill: parent
        active: window._onboardingActive
        // A Loader is a focus scope: without this the wizard never gets
        // the keys (Enter for the step's main action).
        focus: active
        z: Dimensions.zOverlay
        sourceComponent: Component {
            OnboardingWizard {
                animationsEnabled: window.animationsEnabled
                onWizardFinished: {
                    // Only scan + persist for first-launch (not returning users)
                    if (window._onboardingActive) {
                        if (typeof GameService !== "undefined" && GameService.gameCount === 0) {
                            GameService.scanAllLibraries()
                        }
                        if (typeof SettingsManager !== "undefined") {
                            SettingsManager.onboardingCompleted = true
                        }
                        // A new player has nothing to catch up on
                        UpdateService.markNotesSeen()
                    }
                    window._onboardingActive = false
                }
            }
        }
    }

    // ===== PERFORMANCE MONITOR (F3 to toggle, dev builds only) =====
    property bool showPerformanceMonitor: false

    PerformanceMonitor {
        id: perfMonitor
        parent: stage
        visible: devToolsEnabled && window.showPerformanceMonitor
        z: Dimensions.zDebug
    }

    Shortcut {
        enabled: devToolsEnabled
        sequence: "F3"
        onActivated: window.showPerformanceMonitor = !window.showPerformanceMonitor
    }

    // ===== GAME DETECTION TOAST =====
    GameToast {
        id: gameToast
        parent: stage
        z: Dimensions.zToast
    }

    // ===== SWR CATALOG REFRESH BADGE =====
    RefreshIndicator {
        parent: stage
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 24
        anchors.bottomMargin: 24
        z: Dimensions.zOverlay
    }

    Connections {
        target: ProcessScanner
        function onGameDetected(gameId, gameName) {
            gameToast.show(gameId, gameName)
        }
        function onProcessResolved(gameId, gameName, installPath) {
            // Add detected game to library, then show notification
            GameService.addManualGame(installPath)
            gameToast.show(gameId, gameName,
                qsTr("%1 kütüphaneye eklendi!").arg(gameName),
                qsTr("Kütüphaneden Türkçe yama yükleyebilirsiniz."))
        }
        function onProcessNotSupported(processName) {
            gameToast.show("", processName,
                qsTr("%1 desteklenmiyor").arg(processName),
                qsTr("Bu oyun henüz Türkçeye çevrilmedi."))
        }
    }

    // ===== PLUGIN REQUESTS (dev builds only: PluginManager is null otherwise) =====
    // A plugin asks, the user answers through these; the request id ties the two.
    property int _regionRequestId: 0
    property int _attachRequestId: 0
    property string _attachPluginName: ""
    property string _attachGameName: ""
    property var _attachQueue: []

    function _pluginName(pluginId) {
        var info = PluginManager ? PluginManager.plugin(pluginId) : null
        return (info && info.name) || pluginId
    }

    function _answerRegion(x, y, w, h) {
        if (window._regionRequestId > 0 && PluginManager)
            PluginManager.answerRegion(window._regionRequestId, x, y, w, h)
        window._regionRequestId = 0
    }

    function _answerAttach(allow, remember) {
        if (window._attachRequestId > 0 && PluginManager)
            PluginManager.answerAttach(window._attachRequestId, allow, remember)
        window._attachRequestId = 0
    }

    // One dialog at a time; the rest wait in line.
    function _showNextAttach() {
        if (attachDialog.visible || window._attachQueue.length === 0) return
        var next = window._attachQueue[0]
        window._attachQueue = window._attachQueue.slice(1)
        window._attachRequestId = next.requestId
        window._attachPluginName = next.pluginName
        window._attachGameName = next.gameName
        attachDialog.open()
    }

    Connections {
        target: PluginManager
        function onRegionRequested(requestId, pluginId, pluginName) {
            window._regionRequestId = requestId
            regionSelector.pluginName = pluginName
            regionSelector.show()
        }
        function onAttachRequested(requestId, pluginId, pluginName, gameName) {
            window._attachQueue = window._attachQueue.concat([
                { requestId: requestId, pluginId: pluginId, pluginName: pluginName, gameName: gameName }])
            window._showNextAttach()
        }
        // The launcher forgets a request when its plugin stops; so must the UI.
        function onRequestCancelled(requestId) {
            if (window._regionRequestId === requestId) {
                window._regionRequestId = 0
                regionSelector.hide()
            }
            if (window._attachRequestId === requestId) {
                window._attachRequestId = 0
                attachDialog.close()
            }
            window._attachQueue = window._attachQueue.filter(function(r) {
                return r.requestId !== requestId
            })
        }
        function onNotify(pluginId, pluginName, title, message, kind) {
            var mark = { success: "✓ ", warning: "⚠ ", error: "✕ " }[kind] || ""
            gameToast.show("", pluginName,
                mark + (title ? pluginName + ": " + title : pluginName),
                message || qsTr("Eklenti bildirimi"))
        }
        function onPluginError(pluginId, message) {
            var name = pluginId ? window._pluginName(pluginId) : ""
            gameToast.show("", name,
                name ? qsTr("Eklenti hatası: %1").arg(name) : qsTr("Eklenti hatası"),
                message || qsTr("Bilinmeyen bir hata oluştu."))
        }
        function onPluginInstalled(pluginId) {
            var name = window._pluginName(pluginId)
            gameToast.show("", name, qsTr("%1 kuruldu").arg(name),
                qsTr("Ayarlar > Eklentiler bölümünden yönetebilirsiniz."))
        }
    }

    // ===== PLUGIN REGION SELECTOR =====
    RegionSelector {
        id: regionSelector
        onRegionSelected: function(rx, ry, rw, rh) { window._answerRegion(rx, ry, rw, rh) }
        onCancelled: window._answerRegion(0, 0, 0, 0)
    }

    // ===== PLUGIN TRANSLATION OVERLAY =====
    // The plugin shows and hides it (overlay.show / overlay.hide), so no timer here.
    TranslationOverlay {
        id: translationOverlay
        autoHide: false
        sourceRegion: PluginManager ? PluginManager.overlayRegion : Qt.rect(0, 0, 0, 0)
        translatedText: PluginManager ? PluginManager.overlayText : ""
        ocrText: PluginManager ? PluginManager.overlaySourceText : ""
    }

    // A Binding rather than `visible:` — the overlay window assigns visible itself
    // when new text arrives, which would drop a plain binding.
    Binding {
        target: translationOverlay
        property: "visible"
        value: PluginManager ? PluginManager.overlayVisible : false
    }

    // ===== PLUGIN ATTACH CONSENT =====
    // Attaching to a game process can trip anti-cheat, so it is never silent.
    ConfirmDialog {
        id: attachDialog
        parent: Overlay.overlay
        accentColor: Theme.warning
        width: 420
        title: qsTr("Oyuna bağlanma isteği")
        message: qsTr("%1 eklentisi %2 oyununa bağlanmak istiyor.")
                     .arg(window._attachPluginName).arg(window._attachGameName)
                 + "\n\n"
                 + qsTr("Bu işlem çevrimiçi oyunlarda hile koruması tarafından fark edilebilir ve hesabınızın engellenmesine yol açabilir. Yalnızca ne yaptığınızdan eminseniz izin verin.")
        // Escape and the close button count as a refusal.
        onCancelled: window._answerAttach(false, false)
        onClosed: window._showNextAttach()

        glyph: Icons.shieldAlert
        footer: Item {
            implicitHeight: attachButtons.implicitHeight + 40

            ColumnLayout {
                id: attachButtons
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 8

                AppButton {
                    icon: Icons.link
                    Layout.fillWidth: true
                    fillWidth: true
                    style: "filled"
                    tint: Theme.warning
                    text: qsTr("Bağlan")
                    onClicked: { window._answerAttach(true, false); attachDialog.close() }
                }
                AppButton {
                    icon: Icons.shieldCheck
                    Layout.fillWidth: true
                    fillWidth: true
                    style: "gray"
                    text: qsTr("Bu oyun için hep izin ver")
                    onClicked: { window._answerAttach(true, true); attachDialog.close() }
                }
                AppButton {
                    icon: Icons.ban
                    Layout.fillWidth: true
                    fillWidth: true
                    style: "plain"
                    text: qsTr("Reddet")
                    onClicked: { window._answerAttach(false, false); attachDialog.close() }
                }
            }
        }
    }

    // ===== WINDOW RESIZE HANDLES (frameless window) =====
    readonly property int _rm: 5 // resize margin

    // Right edge
    MouseArea {
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: window._rm; cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
    }
    // Bottom edge
    MouseArea {
        anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
        height: window._rm; cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
    }
    // Left edge
    MouseArea {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: window._rm; cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
    }
    // Top edge
    MouseArea {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: window._rm; cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
    }
    // Bottom-right corner
    MouseArea {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: window._rm * 2; height: window._rm * 2; cursorShape: Qt.SizeFDiagCursor
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }
    // Bottom-left corner
    MouseArea {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        width: window._rm * 2; height: window._rm * 2; cursorShape: Qt.SizeBDiagCursor
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }
    // Top-right corner
    MouseArea {
        anchors.right: parent.right; anchors.top: parent.top
        width: window._rm * 2; height: window._rm * 2; cursorShape: Qt.SizeBDiagCursor
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }
    // Top-left corner
    MouseArea {
        anchors.left: parent.left; anchors.top: parent.top
        width: window._rm * 2; height: window._rm * 2; cursorShape: Qt.SizeFDiagCursor
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }
}
