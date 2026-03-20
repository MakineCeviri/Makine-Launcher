import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * InstallFlowController.qml - Thin QML wrapper delegating to InstallFlowService (C++)
 *
 * Chain: antiCheat → installNotes → installOptions → variants → install
 * All business logic lives in InstallFlowService C++. This controller
 * forwards signals and maintains dialog loader data properties for Main.qml.
 */
QtObject {
    id: controller

    // ViewModel reference injected from Main.qml
    property var viewModel: null

    // Pending data for lazy-loaded dialogs (read by Main.qml dialog loaders)
    property var pendingAntiCheatData: null
    property var pendingVariantData: null
    property var pendingInstallOptionsData: null

    // Signals to activate dialog loaders in Main.qml
    signal showAntiCheatWarning()
    signal showInstallOptions()
    signal showVariantSelection()

    // ===== C++ service signal connections =====
    property var _serviceConnections: Connections {
        target: InstallFlowService

        function onShowAntiCheatWarning(data) {
            controller.pendingAntiCheatData = data
            controller.showAntiCheatWarning()
        }

        function onShowInstallOptions(data) {
            controller.pendingInstallOptionsData = data
            controller.showInstallOptions()
        }

        function onShowVariantSelection(data) {
            controller.pendingVariantData = data
            controller.showVariantSelection()
        }
    }

    // ===== ENTRY POINTS =====

    function startInstallFlow(gameId, gameName) {
        if (!viewModel) {
            console.warn("InstallFlowController: viewModel not set")
            return
        }
        InstallFlowService.startInstall(gameId, gameName, viewModel.externalUrl || "")
    }

    function startUpdateFlow(gameId, gameName) {
        if (!viewModel) {
            console.warn("InstallFlowController: viewModel not set")
            return
        }
        InstallFlowService.startUpdate(gameId, gameName)
    }

    // ===== DIALOG CALLBACKS =====

    function onAntiCheatContinue() {
        var vm = viewModel
        if (!vm) return
        InstallFlowService.onAntiCheatContinue(vm.gameId, vm.gameName)
    }

    function onOptionsConfirmed(selectedIds) {
        InstallFlowService.onOptionsConfirmed(selectedIds)
        pendingInstallOptionsData = null
    }

    function onVariantSelected(variant) {
        InstallFlowService.onVariantSelected(variant)
        pendingVariantData = null
    }

    function onOptionsCancelled() {
        InstallFlowService.onOptionsCancelled()
        pendingInstallOptionsData = null
    }

    function onVariantCancelled() {
        InstallFlowService.onVariantCancelled()
        pendingVariantData = null
    }

    // ===== DOWNLOAD CALLBACKS (connected from Main.qml) =====

    function onDownloadReady(appId) {
        InstallFlowService.onDownloadReady(appId)
    }

    function onDownloadFailed(appId, error) {
        InstallFlowService.onDownloadFailed(appId, error)
    }

    // ===== PACKAGE DETAIL CALLBACK =====

    function onPackageDetailEnriched(appId) {
        InstallFlowService.onPackageDetailEnriched(appId)
    }

    // ===== EXTERNAL TRIGGER: Anti-cheat warning from GameService signal =====
    function onAntiCheatWarningNeeded(gameId, antiCheatData) {
        var vm = viewModel
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
