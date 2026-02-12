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
    }

    property int currentNavIndex: 0
    property bool notificationPanelOpen: false

    property QtObject notificationModel: QtObject {
        property int unreadCount: 0
        property ListModel items: ListModel { id: notificationListModel }
        function addNotification(title, message, type) {
            items.insert(0, { "title": title, "message": message, "type": type || "info", "time": Qt.formatTime(new Date(), "HH:mm"), "read": false })
            unreadCount++
        }
        function markAllAsRead() { for (var i = 0; i < items.count; i++) items.setProperty(i, "read", true); unreadCount = 0 }
        function markAsRead(index) { if (index >= 0 && index < items.count && !items.get(index).read) { items.setProperty(index, "read", true); unreadCount = Math.max(0, unreadCount - 1) } }
        function clear() { items.clear(); unreadCount = 0 }
    }

    readonly property int resizeMargin: 6

    // Pending game detail data for lazy-loaded GameDetailScreen
    property var pendingGameDetail: null

    // Pending data for lazy-loaded warning dialogs
    property var pendingCompatData: null
    property var pendingAntiCheatData: null

    // Store normal geometry before maximize so restore works on frameless windows
    property rect normalGeometry: Qt.rect(0, 0, 0, 0)

    // Force quit flag — bypasses minimize-to-tray on close
    property bool forceQuit: false

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
    }

    // ===== WINDOW RESIZE HANDLERS =====

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: window.resizeMargin * 2
        anchors.bottomMargin: window.resizeMargin * 2
        width: window.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: Dimensions.zDialog
        onPressed: window.startSystemResize(Qt.RightEdge)
    }

    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: window.resizeMargin * 2
        anchors.rightMargin: window.resizeMargin * 2
        height: window.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: Dimensions.zDialog
        onPressed: window.startSystemResize(Qt.BottomEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: window.resizeMargin * 2
        anchors.bottomMargin: window.resizeMargin * 2
        width: window.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: Dimensions.zDialog
        onPressed: window.startSystemResize(Qt.LeftEdge)
    }

    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: window.resizeMargin * 2
        anchors.rightMargin: window.resizeMargin * 2
        height: window.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: Dimensions.zDialog
        onPressed: window.startSystemResize(Qt.TopEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: Dimensions.zWindowControls
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: Dimensions.zWindowControls
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: Dimensions.zWindowControls
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: Dimensions.zWindowControls
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
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
            onDonateClicked: Qt.openUrlExternally(Dimensions.donatePageUrl)
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
                    }
                    contentStackContainer.setPageVisible(contentStackContainer.previousIndex, false)
                    contentStackContainer.currentIndex = newIndex
                    contentStackContainer.transitioning = false
                }
            }

            NumberAnimation {
                id: fadeOutAnimation
                property: "opacity"
                from: 1.0
                to: 0
                duration: 180
                easing.type: Easing.OutQuad
            }

            NumberAnimation {
                id: fadeInAnimation
                property: "opacity"
                from: 0
                to: 1.0
                duration: 220
                easing.type: Easing.OutQuad
            }

            HomeScreen {
                id: homeView
                anchors.fill: parent
                visible: contentStackContainer.homeVisible
                animationsEnabled: window.animationsEnabled

                onGameSelected: function(gameId, gameName, installPath, engine) {
                    var gameData = GameService.getGameById(gameId)
                    window.pendingGameDetail = {
                        gameId: gameId,
                        gameName: gameName,
                        engine: engine,
                        imageUrl: (gameData && gameData.headerImageUrl) || "",
                        verified: (gameData && gameData.isVerified) || false,
                        steamAppId: (gameData && gameData.steamAppId) || ""
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
                        gameDetailLoader.item.gameId = d.gameId
                        window.pendingGameDetail = null
                    }
                    contentStackContainer.navigateTo(2)
                }
                onManualFolderRequested: manualFolderDialog.open()
            }

            // Lazy-loaded settings page (Faz 3B)
            Loader {
                id: settingsLoader
                anchors.fill: parent
                active: contentStackContainer.settingsVisible
                visible: contentStackContainer.settingsVisible
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
                sourceComponent: Component {
                    GameDetailScreen {
                        onBackClicked: {
                            contentStackContainer.navigateTo(0)
                            window.currentNavIndex = 0
                        }
                        onTranslateClicked: {
                            var gd = GameService.getGameById(gameId)

                            // Pre-flight 1: Compatibility check
                            var compat = GameService.checkCompatibility(gameId)
                            if (compat && (compat.level === "incompatible" || compat.level === "partial")) {
                                window.pendingCompatData = {
                                    gameName: gameName,
                                    compatibilityLevel: compat.level,
                                    integrityPercent: compat.integrityPercent || 100,
                                    modifiedCount: compat.modifiedCount || 0
                                }
                                compatWarningLoader.active = true
                                return
                            }

                            // Pre-flight 2: Anti-cheat check
                            var antiCheat = GameService.checkAntiCheat(gameId)
                            if (antiCheat && antiCheat.hasAntiCheat && antiCheat.systems.length > 0) {
                                window.pendingAntiCheatData = {
                                    gameName: gameName,
                                    detectedSystems: antiCheat.systems
                                }
                                antiCheatWarningLoader.active = true
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
                                gameId = d.gameId
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
        model: window.notificationModel.items
        z: Dimensions.zNavigation

        onClosed: window.notificationPanelOpen = false
        onNotificationClicked: function(index) {
            window.notificationModel.markAsRead(index)
        }
        onMarkAllRead: window.notificationModel.markAllAsRead()
        onClearAll: {
            window.notificationModel.clear()
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
            var folderName = folderPath.split("/").pop()
            homeView.gameSelected("manual", folderName, folderPath, "Unknown")
        }
    }

    // Convenience function to show notifications from anywhere
    function showNotification(title, message, type) {
        window.notificationModel.addNotification(title, message, type)
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
    }

    // ===== TRANSLATION INSTALL SIGNALS =====
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
            if (success) {
                window.showNotification(
                    qsTr("Yama Kuruldu"),
                    qsTr("Türkçe yama başarıyla kuruldu!"),
                    "success"
                )
            } else {
                window.showNotification(
                    qsTr("Yama Hatası"),
                    message || qsTr("Yama kurulumu başarısız oldu"),
                    "error"
                )
            }
        }
        function onTranslationUninstalled(gameId, success, message) {
            if (success) {
                window.showNotification(
                    qsTr("Yama Kaldırıldı"),
                    qsTr("Türkçe yama başarıyla kaldırıldı"),
                    "info"
                )
            }
        }
    }

    // ===== DROP HANDLER SIGNALS =====
    Connections {
        target: GameService
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
    }

    // ===== COMPATIBILITY WARNING DIALOG (lazy) =====
    Loader {
        id: compatWarningLoader
        active: false
        sourceComponent: Component {
            CompatibilityWarningDialog {
                parent: Overlay.overlay
                onProceedAnyway: {
                    var gd = gameDetailLoader.item
                    if (gd) { var gameData = GameService.getGameById(gd.gameId); gd.startTranslationWorkflow(gameData) }
                }
                onRestoreBackup: contentStackContainer.navigateTo(2)
                onClosed: compatWarningLoader.active = false
                Component.onCompleted: {
                    if (window.pendingCompatData) {
                        gameName = window.pendingCompatData.gameName
                        compatibilityLevel = window.pendingCompatData.compatibilityLevel
                        integrityPercent = window.pendingCompatData.integrityPercent
                        modifiedCount = window.pendingCompatData.modifiedCount
                        window.pendingCompatData = null
                    }
                    open()
                }
            }
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
                    var gd = gameDetailLoader.item
                    if (gd) { var gameData = GameService.getGameById(gd.gameId); gd.startTranslationWorkflow(gameData) }
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

    // ===== DROP ZONE OVERLAY =====
    DropZoneOverlay {
        id: dropZoneOverlay
        anchors.fill: parent
        z: Dimensions.zHeader

        onFilesDropped: function(urls) {
            var type = dropZoneOverlay.classifyDrop(urls)
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
        active: !SettingsManager.onboardingCompleted
        sourceComponent: Component {
            OnboardingWizard {
                z: Dimensions.zOverlay
                onCompleted: {
                    if (GameService.gameCount === 0) {
                        GameService.scanAllLibraries()
                    }
                }
            }
        }
    }

    // ===== TITLE BAR COMPONENT =====
    component TitleBar: Rectangle {
        id: titleBarRoot
        property var windowRef
        property bool translationMode: false
        property bool projectsMode: false
        signal minimizeClicked()
        signal maximizeClicked()
        signal closeClicked()
        signal trayClicked()

        color: Theme.withAlpha(Theme.surface, 0.7)

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.withAlpha(Theme.textPrimary, 0.08)
        }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 160  // Leave space for buttons

            property real lastPressTime: 0

            onPressed: {
                var now = Date.now()
                if (now - lastPressTime < 300) {
                    // Double-click detected
                    titleBarRoot.maximizeClicked()
                    lastPressTime = 0
                } else {
                    lastPressTime = now
                    windowRef.startSystemMove()
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginMS
            anchors.rightMargin: 0
            spacing: Dimensions.spacingMD

            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: Dimensions.radiusStandard
                visible: titleBarRoot.translationMode || titleBarRoot.projectsMode
                color: Theme.turkishRed
                clip: true

                Rectangle {
                    x: 3; y: 4.5
                    width: 9; height: 9
                    radius: 4.5
                    color: Theme.textOnColor
                }
                Rectangle {
                    x: 5; y: 5.3
                    width: 7.5; height: 7.5
                    radius: 3.75
                    color: Theme.turkishRed
                }
                Canvas {
                    x: 9.5; y: 5
                    width: 8; height: 8
                    onPaint: {
                        var ctx = getContext("2d")
                        var cx = 4, cy = 4
                        var R = 3.5
                        var r = R * 0.382
                        ctx.beginPath()
                        for (var i = 0; i < 5; i++) {
                            var oa = i * 72 * Math.PI / 180
                            var ia = (i * 72 + 36) * Math.PI / 180
                            if (i === 0) ctx.moveTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                            else ctx.lineTo(cx - R * Math.cos(oa), cy - R * Math.sin(oa))
                            ctx.lineTo(cx - r * Math.cos(ia), cy - r * Math.sin(ia))
                        }
                        ctx.closePath()
                        ctx.fillStyle = "white"
                        ctx.fill()
                    }
                }
            }

            Image {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                visible: !titleBarRoot.translationMode && !titleBarRoot.projectsMode
                source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                sourceSize: Qt.size(18, 18)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true

                Rectangle {
                    anchors.fill: parent
                    radius: Dimensions.radiusStandard
                    visible: parent.status !== Image.Ready
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.gold }
                        GradientStop { position: 0.5; color: Theme.olive }
                        GradientStop { position: 1.0; color: Theme.pastelBlue }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "M"
                        font.pixelSize: Dimensions.fontCaption
                        font.weight: Font.Bold
                        color: Theme.textOnColor
                    }
                }
            }

            Label {
                text: "MakineAI"
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Row {
                spacing: 0

                WindowButton {
                    icon: "\uE70D"
                    tooltip: qsTr("Minimize to Tray")
                    onClicked: titleBarRoot.trayClicked()
                }

                WindowButton {
                    icon: "\uE921"
                    tooltip: qsTr("Minimize")
                    onClicked: titleBarRoot.minimizeClicked()
                }

                WindowButton {
                    icon: windowRef.visibility === Window.Maximized ? "\uE923" : "\uE922"
                    tooltip: windowRef.visibility === Window.Maximized ? qsTr("Restore") : qsTr("Maximize")
                    onClicked: titleBarRoot.maximizeClicked()
                }

                WindowButton {
                    icon: "\uE8BB"
                    isClose: true
                    tooltip: qsTr("Close")
                    onClicked: titleBarRoot.closeClicked()
                }
            }
        }
    }

    // ===== WINDOW BUTTON COMPONENT (flat, no borders) =====
    component WindowButton: Rectangle {
        property string icon: ""
        property bool isClose: false
        property string tooltip: ""
        signal clicked()

        Accessible.role: Accessible.Button
        Accessible.name: tooltip
        Accessible.onPressAction: clicked()
        activeFocusOnTab: true
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: 46
        height: 32
        color: btnMouse.containsMouse
            ? (isClose ? Theme.closeButtonHover : Theme.glassBorder)
            : "transparent"
        radius: 0

        Behavior on color {
            ColorAnimation { duration: Dimensions.animFast }
        }

        Label {
            anchors.centerIn: parent
            text: icon
            font.pixelSize: Dimensions.fontCaption
            font.family: "Segoe MDL2 Assets"
            color: btnMouse.containsMouse && isClose ? Theme.textOnColor : Theme.textSecondary

            Behavior on color {
                ColorAnimation { duration: Dimensions.animFast }
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor
            onClicked: parent.clicked()
        }

        ToolTip {
            visible: btnMouse.containsMouse && tooltip !== ""
            text: tooltip
            delay: 500
        }
    }

    // ===== NAV BAR COMPONENT =====
    component NavBar: Rectangle {
        id: navBarRoot
        property int currentIndex: 0
        signal homeClicked()
        signal projectsClicked()
        signal translationClicked()
        signal settingsClicked()
        signal donateClicked()

        color: Theme.withAlpha(Theme.surface, 0.7)

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.withAlpha(Theme.textPrimary, 0.08)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Dimensions.marginLG
            anchors.rightMargin: Dimensions.marginLG
            spacing: Dimensions.spacingXL

            Item {
                id: logoContainer
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignVCenter
                scale: logoMouse.containsMouse ? 1.05 : 1.0
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Home")
                activeFocusOnTab: true
                Keys.onReturnPressed: navBarRoot.homeClicked()
                Keys.onSpacePressed: navBarRoot.homeClicked()

                Behavior on scale {
                    NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic }
                }

                AnimatedGradientGlow {
                    anchors.centerIn: parent
                    width: 52; height: 52
                    active: true
                    animationsEnabled: window.animationsEnabled
                    opacity: logoMouse.containsMouse ? 0.9
                           : navBarRoot.currentIndex === 0 ? 0.6
                           : 0.2
                    Behavior on opacity { NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic } }
                }

                Rectangle {
                    id: logoClip
                    anchors.centerIn: parent
                    width: Dimensions.navbarIconSizeLogo
                    height: Dimensions.navbarIconSizeLogo
                    radius: Dimensions.navbarIconSizeLogo * 0.25
                    color: "transparent"
                    clip: true

                    Image {
                        id: logoImage
                        anchors.fill: parent
                        source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        visible: logoImage.status !== Image.Ready
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.logoGold }
                            GradientStop { position: 0.5; color: Theme.logoCoral }
                            GradientStop { position: 1.0; color: Theme.logoGreen }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "M"
                            font.pixelSize: Dimensions.fontLG
                            font.weight: Font.Bold
                            color: Theme.textOnColor
                        }
                    }
                }


                MouseArea {
                    id: logoMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBarRoot.homeClicked()
                }

                ToolTip {
                    visible: logoMouse.containsMouse
                    text: qsTr("Ana Menü")
                    delay: 500
                }
            }

            NavItem {
                text: qsTr("Kütüphane")
                selected: navBarRoot.currentIndex === 2
                onClicked: navBarRoot.translationClicked()
            }

            NavItem {
                text: qsTr("Projelerimiz")
                selected: navBarRoot.currentIndex === 1
                onClicked: navBarRoot.projectsClicked()
            }

            Item { Layout.fillWidth: true }

            Item {
                id: donateItem
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: -8
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Support Us")
                activeFocusOnTab: true
                Keys.onReturnPressed: navBarRoot.donateClicked()
                Keys.onSpacePressed: navBarRoot.donateClicked()

                property bool hovered: donateMouse.containsMouse
                scale: hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

                property real wobble: 0
                SequentialAnimation on wobble {
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                    NumberAnimation { from: 0; to: 6; duration: 1800; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 6; to: -6; duration: 3600; easing.type: Easing.InOutSine }
                    NumberAnimation { from: -6; to: 0; duration: 1800; easing.type: Easing.InOutSine }
                }

                property real colorPhase: 0
                NumberAnimation on colorPhase {
                    from: 0; to: 1
                    duration: 8000
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                }

                Canvas {
                    id: heartCanvas
                    anchors.centerIn: parent
                    width: 20; height: 20
                    rotation: donateItem.wobble
                    opacity: donateItem.hovered ? 1.0 : 0.7
                    Behavior on opacity { NumberAnimation { duration: Dimensions.transitionDuration } }

                    property real phase: donateItem.colorPhase
                    onPhaseChanged: if (donateItem.hovered) requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var angle = phase * Math.PI * 2
                        var cx = width / 2, cy = height / 2
                        var len = 14
                        var x1 = cx + Math.cos(angle) * len
                        var y1 = cy + Math.sin(angle) * len
                        var x2 = cx - Math.cos(angle) * len
                        var y2 = cy - Math.sin(angle) * len

                        var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                        var colors = Theme.brandGradient
                        for (var i = 0; i < colors.length; i++)
                            grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                        ctx.strokeStyle = grad
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        // Heart shape
                        var s = width / 20
                        ctx.beginPath()
                        ctx.moveTo(10 * s, 17 * s)
                        ctx.bezierCurveTo(10 * s, 17 * s, 3 * s, 12 * s, 3 * s, 8 * s)
                        ctx.bezierCurveTo(3 * s, 5 * s, 5.5 * s, 3 * s, 7 * s, 3 * s)
                        ctx.bezierCurveTo(8.5 * s, 3 * s, 9.5 * s, 4 * s, 10 * s, 5.5 * s)
                        ctx.bezierCurveTo(10.5 * s, 4 * s, 11.5 * s, 3 * s, 13 * s, 3 * s)
                        ctx.bezierCurveTo(14.5 * s, 3 * s, 17 * s, 5 * s, 17 * s, 8 * s)
                        ctx.bezierCurveTo(17 * s, 12 * s, 10 * s, 17 * s, 10 * s, 17 * s)
                        ctx.closePath()
                        ctx.stroke()
                    }
                }

                MouseArea {
                    id: donateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBarRoot.donateClicked()
                }

                ToolTip {
                    visible: donateMouse.containsMouse
                    text: qsTr("Destekçi Ol")
                    delay: 400
                }
            }

            Item {
                id: discordItem
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter
                Accessible.role: Accessible.Link
                Accessible.name: "Discord"
                activeFocusOnTab: true
                Keys.onReturnPressed: Qt.openUrlExternally(Dimensions.discordUrl)
                Keys.onSpacePressed: Qt.openUrlExternally(Dimensions.discordUrl)

                property bool hovered: discordMouse.containsMouse
                scale: hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

                property real pulse: 0.7
                SequentialAnimation on pulse {
                    loops: Animation.Infinite
                    running: !discordItem.hovered && window.animationsEnabled
                    NumberAnimation { from: 0.7; to: 1.0; duration: Dimensions.animGradient; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1.0; to: 0.7; duration: Dimensions.animGradient; easing.type: Easing.InOutSine }
                }

                Image {
                    id: discordIcon
                    anchors.centerIn: parent
                    width: 20; height: 20
                    source: "qrc:/qt/qml/MakineAI/resources/icons/discord.svg"
                    sourceSize: Qt.size(20, 20)
                    opacity: discordItem.hovered ? 1.0 : discordItem.pulse
                }

                MouseArea {
                    id: discordMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally(Dimensions.discordUrl)
                }

                ToolTip {
                    visible: discordMouse.containsMouse
                    text: "Discord"
                    delay: 400
                }
            }

            // Separator dot
            Rectangle {
                Layout.preferredWidth: 3; Layout.preferredHeight: 3
                Layout.alignment: Qt.AlignVCenter
                radius: 2
                color: Theme.withAlpha(Theme.textPrimary, 0.15)
            }

            // Settings gear icon
            Item {
                id: settingsItem
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Ayarlar")
                activeFocusOnTab: true
                Keys.onReturnPressed: navBarRoot.settingsClicked()
                Keys.onSpacePressed: navBarRoot.settingsClicked()

                property bool hovered: settingsMouse.containsMouse
                property bool isSelected: navBarRoot.currentIndex === 3
                scale: hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Easing.OutCubic } }

                rotation: hovered ? 30 : 0
                Behavior on rotation { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

                Canvas {
                    id: gearCanvas
                    anchors.centerIn: parent
                    width: 18; height: 18
                    property bool sel: settingsItem.isSelected
                    property bool hov: settingsItem.hovered
                    onSelChanged: requestPaint()
                    onHovChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var c = sel ? Theme.primary : (hov ? Theme.textPrimary : Theme.textMuted)
                        ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, sel ? 1.0 : (hov ? 0.9 : 0.6))
                        ctx.lineWidth = 1.5
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        var cx = 9, cy = 9, outerR = 8, innerR = 6
                        var teeth = 6

                        // Gear outer shape
                        ctx.beginPath()
                        for (var i = 0; i < teeth; i++) {
                            var a1 = (i / teeth) * Math.PI * 2 - Math.PI / 2
                            var a2 = a1 + (0.3 / teeth) * Math.PI * 2
                            var a3 = a1 + (0.5 / teeth) * Math.PI * 2
                            var a4 = a1 + (0.8 / teeth) * Math.PI * 2
                            var a5 = a1 + (1.0 / teeth) * Math.PI * 2

                            if (i === 0) ctx.moveTo(cx + Math.cos(a1) * innerR, cy + Math.sin(a1) * innerR)
                            ctx.lineTo(cx + Math.cos(a2) * outerR, cy + Math.sin(a2) * outerR)
                            ctx.lineTo(cx + Math.cos(a3) * outerR, cy + Math.sin(a3) * outerR)
                            ctx.lineTo(cx + Math.cos(a4) * innerR, cy + Math.sin(a4) * innerR)
                            ctx.lineTo(cx + Math.cos(a5) * innerR, cy + Math.sin(a5) * innerR)
                        }
                        ctx.closePath()
                        ctx.stroke()

                        // Center circle
                        ctx.beginPath()
                        ctx.arc(cx, cy, 3, 0, Math.PI * 2)
                        ctx.stroke()
                    }
                }

                MouseArea {
                    id: settingsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBarRoot.settingsClicked()
                }

                ToolTip {
                    visible: settingsMouse.containsMouse
                    text: qsTr("Ayarlar")
                    delay: 400
                }
            }
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
