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
 * Layout: Single-column Apple-style flow with staggered entry animations
 * Sections: Hero (banner + cover + title + action + tiles + about + contributors),
 *           Runtime (Unity)
 */
Item {
    id: root

    // Single entry point — all game state accessed via viewModel
    required property var viewModel

    // UI-only state (not game data)
    readonly property bool _animEnabled: Dimensions.animSlow > 0

    signal translateClicked()

    // ===== ENTRY ANIMATION =====

    function _replayEntryAnim() {
        _runtimeAnim.stop()

        if (!root._animEnabled) {
            runtimeLoader.opacity = 1; runtimeTranslate.y = 0
            heroSection.replayEntryAnim()
            return
        }

        runtimeLoader.opacity = 0; runtimeTranslate.y = 18

        heroSection.replayEntryAnim()
        _runtimeAnim.start()
    }

    // Runtime section animates independently (550ms delay from hero cascade)
    SequentialAnimation {
        id: _runtimeAnim
        PauseAnimation { duration: 550 }
        ParallelAnimation {
            NumberAnimation { target: runtimeLoader; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
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
        enabled: root.visible
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
                } else {
                    root.viewModel.installErrorMessage = error || qsTr("Çalışma ortamı kurulamadı")
                    installErrorTimer.restart()
                }
            }
        }
        function onTranslationUninstalled(gId, success, message) {
            if (gId !== root.viewModel.gameId) return
            if (success) {
                root.viewModel.packageInstalled = false
                root.viewModel.hasTranslationUpdate = false
                root.viewModel.installErrorMessage = ""
            } else {
                root.viewModel.installErrorMessage = message || qsTr("Yama kaldırılamadı")
                installErrorTimer.restart()
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
                    root.viewModel.hasTranslationUpdate = false
                    root.viewModel.installErrorMessage = ""
                    installSuccessTimer.restart()
                } else {
                    root.viewModel.installErrorMessage = message || qsTr("Yama kurulumu başarısız oldu")
                    installErrorTimer.restart()
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
        function onBackupError(error) {
            root.viewModel.installErrorMessage = error
            installErrorTimer.restart()
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

    // Auto-clear error message after display
    Timer {
        id: installErrorTimer
        interval: 6000
        onTriggered: root.viewModel.installErrorMessage = ""
    }

    // Steam details fetch timeout — reset loading state after 10s
    Timer {
        id: steamDetailTimeout
        interval: 10000
        running: root.viewModel.isLoadingSteamDetails
        onTriggered: {
            if (root.viewModel.isLoadingSteamDetails && !root.viewModel.hasSteamDetails) {
                root.viewModel.isLoadingSteamDetails = false
                root.viewModel.steamFetchFailed = true
            }
        }
    }

    // ===== DOWNLOAD SIGNALS (TranslationDownloader) =====
    Connections {
        target: TranslationDownloader
        enabled: root.visible
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
        function onDownloadRetrying(appId, attempt, maxAttempts) {
            if (appId !== root.viewModel.gameId) return
            root.viewModel.installStatus = qsTr("Bağlantı kesildi, tekrar deneniyor... (%1/%2)").arg(attempt).arg(maxAttempts)
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
            root.viewModel.installErrorMessage = error || qsTr("Indirme basarisiz oldu")
            installErrorTimer.restart()
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

                vm: root.viewModel

                onTranslateClicked: root.translateClicked()
                onUpdateClicked: installFlow.startUpdateFlow(root.viewModel.gameId, root.viewModel.gameName)
                onUninstallClicked: GameService.uninstallTranslation(root.viewModel.gameId)
            }

            // =================================================================
            // UPDATE PROTECTION BANNER
            // =================================================================

            UpdateBanner {
                vm: root.viewModel
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
                    vm: root.viewModel
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: Dimensions.marginLG; Layout.fillWidth: true }

        } // end contentCol
    } // end Flickable

    // =========================================================================
    // LOADING / ERROR OVERLAY
    // =========================================================================

    GameDetailOverlay {
        anchors.fill: parent
        vm: root.viewModel
        z: 5
    }

    // =========================================================================
    // FOCUS INDICATOR
    // =========================================================================

    Accessible.role: Accessible.Pane
    Accessible.name: root.viewModel.gameName
}
