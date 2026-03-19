import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * Library.qml - Installed games & translations library
 */
Item {
    id: libraryRoot

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)

    // No heavy operations on screen entry — use refresh buttons instead

    // Minimum height per section: padding(40) + header(30) + divider(1) + card(185)
    readonly property real sectionMinHeight: 256
    // Available height for sections after margins and spacing
    readonly property real availableHeight: libraryRoot.height - contentMargin * 2
    // Each section's ideal height: half of available, clamped to minimum
    readonly property real sectionHeight: Math.max(sectionMinHeight,
        (availableHeight - Dimensions.spacingMD) / 2)
    // Total content exceeds viewport — enable scroll
    readonly property bool needsScroll: sectionHeight * 2 + Dimensions.spacingMD > availableHeight

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.topMargin: libraryRoot.contentMargin
        anchors.leftMargin: libraryRoot.contentMargin
        anchors.rightMargin: libraryRoot.contentMargin
        contentHeight: contentColumn.implicitHeight
        clip: true
        interactive: libraryRoot.needsScroll
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3000
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }

        ColumnLayout {
            id: contentColumn
            width: flickable.width
            spacing: Dimensions.spacingMD

            opacity: 0
            transform: Translate { id: panelsTranslate; y: 18 }
            Component.onCompleted: panelsEntryAnim.start()

            SequentialAnimation {
                id: panelsEntryAnim
                PauseAnimation { duration: 80 }
                ParallelAnimation {
                    NumberAnimation {
                        target: contentColumn; property: "opacity"
                        from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: panelsTranslate; property: "y"
                        from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                }
            }

            GameSection {
                title: qsTr("Kurulu Oyunlar")
                model: GameService.games
                loading: GameService.isScanning
                emptyText: qsTr("Kurulu oyun bulunamad\u0131")
                badgeColor: Theme.primary
                refreshable: true
                onRefreshClicked: GameService.scanAllLibraries()
                glowPosition: "bottom-left"
                Layout.fillWidth: true
                Layout.preferredHeight: libraryRoot.sectionHeight
                onGameClicked: (gameId, gameName, installPath, engine) =>
                    libraryRoot.gameSelected(gameId, gameName, installPath, engine)
            }

            GameSection {
                title: qsTr("Kurulu Yamalar")
                model: GameService.installedTranslations
                emptyText: qsTr("Kurulu yama yok")
                badgeColor: Theme.accent
                notificationCount: GameService.outdatedPatchCount
                notificationText: GameService.outdatedPatchCount > 0
                    ? qsTr("%1 yamanız güncel değil").arg(GameService.outdatedPatchCount)
                    : ""
                refreshable: true
                onRefreshClicked: GameService.checkForUpdates()
                glowPosition: "top-left"
                Layout.fillWidth: true
                Layout.preferredHeight: libraryRoot.sectionHeight
                onGameClicked: (gameId, gameName, installPath, engine) =>
                    libraryRoot.gameSelected(gameId, gameName, installPath, engine)
            }
        }
    }

}
