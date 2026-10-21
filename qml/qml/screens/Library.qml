// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MakineLauncher 1.0
import "../components"
pragma ComponentBehavior: Bound

/**
 * Library.qml - This PC's games with a Turkish patch, and what each needs next
 *
 * One list instead of two shelves of covers ("Kurulu Oyunlar", "Kurulu
 * Yamalar") that showed the same game twice and said nothing about it. Each
 * row carries the game's state and its one action — install, update, the
 * patch's own site — and opens the game's page. The counts on top are the
 * filters. GameService.library orders the rows: what needs a step first.
 */
Item {
    id: libraryRoot

    property bool animationsEnabled: true
    property real contentMargin: 16

    signal gameSelected(string gameId, string gameName, string installPath, string engine)
    // Install (or update) right away: the game's page opens and starts it.
    signal installRequested(string gameId, string gameName, string installPath, string engine)
    signal manualFolderRequested()

    property string filter: "all"   // all | ready | update | installed

    readonly property var _lib: GameService.library
    readonly property int _ready: _lib.filter(g => g.status === "ready").length
    readonly property int _updates: _lib.filter(g => g.status === "update").length
    readonly property int _patched: _lib.filter(g => g.status === "installed" || g.status === "update").length
    readonly property var _rows: libraryRoot.filter === "all" ? _lib
        : libraryRoot.filter === "installed"
          ? _lib.filter(g => g.status === "installed" || g.status === "update")
        : _lib.filter(g => g.status === libraryRoot.filter)

    // A filter whose rows are gone (the last update installed) falls back.
    on_RowsChanged: if (_rows.length === 0 && filter !== "all") filter = "all"

    function _shownPath(p) {
        return String(p || "").split("/").join(String.fromCharCode(92))
    }
    function _source(g) {
        switch (g.source) {
        case "steam": return "Steam"
        case "epic": return "Epic Games"
        case "gog": return "GOG"
        case "xbox": return "Xbox"
        case "manual": return qsTr("Elle eklendi")
        default: return qsTr("Otomatik bulundu")
        }
    }

    // ── Small parts ──────────────────────────────────────────────────────────
    component LibButton: Rectangle {
        id: lb
        property string glyph: ""
        property string text: ""
        property bool filled: false
        property color fill: Theme.primary
        signal clicked()

        implicitWidth: lbRow.implicitWidth + Dimensions.paddingXL
        implicitHeight: 38
        radius: Dimensions.radiusMD
        opacity: lb.enabled ? 1 : 0.5
        color: lb.filled ? (lbMouse.containsMouse ? Qt.darker(lb.fill, 1.12) : lb.fill)
                         : (lbMouse.containsMouse ? Theme.textPrimary12 : Theme.textPrimary06)
        border.width: lb.activeFocus ? 2 : 0
        border.color: Theme.accentLight
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: lb.text
        Keys.onReturnPressed: lb.clicked()
        Keys.onSpacePressed: lb.clicked()

        Row {
            id: lbRow
            anchors.centerIn: parent
            spacing: Dimensions.spacingMD
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: lb.glyph !== ""
                textFormat: Text.PlainText
                text: lb.glyph
                font.family: "Segoe MDL2 Assets"
                font.pixelSize: 13
                color: lb.filled ? Theme.textOnColor : Theme.textSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: lb.text
                font.pixelSize: Dimensions.fontBody
                font.weight: Font.DemiBold
                color: lb.filled ? Theme.textOnColor : Theme.textPrimary
            }
        }
        // A MouseArea, not a TapHandler: it takes the press, so the row
        // under the button does not open the page as well.
        MouseArea {
            id: lbMouse
            anchors.fill: parent
            enabled: lb.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: lb.clicked()
        }
    }

    // A count that is also the filter for its rows.
    component StatTile: Rectangle {
        id: tile
        property int value: 0
        property string label: ""
        property string key: "all"
        property color tint: Theme.textPrimary
        readonly property bool selected: libraryRoot.filter === tile.key

        Layout.fillWidth: true
        implicitHeight: 76
        radius: Dimensions.radiusLG
        enabled: tile.key === "all" || tile.value > 0
        color: tile.selected ? Theme.withAlpha(tile.tint, 0.10)
             : tileHover.hovered && tile.enabled ? Theme.textPrimary06 : Theme.textPrimary03
        border.width: 1
        border.color: tile.selected ? Theme.withAlpha(tile.tint, 0.45) : Theme.textPrimary06
        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

        activeFocusOnTab: tile.enabled
        Accessible.role: Accessible.RadioButton
        Accessible.checked: tile.selected
        Accessible.name: tile.label + ": " + tile.value
        Keys.onReturnPressed: libraryRoot.filter = tile.key
        Keys.onSpacePressed: libraryRoot.filter = tile.key

        Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Dimensions.paddingLG
            spacing: 2
            Text {
                textFormat: Text.PlainText
                text: String(tile.value)
                font.pixelSize: Dimensions.fontHero
                font.weight: Font.Bold
                color: tile.value > 0 || tile.key === "all" ? tile.tint : Theme.textMuted
            }
            Text {
                textFormat: Text.PlainText
                text: tile.label
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
                color: tile.selected ? Theme.textPrimary : Theme.textSecondary
            }
        }
        Rectangle {
            visible: tile.activeFocus
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 2
            border.color: Theme.accentLight
        }
        HoverHandler { id: tileHover; cursorShape: tile.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { onTapped: libraryRoot.filter = tile.key }
    }

    // ── Page ─────────────────────────────────────────────────────────────────
    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: libraryRoot.contentMargin
        contentHeight: contentColumn.implicitHeight + Dimensions.spacingXL
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: StyledScrollBar {}

        ColumnLayout {
            id: contentColumn
            width: Math.min(flickable.width, Dimensions.contentMaxWidth)
            x: (flickable.width - width) / 2
            spacing: Dimensions.spacingXL

            opacity: 0
            transform: Translate { id: panelsTranslate; y: 18 }
            Component.onCompleted: panelsEntryAnim.start()

            SequentialAnimation {
                id: panelsEntryAnim
                PauseAnimation { duration: 80 }
                ParallelAnimation {
                    NumberAnimation {
                        target: contentColumn; property: "opacity"
                        from: 0; to: 1; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: panelsTranslate; property: "y"
                        from: 18; to: 0; duration: Dimensions.animSlow; easing.type: Easing.OutCubic
                    }
                }
            }

            SectionContainer {
                glowPosition: "top-right"
                contentSpacing: Dimensions.spacingXL

                // Title, what is here, and the two ways to change it
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Dimensions.spacingLG

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Dimensions.spacingXS

                        Text {
                            textFormat: Text.PlainText
                            text: qsTr("Kütüphanem")
                            font.pixelSize: Dimensions.fontHero
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                        }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            font.pixelSize: Dimensions.fontMD
                            color: Theme.textSecondary
                            text: GameService.isScanning && libraryRoot._lib.length === 0
                                  ? qsTr("Oyunların aranıyor…")
                                  : libraryRoot._lib.length === 0
                                    ? qsTr("Bilgisayarında Türkçe yaması olan bir oyun bulamadık.")
                                    : qsTr("Bilgisayarında Türkçe yaması olan %1 oyun var; %2 tanesinde yama kurulu.")
                                          .arg(libraryRoot._lib.length).arg(libraryRoot._patched)
                        }
                    }

                    LibButton {
                        glyph: "\uE72C"
                        text: GameService.isScanning ? qsTr("Taranıyor…") : qsTr("Yeniden tara")
                        enabled: !GameService.isScanning
                        onClicked: GameService.scanAllLibraries()
                    }
                    LibButton {
                        glyph: "\uE710"
                        text: qsTr("Oyun ekle")
                        onClicked: libraryRoot.manualFolderRequested()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: libraryRoot._lib.length > 0
                    spacing: Dimensions.spacingLG

                    StatTile { key: "all"; value: libraryRoot._lib.length; label: qsTr("Bilgisayarında") }
                    StatTile { key: "ready"; value: libraryRoot._ready; label: qsTr("Kurulmayı bekliyor"); tint: Theme.accentLight }
                    StatTile { key: "update"; value: libraryRoot._updates; label: qsTr("Güncelleme var"); tint: Theme.warning }
                    StatTile { key: "installed"; value: libraryRoot._patched; label: qsTr("Yama kurulu"); tint: Theme.success }
                }

                // The games
                Repeater {
                    model: libraryRoot._rows

                    delegate: Rectangle {
                        id: row
                        required property var modelData

                        readonly property string status: row.modelData.status || "ready"
                        readonly property color tone: row.status === "update" ? Theme.warning
                                                    : row.status === "installed" ? Theme.success
                                                    : row.status === "external" ? Theme.textSecondary
                                                    : Theme.accentLight
                        function open() {
                            libraryRoot.gameSelected(row.modelData.id, row.modelData.name || "",
                                                     row.modelData.installPath || "", row.modelData.engine || "")
                        }
                        function act() {
                            if (row.status === "external")
                                Qt.openUrlExternally(row.modelData.externalUrl)
                            else if (row.status === "installed")
                                row.open()
                            else
                                libraryRoot.installRequested(row.modelData.id, row.modelData.name || "",
                                                             row.modelData.installPath || "", row.modelData.engine || "")
                        }

                        Layout.fillWidth: true
                        implicitHeight: 96
                        radius: Dimensions.radiusLG
                        color: rowHover.hovered ? Theme.textPrimary06 : Theme.textPrimary03
                        border.width: row.activeFocus ? 2 : 1
                        border.color: row.activeFocus ? Theme.accentLight : Theme.textPrimary06
                        Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: row.modelData.name || ""
                        Keys.onReturnPressed: row.open()
                        Keys.onSpacePressed: row.open()

                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: row.open()
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Dimensions.paddingMD
                            anchors.rightMargin: Dimensions.paddingLG
                            spacing: Dimensions.spacingXL

                            GameCard {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 53
                                Layout.preferredHeight: 76
                                showName: false
                                gameId: row.modelData.id || ""
                                gameName: row.modelData.name || ""
                                steamAppId: row.modelData.steamAppId || ""
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Dimensions.spacingXS

                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: row.modelData.name || ""
                                    elide: Text.ElideRight
                                    font.pixelSize: Dimensions.fontLG
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    Layout.fillWidth: true
                                    textFormat: Text.PlainText
                                    text: libraryRoot._source(row.modelData) + "  ·  "
                                          + libraryRoot._shownPath(row.modelData.installPath)
                                    elide: Text.ElideMiddle
                                    font.pixelSize: Dimensions.fontSM
                                    color: Theme.textMuted
                                }
                            }

                            // State: a word, never colour alone
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: pillRow.implicitWidth + 20
                                implicitHeight: 26
                                radius: 13
                                color: Theme.withAlpha(row.tone, 0.12)

                                Row {
                                    id: pillRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 7; height: 7; radius: 3.5
                                        color: row.tone
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        textFormat: Text.PlainText
                                        font.pixelSize: Dimensions.fontSM
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                        text: row.status === "update" ? qsTr("Güncelleme var")
                                            : row.status === "installed" ? qsTr("Yama kurulu")
                                            : row.status === "external" ? qsTr("Dış sitede")
                                            : qsTr("Yama hazır")
                                    }
                                }
                            }

                            LibButton {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 184
                                filled: row.status === "ready" || row.status === "update"
                                fill: row.status === "update" ? Theme.warning : Theme.primary
                                glyph: row.status === "ready" ? "\uE896"
                                     : row.status === "update" ? "\uE777"
                                     : row.status === "external" ? "\uE8A7" : "\uE76C"
                                text: row.status === "ready" ? qsTr("Türkçe Yamayı Kur")
                                    : row.status === "update" ? qsTr("Güncelle")
                                    : row.status === "external" ? qsTr("Sitesine git")
                                    : qsTr("Sayfasını aç")
                                onClicked: row.act()
                            }
                        }
                    }
                }

                // Nothing yet: what to do about it
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Dimensions.spacingXL
                    Layout.bottomMargin: Dimensions.spacingXL
                    visible: libraryRoot._lib.length === 0
                    spacing: Dimensions.spacingLG

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 72
                        implicitHeight: 72
                        radius: 36
                        color: Theme.primary12
                        Text {
                            anchors.centerIn: parent
                            textFormat: Text.PlainText
                            text: "\uED25"
                            font.family: "Segoe MDL2 Assets"
                            font.pixelSize: 28
                            color: Theme.accentLight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 560
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                        font.pixelSize: Dimensions.fontMD
                        color: Theme.textSecondary
                        text: qsTr("Steam, Epic, GOG ve Xbox kütüphanelerine baktık. Oyunun başka bir yerdeyse klasörünü ekle ya da pencereye sürükleyip bırak; hangi oyun olduğunu biz buluruz.")
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: libraryRoot._lib.length > 0
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
                font.pixelSize: Dimensions.fontSM
                color: Theme.textMuted
                text: qsTr("Bir oyunun listede yok mu? Klasörünü pencereye sürükleyip bırak ya da \"Oyun ekle\"yi kullan.")
            }
        }
    }
}
