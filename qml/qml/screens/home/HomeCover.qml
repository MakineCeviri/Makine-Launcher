// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeCover — one game on the home page: cover, name, an optional line below.
 *
 * Covers come from the image cache as pre-rounded PNGs (the same source as
 * GameCard). The badge carries the patch's state on this PC, so a shelf says
 * what a click leads to before it is made.
 */
Item {
    id: card

    property string gameId: ""
    property string steamAppId: ""
    property string name: ""
    property string subtitle: ""
    // "" | "ready" (on this PC, patch not installed) | "installable" (same,
    // where "on this PC" is the news) | "installed" | "update"
    property string status: ""

    signal clicked()

    readonly property real coverHeight: Math.round(width * 185 / 130)
    readonly property bool _lifted: hover.hovered || card.activeFocus
    readonly property string _statusText: card.status === "installed" ? qsTr("Kurulu")
                                        : card.status === "update" ? qsTr("Güncelleme var")
                                        : card.status === "ready" ? qsTr("Yama hazır")
                                        : card.status === "installable" ? qsTr("Bilgisayarında")
                                        : ""
    readonly property color _statusColor: card.status === "installed" ? Theme.success
                                        : card.status === "update" ? Theme.warning
                                        : Theme.primary

    implicitHeight: coverHeight + Dimensions.spacingMD + nameLabel.implicitHeight
                    + (subtitleLabel.visible ? subtitleLabel.implicitHeight + 2 : 0)

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
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

    // Fixed target, gated by `enabled`: see GameCard (NATIVE-74).
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
        scale: card._lifted ? 1.04 : 1.0
        Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Dimensions.easeStandard } }

        // Until the cover arrives: the name on a plain tile, never an empty hole.
        Rectangle {
            anchors.fill: parent
            radius: Dimensions.radiusLG
            color: Theme.surfaceLight
            visible: cover.status !== Image.Ready

            Text {
                anchors.centerIn: parent
                width: parent.width - Dimensions.paddingLG * 2
                textFormat: Text.PlainText
                text: card.name
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
                font.pixelSize: Dimensions.fontSM
                font.weight: Font.Medium
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

        // Hover and keyboard focus share one ring; focus is the stronger one.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Dimensions.radiusLG + 3
            color: "transparent"
            border.width: 2
            border.color: card.activeFocus ? Theme.primary : Theme.textPrimary25
            visible: card._lifted
        }

        // State on this PC: dark pill + coloured dot + word, readable on any art.
        Rectangle {
            visible: card._statusText !== ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Dimensions.spacingMD
            height: 22
            width: badgeRow.implicitWidth + Dimensions.paddingLG
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.72)

            Row {
                id: badgeRow
                anchors.centerIn: parent
                spacing: 5

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 6; height: 6; radius: 3
                    color: card._statusColor
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    text: card._statusText
                    font.pixelSize: Dimensions.fontCaption
                    font.weight: Font.DemiBold
                    color: "#FFFFFF"
                }
            }
        }
    }

    Text {
        id: nameLabel
        anchors.top: coverBox.bottom
        anchors.topMargin: Dimensions.spacingMD
        width: card.width
        textFormat: Text.PlainText
        text: card.name
        // Two lines: "Assassin's Creed IV: Black Flag" cut at one read as
        // a wall of "Assassin's Creed ...".
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        font.pixelSize: Dimensions.fontBody
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
        font.pixelSize: Dimensions.fontSM
        color: Theme.textMuted
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        onTapped: card.clicked()
    }
}
