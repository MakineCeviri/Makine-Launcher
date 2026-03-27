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
    property string externalUrl: ""
    property bool isApex: false
    property bool isHangar: false
    property string apexTier: ""  // "pro" or "both"

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

    // ===== DOWNLOAD STATE =====
    property bool isDownloading: false

    // ===== UPDATE IMPACT =====
    property var updateImpact: null

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

    // ===== FUNCTIONS =====

    function reset() {
        gameId = ""; gameName = ""; steamAppId = ""; imageUrl = ""
        heroImageUrl = ""; verified = false; engine = ""
        hasTranslation = false; isEditorsPick = false; editorsNote = ""
        isManualGame = false; externalUrl = ""; isApex = false; isHangar = false; apexTier = ""; isGameInstalled = false; packageInstalled = false
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
        updateImpact = null; descriptionExpanded = false
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
        externalUrl = d.externalUrl || ""
        isApex = d.isApex || false
        isHangar = d.isHangar || false
        apexTier = d.apexTier || ""
        isGameInstalled = d.isGameInstalled || false
        packageInstalled = d.packageInstalled || false
        autoInstall = d.autoInstall || false

        // Set gameId after other props so listeners see complete state
        gameId = d.gameId || ""

        // Check for translation package update (installed version vs catalog)
        if (packageInstalled && gameId !== "")
            hasTranslationUpdate = GameService.hasTranslationUpdate(gameId)

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
        contributors = d.contributors || []
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
