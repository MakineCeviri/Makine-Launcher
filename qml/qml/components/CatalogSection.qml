import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0

/**
 * CatalogSection.qml - Localization library catalog with search, game strips and glass decorations.
 *
 * Extracted from HomePage.qml for better separation of concerns.
 */
Rectangle {
    id: catalog

    // ── Public API ──
    property var allGames: []
    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    // ── Layout ──
    Layout.fillWidth: true
    Layout.fillHeight: true

    // ── Visual base ──
    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    radius: Dimensions.radiusSection
    clip: true

    // ── Internal state ──
    readonly property real contentPadding: 16
    property string searchQuery: ""
    property var filteredGames: {
        if (!searchQuery)
            return allGames
        var q = searchQuery.toLowerCase()
        return allGames.filter(function(g) {
            return (g.name || "").toLowerCase().indexOf(q) !== -1
        })
    }
    readonly property int halfCount: Math.ceil(filteredGames.length / 2)
    property var row1Games: filteredGames.slice(0, halfCount)
    property var row2Games: filteredGames.slice(halfCount)

    // ── Search debounce ──
    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: catalog.searchQuery = searchInput.text.trim()
    }

    // ── Entry animation ──
    opacity: 0
    transform: Translate { id: catalogTranslate; y: 18 }
    Component.onCompleted: entryAnim.start()

    SequentialAnimation {
        id: entryAnim
        PauseAnimation { duration: Dimensions.transitionDuration }
        ParallelAnimation {
            NumberAnimation {
                target: catalog; property: "opacity"
                from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: catalogTranslate; property: "y"
                from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
            }
        }
    }

    // ── Backdrop: focus dismiss ──
    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) {
            catalog.forceActiveFocus()
            mouse.accepted = false
        }
    }

    // ── Backdrop: ambient glow ──
    AmbientGlow {
        anchors.fill: parent
        position: "top-right"
        intensity: 0.10
    }

    // ── Backdrop: glass highlight ──
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.rightMargin: 1
        anchors.topMargin: 1
        height: 1; radius: Dimensions.radiusSection
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.2; color: Qt.rgba(1, 1, 1, 0.06) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.10) }
            GradientStop { position: 0.8; color: Qt.rgba(1, 1, 1, 0.06) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // ── Backdrop: square off bottom corners ──
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Dimensions.radiusSection
        color: parent.color
    }

    // ── Content ──
    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: catalog.contentPadding
        anchors.rightMargin: catalog.contentPadding
        anchors.bottomMargin: catalog.contentPadding
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
                Layout.preferredHeight: 30
                Layout.preferredWidth: searchInput.activeFocus || searchInput.text ? 240 : 200
                Behavior on Layout.preferredWidth { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }
                radius: Dimensions.radiusMD
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                border.color: searchInput.activeFocus
                    ? Theme.withAlpha(Theme.accentBase, 0.40)
                    : Theme.withAlpha(Theme.textPrimary, 0.08)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: Dimensions.animFast } }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 8

                    // Search icon (Segoe MDL2 Assets)
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\uE721"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 13
                        color: searchInput.activeFocus ? Theme.accentBase : Theme.textMuted
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }
                    }

                    TextInput {
                        id: searchInput
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 38
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textPrimary
                        clip: true
                        selectByMouse: true
                        onTextChanged: searchDebounce.restart()
                        Keys.onEscapePressed: { text = ""; focus = false }

                        Text {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Oyun ara... (%1)").arg(catalog.allGames.length)
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            visible: !searchInput.text && !searchInput.activeFocus
                        }
                    }
                }
            }
        }

        // Header separator
        SettingsDivider { variant: "section" }

        // Loading state — catalog not yet populated
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: catalog.allGames.length === 0 && !catalog.searchQuery

            Column {
                anchors.centerIn: parent
                spacing: 12

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: visible
                    width: 32; height: 32
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Katalog y\u00FCkleniyor\u2026")
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                }
            }
        }

        // Empty search state — user searched, no match
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: catalog.filteredGames.length === 0
                     && catalog.searchQuery.length > 0

            Column {
                anchors.centerIn: parent
                spacing: 8
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\uE773"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 28
                    color: Theme.textMuted
                    opacity: 0.5
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("\"%1\" ile e\u015Fle\u015Fen oyun bulunamad\u0131").arg(catalog.searchQuery)
                    font.pixelSize: Dimensions.fontSM
                    color: Theme.textMuted
                }
            }
        }

        // Row 1
        HorizontalGameStrip {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: catalog.filteredGames.length > 0
            model: catalog.row1Games
            wrapAround: true
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        // Row separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.leftMargin: parent.width * 0.2
            Layout.rightMargin: parent.width * 0.2
            visible: catalog.row2Games.length > 0
            color: Theme.withAlpha(Theme.textPrimary, 0.08)
        }

        // Row 2
        HorizontalGameStrip {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: catalog.row2Games.length > 0
            model: catalog.row2Games
            wrapAround: true
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }
    }

    // ── Overlay: edge fades ──
    component EdgeFade: Rectangle {
        property bool mirror: false
        anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.topMargin: 40
        width: 28; z: 10
        rotation: mirror ? 180 : 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0;  color: Qt.rgba(0.055, 0.055, 0.055, 0.95) }
            GradientStop { position: 0.15; color: Qt.rgba(0.055, 0.055, 0.055, 0.70) }
            GradientStop { position: 0.35; color: Qt.rgba(0.055, 0.055, 0.055, 0.35) }
            GradientStop { position: 0.6;  color: Qt.rgba(0.055, 0.055, 0.055, 0.10) }
            GradientStop { position: 0.85; color: Qt.rgba(0.055, 0.055, 0.055, 0.02) }
            GradientStop { position: 1.0;  color: "transparent" }
        }
    }
    EdgeFade { anchors.left: parent.left }
    EdgeFade { anchors.right: parent.right; mirror: true }

    // ── Overlay: glass border ──
    Rectangle {
        anchors.fill: parent; z: 20
        radius: Dimensions.radiusSection
        color: "transparent"
        border { color: Qt.rgba(1, 1, 1, 0.08); width: 1 }
    }
}
