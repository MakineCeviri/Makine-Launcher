import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineAI 1.0
pragma ComponentBehavior: Bound

/**
 * CatalogSection.qml - Game catalog with search and two scrollable rows.
 */
Rectangle {
    id: catalog

    property var allGames: []
    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    Layout.fillWidth: true
    Layout.fillHeight: true

    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    radius: Dimensions.radiusSection
    clip: true

    // Internal state
    readonly property real contentPadding: 16
    property string searchQuery: ""
    property var filteredGames: allGames
    property var row1Games: []
    property var row2Games: []
    property bool _row2Ready: false

    function _recomputeFiltered() {
        var src = allGames
        if (searchQuery) {
            var q = searchQuery.toLowerCase()
            src = allGames.filter(function(g) {
                return (g.name || "").toLowerCase().indexOf(q) !== -1
            })
        }
        filteredGames = src
        var half = Math.ceil(src.length / 2)
        row1Games = src.slice(0, half)

        if (_row2Ready || searchQuery) {
            row2Games = src.slice(half)
        } else {
            row2Games = []
            _row2Defer.start()
        }
    }
    onAllGamesChanged: _recomputeFiltered()

    Timer {
        id: _row2Defer
        interval: 800
        onTriggered: {
            catalog._row2Ready = true
            var src = catalog.filteredGames
            catalog.row2Games = src.slice(Math.ceil(src.length / 2))
        }
    }

    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: {
            catalog.searchQuery = searchInput.text.trim()
            catalog._recomputeFiltered()
        }
    }

    // Square off bottom corners
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Dimensions.radiusSection
        color: parent.color
    }

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

            Rectangle {
                Layout.preferredWidth: 6; Layout.preferredHeight: 6
                radius: 3; color: Theme.accentBase
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                textFormat: Text.PlainText
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
                Behavior on Layout.preferredWidth { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                radius: Dimensions.radiusMD
                color: Theme.withAlpha(Theme.textPrimary, 0.06)
                border.color: searchInput.activeFocus
                    ? Theme.withAlpha(Theme.accentBase, 0.40)
                    : Theme.withAlpha(Theme.textPrimary, 0.08)
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 6
                    spacing: 8

                    Text {
                        textFormat: Text.PlainText
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\uE721"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 13
                        color: searchInput.activeFocus ? Theme.accentBase : Theme.textMuted
                    }

                    TextInput {
                        id: searchInput
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 38
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textPrimary
                        clip: true; selectByMouse: true
                        onTextChanged: searchDebounce.restart()
                        Keys.onEscapePressed: { text = ""; focus = false }

                        Text {
                            textFormat: Text.PlainText
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

        SettingsDivider { variant: "section" }

        // Loading state
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog.allGames.length === 0 && !catalog.searchQuery

            Column {
                anchors.centerIn: parent; spacing: 12
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: visible; width: 32; height: 32
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Katalog y\u00FCkleniyor\u2026")
                    font.pixelSize: Dimensions.fontSM; color: Theme.textMuted
                }
            }
        }

        // Empty search state
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog.filteredGames.length === 0 && catalog.searchQuery.length > 0

            Column {
                anchors.centerIn: parent; spacing: 8
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "\uE773"
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 28; color: Theme.textMuted; opacity: 0.5
                }
                Text {
                    textFormat: Text.PlainText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("\"%1\" ile e\u015Fle\u015Fen oyun bulunamad\u0131").arg(catalog.searchQuery)
                    font.pixelSize: Dimensions.fontSM; color: Theme.textMuted
                }
            }
        }

        // Row 1
        HorizontalGameStrip {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog.filteredGames.length > 0
            model: catalog.row1Games
            wrapAround: true
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        // Row separator
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 1
            Layout.leftMargin: parent.width * 0.2; Layout.rightMargin: parent.width * 0.2
            visible: catalog.row2Games.length > 0
            color: Theme.withAlpha(Theme.textPrimary, 0.08)
        }

        // Row 2
        HorizontalGameStrip {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog.row2Games.length > 0
            model: catalog.row2Games
            wrapAround: true
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }
    }

    // Edge fades
    component EdgeFade: Rectangle {
        property bool mirror: false
        anchors { top: parent.top; bottom: parent.bottom; topMargin: 40 }
        width: 28; z: 10; rotation: mirror ? 180 : 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(0.055, 0.055, 0.055, 0.90) }
            GradientStop { position: 0.4; color: Qt.rgba(0.055, 0.055, 0.055, 0.25) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    EdgeFade { anchors.left: parent.left }
    EdgeFade { anchors.right: parent.right; mirror: true }
}
