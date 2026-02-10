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

    signal gameSelected(string gameId)

    property var filteredGames: {
        if (searchText.length === 0)
            return root.games
        var search = searchText.toLowerCase()
        return root.games.filter(g => (g.name || "").toLowerCase().includes(search))
    }

    title: qsTr("Tüm Desteklenen Oyunlar")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: parent ? Math.min(960, parent.width - 64) : 900
    height: parent ? Math.min(720, parent.height - 48) : 680
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

    header: null
    footer: null

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
                        onTextChanged: root.searchText = text

                        Text {
                            anchors.fill: parent
                            text: qsTr("Ara...")
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
        // GAME GRID
        // ================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: scrollArea
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth

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

                Item {
                    width: scrollArea.availableWidth
                    implicitHeight: gamesGrid.height + 24 + 48

                    GridLayout {
                        id: gamesGrid
                        anchors.top: parent.top
                        anchors.topMargin: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - Dimensions.marginLG * 2

                        readonly property int colCount: Math.max(3, Math.floor((width + Dimensions.cardGap) / (Dimensions.cardWidth + Dimensions.cardGap)))
                        columns: colCount
                        columnSpacing: Dimensions.cardGap
                        rowSpacing: Dimensions.cardGap

                        Repeater {
                            model: root.filteredGames

                            // ---- Game Card (portrait, HomeScreen style) ----
                            Item {
                                id: card
                                Layout.preferredWidth: Dimensions.cardWidth
                                Layout.preferredHeight: Dimensions.cardHeight
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

                                // Round mask
                                Item {
                                    id: roundMask
                                    anchors.fill: parent
                                    visible: false
                                    layer.enabled: true

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: Dimensions.cardBorderRadius
                                        color: "white"
                                    }
                                }

                                // Card content (rendered via MultiEffect mask)
                                Item {
                                    id: cardContent
                                    anchors.fill: parent
                                    visible: false
                                    layer.enabled: true

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: Dimensions.cardBorderRadius
                                        color: Theme.surface
                                    }

                                    // Library image (portrait, same as HomeScreen)
                                    Image {
                                        id: img
                                        anchors.fill: parent
                                        source: modelData.headerImageUrl || ""
                                        fillMode: Image.PreserveAspectCrop
                                        sourceSize: Qt.size(Dimensions.cardWidth * 2, Dimensions.cardHeight * 2)
                                        asynchronous: true
                                        cache: true
                                        visible: status === Image.Ready
                                    }

                                    // Placeholder
                                    Rectangle {
                                        anchors.fill: parent
                                        color: Theme.withAlpha(Theme.textPrimary, 0.04)
                                        visible: !img.visible

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

                                    // Hover tint
                                    Rectangle {
                                        anchors.fill: parent
                                        color: Theme.withAlpha(Theme.primary, 0.08)
                                        opacity: card.hov ? 1.0 : 0.0
                                        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                    }
                                }

                                // Masked output
                                MultiEffect {
                                    anchors.fill: cardContent
                                    source: cardContent
                                    maskEnabled: true
                                    maskSource: roundMask
                                }

                                // Hover border
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Dimensions.cardBorderRadius
                                    color: "transparent"
                                    border.width: 1
                                    border.color: Theme.withAlpha(Theme.primary, 0.4)
                                    opacity: card.hov ? 1.0 : 0.0
                                    Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
                                }

                                // Focus indicator
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: Dimensions.cardBorderRadius + 1
                                    color: "transparent"
                                    border.color: Theme.withAlpha(Theme.primary, 0.6)
                                    border.width: 2
                                    visible: parent.activeFocus
                                }

                                MouseArea {
                                    id: cardMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: card.cardAction()
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
                }
            }

            // Top fade gradient
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 32
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.surface }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            // Bottom fade gradient
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 48
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.surface }
                }
            }
        }
    }
}
