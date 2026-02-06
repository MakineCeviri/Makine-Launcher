import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * AllGamesDialog.qml - Native Qt AllGamesDialog birebir port
 * Kaynak: ui/src/widgets/allgamesdialog.cpp
 *
 * Features:
 * - Modal dialog showing all supported games
 * - Search filter
 * - Grid layout with game cards
 * - Category filter
 */
Dialog {
    id: root

    property var games: []
    property string searchText: ""
    property string selectedCategory: "all"
    property var categories: ["all", "verified", "translating", "planned"]

    signal gameSelected(string gameId)

    title: "Tüm Desteklenen Oyunlar"
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 900
    height: 700

    // Center in parent
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    // Filter games based on search and category
    property var filteredGames: {
        var result = root.games
        if (searchText.length > 0) {
            var search = searchText.toLowerCase()
            result = result.filter(g => g.name.toLowerCase().includes(search))
        }
        if (selectedCategory !== "all") {
            result = result.filter(g => g.category === selectedCategory)
        }
        return result
    }

    background: Rectangle {
        color: Theme.surface
        radius: 8
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
    }

    // Custom header
    header: Rectangle {
        height: 140
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            // Top row with title and close
            RowLayout {
                spacing: 16

                // Icon
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 8
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(Theme.gold.r, Theme.gold.g, Theme.gold.b, 0.2) }
                        GradientStop { position: 1.0; color: Qt.rgba(Theme.olive.r, Theme.olive.g, Theme.olive.b, 0.2) }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "\uD83C\uDFAE"  // Game controller
                        font.pixelSize: 24
                    }
                }

                ColumnLayout {
                    spacing: 4

                    Text {
                        text: "Tüm Desteklenen Oyunlar"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        color: Theme.textPrimary
                    }

                    Text {
                        text: filteredGames.length + " oyun listeleniyor"
                        font.pixelSize: 13
                        color: Theme.textMuted
                    }
                }

                Item { Layout.fillWidth: true }

                // Close button
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 4
                    color: closeDialogMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "\u00D7"
                        font.pixelSize: 24
                        color: Theme.textMuted
                    }

                    MouseArea {
                        id: closeDialogMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }

            // Search and filter row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // Search input
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.05)
                    border.color: searchInput.activeFocus ? Theme.primary : Qt.rgba(1, 1, 1, 0.1)
                    border.width: 1

                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: "\uD83D\uDD0D"
                            font.pixelSize: 16
                            color: Theme.textMuted
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            font.pixelSize: 14
                            color: Theme.textPrimary
                            clip: true
                            selectByMouse: true
                            onTextChanged: root.searchText = text

                            Text {
                                anchors.fill: parent
                                text: "Oyun ara..."
                                font.pixelSize: 14
                                color: Theme.textMuted
                                visible: !searchInput.text && !searchInput.activeFocus
                            }
                        }

                        // Clear button
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: 12
                            color: clearSearchMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                            visible: searchInput.text.length > 0

                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                font.pixelSize: 14
                                color: Theme.textMuted
                            }

                            MouseArea {
                                id: clearSearchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: searchInput.text = ""
                            }
                        }
                    }
                }

                // Category filters
                RowLayout {
                    spacing: 8

                    CategoryButton {
                        text: "Tümü"
                        category: "all"
                        isSelected: selectedCategory === "all"
                        onClicked: selectedCategory = "all"
                    }

                    CategoryButton {
                        text: "Onaylı"
                        category: "verified"
                        isSelected: selectedCategory === "verified"
                        onClicked: selectedCategory = "verified"
                    }

                    CategoryButton {
                        text: "Çevrilmeyen"
                        category: "translating"
                        isSelected: selectedCategory === "translating"
                        onClicked: selectedCategory = "translating"
                    }
                }
            }
        }

        // Bottom border
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }
    }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            background: Rectangle { color: "transparent" }
            contentItem: Rectangle {
                implicitWidth: 8
                radius: 4
                color: parent.pressed ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
            }
        }

        // Games grid
        GridLayout {
            id: gamesGrid
            width: parent.width - 48
            anchors.horizontalCenter: parent.horizontalCenter
            columns: Math.max(1, Math.floor((parent.width - 48) / (Dimensions.cardWidth + Dimensions.cardGap)))
            columnSpacing: Dimensions.cardGap
            rowSpacing: Dimensions.cardGap

            Repeater {
                model: root.filteredGames

                // Game card
                Rectangle {
                    Layout.preferredWidth: Dimensions.cardWidth
                    Layout.preferredHeight: Dimensions.cardHeight
                    radius: Dimensions.cardBorderRadius
                    color: Theme.surface
                    clip: true

                    scale: cardMouse.containsMouse ? 1.05 : 1.0
                    transformOrigin: Item.Center

                    Behavior on scale {
                        NumberAnimation {
                            duration: Dimensions.animNormal
                            easing.type: Easing.OutCubic
                        }
                    }

                    // Background gradient
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.surface }
                            GradientStop { position: 1.0; color: Theme.surfaceActive }
                        }
                    }

                    // Game image
                    Image {
                        anchors.fill: parent
                        source: modelData.imageUrl || ""
                        fillMode: Image.PreserveAspectCrop
                        visible: (modelData.imageUrl !== undefined && modelData.imageUrl !== null && modelData.imageUrl !== "")
                    }

                    // Placeholder
                    Text {
                        anchors.centerIn: parent
                        text: modelData.name ? modelData.name.substring(0, 2).toUpperCase() : ""
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: Theme.textMuted
                        visible: modelData.imageUrl === undefined || modelData.imageUrl === null || modelData.imageUrl === ""
                    }

                    // Bottom gradient overlay
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.5
                        radius: Dimensions.cardBorderRadius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.9) }
                        }
                    }

                    // Verified badge
                    Rectangle {
                        visible: modelData.verified === true
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: 8
                        anchors.rightMargin: 8
                        width: 22
                        height: 22
                        radius: 4
                        color: Theme.withAlpha(Theme.primary, 0.9)

                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: "white"
                        }
                    }

                    // Game name
                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 10
                        text: modelData.name || "Unknown"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "white"
                        elide: Text.ElideRight
                    }

                    // Hover overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        opacity: cardMouse.containsMouse ? 1.0 : 0.0
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.withAlpha(Theme.splashGold, 0.08) }
                            GradientStop { position: 1.0; color: Theme.withAlpha(Theme.pink, 0.12) }
                        }

                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }

                    // Border glow
                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.cardBorderRadius
                        color: "transparent"
                        border.width: 2
                        border.color: Theme.withAlpha(Theme.splashGold, 0.6)
                        opacity: cardMouse.containsMouse ? 0.6 : 0

                        Behavior on opacity { NumberAnimation { duration: 250 } }
                    }

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.gameSelected(modelData.id)
                            root.close()
                        }
                    }
                }
            }
        }

        // Empty state
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16
            visible: root.filteredGames.length === 0

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "\uD83D\uDE14"
                font.pixelSize: 48
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Oyun bulunamadı"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Arama kriterlerini değiştirmeyi deneyin"
                font.pixelSize: 14
                color: Theme.textMuted
            }
        }
    }

    // Category button component
    component CategoryButton: Rectangle {
        property string text: ""
        property string category: ""
        property bool isSelected: false
        signal clicked()

        width: catBtnLabel.width + 24
        height: 36
        radius: 4
        color: isSelected ? Theme.withAlpha(Theme.primary, 0.15) : (catBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.05) : "transparent")
        border.color: isSelected ? Theme.primary : (catBtnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent")
        border.width: 1

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Text {
            id: catBtnLabel
            anchors.centerIn: parent
            text: parent.text
            font.pixelSize: 13
            font.weight: isSelected ? Font.DemiBold : Font.Medium
            color: isSelected ? Theme.primary : Theme.textSecondary
        }

        MouseArea {
            id: catBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
