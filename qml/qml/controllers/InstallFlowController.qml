// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

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
        // A game with several patches installs the one picked on its page
        if (viewModel.gameId === gameId && viewModel.hasAlternatives) {
            InstallFlowService.startInstallPackage(gameId, gameName, viewModel.selectedPkg)
            return
        }
        InstallFlowService.startInstall(gameId, gameName)
    }

    // One of the game's patches ("" = the default)
    function startInstallPackageFlow(gameId, gameName, pkg) {
        InstallFlowService.startInstallPackage(gameId, gameName, pkg)
    }

    // The installed patch out, the other one in
    function startSwitchFlow(gameId, gameName, pkg) {
        InstallFlowService.startSwitchPackage(gameId, gameName, pkg)
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
}
