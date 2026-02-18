import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * AllGamesDialog.qml - All supported games overlay
 *
 * Lightweight: no MultiEffect, no layer.enabled, no Canvas per card.
 * Images resolve through ImageCache (qrc first, then CDN fallback).
 */
Dialog {
    id: root

    property var games: []
    property string searchText: ""
    property string activeSearchText: ""

    signal gameSelected(string gameId)

    property var _ctxGameData: null

    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: root.activeSearchText = root.searchText
    }

    property var filteredGames: activeSearchText.length === 0
        ? root.games
        : GameService.filterGames(activeSearchText)

    // Progressive reveal — cards load in batches
    property int revealCount: 0
    property var displayedGames: {
        var all = filteredGames
        if (!all || all.length === 0) return []
        var n = Math.min(revealCount, all.length)
        return Array.prototype.slice.call(all, 0, n)
    }

    Timer {
        id: revealTimer
        interval: 40
        repeat: true
        onTriggered: {
            if (root.revealCount < root.filteredGames.length)
                root.revealCount += 12
            else
                stop()
        }
    }

    onFilteredGamesChanged: {
        revealCount = 0
        revealTimer.restart()
    }

    title: qsTr("Tüm Desteklenen Oyunlar")
    modal: true
    closePolicy: Popup.CloseOnEscape

    readonly property int _cellW: Dimensions.cardWidth + Dimensions.cardGap
    width: 4 * _cellW + Dimensions.marginLG * 2
    height: parent ? Math.min(640, parent.height - 64) : 600
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 0; topPadding: 0; bottomPadding: 0; leftPadding: 0; rightPadding: 0

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
        border.width: 1
    }

    onOpened: {
        searchField.forceActiveFocus()
        revealCount = 0
        revealTimer.restart()
    }
    onClosed: { searchField.text = ""; searchText = ""; activeSearchText = ""; revealCount = 0 }

    header: null
    footer: null

    // Shared context menu
    Menu {
        id: sharedContextMenu
        Overlay.modal: Rectangle { color: "transparent" }

        background: Rectangle {
            implicitWidth: 200
            radius: Dimensions.radiusMD
            color: Theme.glassBackground
            border.color: Theme.glassBorder
            border.width: 1
        }

        MenuItem {
            text: qsTr("Detaylar")
            onTriggered: {
                if (root._ctxGameData) {
                    root.gameSelected(root._ctxGameData.id)
                    root.close()
                }
            }
            contentItem: Label {
                text: parent.text
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                leftPadding: Dimensions.paddingSM
            }
            background: Rectangle {
                color: parent.highlighted ? Theme.withAlpha(Theme.primary, 0.12) : "transparent"
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.08)
            }
        }

        MenuItem {
            text: qsTr("Steam'de Aç")
            visible: root._ctxGameData && (root._ctxGameData.steamAppId || "") !== ""
            height: visible ? implicitHeight : 0
            onTriggered: Qt.openUrlExternally("https://store.steampowered.com/app/" + root._ctxGameData.steamAppId)
            contentItem: Label {
                text: parent.text
                font.pixelSize: Dimensions.fontSM
                color: Theme.textPrimary
                leftPadding: Dimensions.paddingSM
            }
            background: Rectangle {
                color: parent.highlighted ? Theme.withAlpha(Theme.primary, 0.12) : "transparent"
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ===== TOP BAR =====
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginLG
                anchors.rightMargin: Dimensions.marginMS
                spacing: Dimensions.spacingXL

                ColumnLayout {
                    spacing: 1
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: qsTr("Tüm Oyunlar")
                        font.pixelSize: Dimensions.fontLG
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        font.letterSpacing: Dimensions.letterSpacingHeadline
                    }
                    Text {
                        text: qsTr("%1 oyun").arg(root.filteredGames.length)
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }

                // Search
                Item {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 32
                    Layout.alignment: Qt.AlignVCenter

                    TextInput {
                        id: searchField
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 2
                        anchors.rightMargin: clearBtn.visible ? 24 : 2
                        font.pixelSize: Dimensions.fontSM
                        color: Theme.textPrimary
                        clip: true
                        selectByMouse: true
                        Accessible.role: Accessible.EditableText
                        Accessible.name: qsTr("Oyun ara")
                        onTextChanged: {
                            root.searchText = text
                            searchDebounce.restart()
                        }

                        Text {
                            anchors.fill: parent
                            text: qsTr("Türkçe Yama Ara...")
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.textMuted
                            visible: !searchField.text && !searchField.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: searchField.activeFocus ? 2 : 1
                        color: searchField.activeFocus
                               ? Theme.primary
                               : Theme.withAlpha(Theme.textPrimary, 0.1)
                    }

                    Rectangle {
                        id: clearBtn
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18; radius: 9
                        color: clearMa.containsMouse
                               ? Theme.withAlpha(Theme.textPrimary, 0.1) : "transparent"
                        visible: searchField.text.length > 0

                        Text {
                            anchors.centerIn: parent
                            text: "\u00D7"
                            font.pixelSize: Dimensions.fontBody
                            color: Theme.textSecondary
                        }
                        MouseArea {
                            id: clearMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: searchField.text = ""
                        }
                    }
                }

                // Close
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    Layout.alignment: Qt.AlignVCenter
                    radius: Dimensions.radiusStandard
                    color: closeMa.containsMouse
                           ? Theme.withAlpha(Theme.textPrimary, 0.08) : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Kapat")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: root.close()
                    Keys.onSpacePressed: root.close()

                    Text {
                        anchors.centerIn: parent
                        text: "\u00D7"
                        font.pixelSize: Dimensions.fontXL
                        color: Theme.textMuted
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        radius: parent.radius + 1
                        color: "transparent"
                        border.color: Theme.withAlpha(Theme.primary, 0.6)
                        border.width: 2
                        visible: parent.activeFocus
                    }

                    MouseArea {
                        id: closeMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.withAlpha(Theme.textPrimary, 0.05)
            }
        }

        // ===== GAME GRID =====
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property real effectiveCellW: Dimensions.cardWidth + Dimensions.cardGap
            readonly property real effectiveCellH: Dimensions.cardHeight + Dimensions.cardGap

            GridView {
                id: gamesGrid
                anchors.fill: parent
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                anchors.leftMargin: Dimensions.marginLG
                anchors.rightMargin: Dimensions.marginLG
                clip: true
                cellWidth: parent.effectiveCellW
                cellHeight: parent.effectiveCellH
                cacheBuffer: 600
                boundsBehavior: Flickable.StopAtBounds
                model: root.displayedGames

                // Update images when CDN downloads complete
                Connections {
                    target: ImageCache
                    function onImageReady() { gamesGrid.forceLayout() }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    background: Rectangle { color: "transparent" }
                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: Dimensions.radiusStandard
                        color: parent.pressed
                               ? Theme.withAlpha(Theme.textPrimary, 0.18)
                               : Theme.withAlpha(Theme.textPrimary, 0.08)
                    }
                }

                delegate: Item {
                    width: gamesGrid.cellWidth
                    height: gamesGrid.cellHeight

                    Rectangle {
                        id: card
                        width: Dimensions.cardWidth
                        height: Dimensions.cardHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: Dimensions.cardBorderRadius
                        color: Theme.surface
                        layer.enabled: true
                        scale: cardMa.containsMouse ? 1.02 : 1.0

                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name || ""
                        activeFocusOnTab: true
                        Keys.onReturnPressed: cardAction()
                        Keys.onSpacePressed: cardAction()

                        function cardAction() {
                            root.gameSelected(modelData.id)
                            root.close()
                        }

                        // Game image — qrc first, CDN fallback
                        Image {
                            id: img
                            anchors.fill: parent
                            source: {
                                var appId = modelData.steamAppId || ""
                                var url = modelData.headerImageUrl || ""
                                return ImageCache.resolve(appId, url) || url
                            }
                            fillMode: Image.PreserveAspectCrop
                            sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                            asynchronous: true
                            cache: true
                        }

                        // Placeholder
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.withAlpha(Theme.textPrimary, 0.04)
                            visible: img.status !== Image.Ready

                            Text {
                                anchors.centerIn: parent
                                text: modelData.name ? modelData.name.substring(0, 2).toUpperCase() : ""
                                font.pixelSize: Dimensions.fontXL
                                font.weight: Font.Bold
                                color: Theme.withAlpha(Theme.textMuted, 0.5)
                            }
                        }

                        // Bottom gradient
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: parent.height * 0.45
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 0.5; color: Theme.withAlpha("#000000", 0.4) }
                                GradientStop { position: 1.0; color: Theme.withAlpha("#000000", 0.8) }
                            }
                        }

                        // Game name
                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Dimensions.marginSM
                            text: modelData.name || ""
                            font.pixelSize: Dimensions.fontBody
                            font.weight: Font.DemiBold
                            color: "#ffffff"
                            elide: Text.ElideRight
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                        }

                        // Status badges
                        Row {
                            visible: modelData.isVerified === true || modelData.hasTranslation === true || modelData.isInstalled === true
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: Dimensions.marginSM
                            anchors.rightMargin: Dimensions.marginSM
                            spacing: Dimensions.spacingXS

                            Rectangle {
                                visible: modelData.isInstalled === true
                                width: installedLabel.width + 8; height: 14
                                radius: Dimensions.badgeRadius
                                color: Theme.withAlpha("#4CAF50", 0.85)

                                Text {
                                    id: installedLabel
                                    anchors.centerIn: parent
                                    text: qsTr("Kurulu")
                                    font.pixelSize: Dimensions.fontMicro
                                    font.weight: Font.Bold
                                    color: "#ffffff"
                                }
                            }

                            Rectangle {
                                visible: modelData.hasTranslation === true
                                width: 22; height: 14
                                radius: Dimensions.badgeRadius
                                color: Theme.turkishRed

                                Text {
                                    anchors.centerIn: parent
                                    text: "TR"
                                    font.pixelSize: Dimensions.fontMicro
                                    font.weight: Font.Bold
                                    color: "#ffffff"
                                }
                            }

                            Rectangle {
                                visible: modelData.isVerified === true
                                width: 14; height: 14; radius: 7
                                color: Theme.primary

                                Text {
                                    anchors.centerIn: parent
                                    text: "\u2713"
                                    font.pixelSize: Dimensions.fontMicro
                                    font.weight: Font.Bold
                                    color: "#ffffff"
                                }
                            }
                        }

                        MouseArea {
                            id: cardMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    root._ctxGameData = modelData
                                    sharedContextMenu.popup()
                                } else {
                                    card.cardAction()
                                }
                            }
                        }
                    }
                }
            }

            // Empty state
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Dimensions.spacingLG
                visible: root.filteredGames.length === 0 && !revealTimer.running

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Oyun bulunamadı")
                    font.pixelSize: Dimensions.fontSubtitle
                    font.weight: Font.DemiBold
                    color: Theme.textSecondary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Farklı bir arama deneyin")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                }
            }

            // Top fade
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 48
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.surface }
                    GradientStop { position: 0.6; color: Theme.withAlpha(Theme.surface, 0.6) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            // Bottom fade
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 64
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.4; color: Theme.withAlpha(Theme.surface, 0.6) }
                    GradientStop { position: 1.0; color: Theme.surface }
                }
            }
        }
    }
}
