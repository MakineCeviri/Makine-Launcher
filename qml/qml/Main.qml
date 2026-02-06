import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineAI 1.0

/**
 * Main.qml - Native Qt MainWindow birebir port
 * Kaynak: ui/src/mainwindow.cpp
 *
 * Yapı:
 * - TitleBar (32px)
 * - NavBar (72px)
 * - ContentStack (QStackedWidget)
 */
ApplicationWindow {
    id: window
    visible: true

    // Minimum pencere boyutu - büyütünce orantılı büyür
    width: minimumWidth
    height: minimumHeight
    minimumWidth: 900
    minimumHeight: 620

    // Ekran ortasında başlat
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2

    title: "MakineAI"
    color: Theme.bgPrimary

    // Native Qt: flags(Qt::FramelessWindowHint | Qt::Window)
    flags: Qt.Window | Qt.FramelessWindowHint

    // State
    property int currentNavIndex: 0
    property bool aiActive: false

    // Resize edge size
    readonly property int resizeMargin: 6

    // ===== WINDOW RESIZE HANDLERS =====
    // Right edge
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: window.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: 100

        property real startX
        property real startWidth

        onPressed: function(mouse) {
            startX = mouse.x
            startWidth = window.width
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = startWidth + (mouse.x - startX)
                if (newWidth >= window.minimumWidth) {
                    window.width = newWidth
                }
            }
        }
    }

    // Bottom edge
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: window.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: 100

        property real startY
        property real startHeight

        onPressed: function(mouse) {
            startY = mouse.y
            startHeight = window.height
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var newHeight = startHeight + (mouse.y - startY)
                if (newHeight >= window.minimumHeight) {
                    window.height = newHeight
                }
            }
        }
    }

    // Left edge
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: window.resizeMargin
        cursorShape: Qt.SizeHorCursor
        z: 100

        property real startX
        property real startWidth
        property real startWinX

        onPressed: function(mouse) {
            startX = mapToGlobal(mouse.x, 0).x
            startWidth = window.width
            startWinX = window.x
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var globalX = mapToGlobal(mouse.x, 0).x
                var deltaX = globalX - startX
                var newWidth = startWidth - deltaX
                if (newWidth >= window.minimumWidth) {
                    window.x = startWinX + deltaX
                    window.width = newWidth
                }
            }
        }
    }

    // Top edge
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: window.resizeMargin
        cursorShape: Qt.SizeVerCursor
        z: 100

        property real startY
        property real startHeight
        property real startWinY

        onPressed: function(mouse) {
            startY = mapToGlobal(0, mouse.y).y
            startHeight = window.height
            startWinY = window.y
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var globalY = mapToGlobal(0, mouse.y).y
                var deltaY = globalY - startY
                var newHeight = startHeight - deltaY
                if (newHeight >= window.minimumHeight) {
                    window.y = startWinY + deltaY
                    window.height = newHeight
                }
            }
        }
    }

    // Bottom-right corner
    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: 101

        property point startPos
        property real startWidth
        property real startHeight

        onPressed: function(mouse) {
            startPos = Qt.point(mouse.x, mouse.y)
            startWidth = window.width
            startHeight = window.height
        }
        onPositionChanged: function(mouse) {
            if (pressed) {
                var newWidth = startWidth + (mouse.x - startPos.x)
                var newHeight = startHeight + (mouse.y - startPos.y)
                if (newWidth >= window.minimumWidth) window.width = newWidth
                if (newHeight >= window.minimumHeight) window.height = newHeight
            }
        }
    }

    // GPU Optimization: Disable animations when window is not visible/active
    // Flutter'daki idle mode optimizasyonu - minimize edilince GPU = 0
    readonly property bool animationsEnabled: window.visible &&
                                              window.active &&
                                              window.visibility !== Window.Minimized &&
                                              window.visibility !== Window.Hidden

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
            translationMode: window.aiActive  // Show Turkish flag when AI is active

            onMinimizeClicked: window.showMinimized()
            onMaximizeClicked: {
                if (window.visibility === Window.Maximized) {
                    window.showNormal()
                } else {
                    window.showMaximized()
                }
            }
            onCloseClicked: window.close()
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
                homeView.showHomePage()  // HomeScreen'i ana sayfaya döndür
            }
            onProjectsClicked: {
                window.currentNavIndex = 1
                homeView.showProjectsPage()
                contentStackContainer.navigateTo(0)
            }
            onSettingsClicked: {
                window.currentNavIndex = 2
                contentStackContainer.navigateTo(1)
            }
            onAiToggleClicked: function(active) {
                window.aiActive = active
                homeView.setAIActive(active)
                if (active) {
                    contentStackContainer.navigateTo(0)
                    window.currentNavIndex = 0
                }
            }
            onDonateClicked: Qt.openUrlExternally("https://makineai.com/destekci-ol")
        }

        // ===== CONTENT STACK - Simple crossfade transitions =====
        Item {
            id: contentStackContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Track page state for transitions
            property int currentIndex: 0
            property int previousIndex: 0
            property bool transitioning: false

            // Visibility flags for each page (managed by navigation, not bindings)
            property bool homeVisible: true
            property bool settingsVisible: false
            property bool gameDetailVisible: false
            property bool workflowVisible: false

            // Simple crossfade page transition
            function navigateTo(index) {
                if (index === currentIndex || transitioning) return
                transitioning = true
                previousIndex = currentIndex

                var outgoingPage = getPage(previousIndex)
                var incomingPage = getPage(index)

                if (outgoingPage && incomingPage) {
                    // Make incoming page visible but transparent
                    setPageVisible(index, true)
                    incomingPage.opacity = 0

                    // Run crossfade
                    fadeOutAnimation.target = outgoingPage
                    fadeInAnimation.target = incomingPage

                    fadeOutAnimation.start()
                    fadeInAnimation.start()

                    // Update index after animation completes
                    pageChangeTimer.newIndex = index
                    pageChangeTimer.start()
                }
            }

            function setPageVisible(index, visible) {
                switch(index) {
                    case 0: homeVisible = visible; break
                    case 1: settingsVisible = visible; break
                    case 2: gameDetailVisible = visible; break
                    case 3: workflowVisible = visible; break
                }
            }

            function getPage(index) {
                switch(index) {
                    case 0: return homeView
                    case 1: return settingsView
                    case 2: return gameDetailView
                    case 3: return workflowView
                    default: return null
                }
            }

            // Timer to update current index after animation
            Timer {
                id: pageChangeTimer
                interval: 200
                property int newIndex: 0
                onTriggered: {
                    // Reset old page state
                    var oldPage = contentStackContainer.getPage(contentStackContainer.previousIndex)
                    if (oldPage) {
                        oldPage.opacity = 1.0  // Reset opacity for next time
                    }
                    // Hide old page
                    contentStackContainer.setPageVisible(contentStackContainer.previousIndex, false)
                    contentStackContainer.currentIndex = newIndex
                    contentStackContainer.transitioning = false
                }
            }

            // Simple fade out animation
            NumberAnimation {
                id: fadeOutAnimation
                property: "opacity"
                from: 1.0
                to: 0
                duration: 180
                easing.type: Easing.OutQuad
            }

            // Simple fade in animation
            NumberAnimation {
                id: fadeInAnimation
                property: "opacity"
                from: 0
                to: 1.0
                duration: 220
                easing.type: Easing.OutQuad
            }

            // Index 0: HomeView
            HomeScreen {
                id: homeView
                anchors.fill: parent
                visible: contentStackContainer.homeVisible
                animationsEnabled: window.animationsEnabled  // GPU optimization
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    // Navigate to game detail
                    gameDetailView.gameId = gameId
                    gameDetailView.gameName = gameName
                    gameDetailView.engine = engine
                    // Load additional game data from service
                    var gameData = GameService.getGameById(gameId)
                    if (gameData) {
                        gameDetailView.imageUrl = gameData.headerImageUrl || ""
                        gameDetailView.steamAppId = gameData.steamAppId || ""
                        gameDetailView.verified = gameData.isVerified || false
                    }
                    contentStackContainer.navigateTo(2)
                }
            }

            // Index 1: SettingsView
            SettingsScreen {
                id: settingsView
                anchors.fill: parent
                visible: contentStackContainer.settingsVisible
                onBack: {
                    window.currentNavIndex = 0
                    contentStackContainer.navigateTo(0)
                    navBar.currentIndex = 0
                }
            }

            // Index 2: GameDetailScreen
            GameDetailScreen {
                id: gameDetailView
                anchors.fill: parent
                visible: contentStackContainer.gameDetailVisible
                onBackClicked: {
                    contentStackContainer.navigateTo(0)
                    window.currentNavIndex = 0
                }
                onTranslateClicked: {
                    // Navigate to workflow screen with game data
                    var gameData = GameService.getGameById(gameDetailView.gameId)
                    workflowView.gameId = gameDetailView.gameId
                    workflowView.gameName = gameDetailView.gameName
                    workflowView.gamePath = gameData ? gameData.installPath : ""
                    workflowView.gameEngine = gameDetailView.engine
                    workflowView.headerImageUrl = gameDetailView.imageUrl
                    contentStackContainer.navigateTo(3)

                    // Start translation process
                    TranslationService.startTranslation(
                        workflowView.gameId,
                        workflowView.gameName,
                        workflowView.gamePath
                    )
                }
            }

            // Index 3: TranslationWorkflowScreen
            TranslationWorkflowScreen {
                id: workflowView
                anchors.fill: parent
                visible: contentStackContainer.workflowVisible
                onBackClicked: {
                    contentStackContainer.navigateTo(2)
                }
                onCompleted: function(gameId) {
                    // Return to home after completion
                    contentStackContainer.navigateTo(0)
                    window.currentNavIndex = 0
                }
                onCancelled: function(gameId) {
                    // Return to game detail on cancel
                    contentStackContainer.navigateTo(2)
                }
            }
        }
    }

    // ===== TITLE BAR COMPONENT - Native Qt birebir =====
    component TitleBar: Rectangle {
        id: titleBarRoot
        property var windowRef
        property bool translationMode: false  // Native Qt: setTranslationMode
        signal minimizeClicked()
        signal maximizeClicked()
        signal closeClicked()

        color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.7)

        // Bottom border - Native Qt: QPen(QColor(255, 255, 255, 20), 1)
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        // Drag area
        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 120  // Leave space for buttons
            property point dragPos

            onPressed: function(mouse) {
                dragPos = Qt.point(mouse.x, mouse.y)
            }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    var delta = Qt.point(mouse.x - dragPos.x, mouse.y - dragPos.y)
                    windowRef.x += delta.x
                    windowRef.y += delta.y
                }
            }
            onDoubleClicked: titleBarRoot.maximizeClicked()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 0  // Window buttons flush to right edge
            spacing: 8

            // Native Qt: Turkish flag when translation is active (18x18)
            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 2
                visible: titleBarRoot.translationMode
                color: "#E30A17"  // Turkish flag red

                // White crescent (simplified)
                Rectangle {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: -2
                    width: 10
                    height: 10
                    radius: 5
                    color: "white"

                    Rectangle {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 2
                        width: 8
                        height: 8
                        radius: 4
                        color: "#E30A17"
                    }
                }
            }

            // Logo (18px) - Flutter: logo.png
            Image {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                visible: !titleBarRoot.translationMode
                source: "qrc:/qt/qml/MakineAI/resources/images/logo.png"
                sourceSize: Qt.size(18, 18)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true

                // Fallback gradient if image fails to load
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
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: "white"
                    }
                }
            }

            // Title - Native Qt: fontSize 12, weight 500
            Label {
                text: "MakineAI"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            // Window buttons - Flutter style with Segoe MDL2 icons
            Row {
                spacing: 0

                // Minimize button - Segoe MDL2: ChromeMinimize
                WindowButton {
                    icon: "\uE921"
                    onClicked: titleBarRoot.minimizeClicked()
                }

                // Maximize button - Segoe MDL2: ChromeMaximize/ChromeRestore
                WindowButton {
                    icon: windowRef.visibility === Window.Maximized ? "\uE923" : "\uE922"
                    onClicked: titleBarRoot.maximizeClicked()
                }

                // Close button - Segoe MDL2: ChromeClose
                WindowButton {
                    icon: "\uE8BB"
                    isClose: true
                    onClicked: titleBarRoot.closeClicked()
                }
            }
        }
    }

    // ===== WINDOW BUTTON COMPONENT - Flutter style (flat, no borders) =====
    component WindowButton: Rectangle {
        property string icon: ""
        property bool isClose: false
        signal clicked()

        width: 46   // Flutter: width 46
        height: 32  // Flutter: height 32
        color: btnMouse.containsMouse
            ? (isClose ? Theme.closeButtonHover : Qt.rgba(1, 1, 1, 0.1))
            : "transparent"
        radius: 0  // Flutter: flat, no rounded corners

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Label {
            anchors.centerIn: parent
            text: icon
            font.pixelSize: 10  // Segoe MDL2 icons are smaller
            font.family: "Segoe MDL2 Assets"
            color: btnMouse.containsMouse && isClose ? "white" : Theme.textSecondary

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.ArrowCursor  // Windows style
            onClicked: parent.clicked()
        }
    }

    // ===== NAV BAR COMPONENT =====
    component NavBar: Rectangle {
        id: navBarRoot
        property int currentIndex: 0
        signal homeClicked()
        signal projectsClicked()
        signal settingsClicked()
        signal donateClicked()
        signal aiToggleClicked(bool active)

        property bool aiActive: false

        // Native Qt: Colors::surface 0.7 alpha
        color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.7)

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

            // Logo (38px) - Flutter LogoHomeButton
            Item {
                id: logoContainer
                Layout.preferredWidth: 50  // Container for circular hover
                Layout.preferredHeight: 50
                scale: logoMouse.containsMouse ? 1.05 : 1.0

                Behavior on scale {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                // Circular black hover background
                Rectangle {
                    id: hoverCircle
                    anchors.centerIn: parent
                    width: 46
                    height: 46
                    radius: 23  // Fully circular
                    color: Qt.rgba(0, 0, 0, logoMouse.containsMouse ? 0.7 : 0)

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                // Logo image with rounded corners
                Rectangle {
                    id: logoClip
                    anchors.centerIn: parent
                    width: Dimensions.navbarIconSizeLogo  // 38
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

                    // Fallback gradient if logo fails
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        visible: logoImage.status !== Image.Ready
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#E8C547" }
                            GradientStop { position: 0.5; color: "#E8A090" }
                            GradientStop { position: 1.0; color: "#90D090" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "M"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            color: "white"
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
                    text: "Ana Menü"
                    delay: 500
                }
            }

            // AI Toggle - Türkçe Yama butonu (NavItem ile aynı stil)
            Item {
                id: aiToggleItem
                Layout.fillHeight: true  // Navbar yüksekliğini doldur
                Layout.preferredWidth: aiToggleLabel.width + 24

                // Gradient animasyon döngüsü (3000ms)
                property real animPhase: 0.0
                NumberAnimation on animPhase {
                    from: 0.0
                    to: 1.0
                    duration: 3000
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                }

                property real smoothValue: 1.0 - Math.abs(animPhase * 2.0 - 1.0)

                property color gradientColor1: Theme.lerpColor(Theme.gold, Theme.olive, smoothValue)
                property color gradientColor2: Theme.lerpColor(Theme.olive, Theme.gold, smoothValue)

                // Underline genişliği: hover = 70, default = 0
                property real underlineWidth: aiToggleMouse.containsMouse ? 70 : 0
                Behavior on underlineWidth {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                // Gradient metin
                Text {
                    id: aiToggleLabel
                    anchors.centerIn: parent
                    text: navBarRoot.aiActive ? "Kapat" : "Türkçe Yama"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: aiToggleItem.gradientColor1
                }

                // Underline - navbar alt çizgisiyle aynı konumda
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: aiToggleItem.underlineWidth
                    height: 2
                    radius: 1
                    visible: width > 0

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: aiToggleItem.gradientColor1 }
                        GradientStop { position: 1.0; color: aiToggleItem.gradientColor2 }
                    }
                }

                MouseArea {
                    id: aiToggleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        navBarRoot.aiActive = !navBarRoot.aiActive
                        navBarRoot.aiToggleClicked(navBarRoot.aiActive)
                    }
                }
            }

            // Nav items
            NavItem {
                text: "Projelerimiz"
                selected: navBarRoot.currentIndex === 1
                onClicked: navBarRoot.projectsClicked()
            }

            NavItem {
                text: "Ayarlar"
                selected: navBarRoot.currentIndex === 2
                onClicked: navBarRoot.settingsClicked()
            }

            Item { Layout.fillWidth: true }

            // Donate button - Native Qt: animated gradient gold-pink birebir port
            Item {
                id: donateItem
                Layout.preferredWidth: 130
                Layout.preferredHeight: 36

                // Native Qt: 2000ms gradient animation loop
                // GPU optimization: only run when window is active
                property real animValue: 0.0
                NumberAnimation on animValue {
                    from: 0.0
                    to: 1.0
                    duration: 2000
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                }

                // Native Qt: smoothValue = (1 - ((animValue * 2 - 1).abs())).clamp(0.0, 1.0)
                property real smoothValue: 1.0 - Math.abs(animValue * 2.0 - 1.0)

                // Native Qt: color1/color2 gold <-> olive lerp
                property color color1: Theme.lerpColor(Theme.gold, Theme.olive, smoothValue)
                property color color2: Theme.lerpColor(Theme.olive, Theme.gold, smoothValue)

                // Native Qt: glowIntensity = 0.25 + (smoothValue * 0.25), hover -> 0.6
                property real glowIntensity: donateMouse.containsMouse ? 0.6 : (0.25 + smoothValue * 0.25)

                // Native Qt: heart pulse - 800ms, scale 1.0 -> 1.15 -> 1.0
                // GPU optimization: only run when window is active
                property real heartScale: 1.0
                SequentialAnimation on heartScale {
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                    NumberAnimation { to: 1.15; duration: 400; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 400; easing.type: Easing.InOutSine }
                }

                // Glow/shadow - Native Qt: blurRadius hover ? 20 : (10 + smoothValue * 6)
                Rectangle {
                    anchors.fill: donateButton
                    anchors.margins: donateMouse.containsMouse ? -6 : -(3 + donateItem.smoothValue * 2)
                    anchors.topMargin: anchors.margins + 4
                    radius: Dimensions.radiusXS + 2
                    color: Theme.withAlpha(donateItem.color1, donateItem.glowIntensity)
                    z: -1
                }

                // Main button
                Rectangle {
                    id: donateButton
                    anchors.fill: parent
                    radius: Dimensions.radiusXS

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: donateItem.color1 }
                        GradientStop { position: 1.0; color: donateItem.color2 }
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 8

                        // Native Qt: Heart with pulse animation
                        Item {
                            width: 18
                            height: 18
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "\u2665"
                                font.pixelSize: 14
                                color: "white"
                                scale: donateItem.heartScale
                                transformOrigin: Item.Center
                            }
                        }

                        Label {
                            text: "Destekçi Ol"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                MouseArea {
                    id: donateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navBarRoot.donateClicked()
                }
            }
        }
    }

    // ===== PERFORMANCE MONITOR (F3 to toggle) =====
    property bool showPerformanceMonitor: false

    PerformanceMonitor {
        id: perfMonitor
        visible: window.showPerformanceMonitor
        z: 9999  // Always on top
    }

    // F3 shortcut to toggle performance monitor
    Shortcut {
        sequence: "F3"
        onActivated: window.showPerformanceMonitor = !window.showPerformanceMonitor
    }

    // ===== SCREENSHOT FEATURE (F12 to capture) =====
    property int screenshotCounter: 1

    function takeScreenshot() {
        var num = screenshotCounter.toString()
        while (num.length < 3) num = "0" + num

        // Save to cedra folder (known writable location)
        var filename = "C:/cedra/screenshot_" + num + ".png"

        // Grab mainContent (the ColumnLayout with all UI)
        mainContent.grabToImage(function(result) {
            if (result) {
                result.saveToFile(filename)
                screenshotCounter++
            }
        })
    }

    // F12 shortcut to take screenshot (application-wide)
    Shortcut {
        sequence: "F12"
        context: Qt.ApplicationShortcut
        onActivated: window.takeScreenshot()
    }

    // Alternative: Ctrl+Shift+S for screenshot
    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        onActivated: window.takeScreenshot()
    }

    // ===== NAV ITEM COMPONENT - Navigasyon butonu =====
    component NavItem: Item {
        id: navItemRoot
        property string text: ""
        property bool selected: false
        signal clicked()

        Layout.preferredWidth: navItemLabel.width + 24
        Layout.fillHeight: true  // Navbar yüksekliğini doldur, underline alt çizgiyle hizalansın

        // Underline genişliği: selected = 24, hover = 16, default = 0
        property real underlineWidth: selected ? 24 : (navItemMouse.containsMouse ? 16 : 0)
        Behavior on underlineWidth {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        Label {
            id: navItemLabel
            anchors.centerIn: parent
            text: navItemRoot.text
            font.pixelSize: 13
            font.weight: navItemRoot.selected ? Font.DemiBold : Font.Medium
            color: navItemRoot.selected ? Theme.primary
                 : navItemMouse.containsMouse ? Theme.textPrimary
                 : Theme.textSecondary

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // Underline - navbar alt çizgisiyle aynı konumda
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: navItemRoot.underlineWidth
            height: 2
            radius: 1
            color: navItemRoot.selected
                ? Theme.primary
                : Theme.withAlpha(Theme.textPrimary, 0.4)
            visible: width > 0
        }

        MouseArea {
            id: navItemMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: navItemRoot.clicked()
        }
    }
}
