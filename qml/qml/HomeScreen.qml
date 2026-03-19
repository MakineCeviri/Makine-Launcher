import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "components"
import "screens"
pragma ComponentBehavior: Bound

/**
 * HomeScreen.qml - Main container for Home and Library sub-pages
 */
Item {
    id: root

    // GPU optimization - propagated from Main.qml
    property bool animationsEnabled: true

    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)
    signal settingsRequested()
    signal manualFolderRequested()

    Component.onCompleted: {
        if (typeof SceneProfiler !== "undefined")
            SceneProfiler.screenLoaded("Home")
        // Defer heavy I/O until after first frame renders
        scanDelayTimer.start()
    }

    // Filesystem scan after app settles — cached data from initialize() already displayed.
    // Interval must exceed createRootObject time (~4s) to avoid firing during pre-render.
    Timer {
        id: scanDelayTimer
        interval: 5000
        onTriggered: GameService.scanAllLibraries()
    }

    // Update check handled by UpdateService in main.cpp startup.

    // Current sub-page index: 0=Home, 1=Library
    property int currentPage: 0

    function showHomePage() { currentPage = 0 }
    function showLibraryPage() { currentPage = 1 }

    // Sub-pages share the same slot, switched by visibility
    // Async-loaded: defers heavyweight creation (GameDetectionCard,
    // AnnouncementCard, CatalogSection with 2× strip) until after first frame.
    Loader {
        id: homePageLoader
        anchors.fill: parent
        active: true
        visible: root.currentPage === 0
        asynchronous: true
        sourceComponent: Component {
            HomePage {
                animationsEnabled: root.animationsEnabled
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
                onManualFolderRequested: root.manualFolderRequested()
                onSettingsRequested: root.settingsRequested()
            }
        }
    }

    Loader {
        id: libraryLoader
        anchors.fill: parent
        active: root.currentPage === 1 || libraryLoader.status === Loader.Ready
        visible: root.currentPage === 1
        asynchronous: true
        sourceComponent: Component {
            Library {
                animationsEnabled: root.animationsEnabled
                contentMargin: root.contentMargin
                onGameSelected: function(gameId, gameName, installPath, engine) {
                    root.gameSelected(gameId, gameName, installPath, engine)
                }
            }
        }
    }
}
