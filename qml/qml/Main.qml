import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * Main.qml - Application main window with title bar, navigation, and content stack
 */
ApplicationWindow {
    id: window
    visible: true

    minimumWidth: Dimensions.minWindowWidth
    minimumHeight: Dimensions.minWindowHeight

    // Initial size — match screen proportions, centered (replaces C++ MoveWindow)
    width: Math.min(Math.max(1100, Dimensions.minWindowWidth), Screen.width * 0.85)
    height: Math.round(width / (Dimensions.minWindowWidth / Dimensions.minWindowHeight))
    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)

    title: "Makine \u00C7eviri - Makine Launcher"
    color: (window._authReady && !window._onboardingActive) ? Theme.bgPrimary : "#0d1117"

    flags: Qt.Window | Qt.FramelessWindowHint

    // Aspect ratio lock handled natively via WM_SIZING in C++ (main.cpp)
    // — zero recursive bindings, zero frame drops during resize
    // Auth gate — blocks all content until authenticated
    readonly property bool _authReady: typeof AuthService !== "undefined"
                                       && AuthService.isAuthenticated

    Component.onCompleted: {
        // Auth token check moved to C++ splash phase (main.cpp) —
        // state is already resolved by the time QML loads.
        if (typeof SettingsManager !== "undefined")
            window._onboardingActive = !SettingsManager.onboardingCompleted
    }

    property int currentNavIndex: 0
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
    TrayPopup {
        id: trayPopup
        onShowRequested: {
            window.show(); window.raise(); window.requestActivate()
        }
        onCheckUpdatesRequested: UpdateService.check()
        onSettingsRequested: {
            window.show(); window.raise(); window.requestActivate()
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
        onQuitRequested: {
            window.forceQuit = true
            if (!window.visible) window.show()
            window.close()
        }
    }

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
        }
        function onUpdateCheckRequested() {
            UpdateService.check()
        }
    }

    // ===== TRANSLATION DOWNLOADER: route signals to InstallFlowController =====
    Connections {
        target: TranslationDownloader
        function onPackageReady(appId, dirName) {
            installFlow.onDownloadReady(appId)
        }
        function onDownloadError(appId, error) {
            installFlow.onDownloadFailed(appId, error)
        }
        function onDownloadCancelled(appId) {
            installFlow.onDownloadFailed(appId, "")
        }
    }

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
                ProcessScanner.startWatching(windowActive ? 10000 : 60000)
        }
    }

    // ===== KEYBOARD SHORTCUTS =====
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    Shortcut {
        sequences: [StandardKey.Back]
        enabled: !window._onboardingActive && contentStackContainer.currentIndex !== 0
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: !window._onboardingActive && contentStackContainer.currentIndex !== 0
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
        }
    }
    Shortcut {
        sequence: "Ctrl+,"
        enabled: !window._onboardingActive
        onActivated: {
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        enabled: !window._onboardingActive
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
            homeView.showHomePage()
        }
    }
    Shortcut {
        sequence: "Ctrl+1"
        enabled: !window._onboardingActive
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
            homeView.showHomePage()
        }
    }
    Shortcut {
        sequence: "Ctrl+2"
        enabled: !window._onboardingActive
        onActivated: {
            window.currentNavIndex = 1
            contentStackContainer.navigateTo(0)
            homeView.showLibraryPage()
        }
    }
    Shortcut {
        sequence: "Ctrl+R"
        enabled: !window._onboardingActive
        onActivated: GameService.checkForUpdates()
    }

    // GPU Optimization: Disable animations only when minimized or hidden.
    // Animations keep running when window loses focus (e.g. user switches app
    // during install) so shimmer/glow still shows the app is alive.
    readonly property bool animationsEnabled: SettingsManager.enableAnimations &&
                                              window.visible &&
                                              window.visibility !== Window.Minimized &&
                                              window.visibility !== Window.Hidden

    // Visibility-aware resource management
    readonly property bool windowActive: window.visible &&
                                         window.visibility !== Window.Minimized &&
                                         window.visibility !== Window.Hidden

    onWindowActiveChanged: {
        if (window._onboardingActive) return
        if (!SettingsManager.autoDetectGames) return
        if (windowActive) {
            ProcessScanner.startWatching(10000)
        } else {
            ProcessScanner.startWatching(60000)
        }
    }

    // Premium accent-aware ambient background
    AppBackground {
        anchors.fill: parent
        z: -1
        visible: window._authReady
    }

    ColumnLayout {
        id: mainContent
        anchors.fill: parent
        spacing: 0
        visible: !window._onboardingActive && window._authReady

        // ===== TITLE BAR (32px) =====
        TitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.titlebarHeight
            windowRef: window
            libraryMode: window.currentNavIndex === 1

            onMinimizeClicked: window.showMinimized()
            onCloseClicked: window.close()
            onTrayClicked: window.minimizeToTray()
        }

        // ===== INTEGRITY WARNING BANNER =====
        Rectangle {
            id: integrityBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 32 : 0
            visible: IntegrityService.status === "failed"
            color: Theme.warningBg

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginMD
                anchors.rightMargin: Dimensions.marginMD
                spacing: Dimensions.spacingMD

                Image {
                    source: "qrc:/qt/qml/MakineLauncher/resources/icons/shield-check.svg"
                    sourceSize: Qt.size(14, 14)
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    asynchronous: true
                    opacity: 0.8
                }

                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Bütünlük doğrulaması başarısız — bu çalıştırılabilir dosya değiştirilmiş olabilir.")
                    font.pixelSize: Dimensions.fontXS
                    font.weight: Font.Medium
                    color: Theme.warning
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Item {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Uyarıyı kapat")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: integrityBanner.visible = false
                    Keys.onSpacePressed: integrityBanner.visible = false

                    Label {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: "\uE8BB"
                        font.pixelSize: Dimensions.fontCaption
                        font.family: "Segoe MDL2 Assets"
                        color: Theme.textSecondary
                        opacity: dismissMouse.containsMouse ? 1.0 : 0.6
                    }

                    MouseArea {
                        id: dismissMouse
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: integrityBanner.visible = false
                    }

                    StyledToolTip {
                        visible: dismissMouse.containsMouse
                        text: qsTr("Kapat")
                        delay: 500
                    }
                }
            }

            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("Güvenlik uyarısı: bütünlük doğrulaması başarısız")
        }


        // ===== CONNECTION BANNER =====
        Rectangle {
            id: connectionBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 36 : 0
            visible: ManifestSync.isOffline
            color: Theme.error08

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginMD
                anchors.rightMargin: Dimensions.marginMD
                spacing: Dimensions.spacingMD

                Label {
                    textFormat: Text.PlainText
                    text: ""
                    font.pixelSize: 14
                    font.family: "Segoe MDL2 Assets"
                    color: Theme.error
                    opacity: 0.8
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    textFormat: Text.PlainText
                    text: qsTr("İnternet bağlantısı bulunamadı — bağlanmaya çalışılıyor...")
                    font.pixelSize: Dimensions.fontXS
                    font.weight: Font.Medium
                    color: Theme.error
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Label {
                    textFormat: Text.PlainText
                    text: qsTr("Tekrar deneniyor...")
                    font.pixelSize: Dimensions.fontXS
                    color: Theme.textMuted
                    visible: ManifestSync.isSyncing
                }
            }

            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("Bağlantı uyarısı: internet bağlantısı yok")
        }

        // ===== NAV BAR =====
        NavBar {
            id: navBar
            z: 1
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.navbarHeight
            currentIndex: window.currentNavIndex
            showBottomLine: false
            animationsEnabled: window.animationsEnabled

            onHomeClicked: {
                window.currentNavIndex = 0
                contentStackContainer.navigateTo(0)
                homeView.showHomePage()
            }
            onSettingsClicked: {
                window.currentNavIndex = 2
                contentStackContainer.navigateTo(1)
            }
            onLibraryClicked: {
                window.currentNavIndex = 1
                contentStackContainer.navigateTo(0)
                homeView.showLibraryPage()
            }
        }

        // ===== CONTENT STACK - Simple crossfade transitions =====
        Rectangle {
            id: contentStackContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Dimensions.radiusSection
            color: Theme.bgPrimary
            clip: true

            // Apple-style subtle edge border around content area
            GradientBorder {
                cornerRadius: Dimensions.radiusSection
                topColor: Qt.rgba(1, 1, 1, 0.08)
                midColor: Qt.rgba(1, 1, 1, 0.04)
                bottomColor: Qt.rgba(1, 1, 1, 0.015)
            }

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
                    incomingPage.y = 16

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
                    from: 0; to: -12
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
                    from: 16; to: 0
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
                    detailVM.loadGame(gameDataResolver.resolve(gameId, gameName, installPath, engine, false))
                    detailVM.fromLibrary = (homeView.currentPage === 1)
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
                }
                onManualFolderRequested: manualFolderDialog.open()

                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    detailVM.loadGame(gameDataResolver.resolve(gameId, gameName, installPath, engine, true))
                    detailVM.fromLibrary = (homeView.currentPage === 1)
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
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
                        onBackClicked: {
                            window.currentNavIndex = window.previousNavIndex
                            if (window.previousNavIndex === 2) {
                                contentStackContainer.navigateTo(1)
                            } else {
                                contentStackContainer.navigateTo(0)
                                if (window.previousNavIndex === 1)
                                    homeView.showLibraryPage()
                                else
                                    homeView.showHomePage()
                            }
                        }
                    }
                }
            }

        }
    }

    // ===== MANUAL GAME FOLDER DIALOG =====
    FolderDialog {
        id: manualFolderDialog
        title: qsTr("Oyun Klasörünü Seç")
        onAccepted: {
            var folderPath = selectedFolder.toString().replace("file:///", "")
            GameService.addManualGame(folderPath)
        }
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
        anchors.fill: parent
        active: !window._authReady || window._onboardingActive
        z: Dimensions.zOverlay
        sourceComponent: Component {
            OnboardingWizard {
                onWizardFinished: {
                    // Only scan + persist for first-launch (not returning users)
                    if (window._onboardingActive) {
                        if (typeof GameService !== "undefined" && GameService.gameCount === 0) {
                            GameService.scanAllLibraries()
                        }
                        if (typeof SettingsManager !== "undefined") {
                            SettingsManager.onboardingCompleted = true
                        }
                    }
                    window._onboardingActive = false
                }
            }
        }
    }

    // ===== GLOBAL LOADING INDICATOR =====
    Rectangle {
        id: globalLoadingBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        z: Dimensions.zOverlay + 10
        color: "transparent"
        visible: GameService.isScanning

        Rectangle {
            id: loadingSlider
            height: parent.height
            width: parent.width * 0.3
            radius: 1
            color: Theme.primary

            SequentialAnimation on x {
                id: loadingShimmerAnim
                running: globalLoadingBar.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: -globalLoadingBar.width * 0.3
                    to: globalLoadingBar.width
                    duration: Dimensions.animLoadingCycle
                    easing.type: Easing.InOutQuad
                }
                onRunningChanged: {
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.registerAnimation("loadingBarShimmer", running)
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Theme.primary
                opacity: 0.3
                anchors.leftMargin: -parent.width * 0.2
                width: parent.width * 1.4
            }
        }

        Behavior on visible {
            enabled: false
        }
    }

    // ===== PERFORMANCE MONITOR (F3 to toggle, dev builds only) =====
    property bool showPerformanceMonitor: false

    PerformanceMonitor {
        id: perfMonitor
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
                gameName + " k\u00fct\u00fcphaneye eklendi!",
                "K\u00fct\u00fcphaneden T\u00fcrk\u00e7e yama y\u00fckleyebilirsiniz.")
        }
        function onProcessNotSupported(processName) {
            gameToast.show("", processName,
                processName + " desteklenmiyor",
                "Bu oyun henüz Türkçe'ye çevrilmedi.")
        }
    }

    // ===== OCR REGION SELECTOR (dev builds only) =====
    RegionSelector {
        id: regionSelector
        onRegionSelected: function(rx, ry, rw, rh) {
            if (OcrController) OcrController.setRegion(rx, ry, rw, rh)
        }
    }

    // ===== OCR TRANSLATION OVERLAY (dev builds only) =====
    TranslationOverlay {
        id: translationOverlay
        visible: OcrController ? OcrController.overlayVisible : false
        sourceRegion: OcrController ? OcrController.captureRegion : Qt.rect(0,0,0,0)
    }

    // ===== OCR SIGNAL WIRING (dev builds only) =====
    Connections {
        target: OcrController || null
        function onTranslationReady(text) {
            translationOverlay.translatedText = text
        }
        function onOcrTextReady(text) {
            translationOverlay.ocrText = text
        }
        function onRegionSelectingChanged() {
            if (OcrController && OcrController.regionSelecting)
                regionSelector.show()
            else
                regionSelector.hide()
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
