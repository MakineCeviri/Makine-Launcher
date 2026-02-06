import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "dialogs"
import "components"

/**
 * HomeScreen.qml - Native Qt HomeView birebir port
 * Kaynak: ui/src/views/homeview.cpp
 *
 * Yapı:
 * - QStackedWidget: homePage, projectsPage, aiActivePage
 * - Padding: 48px all around
 * - Spacing: 48px between sections
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    // Signals for navigation
    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal scanRequested()

    Component.onCompleted: {
        // Trigger initial game scan
        GameService.scanAllLibraries()
    }

    // Public functions
    function showHomePage() {
        if (!aiActive) {
            pageStack.currentIndex = 0
            // Replay entry animations when returning to home
            replayEntryAnimations()
        }
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
        if (!aiActive) {
            pageStack.currentIndex = 1
        }
    }

    function setAIActive(active) {
        aiActive = active
        if (active) {
            pageStack.currentIndex = 2
        } else {
            pageStack.currentIndex = 0
        }
    }

    // Private state
    property bool aiActive: false

    // Animation trigger - increment to replay all entry animations
    property int animationTrigger: 0

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

                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    clip: true

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        background: Rectangle { color: "transparent" }
                        contentItem: Rectangle {
                            implicitWidth: 8
                            radius: 4
                            color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                        }
                    }

                    ColumnLayout {
                        width: parent.width
                        // Native Qt: contentLayout->setContentsMargins
                        spacing: Dimensions.marginLG  // 24px - reduced from 48

                        Item { Layout.preferredHeight: Dimensions.marginMD }  // 16px top padding

                        // ===== TOP ROW - Flutter: IntrinsicHeight Row =====
                        RowLayout {
                            id: topRowLayout
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            spacing: Dimensions.marginLG

                            // Simple fade-in animation
                            opacity: 0
                            Component.onCompleted: topRowEntryAnim.start()

                            NumberAnimation {
                                id: topRowEntryAnim
                                target: topRowLayout
                                property: "opacity"
                                from: 0; to: 1
                                duration: 400
                                easing.type: Easing.OutCubic
                            }

                            // ============================================================
                            // GAME STATUS CARD - Flutter: _buildGameStatusCard
                            // Modern Turkish flag icon + balanced typography
                            // ============================================================
                            Rectangle {
                                id: gameStatusCard
                                Layout.fillWidth: true
                                implicitHeight: gameStatusCardLayout.implicitHeight + 40
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    id: gameStatusCardLayout
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 20
                                    spacing: 14

                                    // Header Row with Turkish Flag
                                    Row {
                                        Layout.fillWidth: true
                                        height: 44
                                        spacing: 14

                                        // Modern Turkish Flag Icon
                                        Rectangle {
                                            width: 44
                                            height: 44
                                            radius: 8
                                            color: "#E30A17"  // Turkish red

                                            // White crescent
                                            Rectangle {
                                                x: 10
                                                y: 12
                                                width: 20
                                                height: 20
                                                radius: 10
                                                color: "white"

                                                // Inner red circle for crescent effect
                                                Rectangle {
                                                    x: 5
                                                    y: 2
                                                    width: 16
                                                    height: 16
                                                    radius: 8
                                                    color: "#E30A17"
                                                }
                                            }

                                            // Star
                                            Text {
                                                x: 26
                                                y: 14
                                                text: "★"
                                                font.pixelSize: 12
                                                color: "white"
                                            }
                                        }

                                        // Title Column
                                        Column {
                                            anchors.verticalCenter: parent.children[0].verticalCenter
                                            spacing: 2

                                            Label {
                                                text: "Türkçe Yama"
                                                font.pixelSize: 16
                                                font.weight: Font.DemiBold
                                                font.letterSpacing: 0.3
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                text: "Oyun çeviri durumu"
                                                font.pixelSize: 11
                                                color: Theme.textMuted
                                            }
                                        }

                                        Item { width: 20; height: 1 }

                                        // Status badge
                                        Rectangle {
                                            anchors.verticalCenter: parent.children[0].verticalCenter
                                            width: statusBadgeContent.width + 16
                                            height: 24
                                            radius: 12
                                            color: Theme.withAlpha(Theme.error, 0.12)

                                            Row {
                                                id: statusBadgeContent
                                                anchors.centerIn: parent
                                                spacing: 5

                                                Rectangle {
                                                    width: 6
                                                    height: 6
                                                    radius: 3
                                                    color: Theme.error
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }

                                                Label {
                                                    text: "Bekleniyor"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Medium
                                                    color: Theme.error
                                                }
                                            }
                                        }
                                    }

                                    // Content Box
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 110
                                        radius: 6
                                        color: Qt.rgba(1, 1, 1, 0.02)
                                        border.color: Qt.rgba(1, 1, 1, 0.06)
                                        border.width: 1

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 10

                                            // Hourglass icon
                                            Rectangle {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                width: 40
                                                height: 40
                                                radius: 20
                                                color: Theme.withAlpha(Theme.error, 0.1)

                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "⏳"
                                                    font.pixelSize: 20
                                                }
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: "Oyun Bekleniyor"
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                                color: Theme.textPrimary
                                            }

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: "Desteklenen bir oyun çalıştırın veya manuel seçin"
                                                font.pixelSize: 11
                                                color: Theme.textMuted
                                            }
                                        }
                                    }

                                    // Manual Select Button
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 40
                                        radius: 6
                                        color: manualBtnMouse.containsMouse
                                            ? Qt.rgba(1, 1, 1, 0.1)
                                            : Qt.rgba(1, 1, 1, 0.05)
                                        border.color: Qt.rgba(1, 1, 1, 0.08)
                                        border.width: 1

                                        Behavior on color {
                                            ColorAnimation { duration: 150 }
                                        }

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 8

                                            Label {
                                                text: "+"
                                                font.pixelSize: 16
                                                font.weight: Font.Medium
                                                color: manualBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            }

                                            Label {
                                                text: "Manuel Oyun Seç"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: manualBtnMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            }
                                        }

                                        MouseArea {
                                            id: manualBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: gameDetectorDialog.open()
                                        }
                                    }
                                }
                            }

                            // ============================================================
                            // ANNOUNCEMENT CARD - Balanced typography
                            // ============================================================
                            Rectangle {
                                id: announcementCard
                                Layout.fillWidth: true
                                implicitHeight: announcementCardLayout.implicitHeight + 40
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                ColumnLayout {
                                    id: announcementCardLayout
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 20
                                    spacing: 14

                                    // Header Row
                                    Row {
                                        Layout.fillWidth: true
                                        height: 44
                                        spacing: 14

                                        // Icon container
                                        Rectangle {
                                            width: 44
                                            height: 44
                                            radius: 8
                                            color: Theme.withAlpha(Theme.info, 0.12)

                                            Label {
                                                anchors.centerIn: parent
                                                text: "📢"
                                                font.pixelSize: 22
                                            }
                                        }

                                        // Title section
                                        Column {
                                            anchors.verticalCenter: parent.children[0].verticalCenter
                                            spacing: 3

                                            Row {
                                                spacing: 8

                                                // "ÖNEMLİ" badge
                                                Rectangle {
                                                    width: 52
                                                    height: 18
                                                    radius: 9
                                                    gradient: Gradient {
                                                        orientation: Gradient.Horizontal
                                                        GradientStop { position: 0.0; color: Theme.splashOrange }
                                                        GradientStop { position: 1.0; color: Theme.splashGold }
                                                    }

                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: "ÖNEMLİ"
                                                        font.pixelSize: 9
                                                        font.weight: Font.Bold
                                                        font.letterSpacing: 0.5
                                                        color: "white"
                                                    }
                                                }

                                                Label {
                                                    text: "Duyuru"
                                                    font.pixelSize: 11
                                                    color: Theme.textMuted
                                                    anchors.verticalCenter: parent.children[0].verticalCenter
                                                }
                                            }

                                            Label {
                                                text: "Desteklenmeyen Oyunlar Hakkında"
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                                font.letterSpacing: 0.2
                                                color: Theme.textPrimary
                                            }
                                        }
                                    }

                                    // Description text
                                    Label {
                                        Layout.fillWidth: true
                                        text: "MakineAI, resmi olarak desteklenen oyunlar dışında kullanıldığında beklendiği gibi çalışmayabilir. Desteklenmeyen oyunlarda çeviri hataları veya performans sorunları yaşanabilir."
                                        font.pixelSize: 12
                                        lineHeight: 1.6
                                        color: Theme.textSecondary
                                        wrapMode: Text.WordWrap
                                    }

                                    // Date row
                                    Row {
                                        spacing: 6

                                        Label {
                                            text: "🕐"
                                            font.pixelSize: 11
                                        }

                                        Label {
                                            text: "18 Ocak 2026"
                                            font.pixelSize: 11
                                            color: Theme.textMuted
                                        }
                                    }

                                    // Red warning box
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 64
                                        radius: 6
                                        color: Qt.rgba(0.894, 0.224, 0.208, 0.08)
                                        border.color: Qt.rgba(0.894, 0.224, 0.208, 0.15)
                                        border.width: 1

                                        Row {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 10

                                            Label {
                                                text: "🛡️"
                                                font.pixelSize: 16
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Label {
                                                width: parent.width - 38
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: "Uygulamayı yalnızca resmi web sitemiz makineai.com üzerinden indirdiğinizden emin olun."
                                                font.pixelSize: 11
                                                lineHeight: 1.4
                                                color: Theme.textSecondary
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ===== GAMES SECTION =====
                        ColumnLayout {
                            id: gamesSectionLayout
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            // Native Qt: gamesSectionLayout->setSpacing(20)
                            spacing: 20

                            // Simple fade-in animation (no transform - fixes click issues)
                            opacity: 0
                            Component.onCompleted: gamesSectionEntryAnim.start()

                            SequentialAnimation {
                                id: gamesSectionEntryAnim
                                PauseAnimation { duration: 200 }
                                NumberAnimation {
                                    target: gamesSectionLayout
                                    property: "opacity"
                                    from: 0; to: 1
                                    duration: 400
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // Header row
                            RowLayout {
                                spacing: 12

                                // Title - Native Qt: fontSize 20, w600
                                Label {
                                    text: GameService.isScanning ? "Oyunlar Taraniyor..." : "Desteklenen Oyunlar"
                                    font.pixelSize: 20
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }

                                // Badge - Native Qt: primary 15% alpha
                                Rectangle {
                                    Layout.preferredHeight: 24
                                    Layout.preferredWidth: Math.max(gamesCountLabel.width + 16, 60)
                                    radius: 4
                                    color: Theme.withAlpha(Theme.primary, 0.15)

                                    Label {
                                        id: gamesCountLabel
                                        anchors.centerIn: parent
                                        text: GameService.gameCount + " oyun"
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: Theme.primary
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // Refresh button
                                Rectangle {
                                    Layout.preferredWidth: Math.max(refreshRow.width + 20, 80)
                                    Layout.preferredHeight: 32
                                    radius: 4
                                    color: refreshMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                                    visible: !GameService.isScanning

                                    Row {
                                        id: refreshRow
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Label {
                                            text: "\u21BB"  // Refresh
                                            font.pixelSize: 14
                                            color: refreshMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Label {
                                            text: "Yenile"
                                            font.pixelSize: 12
                                            color: refreshMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: refreshMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: GameService.scanAllLibraries()
                                    }
                                }

                                // Scanning indicator
                                Row {
                                    visible: GameService.isScanning
                                    spacing: 8

                                    BusyIndicator {
                                        width: 20
                                        height: 20
                                        running: GameService.isScanning
                                    }

                                    Label {
                                        text: GameService.scanStatus || "Taraniyor..."
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            // Skeleton loaders during initial scan
                            Flow {
                                id: skeletonFlow
                                Layout.fillWidth: true
                                spacing: Dimensions.cardGap
                                visible: GameService.isScanning && GameService.gameCount === 0

                                Repeater {
                                    model: 6  // Show 6 skeleton cards during loading

                                    GameCardSkeleton {
                                        animationsEnabled: root.animationsEnabled
                                        animationDelay: index * 100  // Staggered shimmer
                                    }
                                }
                            }

                            // Games grid - Native Qt: FlowLayout spacing 16
                            // Staggered loading with fade-in and scale
                            Flow {
                                id: gamesFlow
                                Layout.fillWidth: true
                                spacing: Dimensions.cardGap
                                visible: !skeletonFlow.visible

                                Repeater {
                                    id: gamesRepeater
                                    model: GameService.games.slice(0, 10)  // Show first 10 games

                                    GameCard {
                                        id: gameCardDelegate
                                        required property var modelData
                                        required property int index
                                        gameName: modelData.name || ""
                                        imageUrl: modelData.headerImageUrl || ""
                                        verified: modelData.isVerified || false
                                        translated: modelData.hasTranslation || false

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
                                                duration: 300
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
                                    }
                                }

                                // View All Card - Native Qt: ViewAllCard remainingCount
                                ViewAllCard {
                                    id: viewAllCardItem
                                    remainingCount: Math.max(0, GameService.gameCount - 10)
                                    visible: GameService.gameCount > 10

                                    // Simple fade-in (no transform/scale - fixes click issues)
                                    opacity: 0
                                    Component.onCompleted: viewAllEntryAnim.start()

                                    // Re-trigger animation when animationTrigger changes
                                    Connections {
                                        target: root
                                        function onAnimationTriggerChanged() {
                                            viewAllCardItem.opacity = 0
                                            viewAllEntryAnim.restart()
                                        }
                                    }

                                    SequentialAnimation {
                                        id: viewAllEntryAnim
                                        PauseAnimation { duration: Math.min(gamesRepeater.count, 10) * 40 + 100 }
                                        NumberAnimation {
                                            target: viewAllCardItem
                                            property: "opacity"
                                            from: 0; to: 1
                                            duration: 300
                                            easing.type: Easing.OutCubic
                                        }
                                    }

                                    onClicked: {
                                        allGamesDialog.games = GameService.games
                                        allGamesDialog.open()
                                    }
                                }
                            }

                            // Empty state when no games
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 120
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1
                                visible: GameService.gameCount === 0 && !GameService.isScanning

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 12

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "\uD83C\uDFAE"  // Game controller
                                        font.pixelSize: 32
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "Henuz oyun bulunamadi"
                                        font.pixelSize: 14
                                        color: Theme.textSecondary
                                    }

                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "Steam, Epic veya GOG kutuphane klasorlerinizi kontrol edin"
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: Dimensions.marginXXL }
                    }
                }

                // Bottom gradient - Native Qt: height 120, transparent -> black 35%
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 120

                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.35) }
                    }
                }
            }

            // ===== PROJECTS PAGE (Index 1) =====
            Item {
                id: projectsPage

                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    clip: true

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        background: Rectangle { color: "transparent" }
                        contentItem: Rectangle {
                            implicitWidth: 8
                            radius: 4
                            color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
                        }
                    }

                    ColumnLayout {
                        width: parent.width
                        spacing: 32

                        Item { Layout.preferredHeight: Dimensions.marginXXL }

                        // Projects header
                        RowLayout {
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            spacing: 16

                            // Icon container
                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Qt.rgba(Theme.gold.r, Theme.gold.g, Theme.gold.b, 0.2) }
                                    GradientStop { position: 1.0; color: Qt.rgba(Theme.olive.r, Theme.olive.g, Theme.olive.b, 0.2) }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDE80"  // Rocket
                                    font.pixelSize: 24
                                }
                            }

                            ColumnLayout {
                                spacing: 4

                                Label {
                                    text: "Projelerimiz"
                                    font.pixelSize: 24
                                    font.weight: Font.Bold
                                    color: Theme.textPrimary
                                }

                                Label {
                                    text: "Makine Ceviri toplulugununaktif projeleri"
                                    font.pixelSize: 14
                                    color: Theme.textSecondary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // CEDRA Interactive Card - Flutter: _CedraInteractiveCard with animated gradient
                        CedraInteractiveCard {
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            animationsEnabled: root.animationsEnabled
                        }

                        // Game Projects Category
                        ProjectCategory {
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            title: "Oyun Projeleri"
                            subtitle: "CEDRA Interactive bunyesinde gelistirilen oyunlar"
                            categoryColor: "#E53935"
                        }

                        // Translation Projects Category
                        ProjectCategory {
                            Layout.fillWidth: true
                            Layout.leftMargin: Dimensions.marginXXL
                            Layout.rightMargin: Dimensions.marginXXL
                            title: "Ceviri Projeleri"
                            subtitle: "Topluluk tarafindan yurutulen ceviri projeleri"
                            categoryColor: "#00BCD4"
                        }

                        Item { Layout.preferredHeight: Dimensions.marginXXL }
                    }
                }

                // Bottom gradient shadow - Flutter: 120px, transparent → black 35%
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 120
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.35) }
                    }
                    // Don't block mouse events
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        propagateComposedEvents: true
                    }
                }
            }

            // ===== AI ACTIVE PAGE (Index 2) =====
            Item {
                id: aiActivePage

                Rectangle {
                    anchors.fill: parent
                    color: Theme.bgPrimary

                    // Waiting card - Native Qt: maxWidth 400, padding 32
                    Rectangle {
                        anchors.centerIn: parent
                        width: 400
                        height: waitingCardContent.height + 64
                        radius: 4
                        color: "#151515"
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        ColumnLayout {
                            id: waitingCardContent
                            anchors.centerIn: parent
                            spacing: 20
                            width: parent.width - 64

                            // Search icon circle
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 64
                                radius: 32
                                color: Qt.rgba(1, 1, 1, 0.05)

                                Label {
                                    anchors.centerIn: parent
                                    text: "\uD83D\uDD0D"  // Magnifying glass
                                    font.pixelSize: 28
                                }
                            }

                            // Title
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Oyun Aktif Degil"
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }

                            // Description
                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                text: "Desteklenen bir oyunu baslattignizda\notomatik olarak tespit edilecektir."
                                font.pixelSize: 13
                                color: Theme.textMuted
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                lineHeight: 1.5
                            }

                            // Supported games hint
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.03)

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Label {
                                        text: "\uD83C\uDFAE"  // Game controller
                                        font.pixelSize: 16
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: GameService.gameCount + " desteklenen oyun"
                                        font.pixelSize: 12
                                        color: Theme.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== GLASS CARD COMPONENT =====
    component GlassCard: Rectangle {
        radius: 4
        // Native Qt: white 3% alpha
        color: Qt.rgba(1, 1, 1, 0.03)
        // Native Qt: white 8% alpha border
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
    }

    // ===== GAME CARD COMPONENT - Native Qt GameCard birebir =====
    component GameCard: Rectangle {
        id: gameCardRoot
        property string gameName: ""
        property string imageUrl: ""
        property bool verified: false
        property bool translated: false

        signal clicked()

        // Native Qt: fixedSize(140, 200)
        width: Dimensions.cardWidth
        height: Dimensions.cardHeight
        radius: Dimensions.cardBorderRadius
        clip: true
        color: "transparent"

        // Animation properties - Native Qt: 4 parallel animations
        property real cardScale: 1.0
        property real hoverOpacity: 0.0
        property real glowIntensity: 0.0
        property real borderGlow: 0.0
        property bool isPressed: false
        property bool isHovered: cardMouse.containsMouse

        // Scale animation - Native Qt: 250ms OutCubic
        Behavior on cardScale {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }
        // Hover opacity - Native Qt: 200ms OutQuad
        Behavior on hoverOpacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }
        // Glow intensity - Native Qt: 250ms OutCubic
        Behavior on glowIntensity {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }
        // Border glow - Native Qt: 220ms OutQuad
        Behavior on borderGlow {
            NumberAnimation { duration: 220; easing.type: Easing.OutQuad }
        }

        // Update animations on hover change
        onIsHoveredChanged: {
            if (!isPressed) {
                cardScale = isHovered ? 1.05 : 1.0
            }
            hoverOpacity = isHovered ? 1.0 : 0.0
            glowIntensity = isHovered ? 1.0 : 0.0
            borderGlow = isHovered ? 1.0 : 0.0
        }

        // Scale transform
        transform: Scale {
            origin.x: gameCardRoot.width / 2
            origin.y: gameCardRoot.height / 2
            xScale: gameCardRoot.cardScale
            yScale: gameCardRoot.cardScale
        }

        // Native Qt: Pink shadow layer - #FF69B4 20% alpha, blur 6, offset (0, 4)
        Rectangle {
            visible: gameCardRoot.glowIntensity > 0.01
            anchors.fill: cardContent
            anchors.margins: -6 * gameCardRoot.glowIntensity
            anchors.topMargin: -6 * gameCardRoot.glowIntensity + 4
            anchors.bottomMargin: -6 * gameCardRoot.glowIntensity - 4
            radius: Dimensions.cardBorderRadius + 3
            color: Qt.rgba(1, 0.41, 0.71, 0.2 * gameCardRoot.glowIntensity)
            z: -2
        }

        // Native Qt: Gold shadow layer - #FFD700 10% alpha, blur 8, offset (0, 2)
        Rectangle {
            visible: gameCardRoot.glowIntensity > 0.01
            anchors.fill: cardContent
            anchors.margins: -8 * gameCardRoot.glowIntensity
            anchors.topMargin: -8 * gameCardRoot.glowIntensity + 2
            anchors.bottomMargin: -8 * gameCardRoot.glowIntensity - 2
            radius: Dimensions.cardBorderRadius + 4
            color: Qt.rgba(1, 0.84, 0, 0.1 * gameCardRoot.glowIntensity)
            z: -3
        }

        // Native Qt: Normal shadow - black 20%, blur 6, offset (0, 3)
        Rectangle {
            visible: gameCardRoot.glowIntensity < 0.99
            anchors.fill: cardContent
            anchors.margins: -4
            anchors.topMargin: -4 + 3
            anchors.bottomMargin: -4 - 3
            radius: Dimensions.cardBorderRadius + 2
            color: Qt.rgba(0, 0, 0, 0.2 * (1 - gameCardRoot.glowIntensity))
            z: -4
        }

        // Main card content
        Rectangle {
            id: cardContent
            anchors.fill: parent
            radius: Dimensions.cardBorderRadius
            clip: true

            // Background gradient - Native Qt: surface -> surfaceActive
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.surface }
                GradientStop { position: 1.0; color: Theme.surfaceActive }
            }

            // Game header image
            Image {
                id: gameImage
                anchors.fill: parent
                source: imageUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                visible: status === Image.Ready
            }

            // Game initials placeholder (fallback when no image)
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                visible: gameImage.status !== Image.Ready

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: gameName.substring(0, 2).toUpperCase()
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: Theme.textMuted
                }
            }

            // Bottom gradient overlay - Native Qt: 50% -> 90% black
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * 0.5

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.9) }
                }
            }

            // Verified badge - Native Qt: top 8, right 8, primary 90%
            Rectangle {
                visible: gameCardRoot.verified
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: 22
                height: 22
                radius: 4
                color: Theme.withAlpha(Theme.primary, 0.9)

                Label {
                    anchors.centerIn: parent
                    text: "\u2713"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    color: "white"
                }
            }

            // Translated badge (TR) - Native Qt: success 90%
            Rectangle {
                visible: gameCardRoot.translated
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 8
                anchors.leftMargin: 8
                width: 22
                height: 22
                radius: 4
                color: Theme.withAlpha(Theme.success, 0.9)

                Label {
                    anchors.centerIn: parent
                    text: "TR"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    color: "white"
                }
            }

            // Game name - Native Qt: left 10, bottom 10, fontSize 13, w600
            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                text: gameName
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: "white"
                elide: Text.ElideRight
            }

            // Hover overlay - Native Qt: Gold 8% -> Pink 12%
            Rectangle {
                anchors.fill: parent
                opacity: gameCardRoot.hoverOpacity

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(1, 0.84, 0, 0.08) }
                    GradientStop { position: 1.0; color: Qt.rgba(1, 0.41, 0.71, 0.12) }
                }

                // Detay button - Native Qt: gradient gold-pink, borderRadius 4
                Rectangle {
                    anchors.centerIn: parent
                    width: 60
                    height: 28
                    radius: 4
                    opacity: gameCardRoot.hoverOpacity

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.splashGold }
                        GradientStop { position: 1.0; color: Theme.pink }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: "Detay"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "white"
                    }
                }
            }

            // Border glow - Native Qt: gold-pink gradient border
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Dimensions.cardBorderRadius
                color: "transparent"
                border.width: 2
                opacity: gameCardRoot.borderGlow * 0.6

                border.color: Qt.rgba(1, 0.84, 0, 0.6)
            }
        }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            // Native Qt: Press effect - scale 1.0 -> 1.02 (80ms)
            onPressed: {
                gameCardRoot.isPressed = true
                gameCardRoot.cardScale = 1.02
            }

            // Native Qt: Release effect - scale 1.02 -> 1.05 (150ms)
            onReleased: {
                gameCardRoot.isPressed = false
                if (containsMouse) {
                    gameCardRoot.cardScale = 1.05
                } else {
                    gameCardRoot.cardScale = 1.0
                }
            }

            onClicked: gameCardRoot.clicked()
        }
    }

    // ===== VIEW ALL CARD COMPONENT - Native Qt birebir =====
    component ViewAllCard: Rectangle {
        id: viewAllRoot
        property int remainingCount: 0

        signal clicked()

        // Native Qt: 140x200
        width: Dimensions.cardWidth
        height: Dimensions.cardHeight
        radius: Dimensions.cardBorderRadius

        // Native Qt: 2000ms color animation loop
        // GPU optimization: only run when visible and animations enabled
        property real animValue: 0.0
        NumberAnimation on animValue {
            from: 0.0
            to: 1.0
            duration: 2000
            loops: Animation.Infinite
            running: viewAllRoot.visible && root.animationsEnabled
        }

        // Native Qt: Animated accent color gold <-> olive
        property color accentColor: Theme.lerpColor(Theme.gold, Theme.olive, animValue)

        // Native Qt: hover scale 1.0 -> 1.05 (200ms)
        scale: viewAllMouse.containsMouse ? 1.05 : 1.0
        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        // Native Qt: hover ? darkSurface 80% : transparent
        color: viewAllMouse.containsMouse
            ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.8)
            : "transparent"

        // Native Qt: border color
        // hover ? accentColor 60% : white 20%
        // hover ? width 2 : 1.5
        border.color: viewAllMouse.containsMouse
            ? Theme.withAlpha(accentColor, 0.6)
            : Qt.rgba(1, 1, 1, 0.2)
        border.width: viewAllMouse.containsMouse ? 2 : 1.5

        // Native Qt: Shadow on hover - accentColor 30%, blur 8, offset (0, 8)
        Rectangle {
            visible: viewAllMouse.containsMouse
            anchors.fill: parent
            anchors.topMargin: 8
            radius: Dimensions.cardBorderRadius
            color: Theme.withAlpha(viewAllRoot.accentColor, 0.3)
            z: -1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 0

            Item { Layout.preferredHeight: 14 }

            // Native Qt: Plus icon 50x50 circle
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                radius: 25
                color: viewAllMouse.containsMouse
                    ? Theme.withAlpha(viewAllRoot.accentColor, 0.15)
                    : "transparent"
                border.color: viewAllMouse.containsMouse
                    ? viewAllRoot.accentColor
                    : Qt.rgba(1, 1, 1, 0.3)
                border.width: 2

                // Plus icon
                Rectangle {
                    anchors.centerIn: parent
                    width: 14
                    height: 3
                    radius: 1.5
                    color: viewAllMouse.containsMouse
                        ? viewAllRoot.accentColor
                        : Qt.rgba(1, 1, 1, 0.5)
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 3
                    height: 14
                    radius: 1.5
                    color: viewAllMouse.containsMouse
                        ? viewAllRoot.accentColor
                        : Qt.rgba(1, 1, 1, 0.5)
                }
            }

            Item { Layout.preferredHeight: 16 }

            // Native Qt: "+X" count - fontSize 24, bold
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "+" + remainingCount
                font.pixelSize: 24
                font.weight: Font.Bold
                color: viewAllMouse.containsMouse
                    ? viewAllRoot.accentColor
                    : Theme.textPrimary
            }

            // Native Qt: "daha fazla" - fontSize 13, w500
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "daha fazla"
                font.pixelSize: 13
                font.weight: Font.Medium
                color: viewAllMouse.containsMouse
                    ? Qt.rgba(1, 1, 1, 0.8)
                    : Theme.textSecondary
            }

            Item { Layout.preferredHeight: 12 }

            // Native Qt: "Tumunu Gor" button - padding h12 v6, borderRadius 4
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 90
                Layout.preferredHeight: 26
                radius: Dimensions.radiusXS
                color: viewAllMouse.containsMouse
                    ? Theme.withAlpha(viewAllRoot.accentColor, 0.2)
                    : Qt.rgba(1, 1, 1, 0.08)
                border.color: viewAllMouse.containsMouse
                    ? Theme.withAlpha(viewAllRoot.accentColor, 0.4)
                    : Qt.rgba(1, 1, 1, 0.1)
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: "Tumunu Gor"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: viewAllMouse.containsMouse
                        ? viewAllRoot.accentColor
                        : Theme.textSecondary
                }
            }

            Item { Layout.fillHeight: true }
        }

        MouseArea {
            id: viewAllMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewAllRoot.clicked()
        }
    }

    // ===== PROJECT CATEGORY COMPONENT =====
    component ProjectCategory: Rectangle {
        property string title: ""
        property string subtitle: ""
        property color categoryColor: Theme.primary

        radius: 4
        color: Theme.surface
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        implicitHeight: catContent.height

        ColumnLayout {
            id: catContent
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 14

                // Icon container
                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 4
                    color: Theme.withAlpha(categoryColor, 0.15)

                    Label {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAE"
                        font.pixelSize: 22
                    }
                }

                ColumnLayout {
                    spacing: 2

                    Label {
                        text: title
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: "white"
                    }

                    Label {
                        text: subtitle
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            // Project cards area
            Flow {
                Layout.fillWidth: true
                Layout.margins: 16
                spacing: 16

                ProjectCard {
                    title: "Cyberless: Online"
                    description: "Cok oyunculu cyberpunk aksiyon oyunu"
                    status: "Tamamlandi"
                    statusColor: "#4CAF50"
                }

                ProjectCard {
                    title: "Endurance"
                    description: "Hayatta kalma, kaynak yonetimi, korku ve gerilim oyunu"
                    status: "Gelistiriliyor"
                    statusColor: "#9C27B0"
                }
            }
        }
    }

    // ===== PROJECT CARD COMPONENT =====
    component ProjectCard: Rectangle {
        property string title: ""
        property string description: ""
        property string status: ""
        property color statusColor: Theme.primary

        width: 280
        height: 140
        radius: 4
        color: Qt.rgba(1, 1, 1, 0.03)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            // Top row
            RowLayout {
                spacing: 12

                // Icon container
                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 4
                    color: Theme.withAlpha(statusColor, 0.12)

                    Label {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAE"
                        font.pixelSize: 18
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: title
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: "white"
                }
            }

            // Description
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: description
                font.pixelSize: 13
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            // Status badge
            Rectangle {
                Layout.preferredHeight: 26
                Layout.preferredWidth: statusLabel.width + 20
                radius: 4
                color: Theme.withAlpha(statusColor, 0.12)

                Label {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: status
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: statusColor
                }
            }
        }
    }

    // ===== ALL GAMES DIALOG =====
    AllGamesDialog {
        id: allGamesDialog
        parent: Overlay.overlay

        onGameSelected: function(gameId) {
            var gameData = GameService.getGameById(gameId)
            if (gameData) {
                root.gameSelected(gameId, gameData.name, gameData.installPath, gameData.engine || "Unknown")
            }
            close()
        }
    }

    // ===== GAME DETECTOR DIALOG =====
    GameDetectorDialog {
        id: gameDetectorDialog
        parent: Overlay.overlay

        onGameSelected: function(game) {
            // User selected a game - navigate to detail screen
            if (game) {
                root.gameSelected(game.id, game.name, game.installPath || "", game.engine || "Unknown")
            }
        }

        onDialogClosed: {
            // Dialog was closed
        }
    }
}
