import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
import "dialogs"
import "components"
import "screens"

/**
 * HomeScreen.qml - Main home view with game status, announcements, and projects
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    // Layout tuning values
    property real contentMargin: 16
    property real layoutCardMargin: 8
    property real layoutCardSpacing: 8
    property real layoutTopRowHeight: 210
    property real layoutTopRowGap: 16
    property real layoutGamesSectionGap: 8
    property real layoutCardGap: 32
    property real layoutSepTopMargin: 4
    property real layoutSepBottomMargin: 8

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()

    // Open all-games dialog (reuse existing if already created)
    function openAllGamesDialog() {
        if (allGamesDialogLoader.item) {
            allGamesDialogLoader.item.games = GameService.supportedGames
            allGamesDialogLoader.item.open()
        } else {
            allGamesDialogLoader.shouldOpen = true
            allGamesDialogLoader.active = true
        }
    }
    signal settingsRequested()

    // ===== UPDATE CHECKER (C++ backend) =====
    property bool updateAvailable: UpdateChecker.updateAvailable
    property string latestVersion: UpdateChecker.latestVersion
    property string notificationMessage: ""
    property string notificationType: "info"  // info, warning, error, update

    Connections {
        target: UpdateChecker
        function onCheckCompleted(hasUpdate, version, url) {
            if (hasUpdate) {
                notificationMessage = qsTr("Yeni sürüm mevcut: %1").arg(version)
                notificationType = "update"
            }
        }
    }

    function showNotification(message, type) {
        notificationMessage = message
        notificationType = type || "info"
    }

    function hideNotification() {
        notificationDismissAnim.start()
    }

    SequentialAnimation {
        id: notificationDismissAnim
        NumberAnimation {
            target: notificationBanner
            property: "opacity"
            to: 0
            duration: Dimensions.transitionDuration
            easing.type: Easing.OutCubic
        }
        ScriptAction {
            script: {
                notificationMessage = ""
                notificationBanner.opacity = 1
            }
        }
    }

    Component.onCompleted: {
        GameService.scanAllLibraries()
        if (SettingsManager.showNotifications)
            UpdateChecker.checkForUpdatesIfNeeded()
    }

    function showHomePage() {
        pageStack.currentIndex = 0
        replayEntryAnimations()
    }

    // Replay entry animations when page becomes visible again
    function replayEntryAnimations() {
        // Reset and replay top row animation
        topRowLayout.opacity = 0
        topRowEntryAnim.restart()

        // Reset and replay games section animation
        gamesSectionLayout.opacity = 0
        gamesSectionEntryAnim.restart()

        // Trigger game cards to replay their animations
        animationTrigger++
    }

    function showProjectsPage() {
        pageStack.currentIndex = 1
        projectsPage.replayProjectAnimations()
    }

    function showTranslationPage() {
        pageStack.currentIndex = 2
    }

    // Animation trigger - increment to replay all entry animations
    property int animationTrigger: 0

    // Staged loading — sections appear one by one to avoid startup freeze
    property int loadStage: 0
    Timer {
        id: stageTimer
        interval: 60
        repeat: true
        running: root.loadStage < 4
        onTriggered: root.loadStage++
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        // ===== PAGE STACK - Native Qt QStackedWidget =====
        StackLayout {
            id: pageStack
            anchors.fill: parent
            currentIndex: 0

            // ===== HOME PAGE (Index 0) =====
            Item {
                id: homePage

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: root.contentMargin
                    spacing: Dimensions.marginMD

                        // ===== NOTIFICATION BANNER =====
                        Rectangle {
                            id: notificationBanner
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            Layout.preferredHeight: notificationMessage ? 44 : 0
                            radius: Dimensions.radiusStandard
                            visible: notificationMessage !== ""
                            clip: true

                            color: {
                                switch(notificationType) {
                                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.15)
                                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.15)
                                    case "error": return Theme.withAlpha(Theme.notificationError, 0.15)
                                    default: return Theme.withAlpha(Theme.textMuted, 0.1)
                                }
                            }

                            border.color: {
                                switch(notificationType) {
                                    case "update": return Theme.withAlpha(Theme.notificationUpdate, 0.4)
                                    case "warning": return Theme.withAlpha(Theme.notificationWarning, 0.4)
                                    case "error": return Theme.withAlpha(Theme.notificationError, 0.4)
                                    default: return Theme.withAlpha(Theme.textPrimary, 0.1)
                                }
                            }
                            border.width: 1

                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: Dimensions.transitionDuration; easing.type: Easing.OutCubic }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Dimensions.marginMD
                                anchors.rightMargin: Dimensions.marginMS
                                spacing: Dimensions.spacingLG

                                Text {
                                    text: {
                                        switch(notificationType) {
                                            case "update": return "⬆"
                                            case "warning": return "⚠"
                                            case "error": return "✕"
                                            default: return "ℹ"
                                        }
                                    }
                                    font.pixelSize: Dimensions.fontLG
                                    color: {
                                        switch(notificationType) {
                                            case "update": return Theme.notificationUpdate
                                            case "warning": return Theme.notificationWarning
                                            case "error": return Theme.notificationError
                                            default: return Theme.textSecondary
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: notificationMessage
                                    font.pixelSize: Dimensions.fontBody
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    visible: notificationType === "update"
                                    width: updateBtnText.width + 16
                                    height: 28
                                    radius: Dimensions.radiusStandard
                                    color: updateBtnMouse.containsMouse ? Theme.withAlpha(Theme.notificationUpdate, 0.3) : Theme.withAlpha(Theme.notificationUpdate, 0.2)
                                    scale: updateBtnMouse.pressed ? 0.94 : 1.0
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Go to settings")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: root.settingsRequested()
                                    Keys.onSpacePressed: root.settingsRequested()

                                    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

                                    Text {
                                        id: updateBtnText
                                        anchors.centerIn: parent
                                        text: qsTr("Ayarlara Git")
                                        font.pixelSize: Dimensions.fontSM
                                        font.weight: Font.Medium
                                        color: Theme.notificationUpdate
                                    }

                                    // Focus indicator
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -1
                                        radius: parent.radius + 1
                                        color: "transparent"
                                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                                        border.width: 2
                                        visible: parent.activeFocus
                                    }

                                    MouseArea {
                                        id: updateBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.settingsRequested()
                                    }
                                }

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 12
                                    color: closeBtnMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                                    Accessible.role: Accessible.Button
                                    Accessible.name: qsTr("Close notification")
                                    activeFocusOnTab: true
                                    Keys.onReturnPressed: hideNotification()
                                    Keys.onSpacePressed: hideNotification()

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        font.pixelSize: Dimensions.fontSM
                                        color: Theme.textMuted
                                    }

                                    // Focus indicator
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -1
                                        radius: parent.radius + 1
                                        color: "transparent"
                                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                                        border.width: 2
                                        visible: parent.activeFocus
                                    }

                                    MouseArea {
                                        id: closeBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: hideNotification()
                                    }
                                }
                            }
                        }

                        // ===== UPDATE STATUS PILL =====
                        Row {
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: 6
                            visible: UpdateChecker.statusType === "upToDate" || UpdateChecker.statusType === "updateAvailable"

                            Rectangle {
                                width: 6; height: 6; radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.success
                            }

                            Text {
                                text: UpdateChecker.statusType === "updateAvailable"
                                      ? qsTr("%1 mevcut").arg(UpdateChecker.latestVersion)
                                      : qsTr("Güncel")
                                font.pixelSize: Dimensions.fontXS
                                color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.textMuted

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: UpdateChecker.statusType === "updateAvailable" ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        if (UpdateChecker.statusType === "updateAvailable")
                                            root.settingsRequested()
                                    }
                                }
                            }
                        }

                        // ===== TOP ROW =====
                        RowLayout {
                            id: topRowLayout
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.loadStage >= 1 ? root.layoutTopRowHeight : 0
                            Layout.maximumHeight: root.layoutTopRowHeight
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.layoutTopRowGap
                            visible: root.loadStage >= 1

                            opacity: 0
                            onVisibleChanged: if (visible) topRowEntryAnim.start()

                            NumberAnimation {
                                id: topRowEntryAnim
                                target: topRowLayout
                                property: "opacity"
                                from: 0; to: 1
                                duration: Dimensions.animSlow
                                easing.type: Easing.OutCubic
                            }

                            // ============================================================
                            // GAME STATUS CARD (ambient glow + comet arc)
                            // ============================================================
                            GameStatusCard {
                                animationsEnabled: root.animationsEnabled
                                layoutCardMargin: root.layoutCardMargin
                                layoutCardSpacing: root.layoutCardSpacing
                                layoutTopRowHeight: root.layoutTopRowHeight
                                onManualFolderRequested: root.manualFolderRequested()
                            }

                            // ============================================================
                            // ANNOUNCEMENT CARD (ambient glow + news/security)
                            // ============================================================
                            AnnouncementCard {
                                layoutCardMargin: root.layoutCardMargin
                                layoutCardSpacing: root.layoutCardSpacing
                                layoutTopRowHeight: root.layoutTopRowHeight
                            }
                        }

                        // ===== BATCH OPERATIONS PANEL =====
                        BatchOperationsPanel {
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            animationsEnabled: root.animationsEnabled
                            visible: root.loadStage >= 1
                        }

                        // ===== GAMES SECTION =====
                        ColumnLayout {
                            id: gamesSectionLayout
                            Layout.fillWidth: true
                            Layout.leftMargin: root.contentMargin
                            Layout.rightMargin: root.contentMargin
                            spacing: root.layoutGamesSectionGap
                            visible: root.loadStage >= 2

                            opacity: 0
                            onVisibleChanged: if (visible) gamesSectionEntryAnim.start()

                            SequentialAnimation {
                                id: gamesSectionEntryAnim
                                PauseAnimation { duration: Dimensions.transitionDuration }
                                NumberAnimation {
                                    target: gamesSectionLayout
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: Dimensions.animSlow
                                    easing.type: Easing.OutCubic
                                }
                            }

                            RowLayout {
                                spacing: Dimensions.spacingLG

                                Label {
                                    text: qsTr("Türkçe Yama Kütüphanesi")
                                    font.pixelSize: Dimensions.fontXL
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }

                                BusyIndicator {
                                    visible: GameService.isScanning
                                    running: GameService.isScanning
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                }

                                Item { Layout.fillWidth: true }

                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: root.layoutSepTopMargin
                                Layout.bottomMargin: root.layoutSepBottomMargin
                                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                            }

                            Flow {
                                id: skeletonFlow
                                Layout.fillWidth: true
                                spacing: Dimensions.cardGap
                                visible: GameService.supportedGameCount === 0 && GameService.isScanning

                                Repeater {
                                    model: 7

                                    GameCardSkeleton {
                                        animationsEnabled: root.animationsEnabled
                                        animationDelay: index * 100
                                    }
                                }
                            }

                            Row {
                                id: gamesRow
                                Layout.alignment: Qt.AlignHCenter
                                spacing: root.layoutCardGap
                                visible: !skeletonFlow.visible

                                readonly property int availableWidth: gamesSectionLayout.width
                                readonly property int cardTotal: Dimensions.cardWidth + root.layoutCardGap
                                readonly property int maxCards: Math.max(1, Math.floor((availableWidth - Dimensions.cardWidth) / cardTotal))

                                Repeater {
                                    id: gamesRepeater
                                    model: GameService.supportedGames.slice(0, gamesRow.maxCards)

                                    GameCard {
                                        id: gameCardDelegate
                                        required property var modelData
                                        required property int index
                                        gameName: modelData.name || ""
                                        imageUrl: modelData.headerImageUrl || ""
                                        verified: modelData.isVerified || false
                                        translated: modelData.hasTranslation || false
                                        gameId: modelData.id || ""
                                        installPath: modelData.installPath || ""
                                        steamAppId: modelData.steamAppId || ""

                                        // Staggered fade-in (no transform - fixes click issues)
                                        opacity: 0
                                        Component.onCompleted: entryAnimation.start()

                                        // Re-trigger animation when animationTrigger changes
                                        Connections {
                                            target: root
                                            function onAnimationTriggerChanged() {
                                                gameCardDelegate.opacity = 0
                                                entryAnimation.restart()
                                            }
                                        }

                                        SequentialAnimation {
                                            id: entryAnimation
                                            PauseAnimation { duration: index * 40 }
                                            NumberAnimation {
                                                target: gameCardDelegate
                                                property: "opacity"
                                                from: 0; to: 1
                                                duration: Dimensions.fadeTransitionDuration
                                                easing.type: Easing.OutCubic
                                            }
                                        }

                                        onClicked: {
                                            root.gameSelected(
                                                modelData.id || "",
                                                modelData.name || "",
                                                modelData.installPath || "",
                                                modelData.engine || ""
                                            )
                                        }
                                        onContextAction: function(action) {
                                            if (action === "openFolder")
                                                Qt.openUrlExternally("file:///" + modelData.installPath)
                                            else if (action === "openSteam")
                                                Qt.openUrlExternally("steam://nav/games/details/" + modelData.steamAppId)
                                        }
                                    }
                                }

                                ViewAllCard {
                                    id: viewAllCardItem
                                    remainingCount: Math.max(0, GameService.supportedGameCount - gamesRepeater.count)

                                    onClicked: root.openAllGamesDialog()
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                radius: Dimensions.radiusStandard
                                color: Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1
                                visible: GameService.supportedGameCount === 0 && !GameService.isScanning

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: Dimensions.spacingLG

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "\uD83C\uDFAE"
                                        font.pixelSize: Dimensions.fontBanner
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: qsTr("Çeviri paketi bulunamadı")
                                        font.pixelSize: Dimensions.fontMD
                                        color: Theme.textSecondary
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: qsTr("translation_data klasörünü kontrol edin")
                                        font.pixelSize: Dimensions.fontSM
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== PROJECTS PAGE (Index 1) =====
            Item {
                id: projectsPage

                readonly property var projectData: [
                    { title: "Cyberless: Online", desc: qsTr("Çok oyunculu cyberpunk aksiyon oyunu"), category: "oyun", status: qsTr("Tamamlandı"), statusColor: Theme.statusOnline, accent: Theme.statusOnline, emoji: "\uD83C\uDFAE", progress: 1.0 },
                    { title: "MakineAI Launcher", desc: qsTr("Türkçe oyun çevirisi başlatıcısı ve yönetim aracı"), category: "ceviri", status: qsTr("Alfa"), statusColor: Theme.primary, accent: Theme.primary, emoji: "\uD83D\uDE80", progress: 0.7 },
                    { title: qsTr("Topluluk Çeviri Paketi"), desc: qsTr("Açık kaynak topluluk çeviri paketleri"), category: "ceviri", status: qsTr("Sürekli"), statusColor: Theme.statusCyan, accent: Theme.statusCyan, emoji: "\uD83C\uDF0D", progress: -1 },
                    { title: "MakineAI", desc: qsTr("Oyun çevirisi için yapay zeka dil modeli"), category: "ceviri", status: qsTr("Geliştiriliyor"), statusColor: Theme.statusPurple, accent: Theme.statusPurple, emoji: "\uD83E\uDD16", progress: 0.3 },
                    { title: qsTr("Çeviri API"), desc: qsTr("Geliştiriciler için RESTful çeviri API hizmeti"), category: "ceviri", status: qsTr("Planlanıyor"), statusColor: Theme.warning, accent: Theme.warning, emoji: "\u26A1", progress: 0.0 }
                ]

                function replayProjectAnimations() {
                    for (var i = 0; i < projectCardsRepeater.count; i++) {
                        var item = projectCardsRepeater.itemAt(i)
                        if (item) item.replayAnimation()
                    }
                }

                // Main layout - EXACT same pattern as homepage
                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: root.contentMargin
                    spacing: Dimensions.marginMD

                    // ===== COMMUNITY SECTION =====
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.contentMargin
                        Layout.rightMargin: root.contentMargin
                        spacing: root.layoutGamesSectionGap

                        Label {
                            text: qsTr("Topluluk")
                            font.pixelSize: Dimensions.fontXL
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: root.layoutSepTopMargin
                            Layout.bottomMargin: root.layoutSepBottomMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingLG

                            component LinkCard: Rectangle {
                                property string label: ""
                                property string subtitle: ""
                                property string url: ""
                                property color cardColor: Theme.primary
                                property string iconType: "" // "discord", "globe", "heart"

                                Layout.fillWidth: true
                                Layout.preferredHeight: 60
                                radius: Dimensions.radiusStandard
                                color: lcMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.06)
                                                             : Theme.withAlpha(Theme.textPrimary, 0.03)
                                border.color: lcMouse.containsMouse ? Theme.withAlpha(cardColor, 0.25)
                                                                    : Theme.withAlpha(Theme.textPrimary, 0.08)
                                border.width: 1
                                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Dimensions.marginMS
                                    anchors.rightMargin: Dimensions.marginMS
                                    spacing: Dimensions.spacingLG

                                    // Icon circle with Canvas-drawn vector icon
                                    Rectangle {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                        Layout.alignment: Qt.AlignVCenter
                                        radius: 16
                                        color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.15 : 0.08)
                                        border.color: Theme.withAlpha(cardColor, lcMouse.containsMouse ? 0.25 : 0.12)
                                        border.width: 1
                                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                                        Canvas {
                                            anchors.centerIn: parent
                                            width: 16; height: 16
                                            onPaint: {
                                                var ctx = getContext("2d")
                                                ctx.clearRect(0, 0, width, height)
                                                ctx.strokeStyle = Qt.rgba(cardColor.r, cardColor.g, cardColor.b, 0.9)
                                                ctx.fillStyle = ctx.strokeStyle
                                                ctx.lineWidth = 1.5
                                                ctx.lineCap = "round"
                                                ctx.lineJoin = "round"

                                                if (iconType === "discord") {
                                                    // Chat bubble icon
                                                    ctx.beginPath()
                                                    ctx.moveTo(2, 4)
                                                    ctx.lineTo(2, 11)
                                                    ctx.lineTo(5, 11)
                                                    ctx.lineTo(5, 14)
                                                    ctx.lineTo(8, 11)
                                                    ctx.lineTo(14, 11)
                                                    ctx.lineTo(14, 4)
                                                    ctx.closePath()
                                                    ctx.stroke()
                                                    // Dots inside
                                                    ctx.beginPath()
                                                    ctx.arc(6, 7.5, 1, 0, Math.PI * 2)
                                                    ctx.fill()
                                                    ctx.beginPath()
                                                    ctx.arc(10, 7.5, 1, 0, Math.PI * 2)
                                                    ctx.fill()
                                                } else if (iconType === "globe") {
                                                    // Globe icon
                                                    ctx.beginPath()
                                                    ctx.arc(8, 8, 6.5, 0, Math.PI * 2)
                                                    ctx.stroke()
                                                    // Horizontal line
                                                    ctx.beginPath()
                                                    ctx.moveTo(1.5, 8)
                                                    ctx.lineTo(14.5, 8)
                                                    ctx.stroke()
                                                    // Vertical ellipse (meridian)
                                                    ctx.beginPath()
                                                    ctx.ellipse(4.5, 1.5, 7, 13, 0, 0, Math.PI * 2)
                                                    ctx.stroke()
                                                } else if (iconType === "heart") {
                                                    // Heart icon
                                                    ctx.beginPath()
                                                    ctx.moveTo(8, 14)
                                                    ctx.bezierCurveTo(1, 9, 1, 3.5, 4.5, 2.5)
                                                    ctx.bezierCurveTo(6.5, 2, 8, 4, 8, 4)
                                                    ctx.bezierCurveTo(8, 4, 9.5, 2, 11.5, 2.5)
                                                    ctx.bezierCurveTo(15, 3.5, 15, 9, 8, 14)
                                                    ctx.closePath()
                                                    ctx.fill()
                                                    ctx.globalAlpha = 0.3
                                                    ctx.stroke()
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: Dimensions.spacingXXS
                                        Label {
                                            text: label; font.pixelSize: Dimensions.fontBody; font.weight: Font.DemiBold
                                            color: lcMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                                        }
                                        Label { text: subtitle; font.pixelSize: Dimensions.fontCaption; color: Theme.textMuted }
                                    }

                                    Label {
                                        text: "\u2197"; font.pixelSize: Dimensions.fontBody; color: Theme.textMuted
                                        opacity: lcMouse.containsMouse ? 1.0 : 0.3
                                        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                    }
                                }

                                MouseArea {
                                    id: lcMouse; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally(url)
                                }
                                Accessible.role: Accessible.Link; Accessible.name: label
                                activeFocusOnTab: true
                                Keys.onReturnPressed: Qt.openUrlExternally(url)
                                Keys.onSpacePressed: Qt.openUrlExternally(url)
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: -1
                                    radius: parent.radius + 1; color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6); border.width: 2
                                    visible: parent.activeFocus
                                }
                            }

                            LinkCard { label: "Discord"; subtitle: qsTr("Topluluğa katıl"); url: Dimensions.discordUrl; cardColor: Theme.discordColor; iconType: "discord" }
                            LinkCard { label: qsTr("Web Sitesi"); subtitle: "makineai.com"; url: Dimensions.websiteUrl; cardColor: Theme.primary; iconType: "globe" }
                            LinkCard { label: qsTr("Destekçi Ol"); subtitle: qsTr("Projeleri destekle"); url: Dimensions.donatePageUrl; cardColor: Theme.brandCoral; iconType: "heart" }
                        }
                    }

                    // ===== PROJECTS SECTION =====
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.contentMargin
                        Layout.rightMargin: root.contentMargin
                        spacing: root.layoutGamesSectionGap

                        RowLayout {
                            spacing: Dimensions.spacingLG

                            Label {
                                text: qsTr("Aktif Projeler")
                                font.pixelSize: Dimensions.fontXL
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            Rectangle {
                                Layout.preferredHeight: 22
                                Layout.preferredWidth: projectCountLabel.width + 14
                                radius: Dimensions.badgeRadius
                                color: Theme.withAlpha(Theme.primary, 0.12)
                                Label {
                                    id: projectCountLabel; anchors.centerIn: parent
                                    text: qsTr("%1 proje").arg(projectsPage.projectData.length)
                                    font.pixelSize: Dimensions.fontXS; font.weight: Font.Medium; color: Theme.primary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            Layout.topMargin: root.layoutSepTopMargin
                            Layout.bottomMargin: root.layoutSepBottomMargin
                            color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Dimensions.spacingXL
                            rowSpacing: Dimensions.spacingLG

                            Repeater {
                                id: projectCardsRepeater
                                model: projectsPage.projectData

                                ProjectShowcaseCard {
                                    title: modelData.title
                                    description: modelData.desc
                                    status: modelData.status
                                    statusColor: modelData.statusColor
                                    accentColor: modelData.accent
                                    emoji: modelData.emoji
                                    progress: modelData.progress
                                    entryIndex: index
                                    animationsEnabled: root.animationsEnabled
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // Bottom fade gradient overlay
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 60
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Theme.bgPrimary }
                    }
                }
            }

            // ===== TRANSLATION PAGE (Index 2) - Türkçe Yamalar =====
            TranslationLibraryPage {
                id: translationPage
                animationsEnabled: root.animationsEnabled
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
                onInstallAndShowDetail: function(gameId, gameName, installPath, engine) {
                    root.installAndShowDetail(gameId, gameName, installPath, engine)
                }
            }
        }
    }

    // ===== GAME CARD COMPONENT - Modern minimal hover =====
    component GameCard: Item {
        id: gameCardRoot
        property string gameName: ""
        property string imageUrl: ""
        property bool verified: false
        property bool translated: false
        property string gameId: ""
        property string installPath: ""
        property string steamAppId: ""
        signal clicked()
        signal contextAction(string action)

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: gameName
        Accessible.description: {
            var desc = gameName
            if (verified) desc += " — " + qsTr("Verified")
            if (translated) desc += " — " + qsTr("Turkish translation available")
            return desc
        }
        Accessible.onPressAction: clicked()
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: cardMouse.containsMouse

        transform: [
            Translate { y: gameCardRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: gameCardRoot.width / 2; origin.y: gameCardRoot.height / 2
                xScale: gameCardRoot.isHovered ? 1.02 : 1.0; yScale: gameCardRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            }
        ]

        Rectangle {
            id: cardContent
            anchors.fill: parent
            radius: Dimensions.cardBorderRadius
            clip: true
            color: Theme.surface

            property real borderPhase: 0
            NumberAnimation on borderPhase {
                from: 0; to: 1
                duration: 8000
                loops: Animation.Infinite
                running: gameCardRoot.isHovered && root.animationsEnabled
            }

            Canvas {
                anchors.fill: parent
                z: Dimensions.zContent
                property real phase: cardContent.borderPhase
                onPhaseChanged: if (hov) requestPaint()
                property bool hov: gameCardRoot.isHovered
                onHovChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var angle = phase * Math.PI * 2
                    var cx = width / 2, cy = height / 2
                    var len = Math.max(width, height) * 0.7
                    var x1 = cx + Math.cos(angle) * len
                    var y1 = cy + Math.sin(angle) * len
                    var x2 = cx - Math.cos(angle) * len
                    var y2 = cy - Math.sin(angle) * len

                    var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                    var colors = Theme.brandGradient
                    for (var i = 0; i < colors.length; i++)
                        grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                    var r = Dimensions.cardBorderRadius
                    var bw = 1.5
                    var px = bw / 2, py = bw / 2
                    var w = width - bw, h = height - bw

                    ctx.beginPath()
                    ctx.moveTo(px + r, py)
                    ctx.lineTo(px + w - r, py)
                    ctx.arcTo(px + w, py, px + w, py + r, r)
                    ctx.lineTo(px + w, py + h - r)
                    ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                    ctx.lineTo(px + r, py + h)
                    ctx.arcTo(px, py + h, px, py + h - r, r)
                    ctx.lineTo(px, py + r)
                    ctx.arcTo(px, py, px + r, py, r)
                    ctx.closePath()

                    ctx.strokeStyle = grad
                    ctx.lineWidth = bw
                    ctx.globalAlpha = hov ? 0.8 : 0.0
                    ctx.stroke()
                }
            }

            Item {
                id: imageMask
                anchors.fill: parent
                visible: false
                layer.enabled: true
                Rectangle { anchors.fill: parent; radius: Dimensions.cardBorderRadius; color: "white" }
            }

            Image {
                id: gameImage
                anchors.fill: parent
                source: imageUrl
                fillMode: Image.PreserveAspectCrop
                sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                asynchronous: true
                cache: true
                visible: false
            }

            MultiEffect {
                anchors.fill: gameImage
                source: gameImage
                maskEnabled: true
                maskSource: imageMask
                visible: gameImage.status === Image.Ready
                brightness: gameCardRoot.isHovered ? 0.06 : 0
                Behavior on brightness { NumberAnimation { duration: Dimensions.animNormal } }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingMD
                visible: gameImage.status !== Image.Ready
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: Dimensions.fontXL
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }

            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: parent.height * 0.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.withAlpha(Theme.bgPrimary, 0.9) }
                }
            }

            Rectangle {
                visible: gameCardRoot.translated || gameCardRoot.verified
                anchors.top: parent.top; anchors.right: parent.right
                anchors.topMargin: Dimensions.spacingSM; anchors.rightMargin: Dimensions.spacingSM
                width: badgeRow.width + 8; height: 20
                radius: Dimensions.badgeRadius
                color: Theme.withAlpha(Theme.bgPrimary, 0.7)

                Row {
                    id: badgeRow
                    anchors.centerIn: parent; spacing: Dimensions.spacingXS
                    Rectangle {
                        visible: gameCardRoot.translated
                        width: 20; height: 14; radius: Dimensions.badgeRadius
                        color: Theme.turkishRed; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "TR"; font.pixelSize: Dimensions.fontMicro; font.weight: Font.Bold; color: Theme.textOnColor }
                    }
                    Rectangle {
                        visible: gameCardRoot.verified
                        width: 14; height: 14; radius: 7
                        color: Theme.primary; anchors.verticalCenter: parent.verticalCenter
                        Label { anchors.centerIn: parent; text: "✓"; font.pixelSize: Dimensions.fontMini; font.weight: Font.Bold; color: Theme.textOnColor }
                    }
                }
            }


            Label {
                id: gameNameLabel
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.margins: Dimensions.marginBase
                text: gameName; font.pixelSize: Dimensions.fontCaption; font.weight: Font.DemiBold
                color: Theme.textPrimary; elide: Text.ElideRight

                ToolTip {
                    visible: gameCardRoot.isHovered && gameNameLabel.truncated
                    delay: 300; text: gameName; font.pixelSize: Dimensions.fontSM
                    background: Rectangle { color: Theme.withAlpha(Theme.surface, 0.96); radius: Dimensions.radiusStandard; border.color: Theme.withAlpha(Theme.textPrimary, 0.12); border.width: 1 }
                }
            }
        }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton)
                    gameContextMenu.popup()
                else
                    gameCardRoot.clicked()
            }
        }

        Menu {
            id: gameContextMenu

            Overlay.modal: Rectangle { color: "transparent" }

            background: Rectangle {
                implicitWidth: 200
                radius: Dimensions.radiusMD
                color: Theme.glassBackground
                border.color: Theme.glassBorder
                border.width: 1
            }

            MenuItem {
                text: qsTr("Detaylar")
                onTriggered: gameCardRoot.clicked()
                contentItem: Label {
                    text: parent.text
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textPrimary
                    leftPadding: Dimensions.paddingSM
                }
                background: Rectangle {
                    color: parent.highlighted ? Theme.withAlpha(Theme.primary, 0.12) : "transparent"
                }
            }

            MenuSeparator {
                contentItem: Rectangle {
                    implicitHeight: 1
                    color: Theme.withAlpha(Theme.textPrimary, 0.08)
                }
            }

            MenuItem {
                text: qsTr("Klasörü Aç")
                enabled: gameCardRoot.installPath !== ""
                onTriggered: gameCardRoot.contextAction("openFolder")
                contentItem: Label {
                    text: parent.text
                    font.pixelSize: Dimensions.fontSM
                    color: parent.enabled ? Theme.textPrimary : Theme.textMuted
                    leftPadding: Dimensions.paddingSM
                }
                background: Rectangle {
                    color: parent.highlighted ? Theme.withAlpha(Theme.primary, 0.12) : "transparent"
                }
            }

            MenuItem {
                text: qsTr("Steam'de Aç")
                visible: gameCardRoot.steamAppId !== ""
                height: visible ? implicitHeight : 0
                onTriggered: gameCardRoot.contextAction("openSteam")
                contentItem: Label {
                    text: parent.text
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textPrimary
                    leftPadding: Dimensions.paddingSM
                }
                background: Rectangle {
                    color: parent.highlighted ? Theme.withAlpha(Theme.primary, 0.12) : "transparent"
                }
            }
        }
    }

    // ===== VIEW ALL CARD - Modern, GameCard ile uyumlu =====
    component ViewAllCard: Item {
        id: viewAllRoot
        property int remainingCount: 0

        signal clicked()

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("View All (+%1)").arg(remainingCount)
        Accessible.onPressAction: clicked()
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()

        width: Dimensions.cardWidth
        height: Dimensions.cardHeight

        property bool isHovered: viewAllMouse.containsMouse

        transform: [
            Translate { y: viewAllRoot.isHovered ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
            Scale {
                origin.x: viewAllRoot.width / 2; origin.y: viewAllRoot.height / 2
                xScale: viewAllRoot.isHovered ? 1.02 : 1.0; yScale: viewAllRoot.isHovered ? 1.02 : 1.0
                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
            }
        ]

        AnimatedGradientGlow {
            anchors.centerIn: viewAllContent
            width: viewAllContent.width + 40
            height: viewAllContent.height + 40
            active: viewAllRoot.isHovered
            animationsEnabled: root.animationsEnabled
            z: -2
        }

        Rectangle {
            id: viewAllContent
            anchors.fill: parent
            radius: Dimensions.cardBorderRadius
            color: Theme.bgPrimary

            property real borderPhase: 0
            NumberAnimation on borderPhase {
                from: 0; to: 1
                duration: 8000
                loops: Animation.Infinite
                running: viewAllRoot.visible && root.animationsEnabled
            }

            Canvas {
                anchors.fill: parent
                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (viewAllRoot.isHovered) requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    var angle = phase * Math.PI * 2
                    var cx = width / 2, cy = height / 2
                    var len = Math.max(width, height) * 0.7
                    var x1 = cx + Math.cos(angle) * len
                    var y1 = cy + Math.sin(angle) * len
                    var x2 = cx - Math.cos(angle) * len
                    var y2 = cy - Math.sin(angle) * len

                    var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                    var colors = Theme.brandGradient
                    for (var i = 0; i < colors.length; i++)
                        grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                    var r = Dimensions.cardBorderRadius
                    var bw = 1.5
                    var px = bw / 2, py = bw / 2
                    var w = width - bw, h = height - bw

                    ctx.beginPath()
                    ctx.moveTo(px + r, py)
                    ctx.lineTo(px + w - r, py)
                    ctx.arcTo(px + w, py, px + w, py + r, r)
                    ctx.lineTo(px + w, py + h - r)
                    ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                    ctx.lineTo(px + r, py + h)
                    ctx.arcTo(px, py + h, px, py + h - r, r)
                    ctx.lineTo(px, py + r)
                    ctx.arcTo(px, py, px + r, py, r)
                    ctx.closePath()

                    ctx.strokeStyle = grad
                    ctx.lineWidth = bw
                    ctx.globalAlpha = viewAllRoot.isHovered ? 0.8 : 0.4
                    ctx.stroke()
                }

                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
            }

            Canvas {
                id: viewAllCanvas
                anchors.fill: parent

                property real phase: viewAllContent.borderPhase
                onPhaseChanged: if (hov) requestPaint()
                property bool hov: viewAllRoot.isHovered
                onHovChanged: requestPaint()
                property int count: viewAllRoot.remainingCount
                onCountChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width / 2;
                    var angle = phase * Math.PI * 2;
                    var len = width * 0.6;
                    var x1 = cx + Math.cos(angle) * len;
                    var x2 = cx - Math.cos(angle) * len;
                    var colors = Theme.brandGradient;

                    var numGrad = ctx.createLinearGradient(x1, 0, x2, 0);
                    for (var i = 0; i < colors.length; i++)
                        numGrad.addColorStop(i / Math.max(1, colors.length - 1), colors[i]);

                    // Sideways "+N" number - centered
                    ctx.save();
                    ctx.translate(cx + 5, height / 2 - 4);
                    ctx.rotate(-Math.PI / 2);
                    ctx.font = "bold 46px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "middle";
                    ctx.globalAlpha = hov ? 1.0 : 0.6;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.5)";
                    ctx.fillText("+" + count, 0, 0);
                    ctx.restore();

                    // Gradient separator line - exact center
                    var lineY = height - 34;
                    var lineW = 36;
                    var lineX = Math.round(cx - lineW / 2);
                    ctx.globalAlpha = hov ? 0.5 : 0.15;
                    ctx.fillStyle = numGrad;
                    ctx.fillRect(lineX, lineY, lineW, 1.5);

                    // "Tümünü Gör" - exact center
                    ctx.font = "500 11px sans-serif";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "top";
                    ctx.globalAlpha = hov ? 0.9 : 0.4;
                    ctx.fillStyle = hov ? numGrad : "rgba(255,255,255,0.4)";
                    var label = hov ? qsTr("Tümünü Gör") + " →" : qsTr("Tümünü Gör");
                    ctx.fillText(label, Math.round(cx), lineY + 6);
                }
            }
        }

        // Focus indicator
        Rectangle {
            anchors.fill: viewAllContent
            anchors.margins: -2
            radius: viewAllContent.radius + 2
            color: "transparent"
            border.color: Theme.withAlpha(Theme.primary, 0.6)
            border.width: 2
            visible: viewAllRoot.activeFocus
            z: Dimensions.zBase
        }

        MouseArea {
            id: viewAllMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewAllRoot.clicked()
        }
    }

    // ===== ALL GAMES LOADING OVERLAY =====
    Rectangle {
        id: allGamesLoadingOverlay
        parent: Overlay.overlay
        anchors.fill: parent
        color: Theme.withAlpha("#000000", 0.4)
        visible: allGamesDialogLoader.shouldOpen && allGamesDialogLoader.status !== Loader.Ready
        z: 999

        // Block clicks while loading
        MouseArea { anchors.fill: parent }

        Column {
            anchors.centerIn: parent
            spacing: 16

            // Spinning arc indicator
            Item {
                width: 40; height: 40
                anchors.horizontalCenter: parent.horizontalCenter

                Rectangle {
                    id: spinnerRing
                    anchors.fill: parent
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.withAlpha(Theme.textPrimary, 0.12)
                    border.width: 3
                }

                Canvas {
                    id: spinnerArc
                    anchors.fill: parent

                    property real angle: 0
                    NumberAnimation on angle {
                        from: 0; to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: allGamesLoadingOverlay.visible
                    }
                    onAngleChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.strokeStyle = Theme.primary
                        ctx.lineWidth = 3
                        ctx.lineCap = "round"
                        var startRad = angle * Math.PI / 180
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 2, startRad, startRad + Math.PI * 0.75)
                        ctx.stroke()
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Yükleniyor...")
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: Theme.textPrimary
            }
        }
    }

    // Quick search shortcut
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: root.openAllGamesDialog()
    }

    // Pre-warm dialog after startup idle (creates component in background)
    Timer {
        interval: 2000
        running: true
        repeat: false
        onTriggered: {
            if (!allGamesDialogLoader.active)
                allGamesDialogLoader.active = true
        }
    }

    // ===== ALL GAMES DIALOG (kept alive after first creation) =====
    Loader {
        id: allGamesDialogLoader
        active: false
        property bool shouldOpen: false

        sourceComponent: Component {
            AllGamesDialog {
                parent: Overlay.overlay

                Component.onCompleted: {
                    games = GameService.supportedGames
                    if (allGamesDialogLoader.shouldOpen) {
                        allGamesDialogLoader.shouldOpen = false
                        open()
                    }
                }

                onGameSelected: function(gameId) {
                    var gameData = GameService.getGameById(gameId)
                    if (gameData) {
                        root.gameSelected(gameId, gameData.name, gameData.installPath, gameData.engine || "Unknown")
                    }
                    close()
                }

                // Dialog stays alive — no deactivation on close
            }
        }
    }

}
