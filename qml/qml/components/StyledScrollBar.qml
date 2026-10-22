// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import QtQuick.Controls
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * StyledScrollBar — macOS overlay scroller: a thin thumb that shows while
 * scrolling or under the pointer and widens when grabbed.
 *
 *   ScrollBar.vertical: StyledScrollBar {}
 */
ScrollBar {
    id: root

    policy: ScrollBar.AsNeeded
    padding: 2
    minimumSize: 0.08

    background: Item {}

    contentItem: Rectangle {
        implicitWidth: root.hovered || root.pressed ? 8 : 6
        implicitHeight: root.hovered || root.pressed ? 8 : 6
        radius: Math.min(width, height) / 2
        color: root.pressed || root.hovered ? Theme.scrollbarThumbHover : Theme.scrollbarThumb
        opacity: root.active || root.hovered ? 1.0 : 0.0

        Behavior on implicitWidth { NumberAnimation { duration: Dimensions.animVeryFast } }
        Behavior on opacity { NumberAnimation { duration: Dimensions.animNormal } }
    }
}
