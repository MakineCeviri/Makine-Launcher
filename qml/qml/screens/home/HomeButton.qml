// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * HomeButton — the home page's buttons.
 *
 * primary: the one thing to do (accent fill) · secondary: a quieter
 * alternative · ghost: header actions next to a section title.
 */
Rectangle {
    id: btn

    property string text: ""
    property string icon: ""        // Segoe MDL2 Assets glyph, optional
    property url image: ""          // or an image icon (e.g. a brand mark)
    property string kind: "primary" // primary | secondary | ghost
    property color fill: Theme.primary  // primary only

    signal clicked()

    readonly property bool _ghost: btn.kind === "ghost"

    implicitWidth: row.implicitWidth + (btn._ghost ? Dimensions.paddingLG : Dimensions.paddingXL * 1.5)
    implicitHeight: btn._ghost ? 32 : 42
    radius: Dimensions.radiusMD + 2
    color: btn.kind === "primary" ? (hover.hovered ? Qt.darker(btn.fill, 1.12) : btn.fill)
         : btn.kind === "secondary" ? (hover.hovered ? Theme.textPrimary15 : Theme.textPrimary10)
         : (hover.hovered ? Theme.textPrimary08 : "transparent")
    border.width: btn.activeFocus ? 2 : 0
    border.color: Theme.textPrimary
    scale: tap.pressed ? Dimensions.pressScale : 1.0
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: btn.text
    Keys.onReturnPressed: btn.clicked()
    Keys.onSpacePressed: btn.clicked()

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Dimensions.spacingMD

        Image {
            anchors.verticalCenter: parent.verticalCenter
            visible: btn.image.toString() !== ""
            source: btn.image
            width: 18; height: 18
            sourceSize: Qt.size(18, 18)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: btn.icon !== ""
            textFormat: Text.PlainText
            text: btn.icon
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: btn._ghost ? 12 : 14
            color: label.color
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: btn.text
            font.pixelSize: btn._ghost ? Dimensions.fontSM : Dimensions.fontMD
            font.weight: Font.DemiBold
            color: btn.kind === "primary" ? Theme.textOnColor
                 : btn._ghost && !hover.hovered ? Theme.textSecondary
                 : Theme.textPrimary
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        id: tap
        onTapped: btn.clicked()
    }
}
