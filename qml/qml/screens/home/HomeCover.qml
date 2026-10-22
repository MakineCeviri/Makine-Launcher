// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Effects
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * HomeCover — one game as a tile: cover art, its name, an optional line
 * under it. Used by the home shelves, "Tüm Oyunlar" and the welcome flow.
 *
 * Covers come from the image cache as pre-rounded PNGs (corner ≈ 11.8 % of
 * the width), so no mask is needed. The badge carries the patch's state on
 * this PC, so a tile says what a click leads to before it is made.
 */
Item {
    id: card

    property string gameId: ""
    property string steamAppId: ""
    property string name: ""
    property string subtitle: ""
    // false: a picture of the game, not a control (the welcome flow)
    property bool interactive: true
    // "" | "ready" (on this PC, patch not installed) | "installable" (same,
    // where "on this PC" is the news) | "installed" | "update" | "external"
    property string status: ""
    // Overrides the status wording (a date on "Yeni güncellenen")
    property string badge: ""
    property color badgeColor: Theme.primary
    // Keyboard position in a grid that keeps focus itself
    property bool highlighted: false

    signal clicked()

    readonly property real coverHeight: Math.round(width * 185 / 130)
    readonly property real _corner: width * 0.118
    readonly property bool _lifted: card.interactive && (hover.hovered || card.activeFocus)
    readonly property string _statusText: card.badge !== "" ? card.badge
                                        : card.status === "installed" ? qsTr("Kurulu")
                                        : card.status === "update" ? qsTr("Güncelleme var")
                                        : card.status === "ready" ? qsTr("Yama hazır")
                                        : card.status === "installable" ? qsTr("Bilgisayarında")
                                        : card.status === "external" ? qsTr("Dış sitede")
                                        : ""
    readonly property color _statusColor: card.badge !== "" ? card.badgeColor
                                        : card.status === "installed" ? Theme.success
                                        : card.status === "update" ? Theme.warning
                                        : card.status === "external" ? "#AEAEB2"
                                        : Theme.primary

    implicitHeight: coverHeight + 10 + nameLabel.implicitHeight
                    + (subtitleLabel.visible ? subtitleLabel.implicitHeight + 2 : 0)

    activeFocusOnTab: card.interactive
    Accessible.role: card.interactive ? Accessible.Button : Accessible.Graphic
    Accessible.name: card._statusText !== "" ? card.name + ", " + card._statusText : card.name
    Keys.onReturnPressed: card.clicked()
    Keys.onSpacePressed: card.clicked()

    // Resolved file:// URL from the image cache ("" until downloaded)
    property string _src: ""
    function _resolve() {
        const id = card.steamAppId || card.gameId
        card._src = id ? ImageCache.resolve(id) : ""
    }
    onSteamAppIdChanged: _resolve()
    onGameIdChanged: _resolve()
    Component.onCompleted: _resolve()
    ListView.onPooled: card._src = ""
    ListView.onReused: card._resolve()
    GridView.onPooled: card._src = ""
    GridView.onReused: card._resolve()

    // Fixed target, gated by `enabled`: changing `target` from inside the
    // handler is a use-after-free in delegates (NATIVE-74).
    Connections {
        target: ImageCache
        enabled: card._src === ""
        function onImageReady(readyId) {
            if (readyId === (card.steamAppId || card.gameId))
                card._resolve()
        }
    }

    Item {
        id: coverBox
        width: card.width
        height: card.coverHeight
        scale: card._lifted ? 1.035 : 1.0
        transformOrigin: Item.Bottom
        Behavior on scale { NumberAnimation { duration: Dimensions.animNormal; easing.type: Easing.OutCubic } }

        RectangularShadow {
            anchors.fill: parent
            radius: card._corner
            offset.y: card._lifted ? 10 : 4
            blur: card._lifted ? 26 : 12
            spread: -2
            color: Qt.rgba(0, 0, 0, Theme.darkMode ? (card._lifted ? 0.55 : 0.35) : (card._lifted ? 0.22 : 0.12))
            Behavior on offset.y { NumberAnimation { duration: Dimensions.animNormal } }
        }

        // Until the cover arrives: the name on a plain tile, never an empty hole.
        Rectangle {
            anchors.fill: parent
            radius: card._corner
            color: Theme.surfaceLight
            visible: cover.status !== Image.Ready

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                textFormat: Text.PlainText
                text: card.name
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
                font.pixelSize: Dimensions.callout
                font.weight: Font.DemiBold
                color: Theme.textMuted
            }
        }

        Image {
            id: cover
            anchors.fill: parent
            source: card._src
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(260, 370)
            asynchronous: true
            mipmap: true
        }

        // Keyboard focus: the macOS accent ring around the art
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: card._corner + 4
            color: "transparent"
            border.width: 3
            border.color: Theme.withAlpha(Theme.primary, 0.6)
            visible: card.activeFocus || card.highlighted
        }

        StatusBadge {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            onArt: true
            text: card._statusText
            tint: card._statusColor
        }
    }

    Text {
        id: nameLabel
        anchors.top: coverBox.bottom
        anchors.topMargin: 10
        width: card.width
        textFormat: Text.PlainText
        text: card.name
        // Two lines: "Assassin's Creed IV: Black Flag" cut at one read as
        // a wall of "Assassin's Creed ...".
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        lineHeight: 1.08
        font.pixelSize: Dimensions.callout
        font.weight: Font.Medium
        color: Theme.textPrimary
    }

    Text {
        id: subtitleLabel
        anchors.top: nameLabel.bottom
        anchors.topMargin: 2
        width: card.width
        visible: card.subtitle !== ""
        textFormat: Text.PlainText
        text: card.subtitle
        elide: Text.ElideRight
        font.pixelSize: Dimensions.footnote
        color: Theme.textMuted
    }

    HoverHandler {
        id: hover
        enabled: card.interactive
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        enabled: card.interactive
        onTapped: card.clicked()
    }
}
