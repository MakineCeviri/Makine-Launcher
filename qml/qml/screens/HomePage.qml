import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

/**
 * HomePage.qml - Main home view: game detection, announcements, catalog.
 */
Item {
    id: homePage

    property bool animationsEnabled: true
    property real contentMargin: 16
    readonly property real gap: 16
    readonly property real topRowHeight: Math.max(140, Math.min(220, homePage.height * 0.22))

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

        // Update status pill
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

        // Top row: detection + announcement
        RowLayout {
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
            Layout.fillWidth: true
            animationsEnabled: homePage.animationsEnabled
        }

        // Catalog
        CatalogSection {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 0; Layout.rightMargin: 0
            Layout.bottomMargin: -homePage.gap
            allGames: GameService.supportedGames || []
            onGameClicked: (gameId, gameName, installPath, engine) =>
                homePage.gameSelected(gameId, gameName, installPath, engine)
        }
    }
}
