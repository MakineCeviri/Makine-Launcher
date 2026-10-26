// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "screens/detail"
import "components"
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
    // The page the back button returns to, its label
    property string backTitle: qsTr("Geri")

    // UI-only state (not game data)
    readonly property bool _animEnabled: Dimensions.animSlow > 0

    // Last GameService.getPostInstallGuidance() result — what the user still
    // has to do, read off the files the install actually wrote.
    property var _postInstall: ({})
    // Game whose post-install steps arrived while its page was not showing
    property string _pendingNotesGameId: ""

    signal translateClicked()
    signal backClicked()


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
            root._verifyInstalledPatch()
            if (root.viewModel.autoInstall)
                autoInstallTimer.restart()
        }
    }

    onVisibleChanged: _showPendingNotes()

    // The last install step is often the user's own; it waits until they are
    // back on that game's page rather than popping up over another one.
    function _showPendingNotes() {
        if (!root.visible || root._pendingNotesGameId === ""
            || root._pendingNotesGameId !== root.viewModel.gameId) return
        root._pendingNotesGameId = ""
        installNotesLoader.active = true
    }

    // A patch recorded as installed can be gone from disk without the launcher
    // knowing: a store integrity check deletes it, a game update overwrites it,
    // antivirus quarantines it hours later. The library keeps saying "installed"
    // while the game runs untranslated. Check when the user opens the page —
    // that is when they are looking for the answer.
    function _verifyInstalledPatch() {
        if (!root.viewModel.packageInstalled) return
        GameService.checkPatchIntegrity(root.viewModel.gameId)
    }

    // ===== VIEWMODEL WATCHERS =====

    Connections {
        target: root.viewModel
        function onGameIdChanged() {
            if (root.viewModel.gameId === "") return
            mainFlick.contentY = 0
            root._replayEntryAnim()
            root._verifyInstalledPatch()
            root._showPendingNotes()
        }
        // An install, or the update of an installed patch ("Güncelle" in the
        // library opens the page with autoInstall set). Decided when the timer
        // fires: loadGame sets autoInstall before the update check has run.
        function onAutoInstallChanged() {
            if (root.viewModel.autoInstall)
                autoInstallTimer.restart()
        }
    }

    // ===== SERVICE CONNECTIONS =====

    // Not gated on visibility: an install the user walked away from still
    // ends, and its result has to land.
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
                } else {
                    root.viewModel.installErrorMessage = error || qsTr("Çalışma ortamı kurulamadı")
                    installErrorTimer.restart()
                }
            }
        }
        function onTranslationUninstalled(gId, success, message) {
            if (gId !== root.viewModel.gameId) return
            root.viewModel.isUndoing = false
            if (success) {
                root.viewModel.packageInstalled = false
                root.viewModel.hasTranslationUpdate = false
                root.viewModel.installErrorMessage = ""
            } else {
                // Deliberately NOT auto-cleared. A failed uninstall leaves the
                // game patched with no way back, and the message carries the
                // only recovery there is — the "Oyun dosyalarını doğrula / onar"
                // button, which is bound to this very string being non-empty.
                // Fading both after six seconds is why twelve users pressed
                // Kaldır 368 times: the way out disappeared while they read it.
                // Cleared on a successful install/uninstall, when repair starts,
                // and when another game is opened.
                root.viewModel.installErrorMessage = message || qsTr("Yama kaldırılamadı")
            }
        }
        function onPatchIntegrityChecked(gId, ok, message) {
            if (gId !== root.viewModel.gameId || ok) return
            // Recorded files are gone from disk. Surface it as a "lost" update
            // impact so the action button flips to the amber "Onar" state —
            // reusing the same repair path as post-update breakage, instead of
            // a transient banner over a misleading "Kurulu" button.
            root.viewModel.updateImpact = { "level": "lost", "summary": message }
        }
        function onGameRepairStarted(gId, started, message) {
            if (gId !== root.viewModel.gameId) return
            // Reuse the error banner: it is the surface the user is already
            // looking at when they reach for repair, and it auto-clears.
            root.viewModel.installErrorMessage = message
            // Steam takes over when the check started; otherwise the message
            // lists the store's steps by hand and has to stay to be followed.
            if (started) installErrorTimer.restart()
            else installErrorTimer.stop()
        }
        function onTranslationInstallStarted(gId) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.installErrorMessage = ""
                root.viewModel.isInstallingTranslation = true
                root.viewModel.installProgress = 0
                root.viewModel.installStatus = qsTr("Kuruluyor...")
                root.viewModel.installConsole = ""
            }
        }
        function onTranslationInstallProgress(gId, progress, status) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.installProgress = progress
                root.viewModel.installStatus = status || qsTr("Kuruluyor...")
            }
        }
        function onTranslationInstallOutput(gId) {
            if (gId === root.viewModel.gameId) {
                root.viewModel.installConsole = GameService.installConsole(gId)
                root.viewModel.isUndoing = GameService.isUndoing(gId)
            }
        }
        // The folder is another version of the game than the patch was made
        // on, or not the game at all (OyunBul's fingerprint): nothing was
        // touched, and the player decides. The banner keeps the same text.
        function onInstallNeedsConfirmation(gId, info) {
            if (gId !== root.viewModel.gameId) return
            printConfirm.kind = info.kind || ""
            printConfirm.title = info.title || ""
            printConfirm.message = info.message || ""
            printConfirm.open()
        }
        function onTranslationInstallCompleted(gId, success, message) {
            // Reported from the field as "yama kurulu görünüyor ama oyun hâlâ
            // İngilizce". The last install step is often the user's: switch
            // the game language, enable mods, add a launch option.
            // GameService derives it from the files that actually landed
            // rather than from the catalogue note, which for Far Cry 6 named a
            // language the game does not have — and stays quiet when there is
            // nothing to do.
            if (success) {
                var guidance = GameService.getPostInstallGuidance(gId)
                if (guidance && guidance.actionRequired) {
                    root._postInstall = guidance
                    root._pendingNotesGameId = gId
                    root._showPendingNotes()
                }
            }
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
                    // Not auto-cleared either, for the uninstall reason above:
                    // the message is the fix (close the game, say "Evet" to the
                    // prompt, a folder to open) and takes longer than six
                    // seconds to read. Cleared when the next install starts.
                    installErrorTimer.stop()
                    root.viewModel.installErrorOnFolder = GameService.installFailedOnFolder(gId)
                    root.viewModel.installErrorMessage = message || qsTr("Yama kurulumu başarısız oldu")
                    // The rollback's restore already set this false (onBackupRestored),
                    // yet a record can stay — files it could not delete, the version
                    // an update went over — and then "Kaldır" is the way out.
                    root.viewModel.packageInstalled = GameService.getGameById(gId).packageInstalled === true
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

    ConfirmDialog {
        id: uninstallConfirm
        parent: Overlay.overlay
        width: 460
        title: qsTr("Türkçe yama kaldırılsın mı?")
        message: root.viewModel.patchProgram === ""
            ? qsTr("%1 için kurulu Türkçe yama kaldırılacak; yedeklenen orijinal oyun dosyaları geri yüklenecek. İstediğin zaman yeniden kurabilirsin.")
                  .arg(root.viewModel.gameName)
            : root.viewModel.patchUndoes
            ? qsTr("%1 için kurulu Türkçe yama kaldırılacak: yamanın kendi programı açılıp oyunda yaptıklarını geri alacak. İstediğin zaman yeniden kurabilirsin.")
                  .arg(root.viewModel.gameName)
            : qsTr("%1 için kurulu Türkçe yama kaldırılacak. Yamanın programı yaptıklarını geri almıyor; değiştirdiği oyun dosyaları mağazanın dosya onarımıyla geri gelir. İstediğin zaman yeniden kurabilirsin.")
                  .arg(root.viewModel.gameName)
        confirmText: qsTr("Yamayı kaldır")
        glyph: Icons.trash2
        onConfirmed: GameService.uninstallTranslation(root.viewModel.gameId)
    }

    ConfirmDialog {
        id: printConfirm
        property string kind: ""
        parent: Overlay.overlay
        width: 480
        accentColor: Theme.warning
        glyph: printConfirm.kind === "folder" ? Icons.folder : Icons.triangleAlert
        confirmText: qsTr("Yine de kur")
        onConfirmed: GameService.installAnyway(root.viewModel.gameId)
    }

    // ===== POST-INSTALL NOTES DIALOG (lazy) =====
    Loader {
        id: installNotesLoader
        active: false
        sourceComponent: Component {
            InstallNotesDialog {
                parent: Overlay.overlay
                message: root._postInstall.notes || ""
                languageSlot: root._postInstall.languageSlot || ""
                writtenFiles: root._postInstall.files || []
                onClosed: installNotesLoader.active = false
                Component.onCompleted: open()
            }
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
            root.viewModel.installErrorMessage = error || qsTr("İndirme başarısız oldu")
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
            if (!root.viewModel.hasTranslation || !root.viewModel.isGameInstalled
                || root.viewModel.isInstallingTranslation || root.viewModel.externalUrl)
                return
            if (!root.viewModel.packageInstalled)
                root.translateClicked()
            else if (root.viewModel.hasTranslationUpdate)
                installFlow.startUpdateFlow(root.viewModel.gameId, root.viewModel.gameName)
        }
    }


    // No background of its own: the lit page (AppBackground) shows through.

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

        // Beside the page, not in it: TopDissolve masks the page's top while
        // it is scrolled, and the bar would fade there with it.
        ScrollBar.vertical: StyledScrollBar {
            parent: mainFlick.parent
            anchors.top: mainFlick.top; anchors.right: mainFlick.right; anchors.bottom: mainFlick.bottom
        }

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
                // The installed button turns into "Kaldır" under the pointer; a
                // click meant for the page took the patch off without a word.
                onUninstallClicked: uninstallConfirm.open()
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
                Layout.maximumWidth: Dimensions.contentMaxWidth
                Layout.alignment: Qt.AlignHCenter
                opacity: 0
                transform: Translate { id: runtimeTranslate; y: 18 }
                active: root.viewModel.isUnityGame && root.viewModel.runtimeNeeded
                sourceComponent: RuntimeSection {
                    vm: root.viewModel
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 40; Layout.fillWidth: true }

        } // end contentCol
    } // end Flickable

    // =========================================================================
    // A GAME FOLDER DROPPED ON THE PAGE — same as picking it (GameFolderBar)
    // =========================================================================

    FolderDropOverlay {
        anchors.fill: parent
        z: 200
        accepts: root.viewModel.hasTranslation && root.viewModel.externalUrl === ""
        title: qsTr("%1 klasörünü bırak").arg(root.viewModel.gameName)
        subtitle: qsTr("Oyunun klasörü, .exe dosyası ya da içindeki herhangi bir klasör olabilir; yamanın gideceği yeri biz buluruz.")
        onFolderDropped: (url) => heroSection.inspectFolder(url)
    }

    // =========================================================================
    // FOCUS INDICATOR
    // =========================================================================

    Accessible.role: Accessible.Pane
    Accessible.name: root.viewModel.gameName

    // =========================================================================
    // BACK — a toolbar button over the page's top strip
    // =========================================================================

    // On the hero's left edge, in the middle of the gap above it
    GlassButton {
        x: Math.round((root.width - heroSection._width) / 2)
        y: Math.round((heroSection.topGap - height) / 2)
        z: 100
        backdrop: mainFlick
        // Opens under the pointer to where it leads; no tooltip on top of that
        text: root.backTitle
        onClicked: root.backClicked()
    }

    // A scrolled page sinks into the background under the top strip
    TopDissolve {
        flick: mainFlick
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
}
