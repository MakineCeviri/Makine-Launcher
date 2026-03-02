import QtQuick
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * InstallFlowController.qml - Manages the install dialog chain
 *
 * Chain: antiCheat → installNotes → installOptions → variants → install
 * This was duplicated in 3 places in Main.qml (GameDetailScreen.onTranslateClicked,
 * antiCheatWarning.onContinueAnyway, installNotes.onAccepted).
 */
QtObject {
    id: controller

    // ViewModel reference injected from Main.qml
    property var viewModel: null

    // Pending data for lazy-loaded dialogs
    property var pendingAntiCheatData: null
    property var pendingVariantData: null
    property var pendingInstallOptionsData: null

    // Pending download state for R2 packages
    property var _pendingDownload: null

    // Pending detail fetch state (waiting for package detail to load)
    property string _pendingDetailGameId: ""
    property string _pendingDetailGameName: ""

    // Signals to activate dialog loaders in Main.qml
    signal showAntiCheatWarning()
    signal showInstallOptions()
    signal showVariantSelection()

    // Get the current ViewModel reference
    function _vm() {
        return viewModel
    }

    // ===== ENTRY POINT: Start the install flow from GameDetailScreen =====
    function startInstallFlow(gameId, gameName) {
        if (!viewModel) {
            console.warn("InstallFlowController: viewModel not set")
            return
        }
        // Pre-flight: Anti-cheat check
        var antiCheat = GameService.checkAntiCheat(gameId)
        if (antiCheat && antiCheat.hasAntiCheat && antiCheat.systems.length > 0) {
            pendingAntiCheatData = {
                gameName: gameName,
                detectedSystems: antiCheat.systems
            }
            showAntiCheatWarning()
            return
        }

        _continueAfterAntiCheat(gameId, gameName)
    }

    // ===== ANTI-CHEAT: User chose to continue =====
    function onAntiCheatContinue() {
        var vm = _vm()
        if (!vm) return
        GameService.acknowledgeAntiCheat(vm.gameId)
        _continueAfterAntiCheat(vm.gameId, vm.gameName)
    }

    function _continueAfterAntiCheat(gameId, gameName) {
        // Pre-flight: Ensure package detail is loaded (for install steps)
        if (!CoreBridge.isPackageDetailLoaded(gameId)) {
            if (!CoreBridge.ensurePackageDetail(gameId)) {
                // Need to fetch from network — save state and wait
                _pendingDetailGameId = gameId
                _pendingDetailGameName = gameName
                ManifestSync.fetchPackageDetail(gameId)
                return
            }
        }
        _continueWithDetail(gameId, gameName)
    }

    function _continueWithDetail(gameId, gameName) {
        // Install notes are shown in AboutSection as "Yama Notları" —
        // skip the blocking dialog and go straight to install options
        _continueAfterNotes(gameId, gameName)
    }

    // Called when package detail arrives from network
    function onPackageDetailEnriched(appId) {
        if (_pendingDetailGameId === appId) {
            var gId = _pendingDetailGameId
            var gName = _pendingDetailGameName
            _pendingDetailGameId = ""
            _pendingDetailGameName = ""
            _continueWithDetail(gId, gName)
        }
    }

    function _continueAfterNotes(gameId, gameName) {
        // Pre-flight: Install options check (checkbox-style)
        var options = GameService.getInstallOptions(gameId)
        if (options && options.length > 0) {
            pendingInstallOptionsData = {
                gameId: gameId,
                options: options,
                specialDialog: GameService.getSpecialDialog(gameId),
                gameName: gameName
            }
            showInstallOptions()
            return
        }

        _continueAfterOptions(gameId, "")
    }

    // ===== INSTALL OPTIONS: User confirmed =====
    function onOptionsConfirmed(selectedIds) {
        if (pendingInstallOptionsData) {
            var variant = pendingInstallOptionsData.variant || ""
            _doInstall(pendingInstallOptionsData.gameId, variant, selectedIds)
            pendingInstallOptionsData = null
        }
    }

    function _continueAfterOptions(gameId, variant) {
        // Pre-flight: Variant check
        var variants = GameService.getVariants(gameId)
        if (variants && variants.length > 0) {
            pendingVariantData = {
                gameId: gameId,
                variants: variants,
                variantType: GameService.getVariantType(gameId)
            }
            showVariantSelection()
            return
        }

        // All checks passed — install (with download gate)
        _doInstall(gameId, "", [])
    }

    // ===== VARIANT SELECTION: User chose a variant =====
    function onVariantSelected(variant) {
        if (!pendingVariantData) return
        var gameId = pendingVariantData.gameId
        pendingVariantData = null

        // Check for variant-specific install options (e.g. GTA III patch/dubbing)
        var variantOptions = GameService.getVariantInstallOptions(gameId, variant)
        if (variantOptions && variantOptions.length > 0) {
            var vm = _vm()
            pendingInstallOptionsData = {
                gameId: gameId,
                options: variantOptions,
                specialDialog: GameService.getVariantSpecialDialog(gameId, variant),
                gameName: vm ? vm.gameName : "",
                variant: variant
            }
            showInstallOptions()
            return
        }

        _doInstall(gameId, variant, [])
    }

    // ===== DOWNLOAD GATE: check local package, download from R2 if needed =====
    function _doInstall(gameId, variant, selectedOptions) {
        _pendingDownload = null  // Clear any previous state
        // If local package already exists, install directly
        if (GameService.hasLocalPackage(gameId)) {
            GameService.installTranslation(gameId, variant, selectedOptions)
            return
        }

        // Check catalog for dataUrl (R2 download)
        var catalog = GameService.getCatalogEntry(gameId)
        var dataUrl = catalog ? (catalog.dataUrl || "") : ""

        if (!dataUrl || dataUrl === "") {
            // No remote package available, try local install anyway
            GameService.installTranslation(gameId, variant, selectedOptions)
            return
        }

        // Resolve dirName: catalog → CoreBridge fallback → gameName
        var dirName = catalog.dirName || ""
        if (!dirName)
            dirName = CoreBridge.getPackageDirName(gameId)
        if (!dirName)
            dirName = (catalog.gameName || catalog.name || gameId)

        // Save pending state and start download
        _pendingDownload = {
            gameId: gameId,
            variant: variant,
            selectedOptions: selectedOptions
        }

        TranslationDownloader.downloadPackage(gameId, dataUrl, dirName)
    }

    // ===== DOWNLOAD CALLBACKS (connected from Main.qml) =====
    function onDownloadReady(appId) {
        if (!_pendingDownload || _pendingDownload.gameId !== appId) return
        var pending = _pendingDownload
        _pendingDownload = null

        // Reload LocalPackageManager so CoreBridge picks up the new package
        CoreBridge.refreshPackageManifest()

        GameService.installTranslation(pending.gameId, pending.variant, pending.selectedOptions)
    }

    function onDownloadFailed(appId, error) {
        if (!_pendingDownload || _pendingDownload.gameId !== appId) return
        _pendingDownload = null
        // Error is shown via GameDetailScreen download connections
    }

    // ===== CANCEL HANDLERS =====
    function onOptionsCancelled() { pendingInstallOptionsData = null }
    function onVariantCancelled() { pendingVariantData = null }

    // ===== EXTERNAL TRIGGER: Anti-cheat warning from GameService signal =====
    function onAntiCheatWarningNeeded(gameId, antiCheatData) {
        var vm = _vm()
        var gameName = vm ? vm.gameName : ""
        pendingAntiCheatData = {
            gameName: gameName,
            detectedSystems: antiCheatData.systems
        }
        showAntiCheatWarning()
    }

    // ===== EXTERNAL TRIGGER: Translation impact =====
    function onTranslationImpactDetected(gameId, gameName, impact) {
        // Handled separately via updateAlertLoader — just a pass-through signal
    }
}
