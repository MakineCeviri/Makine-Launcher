import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import MakineAI 1.0

/**
 * Main.qml - Application main window with title bar, navigation, and content stack
 */
ApplicationWindow {
    id: window
    visible: true

    width: minimumWidth
    height: minimumHeight
    minimumWidth: 900
    minimumHeight: 620

    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2

    title: "MakineAI"
    color: Theme.bgPrimary

    flags: Qt.Window | Qt.FramelessWindowHint

    property int currentNavIndex: 0
    property bool aiActive: false

    readonly property int resizeMargin: 6

    onClosing: Qt.quit()

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
        function onQuitRequested() {
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
        z: 100
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
        z: 100
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
        z: 100
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
        z: 100
        onPressed: window.startSystemResize(Qt.TopEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: 101
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: 101
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeBDiagCursor
        z: 101
        onPressed: window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: window.resizeMargin * 2
        height: window.resizeMargin * 2
        cursorShape: Qt.SizeFDiagCursor
        z: 101
        onPressed: window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
    }

    // GPU Optimization: Disable animations when window is not visible/active
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
            translationMode: window.aiActive

            onMinimizeClicked: window.showMinimized()
            onMaximizeClicked: {
                if (window.visibility === Window.Maximized) {
                    window.showNormal()
                } else {
                    window.showMaximized()
                }
            }
            onCloseClicked: Qt.quit()
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
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Image {
                    source: "qrc:/qt/qml/MakineAI/resources/icons/shield-check.svg"
                    sourceSize: Qt.size(14, 14)
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    opacity: 0.8
                }

                Label {
                    text: qsTr("Binary integrity check failed — this executable may have been modified.")
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: Theme.warning
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Label {
                    text: "\uE8BB"
                    font.pixelSize: 10
                    font.family: "Segoe MDL2 Assets"
                    color: Theme.textSecondary
                    opacity: dismissMouse.containsMouse ? 1.0 : 0.6

                    MouseArea {
                        id: dismissMouse
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: integrityBanner.visible = false
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
            property bool workflowVisible: false

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

            Timer {
                id: pageChangeTimer
                interval: 200
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
                    gameDetailView.resetDetails()
                    gameDetailView.gameName = gameName
                    gameDetailView.engine = engine
                    var gameData = GameService.getGameById(gameId)
                    if (gameData) {
                        gameDetailView.imageUrl = gameData.headerImageUrl || ""
                        gameDetailView.verified = gameData.isVerified || false
                        // Set steamAppId before gameId so onSteamAppIdChanged fires with data ready
                        gameDetailView.steamAppId = gameData.steamAppId || ""
                    }
                    gameDetailView.gameId = gameId
                    contentStackContainer.navigateTo(2)
                }
            }

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

            GameDetailScreen {
                id: gameDetailView
                anchors.fill: parent
                visible: contentStackContainer.gameDetailVisible
                onBackClicked: {
                    contentStackContainer.navigateTo(0)
                    window.currentNavIndex = 0
                }
                onTranslateClicked: {
                    var gameData = GameService.getGameById(gameDetailView.gameId)
                    workflowView.gameId = gameDetailView.gameId
                    workflowView.gameName = gameDetailView.gameName
                    workflowView.gamePath = gameData ? gameData.installPath : ""
                    workflowView.gameEngine = gameDetailView.engine
                    workflowView.headerImageUrl = gameDetailView.imageUrl
                    contentStackContainer.navigateTo(3)

                    TranslationService.startTranslation(
                        workflowView.gameId,
                        workflowView.gameName,
                        workflowView.gamePath
                    )
                }
            }

            TranslationWorkflowScreen {
                id: workflowView
                anchors.fill: parent
                visible: contentStackContainer.workflowVisible
                onBackClicked: {
                    contentStackContainer.navigateTo(2)
                }
                onCompleted: function(gameId) {
                    contentStackContainer.navigateTo(0)
                    window.currentNavIndex = 0
                }
                onCancelled: function(gameId) {
                    contentStackContainer.navigateTo(2)
                }
            }

            // ===== BOTTOM GRADIENT SHADOW =====
            Rectangle {
                id: bottomGradientShadow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 120
                z: 10
                enabled: false

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.35) }
                }
            }
        }
    }

    // ===== TITLE BAR COMPONENT =====
    component TitleBar: Rectangle {
        id: titleBarRoot
        property var windowRef
        property bool translationMode: false
        signal minimizeClicked()
        signal maximizeClicked()
        signal closeClicked()
        signal trayClicked()

        color: Theme.withAlpha(Theme.surface, 0.7)

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
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
            anchors.leftMargin: 12
            anchors.rightMargin: 0
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: Dimensions.radiusStandard
                visible: titleBarRoot.translationMode
                color: Theme.turkishRed
                clip: true

                Rectangle {
                    x: 3; y: 4.5
                    width: 9; height: 9
                    radius: 4.5
                    color: "white"
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
                visible: !titleBarRoot.translationMode
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
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: "white"
                    }
                }
            }

            Label {
                text: "MakineAI"
                font.pixelSize: 12
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

        width: 46
        height: 32
        color: btnMouse.containsMouse
            ? (isClose ? Theme.closeButtonHover : Theme.glassBorder)
            : "transparent"
        radius: 0

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Label {
            anchors.centerIn: parent
            text: icon
            font.pixelSize: 10
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
        signal settingsClicked()
        signal donateClicked()
        signal aiToggleClicked(bool active)

        property bool aiActive: false

        color: Theme.withAlpha(Theme.surface, 0.7)

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

            Item {
                id: logoContainer
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignVCenter
                scale: logoMouse.containsMouse ? 1.05 : 1.0

                Behavior on scale {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                AnimatedGradientGlow {
                    anchors.centerIn: parent
                    width: 52; height: 52
                    active: true
                    animationsEnabled: window.animationsEnabled
                    opacity: logoMouse.containsMouse ? 0.7 : 0.35
                    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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

            Item {
                id: aiToggleItem
                Layout.fillHeight: true
                Layout.preferredWidth: aiToggleLabel.contentWidth + 24

                readonly property var rainbowColors: Theme.brandGradient

                property real animPhase: 0
                NumberAnimation on animPhase {
                    from: 0; to: 1
                    duration: 3000
                    loops: Animation.Infinite
                    running: window.animationsEnabled
                }

                function lerpHex(c1, c2, t) {
                    var r1 = parseInt(c1.substring(1, 3), 16)
                    var g1 = parseInt(c1.substring(3, 5), 16)
                    var b1 = parseInt(c1.substring(5, 7), 16)
                    var r2 = parseInt(c2.substring(1, 3), 16)
                    var g2 = parseInt(c2.substring(3, 5), 16)
                    var b2 = parseInt(c2.substring(5, 7), 16)
                    var r = Math.round(r1 + (r2 - r1) * t)
                    var g = Math.round(g1 + (g2 - g1) * t)
                    var b = Math.round(b1 + (b2 - b1) * t)
                    function pad(s) { return s.length < 2 ? "0" + s : s }
                    return "#" + pad(r.toString(16)) + pad(g.toString(16)) + pad(b.toString(16))
                }

                function makeRainbowText(str, phase) {
                    var nonSpace = []
                    for (var i = 0; i < str.length; i++) {
                        if (str[i] !== " ") nonSpace.push(i)
                    }
                    var result = ""
                    var n = rainbowColors.length
                    for (var j = 0; j < str.length; j++) {
                        if (str[j] === " ") {
                            result += " "
                        } else {
                            var ci = nonSpace.indexOf(j)
                            var pos = ((ci / Math.max(1, nonSpace.length - 1)) + phase) % 1.0
                            var scaledPos = pos * (n - 1)
                            var idx = Math.floor(scaledPos)
                            var frac = scaledPos - idx
                            var c = lerpHex(rainbowColors[idx % n], rainbowColors[(idx + 1) % n], frac)
                            result += '<span style="color:' + c + '">' + str[j] + '</span>'
                        }
                    }
                    return result
                }

                property real underlineWidth: aiToggleMouse.containsMouse ? 70 : 0
                Behavior on underlineWidth {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }

                Text {
                    id: aiToggleLabel
                    anchors.centerIn: parent
                    text: aiToggleItem.makeRainbowText(navBarRoot.aiActive ? "Kapat" : "Türkçe Yama", aiToggleItem.animPhase)
                    textFormat: Text.RichText
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Item {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: aiToggleItem.underlineWidth
                    height: 2
                    clip: true
                    visible: width > 0

                    Rectangle {
                        width: parent.width * 2
                        height: parent.height
                        radius: 1
                        x: -parent.width * aiToggleItem.animPhase

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.000; color: Theme.brandGold }
                            GradientStop { position: 0.056; color: Theme.brandOrange }
                            GradientStop { position: 0.111; color: Theme.brandCoral }
                            GradientStop { position: 0.167; color: Theme.brandPurple }
                            GradientStop { position: 0.222; color: Theme.brandBlue }
                            GradientStop { position: 0.278; color: Theme.brandTeal }
                            GradientStop { position: 0.333; color: Theme.brandGreen }
                            GradientStop { position: 0.389; color: Theme.brandLime }
                            GradientStop { position: 0.444; color: Theme.brandOlive }
                            GradientStop { position: 0.500; color: Theme.brandGold }
                            GradientStop { position: 0.556; color: Theme.brandOrange }
                            GradientStop { position: 0.611; color: Theme.brandCoral }
                            GradientStop { position: 0.667; color: Theme.brandPurple }
                            GradientStop { position: 0.722; color: Theme.brandBlue }
                            GradientStop { position: 0.778; color: Theme.brandTeal }
                            GradientStop { position: 0.833; color: Theme.brandGreen }
                            GradientStop { position: 0.889; color: Theme.brandLime }
                            GradientStop { position: 0.944; color: Theme.brandOlive }
                            GradientStop { position: 1.000; color: Theme.brandGold }
                        }
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

                ToolTip {
                    visible: aiToggleMouse.containsMouse
                    text: navBarRoot.aiActive ? qsTr("Türkçe Yamayı Kapat") : qsTr("Türkçe Yamayı Aç")
                    delay: 500
                }
            }

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

            Item {
                id: donateItem
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter

                property bool hovered: donateMouse.containsMouse
                scale: hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

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
                    id: coffeeCanvas
                    anchors.centerIn: parent
                    width: 20; height: 20
                    rotation: donateItem.wobble
                    opacity: donateItem.hovered ? 1.0 : 0.7
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    property real phase: donateItem.colorPhase
                    onPhaseChanged: requestPaint()

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
                            grad.addColorStop(i / (colors.length - 1), colors[i])

                        ctx.strokeStyle = grad
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        var s = 20 / 24
                        ctx.beginPath()
                        ctx.arc(17 * s - 0.5, 12 * s, 3.2 * s, -Math.PI / 2, Math.PI / 2)
                        ctx.stroke()

                        ctx.beginPath()
                        ctx.moveTo(3 * s, 8 * s)
                        ctx.lineTo(17 * s, 8 * s)
                        ctx.lineTo(17 * s, 17 * s)
                        ctx.quadraticCurveTo(17 * s, 21 * s, 13 * s, 21 * s)
                        ctx.lineTo(7 * s, 21 * s)
                        ctx.quadraticCurveTo(3 * s, 21 * s, 3 * s, 17 * s)
                        ctx.closePath()
                        ctx.stroke()

                        ctx.beginPath()
                        ctx.moveTo(6 * s, 2 * s); ctx.lineTo(6 * s, 4.5 * s)
                        ctx.moveTo(10 * s, 2 * s); ctx.lineTo(10 * s, 4.5 * s)
                        ctx.moveTo(14 * s, 2 * s); ctx.lineTo(14 * s, 4.5 * s)
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
                    text: "Destekçi Ol"
                    delay: 400
                }
            }

            Item {
                id: discordItem
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                Layout.alignment: Qt.AlignVCenter

                property bool hovered: discordMouse.containsMouse
                scale: hovered ? 1.1 : 1.0
                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                property real pulse: 0.7
                SequentialAnimation on pulse {
                    loops: Animation.Infinite
                    running: !discordItem.hovered && window.animationsEnabled
                    NumberAnimation { from: 0.7; to: 1.0; duration: 2000; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1.0; to: 0.7; duration: 2000; easing.type: Easing.InOutSine }
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
        }
    }

    // ===== PERFORMANCE MONITOR (F3 to toggle) =====
    property bool showPerformanceMonitor: false

    PerformanceMonitor {
        id: perfMonitor
        visible: window.showPerformanceMonitor
        z: 9999
    }

    Shortcut {
        sequence: "F3"
        onActivated: window.showPerformanceMonitor = !window.showPerformanceMonitor
    }

}
