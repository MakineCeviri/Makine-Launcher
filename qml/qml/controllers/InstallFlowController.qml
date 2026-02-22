import QtQuick
import MakineAI 1.0

/**
 * InstallFlowController.qml - Manages the install dialog chain
 *
 * Chain: antiCheat → installNotes → installOptions → variants → install
 * This was duplicated in 3 places in Main.qml (GameDetailScreen.onTranslateClicked,
 * antiCheatWarning.onContinueAnyway, installNotes.onAccepted).
 */
QtObject {
    id: controller

    // References injected from Main.qml
    property var gameDetailLoader: null

    // Pending data for lazy-loaded dialogs
    property var pendingAntiCheatData: null
    property var pendingVariantData: null
    property var pendingInstallNotes: null
    property var pendingInstallOptionsData: null

    // Signals to activate dialog loaders in Main.qml
    signal showAntiCheatWarning()
    signal showInstallNotes()
    signal showInstallOptions()
    signal showVariantSelection()

    // Get the current game detail screen reference
    function _gd() {
        return gameDetailLoader && gameDetailLoader.item ? gameDetailLoader.item : null
    }

    // ===== ENTRY POINT: Start the install flow from GameDetailScreen =====
    function startInstallFlow(gameId, gameName) {
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
        var gd = _gd()
        if (!gd) return
        GameService.acknowledgeAntiCheat(gd.gameId)
        _continueAfterAntiCheat(gd.gameId, gd.gameName)
    }

    function _continueAfterAntiCheat(gameId, gameName) {
        // Pre-flight: Install notes check
        var notes = GameService.getInstallNotes(gameId)
        if (notes && notes.length > 0) {
            pendingInstallNotes = { gameId: gameId, notes: notes }
            showInstallNotes()
            return
        }

        _continueAfterNotes(gameId, gameName)
    }

    // ===== INSTALL NOTES: User accepted =====
    function onInstallNotesAccepted() {
        var data = pendingInstallNotes
        pendingInstallNotes = null
        if (!data) return
        var gd = _gd()
        var gameName = gd ? gd.gameName : ""
        _continueAfterNotes(data.gameId, gameName)
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
            GameService.installTranslation(pendingInstallOptionsData.gameId, variant, selectedIds)
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

        // All checks passed — install
        GameService.installTranslation(gameId)
    }

    // ===== VARIANT SELECTION: User chose a variant =====
    function onVariantSelected(variant) {
        if (!pendingVariantData) return
        var gameId = pendingVariantData.gameId
        pendingVariantData = null

        // Check for variant-specific install options (e.g. GTA III patch/dubbing)
        var variantOptions = GameService.getVariantInstallOptions(gameId, variant)
        if (variantOptions && variantOptions.length > 0) {
            var gd = _gd()
            pendingInstallOptionsData = {
                gameId: gameId,
                options: variantOptions,
                specialDialog: GameService.getVariantSpecialDialog(gameId, variant),
                gameName: gd ? gd.gameName : "",
                variant: variant
            }
            showInstallOptions()
            return
        }

        GameService.installTranslation(gameId, variant)
    }

    // ===== CANCEL HANDLERS =====
    function onInstallNotesCancelled() { pendingInstallNotes = null }
    function onOptionsCancelled() { pendingInstallOptionsData = null }
    function onVariantCancelled() { pendingVariantData = null }

    // ===== EXTERNAL TRIGGER: Anti-cheat warning from GameService signal =====
    function onAntiCheatWarningNeeded(gameId, antiCheatData) {
        var gd = _gd()
        var gameName = gd ? gd.gameName : ""
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
