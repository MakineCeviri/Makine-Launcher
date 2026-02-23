import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "components"
import "screens"

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

    // ===== UPDATE CHECKER (C++ backend) =====
    property bool updateAvailable: UpdateChecker.updateAvailable
    property string latestVersion: UpdateChecker.latestVersion

    Component.onCompleted: {
        // Defer heavy I/O until after first frame renders
        scanDelayTimer.start()
    }

    Timer {
        id: scanDelayTimer
        interval: 500
        onTriggered: {
            GameService.scanAllLibraries()
            if (SettingsManager.showNotifications)
                updateCheckTimer.start()
        }
    }

    Timer {
        id: updateCheckTimer
        interval: 2500
        onTriggered: UpdateChecker.checkForUpdatesIfNeeded()
    }

    // Current sub-page index: 0=Home, 1=Library
    property int currentPage: 0

    function showHomePage() { currentPage = 0 }
    function showLibraryPage() { currentPage = 1 }

    // Sub-pages share the same slot, switched by visibility
    HomePage {
        id: homePage
        anchors.fill: parent
        visible: root.currentPage === 0
        animationsEnabled: root.animationsEnabled
        contentMargin: root.contentMargin
        onGameSelected: function(gameId, gameName, installPath, engine) {
            root.gameSelected(gameId, gameName, installPath, engine)
        }
        onManualFolderRequested: root.manualFolderRequested()
        onSettingsRequested: root.settingsRequested()
    }

    Loader {
        id: libraryLoader
        anchors.fill: parent
        active: root.currentPage === 1
        visible: active
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
