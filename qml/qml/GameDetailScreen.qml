import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "screens/detail"
pragma ComponentBehavior: Bound

/**
 * GameDetailScreen.qml — Store-style game detail page
 *
 * Zero-rebinding architecture: all state lives in viewModel (GameDetailViewModel).
 * Screen and children bind to viewModel ONCE — bindings never break on game switch.
 *
 * Layout: Hero banner + two-column (cover + info/action) → section cards below
 * Sections: Hero (banner + cover + action + about + contributors),
 *           Runtime (Unity)
 */
Item {
    id: root

    // Single entry point — all game state accessed via viewModel
    required property var viewModel

    // UI-only state (not game data)
    readonly property bool _animEnabled: Dimensions.animSlow > 0

    signal backClicked()
    signal translateClicked()

    // ===== ENTRY ANIMATION =====

    function _replayEntryAnim() {
        _entryAnim.stop()

        if (!root._animEnabled) {
            heroSection.opacity = 1
            runtimeLoader.opacity = 1; runtimeTranslate.y = 0
            return
        }

        heroSection.opacity = 0
        runtimeLoader.opacity = 0; runtimeTranslate.y = 18

        _entryAnim.start()
    }

    ParallelAnimation {
        id: _entryAnim

        // Hero (includes about + contributors) — 0ms, fade only
        NumberAnimation { target: heroSection; property: "opacity"; from: 0; to: 1; duration: 500; easing.type: Easing.OutCubic }

        // Runtime — 150ms delay
        SequentialAnimation {
            PauseAnimation { duration: 150 }
            NumberAnimation { target: runtimeLoader; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            PauseAnimation { duration: 150 }
            NumberAnimation { target: runtimeTranslate; property: "y"; from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
        }
    }

    // First load: Connections miss the gameId change that happened before
    // the Loader created this component. Catch up here.
    Component.onCompleted: {
        if (root.viewModel.gameId !== "") {
            mainFlick.contentY = 0
            root._replayEntryAnim()
        }
    }

    // ===== VIEWMODEL WATCHERS =====

    Connections {
        target: root.viewModel
        function onGameIdChanged() {
            if (root.viewModel.gameId === "") return
            mainFlick.contentY = 0
            root._replayEntryAnim()
        }
        function onAutoInstallChanged() {
            if (root.viewModel.autoInstall && root.viewModel.hasTranslation &&
                root.viewModel.isGameInstalled && !root.viewModel.packageInstalled &&
                !root.viewModel.isInstallingTranslation) {
                autoInstallTimer.restart()
            }
        }
    }

    // ===== SERVICE CONNECTIONS =====

    Connections {
        target: GameService
        function onSteamDetailsFetched(appId, details) {
            if (appId === root.viewModel.steamAppId)
                root.viewModel.populateSteamDetails(details)
        }
        function onSteamDetailsFetchError(appId, error) {
            if (appId === root.viewModel.steamAppId) {
                root.viewModel.isLoadingSteamDetails = false
                root.viewModel.steamFetchFailed = true
            }
        }
        function onRuntimeInstallFinished(gId, success, error) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.isInstallingRuntime = false
                if (success) {
                    var rt = GameService.getRuntimeStatus(root.viewModel.gameId)
                    if (rt) {
                        root.viewModel.runtimeInstalled = rt.installed || false
                        root.viewModel.runtimeUpToDate = rt.upToDate || false
                        root.viewModel.bepinexVersion = rt.bepinexVersion || ""
                        root.viewModel.xunityVersion = rt.xunityVersion || ""
                    }
                }
            }
        }
        function onTranslationInstallStarted(gId) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.isInstallingTranslation = true
                root.viewModel.installProgress = 0
                root.viewModel.installStatus = qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallProgress(gId, progress, status) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.installProgress = progress
                root.viewModel.installStatus = status || qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallCompleted(gId, success, message) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.isInstallingTranslation = false
                root.viewModel.installProgress = 0
                root.viewModel.installStatus = ""
                if (success) {
                    root.viewModel.installCompleted = true
                    root.viewModel.packageInstalled = true
                    installSuccessTimer.restart()
                }
            }
        }
    }

    // ===== BACKUP RESTORE SIGNAL =====
    Connections {
        target: BackupManager
        function onBackupRestored(gId) {
            if (gId === root.viewModel.gameId)
                root.viewModel.packageInstalled = false
        }
    }

    // ===== IMAGE CACHE (R2 async download complete) =====
    Connections {
        target: ImageCache
        function onImageReady(appId) {
            if (appId === root.viewModel.steamAppId || appId === root.viewModel.gameId)
                root.viewModel.imageUrl = ImageCache.resolve(appId)
        }
    }

    // Brief success indicator before showing uninstall button
    Timer {
        id: installSuccessTimer
        interval: 3000
        onTriggered: root.viewModel.installCompleted = false
    }

    // ===== DOWNLOAD SIGNALS (TranslationDownloader) =====
    Connections {
        target: TranslationDownloader
        function onDownloadProgress(appId, received, total) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.isDownloading = true
            root.viewModel.isInstallingTranslation = true
            if (total > 0) {
                root.viewModel.installProgress = received / total
                var mbReceived = (received / 1048576).toFixed(1)
                var mbTotal = (total / 1048576).toFixed(1)
                root.viewModel.installStatus = qsTr("İndiriliyor... %1 / %2 MB").arg(mbReceived).arg(mbTotal)
            } else {
                root.viewModel.installStatus = qsTr("İndiriliyor...")
            }
        }
        function onExtractionStarted(appId) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.installProgress = 0
            root.viewModel.installStatus = qsTr("Çıkartılıyor...")
        }
        function onPackageReady(appId, dirName) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.isDownloading = false
            root.viewModel.installProgress = 0
            root.viewModel.installStatus = qsTr("Kuruluyor...")
            // Install flow continues via InstallFlowController.onDownloadReady
        }
        function onDownloadError(appId, error) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.isDownloading = false
            root.viewModel.isInstallingTranslation = false
            root.viewModel.installProgress = 0
            root.viewModel.installStatus = ""
        }
        function onDownloadCancelled(appId) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.isDownloading = false
            root.viewModel.isInstallingTranslation = false
            root.viewModel.installProgress = 0
            root.viewModel.installStatus = ""
        }
    }

    // Auto-install: trigger after brief delay so UI has time to render
    Timer {
        id: autoInstallTimer
        interval: 200
        repeat: false
        onTriggered: {
            if (root.viewModel.hasTranslation && root.viewModel.isGameInstalled &&
                !root.viewModel.packageInstalled && !root.viewModel.isInstallingTranslation) {
                root.translateClicked()
            }
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

                vm: root.viewModel

                onTranslateClicked: root.translateClicked()
                onUninstallClicked: GameService.uninstallTranslation(root.viewModel.gameId)
            }

            // =================================================================
            // UPDATE PROTECTION BANNER
            // =================================================================

            Rectangle {
                id: updateBanner
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL; Layout.rightMargin: Dimensions.marginXL
                Layout.topMargin: 56
                visible: root.viewModel.impactLevel !== "" && root.viewModel.impactLevel !== "safe" && root.viewModel.impactLevel !== "unknown"
                implicitHeight: bannerContent.height + Dimensions.marginML * 2
                radius: Dimensions.radiusLG
                color: root.viewModel.impactLevel === "broken"
                    ? Theme.error08
                    : Theme.warning08
                border.color: root.viewModel.impactLevel === "broken"
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
                            color: root.viewModel.impactLevel === "broken"
                                ? Theme.error : Theme.warning
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Dimensions.spacingXXS
                            Text {
                                textFormat: Text.PlainText
                                text: root.viewModel.impactLevel === "broken"
                                    ? qsTr("Oyun Güncellendi — Çeviri Bozulmuş")
                                    : qsTr("Bazı Çeviri Dosyaları Eksik")
                                font.pixelSize: Dimensions.fontBody
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                            }
                            Text {
                                textFormat: Text.PlainText
                                text: root.viewModel.updateImpact ? root.viewModel.updateImpact.summary : ""
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
                                    GameService.recoverTranslation(root.viewModel.gameId)
                                    root.viewModel.updateImpact = null
                                }
                            }
                        }
                    }
                }
            }

            // =================================================================
            // RUNTIME (Unity BepInEx) — lazy loaded, conditional
            // =================================================================

            Item { Layout.preferredHeight: Dimensions.spacingXL; Layout.fillWidth: true; visible: root.viewModel.isUnityGame && root.viewModel.runtimeNeeded }

            Loader {
                id: runtimeLoader
                Layout.fillWidth: true
                Layout.leftMargin: Dimensions.marginXL
                Layout.rightMargin: Dimensions.marginXL
                opacity: 0
                transform: Translate { id: runtimeTranslate; y: 18 }
                active: root.viewModel.isUnityGame && root.viewModel.runtimeNeeded
                sourceComponent: RuntimeSection {
                    gameId: root.viewModel.gameId
                    isUnityGame: root.viewModel.isUnityGame
                    runtimeNeeded: root.viewModel.runtimeNeeded
                    runtimeInstalled: root.viewModel.runtimeInstalled
                    runtimeUpToDate: root.viewModel.runtimeUpToDate
                    bepinexVersion: root.viewModel.bepinexVersion
                    xunityVersion: root.viewModel.xunityVersion
                    unityBackend: root.viewModel.unityBackend
                    unityVersion: root.viewModel.unityVersion
                    hasAntiCheat: root.viewModel.hasAntiCheat
                    antiCheatName: root.viewModel.antiCheatName
                    isInstallingRuntime: root.viewModel.isInstallingRuntime
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
        visible: root.viewModel.isLoadingSteamDetails && !root.viewModel.hasSteamDetails
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
        visible: root.viewModel.steamFetchFailed && !root.viewModel.hasSteamDetails
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
                MouseArea { id: retryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.viewModel.steamFetchFailed = false; root.viewModel.isLoadingSteamDetails = true; GameService.fetchSteamDetails(root.viewModel.steamAppId) } }
            }
        }
    }

    // =========================================================================
    // FOCUS INDICATOR
    // =========================================================================

    Accessible.role: Accessible.Pane
    Accessible.name: root.viewModel.gameName
}
