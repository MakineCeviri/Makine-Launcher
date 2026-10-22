// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Makine Çeviri

import QtQuick
import MakineLauncher 1.0
pragma ComponentBehavior: Bound

/**
 * FocusRing — macOS keyboard focus ring: an accent halo just outside the
 * focused item. Place as a child of the focusable item:
 *   Rectangle { activeFocusOnTab: true; FocusRing {} }
 */
Rectangle {
    id: ring

    property Item target: parent
    property bool showRing: target ? target.activeFocus : false
    property color ringColor: Theme.withAlpha(Theme.primary, 0.45)
    property int offset: -3

    anchors.fill: target
    anchors.margins: offset
    radius: (target && target.radius !== undefined) ? target.radius - offset : -offset
    color: "transparent"
    border.color: ringColor
    border.width: 3
    visible: showRing
}
