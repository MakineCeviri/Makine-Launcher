// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

Item {
    id: root

    property string gameId: ""
    property string gameName: ""
    property string steamAppId: ""
    property string installPath: ""
    // Optional state pill on the cover ("Kurulu", "Bilgisayarında", a date…)
    property string badge: ""
    property color badgeColor: Theme.primary
    // The name over the cover's foot on hover: much cover art carries no
    // title, and a screen reader or a slow image has nothing else to go on.
    property bool showName: true

    signal clicked()

    Accessible.role: Accessible.Button
    Accessible.name: root.gameName

    readonly property real _aspectRatio: 130.0 / 185.0
    readonly property int _minHeight: 100
    width: Math.round(height * _aspectRatio)
    height: Math.max(_minHeight, Dimensions.cardHeight)

    // Resolved file:// URL from ImageCacheManager
    property string _src: ""

    function _resolve() {
        var id = steamAppId || gameId
        if (!id) { _src = ""; return }
        _src = ImageCache.resolve(id)
    }

    onSteamAppIdChanged: _resolve()
    Component.onCompleted: _resolve()
    ListView.onPooled: _src = ""
    ListView.onReused: _resolve()

    // Listen for download completions — only when image not yet cached.
    // The target stays fixed and `enabled` does the gating: the handler below
    // changes _src itself, and changing `target` from inside the handler makes
    // QQmlConnections detach the running QQmlBoundSignal and delete it later.
    // A delegate destroyed before that deferred delete left the handler wired
    // to a freed Connections — the NATIVE-74 crash (Qt6Qml.dll+0xb4a0e),
    // 24 of 27 field fatals in 0.1.4.
    Connections {
        target: ImageCache
        enabled: root._src === ""
        function onImageReady(readyId) {
            if (readyId === (root.steamAppId || root.gameId))
                root._resolve()
        }
    }

    // Placeholder — only for games with no image (or not yet): the name, so
    // the card still says which game it is.
    Rectangle {
        anchors.fill: parent
        radius: root.width * 0.07
        color: Theme.surfaceLight
        visible: !root._src || img.status !== Image.Ready

        Text {
            anchors.fill: parent
            anchors.margins: Dimensions.paddingMD
            visible: root.height >= 120
            textFormat: Text.PlainText
            text: root.gameName
            wrapMode: Text.WordWrap
            maximumLineCount: 4
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Dimensions.fontMD
            font.weight: Font.DemiBold
            color: Theme.textSecondary
        }
    }

    Image {
        id: img
        anchors.fill: parent
        source: root._src
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(260, 370)
        asynchronous: true
        mipmap: true
    }

    // Dark pill + coloured dot + word: readable on any cover art.
    Rectangle {
        visible: root.badge !== ""
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Dimensions.spacingMD
        height: 20
        width: badgeRow.implicitWidth + 14
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.72)

        Row {
            id: badgeRow
            anchors.centerIn: parent
            spacing: 5
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 6; height: 6; radius: 3
                color: root.badgeColor
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                textFormat: Text.PlainText
                text: root.badge
                font.pixelSize: Dimensions.fontCaption
                font.weight: Font.DemiBold
                color: "#FFFFFF"
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: nameText.implicitHeight + Dimensions.paddingMD * 2
        bottomLeftRadius: root.width * 0.07
        bottomRightRadius: root.width * 0.07
        color: Qt.rgba(0, 0, 0, 0.78)
        opacity: root.showName && cardHover.hovered && img.status === Image.Ready ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }

        Text {
            id: nameText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Dimensions.paddingMD
            anchors.rightMargin: Dimensions.paddingMD
            textFormat: Text.PlainText
            text: root.gameName
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Dimensions.fontBody
            font.weight: Font.DemiBold
            color: "#FFFFFF"
        }
    }

    scale: cardHover.hovered ? 1.03 : 1.0
    Behavior on scale { NumberAnimation { duration: Dimensions.animFast; easing.type: Dimensions.easeStandard } }

    HoverHandler {
        id: cardHover
        cursorShape: Qt.PointingHandCursor
    }
}
