// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
import "../../components"
pragma ComponentBehavior: Bound

/**
 * OnboardingButton — buttons on the always-dark welcome flow.
 *
 * primary: accent fill, the step's one action · quiet: text only (Geri,
 * Yeniden tara). Colours are fixed to the dark backdrop rather than the
 * theme: a light theme picked on the last step must not repaint these.
 */
Rectangle {
    id: btn

    property string text: ""
    property string trailingIcon: ""   // Icons.* glyph
    property string kind: "primary"    // primary | quiet

    signal clicked()

    readonly property bool _primary: btn.kind === "primary"

    implicitWidth: row.implicitWidth + (btn._primary ? 52 : 28)
    implicitHeight: btn._primary ? 48 : 40
    radius: height / 2
    color: btn._primary ? (hover.hovered ? Qt.lighter(Theme.accentBase, 1.12) : Theme.accentBase)
                        : (hover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
    border.width: btn.activeFocus ? 3 : 0
    border.color: Qt.rgba(1, 1, 1, 0.6)
    opacity: tap.pressed ? 0.75 : 1.0
    Behavior on color { ColorAnimation { duration: Dimensions.animFast } }

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: btn.text
    Keys.onReturnPressed: btn.clicked()
    Keys.onSpacePressed: btn.clicked()

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10

        Text {
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.PlainText
            text: btn.text
            font.pixelSize: btn._primary ? 15 : 14
            font.weight: btn._primary ? Font.DemiBold : Font.Medium
            color: btn._primary || hover.hovered ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.6)
        }
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: btn.trailingIcon !== ""
            glyph: btn.trailingIcon
            size: 16
            color: "#FFFFFF"
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
