import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

/**
 * HomePage.qml - "Kütüphanem" page with installed games & translations
 */
Item {
    id: root

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    signal installAndShowDetail(string gameId, string gameName, string installPath, string engine)

    ColumnLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.topMargin: root.contentMargin
        anchors.leftMargin: root.contentMargin
        anchors.rightMargin: root.contentMargin
        spacing: Dimensions.spacingMD

        opacity: 0
        transform: Translate { id: panelsTranslate; y: 18 }
        Component.onCompleted: panelsEntryAnim.start()

        SequentialAnimation {
            id: panelsEntryAnim
            PauseAnimation { duration: 80 }
            ParallelAnimation {
                NumberAnimation {
                    target: mainLayout; property: "opacity"
                    from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: panelsTranslate; property: "y"
                    from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                }
            }
        }

        // ===== INSTALLED GAMES =====
        SectionContainer {
            Layout.fillWidth: true
            Layout.fillHeight: true
            glowPosition: "bottom-left"

            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingSM

                Label {
                    text: qsTr("Kurulu Oyunlar")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                BusyIndicator {
                    visible: GameService.isScanning
                    running: GameService.isScanning
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: installedCountLbl.width + 14
                    radius: 10
                    color: Theme.withAlpha(Theme.primary, 0.12)
                    border.color: Theme.withAlpha(Theme.primary, 0.20)
                    border.width: 1
                    Label {
                        id: installedCountLbl
                        anchors.centerIn: parent
                        text: (GameService.games || []).length
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.Medium
                        color: Theme.primary
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }

            GridView {
                id: installedGamesGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: GameService.games || []
                clip: true
                cellWidth: Dimensions.cardWidth + Dimensions.cardGap
                cellHeight: Dimensions.cardHeight + Dimensions.cardGap
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: StyledScrollBar {}

                delegate: Item {
                    required property var modelData
                    required property int index
                    width: installedGamesGrid.cellWidth
                    height: installedGamesGrid.cellHeight

                    GameCard {
                        anchors.centerIn: parent
                        gameId: modelData.id || ""
                        gameName: modelData.name || ""
                        steamAppId: modelData.steamAppId || ""
                        imageUrl: ImageCache.resolve(
                            modelData.steamAppId || modelData.id || "",
                            modelData.headerImageUrl || ""
                        )
                        installPath: modelData.installPath || ""
                        verified: modelData.isVerified || false
                        translated: modelData.hasTranslation || false
                        onClicked: {
                            root.gameSelected(
                                modelData.id || "", modelData.name || "",
                                modelData.installPath || "", modelData.engine || ""
                            )
                        }
                    }
                }

                // Empty state
                Label {
                    anchors.centerIn: parent
                    visible: (GameService.games || []).length === 0 && !GameService.isScanning
                    text: qsTr("Kurulu oyun bulunamad\u0131")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                }
            }
        }

        // ===== INSTALLED TRANSLATIONS =====
        SectionContainer {
            id: patchesContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            glowPosition: "top-right"

            property var patchedGames: GameService.installedTranslations()

            Connections {
                target: GameService
                function onGamesChanged() {
                    patchesContainer.patchedGames = GameService.installedTranslations()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Dimensions.spacingSM

                Label {
                    text: qsTr("Kurulu Yamalar")
                    font.pixelSize: Dimensions.fontLG
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: patchCountLbl.width + 14
                    radius: 10
                    color: Theme.withAlpha(Theme.accent, 0.12)
                    border.color: Theme.withAlpha(Theme.accent, 0.20)
                    border.width: 1
                    Label {
                        id: patchCountLbl
                        anchors.centerIn: parent
                        text: patchesContainer.patchedGames.length
                        font.pixelSize: Dimensions.fontXS
                        font.weight: Font.Medium
                        color: Theme.accentLight
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
            }

            GridView {
                id: patchedGamesGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: patchesContainer.patchedGames
                clip: true
                cellWidth: Dimensions.cardWidth + Dimensions.cardGap
                cellHeight: Dimensions.cardHeight + Dimensions.cardGap
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: StyledScrollBar {}

                delegate: Item {
                    required property var modelData
                    required property int index
                    width: patchedGamesGrid.cellWidth
                    height: patchedGamesGrid.cellHeight

                    GameCard {
                        anchors.centerIn: parent
                        gameId: modelData.gameId || modelData.id || ""
                        gameName: modelData.name || modelData.gameName || ""
                        steamAppId: modelData.steamAppId || ""
                        imageUrl: ImageCache.resolve(
                            modelData.steamAppId || modelData.gameId || modelData.id || "",
                            modelData.headerImageUrl || ""
                        )
                        installPath: modelData.installPath || ""
                        translated: true
                        onClicked: {
                            root.gameSelected(
                                modelData.gameId || modelData.id || "",
                                modelData.name || modelData.gameName || "",
                                modelData.installPath || "",
                                modelData.engine || ""
                            )
                        }
                    }
                }

                // Empty state
                Label {
                    anchors.centerIn: parent
                    visible: patchesContainer.patchedGames.length === 0
                    text: qsTr("Kurulu yama yok")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                }
            }
        }
    }
}
