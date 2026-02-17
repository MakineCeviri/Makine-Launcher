import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0
import "../components"
import "../dialogs"

/**
 * TranslationLibraryPage.qml - Library page inspired by store-style layout
 *
 * Layout (GridView with header):
 *   header:
 *     1. Hero carousel — featured game with Turkish translation
 *     2. "Son Eklenenler" — horizontal scrollable GameCard row (translated games)
 *     3. "Tüm Kütüphane" — section header
 *   grid (virtualized):
 *     GameCard delegates — only visible items are instantiated
 */
Item {
    id: translationPage

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)

    // Data sources
    property var translatedGames: GameService.gamesWithTranslation || []
    property var allGames: GameService.games || []

    // Planned localization showcase (always visible)
    readonly property string plannedAppId: "3764200"
    readonly property var plannedGame: {
        // Check installed games first
        var games = GameService.games || []
        for (var i = 0; i < games.length; i++) {
            if ((games[i].steamAppId || "") === plannedAppId)
                return games[i]
        }
        // Fallback: synthetic entry so it always shows
        return {
            id: "steam_" + plannedAppId,
            steamAppId: plannedAppId,
            name: "Resident Evil Requiem",
            headerImageUrl: "https://cdn.akamai.steamstatic.com/steam/apps/" + plannedAppId + "/header.jpg",
            installPath: "",
            engine: ""
        }
    }

    // Hero carousel: planned game first, then translated games
    readonly property var heroGames: {
        var list = [plannedGame]
        var translated = translatedGames || []
        for (var i = 0; i < translated.length; i++) {
            if ((translated[i].steamAppId || "") !== plannedAppId)
                list.push(translated[i])
        }
        return list
    }

    // Hero carousel index
    property int heroIndex: 0
    property int heroCount: heroGames.length

    // Current hero game
    readonly property var heroGame: heroCount > 0 ? heroGames[heroIndex] : null

    // Is current hero the planned game?
    readonly property bool isPlannedHero: heroGame !== null
        && (heroGame.steamAppId || "") === plannedAppId

    // Uninstall dialog
    UninstallConfirmDialog {
        id: uninstallDialog
        parent: Overlay.overlay
        onConfirmed: GameService.uninstallTranslation(uninstallDialog.gameId)
    }

    // Refresh on game changes
    Connections {
        target: GameService
        function onGamesChanged() {
            translationPage.translatedGames = GameService.gamesWithTranslation || []
            translationPage.allGames = GameService.games || []
        }
    }

    // Auto-rotate hero every 8 seconds
    Timer {
        id: heroRotateTimer
        interval: 8000
        repeat: true
        running: translationPage.heroCount > 1 && translationPage.visible
        onTriggered: {
            translationPage.heroIndex = (translationPage.heroIndex + 1) % translationPage.heroCount
        }
    }

    GridView {
        id: libraryGrid
        anchors.fill: parent
        anchors.leftMargin: translationPage.contentMargin
        anchors.rightMargin: translationPage.contentMargin
        clip: true

        readonly property real effectiveCellW: Dimensions.cardWidth + Dimensions.cardGap + 4
        readonly property real effectiveCellH: Dimensions.cardHeight + Dimensions.cardGap + 4

        cellWidth: effectiveCellW
        cellHeight: effectiveCellH
        cacheBuffer: 800
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        displayMarginBeginning: 100
        displayMarginEnd: 100

        model: translationPage.allGames

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "transparent" }
            contentItem: Rectangle {
                implicitWidth: parent.pressed ? 5 : 3; radius: implicitWidth / 2
                color: parent.pressed ? Theme.scrollbarThumbHover
                     : parent.hovered ? Theme.scrollbarThumbHover : Theme.scrollbarThumb
                opacity: parent.active ? 1.0 : 0.0
                Behavior on implicitWidth { NumberAnimation { duration: 120 } }
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // ===== HEADER: Hero + Card Row + Grid Section Title =====
        header: ColumnLayout {
            width: libraryGrid.width
            spacing: 0

            // ===== HERO CAROUSEL =====
            Item {
                id: heroSection
                Layout.fillWidth: true
                Layout.preferredHeight: 340
                Layout.topMargin: translationPage.contentMargin
                visible: translationPage.heroCount > 0
                clip: true

                // Entry animation
                opacity: 0
                transform: Translate { id: heroTranslate; y: 14 }
                Component.onCompleted: heroEntryAnim.start()

                ParallelAnimation {
                    id: heroEntryAnim
                    NumberAnimation {
                        target: heroSection
                        property: "opacity"
                        from: 0; to: 1
                        duration: Dimensions.animSlow
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: heroTranslate
                        property: "y"
                        from: 14; to: 0
                        duration: Dimensions.animSlow
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle {
                    id: heroContainer
                    anchors.fill: parent
                    radius: 20
                    color: Theme.surface
                    clip: true
                    border.color: heroMouseArea.containsMouse
                        ? Theme.withAlpha(Theme.primary, 0.20) : Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    Behavior on border.color { ColorAnimation { duration: 250 } }

                    // Ambient glow (soft purple, bottom-left)
                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cr = 20
                            ctx.beginPath()
                            ctx.moveTo(cr, 0)
                            ctx.lineTo(width - cr, 0)
                            ctx.quadraticCurveTo(width, 0, width, cr)
                            ctx.lineTo(width, height - cr)
                            ctx.quadraticCurveTo(width, height, width - cr, height)
                            ctx.lineTo(cr, height)
                            ctx.quadraticCurveTo(0, height, 0, height - cr)
                            ctx.lineTo(0, cr)
                            ctx.quadraticCurveTo(0, 0, cr, 0)
                            ctx.closePath()
                            ctx.clip()
                            var cx = 40
                            var cy = height - 40
                            var r = Math.max(width, height) * 0.55
                            var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
                            grad.addColorStop(0.0, "rgba(130, 60, 200, 0.10)")
                            grad.addColorStop(0.3, "rgba(120, 50, 190, 0.05)")
                            grad.addColorStop(0.6, "rgba(110, 40, 180, 0.02)")
                            grad.addColorStop(1.0, "rgba(100, 30, 170, 0.0)")
                            ctx.fillStyle = grad
                            ctx.fillRect(0, 0, width, height)
                        }
                    }

                    // Background image
                    Image {
                        id: heroImage
                        anchors.fill: parent
                        source: {
                            if (!translationPage.heroGame) return ""
                            var g = translationPage.heroGame
                            return ImageCache.resolve(
                                g.steamAppId || g.id || "",
                                g.headerImageUrl || ""
                            )
                        }
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                        visible: false

                        Connections {
                            target: ImageCache
                            function onImageReady(readyId) {
                                if (!translationPage.heroGame) return
                                var myId = translationPage.heroGame.steamAppId || translationPage.heroGame.id || ""
                                if (readyId === myId)
                                    heroImage.source = ImageCache.resolve(myId, translationPage.heroGame.headerImageUrl || "")
                            }
                        }
                    }

                    // Mask for rounded corners
                    Item {
                        id: heroMask
                        anchors.fill: parent
                        visible: false
                        layer.enabled: true
                        Rectangle { anchors.fill: parent; radius: 20; color: "white" }
                    }

                    MultiEffect {
                        anchors.fill: heroImage
                        source: heroImage
                        maskEnabled: true
                        maskSource: heroMask
                        visible: heroImage.status === Image.Ready
                    }

                    // Placeholder when no image
                    Rectangle {
                        anchors.fill: parent
                        visible: heroImage.status !== Image.Ready
                        radius: 20
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.primary, 0.15) }
                            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.surface, 0.8) }
                        }
                    }

                    // Dark overlay for text readability
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: Theme.withAlpha("#000000", 0.2) }
                            GradientStop { position: 1.0; color: Theme.withAlpha("#000000", 0.7) }
                        }
                    }

                    // Hero badge (top-right) — glassmorphic
                    Rectangle {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: Dimensions.marginMD
                        anchors.rightMargin: Dimensions.marginMD
                        width: trBadgeRow.width + 16
                        height: 28
                        radius: 10
                        color: translationPage.isPlannedHero
                            ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.08)
                        border.color: translationPage.isPlannedHero
                            ? Theme.withAlpha(Theme.brandCoral, 0.35) : Qt.rgba(1, 1, 1, 0.15)
                        border.width: 1

                        Row {
                            id: trBadgeRow
                            anchors.centerIn: parent
                            spacing: Dimensions.spacingSM

                            TurkishFlagBadge {
                                flagWidth: 18; flagHeight: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: translationPage.isPlannedHero
                                    ? qsTr("Planlanan Yerelleştirme")
                                    : qsTr("TÜRKÇE YAMA")
                                font.pixelSize: Dimensions.fontXS
                                font.weight: Font.Bold
                                color: "white"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    // Game name (bottom-left)
                    Column {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: Dimensions.marginLG
                        anchors.bottomMargin: Dimensions.marginLG
                        anchors.right: parent.right
                        anchors.rightMargin: 100
                        spacing: Dimensions.spacingXS

                        Text {
                            text: translationPage.heroGame ? (translationPage.heroGame.name || "") : ""
                            font.pixelSize: Dimensions.fontHero
                            font.weight: Font.Bold
                            color: "white"
                            width: parent.width
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }

                    // Navigation arrows
                    Rectangle {
                        id: leftArrow
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Dimensions.marginSM
                        width: 32; height: 32; radius: 16
                        color: leftArrowMouse.containsMouse ? Theme.withAlpha("#000000", 0.6) : Theme.withAlpha("#000000", 0.3)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        visible: translationPage.heroCount > 1
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: "\u2039"
                            font.pixelSize: Dimensions.fontXL
                            font.weight: Font.Bold
                            color: "white"
                        }
                        MouseArea {
                            id: leftArrowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                heroRotateTimer.restart()
                                translationPage.heroIndex = (translationPage.heroIndex - 1 + translationPage.heroCount) % translationPage.heroCount
                            }
                        }
                    }

                    Rectangle {
                        id: rightArrow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: Dimensions.marginSM
                        width: 32; height: 32; radius: 16
                        color: rightArrowMouse.containsMouse ? Theme.withAlpha("#000000", 0.6) : Theme.withAlpha("#000000", 0.3)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        visible: translationPage.heroCount > 1
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: "\u203A"
                            font.pixelSize: Dimensions.fontXL
                            font.weight: Font.Bold
                            color: "white"
                        }
                        MouseArea {
                            id: rightArrowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                heroRotateTimer.restart()
                                translationPage.heroIndex = (translationPage.heroIndex + 1) % translationPage.heroCount
                            }
                        }
                    }

                    // Dot indicators
                    Row {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: Dimensions.marginSM
                        spacing: Dimensions.spacingSM
                        visible: translationPage.heroCount > 1

                        Repeater {
                            model: Math.min(translationPage.heroCount, 10)
                            Rectangle {
                                required property int index
                                width: index === translationPage.heroIndex ? 20 : 6
                                height: 6; radius: 3
                                color: index === translationPage.heroIndex ? "white" : Theme.withAlpha("white", 0.4)
                                Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }
                    }

                    // Click to open game detail
                    MouseArea {
                        id: heroMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: -1
                        onClicked: {
                            if (translationPage.heroGame) {
                                var g = translationPage.heroGame
                                translationPage.gameSelected(g.id || "", g.name || "", g.installPath || "", g.engine || "")
                            }
                        }
                    }
                }
            }

            // ===== SON EKLENENLER (Translated Games) =====
            ColumnLayout {
                id: cardRowSection
                Layout.fillWidth: true
                spacing: 0

                // Entry animation (delayed 200ms)
                opacity: 0
                transform: Translate { id: cardRowTranslate; y: 18 }
                Component.onCompleted: cardRowEntryAnim.start()

                SequentialAnimation {
                    id: cardRowEntryAnim
                    PauseAnimation { duration: Dimensions.transitionDuration }
                    ParallelAnimation {
                        NumberAnimation {
                            target: cardRowSection
                            property: "opacity"
                            from: 0; to: 1
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: cardRowTranslate
                            property: "y"
                            from: 18; to: 0
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Item { Layout.preferredHeight: Dimensions.spacingSection }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingMD

                    // Brand gradient dot
                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Theme.brandCoral
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Label {
                        text: qsTr("Türkçe Yamalar")
                        font.pixelSize: Dimensions.fontXL
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    // Count badge — glassmorphic pill
                    Rectangle {
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: recentCountLabel.width + 14
                        radius: 10
                        color: Qt.rgba(1, 1, 1, 0.06)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        Label {
                            id: recentCountLabel
                            anchors.centerIn: parent
                            text: qsTr("%1 yama").arg(translationPage.translatedGames.length)
                            font.pixelSize: Dimensions.fontXS
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                        }
                    }

                    // Scroll arrows for card row
                    Row {
                        spacing: Dimensions.spacingSM
                        visible: translationPage.translatedGames.length > 5

                        Rectangle {
                            width: 28; height: 28; radius: 14
                            color: recentLeftMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                            border.color: Qt.rgba(1, 1, 1, 0.06)
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: "\u2039"
                                font.pixelSize: Dimensions.fontLG
                                color: recentLeftMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            MouseArea {
                                id: recentLeftMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: recentListView.flick(800, 0)
                            }
                        }
                        Rectangle {
                            width: 28; height: 28; radius: 14
                            color: recentRightMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                            border.color: Qt.rgba(1, 1, 1, 0.06)
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: "\u203A"
                                font.pixelSize: Dimensions.fontLG
                                color: recentRightMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            MouseArea {
                                id: recentRightMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: recentListView.flick(-800, 0)
                            }
                        }
                    }
                }

                // Separator line
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: Dimensions.spacingMD
                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                }

                Item { Layout.preferredHeight: Dimensions.spacingMD }

                // Horizontal scrolling card row
                ListView {
                    id: recentListView
                    Layout.fillWidth: true
                    Layout.preferredHeight: Dimensions.cardHeight + 32
                    topMargin: 12
                    orientation: ListView.Horizontal
                    spacing: Dimensions.cardGap
                    clip: false
                    boundsBehavior: Flickable.StopAtBounds
                    model: translationPage.translatedGames
                    cacheBuffer: 600

                    delegate: GameCard {
                        id: recentCardDelegate
                        required property var modelData
                        required property int index

                        gameId: modelData.steamAppId || modelData.id || ""
                        gameName: modelData.name || ""
                        imageUrl: modelData.logoImageUrl || modelData.headerImageUrl || ""
                        translated: true
                        verified: modelData.isVerified || false
                        steamAppId: modelData.steamAppId || ""
                        installPath: modelData.installPath || ""

                        // Staggered fade-in
                        opacity: 0
                        Component.onCompleted: recentCardEntry.start()

                        SequentialAnimation {
                            id: recentCardEntry
                            PauseAnimation { duration: index * 40 }
                            NumberAnimation {
                                target: recentCardDelegate
                                property: "opacity"
                                from: 0; to: 1
                                duration: Dimensions.fadeTransitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }

                        onClicked: {
                            translationPage.gameSelected(
                                modelData.id || "", modelData.name || "",
                                modelData.installPath || "", modelData.engine || ""
                            )
                        }
                    }
                }

                // Empty state for translated games — glassmorphic
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    radius: 14
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    visible: translationPage.translatedGames.length === 0 && !GameService.isScanning

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingMD

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: ":/"
                            font.pixelSize: Dimensions.fontHero
                            color: Theme.textMuted
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Henüz yama bulunamadı")
                            font.pixelSize: Dimensions.fontBody
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Bilgisayarınızda çevirisi olan oyun bulunamadı")
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                        }
                    }
                }

                // Scanning indicator — glassmorphic
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 12
                    color: Qt.rgba(1, 1, 1, 0.04)
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    visible: GameService.isScanning

                    Row {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingMD
                        BusyIndicator { width: 14; height: 14; running: true }
                        Label {
                            text: GameService.scanStatus || qsTr("Kütüphaneler taranıyor...")
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.textMuted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ===== TÜM KÜTÜPHANE — Section Header =====
            ColumnLayout {
                id: gridSectionHeader
                Layout.fillWidth: true
                spacing: 0

                // Entry animation (delayed 400ms)
                opacity: 0
                transform: Translate { id: gridTranslate; y: 18 }
                Component.onCompleted: gridEntryAnim.start()

                SequentialAnimation {
                    id: gridEntryAnim
                    PauseAnimation { duration: Dimensions.animSlow }
                    ParallelAnimation {
                        NumberAnimation {
                            target: gridSectionHeader
                            property: "opacity"
                            from: 0; to: 1
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: gridTranslate
                            property: "y"
                            from: 18; to: 0
                            duration: Dimensions.animSlow
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Item { Layout.preferredHeight: Dimensions.spacingSection }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingMD

                    // Brand gradient dot
                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Theme.brandCoral
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Label {
                        text: qsTr("Tüm Kütüphane")
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

                    // Count badge — glassmorphic pill
                    Rectangle {
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: libCountLabel.width + 14
                        radius: 10
                        color: Qt.rgba(1, 1, 1, 0.06)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        Label {
                            id: libCountLabel
                            anchors.centerIn: parent
                            text: qsTr("%1 oyun").arg(GameService.gameCount)
                            font.pixelSize: Dimensions.fontXS
                            font.weight: Font.Medium
                            color: Theme.textSecondary
                        }
                    }

                    // Rescan button
                    Rectangle {
                        visible: !GameService.isScanning
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        radius: Dimensions.badgeRadius
                        color: libRescanMouse.containsMouse ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        Label {
                            anchors.centerIn: parent
                            text: "\u21BB"
                            font.pixelSize: Dimensions.fontBody
                            color: libRescanMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        }

                        MouseArea {
                            id: libRescanMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: GameService.scanAllLibraries()
                        }

                        ToolTip {
                            visible: libRescanMouse.containsMouse
                            text: qsTr("Kütüphaneleri yeniden tara")
                            delay: 400
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.topMargin: Dimensions.spacingMD
                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                }

                // Library empty state — glassmorphic (shown when no games)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    Layout.topMargin: Dimensions.spacingMD
                    radius: 14
                    color: Qt.rgba(1, 1, 1, 0.03)
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    visible: GameService.gameCount === 0 && !GameService.isScanning

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Dimensions.spacingSM

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Kurulu oyun bulunamadı")
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textSecondary
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Steam, Epic veya GOG kütüphanenizi tarayın")
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                        }
                    }
                }

                Item { Layout.preferredHeight: Dimensions.spacingMD }
            }

            // Spacer before grid items
            Item { Layout.preferredHeight: 8 }
        }

        // ===== GRID DELEGATE (virtualized) =====
        delegate: Item {
            width: libraryGrid.cellWidth
            height: libraryGrid.cellHeight

            required property var modelData
            required property int index

            GameCard {
                anchors.horizontalCenter: parent.horizontalCenter
                gameId: modelData.steamAppId || modelData.id || ""
                gameName: modelData.name || ""
                imageUrl: modelData.headerImageUrl || ""
                translated: modelData.hasTranslation === true
                verified: modelData.isVerified || false
                steamAppId: modelData.steamAppId || ""
                installPath: modelData.installPath || ""

                onClicked: {
                    translationPage.gameSelected(
                        modelData.id || "", modelData.name || "",
                        modelData.installPath || "", modelData.engine || ""
                    )
                }
            }
        }

        footer: Item { width: 1; height: 60 }
    }

    // Bottom fade gradient
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 60; z: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }
    }
}
