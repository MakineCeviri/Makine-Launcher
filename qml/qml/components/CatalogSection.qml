import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * CatalogSection.qml - Game catalog with search and two scrollable rows.
 * Uses CatalogProxyModel (C++) instead of JS filter/slice/concat.
 */
Rectangle {
    id: catalog

    signal gameClicked(string gameId, string gameName, string installPath, string engine)

    function replayContentFade() {
        catalogContent.opacity = 0
        catalogContentFade.start()
    }

    Layout.fillWidth: true
    Layout.fillHeight: true

    color: Qt.rgba(0.055, 0.055, 0.055, 0.85)
    radius: Dimensions.radiusSection

    GradientBorder { cornerRadius: parent.radius }

    // Internal state
    readonly property real contentPadding: 16
    property string searchQuery: ""
    readonly property bool _isSearching: searchQuery.length > 0
    property bool _row2Ready: false

    readonly property int _totalCount: GameService.supportedGamesModel
                                        ? GameService.supportedGamesModel.count : 0
    readonly property int _half: Math.ceil(_totalCount / 2)

    // Row 1 proxy: first half (or all during search)
    CatalogProxyModel {
        id: row1Proxy
        sourceModel: GameService.supportedGamesModel
        searchFilter: catalog.searchQuery
        rowOffset: 0
        rowLimit: catalog._isSearching ? -1 : catalog._half
        wrapAround: !catalog._isSearching
    }

    // Row 2 proxy: second half (hidden during search)
    CatalogProxyModel {
        id: row2Proxy
        sourceModel: catalog._row2Ready ? GameService.supportedGamesModel : null
        searchFilter: catalog.searchQuery
        rowOffset: catalog._half
        rowLimit: -1
        wrapAround: true
    }

    Timer {
        id: _row2Defer
        interval: 800
        onTriggered: catalog._row2Ready = true
    }

    Component.onCompleted: _row2Defer.start()

    Timer {
        id: searchDebounce
        interval: 100
        onTriggered: catalog.searchQuery = searchInput.text.trim()
    }

    // Square off bottom corners
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Dimensions.radiusSection
        color: parent.color
    }

    ColumnLayout {
        id: catalogContent
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: catalog.contentPadding
        anchors.rightMargin: catalog.contentPadding
        anchors.bottomMargin: catalog.contentPadding
        spacing: 0
        opacity: 0

        Component.onCompleted: catalogContentFade.start()
        NumberAnimation {
            id: catalogContentFade
            target: catalogContent; property: "opacity"
            from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic
        }

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
                color: searchInput.activeFocus ? Theme.primary05 : Theme.textPrimary06
                border.color: searchInput.activeFocus
                    ? Theme.accentBase40
                    : Theme.textPrimary08
                border.width: 1
                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                Row {
                    anchors.left: parent.left; anchors.right: clearBtn.left
                    anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.leftMargin: 10; anchors.rightMargin: 4
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
                        width: parent.width - 28
                        font.pixelSize: Dimensions.fontXS
                        color: Theme.textPrimary
                        clip: true; selectByMouse: true
                        onTextChanged: searchDebounce.restart()
                        Keys.onEscapePressed: { text = ""; focus = false }

                        Text {
                            textFormat: Text.PlainText
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("Oyun ara... (%1)").arg(catalog._totalCount)
                            font.pixelSize: Dimensions.fontXS
                            color: Theme.textMuted
                            visible: !searchInput.text && !searchInput.activeFocus
                        }
                    }
                }

                // Clear button — slides in from right
                Rectangle {
                    id: clearBtn
                    anchors.right: parent.right; anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: searchInput.text.length > 0 ? 22 : 0
                    height: 22; radius: 4; clip: true
                    color: clearMa.containsMouse ? Theme.textPrimary15 : "transparent"
                    opacity: searchInput.text.length > 0 ? 1 : 0

                    Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    Text {
                        textFormat: Text.PlainText
                        anchors.centerIn: parent
                        text: "\uE711"
                        font.family: "Segoe MDL2 Assets"
                        font.pixelSize: 10
                        color: clearMa.containsMouse ? Theme.textPrimary : Theme.textMuted
                    }

                    MouseArea {
                        id: clearMa; anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { searchInput.text = ""; searchInput.focus = true }
                    }
                }
            }
        }

        SettingsDivider { variant: "section" }

        // Loading state
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: catalog._totalCount === 0 && !catalog.searchQuery

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
            visible: row1Proxy.sourceCount === 0 && catalog.searchQuery.length > 0

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

        Item { Layout.preferredHeight: Dimensions.cardGap }

        // Row 1 (expands to full height with large cards during search)
        HorizontalGameStrip {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: row1Proxy.count > 0 || catalog._totalCount > 0
            model: row1Proxy
            wrapAround: !catalog._isSearching
            largeCards: catalog._isSearching
            wheelEnabled: !catalog._isSearching || row1Proxy.sourceCount >= 4
            driftSpeed: 0
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        // Row separator with padding
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Dimensions.cardGap * 2
            visible: !catalog._isSearching && catalog._row2Ready && row2Proxy.count > 0

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.6
                height: 1
                color: Theme.textPrimary08
            }
        }

        // Row 2
        HorizontalGameStrip {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: !catalog._isSearching && catalog._row2Ready && row2Proxy.count > 0
            model: row2Proxy
            wrapAround: true
            driftSpeed: 0
            onGameClicked: (gameId, gameName, installPath, engine) =>
                catalog.gameClicked(gameId, gameName, installPath, engine)
        }

        Item { Layout.preferredHeight: Dimensions.cardGap }
    }

    // Edge fades — inline component named CatalogEdgeFade to avoid shadowing SectionContainer's EdgeFade
    component CatalogEdgeFade: Rectangle {
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
    CatalogEdgeFade { anchors.left: parent.left }
    CatalogEdgeFade { anchors.right: parent.right; mirror: true }
}
