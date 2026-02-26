import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * Main.qml - Application main window with title bar, navigation, and content stack
 */
ApplicationWindow {
    id: window
    visible: true

    minimumWidth: Dimensions.minWindowWidth
    minimumHeight: Dimensions.minWindowHeight

    title: "Makine \u00C7eviri - MakineAI"
    color: Theme.bgPrimary

    flags: Qt.Window | Qt.FramelessWindowHint

    // Window sizing + positioning handled in C++ (main.cpp) via Win32 API
    Component.onCompleted: {
        if (typeof SettingsManager !== "undefined")
            window._onboardingActive = !SettingsManager.onboardingCompleted
    }

    property int currentNavIndex: 0
    property int previousNavIndex: 0  // Remember nav index before game detail

    // Pending game detail data for lazy-loaded GameDetailScreen
    property var pendingGameDetail: null

    // Force quit flag — bypasses minimize-to-tray on close
    property bool forceQuit: false

    // Onboarding: start false to avoid creating/destroying OnboardingWizard
    // for returning users. Component.onCompleted flips to true if needed.
    property bool _onboardingActive: false

    // Preload heavy pages in background after startup settles.
    // Guard: never preload during a page transition (sync load blocks main thread).
    // On collision, retry in 200ms (not full interval) so preload fires right after transition ends.
    property bool _settingsPreloaded: false
    property bool _gameDetailPreloaded: false
    Timer {
        id: _settingsPreloadTimer
        interval: 3000; running: true
        onTriggered: {
            if (contentStackContainer.transitioning) {
                interval = 200; restart(); return
            }
            window._settingsPreloaded = true
            _gameDetailPreloadTimer.start()
        }
    }
    Timer {
        id: _gameDetailPreloadTimer
        interval: 2000
        onTriggered: {
            if (contentStackContainer.transitioning) {
                interval = 200; restart(); return
            }
            window._gameDetailPreloaded = true
        }
    }

    Component.onDestruction: pageChangeTimer.stop()

    onClosing: function(close) {
        // Window always repositions on launch — no need to save geometry

        if (SettingsManager.minimizeToTray && !window.forceQuit) {
            close.accepted = false
            window.minimizeToTray()
        } else {
            Qt.quit()
        }
    }

    function minimizeToTray() {
        window.hide()
    }

    // ===== CONTROLLERS =====
    GameDataResolver { id: gameDataResolver }

    InstallFlowController {
        id: installFlow
        gameDetailLoader: gameDetailLoader
        onShowAntiCheatWarning: antiCheatWarningLoader.active = true
        onShowInstallNotes: installNotesLoader.active = true
        onShowInstallOptions: installOptionsLoader.active = true
        onShowVariantSelection: variantSelectionLoader.active = true
    }

    // ===== SYSTEM TRAY =====
    Connections {
        target: SystemTrayManager
        function onShowWindowRequested() {
            window.show()
            window.raise()
            window.requestActivate()
        }
        function onSettingsRequested() {
            window.show()
            window.raise()
            window.requestActivate()
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
        function onQuitRequested() {
            window.forceQuit = true
            if (!window.visible) window.show()
            window.close()
        }
        function onUpdateCheckRequested() {
            UpdateChecker.checkForUpdates()
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
        function onTranslationImpactDetected(gameId, gameName, impact) {
            // Queue affected games; dialog opens when loader completes
            if (!window._pendingImpacts) window._pendingImpacts = []
            window._pendingImpacts.push({gameId: gameId, gameName: gameName, impact: impact})
            updateAlertLoader.active = true
            if (updateAlertLoader.item) {
                updateAlertLoader.item.addAffectedGame(gameId, gameName, impact)
                if (!updateAlertLoader.item.visible)
                    updateAlertLoader.item.open()
            }
        }
    }

    // ===== CORE BRIDGE: package detail enrichment =====
    Connections {
        target: CoreBridge
        function onPackageDetailEnriched(appId) {
            installFlow.onPackageDetailEnriched(appId)
        }
    }

    // ===== KEYBOARD SHORTCUTS =====
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    Shortcut {
        sequences: [StandardKey.Back]
        enabled: contentStackContainer.currentIndex !== 0
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: contentStackContainer.currentIndex !== 0
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
        }
    }
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: {
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(1)
        }
    }
    Shortcut {
        sequence: "Ctrl+H"
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
            homeView.showHomePage()
        }
    }
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: {
            window.currentNavIndex = 0
            contentStackContainer.navigateTo(0)
            homeView.showHomePage()
        }
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: {
            window.currentNavIndex = 1
            contentStackContainer.navigateTo(0)
            homeView.showLibraryPage()
        }
    }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: GameService.scanAllLibraries()
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
        if (windowActive) {
            ProcessScanner.startWatching(10000)
        } else {
            ProcessScanner.startWatching(60000)
        }
    }

    ColumnLayout {
        id: mainContent
        anchors.fill: parent
        spacing: 0

        // ===== TITLE BAR (32px) =====
        TitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.titlebarHeight
            windowRef: window
            libraryMode: window.currentNavIndex === 1

            onMinimizeClicked: window.showMinimized()
            onCloseClicked: {
                if (SettingsManager.minimizeToTray) {
                    window.minimizeToTray()
                } else {
                    Qt.quit()
                }
            }
            onTrayClicked: window.minimizeToTray()
        }

        // ===== INTEGRITY WARNING BANNER =====
        Rectangle {
            id: integrityBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 32 : 0
            visible: IntegrityService.status === "failed"
            color: Theme.warningBg
            clip: true

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginMD
                anchors.rightMargin: Dimensions.marginMD
                spacing: Dimensions.spacingMD

                Image {
                    source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
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

                    ToolTip {
                        visible: dismissMouse.containsMouse
                        text: qsTr("Kapat")
                        delay: 500
                    }
                }
            }

            Accessible.role: Accessible.AlertMessage
            Accessible.name: qsTr("Güvenlik uyarısı: bütünlük doğrulaması başarısız")
        }

        // ===== NAV BAR =====
        NavBar {
            id: navBar
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.navbarHeight
            currentIndex: window.currentNavIndex
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
        Item {
            id: contentStackContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                    var d = gameDataResolver.resolve(gameId, gameName, installPath, engine, false)
                    window.pendingGameDetail = d
                    _applyPendingGameDetail()
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
                }
                onManualFolderRequested: manualFolderDialog.open()

                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    var d = gameDataResolver.resolve(gameId, gameName, installPath, engine, true)
                    window.pendingGameDetail = d
                    _applyPendingGameDetail()
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
                }
            }

            // Lazy-loaded settings page
            Loader {
                id: settingsLoader
                anchors.fill: parent
                active: contentStackContainer.settingsVisible || window._settingsPreloaded
                visible: contentStackContainer.settingsVisible
                asynchronous: false
                onLoaded: {
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.markLoaderReady("SettingsScreen")
                }
                sourceComponent: Component {
                    SettingsScreen {
                        onBack: {
                            window.currentNavIndex = 0
                            contentStackContainer.navigateTo(0)
                        }
                    }
                }
            }

            // Lazy-loaded game detail page
            Loader {
                id: gameDetailLoader
                anchors.fill: parent
                active: contentStackContainer.gameDetailVisible || window._gameDetailPreloaded
                visible: contentStackContainer.gameDetailVisible
                asynchronous: false
                onLoaded: {
                    if (typeof SceneProfiler !== "undefined")
                        SceneProfiler.markLoaderReady("GameDetailScreen")
                }
                sourceComponent: Component {
                    GameDetailScreen {
                        onBackClicked: {
                            contentStackContainer.navigateTo(0)
                            window.currentNavIndex = window.previousNavIndex
                        }
                        onTranslateClicked: {
                            installFlow.startInstallFlow(gameId, gameName)
                        }
                        Component.onCompleted: {
                            if (window.pendingGameDetail) {
                                var d = window.pendingGameDetail
                                resetDetails()
                                gameName = d.gameName
                                engine = d.engine
                                imageUrl = d.imageUrl
                                verified = d.verified
                                steamAppId = d.steamAppId
                                hasTranslation = d.hasTranslation || false
                                gameId = d.gameId
                                isManualGame = d.isManualGame || false
                                isGameInstalled = d.isGameInstalled || false
                                packageInstalled = d.packageInstalled || false
                                autoInstall = d.autoInstall || false
                                window.pendingGameDetail = null
                            }
                        }
                    }
                }
            }

        }
    }

    // Helper: apply pending game data to already-loaded detail screen
    function _applyPendingGameDetail() {
        if (gameDetailLoader.item && window.pendingGameDetail) {
            var d = window.pendingGameDetail
            gameDetailLoader.item.resetDetails()
            gameDetailLoader.item.gameName = d.gameName
            gameDetailLoader.item.engine = d.engine
            gameDetailLoader.item.imageUrl = d.imageUrl
            gameDetailLoader.item.verified = d.verified
            gameDetailLoader.item.steamAppId = d.steamAppId
            gameDetailLoader.item.hasTranslation = d.hasTranslation
            gameDetailLoader.item.gameId = d.gameId
            gameDetailLoader.item.isManualGame = d.isManualGame
            gameDetailLoader.item.isGameInstalled = d.isGameInstalled
            gameDetailLoader.item.packageInstalled = d.packageInstalled
            gameDetailLoader.item.autoInstall = d.autoInstall
            window.pendingGameDetail = null
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

    // ===== INSTALL NOTES DIALOG (lazy) =====
    Loader {
        id: installNotesLoader
        active: false
        sourceComponent: Component {
            InstallNotesDialog {
                parent: Overlay.overlay
                onAccepted: installFlow.onInstallNotesAccepted()
                onCancelled: installFlow.onInstallNotesCancelled()
                onClosed: {
                    installNotesLoader.active = false
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogClose("InstallNotes")
                }
                Component.onCompleted: {
                    if (typeof SceneProfiler !== "undefined") SceneProfiler.markDialogOpen("InstallNotes")
                    if (installFlow.pendingInstallNotes) {
                        notes = installFlow.pendingInstallNotes.notes
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

    // ===== ONBOARDING WIZARD OVERLAY (lazy: unloaded after completion) =====
    Loader {
        id: onboardingLoader
        anchors.fill: parent
        active: window._onboardingActive
        sourceComponent: Component {
            OnboardingWizard {
                z: Dimensions.zOverlay
                onWizardFinished: {
                    window._onboardingActive = false
                    if (typeof GameService !== "undefined" && GameService.gameCount === 0) {
                        GameService.scanAllLibraries()
                    }
                    if (typeof SettingsManager !== "undefined") {
                        SettingsManager.onboardingCompleted = true
                    }
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

}
