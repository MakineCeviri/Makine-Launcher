import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "screens/detail"
pragma ComponentBehavior: Bound

/**
 * GameDetailScreen.qml — Store-style game detail page
 *
 * Layout: Hero banner + two-column (cover + info/action) → section cards below
 * Sections: Hero (banner + cover + action + screenshots), About,
 *           Contributors, Runtime (Unity), Backup Management
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

    // ===== DOWNLOAD STATE =====
    property bool isDownloading: false

    // ===== UPDATE IMPACT =====
    property var updateImpact: null  // { level, summary, totalFiles, ... }
    readonly property string _impactLevel: updateImpact ? updateImpact.level : ""

    // ===== UI STATE =====
    property bool descriptionExpanded: false
    property bool _initialComplete: false
    readonly property bool _animEnabled: Dimensions.animSlow > 0

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
        installCompleted = false; autoInstall = false; isDownloading = false
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

    // ===== ENTRY ANIMATION =====

    function _replayEntryAnim() {
        _entryAnim.stop()

        if (!root._animEnabled) {
            // Animations disabled — show everything instantly
            heroSection.opacity = 1
            aboutContainer.opacity = 1; aboutTranslate.y = 0
            contributorsLoader.opacity = 1; contributorsTranslate.y = 0
            runtimeLoader.opacity = 1; runtimeTranslate.y = 0
            backupLoader.opacity = 1; backupTranslate.y = 0
            return
        }

        // Reset all sections to initial state
        heroSection.opacity = 0
        aboutContainer.opacity = 0; aboutTranslate.y = 18
        contributorsLoader.opacity = 0; contributorsTranslate.y = 18
        runtimeLoader.opacity = 0; runtimeTranslate.y = 18
        backupLoader.opacity = 0; backupTranslate.y = 18

        _entryAnim.start()
    }

    ParallelAnimation {
        id: _entryAnim

        // Hero — 0ms, fade only
        NumberAnimation { target: heroSection; property: "opacity"; from: 0; to: 1; duration: 500; easing.type: Easing.OutCubic }

        // About — 150ms delay
        SequentialAnimation {
            PauseAnimation { duration: 150 }
            NumberAnimation { target: aboutContainer; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 150 }
            NumberAnimation { target: aboutTranslate; property: "y"; from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }

        // Contributors — 260ms delay
        SequentialAnimation {
            PauseAnimation { duration: 260 }
            NumberAnimation { target: contributorsLoader; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 260 }
            NumberAnimation { target: contributorsTranslate; property: "y"; from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }

        // Runtime — 370ms delay
        SequentialAnimation {
            PauseAnimation { duration: 370 }
            NumberAnimation { target: runtimeLoader; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 370 }
            NumberAnimation { target: runtimeTranslate; property: "y"; from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }

        // Backup — 480ms delay
        SequentialAnimation {
            PauseAnimation { duration: 480 }
            NumberAnimation { target: backupLoader; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 480 }
            NumberAnimation { target: backupTranslate; property: "y"; from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
    }

    // ===== SIGNALS & CONNECTIONS =====

    onSteamAppIdChanged: {
        if (steamAppId === "") return
        var cached = GameService.getSteamDetails(steamAppId)
        if (cached && cached.description !== undefined)
            populateSteamDetails(cached)
        else
            isLoadingSteamDetails = true
        GameService.fetchSteamDetails(steamAppId)

        // Pre-fetch package detail so install doesn't wait
        if (!CoreBridge.isPackageDetailLoaded(steamAppId))
            ManifestSync.fetchPackageDetail(steamAppId)
    }

    onGameIdChanged: {
        if (gameId === "") return

        if (typeof SceneProfiler !== "undefined")
            SceneProfiler.screenLoaded("GameDetail")

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

        // Reset scroll and play entry animation
        mainFlick.contentY = 0
        _replayEntryAnim()
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

    // ===== DOWNLOAD SIGNALS (TranslationDownloader) =====
    Connections {
        target: TranslationDownloader
        function onDownloadProgress(appId, received, total) {
            if (appId !== root.gameId) return
            root.isDownloading = true
            root.isInstallingTranslation = true
            if (total > 0) {
                root.installProgress = received / total
                var mbReceived = (received / 1048576).toFixed(1)
                var mbTotal = (total / 1048576).toFixed(1)
                root.installStatus = qsTr("İndiriliyor... %1 / %2 MB").arg(mbReceived).arg(mbTotal)
            } else {
                root.installStatus = qsTr("İndiriliyor...")
            }
        }
        function onExtractionStarted(appId) {
            if (appId !== root.gameId) return
            root.installProgress = 0
            root.installStatus = qsTr("Çıkartılıyor...")
        }
        function onPackageReady(appId, dirName) {
            if (appId !== root.gameId) return
            root.isDownloading = false
            root.installProgress = 0
            root.installStatus = qsTr("Kuruluyor...")
            // Install flow continues via InstallFlowController.onDownloadReady
        }
        function onDownloadError(appId, error) {
            if (appId !== root.gameId) return
            root.isDownloading = false
            root.isInstallingTranslation = false
            root.installProgress = 0
            root.installStatus = ""
        }
        function onDownloadCancelled(appId) {
            if (appId !== root.gameId) return
            root.isDownloading = false
            root.isInstallingTranslation = false
            root.installProgress = 0
            root.installStatus = ""
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
    // BACKGROUND — Solid color
    // =========================================================================

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary
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
            ? Theme.bgPrimary82
            : Theme.bgPrimary50
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
            textFormat: Text.PlainText
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
    // MAIN CONTENT
    // =========================================================================

    Flickable {
        id: mainFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: StyledScrollBar {}

        ColumnLayout {
            id: contentCol
            width: mainFlick.width
            spacing: 0

            // =================================================================
            // HERO SECTION
            // =================================================================

            HeroSection {
                id: heroSection
                opacity: 0

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
                isDownloading: root.isDownloading
                updateImpact: root.updateImpact
                screenshots: root.screenshots

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
                visible: root._impactLevel !== "" && root._impactLevel !== "safe" && root._impactLevel !== "unknown"
                implicitHeight: bannerContent.height + Dimensions.marginML * 2
                radius: Dimensions.radiusLG
                color: root._impactLevel === "broken"
                    ? Theme.error08
                    : Theme.warning08
                border.color: root._impactLevel === "broken"
                    ? Theme.error25
                    : Theme.warning25
                border.width: 1

                ColumnLayout {
                    id: bannerContent
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: Dimensions.marginML
                    spacing: Dimensions.spacingLG

                    RowLayout {
                        spacing: Dimensions.spacingLG

                        Text {
                            textFormat: Text.PlainText
                            text: "\u26A0"
                            font.pixelSize: Dimensions.fontTitle
                            color: root._impactLevel === "broken"
                                ? Theme.error : Theme.warning
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXXS
                            Text {
                                textFormat: Text.PlainText
                                text: root._impactLevel === "broken"
                                    ? qsTr("Oyun Güncellendi — Çeviri Bozulmuş")
                                    : qsTr("Bazı Çeviri Dosyaları Eksik")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                textFormat: Text.PlainText
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
                                ? Theme.accent20
                                : Theme.accent10
                            border.color: Theme.accent30; border.width: 1
                            Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                            Row {
                                id: repairRow; anchors.centerIn: parent; spacing: Dimensions.spacingMD
                                Text {
                                    textFormat: Text.PlainText
                                    text: "\u2699"
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.accent
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    text: qsTr("Onar")
                                    font.pixelSize: Dimensions.fontSM
                                    font.weight: Font.DemiBold
                                    color: Theme.accent
                                    anchors.verticalCenter: parent.verticalCenter
                                }
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
            // ABOUT SECTION (glass container)
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.spacingLG; Layout.fillWidth: true }

            Rectangle {
                id: aboutContainer
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                implicitHeight: aboutSection.height + Dimensions.paddingXL * 2
                radius: Dimensions.radiusLG
                color: Theme.textPrimary03
                border.color: Theme.glassBorder; border.width: 1
                opacity: 0
                transform: Translate { id: aboutTranslate; y: 18 }

                AboutSection {
                    id: aboutSection
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Dimensions.paddingXL

                    description: root.description
                    developers: root.developers
                    publishers: root.publishers
                    releaseDate: root.releaseDate
                    engine: root.engine
                    genres: root.genres
                    descriptionExpanded: root.descriptionExpanded

                    onExpandDescription: root.descriptionExpanded = true
                }
            }

            // =================================================================
            // CONTRIBUTORS — lazy loaded
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.spacingLG; Layout.fillWidth: true }

            Loader {
                id: contributorsLoader
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                opacity: 0
                transform: Translate { id: contributorsTranslate; y: 18 }
                active: true
                sourceComponent: ContributorsSection {
                    contributors: root.contributors
                }
            }

            // =================================================================
            // RUNTIME (Unity BepInEx) — lazy loaded, conditional
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.spacingLG; Layout.fillWidth: true; visible: root.isUnityGame && root.runtimeNeeded }

            Loader {
                id: runtimeLoader
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                opacity: 0
                transform: Translate { id: runtimeTranslate; y: 18 }
                active: root.isUnityGame && root.runtimeNeeded
                sourceComponent: RuntimeSection {
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
            }

            // =================================================================
            // BACKUP MANAGEMENT — lazy loaded
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.spacingLG; Layout.fillWidth: true }

            Loader {
                id: backupLoader
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                opacity: 0
                transform: Translate { id: backupTranslate; y: 18 }
                active: true
                sourceComponent: BackupSection {
                    gameId: root.gameId
                    updateImpact: root.updateImpact
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

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
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: root.isLoadingSteamDetails && !root.hasSteamDetails
        z: 5

        RowLayout {
            id: loadingRow; anchors.centerIn: parent; spacing: Dimensions.spacingLG
            BusyIndicator { width: 20; height: 20; running: visible }
            Text {
                textFormat: Text.PlainText
                text: qsTr("Steam bilgileri yükleniyor...")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textSecondary
            }
        }
    }

    // Error + Retry
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.45
        width: errorCol.width + 40; height: errorCol.height + 24
        radius: Dimensions.radiusStandard
        color: Theme.surface92
        border.color: Theme.glassBorder; border.width: 1
        visible: root.steamFetchFailed && !root.hasSteamDetails
        z: 5

        ColumnLayout {
            id: errorCol; anchors.centerIn: parent; spacing: Dimensions.spacingMD
            Text {
                textFormat: Text.PlainText
                text: qsTr("Steam bilgileri alınamadı")
                font.pixelSize: Dimensions.fontBody
                color: Theme.textMuted
                Layout.alignment: Qt.AlignHCenter
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: retryLbl.width + 24; implicitHeight: 30
                radius: Dimensions.radiusStandard
                color: retryMouse.containsMouse ? Theme.primaryHover : Theme.primary
                Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Tekrar Dene")

                Text {
                    textFormat: Text.PlainText
                    id: retryLbl
                    anchors.centerIn: parent
                    text: qsTr("Tekrar Dene")
                    font.pixelSize: Dimensions.fontSM
                    font.weight: Font.DemiBold
                    color: Theme.textOnColor
                }
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
