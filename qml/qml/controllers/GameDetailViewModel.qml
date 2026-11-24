// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * GameDetailViewModel.qml - Single source of truth for game detail state
 *
 * Zero-rebinding architecture: Screen and children bind to this object ONCE.
 * On game switch, only ViewModel properties update — bindings never break.
 */
QtObject {
    id: vm

    // ===== METADATA (from GameDataResolver) =====
    property string gameId: ""
    property string gameName: ""
    property string steamAppId: ""
    property string imageUrl: ""
    property string heroImageUrl: ""
    property bool verified: false
    property string engine: ""
    property bool hasTranslation: false
    property bool isEditorsPick: false
    property string editorsNote: ""
    property bool isManualGame: false
    property bool comingSoon: false  // no package of ours yet ("Yakında")

    // ===== GAME INSTALL STATE =====
    property bool isGameInstalled: false
    property bool packageInstalled: false
    property bool hasTranslationUpdate: false
    property bool autoInstall: false
    property bool fromLibrary: false

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
    property bool isLoadingSteamDetails: false
    property bool steamFetchFailed: false

    // ===== CONTRIBUTORS =====
    property var contributors: []
    property string translationNotes: ""

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

    // ===== INSTALL STATE =====
    property bool isInstallingTranslation: false
    property real installProgress: 0
    property string installStatus: ""
    property bool installCompleted: false
    property string installErrorMessage: ""
    // Output of the patch's own program (GameService.installConsole)
    property string installConsole: ""
    // A picked game folder is being looked at (GameFolderBar)
    property bool folderChecking: false
    // The patch's own installer program, "" when the launcher copies the patch
    property string patchProgram: ""
    // ...and whether it also takes the patch out again
    property bool patchUndoes: false
    // An uninstall is running that program's undo (its console takes answers)
    property bool isUndoing: false
    // The last install stopped because the folder is not the game's
    property bool installErrorOnFolder: false

    // ===== DOWNLOAD STATE =====
    property bool isDownloading: false

    // ===== UPDATE IMPACT =====
    property var updateImpact: null

    // ===== PATCHES (alternatives) + GAME VERSION =====
    // GameService.patchOptions: {anyInstalled, installedPkg, current: {build,
    // source, date}, options: [{pkg, label, description, contributors, version,
    // sizeBytes, downloadSize, gameVersion, installed, action, comingSoon, warn}]}
    property var patchInfo: ({})
    // The patch picked on the page ("" = the default); follows the installed one
    property string selectedPkg: ""

    // ===== UI STATE =====
    property bool descriptionExpanded: false

    // ===== DERIVED =====
    readonly property string impactLevel: updateImpact ? updateImpact.level : ""
    readonly property int progressPercent: Math.round(installProgress * 100)

    // Pre-computed Steam CDN URLs — delegates to GameService C++ (no JS string concat)
    readonly property string heroUrl: {
        var url = GameService.steamHeroUrl(steamAppId)
        return url !== "" ? url : heroImageUrl
    }
    readonly property string coverUrl: imageUrl !== "" ? imageUrl : GameService.steamCoverUrl(steamAppId)
    readonly property string logoUrl: GameService.steamLogoUrl(steamAppId)

    // Pre-joined detail strings — eliminates .join() in view bindings
    readonly property string developersText: developers.join(", ")
    readonly property string publishersText: publishers.join(", ")
    readonly property string genresText: genres.join(", ")

    // Patches: the default first, then the alternatives in catalogue order
    readonly property var patchOptions: patchInfo && patchInfo.options ? patchInfo.options : []
    readonly property bool hasAlternatives: patchOptions.length > 1
    readonly property var selectedPatch: {
        for (let i = 0; i < patchOptions.length; ++i)
            if (patchOptions[i].pkg === selectedPkg) return patchOptions[i]
        return patchOptions.length > 0 ? patchOptions[0] : null
    }
    readonly property var installedPatch: {
        for (let i = 0; i < patchOptions.length; ++i)
            if (patchOptions[i].installed) return patchOptions[i]
        return null
    }
    readonly property var gameBuild: patchInfo && patchInfo.current ? patchInfo.current : ({})

    // ===== FUNCTIONS =====

    function reset() {
        gameId = ""; gameName = ""; steamAppId = ""; imageUrl = ""
        heroImageUrl = ""; verified = false; engine = ""
        hasTranslation = false; isEditorsPick = false; editorsNote = ""
        isManualGame = false; comingSoon = false; isGameInstalled = false; packageInstalled = false
        hasTranslationUpdate = false; autoInstall = false; fromLibrary = false
        description = ""; developers = []; publishers = []
        releaseDate = ""; genres = []; metacriticScore = 0
        hasWindows = true; hasMac = false; hasLinux = false
        price = ""; discountPercent = 0; hasSteamDetails = false
        isLoadingSteamDetails = false; steamFetchFailed = false
        contributors = []; translationNotes = ""
        isUnityGame = false; runtimeNeeded = false; runtimeInstalled = false
        runtimeUpToDate = false; bepinexVersion = ""; xunityVersion = ""
        unityBackend = ""; unityVersion = ""; hasAntiCheat = false
        antiCheatName = ""; isInstallingRuntime = false
        isInstallingTranslation = false; installProgress = 0; installStatus = ""
        installCompleted = false; installErrorMessage = ""; isDownloading = false
        installConsole = ""; folderChecking = false; patchProgram = ""; installErrorOnFolder = false
        patchUndoes = false; isUndoing = false
        updateImpact = null; descriptionExpanded = false
        patchInfo = ({}); selectedPkg = ""
    }

    // The patches and the game's build again: after an install, an uninstall,
    // a catalogue sync or new game-builds.json. The pick stays unless it is
    // gone; with nothing picked yet it is the installed patch.
    function refreshPatches() {
        if (gameId === "" || !hasTranslation) { patchInfo = ({}); return }
        const info = GameService.patchOptions(gameId)
        patchInfo = info || ({})
        const opts = patchInfo.options || []
        let keep = false
        for (let i = 0; i < opts.length; ++i)
            if (opts[i].pkg === selectedPkg) keep = true
        if (!keep || !_pickedByUser)
            selectedPkg = patchInfo.anyInstalled ? (patchInfo.installedPkg || "") : ""
    }
    property bool _pickedByUser: false
    function selectPatch(pkg) {
        _pickedByUser = true
        selectedPkg = pkg
    }

    // ---- Patch text (picker, compatibility line) ----
    function patchLabel(opt) {
        return opt && opt.label ? opt.label : qsTr("Türkçe yama")
    }
    // "2026-10-28" → "28 Ekim 2026"; anything else as it is
    function dateText(iso) {
        if (!iso) return ""
        const d = new Date(iso + "T00:00:00")
        return isNaN(d.getTime()) ? iso : d.toLocaleDateString(Qt.locale("tr_TR"), "d MMMM yyyy")
    }
    function sizeText(bytes) {
        if (!bytes || bytes <= 0) return ""
        const mb = bytes / 1048576
        if (mb >= 1024) return qsTr("%1 GB").arg((mb / 1024).toLocaleString(Qt.locale("tr_TR"), "f", 1))
        if (mb >= 1) return qsTr("%1 MB").arg(mb.toLocaleString(Qt.locale("tr_TR"), "f", 1))
        return qsTr("%1 KB").arg(Math.max(1, Math.round(bytes / 1024)))
    }
    // {label, steamBuild, date} → "1.16.1 (build 16412345, 28 Ekim 2026)"
    function gameVersionText(gv) {
        if (!gv) return ""
        const parts = []
        if (gv.steamBuild > 0) parts.push(qsTr("build %1").arg(gv.steamBuild))
        const dt = dateText(gv.date || "")
        if (dt !== "") parts.push(dt)
        if (gv.label) return parts.length > 0 ? gv.label + " (" + parts.join(", ") + ")" : gv.label
        return parts.join(", ")
    }
    function translatorsText(contributors) {
        const names = []
        for (let i = 0; i < (contributors || []).length; ++i) {
            const c = contributors[i]
            if (c && c.name && c.name !== "Türkiye Oyuncu Topluluğu" && names.indexOf(c.name) < 0)
                names.push(c.name)
        }
        return names.join(", ")
    }

    function loadGame(d) {
        reset()

        // Apply resolved data (from GameDataResolver)
        gameName = d.gameName || ""
        engine = d.engine || ""
        imageUrl = d.imageUrl || ""
        verified = d.verified || false
        steamAppId = d.steamAppId || ""
        hasTranslation = d.hasTranslation || false
        isManualGame = d.isManualGame || false
        comingSoon = d.comingSoon || false
        isGameInstalled = d.isGameInstalled || false
        packageInstalled = d.packageInstalled || false
        autoInstall = d.autoInstall || false
        contributors = d.contributors || []

        // A download or install keeps running while the page is closed; pick
        // it up instead of offering to start another one.
        var id = d.gameId || ""
        isDownloading = id !== "" && TranslationDownloader.isDownloading(id)
        isInstallingTranslation = isDownloading || (id !== "" && GameService.isInstalling(id))
        if (isInstallingTranslation)
            installStatus = isDownloading ? qsTr("İndiriliyor...") : qsTr("Kuruluyor...")
        installConsole = id !== "" ? GameService.installConsole(id) : ""
        patchProgram = id !== "" ? GameService.patchProgram(id) : ""
        patchUndoes = id !== "" && GameService.patchProgramUndoes(id)
        isUndoing = id !== "" && GameService.isUndoing(id)

        // Set gameId after other props so listeners see complete state
        gameId = id

        // Check for translation package update (installed version vs catalog)
        if (packageInstalled && gameId !== "")
            hasTranslationUpdate = GameService.hasTranslationUpdate(gameId)

        _pickedByUser = false
        refreshPatches()

        // Fetch additional game details (sync)
        _applyGameDetails()

        // Fetch steam details (async)
        _fetchSteamDetails()
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
        if (details.backgroundUrl && details.backgroundUrl !== "")
            heroImageUrl = details.backgroundUrl
        hasSteamDetails = true
        isLoadingSteamDetails = false
    }

    function _applyGameDetails() {
        if (gameId === "") return

        if (typeof SceneProfiler !== "undefined")
            SceneProfiler.screenLoaded("GameDetail")

        var d = GameService.getGameDetails(gameId)
        // contributors come from catalog (index.json) via loadGame — don't touch here
        translationNotes = d.installNotes || ""
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

        updateImpact = null
    }

    function _fetchSteamDetails() {
        if (steamAppId === "") return

        var cached = GameService.getSteamDetails(steamAppId)
        if (cached && cached.description !== undefined)
            populateSteamDetails(cached)
        else
            isLoadingSteamDetails = true

        GameService.fetchSteamDetails(steamAppId)

        if (!CoreBridge.isPackageDetailLoaded(steamAppId))
            ManifestSync.fetchPackageDetail(steamAppId)
    }
}
