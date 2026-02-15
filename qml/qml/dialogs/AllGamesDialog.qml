import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import MakineAI 1.0

/**
 * AllGamesDialog.qml - Full-screen overlay showing all supported games
 * with search capability. Card style matches HomeScreen GameCards.
 */
Dialog {
    id: root

    property var games: []
    property string searchText: ""
    property string activeSearchText: ""

    signal gameSelected(string gameId)

    // Shared context menu data (one Menu for all cards instead of per-delegate)
    property var _ctxGameData: null

    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: root.activeSearchText = root.searchText
    }

    property var filteredGames: activeSearchText.length === 0
        ? root.games
        : GameService.filterGames(activeSearchText)

    title: qsTr("Tüm Desteklenen Oyunlar")
    modal: true
    closePolicy: Popup.CloseOnEscape
    // Tight-fit: exactly 4 columns
    readonly property int _cellW: Dimensions.cardWidth + Dimensions.cardGap
    width: 4 * _cellW + Dimensions.marginLG * 2
    height: parent ? Math.min(640, parent.height - 64) : 600
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    background: Rectangle {
        color: Theme.surface
        radius: Dimensions.radiusStandard
        border.color: Theme.withAlpha(Theme.textPrimary, 0.08)
        border.width: 1
    }

    onOpened: searchField.forceActiveFocus()
    onClosed: { searchField.text = ""; searchText = ""; activeSearchText = "" }

    header: null
    footer: null

    // Shared context menu — ONE instance for all cards (not per-delegate)
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

        // ================================================================
        // TOP BAR
        // ================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Dimensions.marginLG
                anchors.rightMargin: Dimensions.marginMS
                spacing: Dimensions.spacingXL

                // Title
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
                        text: qsTr("%1 oyun").arg(filteredGames.length)
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }

                // Search — minimal underline style
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
                        Accessible.name: qsTr("Search games")
                        onTextChanged: {
                            root.searchText = text
                            searchDebounce.restart()
                        }

                        Text {
                            anchors.fill: parent
                            text: qsTr("Ara...  (Ctrl+K)")
                            font.pixelSize: Dimensions.fontSM
                            color: Theme.textMuted
                            visible: !searchField.text && !searchField.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // Underline
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: searchField.activeFocus ? 2 : 1
                        color: searchField.activeFocus
                               ? Theme.primary
                               : Theme.withAlpha(Theme.textPrimary, 0.1)
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                        Behavior on height { NumberAnimation { duration: Dimensions.animFast } }
                    }

                    // Clear
                    Rectangle {
                        id: clearBtn
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18
                        radius: 9
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
                    Accessible.name: qsTr("Close")
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

        // ================================================================
        // GAME GRID (virtualized — only visible cards are created)
        // ================================================================
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
                cacheBuffer: 300
                boundsBehavior: Flickable.StopAtBounds
                model: root.filteredGames

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

                    // ---- Game Card (HomeScreen GameCard hover style) ----
                    Item {
                        id: card
                        width: Dimensions.cardWidth
                        height: Dimensions.cardHeight
                        anchors.horizontalCenter: parent.horizontalCenter
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.name || qsTr("Unknown")
                        activeFocusOnTab: true
                        Keys.onReturnPressed: cardAction()
                        Keys.onSpacePressed: cardAction()

                        function cardAction() {
                            root.gameSelected(modelData.id)
                            root.close()
                        }

                        property bool hov: cardMa.containsMouse

                        // Hover lift: Y translate + scale (matches GameCard)
                        transform: [
                            Translate { y: card.hov ? -4 : 0; Behavior on y { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } } },
                            Scale {
                                origin.x: card.width / 2; origin.y: card.height / 2
                                xScale: card.hov ? 1.02 : 1.0; yScale: card.hov ? 1.02 : 1.0
                                Behavior on xScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                                Behavior on yScale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                            }
                        ]

                        Rectangle {
                            id: cardClip
                            anchors.fill: parent
                            radius: Dimensions.cardBorderRadius
                            color: Theme.surface
                            clip: true

                            // Animated gradient border phase
                            property real borderPhase: 0
                            NumberAnimation on borderPhase {
                                from: 0; to: 1
                                duration: 8000
                                loops: Animation.Infinite
                                running: card.hov
                            }

                            // Animated rainbow gradient border (matches GameCard)
                            Canvas {
                                anchors.fill: parent
                                z: Dimensions.zContent
                                property real phase: cardClip.borderPhase
                                onPhaseChanged: if (hov) requestPaint()
                                property bool hov: card.hov
                                onHovChanged: requestPaint()

                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)

                                    var angle = phase * Math.PI * 2
                                    var cx = width / 2, cy = height / 2
                                    var len = Math.max(width, height) * 0.7
                                    var x1 = cx + Math.cos(angle) * len
                                    var y1 = cy + Math.sin(angle) * len
                                    var x2 = cx - Math.cos(angle) * len
                                    var y2 = cy - Math.sin(angle) * len

                                    var grad = ctx.createLinearGradient(x1, y1, x2, y2)
                                    var colors = Theme.brandGradient
                                    for (var i = 0; i < colors.length; i++)
                                        grad.addColorStop(i / Math.max(1, colors.length - 1), colors[i])

                                    var bw = 1.5
                                    var r = Dimensions.cardBorderRadius - bw / 2
                                    var px = bw / 2, py = bw / 2
                                    var w = width - bw, h = height - bw

                                    ctx.beginPath()
                                    ctx.moveTo(px + r, py)
                                    ctx.lineTo(px + w - r, py)
                                    ctx.arcTo(px + w, py, px + w, py + r, r)
                                    ctx.lineTo(px + w, py + h - r)
                                    ctx.arcTo(px + w, py + h, px + w - r, py + h, r)
                                    ctx.lineTo(px + r, py + h)
                                    ctx.arcTo(px, py + h, px, py + h - r, r)
                                    ctx.lineTo(px, py + r)
                                    ctx.arcTo(px, py, px + r, py, r)
                                    ctx.closePath()

                                    ctx.strokeStyle = grad
                                    ctx.lineWidth = bw
                                    ctx.globalAlpha = hov ? 0.8 : 0.0
                                    ctx.stroke()
                                }
                            }

                            // Mask for rounded corners
                            Item {
                                id: imgMask
                                anchors.fill: parent
                                visible: false
                                layer.enabled: true

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Dimensions.cardBorderRadius
                                    color: Theme.textOnColor
                                }
                            }

                            // Game image
                            Image {
                                id: img
                                anchors.fill: parent
                                source: modelData.headerImageUrl || ""
                                fillMode: Image.PreserveAspectCrop
                                sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                                asynchronous: true
                                cache: true
                                visible: false
                            }

                            // Masked image with brightness on hover
                            MultiEffect {
                                anchors.fill: img
                                source: img
                                maskEnabled: true
                                maskSource: imgMask
                                visible: img.status === Image.Ready
                                brightness: card.hov ? 0.06 : 0
                                Behavior on brightness { NumberAnimation { duration: Dimensions.animNormal } }
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
                                anchors.leftMargin: Dimensions.marginSM
                                anchors.rightMargin: Dimensions.marginSM
                                anchors.bottomMargin: Dimensions.marginSM
                                text: modelData.name || qsTr("Unknown")
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
                                    width: 14; height: 14
                                    radius: 7
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
                        }

                        // Focus indicator
                        Rectangle {
                            anchors.fill: cardClip
                            anchors.margins: -2
                            radius: cardClip.radius + 2
                            color: "transparent"
                            border.color: Theme.withAlpha(Theme.primary, 0.6)
                            border.width: 2
                            visible: card.activeFocus
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
                visible: root.filteredGames.length === 0

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

            // Top fade shadow
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

            // Bottom fade shadow
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
