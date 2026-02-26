import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
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
        updatePill.opacity = 0; pillTranslate.y = 20
        topRow.opacity = 0; topRowTranslate.y = 20
        batchPanel.opacity = 0; batchTranslate.y = 20
        catalogSection.opacity = 0; catalogTranslate.y = 20

        if (!animationsEnabled) {
            updatePill.opacity = 1; pillTranslate.y = 0
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

        // Update status pill
        Row {
            id: updatePill
            opacity: 0
            transform: Translate { id: pillTranslate; y: 20 }
            spacing: 6
            visible: UpdateChecker.statusType === "upToDate" || UpdateChecker.statusType === "updateAvailable"

            Rectangle {
                width: 6; height: 6; radius: 3
                anchors.verticalCenter: parent.verticalCenter
                color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.success
            }

            Text {
                textFormat: Text.PlainText
                text: UpdateChecker.statusType === "updateAvailable"
                      ? qsTr("%1 mevcut").arg(UpdateChecker.latestVersion)
                      : qsTr("G\u00FCncel")
                font.pixelSize: Dimensions.fontXS
                color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.textMuted

                MouseArea {
                    anchors.fill: parent
                    cursorShape: UpdateChecker.statusType === "updateAvailable" ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (UpdateChecker.statusType === "updateAvailable")
                            homePage.settingsRequested()
                    }
                }
            }
        }

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
                updatePill.opacity = 1; pillTranslate.y = 0
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

            // Update pill — delay 0, 350ms
            SequentialAnimation {
                NumberAnimation { target: updatePill; property: "opacity"; from: 0; to: 1; duration: 350; easing.type: Easing.OutCubic }
            }
            SequentialAnimation {
                NumberAnimation { target: pillTranslate; property: "y"; from: 20; to: 0; duration: 350; easing.type: Easing.OutCubic }
            }

            // Top row — delay 80ms, 400ms
            SequentialAnimation {
                PauseAnimation { duration: 80 }
                NumberAnimation { target: topRow; property: "opacity"; from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic }
            }
            SequentialAnimation {
                PauseAnimation { duration: 80 }
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
