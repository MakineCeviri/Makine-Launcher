import QtQuick
import MakineAI 1.0

/**
 * GameDataResolver.qml - Resolves game metadata for detail screen navigation
 *
 * Consolidates the duplicate game data resolution logic from Main.qml
 * (onGameSelected and onInstallAndShowDetail handlers).
 */
QtObject {
    id: resolver

    function resolve(gameId, gameName, installPath, engine, forceAutoInstall) {
        var gameData = GameService.getGameById(gameId)
        var isManual = (gameData && gameData.source === "manual") || gameId.startsWith("manual_")
        var isInstalled = (gameData && gameData.isInstalled) || installPath !== ""
        var resolvedSteamAppId = (gameData && gameData.steamAppId) || ""

        // For catalog-only games, gameId IS the steamAppId
        if (resolvedSteamAppId === "" && /^\d+$/.test(gameId))
            resolvedSteamAppId = gameId

        var resolvedImageUrl = ImageCache.resolve(resolvedSteamAppId || gameId)
        var hasTranslation = (gameData && gameData.hasTranslation) || false
        var pkgInstalled = (gameData && gameData.packageInstalled) || false

        return {
            gameId: gameId,
            gameName: gameName,
            engine: engine,
            imageUrl: resolvedImageUrl,
            verified: (gameData && gameData.isVerified) || false,
            steamAppId: resolvedSteamAppId,
            hasTranslation: hasTranslation,
            isManualGame: isManual,
            isGameInstalled: isInstalled,
            packageInstalled: pkgInstalled,
            autoInstall: forceAutoInstall ? true : (isInstalled && hasTranslation && !pkgInstalled)
        }
    }
}
