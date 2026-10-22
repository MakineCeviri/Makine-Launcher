// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import MakineLauncher 1.0

/**
 * StyledToolTip — macOS help tag: a small elevated label under the pointer.
 */
ToolTip {
    id: tip
    delay: 500
    timeout: 6000

    contentItem: Text {
        text: tip.text
        font.pixelSize: Dimensions.caption1
        font.weight: Font.Medium
        color: Theme.textPrimary
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        color: Theme.surfaceElevated
        border.color: Theme.separatorStrong
        border.width: Theme.darkMode ? 1 : 0.5
        radius: 6

        RectangularShadow {
            z: -1
            anchors.fill: parent
            radius: parent.radius
            offset.y: 2
            blur: 8
            color: Theme.shadowSoft
        }
    }

    padding: 5
    leftPadding: 8
    rightPadding: 8

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Dimensions.animFast } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Dimensions.animVeryFast } }
}
