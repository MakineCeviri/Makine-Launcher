import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
import "../components"

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
            Component.onCompleted: topRowEntryAnim.start()

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
        Rectangle {
            id: locLibContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.bottomMargin: -homePage.gap
            color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
            radius: Dimensions.radiusSection
            border.width: 0
            clip: true

            // Square off bottom corners — flush with window edge
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Dimensions.radiusSection
                color: parent.color
            }
            // Hide bottom border
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: parent.color
            }

            opacity: 0
            transform: Translate { id: libTranslate; y: 18 }
            Component.onCompleted: libEntryAnim.start()

            SequentialAnimation {
                id: libEntryAnim
                PauseAnimation { duration: Dimensions.transitionDuration }
                ParallelAnimation {
                    NumberAnimation {
                        target: locLibContainer; property: "opacity"
                        from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: libTranslate; property: "y"
                        from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                }
            }

            // Split model into two halves
            property var allGames: GameService.supportedGames || []
            readonly property int halfCount: Math.ceil(allGames.length / 2)
            property var row1Games: allGames.slice(0, halfCount)
            property var row2Games: allGames.slice(halfCount)

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 10
                anchors.leftMargin: homePage.gap
                anchors.rightMargin: homePage.gap
                anchors.bottomMargin: homePage.gap
                spacing: 0

                // Header
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 4
                    spacing: Dimensions.spacingSM

                    // Accent pin dot
                    Rectangle {
                        Layout.preferredWidth: 6; Layout.preferredHeight: 6
                        radius: 3
                        color: Theme.accentBase
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Label {
                        text: qsTr("Yerelle\u015Ftirme K\u00FCt\u00FCphanesi")
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    // Search box
                    Rectangle {
                        Layout.preferredHeight: 28
                        Layout.preferredWidth: 200
                        radius: 14
                        color: Theme.withAlpha(Theme.textPrimary, 0.06)
                        border.color: searchInput.activeFocus
                            ? Theme.withAlpha(Theme.accentBase, 0.40)
                            : Theme.withAlpha(Theme.textPrimary, 0.08)
                        border.width: 1
                        Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 6

                            // Search icon
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "\uD83D\uDD0D"
                                font.pixelSize: 11
                                opacity: 0.4
                            }

                            TextInput {
                                id: searchInput
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 28
                                font.pixelSize: Dimensions.fontXS
                                color: Theme.textPrimary
                                clip: true
                                selectByMouse: true

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("Ara... (%1 oyun)").arg(locLibContainer.allGames.length)
                                    font.pixelSize: Dimensions.fontXS
                                    color: Theme.textMuted
                                    visible: !searchInput.text && !searchInput.activeFocus
                                }
                            }
                        }
                    }
                }

                // Header separator
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.withAlpha(Theme.textPrimary, 0.06)
                }

                // Row 1
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: libRow1
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: Dimensions.cardHeight
                        orientation: ListView.Horizontal
                        model: locLibContainer.row1Games
                        spacing: Dimensions.cardGap
                        clip: false
                        boundsBehavior: Flickable.DragOverBounds
                        flickDeceleration: 1800
                        maximumFlickVelocity: 800
                        cacheBuffer: 400
                        pixelAligned: true
                        reuseItems: true
                        rebound: Transition {
                            NumberAnimation {
                                properties: "x"
                                duration: 600
                                easing.type: Easing.OutQuint
                            }
                        }

                        delegate: GameCard {
                            required property var modelData
                            required property int index
                            gameId: modelData.id || ""
                            gameName: modelData.name || ""
                            steamAppId: modelData.steamAppId || ""
                            imageUrl: ImageCache.resolve(
                                modelData.steamAppId || modelData.id || "",
                                modelData.headerImageUrl || ""
                            )
                            installPath: modelData.installPath || ""
                            verified: modelData.isVerified || false
                            translated: true
                            onClicked: {
                                homePage.gameSelected(
                                    modelData.id || "", modelData.name || "",
                                    modelData.installPath || "", modelData.engine || ""
                                )
                            }
                        }
                    }

                    // Vertical mouse wheel → horizontal scroll
                    WheelHandler {
                        orientation: Qt.Vertical
                        property real _prev: 0
                        onRotationChanged: {
                            var d = rotation - _prev; _prev = rotation
                            var lo = libRow1.originX
                            var hi = libRow1.contentWidth - libRow1.width + lo
                            libRow1.contentX = Math.max(lo, Math.min(hi, libRow1.contentX - d * 4))
                        }
                    }
                }

                // Row separator — fades toward edges
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.6
                        height: 1
                        radius: 0.5
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.3; color: Theme.withAlpha(Theme.textPrimary, 0.10) }
                            GradientStop { position: 0.5; color: Theme.withAlpha(Theme.textPrimary, 0.12) }
                            GradientStop { position: 0.7; color: Theme.withAlpha(Theme.textPrimary, 0.10) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }
                }

                // Row 2
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: libRow2
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: Dimensions.cardHeight
                        orientation: ListView.Horizontal
                        model: locLibContainer.row2Games
                        spacing: Dimensions.cardGap
                        clip: false
                        boundsBehavior: Flickable.DragOverBounds
                        flickDeceleration: 1800
                        maximumFlickVelocity: 800
                        cacheBuffer: 400
                        pixelAligned: true
                        reuseItems: true
                        rebound: Transition {
                            NumberAnimation {
                                properties: "x"
                                duration: 600
                                easing.type: Easing.OutQuint
                            }
                        }

                        delegate: GameCard {
                            required property var modelData
                            required property int index
                            gameId: modelData.id || ""
                            gameName: modelData.name || ""
                            steamAppId: modelData.steamAppId || ""
                            imageUrl: ImageCache.resolve(
                                modelData.steamAppId || modelData.id || "",
                                modelData.headerImageUrl || ""
                            )
                            installPath: modelData.installPath || ""
                            verified: modelData.isVerified || false
                            translated: true
                            onClicked: {
                                homePage.gameSelected(
                                    modelData.id || "", modelData.name || "",
                                    modelData.installPath || "", modelData.engine || ""
                                )
                            }
                        }
                    }

                    // Vertical mouse wheel → horizontal scroll
                    WheelHandler {
                        orientation: Qt.Vertical
                        property real _prev: 0
                        onRotationChanged: {
                            var d = rotation - _prev; _prev = rotation
                            var lo = libRow2.originX
                            var hi = libRow2.contentWidth - libRow2.width + lo
                            libRow2.contentX = Math.max(lo, Math.min(hi, libRow2.contentX - d * 4))
                        }
                    }
                }
            }

            // Edge fade overlays — short but intense
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.topMargin: 40
                width: 28; z: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.98) }
                    GradientStop { position: 0.25; color: Qt.rgba(0.055, 0.055, 0.055, 0.80) }
                    GradientStop { position: 0.55; color: Qt.rgba(0.055, 0.055, 0.055, 0.30) }
                    GradientStop { position: 0.80; color: Qt.rgba(0.055, 0.055, 0.055, 0.05) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.0) }
                }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.topMargin: 40
                width: 28; z: 10
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.0) }
                    GradientStop { position: 0.20; color: Qt.rgba(0.055, 0.055, 0.055, 0.05) }
                    GradientStop { position: 0.45; color: Qt.rgba(0.055, 0.055, 0.055, 0.30) }
                    GradientStop { position: 0.75; color: Qt.rgba(0.055, 0.055, 0.055, 0.80) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.98) }
                }
            }

            // Border overlay — on top of edge fades
            Rectangle {
                anchors.fill: parent
                radius: Dimensions.radiusSection
                color: "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.06)
                border.width: 1
                z: 20
            }
        }
    }
}
