import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "screens/detail"

/**
 * GameDetailScreen.qml — Modern cinematic game detail page
 *
 * Layout: Full-width hero with parallax → glass cards below
 * Sections: Hero, Quick Stats, About, Screenshots, Translation Status,
 *           Runtime (Unity), Backup Management
 */
Item {
    id: root

    // ===== INPUT PROPERTIES (set by Main.qml) =====
    property string gameId: ""
    property string gameName: "Game Name"
    property string steamAppId: ""
    property string imageUrl: ""
    property string heroImageUrl: ""
    property bool verified: false
    property string engine: ""
    property bool hasTranslation: false
    property bool isEditorsPick: false
    property string editorsNote: ""

    // ===== STEAM DATA =====
    property string description: ""
    property var developers: []
    property var publishers: []
    property string releaseDate: ""
    property var genres: []
    property int metacriticScore: 0
    property bool hasWindows: true
    property bool hasMac: false
    property bool hasLinux: false
    property string price: ""
    property int discountPercent: 0
    property bool hasSteamDetails: false
    property var screenshots: []
    property bool isLoadingSteamDetails: false
    property bool steamFetchFailed: false

    // ===== CONTRIBUTORS =====
    property var contributors: []  // [{name, role}]

    // ===== RUNTIME (BepInEx) =====
    property bool isUnityGame: false
    property bool runtimeNeeded: false
    property bool runtimeInstalled: false
    property bool runtimeUpToDate: false
    property string bepinexVersion: ""
    property string xunityVersion: ""
    property string unityBackend: ""
    property string unityVersion: ""
    property bool hasAntiCheat: false
    property string antiCheatName: ""
    property bool isInstallingRuntime: false

    // ===== MANUAL GAME FLAG =====
    property bool isManualGame: false

    // ===== GAME INSTALL STATE =====
    property bool isGameInstalled: false     // Game is installed on PC
    property bool packageInstalled: false    // Translation already applied
    property bool autoInstall: false         // Auto-start install on open

    // ===== INSTALL STATE =====
    property bool isInstallingTranslation: false
    property real installProgress: 0
    property string installStatus: ""
    property bool installCompleted: false    // Just finished installing

    // ===== UPDATE IMPACT =====
    property var updateImpact: null  // { level, summary, totalFiles, ... }

    // ===== UI STATE =====
    property bool descriptionExpanded: false
    property real scrollY: 0
    property int _reveal: -1

    signal backClicked()
    signal translateClicked()

    // ===== DATA LOGIC =====

    function resetDetails() {
        description = ""; developers = []; publishers = []
        releaseDate = ""; genres = []; metacriticScore = 0
        hasWindows = true; hasMac = false; hasLinux = false
        price = ""; discountPercent = 0; hasSteamDetails = false
        screenshots = []; isLoadingSteamDetails = false; steamFetchFailed = false
        contributors = []
        isEditorsPick = false; editorsNote = ""
        isUnityGame = false; runtimeNeeded = false; runtimeInstalled = false
        runtimeUpToDate = false; bepinexVersion = ""; xunityVersion = ""
        unityBackend = ""; unityVersion = ""; hasAntiCheat = false
        antiCheatName = ""; isInstallingRuntime = false
        isInstallingTranslation = false; installProgress = 0; installStatus = ""
        installCompleted = false; autoInstall = false
        isGameInstalled = false; packageInstalled = false
        isManualGame = false; updateImpact = null
        descriptionExpanded = false
    }

    function populateSteamDetails(details) {
        description = details.description || ""
        developers = details.developers || []
        publishers = details.publishers || []
        releaseDate = details.releaseDate || ""
        genres = details.genres || []
        metacriticScore = details.metacriticScore || 0
        hasWindows = details.hasWindows !== undefined ? details.hasWindows : true
        hasMac = details.hasMac || false
        hasLinux = details.hasLinux || false
        price = details.price || ""
        discountPercent = details.discountPercent || 0
        screenshots = details.screenshots || []
        if (details.backgroundUrl && details.backgroundUrl !== "")
            heroImageUrl = details.backgroundUrl
        hasSteamDetails = true
        isLoadingSteamDetails = false
    }

    onSteamAppIdChanged: {
        if (steamAppId === "") return
        var cached = GameService.getSteamDetails(steamAppId)
        if (cached && cached.description !== undefined)
            populateSteamDetails(cached)
        else
            isLoadingSteamDetails = true
        GameService.fetchSteamDetails(steamAppId)
    }

    onGameIdChanged: {
        if (gameId === "") return

        var d = GameService.getGameDetails(gameId)

        // Contributors
        contributors = d.contributors || []

        // Unity runtime
        isUnityGame = d.isUnityGame || false
        runtimeNeeded = d.runtimeNeeded || false
        runtimeInstalled = d.runtimeInstalled || false
        runtimeUpToDate = d.runtimeUpToDate || false
        bepinexVersion = d.bepinexVersion || ""
        xunityVersion = d.xunityVersion || ""
        unityBackend = d.unityBackend || "unknown"
        unityVersion = d.unityVersion || ""
        hasAntiCheat = d.hasAntiCheat || false
        antiCheatName = d.antiCheatName || ""

        // Check update impact for installed translations
        if (packageInstalled && GameService.hasGameUpdate(gameId)) {
            updateImpact = GameService.checkUpdateImpact(gameId)
        } else {
            updateImpact = null
        }

        // Reset scroll and start staggered entrance
        mainFlick.contentY = 0
        root._reveal = -1
        _contentSlide.y = 20
        _slideAnim.restart()
        _stagger.restart()
    }

    Connections {
        target: GameService
        function onSteamDetailsFetched(appId, details) {
            if (appId === root.steamAppId) root.populateSteamDetails(details)
        }
        function onSteamDetailsFetchError(appId, error) {
            if (appId === root.steamAppId) {
                root.isLoadingSteamDetails = false
                root.steamFetchFailed = true
            }
        }
        function onRuntimeInstallFinished(gId, success, error) {
            if (gId === root.gameId) {
                root.isInstallingRuntime = false
                if (success) {
                    var rt = GameService.getRuntimeStatus(root.gameId)
                    if (rt) {
                        root.runtimeInstalled = rt.installed || false
                        root.runtimeUpToDate = rt.upToDate || false
                        root.bepinexVersion = rt.bepinexVersion || ""
                        root.xunityVersion = rt.xunityVersion || ""
                    }
                }
            }
        }
        function onTranslationInstallStarted(gId) {
            if (gId === root.gameId) {
                root.isInstallingTranslation = true
                root.installProgress = 0
                root.installStatus = qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallProgress(gId, progress, status) {
            if (gId === root.gameId) {
                root.installProgress = progress
                root.installStatus = status || qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallCompleted(gId, success, message) {
            if (gId === root.gameId) {
                root.isInstallingTranslation = false
                root.installProgress = 0
                root.installStatus = ""
                if (success) {
                    root.installCompleted = true
                    root.packageInstalled = true
                }
            }
        }
    }

    // Auto-install: trigger after brief delay so UI has time to render
    Timer {
        id: autoInstallTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (root.hasTranslation && root.isGameInstalled && !root.packageInstalled && !root.isInstallingTranslation) {
                root.translateClicked()
            }
        }
    }

    onAutoInstallChanged: {
        if (autoInstall && hasTranslation && isGameInstalled && !packageInstalled && !isInstallingTranslation) {
            autoInstallTimer.restart()
        }
    }


    // =========================================================================
    // BACKGROUND — Hero image with parallax + gradient overlay
    // =========================================================================

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary

        Image {
            id: heroBackground
            anchors.fill: parent
            source: root.heroImageUrl !== "" ? root.heroImageUrl : root.imageUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            opacity: 0.30
            visible: source !== ""
            transform: Translate { y: -root.scrollY * 0.15 }
        }

        // Game logo overlay — full-width, fading top-to-bottom
        Image {
            id: gameLogo
            anchors.left: parent.left
            anchors.right: parent.right
            y: 20 - root.scrollY * 0.06
            height: parent.height * 0.6
            fillMode: Image.PreserveAspectFit
            source: root.steamAppId !== "" ? "https://cdn.akamai.steamstatic.com/steam/apps/" + root.steamAppId + "/logo.png" : ""
            asynchronous: true; cache: true
            visible: status === Image.Ready
            opacity: 0
            states: State {
                name: "visible"; when: gameLogo.status === Image.Ready
                PropertyChanges { target: gameLogo; opacity: 0.15 }
            }
            Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
        }

        // Fade mask: logo fades out toward the bottom
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.20; color: Theme.withAlpha(Theme.bgPrimary, 0.30) }
                GradientStop { position: 0.45; color: Theme.withAlpha(Theme.bgPrimary, 0.80) }
                GradientStop { position: 0.65; color: Theme.bgPrimary }
            }
        }

        // Main gradient overlay
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.withAlpha(Theme.bgPrimary, 0.15) }
                GradientStop { position: 0.35; color: Theme.withAlpha(Theme.bgPrimary, 0.75) }
                GradientStop { position: 0.65; color: Theme.bgPrimary }
            }
        }
    }

    // =========================================================================
    // FLOATING BACK BUTTON — bookmark tab style
    // =========================================================================

    Rectangle {
        id: backBtn
        x: 0; y: Dimensions.marginML
        width: backMouse.containsMouse ? 46 : 42
        height: 38
        topLeftRadius: 0; bottomLeftRadius: 0
        topRightRadius: 12; bottomRightRadius: 12
        color: backMouse.containsMouse
            ? Theme.withAlpha(Theme.bgPrimary, 0.82)
            : Theme.withAlpha(Theme.bgPrimary, 0.50)
        border { color: Theme.glassBorder; width: 1 }
        z: Dimensions.zDialog

        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // Hide left border (flush with edge)
        Rectangle {
            x: -1; y: 0; width: 2; height: parent.height
            color: parent.color
        }

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Geri")
        activeFocusOnTab: true
        Keys.onReturnPressed: root.backClicked()
        Keys.onSpacePressed: root.backClicked()

        Text {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: -1
            text: "\u2190"
            font.pixelSize: Dimensions.fontXL
            color: backMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
        }
        MouseArea {
            id: backMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.backClicked()
        }
    }

    // =========================================================================
    // CONTENT ENTRANCE ANIMATION
    // =========================================================================

    // Global slide + per-section staggered fade
    NumberAnimation {
        id: _slideAnim
        target: _contentSlide; property: "y"
        from: 20; to: 0; duration: Dimensions.animSlow
        easing.type: Easing.OutCubic
    }

    Timer {
        id: _stagger
        interval: 80; repeat: true
        onTriggered: {
            root._reveal++
            if (root._reveal >= 6) stop()
        }
    }

    // =========================================================================
    // MAIN CONTENT
    // =========================================================================

    Flickable {
        id: mainFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        onContentYChanged: root.scrollY = contentY

        ScrollBar.vertical: StyledScrollBar {}

        ColumnLayout {
            id: contentCol
            width: mainFlick.width
            spacing: 0
            transform: Translate { id: _contentSlide }

            // =================================================================
            // HERO SECTION
            // =================================================================

            HeroSection {
                opacity: root._reveal >= 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                transform: Scale {
                    origin.x: root.width / 2; origin.y: 200
                    xScale: root._reveal >= 0 ? 1.0 : 1.03
                    yScale: root._reveal >= 0 ? 1.0 : 1.03
                    Behavior on xScale { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
                    Behavior on yScale { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
                }

                gameId: root.gameId
                gameName: root.gameName
                steamAppId: root.steamAppId
                imageUrl: root.imageUrl
                verified: root.verified
                engine: root.engine
                hasTranslation: root.hasTranslation
                isEditorsPick: root.isEditorsPick
                editorsNote: root.editorsNote
                isManualGame: root.isManualGame
                isGameInstalled: root.isGameInstalled
                packageInstalled: root.packageInstalled
                isInstallingTranslation: root.isInstallingTranslation
                installProgress: root.installProgress
                installStatus: root.installStatus
                installCompleted: root.installCompleted
                updateImpact: root.updateImpact

                onTranslateClicked: root.translateClicked()
            }

            // =================================================================
            // UPDATE PROTECTION BANNER
            // =================================================================

            Rectangle {
                id: updateBanner
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
                Layout.topMargin: 56
                visible: root.updateImpact && root.updateImpact.level !== "safe" && root.updateImpact.level !== "unknown"
                implicitHeight: bannerContent.height + Dimensions.marginML * 2
                radius: Dimensions.radiusStandard
                color: root.updateImpact && root.updateImpact.level === "broken"
                    ? Theme.withAlpha(Theme.error, 0.08)
                    : Theme.withAlpha(Theme.warning, 0.08)
                border.color: root.updateImpact && root.updateImpact.level === "broken"
                    ? Theme.withAlpha(Theme.error, 0.25)
                    : Theme.withAlpha(Theme.warning, 0.25)
                border.width: 1

                ColumnLayout {
                    id: bannerContent
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: Dimensions.marginML
                    spacing: Dimensions.spacingLG

                    RowLayout {
                        spacing: Dimensions.spacingLG

                        Text {
                            text: "\u26A0"
                            font.pixelSize: Dimensions.fontTitle
                            color: root.updateImpact && root.updateImpact.level === "broken"
                                ? Theme.error : Theme.warning
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXXS
                            Text {
                                text: root.updateImpact && root.updateImpact.level === "broken"
                                    ? qsTr("Oyun Güncellendi — Çeviri Bozulmuş")
                                    : qsTr("Bazı Çeviri Dosyaları Eksik")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                text: root.updateImpact ? root.updateImpact.summary : ""
                                font.pixelSize: Dimensions.fontCaption
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    RowLayout {
                        spacing: Dimensions.spacingLG

                        // Repair button
                        Rectangle {
                            implicitWidth: repairRow.width + 32; implicitHeight: 38
                            radius: Dimensions.radiusStandard
                            color: repairMouse.containsMouse
                                ? Theme.withAlpha(Theme.accent, 0.20)
                                : Theme.withAlpha(Theme.accent, 0.10)
                            border.color: Theme.withAlpha(Theme.accent, 0.30); border.width: 1
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            Row {
                                id: repairRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                Text { text: "\u2699"; font.pixelSize: Dimensions.fontSM; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("Onar"); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                            }
                            MouseArea {
                                id: repairMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    GameService.recoverTranslation(root.gameId)
                                    root.updateImpact = null
                                }
                            }
                        }
                    }
                }
            }

            // =================================================================
            // QUICK STATS ROW
            // =================================================================

            Item { Layout.preferredHeight: updateBanner.visible ? Dimensions.spacingLG : 56; Layout.fillWidth: true }

            QuickStatsRow {
                opacity: root._reveal >= 1 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 1 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                hasSteamDetails: root.hasSteamDetails
                metacriticScore: root.metacriticScore
                price: root.price
                discountPercent: root.discountPercent
                genres: root.genres
                hasWindows: root.hasWindows
                hasMac: root.hasMac
                hasLinux: root.hasLinux
            }

            // =================================================================
            // ABOUT SECTION
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            AboutSection {
                opacity: root._reveal >= 2 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 2 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                description: root.description
                developers: root.developers
                publishers: root.publishers
                releaseDate: root.releaseDate
                engine: root.engine
                genres: root.genres
                descriptionExpanded: root.descriptionExpanded

                onExpandDescription: root.descriptionExpanded = true
            }

            // =================================================================
            // SCREENSHOTS
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true; visible: root.screenshots.length > 0 }

            ScreenshotCarousel {
                opacity: root._reveal >= 3 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 3 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                screenshots: root.screenshots
            }

            // =================================================================
            // CONTRIBUTORS
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            ContributorsSection {
                opacity: root._reveal >= 4 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 4 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                contributors: root.contributors
            }

            // =================================================================
            // RUNTIME (Unity BepInEx) — conditional
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true; visible: root.isUnityGame && root.runtimeNeeded }

            RuntimeSection {
                opacity: root._reveal >= 5 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 5 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                gameId: root.gameId
                isUnityGame: root.isUnityGame
                runtimeNeeded: root.runtimeNeeded
                runtimeInstalled: root.runtimeInstalled
                runtimeUpToDate: root.runtimeUpToDate
                bepinexVersion: root.bepinexVersion
                xunityVersion: root.xunityVersion
                unityBackend: root.unityBackend
                unityVersion: root.unityVersion
                hasAntiCheat: root.hasAntiCheat
                antiCheatName: root.antiCheatName
                isInstallingRuntime: root.isInstallingRuntime
            }

            // =================================================================
            // BACKUP MANAGEMENT
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

            BackupSection {
                id: backupSection
                opacity: root._reveal >= 6 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } }
                transform: Translate { y: root._reveal >= 6 ? 0 : 18; Behavior on y { NumberAnimation { duration: Dimensions.animSlow; easing.type: Easing.OutCubic } } }

                gameId: root.gameId
                updateImpact: root.updateImpact
            }

            // Bottom spacer
            Item { Layout.preferredHeight: Dimensions.marginXXL; Layout.fillWidth: true }

        } // end contentCol
    } // end Flickable

    // =========================================================================
    // LOADING OVERLAY (Steam details)
    // =========================================================================

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: loadingRow.width + 40; height: 44
        radius: Dimensions.radiusFull
        color: Theme.withAlpha(Theme.surface, 0.92)
        border.color: Theme.glassBorder; border.width: 1
        visible: root.isLoadingSteamDetails && !root.hasSteamDetails
        z: 5

        RowLayout {
            id: loadingRow; anchors.centerIn: parent; spacing: Dimensions.spacingLG
            BusyIndicator { width: 20; height: 20; running: visible }
            Text { text: qsTr("Steam bilgileri yükleniyor..."); font.pixelSize: Dimensions.fontBody; color: Theme.textSecondary }
        }
    }

    // Error + Retry
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: errorCol.width + 40; height: errorCol.height + 24
        radius: Dimensions.radiusStandard
        color: Theme.withAlpha(Theme.surface, 0.92)
        border.color: Theme.glassBorder; border.width: 1
        visible: root.steamFetchFailed && !root.hasSteamDetails
        z: 5

        ColumnLayout {
            id: errorCol; anchors.centerIn: parent; spacing: Dimensions.spacingMD
            Text { text: qsTr("Steam bilgileri alınamadı"); font.pixelSize: Dimensions.fontBody; color: Theme.textMuted; Layout.alignment: Qt.AlignHCenter }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: retryLbl.width + 24; implicitHeight: 30
                radius: Dimensions.radiusStandard
                color: retryMouse.containsMouse ? Theme.primaryHover : Theme.primary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Tekrar Dene")

                Text { id: retryLbl; anchors.centerIn: parent; text: qsTr("Tekrar Dene"); font.pixelSize: Dimensions.fontSM; font.weight: Font.DemiBold; color: Theme.textOnColor }
                MouseArea { id: retryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.steamFetchFailed = false; root.isLoadingSteamDetails = true; GameService.fetchSteamDetails(root.steamAppId) } }
            }
        }
    }

    // =========================================================================
    // FOCUS INDICATOR
    // =========================================================================

    Accessible.role: Accessible.Pane
    Accessible.name: root.gameName
}
