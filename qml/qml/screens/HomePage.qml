import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * HomePage.qml - Main home view: game detection, announcements, catalog.
 */
Item {
    id: homePage

    property bool animationsEnabled: true
    property real contentMargin: 16
    readonly property real gap: 16
    readonly property real topRowHeight: Math.max(140, Math.min(220, homePage.height * 0.22))

    property bool _initialComplete: false

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    signal settingsRequested()

    onVisibleChanged: {
        if (visible && _initialComplete)
            _replayEntryAnim()
    }

    function _replayEntryAnim() {
        entryAnim.stop()
        topRow.opacity = 0; topRowTranslate.y = 20
        batchPanel.opacity = 0; batchTranslate.y = 20
        catalogSection.opacity = 0; catalogTranslate.y = 20

        if (!animationsEnabled) {
            topRow.opacity = 1; topRowTranslate.y = 0
            batchPanel.opacity = 1; batchTranslate.y = 0
            catalogSection.opacity = 1; catalogTranslate.y = 0
            return
        }
        catalogSection.replayContentFade()
        entryAnim.start()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: homePage.gap
        anchors.leftMargin: homePage.gap
        anchors.rightMargin: homePage.gap
        anchors.bottomMargin: 0
        spacing: homePage.gap

        // Update status moved to NavBar — no duplicate here

        // Top row: detection + announcement
        RowLayout {
            id: topRow
            opacity: 0
            transform: Translate { id: topRowTranslate; y: 20 }
            Layout.fillWidth: true
            Layout.preferredHeight: homePage.topRowHeight
            Layout.maximumHeight: homePage.topRowHeight
            spacing: homePage.gap

            GameDetectionCard {
                animationsEnabled: homePage.animationsEnabled
                layoutCardMargin: 0; layoutCardSpacing: 0
                layoutTopRowHeight: homePage.topRowHeight
                onManualFolderRequested: homePage.manualFolderRequested()
            }

            AnnouncementCard {
                layoutCardMargin: 0; layoutCardSpacing: 0
                layoutTopRowHeight: homePage.topRowHeight
            }
        }

        // Batch operations (only visible when running)
        BatchOperationsPanel {
            id: batchPanel
            opacity: 0
            transform: Translate { id: batchTranslate; y: 20 }
            Layout.fillWidth: true
            animationsEnabled: homePage.animationsEnabled
        }

        // Catalog
        CatalogSection {
            id: catalogSection
            opacity: 0
            transform: Translate { id: catalogTranslate; y: 20 }
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 0; Layout.rightMargin: 0
            Layout.bottomMargin: -homePage.gap
            onGameClicked: (gameId, gameName, installPath, engine) =>
                homePage.gameSelected(gameId, gameName, installPath, engine)
        }

        Component.onCompleted: {
            if (!homePage.animationsEnabled) {
                topRow.opacity = 1; topRowTranslate.y = 0
                batchPanel.opacity = 1; batchTranslate.y = 0
                catalogSection.opacity = 1; catalogTranslate.y = 0
            } else {
                entryAnim.start()
            }
            homePage._initialComplete = true
        }

        ParallelAnimation {
            id: entryAnim

            // Top row — delay 0, 400ms
            SequentialAnimation {
                NumberAnimation { target: topRow; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }
            SequentialAnimation {
                NumberAnimation { target: topRowTranslate; property: "y"; from: 20; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }

            // Batch panel — delay 140ms, 400ms
            SequentialAnimation {
                PauseAnimation { duration: 140 }
                NumberAnimation { target: batchPanel; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }
            SequentialAnimation {
                PauseAnimation { duration: 140 }
                NumberAnimation { target: batchTranslate; property: "y"; from: 20; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }

            // Catalog — delay 200ms, 400ms
            SequentialAnimation {
                PauseAnimation { duration: 200 }
                NumberAnimation { target: catalogSection; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }
            SequentialAnimation {
                PauseAnimation { duration: 200 }
                NumberAnimation { target: catalogTranslate; property: "y"; from: 20; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }
        }
    }
}
