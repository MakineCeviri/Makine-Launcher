// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeShelf — a titled row of games that scrolls sideways.
 *
 * Items are plain objects: { gameId, steamAppId, name, subtitle, status,
 * installPath, engine }. The row never takes the vertical wheel — that
 * scrolls the page; arrows, a horizontal wheel/touchpad or Shift+wheel move
 * the row, and Tab brings the focused card into view.
 */
Item {
    id: shelf

    property string title: ""
    property string subtitle: ""
    property var items: []
    property real cardWidth: 148
    default property alias actions: header.actions

    signal gameClicked(var item)

    readonly property bool _hasSubtitles: shelf.items.some(i => !!i.subtitle)
    readonly property real _rowHeight: Math.round(shelf.cardWidth * 185 / 130)
                                       + Dimensions.spacingMD + 36
                                       + (shelf._hasSubtitles ? 2 + 16 : 0)
    readonly property real _minX: view.originX - view.leftMargin
    readonly property real _maxX: Math.max(_minX, view.originX + view.contentWidth
                                                  + view.rightMargin - view.width)
    readonly property bool _canLeft: view.contentX > _minX + 1
    readonly property bool _canRight: view.contentX < _maxX - 1

    function _scrollTo(x, animated) {
        const target = Math.max(_minX, Math.min(_maxX, x))
        scrollAnim.stop()
        if (animated) {
            scrollAnim.to = target
            scrollAnim.start()
        } else {
            view.contentX = target
        }
    }

    implicitHeight: header.implicitHeight + Dimensions.spacingLG + view.height

    HomeSectionHeader {
        id: header
        width: parent.width
        title: shelf.title
        subtitle: shelf.subtitle
    }

    ListView {
        id: view
        // Room for the hover ring and lift on the outer cards.
        readonly property int pad: 6
        x: -pad
        width: parent.width + pad * 2
        anchors.top: header.bottom
        anchors.topMargin: Dimensions.spacingLG - pad
        height: shelf._rowHeight + pad * 2
        orientation: ListView.Horizontal
        spacing: Dimensions.cardGap
        leftMargin: pad
        rightMargin: pad
        topMargin: pad
        interactive: false
        clip: true
        model: shelf.items

        delegate: HomeCover {
            id: cover
            required property var modelData
            required property int index
            width: shelf.cardWidth
            gameId: cover.modelData.gameId || ""
            steamAppId: cover.modelData.steamAppId || ""
            name: cover.modelData.name || ""
            subtitle: cover.modelData.subtitle || ""
            status: cover.modelData.status || ""
            onClicked: shelf.gameClicked(cover.modelData)
            onActiveFocusChanged: if (activeFocus) view.positionViewAtIndex(cover.index, ListView.Contain)
        }

        NumberAnimation {
            id: scrollAnim
            target: view
            property: "contentX"
            duration: Dimensions.animNormal
            easing.type: Dimensions.easeStandard
        }

        WheelHandler {
            target: null
            orientation: Qt.Horizontal
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => shelf._scrollTo(view.contentX - event.angleDelta.x, false)
        }
        WheelHandler {
            target: null
            acceptedModifiers: Qt.ShiftModifier
            onWheel: (event) => shelf._scrollTo(view.contentX - event.angleDelta.y, false)
        }
    }

    HoverHandler { id: shelfHover }

    component Arrow: Rectangle {
        id: arrow
        property bool pointsLeft: true
        signal pressed()

        width: 40; height: 40; radius: 20
        y: view.y + view.pad + Math.round(shelf.cardWidth * 185 / 130) / 2 - height / 2
        color: arrowHover.hovered ? Theme.surfaceElevated : Theme.withAlpha(Theme.bgPrimary, 0.85)
        border.width: 1
        border.color: Theme.textPrimary15
        Accessible.role: Accessible.Button
        Accessible.name: arrow.pointsLeft ? qsTr("Sola kaydır") : qsTr("Sağa kaydır")

        Text {
            anchors.centerIn: parent
            textFormat: Text.PlainText
            text: arrow.pointsLeft ? "" : ""
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: 14
            color: Theme.textPrimary
        }
        HoverHandler { id: arrowHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: arrow.pressed() }
    }

    Arrow {
        pointsLeft: true
        x: -width / 2
        visible: shelfHover.hovered && shelf._canLeft
        onPressed: shelf._scrollTo(view.contentX - view.width * 0.8, true)
    }
    Arrow {
        pointsLeft: false
        x: shelf.width - width / 2
        visible: shelfHover.hovered && shelf._canRight
        onPressed: shelf._scrollTo(view.contentX + view.width * 0.8, true)
    }
}
