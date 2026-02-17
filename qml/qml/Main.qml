import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import MakineAI 1.0

/**
 * Main.qml - Application main window with title bar, navigation, and content stack
 */
ApplicationWindow {
    id: window
    visible: true

    width: minimumWidth
    height: minimumHeight
    minimumWidth: Dimensions.minWindowWidth
    minimumHeight: Dimensions.minWindowHeight

    title: "MakineAI"
    color: Theme.bgPrimary

    flags: Qt.Window | Qt.FramelessWindowHint

    // Restore saved window position/size or center on screen
    Component.onCompleted: {
        if (typeof SettingsManager !== "undefined") {
            var geo = SettingsManager.windowGeometry()
            if (geo.width > 0 && geo.height > 0) {
                window.x = geo.x
                window.y = geo.y
                window.width = Math.max(geo.width, minimumWidth)
                window.height = Math.max(geo.height, minimumHeight)
                if (geo.maximized) window.showMaximized()
            } else {
                window.x = (Screen.width - width) / 2
                window.y = (Screen.height - height) / 2
            }
            window._onboardingActive = !SettingsManager.onboardingCompleted
        } else {
            window.x = (Screen.width - width) / 2
            window.y = (Screen.height - height) / 2
        }
    }

    property int currentNavIndex: 0
    property int previousNavIndex: 0  // Remember nav index before game detail
    property bool notificationPanelOpen: false

    readonly property int resizeMargin: 6

    // Pending game detail data for lazy-loaded GameDetailScreen
    property var pendingGameDetail: null

    // Pending data for lazy-loaded warning dialogs
    property var pendingAntiCheatData: null
    property var pendingVariantData: null
    property var pendingInstallNotes: null

    // Store normal geometry before maximize so restore works on frameless windows
    property rect normalGeometry: Qt.rect(0, 0, 0, 0)

    // Force quit flag — bypasses minimize-to-tray on close
    property bool forceQuit: false

    // Onboarding: local flag so Loader doesn't depend on SettingsManager binding
    property bool _onboardingActive: true

    Component.onDestruction: pageChangeTimer.stop()

    onClosing: function(close) {
        // Save window geometry before closing
        var isMax = (window.visibility === Window.Maximized)
        if (!isMax) {
            SettingsManager.saveWindowGeometry(window.x, window.y, window.width, window.height, false)
        } else {
            SettingsManager.saveWindowGeometry(window.x, window.y, window.width, window.height, true)
        }

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
            window.currentNavIndex = 3
            contentStackContainer.navigateTo(1)
        }
        function onQuitRequested() {
            window.forceQuit = true
            Qt.quit()
        }
        function onUpdateCheckRequested() {
            UpdateChecker.checkForUpdates()
        }
    }

    // Auto-update download/install notifications
    Connections {
        target: UpdateChecker
        function onDownloadCompleted() {
            window.showNotification(
                qsTr("İndirme Tamamlandı"),
                qsTr("Güncelleme kurulmaya hazır"),
                "success"
            )
        }
        function onInstallStarted() {
            window.showNotification(
                qsTr("Kurulum Başlatılıyor"),
                qsTr("Uygulama yeniden başlatılacak..."),
                "update"
            )
        }
        function onDownloadErrorChanged() {
            if (UpdateChecker.downloadError) {
                window.showNotification(
                    qsTr("İndirme Hatası"),
                    UpdateChecker.downloadError,
                    "error"
                )
            }
        }
    }

    // ===== WINDOW RESIZE HANDLERS =====
    WindowResizeHandles {
        anchors.fill: parent
        windowRef: window
        resizeMargin: window.resizeMargin
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
            window.currentNavIndex = 3
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
            window.currentNavIndex = 2
            contentStackContainer.navigateTo(0)
            homeView.showTranslationPage()
        }
    }
    Shortcut {
        sequence: "Ctrl+3"
        onActivated: {
            window.currentNavIndex = 1
            contentStackContainer.navigateTo(0)
            homeView.showProjectsPage()
        }
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            if (window.notificationPanelOpen) {
                notificationPanel.close()
            } else {
                window.notificationPanelOpen = true
                notificationPanel.x = window.width - notificationPanel.width - 80
                notificationPanel.y = Dimensions.titlebarHeight + Dimensions.navbarHeight + 4
                notificationPanel.open()
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+R"
        onActivated: GameService.scanAllLibraries()
    }

    // GPU Optimization: Disable animations when window is not visible/active or user disabled them
    readonly property bool animationsEnabled: SettingsManager.enableAnimations &&
                                              window.visible &&
                                              window.active &&
                                              window.visibility !== Window.Minimized &&
                                              window.visibility !== Window.Hidden

    // Visibility-aware resource management (Faz 3A)
    readonly property bool windowActive: window.visible &&
                                         window.visibility !== Window.Minimized &&
                                         window.visibility !== Window.Hidden

    onWindowActiveChanged: {
        if (windowActive) {
            // Window restored: use normal scan interval
            ProcessScanner.startWatching(3000)
        } else {
            // Window minimized/hidden: reduce scan frequency
            ProcessScanner.startWatching(30000)
        }
    }

    ColumnLayout {
        id: mainContent
        anchors.fill: parent
        spacing: 0

        // ===== TITLE BAR (32px) - Native Qt TitleBar =====
        TitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.titlebarHeight
            windowRef: window
            translationMode: window.currentNavIndex === 2
            projectsMode: window.currentNavIndex === 1

            onMinimizeClicked: window.showMinimized()
            onMaximizeClicked: {
                if (window.visibility === Window.Maximized) {
                    window.showNormal()
                    // Restore saved geometry for frameless windows
                    if (window.normalGeometry.width > 0) {
                        window.x = window.normalGeometry.x
                        window.y = window.normalGeometry.y
                        window.width = window.normalGeometry.width
                        window.height = window.normalGeometry.height
                    }
                } else {
                    // Save current geometry before maximizing
                    window.normalGeometry = Qt.rect(window.x, window.y, window.width, window.height)
                    window.showMaximized()
                }
            }
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
                    opacity: 0.8
                }

                Label {
                    text: qsTr("Binary integrity check failed — this executable may have been modified.")
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
                    Accessible.name: qsTr("Dismiss warning")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: integrityBanner.visible = false
                    Keys.onSpacePressed: integrityBanner.visible = false

                    Label {
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
            Accessible.name: qsTr("Security warning: binary integrity check failed")
        }

        // ===== NAV BAR (72px) - Native Qt NavBar =====
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
            onProjectsClicked: {
                window.currentNavIndex = 1
                homeView.showProjectsPage()
                contentStackContainer.navigateTo(0)
            }
            onSettingsClicked: {
                window.currentNavIndex = 3
                contentStackContainer.navigateTo(1)
            }
            onTranslationClicked: {
                window.currentNavIndex = 2
                contentStackContainer.navigateTo(0)
                homeView.showTranslationPage()
            }
            onNotificationClicked: {
                if (window.notificationPanelOpen) {
                    notificationPanel.close()
                } else {
                    window.notificationPanelOpen = true
                    notificationPanel.x = window.width - notificationPanel.width - 80
                    notificationPanel.y = Dimensions.titlebarHeight + Dimensions.navbarHeight + 4
                    notificationPanel.open()
                }
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

            function navigateTo(index) {
                if (index === currentIndex || transitioning) return
                transitioning = true
                previousIndex = currentIndex

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
                }
            }

            // Outgoing page: fade out + subtle slide up
            ParallelAnimation {
                id: fadeOutAnimation
                property var target
                NumberAnimation {
                    target: fadeOutAnimation.target
                    property: "opacity"
                    from: 1.0; to: 0
                    duration: 160
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: fadeOutAnimation.target
                    property: "y"
                    from: 0; to: -12
                    duration: 160
                    easing.type: Easing.InCubic
                }
            }

            // Incoming page: fade in + slide up from below
            ParallelAnimation {
                id: fadeInAnimation
                property var target
                NumberAnimation {
                    target: fadeInAnimation.target
                    property: "opacity"
                    from: 0; to: 1.0
                    duration: 240
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: fadeInAnimation.target
                    property: "y"
                    from: 16; to: 0
                    duration: 240
                    easing.type: Easing.OutCubic
                }
            }

            // Subtle grid pattern background (40px cells)
            Canvas {
                id: gridBackground
                anchors.fill: parent
                z: -1

                onWidthChanged: repaintTimer.restart()
                onHeightChanged: repaintTimer.restart()
                Component.onCompleted: requestPaint()

                Timer {
                    id: repaintTimer
                    interval: 100
                    onTriggered: gridBackground.requestPaint()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.03)
                    ctx.lineWidth = 1

                    var cell = 40

                    // Vertical lines
                    ctx.beginPath()
                    for (var x = cell; x < width; x += cell) {
                        ctx.moveTo(Math.floor(x) + 0.5, 0)
                        ctx.lineTo(Math.floor(x) + 0.5, height)
                    }
                    ctx.stroke()

                    // Horizontal lines
                    ctx.beginPath()
                    for (var y = cell; y < height; y += cell) {
                        ctx.moveTo(0, Math.floor(y) + 0.5)
                        ctx.lineTo(width, Math.floor(y) + 0.5)
                    }
                    ctx.stroke()
                }
            }

            HomeScreen {
                id: homeView
                anchors.fill: parent
                visible: contentStackContainer.homeVisible
                animationsEnabled: window.animationsEnabled

                onGameSelected: function(gameId, gameName, installPath, engine) {
                    var gameData = GameService.getGameById(gameId)
                    var isManual = (gameData && gameData.source === "manual") || gameId.startsWith("manual_")
                    var isInstalled = (gameData && gameData.isInstalled) || installPath !== ""
                    var resolvedSteamAppId = (gameData && gameData.steamAppId) || ""
                    // For catalog-only games, gameId IS the steamAppId
                    if (resolvedSteamAppId === "" && /^\d+$/.test(gameId))
                        resolvedSteamAppId = gameId
                    var rawImageUrl = (gameData && gameData.headerImageUrl) || ""
                    if (rawImageUrl === "" && resolvedSteamAppId !== "")
                        rawImageUrl = "https://cdn.akamai.steamstatic.com/steam/apps/" + resolvedSteamAppId + "/library_600x900_2x.jpg"
                    var resolvedImageUrl = ImageCache.resolve(resolvedSteamAppId || gameId, rawImageUrl)
                    var hasTranslation = (gameData && gameData.hasTranslation) || false
                    var pkgInstalled = (gameData && gameData.packageInstalled) || false

                    window.pendingGameDetail = {
                        gameId: gameId,
                        gameName: gameName,
                        engine: engine,
                        imageUrl: resolvedImageUrl,
                        verified: (gameData && gameData.isVerified) || false,
                        steamAppId: resolvedSteamAppId,
                        hasTranslation: hasTranslation,
                        isManualGame: isManual,
                        isGameInstalled: isInstalled,
                        packageInstalled: pkgInstalled,
                        autoInstall: isInstalled && hasTranslation && !pkgInstalled
                    }
                    // If loader already active, apply immediately
                    if (gameDetailLoader.item) {
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
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
                }
                onManualFolderRequested: manualFolderDialog.open()

                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    var gameData = GameService.getGameById(gameId)
                    var isManual = (gameData && gameData.source === "manual") || gameId.startsWith("manual_")
                    var isInstalled = (gameData && gameData.isInstalled) || installPath !== ""
                    var resolvedSteamAppId = (gameData && gameData.steamAppId) || ""
                    if (resolvedSteamAppId === "" && /^\d+$/.test(gameId))
                        resolvedSteamAppId = gameId
                    var rawImageUrl2 = (gameData && gameData.headerImageUrl) || ""
                    if (rawImageUrl2 === "" && resolvedSteamAppId !== "")
                        rawImageUrl2 = "https://cdn.akamai.steamstatic.com/steam/apps/" + resolvedSteamAppId + "/library_600x900_2x.jpg"
                    var resolvedImageUrl = ImageCache.resolve(resolvedSteamAppId || gameId, rawImageUrl2)
                    var hasTranslation = (gameData && gameData.hasTranslation) || false
                    var pkgInstalled = (gameData && gameData.packageInstalled) || false

                    window.pendingGameDetail = {
                        gameId: gameId,
                        gameName: gameName,
                        engine: engine,
                        imageUrl: resolvedImageUrl,
                        verified: (gameData && gameData.isVerified) || false,
                        steamAppId: resolvedSteamAppId,
                        hasTranslation: hasTranslation,
                        isManualGame: isManual,
                        isGameInstalled: isInstalled,
                        packageInstalled: pkgInstalled,
                        autoInstall: true
                    }
                    if (gameDetailLoader.item) {
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
                    window.previousNavIndex = window.currentNavIndex
                    contentStackContainer.navigateTo(2)
                }
            }

            // Lazy-loaded settings page (Faz 3B)
            Loader {
                id: settingsLoader
                anchors.fill: parent
                active: contentStackContainer.settingsVisible
                visible: contentStackContainer.settingsVisible
                asynchronous: true
                sourceComponent: Component {
                    SettingsScreen {
                        onBack: {
                            window.currentNavIndex = 0
                            contentStackContainer.navigateTo(0)
                        }
                    }
                }
            }
            // Lazy-loaded game detail page (only created when navigated to)
            Loader {
                id: gameDetailLoader
                anchors.fill: parent
                active: contentStackContainer.gameDetailVisible
                visible: contentStackContainer.gameDetailVisible
                asynchronous: true
                sourceComponent: Component {
                    GameDetailScreen {
                        onBackClicked: {
                            contentStackContainer.navigateTo(0)
                            window.currentNavIndex = window.previousNavIndex
                        }
                        onTranslateClicked: {
                            // Pre-flight: Anti-cheat check
                            var antiCheat = GameService.checkAntiCheat(gameId)
                            if (antiCheat && antiCheat.hasAntiCheat && antiCheat.systems.length > 0) {
                                window.pendingAntiCheatData = {
                                    gameName: gameName,
                                    detectedSystems: antiCheat.systems
                                }
                                antiCheatWarningLoader.active = true
                                return
                            }

                            // Pre-flight: Install notes check
                            var notes = GameService.getInstallNotes(gameId)
                            if (notes && notes.length > 0) {
                                window.pendingInstallNotes = {
                                    gameId: gameId,
                                    notes: notes
                                }
                                installNotesLoader.active = true
                                return
                            }

                            // Pre-flight: Variant check
                            var variants = GameService.getVariants(gameId)
                            if (variants && variants.length > 0) {
                                window.pendingVariantData = {
                                    gameId: gameId,
                                    variants: variants,
                                    variantType: GameService.getVariantType(gameId)
                                }
                                variantSelectionLoader.active = true
                                return
                            }

                            // All checks passed — install translation package
                            GameService.installTranslation(gameId)
                        }
                        Component.onCompleted: {
                            // Apply pending game data when loader creates the screen
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

    // ===== NOTIFICATION PANEL =====
    NotificationPanel {
        id: notificationPanel
        parent: Overlay.overlay
        model: NotificationService
        z: Dimensions.zNavigation

        onClosed: window.notificationPanelOpen = false
        onNotificationClicked: function(index) {
            NotificationService.markAsRead(index)
        }
        onMarkAllRead: NotificationService.markAllAsRead()
        onClearAll: {
            NotificationService.clear()
            notificationPanel.close()
        }
    }

    // ===== NOTIFICATION TOAST =====
    NotificationToast {
        id: notificationToast
        parent: Overlay.overlay
        z: Dimensions.zToast

        onClicked: {
            notificationToast.dismiss()
            if (!window.notificationPanelOpen) {
                window.notificationPanelOpen = true
                notificationPanel.x = window.width - notificationPanel.width - 80
                notificationPanel.y = Dimensions.titlebarHeight + Dimensions.navbarHeight + 4
                notificationPanel.open()
            }
        }
    }

    // ===== MANUAL GAME FOLDER DIALOG =====
    FolderDialog {
        id: manualFolderDialog
        title: qsTr("Oyun Klasörünü Seç")
        onAccepted: {
            var folderPath = selectedFolder.toString().replace("file:///", "")
            // Add to library (detects engine, matches against translation catalog)
            var newGameId = GameService.addManualGame(folderPath)
            if (newGameId && newGameId !== "") {
                var gameData = GameService.getGameById(newGameId)
                var gameName = (gameData && gameData.name) || folderPath.split("/").pop()
                var engine = (gameData && gameData.engine) || "Unknown"
                homeView.gameSelected(newGameId, gameName, folderPath, engine)
            }
        }
    }

    // Convenience function to show notifications from anywhere
    function showNotification(title, message, type) {
        NotificationService.addNotification(title, message, type)
        notificationToast.show(title, message, type, 5000)
    }

    // Wire up existing update notification from HomeScreen
    Connections {
        target: homeView
        function onUpdateAvailableChanged() {
            if (homeView.updateAvailable) {
                window.showNotification(
                    qsTr("Güncelleme Mevcut"),
                    qsTr("Yeni sürüm mevcut: %1").arg(homeView.latestVersion),
                    "update"
                )
            }
        }
        function onSettingsRequested() {
            window.currentNavIndex = 3
            contentStackContainer.navigateTo(1)
        }
    }

    // ===== GAME SERVICE SIGNALS =====
    Connections {
        target: GameService
        function onTranslationInstallStarted(gameId) {
            window.showNotification(
                qsTr("Yama Kuruluyor"),
                qsTr("Türkçe yama indiriliyor ve kuruluyor..."),
                "translation"
            )
        }
        function onTranslationInstallCompleted(gameId, success, message) {
            window.showNotification(
                success ? qsTr("Çeviri Kuruldu") : qsTr("Çeviri Hatası"),
                message,
                success ? "success" : "error"
            )
        }
        function onTranslationUninstalled(gameId, success, message) {
            window.showNotification(
                success ? qsTr("Çeviri Kaldırıldı") : qsTr("Kaldırma Hatası"),
                message,
                success ? "info" : "error"
            )
        }
        function onRuntimeInstallFinished(gameId, success, error) {
            if (success) {
                window.showNotification(
                    qsTr("Runtime Kuruldu"),
                    qsTr("Runtime başarıyla kuruldu/kaldırıldı"),
                    "success"
                )
            } else {
                window.showNotification(
                    qsTr("Runtime Hatası"),
                    error || qsTr("Runtime işlemi başarısız oldu"),
                    "error"
                )
            }
        }
        function onLocalPackageReady(packageName, gameName, filePath) {
            window.showNotification(
                qsTr("Paket Hazır: %1").arg(packageName),
                qsTr("Oyun: %1").arg(gameName),
                "translation"
            )
        }
        function onLocalPackageError(filePath, error) {
            window.showNotification(
                qsTr("Paket Hatası"),
                error,
                "error"
            )
        }
        function onFolderDropped(path, isGame) {
            if (isGame) {
                window.showNotification(
                    qsTr("Oyun Eklendi"),
                    path,
                    "info"
                )
            }
        }
        function onGameUpdateDetected(gameId, gameName, summary) {
            window.showNotification(
                qsTr("Oyun Güncellendi: %1").arg(gameName),
                summary,
                "warning"
            )
        }
        function onScanCompleted(count) {
            window.showNotification(
                qsTr("Tarama Tamamlandı"),
                qsTr("%1 desteklenen oyun bulundu").arg(count),
                "info"
            )
        }
    }

    // ===== BACKUP MANAGER SIGNALS =====
    Connections {
        target: BackupManager
        function onBackupCreated(gameId) {
            window.showNotification(
                qsTr("Yedek Oluşturuldu"),
                qsTr("Orijinal dosyalar başarıyla yedeklendi"),
                "success"
            )
        }
        function onBackupRestored(gameId) {
            window.showNotification(
                qsTr("Yedek Geri Yüklendi"),
                qsTr("Orijinal dosyalar başarıyla geri yüklendi"),
                "success"
            )
        }
        function onBackupDeleted(backupId) {
            window.showNotification(
                qsTr("Yedek Silindi"),
                qsTr("Yedek dosyaları başarıyla silindi"),
                "info"
            )
        }
        function onBackupError(error) {
            window.showNotification(
                qsTr("Yedekleme Hatası"),
                error,
                "error"
            )
        }
    }

    // ===== SETTINGS MANAGER SIGNALS =====
    Connections {
        target: SettingsManager
        function onCacheClearCompleted(success, message) {
            window.showNotification(
                success ? qsTr("Önbellek Temizlendi") : qsTr("Önbellek Hatası"),
                message,
                success ? "success" : "error"
            )
        }
        function onSettingsResetCompleted() {
            window.showNotification(
                qsTr("Ayarlar Sıfırlandı"),
                qsTr("Tüm ayarlar varsayılan değerlere döndürüldü"),
                "info"
            )
        }
        function onGameUpdateMonitoringChanged() {
            GameService.setUpdateMonitoringEnabled(SettingsManager.gameUpdateMonitoring)
        }
    }

    // (GameService signals merged into single Connections block above)

    // ===== ANTI-CHEAT WARNING DIALOG (lazy) =====
    Loader {
        id: antiCheatWarningLoader
        active: false
        sourceComponent: Component {
            AntiCheatWarningDialog {
                parent: Overlay.overlay
                onContinueAnyway: {
                    var gd = gameDetailLoader.item
                    if (gd) {
                        // Check for install notes before variants
                        var notes = GameService.getInstallNotes(gd.gameId)
                        if (notes && notes.length > 0) {
                            window.pendingInstallNotes = {
                                gameId: gd.gameId,
                                notes: notes
                            }
                            installNotesLoader.active = true
                            return
                        }
                        // Check for variants before installing
                        var variants = GameService.getVariants(gd.gameId)
                        if (variants && variants.length > 0) {
                            window.pendingVariantData = {
                                gameId: gd.gameId,
                                variants: variants,
                                variantType: GameService.getVariantType(gd.gameId)
                            }
                            variantSelectionLoader.active = true
                        } else {
                            GameService.installTranslation(gd.gameId)
                        }
                    }
                }
                onClosed: antiCheatWarningLoader.active = false
                Component.onCompleted: {
                    if (window.pendingAntiCheatData) {
                        gameName = window.pendingAntiCheatData.gameName
                        detectedSystems = window.pendingAntiCheatData.detectedSystems
                        window.pendingAntiCheatData = null
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
                onAccepted: {
                    var data = window.pendingInstallNotes
                    window.pendingInstallNotes = null
                    if (data) {
                        // Continue to variant check
                        var variants = GameService.getVariants(data.gameId)
                        if (variants && variants.length > 0) {
                            window.pendingVariantData = {
                                gameId: data.gameId,
                                variants: variants,
                                variantType: GameService.getVariantType(data.gameId)
                            }
                            variantSelectionLoader.active = true
                        } else {
                            GameService.installTranslation(data.gameId)
                        }
                    }
                }
                onCancelled: { window.pendingInstallNotes = null }
                onClosed: installNotesLoader.active = false
                Component.onCompleted: {
                    if (window.pendingInstallNotes) {
                        notes = window.pendingInstallNotes.notes
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
                    if (window.pendingVariantData) {
                        GameService.installTranslation(window.pendingVariantData.gameId, variant)
                        window.pendingVariantData = null
                    }
                }
                onCancelled: { window.pendingVariantData = null }
                onClosed: variantSelectionLoader.active = false
                Component.onCompleted: {
                    if (window.pendingVariantData) {
                        variants = window.pendingVariantData.variants
                        variantType = window.pendingVariantData.variantType || "version"
                    }
                    open()
                }
            }
        }
    }

    // ===== DROP ZONE OVERLAY =====
    DropZoneOverlay {
        id: dropZoneOverlay
        anchors.fill: parent
        z: Dimensions.zHeader

        onFilesDropped: function(urls) {
            var type = GameService.classifyDroppedUrls(urls)
            DebugHelper.log("DropZone", "Files dropped: " + urls.length + " (" + type + ")")

            if (type === "package") {
                window.showNotification(
                    qsTr("Çeviri Paketi Algılandı"),
                    qsTr("Paket yükleme hazırlanıyor..."),
                    "translation"
                )
            } else if (type === "archive") {
                window.showNotification(
                    qsTr("Arşiv Algılandı"),
                    qsTr("Arşiv içeriği analiz ediliyor..."),
                    "info"
                )
            } else if (type === "folder") {
                window.showNotification(
                    qsTr("Klasör Algılandı"),
                    qsTr("Oyun klasörü algılanıyor..."),
                    "info"
                )
            }

            // Delegate to GameService for actual file processing
            GameService.handleDroppedFiles(urls)
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
                    // Dismiss wizard immediately via local property (always works)
                    window._onboardingActive = false
                    // Scan if no games found yet
                    if (typeof GameService !== "undefined" && GameService.gameCount === 0) {
                        GameService.scanAllLibraries()
                    }
                    // Persist so wizard won't show on next launch
                    if (typeof SettingsManager !== "undefined") {
                        SettingsManager.onboardingCompleted = true
                    }
                }
            }
        }
    }

    // ===== GLOBAL LOADING INDICATOR =====
    // Subtle top-bar progress indicator for long-running operations
    Rectangle {
        id: globalLoadingBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        z: Dimensions.zOverlay + 10
        color: "transparent"
        visible: GameService.isScanning

        // Indeterminate sliding bar
        Rectangle {
            id: loadingSlider
            height: parent.height
            width: parent.width * 0.3
            radius: 1
            color: Theme.primary

            SequentialAnimation on x {
                running: globalLoadingBar.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: -globalLoadingBar.width * 0.3
                    to: globalLoadingBar.width
                    duration: 1500
                    easing.type: Easing.InOutQuad
                }
            }

            // Soft glow trail
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
            enabled: false // instant show
        }
    }

    // ===== PERFORMANCE MONITOR (F3 to toggle) =====
    property bool showPerformanceMonitor: false

    PerformanceMonitor {
        id: perfMonitor
        visible: window.showPerformanceMonitor
        z: Dimensions.zDebug
    }

    Shortcut {
        sequence: "F3"
        onActivated: window.showPerformanceMonitor = !window.showPerformanceMonitor
    }

}
