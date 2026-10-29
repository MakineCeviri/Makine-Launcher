// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Layouts
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * Shelf — a titled row of game tiles that scrolls sideways, in a container
 * of its own: title and a line under it on the left, "Tümünü gör" on the
 * right, a fading divider, then the row, whose tiles fade out at the
 * container's sides instead of being cut. Round arrows on the row's ends
 * under the pointer. A vertical wheel scrolls the page; a sideways one (or
 * Shift + wheel, or a drag) the row.
 *
 * model: a JS array of { id/gameId, name, steamAppId, installPath, engine,
 * status?, subtitle? } or a CatalogProxyModel.
 */
Card {
    id: shelf

    property string title: ""
    property string subtitle: ""
    property string seeAllText: qsTr("Tümünü gör")
    property bool showSeeAll: true
    property var model: []
    property real tileWidth: 150
    // Left/right inset of the row inside the container, so the first tile
    // lines up with the title
    property real inset: 22

    signal gameClicked(string gameId, string gameName, string installPath, string engine)
    signal seeAllClicked()

    Layout.leftMargin: Dimensions.pageMargin
    Layout.rightMargin: Dimensions.pageMargin
    radius: Dimensions.radiusSection
    implicitHeight: body.implicitHeight + 18

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 18
        spacing: 12

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: shelf.inset
        Layout.rightMargin: shelf.inset - 6
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: shelf.title
                elide: Text.ElideRight
                font.family: Dimensions.fontDisplay
                font.pixelSize: Dimensions.title3
                font.weight: Font.Bold
                font.letterSpacing: Dimensions.trackingHeadline
                color: Theme.textPrimary
                Accessible.role: Accessible.Heading
                Accessible.name: shelf.title
            }
            Text {
                visible: shelf.subtitle !== ""
                Layout.fillWidth: true
                textFormat: Text.PlainText
                text: shelf.subtitle
                elide: Text.ElideRight
                font.pixelSize: Dimensions.footnote
                color: Theme.textMuted
            }
        }
        AppButton {
            visible: shelf.showSeeAll
            Layout.alignment: Qt.AlignVCenter
            style: "plain"
            size: "small"
            text: shelf.seeAllText
            trailingIcon: Icons.chevronRight
            onClicked: shelf.seeAllClicked()
        }
    }

    FadeLine { Layout.leftMargin: 12; Layout.rightMargin: 12 }

    Item {
        id: rowBox
        Layout.fillWidth: true
        Layout.leftMargin: 1
        Layout.rightMargin: 1
        Layout.topMargin: -6
        Layout.preferredHeight: view.height

        ListView {
            id: view
            anchors.left: parent.left
            anchors.right: parent.right
            // Room above for the lift on hover, below for the shadow
            height: Math.round(shelf.tileWidth * 185 / 130) + 76
            orientation: ListView.Horizontal
            spacing: 18
            leftMargin: shelf.inset
            rightMargin: shelf.inset
            clip: true
            model: shelf.model
            boundsBehavior: Flickable.StopAtBounds
            // Not a flickable to the wheel or a drag: a horizontal Flickable
            // turns a plain wheel into a sideways scroll, so the page stopped
            // scrolling under the pointer — even on a row with two tiles and
            // nowhere to go. The arrows move it, and a sideways wheel or
            // Shift + wheel when it has more than fits.
            interactive: false
            readonly property bool overflows: contentWidth + leftMargin + rightMargin > width + 1
            reuseItems: true
            cacheBuffer: 600
            pixelAligned: true

            delegate: Item {
                id: cell
                required property var model
                required property int index
                width: shelf.tileWidth
                height: view.height

                readonly property string _gameKey: cell.model.gameId ?? cell.model.id ?? ""

                HomeCover {
                    y: 8
                    width: shelf.tileWidth
                    gameId: cell._gameKey
                    name: cell.model.name ?? cell.model.gameName ?? ""
                    steamAppId: cell.model.steamAppId ?? ""
                    subtitle: cell.model.subtitle ?? ""
                    status: cell.model.status
                            ?? (cell.model.packageInstalled ? "installed"
                                : cell.model.isInstalled ? "installable"
                                : cell.model.comingSoon ? "soon" : "")
                    onClicked: shelf.gameClicked(cell._gameKey, cell.model.name ?? "",
                                                 cell.model.installPath ?? "", cell.model.engine ?? "")
                }
            }

            // A plain wheel belongs to the page; a sideways one, or Shift +
            // wheel, moves the row — when there is more of it than fits.
            // Sideways only: Shift + wheel, or a tilting wheel / touchpad. A
            // plain wheel is the page's, and these handlers must not take it
            // at all: one that receives a wheel event blocks the page behind
            // it even when it ignores the event (0.2.1: the page would not
            // scroll with the pointer over a row that overflows).
            WheelHandler {
                target: null
                enabled: view.overflows
                acceptedModifiers: Qt.ShiftModifier
                onWheel: (event) => view._scrollBy(event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y)
            }
            WheelHandler {
                target: null
                enabled: view.overflows
                orientation: Qt.Horizontal
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                acceptedModifiers: Qt.NoModifier
                onWheel: (event) => view._scrollBy(event.angleDelta.x)
            }

            function _scrollBy(d) {
                const lo = view.originX - view.leftMargin
                const hi = Math.max(lo, view.contentWidth - view.width + view.originX + view.rightMargin)
                view.contentX = Math.max(lo, Math.min(hi, view.contentX - d))
            }

            function _page(dir) {
                const lo = view.originX - view.leftMargin
                const hi = Math.max(lo, view.contentWidth - view.width + view.originX + view.rightMargin)
                pageAnim.to = Math.max(lo, Math.min(hi, view.contentX + dir * (view.width - shelf.inset * 2) * 0.9))
                pageAnim.restart()
            }
            NumberAnimation {
                id: pageAnim
                target: view
                property: "contentX"
                duration: Dimensions.animSlow
                easing.type: Easing.OutCubic
            }

            readonly property bool atStart: contentX <= originX - leftMargin + 2
            readonly property bool atEnd: contentX >= contentWidth - width + originX + rightMargin - 2
        }

        // Tiles fade out at the container's sides
        EdgeFade {
            target: view
            leftFade: view.atStart ? 0 : 40
            rightFade: !view.atEnd && view.overflows ? 40 : 0
        }

        HoverHandler { id: rowHover }

        // Paging arrows: glass circles on the row's ends, over the art where
        // it fades out
        PagerButton {
            z: 6
            anchors.left: parent.left
            anchors.leftMargin: shelf.inset - 18
            y: 8 + Math.round(shelf.tileWidth * 185 / 130) / 2 - height / 2
            direction: -1
            size: 38
            opacity: rowHover.hovered && !view.atStart ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
            onClicked: view._page(-1)
        }
        PagerButton {
            z: 6
            anchors.right: parent.right
            anchors.rightMargin: shelf.inset - 18
            y: 8 + Math.round(shelf.tileWidth * 185 / 130) / 2 - height / 2
            direction: 1
            size: 38
            opacity: rowHover.hovered && !view.atEnd && view.overflows ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: Dimensions.animFast } }
            onClicked: view._page(1)
        }
    }
    }
}
