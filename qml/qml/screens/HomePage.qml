import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

/**
 * HomePage.qml - Main home view with game detection, announcements and localization library
 */
Item {
    id: homePage

    // Properties propagated from HomeScreen
    property bool animationsEnabled: true
    property real contentMargin: 16
    // Uniform gap — used everywhere for consistent spacing
    readonly property real gap: 16

    // Responsive top row height: 22% of page height, clamped
    readonly property real topRowHeight: Math.max(140, Math.min(220, homePage.height * 0.22))

    // Signals
    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()
    signal settingsRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: homePage.gap
        anchors.leftMargin: homePage.gap
        anchors.rightMargin: homePage.gap
        anchors.bottomMargin: 0
        spacing: homePage.gap

        // ===== UPDATE STATUS PILL =====
        Row {
            spacing: 6
            visible: UpdateChecker.statusType === "upToDate" || UpdateChecker.statusType === "updateAvailable"

            Rectangle {
                width: 6; height: 6; radius: 3
                anchors.verticalCenter: parent.verticalCenter
                color: UpdateChecker.statusType === "updateAvailable" ? Theme.warning : Theme.success
            }

            Text {
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

        // ===== TOP ROW =====
        RowLayout {
            id: topRowLayout
            Layout.fillWidth: true
            Layout.preferredHeight: homePage.topRowHeight
            Layout.maximumHeight: homePage.topRowHeight
            spacing: homePage.gap

            opacity: 0
            transform: Translate { id: topRowTranslate; y: 14 }
            property bool _entryPlayed: false
            Component.onCompleted: {
                if (!_entryPlayed) {
                    topRowEntryAnim.start()
                    _entryPlayed = true
                }
            }

            ParallelAnimation {
                id: topRowEntryAnim
                NumberAnimation {
                    target: topRowLayout; property: "opacity"
                    from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: topRowTranslate; property: "y"
                    from: 14; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                }
            }

            GameDetectionCard {
                animationsEnabled: homePage.animationsEnabled
                layoutCardMargin: 0
                layoutCardSpacing: 0
                layoutTopRowHeight: homePage.topRowHeight
                onManualFolderRequested: homePage.manualFolderRequested()
            }

            AnnouncementCard {
                layoutCardMargin: 0
                layoutCardSpacing: 0
                layoutTopRowHeight: homePage.topRowHeight
                onGameClicked: function(gameId, gameName, installPath, engine) {
                    homePage.gameSelected(gameId, gameName, installPath, engine)
                }
            }
        }

        // ===== BATCH OPERATIONS PANEL =====
        BatchOperationsPanel {
            Layout.fillWidth: true
            animationsEnabled: homePage.animationsEnabled
        }

        // ===== LOCALIZATION LIBRARY (flush to bottom) =====
        CatalogSection {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.bottomMargin: -homePage.gap
            allGames: GameService.supportedGames || []
            onGameClicked: (gameId, gameName, installPath, engine) =>
                homePage.gameSelected(gameId, gameName, installPath, engine)
        }
    }
}
