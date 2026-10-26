// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * MenuButton — macOS's pop-up button: the choice in use on a button
 * ("⇅ A–Z ⌄"), a short menu under it with that choice ticked.
 *
 *   model: [{ value: "", text: "A–Z" }, …] · currentValue · activated(value)
 */
Item {
    id: mb

    property var model: []
    property string currentValue: ""
    property string icon: ""
    property string toolTip: ""

    signal activated(string value)

    readonly property var _current: {
        for (const m of mb.model)
            if (m.value === mb.currentValue) return m
        return mb.model.length > 0 ? mb.model[0] : null
    }

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    AppButton {
        id: button
        style: "gray"
        icon: mb.icon
        text: mb._current ? mb._current.text : ""
        trailingIcon: Icons.chevronDown
        trailingRotation: menu.opened ? 180 : 0
        toolTip: menu.opened ? "" : mb.toolTip
        onClicked: menu.opened ? menu.close() : menu.open()
    }

    Popup {
        id: menu
        y: button.height + 6
        x: button.width - width
        width: Math.max(button.width, 200)
        padding: 5
        transformOrigin: Popup.TopRight
        scale: Dimensions.uiZoom
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.animFast; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.97 * Dimensions.uiZoom; to: Dimensions.uiZoom; duration: Dimensions.animFast; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: Dimensions.animVeryFast }
        }

        background: Item {
            RectangularShadow {
                anchors.fill: sheet
                radius: sheet.radius
                offset.y: 8
                blur: 24
                spread: -4
                color: Qt.rgba(0, 0, 0, Theme.darkMode ? 0.5 : 0.18)
            }
            Rectangle {
                id: sheet
                anchors.fill: parent
                radius: Dimensions.radiusLG
                color: Theme.surfaceElevated
                border.width: 1
                border.color: Theme.separatorStrong
            }
        }

        contentItem: Column {
            spacing: 0

            Repeater {
                model: mb.model

                Item {
                    id: option
                    required property var modelData
                    readonly property bool selected: option.modelData.value === (mb._current ? mb._current.value : "")

                    width: menu.availableWidth
                    height: 32

                    Accessible.role: Accessible.MenuItem
                    Accessible.name: option.modelData.text
                    Accessible.checked: option.selected

                    Rectangle {
                        anchors.fill: parent
                        radius: Dimensions.radiusMD - 2
                        color: optionHover.hovered ? Theme.primary : "transparent"
                    }
                    Icon {
                        id: tick
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: option.selected
                        glyph: Icons.check
                        size: 14
                        color: optionHover.hovered ? "#FFFFFF" : Theme.primaryText
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 30
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.PlainText
                        text: option.modelData.text
                        elide: Text.ElideRight
                        font.pixelSize: Dimensions.callout
                        font.weight: option.selected ? Font.DemiBold : Font.Normal
                        color: optionHover.hovered ? "#FFFFFF" : Theme.textPrimary
                    }

                    HoverHandler { id: optionHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            menu.close()
                            mb.activated(option.modelData.value)
                        }
                    }
                }
            }
        }
    }
}
